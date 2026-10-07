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

  void buildCustomTLAS(const u32 primID, tinybvh::bvhvec3 &bMin, tinybvh::bvhvec3 &bMax, void *data) {
    Object *obj = &((Object*)data)[primID];
    ASSERT(obj->Type == Object_type::SPHERE, "Only sphere TLAS is supported!");
    tinybvh::bvhvec3 pos {obj->Transform[3], obj->Transform[7], obj->Transform[11]};
    // diagonals in matrix store scale, which equals radius for a sphere
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
    // write objects, bvh to GPU. tinybvh reorders primitives, so we have to account for that
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

    // materials, insert each material into AoSoA chunk
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
	  // set to metallic in order to avoid the pdf logic in diffuse
	  if (param.Materials[matI].Flags & TRACER_MATERIAL_FLAG_EMISSIVE) {
	    param.Materials[matI].Flags |= TRACER_MATERIAL_FLAG_METALLIC;
	  }
	  mc[i].Flags[j] = param.Materials[matI].Flags;
	}
      }
    }
  }

  void
  Object_buffers::Free() {
    GPU::Free(this->ObjectBuffer);
    GPU::Free(this->GPUTLAS[Object_type::SPHERE]);
  }

  GPU::KernelHandle
  World::LoadKernel(const char *path) {
    Util::File f;
    int res = Util::OpenFile(path, Util::IO_FILE_READ | Util::IO_FILE_SHARE_READ | Util::IO_OPEN_ONLY | Util::IO_FLAG_MEM_MAP, &f, 0);
    if (res) {
      char buf[2048];
      sprintf(buf, "Could not open compute kernel with path %s", path);
      Util::ErrMsgBox(buf);
      return NULL_HANDLE;
    }
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

    mProgramSignals |= TRACER_PS_NO_ACCUM_BIT;
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
      materialIndices[i] = hash1(i) % 4;
    }
    // sun
    mSunData = {0, 15.0f, 50.0f, 6.0f};
    for (mSunObjectIndex = i; i < mSphereCount; i++) {
      s[i].Position.x = mSunData.x;
      s[i].Position.y = mSunData.y;
      s[i].Position.z = mSunData.z;
      s[i].Radius = mSunData.w;
      materialIndices[i] = 5;
      mSunMaterialIndex = materialIndices[i];
    }
    // lights
    materials[0] = {
      .Color = {0.95, 0.1, 0.2},
      .Roughness = 0.05f,
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
      .Roughness = 0.999f
    };
    materials[4] = {
      .Color = {0.2, 3.5, 4.5},
      .Roughness = 0.95f,
      .Flags = TRACER_MATERIAL_FLAG_EMISSIVE
    };
    materials[5] = {
      .Color = {90, 100, 140},
      .Roughness = 0.0f,
      .Flags = TRACER_MATERIAL_FLAG_EMISSIVE
    };
    mRayBuffers.Init({.RayCapacity = (MEGA(4) + KILO(400))});
    mObjectBuffers.Init(&scratch, pCmdBuf, objCreateParam);
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
    if (!(mSphereTestKernel.IsValid() && mShaderKernel.IsValid() && mScreenWriteKernel.IsValid())) {
      mPermanentProgramSignals |= TRACER_PS_SKIP_EXEC_BIT;
    }
    mCamera = {
      .Position = {0, 0, 0}
    };
    mCamera.Forward = {0, 0, 1};
    mCamera.Up = {0, 1, 0};
    mCamera.VFov = RadFromDeg(45.0f);
    mCamera.FocusDistance = 10.0f;
    ReinitCamera(imgDim);
    mProgramSignals |= TRACER_PS_NO_ACCUM_BIT;
  }

  bool
  World::PollMovement() {
    bool moved = false;
    const float spd = 0.1f;
    const float rotspd = 0.4f * (mCamera.VFov / TRACER_MAX_CAMERA_VFOV);
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
      mProgramSignals |= TRACER_PS_NO_ACCUM_BIT | TRACER_PS_NO_AA_BIT;
      ReinitCamera(GPU::GetImage(mBaseAttachment)->GetDim());
    }
    return moved;
  }

  static bool
  ShouldEnablePartialRendering(u32 rayCount, u32 rayCapa) {
    return rayCount > rayCapa;
  }

  uv4
  GetHash(const u64 tickCount, const u32 maxDepth, const u32 i) {
    const u32 baseHash = ((tickCount * maxDepth + i) * 4);
    const uv4 hash = { baseHash + i, baseHash + i + 1, baseHash + i + 2, baseHash + i + 3};
    return hash;
  }

  void
  World::PollEditor(const Editor_params &params) {
    bool resetTimer = false;
    bool resetColorAccum = false;
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    const uv2 swapchainDim = GPU::GetSwapchainDim();
    const uv2 fbDim = GPU::GetImage(mBaseAttachment)->GetDim();
    const float maximumWindowHeight = swapchainDim.y * 0.45f;
    ImGui::SetNextWindowPos(ImVec2());
    ImGui::Begin("Editor");
    ImGui::SetWindowSize(ImVec2(ImGui::GetWindowWidth(), maximumWindowHeight));
    if (ImGui::CollapsingHeader("Controls")) {
      ImGui::Text("WASD: Move");
      ImGui::Text("SHIFT/SPACE: Move vertically");
      ImGui::Text("Right mouse: Rotate");
    }
    ImGui::SeparatorText("Stats");
    {
      // debug button status
      ImGui::PushItemWidth(ImGui::GetFontSize() * 8.0f);
      ImGui::Text("Tick count: %llu", mTickCount);
      const uv4 thisFrameHash = GetHash(mTickCount, mMaxDepth, 0);
      ImGui::Text("This frame's hash (at depth 0): { %u, %u, %u, %u }", thisFrameHash.x, thisFrameHash.y, thisFrameHash.z, thisFrameHash.w);
      ImGui::Text("Swapchain dim.: %u, %u", swapchainDim.x, swapchainDim.y);
      ImGui::Text("Framebuffer dim.: %u, %u", fbDim.x, fbDim.y);
      if (mStartTime && mEndTime > mStartTime) {
	double t = GPU::GetMicrosecondsElapsed(mStartTime, mEndTime);
	mAccumTime += t;
	mAccumTimeCounts += 1.0;
	double avg = mAccumTime / mAccumTimeCounts;
	ImGui::Text("Ray count / Ray buffer capacity : %u / %u", params.RayCountThisFrame, params.RayCapacity);
	ImGui::Text("Trace time (microsecs) / FPS: %f / %f", avg, (1000000.0) / avg);
      }
      if (ShouldEnablePartialRendering(params.TotalRayCount, params.RayCapacity)) {
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.99f, 1.0f, 0.1f, 1.0f));
	ImGui::Text("WARNING: Partial rendering is currently on.");
	ImGui::PopStyleColor(1);
      }
      ImGui::PopItemWidth();
      ImGui::SeparatorText("Camera");
      ImGui::PushItemWidth(MIN(ImGui::GetContentRegionAvail().x * 0.6f, 80.0f));
      float degvfov = DegFromRad(mCamera.VFov);
      resetColorAccum |= ImGui::InputFloat("Vertical FOV", &degvfov, 0.1f, 1.0f, "%.1f");
      float newVFov = MIN(RadFromDeg(degvfov), TRACER_MAX_CAMERA_VFOV);
      if (abs(newVFov - mCamera.VFov) > FLOAT_EPS) {
	mCamera.VFov = newVFov;
	ReinitCamera(fbDim);
      }
      resetColorAccum |= ImGui::InputFloat("Focus distance", &mCamera.FocusDistance, 0.1f, 1.0f, "%.1f");
      resetColorAccum |= ImGui::InputFloat("Defocus angle (deg)", &mCamera.DefocusAngle, 0.01f, 0.1f);
    }
    ImGui::SeparatorText("Other");
    {
      bool pdfOff = mPermanentProgramSignals & TRACER_PS_NO_PDF_BIT;
      u32 bits[] = {
	TRACER_PS_NO_ACCUM_BIT,
	TRACER_PS_NO_AA_BIT,
	TRACER_PS_NO_SS_BIT,
	TRACER_PS_NO_PDF_BIT,
      };
      const char* names[] = {
	"color accumulation",
	"anti-aliasing",
	"stratified sampling",
	"importance sampling"
      };
      char buf[1024];
      for (u32 i = 0; i < ARRAYCOUNT(bits); i++) {
	bool status = mPermanentProgramSignals & bits[i];
	sprintf(buf, "Turn off %s", names[i]);
	ImGui::Checkbox(buf, &status);
	mPermanentProgramSignals = status ? (mPermanentProgramSignals | bits[i]) : (mPermanentProgramSignals & ~(bits[i]));
      }
      if (pdfOff != (bool)(mPermanentProgramSignals & TRACER_PS_NO_PDF_BIT)) {
	mProgramSignals |= TRACER_PS_NO_ACCUM_BIT;
      }
      i32 d = mMaxDepth;
      resetTimer |= ImGui::SliderInt("Depth", &d, 0, TRACER_MAX_DEPTH);
      mMaxDepth = d;
      d = mRaysPerPixelPow2;
      bool changeSPP = ImGui::SliderInt("SPP (pow 2)", &d, 0, 8);
      mRaysPerPixelPow2 = d;
      ImGui::SameLine(); ImGui::Text("SPP: %u", (1 << mRaysPerPixelPow2));
      resetTimer |= changeSPP;
      resetColorAccum |= changeSPP;
      d = mFrameBufferInvRelSizePow2;
      bool changeDim = ImGui::SliderInt("Framebuffer size rel. window (pow 2)", &d, 0, 4);
      ImGui::PopItemWidth();
      resetColorAccum |= ImGui::SliderFloat4("Sky color", mSkyColor.v, 0.0f, 1.0f);
      mFrameBufferInvRelSizePow2 = d;
      if (changeDim) {
	GPU::WaitIdle();
	OnResize(swapchainDim);
      }
    }
    ImGui::End();

    if (resetTimer) {
      mAccumTime = mAccumTimeCounts = 0.0;
    }
    if (resetColorAccum) {
      mProgramSignals |= TRACER_PS_NO_ACCUM_BIT;
    }
  }

  void
  World::RenderEditor(GPU::Image *swapchainImg, const GPU::Cmd_buf *pCmdBuf) {
    ImGui::Render();
    GPU::Attachment_info info = {
      .Img = swapchainImg
    };
    pCmdBuf->BeginRendering({
	.LayerCount = 1,
	.ColorAttachmentCount = 1,
	.PColorAttachments = &info,
      }, {}, GPU::GetSwapchainDim());
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), pCmdBuf->CommandBuffer);
    pCmdBuf->EndRendering();
  }

  void
  World::Tick(const GPU::Cmd_buf *pCmdBuf, i32 imageIndex) {
    // may be set externally (via callback) so must check before resetting
    bool extResized = (bool)(mProgramSignals & TRACER_PS_EXT_RESIZED_BIT);
    mProgramSignals = mPermanentProgramSignals;
    if (mProgramSignals & TRACER_PS_SKIP_EXEC_BIT) return;
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
    u32 rayIdxOffset = 0;
    u32 effRayCount = totalRayCount;
    pCmdBuf->ResetTimestamps();
    pCmdBuf->SetTimestamp(true, &mStartTime);
    
    // may set a signal, so important to poll editor after signal reset
    PollMovement();
    PollEditor({
	.RayCountThisFrame = effRayCount,
	.TotalRayCount = totalRayCount,
	.RayCapacity = rayCapacity});
    
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
    Camera cam = mCamera;
    // turn off DoF if not currently accumulating
    if (mProgramSignals & TRACER_PS_NO_ACCUM_BIT)
      cam.DefocusAngle = 0.0f;
    GPU::Pointer camP = bmp->Bump<Camera>(1);
    camP.Write(&cam, sizeof (cam));
    const u64 rayBufferPointer = mRayBuffers.RayBuffer.Gpu();
    // ray trace
    const u32 maxDepth = mMaxDepth;
    // only render part of frame
    if (ShouldEnablePartialRendering(totalRayCount, rayCapacity)) {
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
    // unused if TRACER_PERSISTENT_THREADS = 0
    GPU::Pointer globalCountBuffer = bmp->Bump<u32>(1);
    const u32 defaultCount = 0;
    globalCountBuffer.Write(&defaultCount, sizeof (u32));
    
    for (u32 i = 0; i < maxDepth; i++) {
      const uv4 hash = GetHash(mTickCount, maxDepth, i);
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
	.EnableAntiAliasing = (u32)(mProgramSignals & TRACER_PS_NO_AA_BIT ? (0) : (1)),
	.EnableStratifiedSampling = (u32)(mProgramSignals & TRACER_PS_NO_SS_BIT ? (0) : (1))
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
      // shading pass has a different format for depth, due to needing to determine
      // if a ray terminated without hitting light or not 
      Rs_pcd rs = {
	.Hash = hash,
	.Dimensions = imageDim,
	.RayIdxOffset = rayIdxOffset,
	// high 16 are depth, LSB is last bounce flag
	.DepthAndFlags = (i << 16) |
  	  (i == maxDepth - 1 ? TRACER_RS_LAST_BOUNCE_BIT : 0) |
	  ((mProgramSignals & TRACER_PS_NO_PDF_BIT) ? TRACER_RS_NO_PDF_BIT : 0),
	.RayCount = effRayCount,
	.RaysPerPixelPow2 = raysPerPixelPow2,
#if !TRACER_RAY_HIT_UNIFIED
	.HitBuffer = rt.Buffers[RT_PCD_HIT_BUF],
#endif
	.MaterialBuffer = mObjectBuffers.MaterialBuffer.Gpu(),
	.RayBuffer = rt.Buffers[RT_PCD_RAY_BUF],
	.GlobalCountBuffer = globalCountBuffer.Gpu(),
	.SkyColor = mSkyColor,
	.SunData = mSunData
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

    mAccumSamplesCount = extResized || (mProgramSignals & TRACER_PS_NO_ACCUM_BIT) ? 0.0f : mAccumSamplesCount;
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
      .v = {
	((imageDim.x) + 32 - 1) / 32,
	((imageDim.y) + 32 - 1) / 32,
	1
      }
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
    
    // swapchain, to clr attach opt (render editor)
    barriers[0] = {
      .SrcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
      .SrcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
      .DstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      .DstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      .NewLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .Img = &swapchainImage
    };
    pCmdBuf->ImageBarrier(barriers, 1);
    pCmdBuf->SetTimestamp(false, &mEndTime);
    
    RenderEditor(&swapchainImage, pCmdBuf);

    // swapchain, to present opt
    barriers[0] = {
      .SrcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      .SrcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      .DstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
      .DstAccessMask = VK_ACCESS_2_NONE,
      .NewLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      .Img = &swapchainImage
    };
    pCmdBuf->ImageBarrier(barriers, 1);
    // reset signals
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
      mProgramSignals |= TRACER_PS_EXT_RESIZED_BIT;
    }
  }
}
