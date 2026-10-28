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
#define INVPI (1.0 / PI)
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

float sinc(float x) {
  x = abs(x); // sinc is even, so this is OK
  return x < 1e-6 ? 1.0f : sin(x) / x;
}


float mitchell1d(float x, float B, float C) {
  x = abs(2*x);
  if (x >= 2.0) return 0.0;
  if (x > 1) {
    return ((-B - 6*C) * x*x*x + (6*B + 30*C) * x*x +
	    (-12*B - 48*C) * x + (8*B + 24*C)) * (1.f/6.f);
  } else {
    return ((12 - 9*B - 6*C) * x*x*x + 
	    (-18 + 12*B + 6*C) * x*x +
	    (6 - 2*B)) * (1.f/6.f);
  }
}

float mitchell2d(vec2 p, float invRad, float b, float c) {
  return mitchell1d(p.x * invRad, b, c) * mitchell1d(p.y * invRad, b, c);
}

uint hash1(uint x) {
  x = ((x >> 16) ^ x) * 0x45d9f3bu;
  x = ((x >> 16) ^ x) * 0x45d9f3bu;
  x = (x >> 16) ^ x;
  return x;
}

float signNotZero(in float f) {
  return f >= 0.0 ? 1.0 : -1.0;
}

vec2 signNotZero(in vec2 v) {
  return vec2(signNotZero(v.x), signNotZero(v.y));
}

// x and y are signed normalized floats 
vec3 octadecode(vec2 pos) {
  vec3 v = vec3(pos, 0.0);
  v.z = 1.0f - abs(v.x) - abs(v.y);
  float t = max(-v.z, 0.0);
  v.x += (v.x >= 0.0f ? -t : t);
  v.y += (v.y >= 0.0f ? -t : t);
  return (v);
}

/* Assumes v is a unit vector. Returns a vec2 on the unit square [-1, 1]^2 */
vec2 octaencode(in vec3 v) {
    float l1norm = abs(v.x) + abs(v.y) + abs(v.z);
    vec2 result = v.xy * (1.0 / l1norm);
    if (v.z < 0.0) {
        result = (1.0 - abs(result.yx)) * signNotZero(result.xy);
    }
    return result;
}

// returns random float [-1, 1]
float randomFloatNorm(uint seed) {
    uint x = hash1(seed);
    return float(x >> 8) * (1.0 / 16777216.0) * 2.0 - 1.0;
}
/*float randomFloatNorm(uint seed) {
  return float((float(hash1(seed)) / 4294967295.0)) * 2.0 - 1.0;
  } */

// returns random vec3 INSIDE unit sphere
vec3 randomUnitVec3(uint seed) {
  return normalize(vec3(randomFloatNorm(seed), randomFloatNorm(hash1(seed+1)), randomFloatNorm(hash1(seed + 3))));
}

vec3 randomHemisphereVec3(uint seed, vec3 normal) {
  vec3 u = randomUnitVec3(seed);
  if (dot(u, normal) < 0.0) return -u;
  return u;
}

vec3 randomUnitVec3(uvec3 seeds) {
  float th = randomFloatNorm(seeds[0]) * PI + PI;
  float z = randomFloatNorm(seeds[1]);
  float sq = sqrt(1 - z * z);
  return vec3(sq * cos(th), sq * sin(th), z);
}

vec3 randomHemisphereVec3(uvec3 seeds, vec3 normal) {
  vec3 u = randomUnitVec3(seeds);
  if (dot(u, normal) < 0.0) return -u;
  return u;
}

vec3 randomCosDir(uvec2 seeds, vec3 normal) {
  float a = randomFloatNorm(seeds[0]);
  float b = sqrt(max(-fma(a, a, -1), 0.0));
  float phi = PI * (randomFloatNorm(seeds[1]) + 1.0);
  vec3 v;
  v.x = fma(b, cos(phi), normal.x); 
  v.y = fma(b, sin(phi), normal.y); 
  v.z = normal.z + a;
  return normalize(v);
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

// take a unit vector v and rotate it. the rotation goes from +Z to normal
vec3 rotFromZPos(vec3 normal, vec3 v) {
  float d = dot(normal, vec3(0,0,1));
  if (abs(abs(d) - 1.0) < 0.000001) return vec3(v.x, v.y * sign(d), v.z * sign(d));
  vec3 h = normalize(normal + vec3(0,0,1));
  vec4 q = vec4(-h.y, h.x, 0, h.z);

  float mod = 2.0f * dot(q.xy, v.xy);
  float mod2 = fma(h.z, h.z, -dot(q.xy, q.xy)); 
  vec3 v2 = (q.xyz * mod + v * mod2);
  return (v2 + (cross(q.xyz, v) * 2.0 * h.z));
}

vec2 randomDisk(uvec2 seeds) {
  // randomFloatNorm is in [-1, 1], change to [0, 1]
  vec2 u = vec2(randomFloatNorm(seeds[0]), randomFloatNorm(seeds[1])) * 0.5 + 0.5;
  float a = 2 * u[0] - 1;
  float b = 2 * u[1] - 1;
  if (a == 0.0 && b == 0.0)
    return vec2(0.0);  
  float r, phi;
  if (abs(a) > abs(b)) {
    r = a;
    phi = (PI * 0.25) * (b / a);
  } else {
    r = b;
    phi = (PI * 0.5) - (PI * 0.25) * (a / b);
  }
  return r * vec2(cos(phi), sin(phi));
}

vec3 randomConeDir(uvec2 seeds, vec3 normal, float cosThMax) {
  // get random
  vec2 u = vec2(randomFloatNorm(seeds[0]), randomFloatNorm(seeds[1])) * 0.5 + 0.5;
  float cosTh = (1 - u[0]) + u[0] * cosThMax;
  float sinTh = sqrt(abs(1.0 - cosTh * cosTh));
  float ph = u[1] * 2 * PI;
  vec3 n = vec3(cos(ph) * sinTh, sin(ph) * sinTh, cosTh);
  return rotFromZPos(normal, n);
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

float degToRad(float deg) {
  return PI * (deg * (1.0 / 180.0));
}
