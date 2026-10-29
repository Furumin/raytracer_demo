#pragma once
#include "tracer_interop.h"

namespace Tracer {
#define TRACER_OBJECT_TYPE_COUNT (2)
  enum Object_type {
    SPHERE = 0,
    MESH = 1
  };

  struct Ray_trace_buffers_create_param {
    u32 RayCapacity = 32000; // obligatory
  };
  
  struct Ray_trace_buffers {
    GPU::Pointer RayBuffer; // populated at step 1
    GPU::Pointer HitBuffer; // populated at step 2
    u32 Hash;
    u32 RayChunkCapacity;
    void
    Init(const Ray_trace_buffers_create_param &param);
    void
    Free();
  };

  struct Object_buffers_create_param {
    Material *Materials;
    u32 MaterialCount;
    //    u32 *SphereMaterialIndices;
    //    void *SpherePositions; // vec3
    //    u32 SpherePositionStride = sizeof (v3); // defa
    //    void *SphereRadii; // float
    //    u32 SphereRadiusStride = sizeof (float);
    Sphere_object *Spheres;
    u32 SphereCount;
  };

  struct Object_buffers {
    tinybvh::BVH_GPU CPUTLAS[1];
    GPU::Pointer GPUTLAS[1];
    GPU::Pointer ObjectBuffer;
    GPU::Pointer MaterialBuffer;
    GPU::Pointer CollisionDataBuffers[TRACER_OBJECT_TYPE_COUNT];
    u32 ObjectCount;
    u32 CollisionDataCounts[TRACER_OBJECT_TYPE_COUNT];

    void
    Init(Scratch *pScratch, const GPU::Cmd_buf *pCmdBuf, const Object_buffers_create_param &param);

    void
    Free();
  };
  
  class World {
  private:
    Scratch_arena mScratchArena;
    Input_stream mInput;
    GPU::ImageHandle mBaseAttachment;
    GPU::ImageHandle mTonemapAttachment;    
    GPU::KernelHandle mSphereTestKernel;   // tracer_rt_sphere.comp
    GPU::KernelHandle mShaderKernel;       // tracer_shade.comp
    GPU::KernelHandle mScreenWriteKernel;  // tracer_sw.comp
    
    // (todo) double-buffer this if CPU is able to modify
    Object_buffers mObjectBuffers;
    
    Ray_trace_buffers mRayBuffers;

    quat mCameraQuat = {0, 0, 0, 1};
    Camera mCamera;

    u64 mTickCount = 0;
    u32 mProgramSignals = 0; // reset every frame
    u32 mPermanentProgramSignals = 0; // never reset
    u32 mSphereCount;

    v4 mSkyColor;
    v4 mSunData;
    u32 mSunObjectIndex;
    u32 mSunMaterialIndex;

    // timestamps
    u64 mStartTime = 0, mEndTime = 0;
    double mAccumTime = 0;
    double mAccumTimeCounts = 0;

    // amount of previous samples taken per pixel
    // reset this when objects/camera change!
    float mAccumSamplesCount = 0;

    u32 mFrameBufferInvRelSizePow2 = 0;
    u32 mRaysPerPixelPow2 = 0;
    u32 mMaxDepth = 15;

    u32 mRayFrameCounter = 0;

    enum LANG {
      JP = 0,
      //      EN = 1 /* Unused */
    };
    u32 Language = JP;

    enum LABEL_ID {
      CONTROLS = 0,
      CONTROLS_MOVE,
      CONTROLS_V_MOVE,
      CONTROLS_ROTATE,
      STATS,
      TICK_COUNTS,
      ACCUM_SPP,
      FRAME_HASH,
      SC_DIM,
      FB_DIM,
      RAY_COUNT,
      FRAME_TIME,
      WARNING,
      CAMERA,
      CAM_POS,
      VFOV,
      FOCUS_DISTANCE,
      DEFOCUS_ANGLE,
      OTHER,
      COLOR_ACCUMULATION,
      ANTI_ALIASING,
      STRATIFIED_SAMPLING,
      IMPORTANCE_SAMPLING,
      RUSSIAN_ROULETTE,
      CORRECT_RECONSTRUCTION,
      TURN_OFF,
      MAX_DEPTH,
      FB_SIZE_REL_WINDOW,
      SKY_COLOR
    };
    
    struct Label_db {
      const char *Labels[128];
    };
    Label_db LabelDB[2] = {
      {
	"操作方法",
	"WASD : 移動",
	"SPACE/SHIFT : 乗降",
	"右マウスを押したままマウス移動 : カメラ",
	"一般情報",
	"フレーム数 / 蓄積開始から : %llu / %llu",
	"蓄積された標本数 : %u",
	"ハッシュのシード : %u, %u, %u, %u",
	"swapchain寸法 : (%u, %u)",
	"フレームバッファ寸法 : (%u, %u)",
	"光線数 / 光線バッファー容量 : %u / %u",
	"フレーム時間 (マイクロ秒) / FPS : %.2f / %.2f",
	"キャパオーバーのため部分的描画が有効になっています。",
	"キャメラ",
	"位置 : {%.2f, %.2f, %.2f}",
	"垂直視野角",
	"被写界深度",
	"デフォーカス角",
	"その他",
	"色の蓄積",
	"アンチエイリアシング",
	"層化サンプリング",
	"重要度サンプリング",
	"ロシアンルーレット",
	"再構成フィルター",
	"%sを無効化",
	"最大反射回数",
	"フレームバッファ寸法",
	"空色"
      },
      {
	"Controls",
	"WASD: Move",
	"SHIFT/SPACE: Move vertically",
	"Right mouse: Rotate"
      }
    };

    GPU::KernelHandle
    LoadKernel(const char *path);
    void
    ReinitCamera(uv2 pixelImageDim);
    void
    FirstFrameLoad(const GPU::Cmd_buf *pCmdBuf);
    bool
    PollMovement();
    struct Editor_params {
      u32 RayCountThisFrame;
      u32 TotalRayCount;
      u32 RayCapacity;
    };
    void
    PollEditor(const Editor_params &params);
    void
    RenderEditor(GPU::Image *pSwapchainImg, const GPU::Cmd_buf *pCmdBuf);
    const char*
    GetLabel(const LABEL_ID labelId);
  public:
    void
    UpdateInputs(const App_input *pNewState);
    void
    Init();
    void
    Tick(const GPU::Cmd_buf *pCmdBuf, i32 imageIndex);
    void
    OnResize(uv2 newDim);
  };
}
