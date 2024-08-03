#ifndef PACKER_GLSL
#define PACKER_GLSL

// taken from: Q2RTX utils.glsl

uint packNormal(vec3 normal) {
  // project the sphere onto the octahedron (|x|+|y|+|z| = 1) and then onto the xy-plane
  float invL1Norm = 1.0 / (abs(normal.x) + abs(normal.y) + abs(normal.z));
  vec2 p          = normal.xy * invL1Norm;

  // wrap the octahedral faces from the negative-Z space
  p = (normal.z < 0)
          ? (1.0 - abs(p.yx)) * mix(vec2(-1.0), vec2(1.0), greaterThanEqual(p.xy, vec2(0)))
          : p;

  // convert to [0..1]
  p = clamp(p.xy * 0.5 + 0.5, vec2(0), vec2(1));

  // encode as RG16_UNORM
  uvec2 u = uvec2(p * 0xffffu);
  return u.x | (u.y << 16);
}

vec3 unpackNormal(uint enc) {
  // decode RG16_UNORM
  uvec2 u = uvec2(enc & 0xffffu, enc >> 16);
  vec2 p  = vec2(u) / float(0xffff);

  // convert to [-1..1]
  p = p * 2.0 - 1.0;

  // decode the octahedron
  // https://twitter.com/Stubbesaurus/status/937994790553227264
  vec3 n  = vec3(p.x, p.y, 1.0 - abs(p.x) - abs(p.y));
  float t = max(0, -n.z);
  n.xy += mix(vec2(t), vec2(-t), greaterThanEqual(n.xy, vec2(0)));

  return normalize(n);
}

// if sampling is not needed, this is an alternative to R16G16B16A16_SFLOAT, it trades time for
// space
// it uses 9 bits for each color channel, compared to regular 8 bits (UNORM), this method also
// offers exponential encoding, which offers better accuracy, all of this comes at the cost of some
// packing and unpacking overhead
uint packRgbe(vec3 v) {
  vec3 va       = max(vec3(0), v);
  float max_abs = max(va.r, max(va.g, va.b));
  if (max_abs == 0) return 0;

  float exponent = floor(log2(max_abs));

  uint result;
  result = uint(clamp(exponent + 20, 0, 31)) << 27;

  float scale = pow(2, -exponent) * 256.0;
  uvec3 vu    = min(uvec3(511), uvec3(round(va * scale)));
  result |= vu.r;
  result |= vu.g << 9;
  result |= vu.b << 18;

  return result;
}

vec3 unpackRgbe(uint x) {
  int exponent = int(x >> 27) - 20;
  float scale  = pow(2, exponent) / 256.0;

  vec3 v;
  v.r = float(x & 0x1ff) * scale;
  v.g = float((x >> 9) & 0x1ff) * scale;
  v.b = float((x >> 18) & 0x1ff) * scale;

  return v;
}

// const float f = 0.2;

// uint packFloatToUint8(float val, float boundaryMin, float boundaryMax) {
//   boundaryMin = -f;
//   boundaryMax = f;
//   val = clamp(val, boundaryMin, boundaryMax);
//   return uint((val - boundaryMin) / (boundaryMax - boundaryMin) * 255.0);
// }

// float unpackUint8ToFloat(uint encoded, float boundaryMin, float boundaryMax) {
//   boundaryMin = -f;
//   boundaryMax = f;
//   return (float(encoded) / 255.0) * (boundaryMax - boundaryMin) + boundaryMin;
// }

#endif // PACKER_GLSL
