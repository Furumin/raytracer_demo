#pragma once
#if _MSC_VER
#include <cmath>
#endif

#define PI32 3.1415926535
#define FLOAT_EPS 0.000001f

// builtin math
namespace bm {
  inline float sinf(const float rad) {
#if __GNUC__
    return __builtin_sin(rad);
#elif _MSC_VER
    return sinf(rad);
#endif
  }
  inline float cosf(const float rad) {
#if __GNUC__
    return __builtin_cos(rad);
#elif _MSC_VER
    return cosf(rad);
#endif
  }
  inline float tanf(const float rad) {
#if __GNUC__
    return __builtin_tan(rad);
#elif _MSC_VER
    return tanf(rad);
#endif
  }
  inline float sqrtf(const float val) {
#if __GNUC__
    return __builtin_sqrt(val);
#elif _MSC_VER
    return sqrtf(rad);
#endif
  }
}

struct v2 {
  union {
    struct {
      float x, y;
    };
    float v[2];
  };
};
struct v3 {
    union {
    struct {
      float x, y, z;
    };
    float v[3];
  };
};
inline v3
operator*=(v3 &l, const float r) {
  l.x *= r;
  l.y *= r;
  l.z *= r;
  return l;
}
inline v3
operator*(v3 l, const float r) {
  l *= r;
  return l;
}
inline v3
operator/=(v3 &l, const float r) {
  l.x /= r;
  l.y /= r;
  l.z /= r;
  return l;
}
inline v3
operator/(v3 l, const float r) {
  l /= r;
  return l;
}
inline v3
operator+=(v3 &l, v3 r) {
  l.x += r.x;
  l.y += r.y;
  l.z += r.z;
  return l;
}
inline v3
operator+(v3 l, v3 r) {
  l += r;
  return l;
}
inline v3
operator-=(v3 &l, v3 r) {
  l.x -= r.x;
  l.y -= r.y;
  l.z -= r.z;
  return l;
}
inline v3
operator-(v3 l, v3 r) {
  l -= r;
  return l;
}
inline v3
cross(const v3 u, const v3 v) {
  return {u.v[1] * v.v[2] - u.v[2] * v.v[1],
	  u.v[2] * v.v[0] - u.v[0] * v.v[2],
	  u.v[0] * v.v[1] - u.v[1] * v.v[0]};
}
inline float
dot(const v3 a, const v3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline v3
norm(const v3 v) {
  float len = bm::sqrtf(dot(v, v));
  if (len < 0.000001f) return {};
  float invl = 1.0 / len;
  return v * invl;
}
struct v4 {
    union {
    struct {
      float x, y, z, w;
    };
    float v[4];
  };
};

struct iv2 {
  union {
    struct {
      i32 x, y;
    };
    i32 v[2];
  };
};
inline iv2
operator-=(iv2 &v1, iv2 v2) {
  v1.x -= v2.x;
  v1.y -= v2.y;
  return v1;
}
inline iv2
operator-(iv2 v1, iv2 v2) {
  v1 -= v2;
  return v1;
}
struct iv3 {
  union {
    struct {
      i32 x, y, z;
    };
    i32 v[3];
  };
};
struct iv4 {
  union {
    struct {
      i32 x, y, z, w;
    };
    i32 v[4];
  };
};

struct uv2 {
  union {
    struct {
      u32 x, y;
    };
    u32 v[2];
  };
};
struct uv3 {
  union {
    struct {
      u32 x, y, z;
    };
    u32 v[3];
  };
};
struct uv4 {
  union {
    struct {
      u32 x, y, z, w;
    };
    u32 v[4];
  };
};

inline u32
hash1(u32 x) {
  x = ((x >> 16) ^ x) * 0x45d9f3bu;
  x = ((x >> 16) ^ x) * 0x45d9f3bu;
  x = (x >> 16) ^ x;
  return x;
}

float
RadFromDeg(const float deg) {
  const float invHalf = 1.0f / 180.0f;
  return PI32 * (deg * invHalf);
}

float
DegFromRad(const float rad) {
  const float invPI = 1.0f / PI32;
  return 180.0f * (rad * invPI);
}

struct quat {
  union {
    struct {
      float x, y, z, w;
    };
    float v[4];
  };
};

inline quat
operator*=(quat &q, const float f) {
  q.x *= f;
  q.y *= f;
  q.z *= f;
  q.w *= f;
  return q;
}

inline quat
operator*(const quat &q, const float f) {
  quat v = q;
  v *= f;
  return v;
}

inline quat
operator*=(quat &q1, const quat q2) {
  quat q = {.v = {
      q2.x*q1.w + q2.w*q1.x + q2.y*q1.z - q2.z*q1.y ,
      q2.y*q1.w + q2.w*q1.y + q2.z*q1.x - q2.x*q1.z,
      q2.z*q1.w + q2.w*q1.z + q2.x*q1.y - q2.y*q1.x,
      q2.w*q1.w - q2.x*q1.x - q2.y*q1.y - q2.z*q1.z
    }
  };
  q1 = q;
  return q;
}

inline quat
operator*(const quat q1, const quat q2) {
  quat q = q1;
  q *= q2;
  return q;
}

// inverse of normalized quat
inline quat
nquatinv(const quat q)
{
  return {.v = {
      -q.x,
      -q.y,
      -q.z,
      q.w
    }};
}

inline quat
quatangleaxis(const float angle, const v3 axis) {
  v3 n = norm(axis);
  float s = bm::sinf(angle * 0.5f);
  quat q;
  q.x = n.x * s;
  q.y = n.y * s;
  q.z = n.z * s;
  q.w = bm::cosf(angle * 0.5f);
  return q;
}

inline v3
quatrotv3(const quat q, const v3 v) {
  v3 q_vec = {.v = {q.x, q.y, q.z}};
  float q_s = q.w;
  float mod = 2.0f * dot(q_vec, v);
  float mod2 = (q_s * q_s - dot(q_vec, q_vec));
  float mod3 = 2.0f * q_s;
  v3 v1 = (q_vec * mod);
  v3 v2 = (v1 + (v * mod2));
  v3 v3 = (v2 + (cross(q_vec, v) * mod3));
  return v3;
}
