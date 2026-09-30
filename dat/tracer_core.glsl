#include "../src/tracer_interop.h"
layout(std430, buffer_reference, buffer_reference_align = 16) readonly buffer CameraBuffer {
  Camera Data;
};
layout(std430, buffer_reference, buffer_reference_align = 16) buffer MaterialBuffer {
  Material Data[];
};
layout(std430, buffer_reference, buffer_reference_align = 16) buffer RayBuffer {
  Ray Data[];
};
layout(std430, buffer_reference, buffer_reference_align = 16) buffer HitBuffer {
  Hit_record Data[];
};
layout(std430, buffer_reference, buffer_reference_align = 16) readonly buffer BvhBuffer {
  Bvh2 Data[];
};
layout(std430, buffer_reference, buffer_reference_align = 16) readonly buffer ObjectBuffer {
  Object Data[];
};
vec3 getRayMax(in Ray r) {
  return r.Position + r.Direction * r.T;
}

vec3 getRayMin(in Ray r) {
  return r.Position;
}

bool testRaySphere(in Ray r, in Sphere s) {
  vec3 l;
  float sqd = sqDistPointLine(r.Position, r.Direction, s.Position, l);
  vec3 rmx = getRayMax(r);
  vec3 rmn = getRayMin(r);
  float t = length(l - r.Position) / length(rmx - rmn);
  return sqd <= s.Radius * s.Radius && t >= 0.0 && t <= r.T;
}

Ray transformRay(in mat4 m, in Ray r) {
  Ray tr = r;
  tr.Position = (m * vec4(tr.Position, 1.0)).xyz;
  tr.Direction = (m * vec4(tr.Direction, 0.0)).xyz;
  return tr;
}

vec3 rayDirFromPixelPos(in Camera c, in vec2 pixelPos) {
  vec3 basePos = c.Pixel00;
  return basePos + (c.DeltaU * float(pixelPos.x) + c.DeltaV * float(pixelPos.y));
}

// tiny_bvh compatibility
// 6 bytes
// https://github.com/jbikker/tinybvh/blob/main/tiny_bvh_base.h
struct Bvh_aabb8 {
  uint16_t Data[3];
};
// 64 bytes
struct Bvh4 {
  vec3 AabbMin; uint c0; // 16
  vec3 AabbMax; uint c1; // 16
  Bvh_aabb8 c0b, c1b; uint c2; // 16
  Bvh_aabb8 c2b, c3b; uint c3; // 16

  // childInfo, 32bit:
  // msb:        0=interior, 1=leaf
  // leaf:       16 bits: relative start of triangle data, 15 bits: triangle count.
  // interior:   31 bits: child node address, in float4s from BVH data start.
  // Triangle data: directly follows nodes with leaves. Per tri:
  // - bvhvec4 vert0, vert1, vert2
  // - uint vert0.w stores original triangle index.
};

layout (std430, buffer_reference, buffer_reference_align = 16) readonly buffer Bvh4Buffer {
  vec4 RawData[];
};

ivec2 imagePixelPosFromRayIdx(in uint rayIdx, in uvec2 imageDim, in uint raysPerPixelPow2) {
  uint pixelIndex = rayIdx >> raysPerPixelPow2; // same as division by raysPerPixel
  return ivec2(pixelIndex % imageDim.x, pixelIndex / imageDim.x);
}
