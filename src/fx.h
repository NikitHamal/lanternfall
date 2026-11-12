// Particles / ground marks / floating text: fixed pools, rendered by instancing.
#pragma once
#include "mathx.h"
struct Particle { float x = 0, y = 0, z = 0, vx = 0, vy = 0, vz = 0, life = 0, t = 0, size = 0, r = 0, g = 0, b = 0, a = 0, grav = 0, drag = 0; int kind = 0; };  // kind 0 glow(additive) 1 soft(alpha) 2 chip(alpha) 3 streak(additive) 4 rain
struct RingFx { bool on = false; float x, z, r0, r1, t, dur, r, g, b, thick; };
struct FloatText { bool on = false; float x, y, z, t, dur, size; char s[16]; uint32_t col; };
struct FlashLight { bool on = false; float x, y, z, t, dur, r, g, b, inten; };
constexpr int MAXP = 6000, MAXRING = 48, MAXFT = 48;
struct Fx {
  Particle p[MAXP]; int head = 0; RingFx rings[MAXRING]; FloatText ft[MAXFT]; FlashLight fl; Rng rr{99}; float time = 0; int nLive = 0;
  float rnd() { return rr(); }
  Particle& add();
  void embers(float x, float y, float z, int n, const V3* col, float spread, float up = 1);
  void sparks(float x, float y, float z, int n, V3 col, float speed, float dx = 0, float dz = 0);
  void chips(float x, float y, float z, int n, V3 col, float speed);
  void dust(float x, float y, float z, int n, const V3* col, float spread, float up = 1);
  void frost(float x, float y, float z, int n, float spread);
  void flash(float x, float y, float z, V3 col, float inten, float dur);
  void glow(float x, float y, float z, float size, V3 col, float dur);
  void ring(float x, float z, float r0, float r1, float dur, V3 col, float thick = 0.12f);
  void arc(float x, float y, float z, float face, float r, float a0, float a1, float h, const V3* col, float dur);
  void slash(float x, float y, float z, float yaw, float reach, float half, float dirSign, float tilt, V3 col, int n);   // sweeping crescent of streaks
  void impact(float x, float y, float z, float nx, float nz, V3 col, float power);                                       // hit star: sparks + streaks + flash
  void shock(float x, float y, float z, float r, V3 col, int n);                                                         // ground shockwave: ring + radial streaks + dust
  void thrust(float x, float y, float z, float dx, float dy, float dz, float len, V3 col);                                // piercing speed lines
  void floatText(float x, float y, float z, const char* s, uint32_t col, float size, float dur);
  void update(float dt, float t, float cx, float cy, float cz, float drizzle, V3 mote);
  void clear(); void shift(float dx);
};
extern Fx fx;
