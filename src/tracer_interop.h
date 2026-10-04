#if APP_CPP_CODE
#pragma once
namespace Tracer {
#define f16 u16 // do not access from cpu !!
#define f16v4 uv2
#else
#define f16 float16_t
#define f16v4 f16vec4
#define u32 uint
#define u64 uvec2 // don't assume 64-bit support in shaders
#define uv2 uvec2
#define uv3 uvec3
#define uv4 uvec4
#define v2 vec2
#define v3 vec3
#define v4 vec4
  
#endif

#define TRACER_RAY_HIT_UNIFIED (1)
#define TRACER_PERSISTENT_THREADS (0)
#define TRACER_USE_F16_COLOR (1)
  struct Camera {
    v3 Position; // user
    float AspectRatio; // cached, used at runtime
    
    v3 Forward; // user
    u32 PixelWidth; // user
    
    v3 Up; // user
    u32 PixelHeight; // user

    v3 Right;
    u32 _PADDING0[1];
    
    v3 DeltaU; // cached, used at runtime to find world-space points that correspond to each image pixel
    float VFov; // user
    
    v3 DeltaV; // cached, used at runtime to find world-space points that correspond to each image pixel
    u32 _PADDING[1];
    
    v3 Pixel00; // cached, base world-space point to shoot ray towards
    float FocusDistance; // user
  };

#define TRACER_OBJ_INDEX_BIT_COUNT (27)
#define TRACER_OBJ_TYPE_BIT_COUNT (4)
  struct Object {
    float Transform[12]; // mat3x4, implicitly a mat4. row-major (!)
    u32 Type;  // Sphere, mesh, etc
    u32 Index; // MSB is not used, next 4 bits are type, remaining 27 are index into [type]buffer that contains data
    u32 MaterialIndex; // Index into material buffer
    u32 Padding[1]; // If Sphere, radius is stored in Transform
  };

  struct Ray {
    v3 Position;
    u32 Data; // bottom 5 bits represent depth, rest is material index
    v3 Direction; // not necessarily normalized
    float T;
    v3 Color;
  };

#define TRACER_RAY_CHUNK_SIZE_POW2 (5)
#define TRACER_RAY_CHUNK_SIZE (1 << TRACER_RAY_CHUNK_SIZE_POW2)
#define TRACER_MATERIAL_CHUNK_SIZE_POW2 (5)
#define TRACER_MATERIAL_CHUNK_SIZE (1 << TRACER_MATERIAL_CHUNK_SIZE_POW2)
#define TRACER_MATERIAL_INDEX_MASK (0x07ffffff)
  struct Hit_record_chunk_base {
    float PosX   [TRACER_RAY_CHUNK_SIZE];
    float PosY   [TRACER_RAY_CHUNK_SIZE];
    float PosZ   [TRACER_RAY_CHUNK_SIZE];
    //    float UVX    [TRACER_RAY_CHUNK_SIZE];
    //    float UVY    [TRACER_RAY_CHUNK_SIZE];
    float NormalX[TRACER_RAY_CHUNK_SIZE];
    float NormalY[TRACER_RAY_CHUNK_SIZE];
  };

  struct Hit_record_chunk {
    Hit_record_chunk_base Hit;
  };

  struct Ray_chunk {
    u32 Data    [TRACER_RAY_CHUNK_SIZE]; // 5 MSB is depth, rest is material index
    float DirX  [TRACER_RAY_CHUNK_SIZE];
    float DirY  [TRACER_RAY_CHUNK_SIZE];
    float DirZ  [TRACER_RAY_CHUNK_SIZE];
    //    float T     [TRACER_RAY_CHUNK_SIZE];

#if TRACER_RAY_HIT_UNIFIED
    Hit_record_chunk_base Hit;
#endif

    float PosX  [TRACER_RAY_CHUNK_SIZE];
    float PosY  [TRACER_RAY_CHUNK_SIZE];
    float PosZ  [TRACER_RAY_CHUNK_SIZE];

#if TRACER_USE_F16_COLOR
    f16 ColorX[TRACER_RAY_CHUNK_SIZE];
    f16 ColorY[TRACER_RAY_CHUNK_SIZE];
    f16 ColorZ[TRACER_RAY_CHUNK_SIZE];
#else
    float ColorX[TRACER_RAY_CHUNK_SIZE];
    float ColorY[TRACER_RAY_CHUNK_SIZE];
    float ColorZ[TRACER_RAY_CHUNK_SIZE];    
#endif
  };

  struct Sphere {
    v3 Position;
    float Radius;
    v4 Color;
  };

  // master material struct
#define TRACER_MATERIAL_FLAG_METALLIC (1)
#define TRACER_MATERIAL_FLAG_EMISSIVE (2)
  struct Material {
    v3 Color;
    float Roughness;
    float RefractionIndex;
    u32 Flags;
    float _PADDING0[2];
  };

  struct Material_chunk {
    float ColorX         [TRACER_MATERIAL_CHUNK_SIZE];
    float ColorY         [TRACER_MATERIAL_CHUNK_SIZE];
    float ColorZ         [TRACER_MATERIAL_CHUNK_SIZE];
    float Roughness      [TRACER_MATERIAL_CHUNK_SIZE];
    float RefractionIndex[TRACER_MATERIAL_CHUNK_SIZE];
    u32 Flags            [TRACER_MATERIAL_CHUNK_SIZE];
  };

  /*  struct Mesh {
    // (todo) implement

      bvh4 GPUBVH;
      uvec4 DataChannels;

  };*/

  struct Potential_hit_record {
    //    x: u32 ObjectIndex;
    //    y: u32 RayIndex;
    //    z: u32 MaterialIndex;
    //    w: padding
    uv4 Data;
  };

  struct Hit_record {
    v3 HitPosition;
    u32 Data; // Material index, 0xffffffff means invalid hit record
    //    v2 UV;
    v2 Normal;
  };

  // https://github.com/jbikker/tinybvh/blob/main/tiny_bvh_base.h
  struct Bvh2 {
    // Alternative 64-byte BVH node layout, which specifies the bounds of
    // the children rather than the node itself. This layout is used by
    // Aila and Laine in their seminal GPU ray tracing paper.
    v4 lmin;// .w is left;
    v4 lmax;// .w is right;
    v4 rmin;// .w is triCount;
    v4 rmax;// .w is firstTri; // total: 64 bytes
  };

  // program signals (abbrev. PS)
#define TRACER_PS_NO_ACCUM_BIT (2)
  
  struct Ray_intersect_pcd {
    u64 CameraPointer;
    u32 Hash;
    float Hyst;
    uv2 Dimensions;
    u32 ImageStorageHandle;
    u32 Signals;
    u64 SphereBufferPointer;
    u32 SphereCount;
    u32 _Padding2[1];
  };

#define TRACER_RT_WORKGROUP_SIZE (64)
#define TRACER_RT_COARSE_FACTOR (2)
#define TRACER_RS_WORKGROUP_SIZE (128)
#define TRACER_RS_COARSE_FACTOR (1)
#define RT_PCD_RAY_BUF (0)
#define RT_PCD_TLAS_BUF (1)
#define RT_PCD_OBJ_BUF (2)
#define RT_PCD_HIT_BUF (3)
#define RT_PCD_CNT_BUF (4)
  struct Rt_pcd {
    uv4 Hash;
    
    u64 CameraPointer;
    u32 Signals; // unused
    u32 RayIdxOffset;
    
    uv2 Dimensions;
    u32 RaysPerPixelPow2;
    u32 RayCount; // Total ray count
    
    u64 Buffers[5];
    u64 _PADDING1[1];
    
    u32 Depth; // If MSB is asserted, populate ray buffers
    float PinholeRadius;
    u32 _PADDING2[2];
  };

  // shading
  struct Rs_pcd {
    uv4 Hash;
    
    uv2 Dimensions;
    u32 ImageStorageHandle; // unused
    u32 RayIdxOffset;

    float Hyst; // unused
    u32 Depth;
    u32 RayCount;
    u32 RaysPerPixelPow2;

    u64 HitBuffer;
    u64 MaterialBuffer;
    u64 RayBuffer;
    // include even if unused 
    u64 GlobalCountBuffer;

    v4 SkyColor;
  };

  struct Sw_pcd {
    uv2 Dimensions;
    u32 ImageStorageHandle;
    u32 TonemapImageStorageHandle;
    
    float AccumCount;
    u32 RaysPerPixelPow2;
    u64 RayBuffer;

    u32 RayIdxOffset;
    u32 RayCount;
    u32 _PADDING[2];
  };

#undef f16
  
#if APP_CPP_CODE
}
#else
#undef u32
#undef u64
#undef uv2
#undef uv3
#undef uv4
#undef v2
#undef v3
#undef v4
#endif
