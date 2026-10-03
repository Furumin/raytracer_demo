#include "tracer.h"

namespace Tracer {

  void
  Ray_trace_buffers::Init(const Ray_trace_buffers_create_param &param) {
    this->Hash = 0;
    this->RayChunkCapacity = (param.RayCapacity + TRACER_RAY_CHUNK_SIZE - 1) / TRACER_RAY_CHUNK_SIZE;
    this->RayBuffer = GPU::Malloc(sizeof (Ray_chunk) * this->RayChunkCapacity, GPU::GPU_PRIVATE);
#if !TRACER_RAY_HIT_UNIFIED
    this->HitBuffer = GPU::Malloc(sizeof (Hit_record_chunk) * this->RayChunkCapacity, GPU::GPU_PRIVATE);
#endif
  }

  void
  Ray_trace_buffers::Free() {
    GPU::Free(this->RayBuffer);
#if !TRACER_RAY_HIT_UNIFIED
    GPU::Free(this->HitBuffer);
#endif
  }

  void buildSphereAABB(const u32 primID, tinybvh::bvhvec3 &bMin, tinybvh::bvhvec3 &bMax, void *data) {
    bMin = tinybvh::bvhvec3(-1.0f);
    bMax = tinybvh::bvhvec3(1.0f);
  }
  
  void buildCustomTLAS(const u32 primID, tinybvh::bvhvec3 &bMin, tinybvh::bvhvec3 &bMax, void *data) {
    Object *obj = &((Object*)data)[primID];
    ASSERT(obj->Type == Object_type::SPHERE, "Only sphere TLAS is supported!");
    tinybvh::bvhvec3 pos {obj->Transform[3], obj->Transform[7], obj->Transform[11]};
    bMin = pos - tinybvh::bvhvec3(obj->Transform[0]);
    bMax = pos + tinybvh::bvhvec3(obj->Transform[0]);
  }

  void
  Object_buffers::Init(Scratch *pScratch, const GPU::Cmd_buf *pCmdBuf, const Object_buffers_create_param &param) {
    u32 totalObjectCount = param.SphereCount;
    const u32 materialCount = param.MaterialCount;
    totalObjectCount = MIN(totalObjectCount, (1 << TRACER_OBJ_INDEX_BIT_COUNT));
    this->ObjectBuffer = GPU::Malloc(sizeof (Object) * totalObjectCount, GPU::GPU_PRIVATE);
    GPU::Bump_alloc *bumpAlloc = GPU::GetBumpAlloc(GPU::CPU_ACCESS);
    // make objects (only spheres for now)
    Object *objCpuBuffer = pScratch->Push<Object>(totalObjectCount);
    u32 i = 0;
    // build default sphere BVH
    for (; i < param.SphereCount; i++) {
      // ignore material for now
      Sphere sph;
      sph.Position = *((v3*)((u8*)param.SpherePositions + param.SpherePositionStride * i));
      sph.Radius = *((float*)((u8*)param.SphereRadii + param.SphereRadiusStride * i));
      Object *obj = objCpuBuffer + i;
      *obj = { };
      obj->Type = Object_type::SPHERE;
      // set scale ( = radius)
      obj->Transform[0] = sph.Radius;
      obj->Transform[5] = sph.Radius;
      obj->Transform[10] = sph.Radius;
      // row-major, set pos
      obj->Transform[3] = sph.Position.x;
      obj->Transform[7] = sph.Position.y;
      obj->Transform[11] = sph.Position.z;
      obj->MaterialIndex = param.SphereMaterialIndices[i];
    }
    tinybvh::BVH tempBVH;
    tempBVH.customUserdata = objCpuBuffer;
    tempBVH.Build(&buildCustomTLAS, totalObjectCount);
    this->CPUTLAS[Object_type::SPHERE].ConvertFrom(tempBVH);
    // write objects, bvh to GPU. tinybvh reorders primitives, so we have to respect that
    GPU::Pointer objBuffer = bumpAlloc->Bump<Object>(totalObjectCount);
    Object *objGpuBuffer = (Object*)objBuffer.Cpu();
    for (u32 i = 0; i < totalObjectCount; i++) {
      objGpuBuffer[i] = objCpuBuffer[tempBVH.primIdx[i]];
    }
    pCmdBuf->MemCpy(objBuffer, this->ObjectBuffer, sizeof (Object) * totalObjectCount);
    const u32 bvhSize = sizeof (Bvh2) * this->CPUTLAS[Object_type::SPHERE].usedNodes;
    GPU::Pointer bvhStaging = bumpAlloc->Bump<Bvh2>(this->CPUTLAS[Object_type::SPHERE].usedNodes);
    this->GPUTLAS[Object_type::SPHERE] = GPU::Malloc(bvhSize, GPU::GPU_PRIVATE);
    bvhStaging.Write(V_(this->CPUTLAS[Object_type::SPHERE].bvhNode), bvhSize);
    pCmdBuf->MemCpy(bvhStaging, this->GPUTLAS[Object_type::SPHERE], bvhSize);

    // materials
    u32 materialChunkCount = (materialCount + TRACER_MATERIAL_CHUNK_SIZE - 1) / TRACER_MATERIAL_CHUNK_SIZE;
    this->MaterialBuffer = GPU::Malloc(sizeof (Material_chunk) * materialChunkCount, GPU::CPU_ACCESS);
    Material_chunk *mc = (Material_chunk*)this->MaterialBuffer.Cpu();
    u32 matI = 0;
    for (u32 i = 0; i < materialChunkCount; i++) {
      for (u32 j = 0; j < TRACER_MATERIAL_CHUNK_SIZE; j++, matI++) {
	if (matI < materialCount) {
	  mc[i].ColorX[j] = param.Materials[matI].Color.x;
	  mc[i].ColorY[j] = param.Materials[matI].Color.y;
	  mc[i].ColorZ[j] = param.Materials[matI].Color.z;
	  mc[i].Roughness[j] = param.Materials[matI].Roughness;
	  mc[i].RefractionIndex[j] = param.Materials[matI].RefractionIndex;
	  mc[i].Flags[j] = param.Materials[matI].Flags;
	}
      }
    }
    //    this->MaterialBuffer.Write(param.Materials, sizeof (Material) * materialCount);
  }

  void
  Object_buffers::Free() {
    GPU::Free(this->ObjectBuffer);
    GPU::Free(this->GPUTLAS[Object_type::SPHERE]);
  }

  GPU::KernelHandle
  World::LoadKernel(const char *path) {
    Util::File f;
    Util::OpenFile(path, Util::IO_FILE_READ | Util::IO_FILE_SHARE_READ | Util::IO_OPEN_ONLY | Util::IO_FLAG_MEM_MAP, &f, 0);
    auto handle = GPU::CreateKernel(f.MappedMemory, f.Size, "main");
    Util::CloseFile(&f);
    return handle;
  }
  
  void
  World::ReinitCamera(uv2 pixelImageDim) {
    mCamera.PixelWidth = pixelImageDim.x;
    mCamera.PixelHeight = pixelImageDim.y;

    mCamera.Right = quatrotv3(mCameraQuat, {1, 0, 0});
    mCamera.Up = quatrotv3(mCameraQuat, {0, 1, 0});
    mCamera.Forward = quatrotv3(mCameraQuat, {0, 0, 1});
    
    mCamera.AspectRatio = (float)mCamera.PixelWidth / (float)mCamera.PixelHeight;
    float th = mCamera.VFov;
    float h = bm::tanf(th);
    float vpH = 2 * h * mCamera.FocusDistance;
    float vpW = mCamera.AspectRatio * vpH;
    v3 cameraRight = quatrotv3(mCameraQuat, {1, 0, 0});
    v3 vpU = cameraRight * vpW;
    v3 vpV = mCamera.Up * -vpH;
    mCamera.DeltaU = vpU / (float)mCamera.PixelWidth;
    mCamera.DeltaV = vpV / (float)mCamera.PixelHeight;
    v3 vpUL = mCamera.Position + mCamera.Forward * mCamera.FocusDistance - ( vpU + vpV ) * 0.5;
    mCamera.Pixel00 = vpUL + (mCamera.DeltaU + mCamera.DeltaV) * 0.5f;

    mProgramSignals |= RAY_INTERSECT_PCD_NO_HYST_SIGNAL;
  }

  void
  World::FirstFrameLoad(const GPU::Cmd_buf *pCmdBuf) {
    Scratch scratch(&mScratchArena);
    mSphereCount = 10000;
    const u32 matCount = 6;
    Sphere *s = scratch.Push<Sphere>(mSphereCount);
    Material *materials = scratch.Push<Material>(matCount);
    u32 *materialIndices = scratch.Push<u32>(mSphereCount);
    Object_buffers_create_param objCreateParam = {
      .Materials = materials,
      .MaterialCount = matCount,
      .SphereMaterialIndices = materialIndices,
      .SpherePositions = V_(&s[0].Position),
      .SpherePositionStride = sizeof (Sphere),
      .SphereRadii = V_(&s[0].Radius),
      .SphereRadiusStride = sizeof (Sphere),
      .SphereCount = mSphereCount
    };
    s[0] = {
      .Position = {0, -1000, 0},
      .Radius = 998.999f
    };
    materialIndices[0] = 3;
    u32 i;
    for (i = 1; i < mSphereCount - 1; i++) {
      s[i].Position.x = -50.0f + (float)(i % 51) * 1.5f;
      s[i].Position.y = ((float)(hash1(i-1) % 6000) / 3000.0f) * 1.5f;
      s[i].Position.z = -51.0f + (float)(hash1(i+32) % 90) * 1.7f;
      s[i].Radius = (float)(2 + hash1(i + 15) % 7) * 0.1;
      materialIndices[i] = hash1(i) % 6;
    }
    for (; i < mSphereCount; i++) {
      s[i].Position.x = 0;
      s[i].Position.y = 15.0f;
      s[i].Position.z = 50;
      s[i].Radius = 6.0f;
      materialIndices[i] = 5;
    }
    // lights
    materials[0] = {
      .Color = {0.95, 0.1, 0.2},
      .Roughness = 0.8f,
      .Flags = TRACER_MATERIAL_FLAG_METALLIC,
    };
    materials[1] = {
      .Color = {0.95, 0.99, 0.95},
      .Roughness = 0.25f,
      .RefractionIndex = 1.56f
    };
    materials[2] = {
      .Color = {0.95, 0.9f, 0.2f},
      .Roughness = 0.95f
    };
    materials[3] = {
      .Color = {0.3, 0.39, 0.42},
      .Roughness = 0.95f
    };
    materials[4] = {
      .Color = {1.5, 1.5, 1.5},
      .Roughness = 0.95f,
      .Flags = TRACER_MATERIAL_FLAG_EMISSIVE
    };
    materials[5] = {
      .Color = {5, 6, 5},
      .Roughness = 0.0f,
      .Flags = TRACER_MATERIAL_FLAG_EMISSIVE
    };
    mRayBuffers.Init({.RayCapacity = (MEGA(4) + KILO(400))});
    mObjectBuffers.Init(&scratch, pCmdBuf, objCreateParam);
    tinybvh::BVH sphBVH[2] = {};
    tinybvh::BVHBase *bvh[2] = {&mBVHs[0], &mBVHs[1]};
    for (u32 i = 0; i < 2; i++) {
      sphBVH[i].customUserdata = V_(&s[i]);
      sphBVH[i].Build(&buildSphereAABB, 1);
      mBVHs[i].ConvertFrom(sphBVH[i]);
    }
    mInstances[0] = tinybvh::BLASInstance(0),
      mInstances[1] = tinybvh::BLASInstance(1);
    mTLAS.Build(mInstances, 2, bvh, 2);
    GPU::Stage_barrier barriers[2] = {
      {
	.SrcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
	.SrcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
	.DstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
	.DstAccessMask = VK_ACCESS_2_SHADER_READ_BIT
      },
      {
	.SrcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
	.SrcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
	.DstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
	.DstAccessMask = VK_ACCESS_2_SHADER_READ_BIT
      }
    };
    pCmdBuf->StageBarrier(barriers, 2);
  }

  void
  World::UpdateInputs(const App_input *pNewState) {
    mInput.States[Input_stream::PREVIOUS] = mInput.States[Input_stream::CURRENT];
    mInput.States[Input_stream::CURRENT] = *pNewState;
  }

  void
  World::Init() {
#if TRACER_SPECTRAL_MODE
    DEBUGLOG(stderr, "Spectral mode enabled.\n");
#endif 
    mScratchArena.Init(MEGA(2));
    uv2 swapchainDim = GPU::GetSwapchainDim();
    uv2 imgDim = {swapchainDim.x >> mFrameBufferInvRelSizePow2, swapchainDim.y >> mFrameBufferInvRelSizePow2};
    mBaseAttachment = GPU::CreateImage({
	.Format = VK_FORMAT_R32G32B32A32_SFLOAT,
	.Extent = {imgDim.x, imgDim.y, 1},
	.Usage  = VK_IMAGE_USAGE_STORAGE_BIT,
	.CreateAsStorage = true
      });
    // tonemapped
    mTonemapAttachment = GPU::CreateImage({
	.Format = VK_FORMAT_R8G8B8A8_UNORM,
	.Extent = {imgDim.x, imgDim.y, 1},
	.Usage = VK_IMAGE_USAGE_STORAGE_BIT |
	VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
	.CreateAsStorage = true
      });
    mSphereTestKernel = LoadKernel("dat/bin/comp_tracer_rt_sphere.spv");
    mShaderKernel = LoadKernel("dat/bin/comp_tracer_shade.spv");
    mScreenWriteKernel = LoadKernel("dat/bin/comp_tracer_sw.spv");
    mCamera = {
      .Position = {0, 0, 0}
    };
    mCamera.Forward = {0, 0, 1};
    mCamera.Up = {0, 1, 0};
    mCamera.VFov = RadFromDeg(45.0f);
    mCamera.FocusDistance = 3.4f;
    ReinitCamera(imgDim);
  }

  bool
  World::CheckMovement() {
    bool moved = false;
    const float spd = 0.1f;
    const float rotspd = 0.01f;
    quat caminv = nquatinv(mCameraQuat);
    v3 u = {0, 1, 0};
    v3 globalUpInLocalCoord = quatrotv3(caminv, u);
    // rotate left/right around global y-axis, not local
    bool mouseRightDown = mInput.MouseDown(1);
    iv2 mousePosDelta = mInput.MousePos(Input_stream::CURRENT) - mInput.MousePos(Input_stream::PREVIOUS);
    quat q = {0, 0, 0, 1};
    if (mouseRightDown) {
      q = quatangleaxis(rotspd * (mousePosDelta.x), globalUpInLocalCoord);
      q = quatangleaxis(rotspd * mousePosDelta.y, {1, 0, 0}) * q;
      moved = true;
    }
    quat temp = (q * mCameraQuat);
    mCameraQuat = temp;
    v3 f = quatrotv3(mCameraQuat, {0, 0, 1}); // forward
    v3 r = quatrotv3(mCameraQuat, {1, 0, 0}); // right
    bool inputs[] = {
      mInput.Down('A'),
      mInput.Down('D'),
      mInput.Down('S'),
      mInput.Down('W'),
      mInput.Down(VK_SHIFT),
      mInput.Down(VK_SPACE)
    };
    v3 dirs[] = {
      r * -spd,
      r * spd,
      f * -spd,
      f * spd,
      u * -spd,
      u * spd
    };
    for (u32 i = 0; i < 6; i++) {
      if (inputs[i]) {
	mCamera.Position += dirs[i];
	moved = true;
      }
    }
    if (moved) {
      mProgramSignals |= RAY_INTERSECT_PCD_NO_HYST_SIGNAL;
      ReinitCamera(GPU::GetImage(mBaseAttachment)->GetDim());
    }
    return moved;
  }

  void
  World::Tick(const GPU::Cmd_buf *pCmdBuf, i32 imageIndex) {
    if (mTickCount < 1) {
      FirstFrameLoad(pCmdBuf);
    }
    const u32 rayCapacity = mRayBuffers.RayChunkCapacity * TRACER_RAY_CHUNK_SIZE;
    GPU::Image *baseImg = GPU::GetImage(mBaseAttachment);
    const uv2 imageDim = baseImg->GetDim();
    GPU::Image *tonemapImg = GPU::GetImage(mTonemapAttachment);
    GPU::Image swapchainImage = GPU::GetSwapchainImage(imageIndex);
    u32 raysPerPixelPow2 = mRaysPerPixelPow2;
    const u32 totalRayCount = imageDim.x * imageDim.y * (1 << raysPerPixelPow2);
    pCmdBuf->ResetTimestamps();
    pCmdBuf->SetTimestamp(true, &mStartTime);
    bool moved = CheckMovement();
    if (moved && totalRayCount > rayCapacity) {
      //      raysPerPixelPow2 = 0;
    }
    // swapchain, to transfer dst
    GPU::Image_barrier barriers[3];
    barriers[0] = {
      .SrcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
      .SrcAccessMask = VK_ACCESS_2_NONE,
      .DstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
      .DstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
      .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .NewLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .Img = &swapchainImage
    };
    // base img, to shader write
    barriers[1] = {
      .SrcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
      .SrcAccessMask = VK_ACCESS_2_NONE,
      .DstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .DstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
      .NewLayout = VK_IMAGE_LAYOUT_GENERAL,
      .Img = baseImg
    };
    pCmdBuf->ImageBarrier(barriers, 2);
    // Render here
    pCmdBuf->BindDescriptors(VK_PIPELINE_BIND_POINT_COMPUTE);
    GPU::Bump_alloc *bmp = GPU::GetBumpAlloc(GPU::CPU_ACCESS);
    GPU::Pointer camP = bmp->Bump<Camera>(1);
    camP.Write(&mCamera, sizeof (mCamera));
    const u64 rayBufferPointer = mRayBuffers.RayBuffer.Gpu();
    // ray trace
    const u32 maxDepth = mMaxDepth;
    u32 rayIdxOffset = 0;
    u32 effRayCount = totalRayCount;
    // only render part of frame
    if (totalRayCount > rayCapacity) {
      // amount of frames needed to render to whole screen
      const u32 framesPerFullScreen = (totalRayCount + rayCapacity - 1) / rayCapacity;
      // global offset of ray idx
      rayIdxOffset = rayCapacity * mRayFrameCounter;
      mRayFrameCounter = (mRayFrameCounter + 1) % (framesPerFullScreen);
      // how many rays to trace this frame
      effRayCount = MIN(rayCapacity, totalRayCount - rayIdxOffset);
    } else {
      mRayFrameCounter = 0;
    }
    const u32 tick4 = (u32)(mTickCount * 2);
    const uv4 hash = { tick4, (u32)__rdtsc(), tick4 + 1, (u32)(mStartTime) };

    // unused if TRACER_PERSISTENT_THREADS = 0
    GPU::Pointer globalCountBuffer = bmp->Bump<u32>(1);
    const u32 defaultCount = 0;
    globalCountBuffer.Write(&defaultCount, sizeof (u32));
    
    for (u32 i = 0; i < maxDepth; i++) {
      pCmdBuf->SetKernel(mSphereTestKernel);
      Rt_pcd rt = {
	.Hash = hash,
	.CameraPointer = camP.Gpu(),
	.Signals = 0,
	.RayIdxOffset = rayIdxOffset,
	.Dimensions = imageDim,
	.RaysPerPixelPow2 = raysPerPixelPow2,
	.RayCount = effRayCount,
	.Depth = i,
	.PinholeRadius = (mProgramSignals & RAY_INTERSECT_PCD_NO_HYST_SIGNAL) ? 0.0f : 0.05f
      };
      rt.Buffers[RT_PCD_RAY_BUF] = rayBufferPointer;
      rt.Buffers[RT_PCD_TLAS_BUF] = mObjectBuffers.GPUTLAS[Object_type::SPHERE].Gpu();
      rt.Buffers[RT_PCD_OBJ_BUF] = mObjectBuffers.ObjectBuffer.Gpu();
#if !TRACER_RAY_HIT_UNIFIED
      rt.Buffers[RT_PCD_HIT_BUF] = mRayBuffers.HitBuffer.Gpu();
#endif
      rt.Buffers[RT_PCD_CNT_BUF] = globalCountBuffer.Gpu();
      // work group count
      uv3 wgc;
      // rays per thread
      u32 rpt = TRACER_RT_WORKGROUP_SIZE * TRACER_RT_COARSE_FACTOR;
      wgc.x = (effRayCount + rpt - 1) / rpt;
#if TRACER_PERSISTENT_THREADS
      wgc.x = 64;
#endif
      wgc.y = 1;
      wgc.z = 1;
      pCmdBuf->PushConstants(&rt);
      pCmdBuf->Dispatch(wgc);
      Rs_pcd rs = {
	.Hash = hash,
	.Dimensions = imageDim,
	.RayIdxOffset = rayIdxOffset,
	.Depth = i,
	.RayCount = effRayCount,
	.RaysPerPixelPow2 = raysPerPixelPow2,
	#if !TRACER_RAY_HIT_UNIFIED
	.HitBuffer = rt.Buffers[RT_PCD_HIT_BUF],
	#endif
	.MaterialBuffer = mObjectBuffers.MaterialBuffer.Gpu(),
	.RayBuffer = rt.Buffers[RT_PCD_RAY_BUF],
	.GlobalCountBuffer = globalCountBuffer.Gpu(),
	.SkyColor = {}//{0.9, 0.9, 0.97, 1.0}
      };
      rpt = TRACER_RS_WORKGROUP_SIZE * TRACER_RS_COARSE_FACTOR;
      wgc.x = (effRayCount + rpt - 1) / rpt;
      GPU::Stage_barrier barrier = {
	.SrcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
	.SrcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
	.DstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
	.DstAccessMask = VK_ACCESS_2_SHADER_READ_BIT
      };
      pCmdBuf->StageBarrier(&barrier, 1);
      pCmdBuf->SetKernel(mShaderKernel);
      pCmdBuf->PushConstants(&rs);
      pCmdBuf->Dispatch(wgc);
      pCmdBuf->StageBarrier(&barrier, 1);
    }
    // tonemap img, to general
    barriers[0] = {
      .SrcStageMask = 0,
      .SrcAccessMask = VK_ACCESS_2_NONE,
      .DstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .DstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
      .NewLayout = VK_IMAGE_LAYOUT_GENERAL,
      .Img = tonemapImg
    };
    pCmdBuf->ImageBarrier(barriers, 1);

    mAccumSamplesCount = mProgramSignals & RAY_INTERSECT_PCD_NO_HYST_SIGNAL ? 0.0f : mAccumSamplesCount;
    Sw_pcd sw = {
      .Dimensions = imageDim,
      .ImageStorageHandle = baseImg->GetStorageImageIndex(),
      .TonemapImageStorageHandle = tonemapImg->GetStorageImageIndex(),
      .AccumCount = mAccumSamplesCount,
      .RaysPerPixelPow2 = raysPerPixelPow2,
      .RayBuffer = rayBufferPointer,
      .RayIdxOffset = rayIdxOffset,
      .RayCount = effRayCount
    };
    // even if we take >1 spp, we avg. them in the shader, treating them as one.
    mAccumSamplesCount += 1;
    
    pCmdBuf->SetKernel(mScreenWriteKernel);
    pCmdBuf->PushConstants(&sw);
    uv3 wgc = {
      .x = ((imageDim.x) + 32 - 1) / 32,
      .y = ((imageDim.y) + 32 - 1) / 32,
      .z = 1
    };
    pCmdBuf->Dispatch(wgc);
    
    // tonemap img, to transfer src
    barriers[0] = {
      .SrcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .SrcAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
      .DstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
      .DstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
      .NewLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      .Img = tonemapImg
    };
    pCmdBuf->ImageBarrier(barriers, 1);
    pCmdBuf->BlitEntireColorImage(tonemapImg, &swapchainImage, 0, 0, VK_FILTER_NEAREST);
    
    // swapchain, to present
    barriers[0] = {
      .SrcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
      .SrcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
      .DstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
      .DstAccessMask = VK_ACCESS_2_NONE,
      .NewLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      .Img = &swapchainImage
    };
    pCmdBuf->ImageBarrier(barriers, 1);
    pCmdBuf->SetTimestamp(false, &mEndTime);
#if 1
    if (mStartTime && mEndTime > mStartTime) {
      double t = GPU::GetMicrosecondsElapsed(mStartTime, mEndTime);
      mAccumTime += t;
      mAccumTimeCounts += 1.0;
      double avg = mAccumTime / mAccumTimeCounts;
      // if the difference is too big, printing average wont help much; print t instead and reset accums
      double d = (t / avg);
      if (d > 1.3 || d < (1.0 / 1.3)) {
	avg = t;
	mAccumTime = mAccumTimeCounts = 0.0;
      }
      DEBUGLOG("rays traced: %u. \tavg time: %f", effRayCount, avg);
    }
#endif
    // reset signals
    mProgramSignals = 0;
    mTickCount = MAX(1, mTickCount + 1);
  }

  void
  World::OnResize(uv2 newDim) {
    if (newDim.x > 0) {
      uv2 fbDim = { newDim.x >> mFrameBufferInvRelSizePow2, newDim.y >> mFrameBufferInvRelSizePow2 };
      ResizeImage(mBaseAttachment, {fbDim.x, fbDim.y, 1});
      ResizeImage(mTonemapAttachment, {fbDim.x, fbDim.y, 1});
      ReinitCamera(fbDim);
      // reset averages
      mAccumTime = 0.0;
      mAccumTimeCounts = 0.0;
    }
  }
}
