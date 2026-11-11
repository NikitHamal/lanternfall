#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
constexpr float PI = 3.14159265358979f, TAU = PI * 2;
inline float clampf(float x, float a, float b) { return x < a ? a : x > b ? b : x; }
inline float sat(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float smooth(float a, float b, float x) { x = sat((x - a) / (b - a)); return x * x * (3 - 2 * x); }
struct V3 { float x = 0, y = 0, z = 0; };
inline V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float len(V3 a) { return std::sqrt(dot(a, a)); }
inline V3 norm(V3 a) { float l = len(a); return l > 1e-9f ? a * (1 / l) : V3{0, 1, 0}; }
inline V3 mix3(V3 a, V3 b, float t) { return {lerpf(a.x, b.x, t), lerpf(a.y, b.y, t), lerpf(a.z, b.z, t)}; }
// column-major 4x4
struct M4 { float m[16]; };
inline M4 ident() { M4 r{}; r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1; return r; }
inline M4 mul(const M4& a, const M4& b) { M4 r; for (int c = 0; c < 4; c++) for (int rr = 0; rr < 4; rr++) { float s = 0; for (int k = 0; k < 4; k++) s += a.m[k * 4 + rr] * b.m[c * 4 + k]; r.m[c * 4 + rr] = s; } return r; }
inline M4 translate(float x, float y, float z) { M4 r = ident(); r.m[12] = x; r.m[13] = y; r.m[14] = z; return r; }
inline M4 scale4(float x, float y, float z) { M4 r = ident(); r.m[0] = x; r.m[5] = y; r.m[10] = z; return r; }
inline M4 rotX(float a) { M4 r = ident(); float c = std::cos(a), s = std::sin(a); r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c; return r; }
inline M4 rotY(float a) { M4 r = ident(); float c = std::cos(a), s = std::sin(a); r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c; return r; }
inline M4 rotZ(float a) { M4 r = ident(); float c = std::cos(a), s = std::sin(a); r.m[0] = c; r.m[1] = s; r.m[4] = -s; r.m[5] = c; return r; }
inline M4 perspective(float fovy, float asp, float n, float f) { M4 r{}; float t = 1 / std::tan(fovy / 2); r.m[0] = t / asp; r.m[5] = t; r.m[10] = (f + n) / (n - f); r.m[11] = -1; r.m[14] = 2 * f * n / (n - f); return r; }
inline M4 ortho(float l, float r_, float b, float t, float n, float f) { M4 r = ident(); r.m[0] = 2 / (r_ - l); r.m[5] = 2 / (t - b); r.m[10] = -2 / (f - n); r.m[12] = -(r_ + l) / (r_ - l); r.m[13] = -(t + b) / (t - b); r.m[14] = -(f + n) / (f - n); return r; }
// camera-to-world matrix looking from eye at target (right-handed, -Z forward)
inline M4 camWorld(V3 eye, V3 tgt, V3 up = {0, 1, 0}) { V3 z = norm(eye - tgt), x = norm(cross(up, z)), y = cross(z, x); M4 r = ident(); r.m[0] = x.x; r.m[1] = x.y; r.m[2] = x.z; r.m[4] = y.x; r.m[5] = y.y; r.m[6] = y.z; r.m[8] = z.x; r.m[9] = z.y; r.m[10] = z.z; r.m[12] = eye.x; r.m[13] = eye.y; r.m[14] = eye.z; return r; }
inline M4 rigidInverse(const M4& a) { M4 r = ident(); for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) r.m[j * 4 + i] = a.m[i * 4 + j]; for (int i = 0; i < 3; i++) r.m[12 + i] = -(r.m[i] * a.m[12] + r.m[4 + i] * a.m[13] + r.m[8 + i] * a.m[14]); return r; }
inline M4 inverse(const M4& a) {
  const float* m = a.m; float inv[16];
  inv[0] = m[5]*m[10]*m[15]-m[5]*m[11]*m[14]-m[9]*m[6]*m[15]+m[9]*m[7]*m[14]+m[13]*m[6]*m[11]-m[13]*m[7]*m[10];
  inv[4] = -m[4]*m[10]*m[15]+m[4]*m[11]*m[14]+m[8]*m[6]*m[15]-m[8]*m[7]*m[14]-m[12]*m[6]*m[11]+m[12]*m[7]*m[10];
  inv[8] = m[4]*m[9]*m[15]-m[4]*m[11]*m[13]-m[8]*m[5]*m[15]+m[8]*m[7]*m[13]+m[12]*m[5]*m[11]-m[12]*m[7]*m[9];
  inv[12] = -m[4]*m[9]*m[14]+m[4]*m[10]*m[13]+m[8]*m[5]*m[14]-m[8]*m[6]*m[13]-m[12]*m[5]*m[10]+m[12]*m[6]*m[9];
  inv[1] = -m[1]*m[10]*m[15]+m[1]*m[11]*m[14]+m[9]*m[2]*m[15]-m[9]*m[3]*m[14]-m[13]*m[2]*m[11]+m[13]*m[3]*m[10];
  inv[5] = m[0]*m[10]*m[15]-m[0]*m[11]*m[14]-m[8]*m[2]*m[15]+m[8]*m[3]*m[14]+m[12]*m[2]*m[11]-m[12]*m[3]*m[10];
  inv[9] = -m[0]*m[9]*m[15]+m[0]*m[11]*m[13]+m[8]*m[1]*m[15]-m[8]*m[3]*m[13]-m[12]*m[1]*m[11]+m[12]*m[3]*m[9];
  inv[13] = m[0]*m[9]*m[14]-m[0]*m[10]*m[13]-m[8]*m[1]*m[14]+m[8]*m[2]*m[13]+m[12]*m[1]*m[10]-m[12]*m[2]*m[9];
  inv[2] = m[1]*m[6]*m[15]-m[1]*m[7]*m[14]-m[5]*m[2]*m[15]+m[5]*m[3]*m[14]+m[13]*m[2]*m[7]-m[13]*m[3]*m[6];
  inv[6] = -m[0]*m[6]*m[15]+m[0]*m[7]*m[14]+m[4]*m[2]*m[15]-m[4]*m[3]*m[14]-m[12]*m[2]*m[7]+m[12]*m[3]*m[6];
  inv[10] = m[0]*m[5]*m[15]-m[0]*m[7]*m[13]-m[4]*m[1]*m[15]+m[4]*m[3]*m[13]+m[12]*m[1]*m[7]-m[12]*m[3]*m[5];
  inv[14] = -m[0]*m[5]*m[14]+m[0]*m[6]*m[13]+m[4]*m[1]*m[14]-m[4]*m[2]*m[13]-m[12]*m[1]*m[6]+m[12]*m[2]*m[5];
  inv[3] = -m[1]*m[6]*m[11]+m[1]*m[7]*m[10]+m[5]*m[2]*m[11]-m[5]*m[3]*m[10]-m[9]*m[2]*m[7]+m[9]*m[3]*m[6];
  inv[7] = m[0]*m[6]*m[11]-m[0]*m[7]*m[10]-m[4]*m[2]*m[11]+m[4]*m[3]*m[10]+m[8]*m[2]*m[7]-m[8]*m[3]*m[6];
  inv[11] = -m[0]*m[5]*m[11]+m[0]*m[7]*m[9]+m[4]*m[1]*m[11]-m[4]*m[3]*m[9]-m[8]*m[1]*m[7]+m[8]*m[3]*m[5];
  inv[15] = m[0]*m[5]*m[10]-m[0]*m[6]*m[9]-m[4]*m[1]*m[10]+m[4]*m[2]*m[9]+m[8]*m[1]*m[6]-m[8]*m[2]*m[5];
  float det = m[0]*inv[0]+m[1]*inv[4]+m[2]*inv[8]+m[3]*inv[12]; M4 r; float id = std::fabs(det) < 1e-20f ? 0 : 1 / det; for (int i = 0; i < 16; i++) r.m[i] = inv[i] * id; return r; }
inline V3 xformPoint(const M4& a, V3 p) { return {a.m[0]*p.x+a.m[4]*p.y+a.m[8]*p.z+a.m[12], a.m[1]*p.x+a.m[5]*p.y+a.m[9]*p.z+a.m[13], a.m[2]*p.x+a.m[6]*p.y+a.m[10]*p.z+a.m[14]}; }
// rng + noise (identical formulas to the original util)
struct Rng { uint32_t a; explicit Rng(uint32_t s = 1) : a(s) {}
  float operator()() { a += 0x6D2B79F5u; uint32_t t = a; t = (t ^ (t >> 15)) * (t | 1); t ^= t + (t ^ (t >> 7)) * (t | 61); return ((t ^ (t >> 14))) / 4294967296.0f; } };
inline float frac(float x) { return x - std::floor(x); }
inline float hash1(float n) { return frac(std::sin(n * 127.1f + 311.7f) * 43758.5453f); }
inline float hash2(float x, float y) { return frac(std::sin(x * 127.1f + y * 311.7f) * 43758.5453f); }
inline float hash3(float x, float y, float z) { return frac(std::sin(x * 127.1f + y * 311.7f + z * 74.7f) * 43758.5453f); }
inline float vnoise3(float x, float y, float z) {
  float ix = std::floor(x), iy = std::floor(y), iz = std::floor(z), fx = x - ix, fy = y - iy, fz = z - iz;
  fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy); fz = fz * fz * (3 - 2 * fz);
  auto L = [](float a, float b, float t) { return a + (b - a) * t; };
  float a = L(L(hash3(ix, iy, iz), hash3(ix + 1, iy, iz), fx), L(hash3(ix, iy + 1, iz), hash3(ix + 1, iy + 1, iz), fx), fy);
  float b = L(L(hash3(ix, iy, iz + 1), hash3(ix + 1, iy, iz + 1), fx), L(hash3(ix, iy + 1, iz + 1), hash3(ix + 1, iy + 1, iz + 1), fx), fy);
  return L(a, b, fz); }
inline float fbm3(float x, float y, float z, int o = 3) { float s = 0, a = 0.5f; for (int i = 0; i < o; i++) { s += a * vnoise3(x, y, z); x *= 2.03f; y *= 2.03f; z *= 2.03f; a *= 0.5f; } return s; }
// sRGB hex -> linear
inline float lin1(float v) { return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f); }
inline V3 C(uint32_t hex) { return {lin1(((hex >> 16) & 255) / 255.f), lin1(((hex >> 8) & 255) / 255.f), lin1((hex & 255) / 255.f)}; }
// the road (identical in GLSL)
constexpr float WORLD_L = 4096.f;
inline float KW(float n) { return TAU * n / WORLD_L; }
inline float pathZ(float x) { return 2.4f * std::sin(x * KW(20)) + 1.3f * std::sin(x * KW(9) + 1.7f); }
inline float pathDZ(float x) { return 2.4f * KW(20) * std::cos(x * KW(20)) + 1.3f * KW(9) * std::cos(x * KW(9) + 1.7f); }
inline float terrH(float x, float z) {
  float d = std::fabs(z - pathZ(x)), k = smooth(6.5f, 20, d), far_ = smooth(30, 150, d);
  float h1 = 1.6f * std::sin(x * KW(13) + z * 0.071f + 1.3f) + 1.2f * std::sin(x * KW(31) - z * 0.113f + 4.1f) + 0.5f * std::sin(x * KW(67) + z * 0.31f) + 0.35f * std::sin(-x * KW(113) + z * 0.47f + 2.0f);
  float h2 = far_ * far_ * (14 + 9 * std::sin(x * KW(5) + z * 0.011f) + 6 * std::sin(x * KW(11) - z * 0.023f + 1.0f));
  return k * h1 * 0.6f + h2; }
inline float angDiff(float a, float b) { float d = std::fmod(b - a, TAU); if (d > PI) d -= TAU; if (d < -PI) d += TAU; return d; }
