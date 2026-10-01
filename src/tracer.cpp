#include "tracer.h"

namespace Tracer {

  void
  Ray_trace_buffers::Init(const Ray_trace_buffers_create_param &param) {
    this->Hash = 0;
    this->RayChunkCapacity = (param.RayCapacity + TRACER_RAY_CHUNK_SIZE) / TRACER_RAY_CHUNK_SIZE;
    this->RayBuffer = GPU::Malloc(sizeof (Ray_chunk) * this->RayChunkCapacity, GPU::GPU_PRIVATE);
    this->HitBuffer = GPU::Malloc(sizeof (Hit_record_chunk) * this->RayChunkCapacity, GPU::GPU_PRIVATE);
  }

  void
  Ray_trace_buffers::Free() {
    GPU::Free(this->RayBuffer);
    GPU::Free(this->HitBuffer);
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
    
    mCamera.AspectRatio = (float)mCamera.PixelWidth / (float)mCamera.PixelHeight;
    float th = mCamera.VFov;
    float h = tri::tanf(th);
    float vpH = 2 * h * mCamera.FocusDistance;
    float vpW = mCamera.AspectRatio * vpH;
    v3 cameraRight = cross(mCamera.Up, mCamera.Forward); // cross
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
    mSphereCount = 128;
    mSphereBuffer = GPU::Malloc(sizeof (Sphere) * mSphereCount, GPU::GPU_PRIVATE);
    GPU::Bump_alloc *bumpAlloc = GPU::GetBumpAlloc(GPU::CPU_ACCESS);
    GPU::Pointer staging = bumpAlloc->Bump<Sphere>(mSphereCount);
    Sphere *s = scratch.Push<Sphere>(mSphereCount);
    Material *materials = scratch.Push<Material>(4);
    u32 *materialIndices = scratch.Push<u32>(mSphereCount);
    Object_buffers_create_param objCreateParam = {
      .Materials = materials,
      .MaterialCount = 4,
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
    for (u32 i = 1; i < mSphereCount; i++) {
      s[i].Position.x = -10.0f + (float)(i % 20);
      s[i].Position.y = i / 30;
      s[i].Position.z = 6 + i / 20;
      s[i].Radius = (float)(2 + i % 7) * 0.1;
      materialIndices[i] = hash1(i) % 3;
    }
    materials[0] = {
      .Color = {0.999, 0.99, 0.999},
      .Roughness = 0.5f,
      .RefractionIndex = 1.5f
    };
    materials[1] = {
      .Color = {1.0, 0.0, 0.0},
      .Roughness = 0.25f,
      .Flags = TRACER_MATERIAL_FLAG_METALLIC
    };
    materials[2] = {
      .Color = {1.0, 0.7, 0.2},
      .Roughness = 0.95f
    };
    materials[3] = {
      .Color = {0.3, 0.39, 0.42},
      .Roughness = 0.3f
    };
    mRayBuffers.Init({.RayCapacity = MEGA(2)});
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
    staging.Write(&s, sizeof (s));
    pCmdBuf->MemCpy(staging, mSphereBuffer, sizeof (s));
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
  World::Init() {
    mScratchArena.Init(MEGA(2));
    uv2 swapchainDim = GPU::GetSwapchainDim();
    uv2 imgDim = {swapchainDim.x >> mFrameBufferInvRelSizePow2, swapchainDim.y >> mFrameBufferInvRelSizePow2};
    mTestAttachment = GPU::CreateImage({
	.Format = VK_FORMAT_R32G32B32A32_SFLOAT,
	.Extent = {imgDim.x, imgDim.y, 1},
	.Usage = VK_IMAGE_USAGE_SAMPLED_BIT |
	VK_IMAGE_USAGE_STORAGE_BIT |
	VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
	.CreateAsStorage = true
      });
    mTestKernel = LoadKernel("dat/bin/comp_testfill.spv");
    mSphereTestKernel = LoadKernel("dat/bin/comp_tracer_rt_sphere.spv");
    mShaderKernel = LoadKernel("dat/bin/comp_tracer_shade.spv");
    mScreenWriteKernel = LoadKernel("dat/bin/comp_tracer_sw.spv");
    mCamera = {
      .Position = {0, 0, 0}
    };
    mCamera.Forward = {0, 0, 1};
    mCamera.Up = {0, 1, 0};
    mCamera.VFov = RadFromDeg(45.0f);
    mCamera.FocusDistance = 30.0f;
    ReinitCamera(imgDim);
  }

  void
  World::Tick(const GPU::Cmd_buf *pCmdBuf, i32 imageIndex) {
    GPU::Image swapchainImage = GPU::GetSwapchainImage(imageIndex);
    pCmdBuf->ResetTimestamps();
    pCmdBuf->SetTimestamp(true, &mStartTime);
    if (mTickCount < 1) {
      FirstFrameLoad(pCmdBuf);
    }

    if (GetKeyState(VK_SPACE)) {
      mCamera.Position.y += 0.03;
      mProgramSignals |= RAY_INTERSECT_PCD_NO_HYST_SIGNAL;
    }
    if (GetKeyState('W')) {
      mCamera.Position.z += 0.03;
      mProgramSignals |= RAY_INTERSECT_PCD_NO_HYST_SIGNAL;
    }
    if (GetKeyState('D')) {
      mCamera.Position.x += 0.03;
      mProgramSignals |= RAY_INTERSECT_PCD_NO_HYST_SIGNAL;
    }
    if (GetKeyState('M')) {
      mProgramSignals |= RAY_INTERSECT_PCD_NO_HYST_SIGNAL;
    }

    // swapchain, to transfer dst
    GPU::Image_barrier barriers[2];
    barriers[0] = {
      .SrcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
      .SrcAccessMask = VK_ACCESS_2_NONE,
      .DstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
      .DstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
      .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .NewLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .Img = &swapchainImage
    };
    // img, to shader write
    barriers[1] = {
      .SrcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
      .SrcAccessMask = VK_ACCESS_2_NONE,
      .DstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .DstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
      .NewLayout = VK_IMAGE_LAYOUT_GENERAL,
      .Img = GPU::GetImage(mTestAttachment)
    };
    pCmdBuf->ImageBarrier(barriers, 1);
    
    // Render here
    uv3 c;
    pCmdBuf->BindDescriptors(VK_PIPELINE_BIND_POINT_COMPUTE);
    const GPU::Image *attachImg = GPU::GetImage(mTestAttachment);
    GPU::Bump_alloc *bmp = GPU::GetBumpAlloc(GPU::CPU_ACCESS);
    GPU::Pointer camP = bmp->Bump<Camera>(1);
    camP.Write(&mCamera, sizeof (mCamera));
    const uv2 imageDim = attachImg->GetDim();
    const u64 rayBufferPointer = mRayBuffers.RayBuffer.Gpu();
    // ray trace
    const u32 maxDepth = 15;
    const u32 raysPerPixelPow2 = mRaysPerPixelPow2;
    const u32 rayCount = imageDim.x * imageDim.y * (1 << raysPerPixelPow2);
    ASSERT(rayCount < mRayBuffers.RayChunkCapacity * TRACER_RAY_CHUNK_SIZE, "Too many rays!");
    const u32 hash = (u32)mTickCount / 4;
    for (u32 i = 0; i < maxDepth; i++) {
      pCmdBuf->SetKernel(mSphereTestKernel);
      Rt_pcd rt = {
	.CameraPointer = camP.Gpu(),
	.Hash = hash,
	.Signals = 0,
	.Dimensions = imageDim,
	.RaysPerPixelPow2 = raysPerPixelPow2,
	.RayCount = rayCount,
	.Depth = i
      };
      rt.Buffers[RT_PCD_RAY_BUF] = rayBufferPointer;
      rt.Buffers[RT_PCD_TLAS_BUF] = mObjectBuffers.GPUTLAS[Object_type::SPHERE].Gpu();
      rt.Buffers[RT_PCD_OBJ_BUF] = mObjectBuffers.ObjectBuffer.Gpu();
      rt.Buffers[RT_PCD_HIT_BUF] = mRayBuffers.HitBuffer.Gpu();
      // ray shade
      const u32 m = TRACER_RT_WORKGROUP_SIZE * TRACER_RT_COARSE_FACTOR;
      c.x = (rayCount + m - 1) / m;
      c.y = 1;
      c.z = 1;
      pCmdBuf->PushConstants(&rt);
      pCmdBuf->Dispatch(c);
      Rs_pcd rs = {
	.Dimensions = imageDim,
	.Hash = hash,
	.ImageStorageHandle = attachImg->GetStorageImageIndex(),
	.Depth = i,
	.RayCount = rayCount,
	.RaysPerPixelPow2 = raysPerPixelPow2,
	.HitBuffer = rt.Buffers[RT_PCD_HIT_BUF],
	.MaterialBuffer = mObjectBuffers.MaterialBuffer.Gpu(),
	.RayBuffer = rt.Buffers[RT_PCD_RAY_BUF]
      };
      c.x = (rayCount + TRACER_RS_WORKGROUP_SIZE - 1) / TRACER_RS_WORKGROUP_SIZE;
      GPU::Stage_barrier barrier = {
	.SrcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
	.SrcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
	.DstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
	.DstAccessMask = VK_ACCESS_2_SHADER_READ_BIT
      };
      pCmdBuf->StageBarrier(&barrier, 1);
      pCmdBuf->SetKernel(mShaderKernel);
      pCmdBuf->PushConstants(&rs);
      pCmdBuf->Dispatch(c);
      pCmdBuf->StageBarrier(&barrier, 1);
    }

    mAccumCount = mProgramSignals & RAY_INTERSECT_PCD_NO_HYST_SIGNAL ? 0.0f : mAccumCount;
    Sw_pcd sw = {
      .Dimensions = imageDim,
      .ImageStorageHandle = attachImg->GetStorageImageIndex(),
      .AccumCount = mAccumCount,
      .RaysPerPixelPow2 = raysPerPixelPow2,
      .RayBuffer = rayBufferPointer
    };
    mAccumCount += 1;
    
    pCmdBuf->SetKernel(mScreenWriteKernel);
    pCmdBuf->PushConstants(&sw);
    c = {
      .x = ((imageDim.x * imageDim.y) + TRACER_SW_WORKGROUP_SIZE - 1) / TRACER_SW_WORKGROUP_SIZE,
      .y = 1,
      .z = 1
    };
    pCmdBuf->Dispatch(c);
    // img, to transfer src
    barriers[0] = {
      .SrcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .SrcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
      .DstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
      .DstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
      .NewLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      .Img = GPU::GetImage(mTestAttachment)
    };
    pCmdBuf->ImageBarrier(barriers, 1);
    pCmdBuf->BlitEntireColorImage(GPU::GetImage(mTestAttachment), &swapchainImage, 0, 0, VK_FILTER_NEAREST);
    
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
    if (mStartTime && mEndTime) {
      double t = GPU::GetMicrosecondsElapsed(mStartTime, mEndTime);
      mAccumTime += t;
      mAccumCounts += 1.0;
      double avg = mAccumTime / mAccumCounts;
      fprintf(stderr, "rays traced: %u. \tavg time: %f\n", rayCount, avg);
    }
    // reset signals
    mProgramSignals = 0;
    mTickCount = MAX(1, mTickCount + 1);
  }

  void
  World::OnResize(uv2 newDim) {
    if (newDim.x > 0) {
      uv2 fbDim = { newDim.x >> mFrameBufferInvRelSizePow2, newDim.y >> mFrameBufferInvRelSizePow2 };
      ResizeImage(mTestAttachment, {fbDim.x, fbDim.y, 1});
      ReinitCamera(fbDim);
      // reset averages
      mAccumTime = 0.0;
      mAccumCounts = 0.0;
    }
  }
}
