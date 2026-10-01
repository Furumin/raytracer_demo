#define TRACER_SAMPLES_PER_PIXEL (1)

#if APP_CPP_CODE
#pragma once
namespace Tracer {
#else
#define u32 uint
#define u64 uvec2 // don't assume 64-bit support in shaders
#define uv2 uvec2
#define uv3 uvec3
#define uv4 uvec4
#define v2 vec2
#define v3 vec3
#define v4 vec4
  
#endif

  struct Camera {
    v3 Position; // user
    float AspectRatio; // cached, used at runtime
    
    v3 Forward; // user
    u32 PixelWidth; // user
    
    v3 Up; // user
    u32 PixelHeight; // user
    
    v3 DeltaU; // cached, used at runtime to find world-space points that correspond to each image pixel
    float VFov; // user
    
    v3 DeltaV; // cached, used at runtime to find world-space points that correspond to each image pixel
    float _Padding;
    
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
    u32 Data; // bottom 5 bits represent depth, rest is an atomic counter
    v3 Direction; // not necessarily normalized
    float T;
    v4 Color;
  };

#define TRACER_RAY_CHUNK_SIZE_POW2 (5)
#define TRACER_RAY_CHUNK_SIZE (1 << TRACER_RAY_CHUNK_SIZE_POW2)
#define TRACER_MATERIAL_CHUNK_SIZE_POW2 (5)
#define TRACER_MATERIAL_CHUNK_SIZE (1 << TRACER_MATERIAL_CHUNK_SIZE_POW2)
  struct Ray_chunk {
    float PosX[TRACER_RAY_CHUNK_SIZE];
    float PosY[TRACER_RAY_CHUNK_SIZE];
    float PosZ[TRACER_RAY_CHUNK_SIZE];
    u32 Data[TRACER_RAY_CHUNK_SIZE];
    float DirX[TRACER_RAY_CHUNK_SIZE];
    float DirY[TRACER_RAY_CHUNK_SIZE];
    float DirZ[TRACER_RAY_CHUNK_SIZE];
    float T[TRACER_RAY_CHUNK_SIZE];
    float ColorX[TRACER_RAY_CHUNK_SIZE];
    float ColorY[TRACER_RAY_CHUNK_SIZE];
    float ColorZ[TRACER_RAY_CHUNK_SIZE];
  };

  struct Sphere {
    v3 Position;
    float Radius;
    v4 Color;

    // TODO: change to
    /*
      float Radius;
     */
  };

  // master material struct
  struct Material {
    v3 Color;
    float Roughness;
    float RefractionIndex;
    float _PADDING0[3];
  };

  struct Material_chunk {
    float ColorX[TRACER_MATERIAL_CHUNK_SIZE];
    float ColorY[TRACER_MATERIAL_CHUNK_SIZE];
    float ColorZ[TRACER_MATERIAL_CHUNK_SIZE];
    float Roughness[TRACER_MATERIAL_CHUNK_SIZE];
    float RefractionIndex[TRACER_MATERIAL_CHUNK_SIZE];
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
    v2 UV;
    v2 Normal;
  };

  struct Hit_record_chunk {
    float PosX[TRACER_RAY_CHUNK_SIZE];
    float PosY[TRACER_RAY_CHUNK_SIZE];
    float PosZ[TRACER_RAY_CHUNK_SIZE];
    u32 Data[TRACER_RAY_CHUNK_SIZE];
    float UVX[TRACER_RAY_CHUNK_SIZE];
    float UVY[TRACER_RAY_CHUNK_SIZE];
    float NormalX[TRACER_RAY_CHUNK_SIZE];
    float NormalY[TRACER_RAY_CHUNK_SIZE];
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

  // Push constants
#define RAY_INTERSECT_PCD_NO_HYST_SIGNAL (2)
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

#define TRACER_RT_WORKGROUP_SIZE (256)
#define RT_PCD_RAY_BUF (0)
#define RT_PCD_TLAS_BUF (1)
#define RT_PCD_OBJ_BUF (2)
#define RT_PCD_HIT_BUF (3)
  struct Rt_pcd {
    u64 CameraPointer;
    u32 Hash;
    u32 Signals;

    uv2 Dimensions;
    u32 RaysPerPixelPow2;
    u32 RayCount; // Total ray count
    
    u64 Buffers[4];
    
    u32 Depth; // If MSB is asserted, populate ray buffers
    u32 _PADDING0[3];
  };

  // shading
  struct Rs_pcd {
    uv2 Dimensions;
    u32 Hash;
    u32 ImageStorageHandle;

    float Hyst;
    u32 Depth;
    u32 RayCount;
    u32 RaysPerPixelPow2;

    u64 HitBuffer;
    u64 MaterialBuffer;
    u64 RayBuffer;
    u64 _PADDING0[1];
  };

#define TRACER_SW_WORKGROUP_SIZE (256)
  struct Sw_pcd {
    uv2 Dimensions;
    u32 ImageStorageHandle;
    float Hyst;

    u32 _PADDING0[1];
    u32 RaysPerPixelPow2;
    u64 RayBuffer;
  };
  
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
