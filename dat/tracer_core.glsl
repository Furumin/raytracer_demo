#include "../src/tracer_interop.h"
layout(std430, buffer_reference, buffer_reference_align = 16) readonly buffer CameraBuffer {
  Camera Data;
};
layout(std430, buffer_reference, buffer_reference_align = 16) buffer MaterialBuffer {
  Material Data[];
};
layout(std430, buffer_reference, buffer_reference_align = 16) buffer MaterialChunkBuffer {
  Material_chunk Data[];
};
layout(std430, buffer_reference, buffer_reference_align = 16) buffer RayBuffer {
  Ray Data[];
};
layout(std430, buffer_reference, buffer_reference_align = 16) buffer RayChunkBuffer {
  Ray_chunk Data[];
};
#if TRACER_RAY_HIT_UNIFIED
#define HitChunkBuffer RayChunkBuffer
#else
layout(std430, buffer_reference, buffer_reference_align = 16) buffer HitChunkBuffer {
  Hit_record_chunk Data[];
};
#endif
#if TRACER_PERSISTENT_THREADS
layout(std430, buffer_reference, buffer_reference_align = 16) coherent buffer GlobalCountBuffer {
  uint Count;
};
#endif
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

void getRayChunkIdx(in uint rayIdx, out uint chunkIdx, out uint idxInChunk) {
  chunkIdx = rayIdx >> TRACER_RAY_CHUNK_SIZE_POW2; // division
  idxInChunk = rayIdx & (TRACER_RAY_CHUNK_SIZE - 1); // modulo
}

void getMaterialChunkIdx(in uint rayIdx, out uint chunkIdx, out uint idxInChunk) {
  chunkIdx = rayIdx >> TRACER_MATERIAL_CHUNK_SIZE_POW2; // division
  idxInChunk = rayIdx & (TRACER_MATERIAL_CHUNK_SIZE - 1); // modulo
}

#if TRACER_USE_F16_COLOR
#define _STORE_COLOR(a) float16_t(a)
#define _LOAD_COLOR(a) float(a)
#else
#define _STORE_COLOR(a) (a)
#define _LOAD_COLOR(a) (a)
#endif

#define _RAY_BUF_ACCESS(prop) buf.Data[chunkIdx]. prop [idxInChunk]

Ray loadRayByIdx(in RayChunkBuffer buf, in uint rayIdx, bool loadColor, bool loadPos) {
  uint chunkIdx, idxInChunk;
  getRayChunkIdx(rayIdx, chunkIdx, idxInChunk);
  Ray r;
  if (loadPos) {
    r.Position.x = _RAY_BUF_ACCESS(PosX);
    r.Position.y = _RAY_BUF_ACCESS(PosY);
    r.Position.z = _RAY_BUF_ACCESS(PosZ);
  }
  r.Data = _RAY_BUF_ACCESS(Data);
  r.Direction.x = _RAY_BUF_ACCESS(DirX);
  r.Direction.y = _RAY_BUF_ACCESS(DirY);
  r.Direction.z = _RAY_BUF_ACCESS(DirZ);

  if (loadColor) {
    r.Color.x = _LOAD_COLOR(_RAY_BUF_ACCESS(ColorX));
    r.Color.y = _LOAD_COLOR(_RAY_BUF_ACCESS(ColorY));
    r.Color.z = _LOAD_COLOR(_RAY_BUF_ACCESS(ColorZ));
  }
  
  return r;
}

vec3 loadRayClrByIdx(in RayChunkBuffer buf, in uint rayIdx) {
  uint chunkIdx, idxInChunk;
  getRayChunkIdx(rayIdx, chunkIdx, idxInChunk);
  vec3 v;
  v.x = _LOAD_COLOR(_RAY_BUF_ACCESS(ColorX));
  v.y = _LOAD_COLOR(_RAY_BUF_ACCESS(ColorY));
  v.z = _LOAD_COLOR(_RAY_BUF_ACCESS(ColorZ));
  return v;
}

void storeRayByIdx(in RayChunkBuffer buf, in uint rayIdx, in Ray r, bool storeColor, bool storePos) {
  uint chunkIdx, idxInChunk;
  getRayChunkIdx(rayIdx, chunkIdx, idxInChunk);
  if (storePos) {
    _RAY_BUF_ACCESS(PosX) = r.Position.x;
    _RAY_BUF_ACCESS(PosY) = r.Position.y;
    _RAY_BUF_ACCESS(PosZ) = r.Position.z;
  }
  _RAY_BUF_ACCESS(Data) = r.Data;

  _RAY_BUF_ACCESS(DirX) = r.Direction.x;
  _RAY_BUF_ACCESS(DirY) = r.Direction.y;
  _RAY_BUF_ACCESS(DirZ) = r.Direction.z;
  if (storeColor) {
    _RAY_BUF_ACCESS(ColorX) = _STORE_COLOR(r.Color.x);
    _RAY_BUF_ACCESS(ColorY) = _STORE_COLOR(r.Color.y);
    _RAY_BUF_ACCESS(ColorZ) = _STORE_COLOR(r.Color.z);
  }
}

Hit_record loadHitByIdx(in HitChunkBuffer buf, in uint rayIdx) {
  uint chunkIdx, idxInChunk;
  getRayChunkIdx(rayIdx, chunkIdx, idxInChunk);
  Hit_record r;
  r.HitPosition.x = _RAY_BUF_ACCESS(Hit.PosX);
  r.HitPosition.y = _RAY_BUF_ACCESS(Hit.PosY);
  r.HitPosition.z = _RAY_BUF_ACCESS(Hit.PosZ);

  //  r.UV.x = _RAY_BUF_ACCESS(Hit.UVX);
  //  r.UV.y = _RAY_BUF_ACCESS(Hit.UVY);

  r.Normal.x = _RAY_BUF_ACCESS(Hit.NormalX);
  r.Normal.y = _RAY_BUF_ACCESS(Hit.NormalY);

  return r;
}
void storeHitByIdx(in HitChunkBuffer buf, in uint rayIdx, in Hit_record r) {
  uint chunkIdx, idxInChunk;
  getRayChunkIdx(rayIdx, chunkIdx, idxInChunk);

  _RAY_BUF_ACCESS(Hit.PosX) = r.HitPosition.x;
  _RAY_BUF_ACCESS(Hit.PosY) = r.HitPosition.y;
  _RAY_BUF_ACCESS(Hit.PosZ) = r.HitPosition.z;

  //  _RAY_BUF_ACCESS(Hit.UVX) = r.UV.x;
  //  _RAY_BUF_ACCESS(Hit.UVY) = r.UV.y;

  _RAY_BUF_ACCESS(Hit.NormalX) = r.Normal.x;
  _RAY_BUF_ACCESS(Hit.NormalY) = r.Normal.y;
}

Material loadMaterialByIdx(in MaterialChunkBuffer buf, in uint matIdx) {
  uint chunkIdx, idxInChunk;
  getMaterialChunkIdx(matIdx, chunkIdx, idxInChunk);
  Material m;
  m.Color.x = _RAY_BUF_ACCESS(ColorX);
  m.Color.y = _RAY_BUF_ACCESS(ColorY);
  m.Color.z = _RAY_BUF_ACCESS(ColorZ);
  m.Roughness = _RAY_BUF_ACCESS(Roughness);
  m.RefractionIndex = _RAY_BUF_ACCESS(RefractionIndex);
  m.Flags = _RAY_BUF_ACCESS(Flags);
  return m;
}

bool sphereHit(in vec3 rayPos, in vec3 rayDir, in vec3 sPos, in float sRad, in float rayTMax, out float tMin) {
  vec3 oc = sPos - rayPos;
  float a = dot(rayDir, rayDir);
  float h = dot(rayDir, oc);
  float c = -fma(sRad, sRad, -dot(oc, oc)); //dot(oc, oc) - s.Radius * s.Radius; 
  float disc = fma(h, h, -a * c); //h * h - a * c;
  bool ht = disc >= 0;

  const float recipA = 1.0 / a;
  float sqf = sqrt(disc);
  float r = (h - sqf) * recipA;
  if (!(r >= 0.0f && r <= rayTMax)) {
    r = (h + sqf) * recipA;
    ht = ht && ((r >= 0.0f && r <= rayTMax));
  }

  // hit
  if (ht)
    tMin = r;
  return ht;
}

bool sphereHit(in Ray ray, in Sphere s, out float tMin) {
  vec3 oc = s.Position - ray.Position;
  float a = dot(ray.Direction, ray.Direction);
  float h = dot(ray.Direction, oc);
  float c = -fma(s.Radius, s.Radius, -dot(oc, oc)); //dot(oc, oc) - s.Radius * s.Radius; 
  float disc = fma(h, h, -a * c); //h * h - a * c;
  bool ht = disc >= 0;

  const float recipA = 1.0 / a;
  float sqf = sqrt(disc);
  float r = (h - sqf) * recipA;
  if (!(r >= 0.0f && r <= ray.T)) {
    r = (h + sqf) * recipA;
    ht = ht && ((r >= 0.0f && r <= ray.T));
  }

  // hit
  if (ht)
    tMin = r;
  return ht;
}

// with code from
// https://github.com/jbikker/tinybvh/blob/dev/kernels/traverse_bvh2.cl
bool hitClosest(BvhBuffer bvhBuffer, ObjectBuffer objBuffer, in Ray ray, out uint objId, out float tMin, out vec3 hitSphereCenter, out uint materialIndex) {
  const vec3 rD = 1.0 / ray.Direction;
  const vec3 rO = ray.Position * -rD;
  float hit = ray.T;
  uint nodeIdx = 0;
  uint stack[24];
  const uint stackBase = 0;
  uint stackPtr = stackBase;
  while (true) {
    if ((nodeIdx & 0x80000000) != 0) {
      // found hit! this BVH only contains spheres, so no need to check type
      uint thisObjId = nodeIdx & 0xffffff;
      Sphere s;
      s.Position = vec3(objBuffer.Data[thisObjId].Transform[3], objBuffer.Data[thisObjId].Transform[7], objBuffer.Data[thisObjId].Transform[11]);
      s.Radius = objBuffer.Data[thisObjId].Transform[0];
      float t;
      bool h = sphereHit(ray, s, t);
      if (h && t < hit) {
	hit = t;
	objId = thisObjId;
	hitSphereCenter = s.Position;
	materialIndex = objBuffer.Data[thisObjId].MaterialIndex;
	tMin = hit;
      }
      if (stackPtr == stackBase) break;
      nodeIdx = stack[--stackPtr];
      continue;
    } else {
      const vec4 lmin = bvhBuffer.Data[nodeIdx].lmin, lmax = bvhBuffer.Data[nodeIdx].lmax;
      const vec4 rmin = bvhBuffer.Data[nodeIdx].rmin, rmax = bvhBuffer.Data[nodeIdx].rmax;
      const vec3 t1a = fma( lmin.xyz, rD, rO ), t2a = fma( lmax.xyz, rD, rO );
      const vec3 t1b = fma( rmin.xyz, rD, rO ), t2b = fma( rmax.xyz, rD, rO );
      const vec3 minta = min( t1a, t2a ), maxta = max( t1a, t2a );
      const vec3 mintb = min( t1b, t2b ), maxtb = max( t1b, t2b );
      const float tmina = max( max( max( minta.x, minta.y ), minta.z ), 0 );
      const float tminb = max( max( max( mintb.x, mintb.y ), mintb.z ), 0 );
      const float tmaxa = min( min( min( maxta.x, maxta.y ), maxta.z ), hit );
      const float tmaxb = min( min( min( maxtb.x, maxtb.y ), maxtb.z ), hit );
      const bool hitA = tmina <= tmaxa, hitB = tminb <= tmaxb;
      uint left = floatBitsToUint( lmin.w ), right = floatBitsToUint( lmax.w );
      if (hitA && hitB) {
	// add furthest hit first, nearest hit at top of stack
	  uint near = left, far = right;
	  if (tminb < tmina) near = right, far = left;
	  stack[stackPtr++] = far, nodeIdx = near;
	}
      else if (hitA) nodeIdx = left;
      else if (hitB) nodeIdx = right;
      else { if (stackPtr == stackBase) break; nodeIdx = stack[--stackPtr]; }
    }
  }
  return hit < ray.T;
}

bool hitAny(BvhBuffer bvhBuffer, ObjectBuffer objBuffer, in Ray ray, bool disqualifyAlpha) {
  const vec3 rD = 1.0 / ray.Direction;
  const vec3 rO = ray.Position * -rD;
  float hit = ray.T;
  uint nodeIdx = 0;
  uint stack[24];
  const uint stackBase = 0;
  uint stackPtr = stackBase;
  while (true) {
    if ((nodeIdx & 0x80000000) != 0) {
      // found hit! this BVH only contains spheres, so no need to check type
      uint thisObjId = nodeIdx & 0xffffff;
      Sphere s;
      s.Position = vec3(objBuffer.Data[thisObjId].Transform[3], objBuffer.Data[thisObjId].Transform[7], objBuffer.Data[thisObjId].Transform[11]);
      s.Radius = objBuffer.Data[thisObjId].Transform[0];
      float t;
      bool h = sphereHit(ray, s, t);
      // (todo) use disqualifyAlpha!
      if (h && t < hit && (true)) {
	hit = t;
	return true;
      }
      if (stackPtr == stackBase) break;
      nodeIdx = stack[--stackPtr];
      continue;
    } else {
      const vec4 lmin = bvhBuffer.Data[nodeIdx].lmin, lmax = bvhBuffer.Data[nodeIdx].lmax;
      const vec4 rmin = bvhBuffer.Data[nodeIdx].rmin, rmax = bvhBuffer.Data[nodeIdx].rmax;
      const vec3 t1a = fma( lmin.xyz, rD, rO ), t2a = fma( lmax.xyz, rD, rO );
      const vec3 t1b = fma( rmin.xyz, rD, rO ), t2b = fma( rmax.xyz, rD, rO );
      const vec3 minta = min( t1a, t2a ), maxta = max( t1a, t2a );
      const vec3 mintb = min( t1b, t2b ), maxtb = max( t1b, t2b );
      const float tmina = max( max( max( minta.x, minta.y ), minta.z ), 0 );
      const float tminb = max( max( max( mintb.x, mintb.y ), mintb.z ), 0 );
      const float tmaxa = min( min( min( maxta.x, maxta.y ), maxta.z ), hit );
      const float tmaxb = min( min( min( maxtb.x, maxtb.y ), maxtb.z ), hit );
      const bool hitA = tmina <= tmaxa, hitB = tminb <= tmaxb;
      uint left = floatBitsToUint( lmin.w ), right = floatBitsToUint( lmax.w );
      if (hitA && hitB) {
	// add furthest hit first, nearest hit at top of stack
	  uint near = left, far = right;
	  if (tminb < tmina) near = right, far = left;
	  stack[stackPtr++] = far, nodeIdx = near;
	}
      else if (hitA) nodeIdx = left;
      else if (hitB) nodeIdx = right;
      else { if (stackPtr == stackBase) break; nodeIdx = stack[--stackPtr]; }
    }
  }
  return hit < ray.T;
}

bool hitAny(BvhBuffer bvhBuffer, ObjectBuffer objBuffer, in vec3 rayPos, in vec3 rayDir, in float rayTMax, bool disqualifyAlpha) {
  const vec3 rD = 1.0 / rayDir;
  const vec3 rO = rayPos * -rD;
  float hit = rayTMax;
  uint nodeIdx = 0;
  uint stack[24];
  const uint stackBase = 0;
  uint stackPtr = stackBase;
  while (true) {
    if ((nodeIdx & 0x80000000) != 0) {
      // found hit! this BVH only contains spheres, so no need to check type
      uint thisObjId = nodeIdx & 0xffffff;
      Sphere s;
      s.Position = vec3(objBuffer.Data[thisObjId].Transform[3], objBuffer.Data[thisObjId].Transform[7], objBuffer.Data[thisObjId].Transform[11]);
      s.Radius = objBuffer.Data[thisObjId].Transform[0];
      float t;
      bool h = sphereHit(rayPos, rayDir, s.Position, s.Radius, rayTMax, t);
      // (todo) use disqualifyAlpha!
      if (h && t < rayTMax && (true)) {
	return true;
      }
      if (stackPtr == stackBase) break;
      nodeIdx = stack[--stackPtr];
      continue;
    } else {
      const vec4 lmin = bvhBuffer.Data[nodeIdx].lmin, lmax = bvhBuffer.Data[nodeIdx].lmax;
      const vec4 rmin = bvhBuffer.Data[nodeIdx].rmin, rmax = bvhBuffer.Data[nodeIdx].rmax;
      const vec3 t1a = fma( lmin.xyz, rD, rO ), t2a = fma( lmax.xyz, rD, rO );
      const vec3 t1b = fma( rmin.xyz, rD, rO ), t2b = fma( rmax.xyz, rD, rO );
      const vec3 minta = min( t1a, t2a ), maxta = max( t1a, t2a );
      const vec3 mintb = min( t1b, t2b ), maxtb = max( t1b, t2b );
      const float tmina = max( max( max( minta.x, minta.y ), minta.z ), 0 );
      const float tminb = max( max( max( mintb.x, mintb.y ), mintb.z ), 0 );
      const float tmaxa = min( min( min( maxta.x, maxta.y ), maxta.z ), hit );
      const float tmaxb = min( min( min( maxtb.x, maxtb.y ), maxtb.z ), hit );
      const bool hitA = tmina <= tmaxa, hitB = tminb <= tmaxb;
      uint left = floatBitsToUint( lmin.w ), right = floatBitsToUint( lmax.w );
      if (hitA && hitB) {
	// add furthest hit first, nearest hit at top of stack
	  uint near = left, far = right;
	  if (tminb < tmina) near = right, far = left;
	  stack[stackPtr++] = far, nodeIdx = near;
	}
      else if (hitA) nodeIdx = left;
      else if (hitB) nodeIdx = right;
      else { if (stackPtr == stackBase) break; nodeIdx = stack[--stackPtr]; }
    }
  }
  return hit < rayTMax;
}
