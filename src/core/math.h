#pragma once
#if _MSC_VER
#include <cmath>
#endif
#define PI32 3.1415926535

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
inline v3 operator*=(v3 &l, const float r) {
  l.x *= r;
  l.y *= r;
  l.z *= r;
  return l;
}
inline v3 operator*(v3 l, const float r) {
  l *= r;
  return l;
}
inline v3 operator/=(v3 &l, const float r) {
  l.x /= r;
  l.y /= r;
  l.z /= r;
  return l;
}
inline v3 operator/(v3 l, const float r) {
  l /= r;
  return l;
}
inline v3 operator+=(v3 &l, v3 r) {
  l.x += r.x;
  l.y += r.y;
  l.z += r.z;
  return l;
}
inline v3 operator+(v3 l, v3 r) {
  l += r;
  return l;
}
inline v3 operator-=(v3 &l, v3 r) {
  l.x -= r.x;
  l.y -= r.y;
  l.z -= r.z;
  return l;
}
inline v3 operator-(v3 l, v3 r) {
  l -= r;
  return l;
}
inline v3 cross(const v3 u, const v3 v) {
  return {u.v[1] * v.v[2] - u.v[2] * v.v[1],
	  u.v[2] * v.v[0] - u.v[0] * v.v[2],
	  u.v[0] * v.v[1] - u.v[1] * v.v[0]};
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

namespace tri {
  inline float sinf(const float rad) {
#if __GNUC__
    return __builtin_sin(rad);
#elif _MSC_VER
    return sinf(rad);
#endif
  }
  inline float tanf(const float rad) {
#if __GNUC__
    return __builtin_tan(rad);
#elif _MSC_VER
    return tanf(rad);
#endif
  }
}
