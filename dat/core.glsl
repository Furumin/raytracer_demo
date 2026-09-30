#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_vote : require

#define PI 3.1415926535
#define FLOAT_INF (1.0 / 0.0)

#define GLOBAL_DESCRIPTORS \
  layout (set=0, binding=0) uniform texture2D _textures[];	\
  layout (set=0, binding=0) uniform utexture2D _uTextures[];	\
  layout (set=0, binding=0) uniform texture2DArray _textureArrays[];	\
  layout (set=0, binding=0) uniform textureCube _textureCubes[];		\
  layout (set=1, binding=0, rgba32f) uniform image2D _images_rgba32f[]; \
  layout (set=1, binding=0, rgba32ui) uniform uimage2D _images_rgba32u[]; \
  layout (set=1, binding=0, rgba8) uniform image2D _images_rgba8[]; \
  layout (set=1, binding=0, r16f) uniform image2D _images_r16f[]; \
  layout (set=1, binding=0, r32f) uniform image2D _images_r32f[]; \
  layout (set=1, binding=0, rg16f) uniform image2D _images_rg16f[]; \
  layout (set=1, binding=0, r32ui) uniform uimage2D _images_r32ui[]; \
  layout (set=1, binding=0, r11f_g11f_b10f) uniform image2D _images_b10gr11f[]; \
  layout (set=2, binding=0) uniform sampler _samplers[]; \
  layout (set=3, binding=0) uniform sampler2DArrayShadow _samplerShadowArrays[]; \
  layout (set=3, binding=0) uniform samplerCubeShadow _samplerShadowCubes[];

GLOBAL_DESCRIPTORS

uint hash1(uint x) {
  x = ((x >> 16) ^ x) * 0x45d9f3bu;
  x = ((x >> 16) ^ x) * 0x45d9f3bu;
  x = (x >> 16) ^ x;
  return x;
}

// returns random float [-1, 1]
float randomFloatNorm(uint seed) {
  return float((hash1(seed) / 4294967295.0)) * 2.0 - 1.0;
}

// returns random vec3 INSIDE unit sphere
vec3 randomUnitVec3(uint seed) {
  return vec3(randomFloatNorm(seed), randomFloatNorm(hash1(seed+1)), randomFloatNorm(hash1(seed+2)));
}

vec3 randomHemisphereVec3(uint seed, vec3 normal) {
  vec3 u = randomUnitVec3(seed);
  if (dot(u, normal) < 0.0) return -u;
  return u;
}

void barycentric(vec3 p, vec3 a, vec3 b, vec3 c, out float u, out float v, out float w) {
  vec3 v0 = b - a, v1 = c - a, v2 = p - a;
  float d00 = dot(v0, v0);
  float d01 = dot(v0, v1);
  float d11 = dot(v1, v1);
  float d20 = dot(v2, v0);
  float d21 = dot(v2, v1);
  float denom = (d00 * d11 - d01 * d01);
  v = (d11 * d20 - d01 * d21) / denom;
  w = (d00 * d21 - d01 * d20) / denom;
  u = 1.0f - v - w;
}


/*
  P: Line position
  dir: Direction of line
  Q: Point to test against line
  [out] l: Point on line closest to Q
 */
float sqDistPointLine(vec3 P, vec3 dir, vec3 Q, out vec3 l) {
  vec3 projQ = (1.0 / dot(dir, dir)) * dot(Q - P, dir) * dir;
  l = projQ;
  vec3 diff = projQ - Q;
  return dot(diff, diff);
}
