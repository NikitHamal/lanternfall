#include "fx.h"
#include <cstdio>
Fx fx;
Particle& Fx::add() { Particle& q = p[head]; head = (head + 1) % MAXP; q = Particle(); q.t = 0; return q; }
static inline float R2(Fx& f, float a, float b) { return a + (b - a) * f.rnd(); }
static inline void dirv(Fx& f, float& x, float& y, float& z) { float a = f.rnd() * TAU, c = f.rnd() * 2 - 1, s = std::sqrt(1 - c * c); x = std::cos(a) * s; y = c; z = std::sin(a) * s; }
void Fx::embers(float x, float y, float z, int n, const V3* col, float spread, float up) {
  for (int i = 0; i < n; i++) { Particle& q = add(); float k = R2(*this, 0.6f, 1.f); V3 c = col ? *col : V3{3.0f, 1.5f, 0.35f}; q.kind = 0; q.x = x + R2(*this, -1, 1) * spread * 0.3f; q.y = y; q.z = z + R2(*this, -1, 1) * spread * 0.3f;
    q.vx = R2(*this, -1, 1) * spread; q.vz = R2(*this, -1, 1) * spread; q.vy = R2(*this, 0.3f, 1.4f) * up; q.life = R2(*this, 0.7f, 1.8f); q.size = R2(*this, 0.04f, 0.1f); q.r = c.x * k; q.g = c.y * k; q.b = c.z * k; q.a = 1; q.grav = -0.6f; q.drag = 0.8f; }
}
void Fx::sparks(float x, float y, float z, int n, V3 col, float speed, float dx, float dz) {
  for (int i = 0; i < n; i++) { Particle& q = add(); float a, b, c; dirv(*this, a, b, c); float s = speed * R2(*this, 0.4f, 1.f); q.kind = 3; q.x = x; q.y = y; q.z = z; q.vx = a * s + dx * speed; q.vy = std::fabs(b) * s * 0.8f + 0.5f; q.vz = c * s + dz * speed;
    q.life = R2(*this, 0.18f, 0.45f); q.size = 0.025f; q.r = col.x; q.g = col.y; q.b = col.z; q.a = 1; q.grav = 9; q.drag = 0.4f; }
}
void Fx::chips(float x, float y, float z, int n, V3 col, float speed) {
  for (int i = 0; i < n; i++) { Particle& q = add(); float a, b, c; dirv(*this, a, b, c); float s = speed * R2(*this, 0.3f, 1.f); q.kind = 2; q.x = x; q.y = y; q.z = z; q.vx = a * s; q.vy = std::fabs(b) * s + 1; q.vz = c * s; q.life = R2(*this, 0.5f, 1.0f); q.size = R2(*this, 0.03f, 0.07f);
    float k = R2(*this, 0.6f, 1.2f); q.r = col.x * k; q.g = col.y * k; q.b = col.z * k; q.a = 1; q.grav = 12; q.drag = 0.2f; }
}
void Fx::dust(float x, float y, float z, int n, const V3* col, float spread, float up) {
  for (int i = 0; i < n; i++) { Particle& q = add(); V3 c = col ? *col : V3{0.13f, 0.115f, 0.095f}; q.kind = 1; q.x = x + R2(*this, -1, 1) * spread * 0.4f; q.y = y; q.z = z + R2(*this, -1, 1) * spread * 0.4f; q.vx = R2(*this, -1, 1) * spread; q.vz = R2(*this, -1, 1) * spread; q.vy = R2(*this, 0.1f, 0.6f) * up;
    q.life = R2(*this, 0.6f, 1.3f); q.size = R2(*this, 0.35f, 0.8f); q.r = c.x; q.g = c.y; q.b = c.z; q.a = 0.5f; q.grav = -0.1f; q.drag = 2.0f; }
}
void Fx::frost(float x, float y, float z, int n, float spread) {
  for (int i = 0; i < n; i++) { Particle& q = add(); q.kind = 0; q.x = x + R2(*this, -1, 1) * spread; q.y = y + R2(*this, -0.2f, 0.5f); q.z = z + R2(*this, -1, 1) * spread; q.vx = R2(*this, -1, 1) * 0.5f; q.vz = R2(*this, -1, 1) * 0.5f; q.vy = R2(*this, 0, 1.2f);
    q.life = R2(*this, 0.5f, 1.2f); q.size = R2(*this, 0.03f, 0.08f); float k = R2(*this, 0.6f, 1.f); q.r = 0.8f * k; q.g = 1.8f * k; q.b = 3.2f * k; q.a = 1; q.grav = 0.5f; q.drag = 1.0f; }
}
void Fx::glow(float x, float y, float z, float size, V3 col, float dur) { Particle& q = add(); q.kind = 0; q.x = x; q.y = y; q.z = z; q.life = dur; q.size = size; q.r = col.x; q.g = col.y; q.b = col.z; q.a = 0.5f; q.drag = 0; }
void Fx::flash(float x, float y, float z, V3 col, float inten, float dur) {
  float cur = fl.on ? fl.inten * std::pow(std::max(0.f, 1 - fl.t / fl.dur), 2.f) : 0; if (inten < cur) return; fl = {true, x, y, z, 0, dur, col.x, col.y, col.z, inten};
}
void Fx::ring(float x, float z, float r0, float r1, float dur, V3 col, float thick) {
  RingFx* o = nullptr; for (auto& q : rings) if (!q.on) { o = &q; break; } if (!o) o = &rings[0]; *o = {true, x, z, r0, r1, 0, dur, col.x, col.y, col.z, thick};
}
void Fx::arc(float x, float y, float z, float face, float r, float a0, float a1, float h, const V3* col, float dur) {
  V3 c = col ? *col : V3{2.6f, 2.2f, 1.6f}; int n = 14; for (int i = 0; i < n; i++) { float u = (i + rnd()) / n, a = face + lerpf(a0, a1, u); Particle& q = add(); q.kind = 3; q.x = x + std::sin(a) * r * 0.9f; q.y = y + h * (1 - std::fabs(u * 2 - 1)) * 1.4f + 0.2f * std::sin(u * 6); q.z = z + std::cos(a) * r * 0.9f;
    float dir = a1 > a0 ? 1.f : -1.f; q.vx = std::cos(a) * dir * 3; q.vz = -std::sin(a) * dir * 3; q.vy = 0; q.life = dur * R2(*this, 0.8f, 1.6f); q.size = 0.03f; q.r = c.x; q.g = c.y; q.b = c.z; q.a = 1; q.grav = 0; q.drag = 4; }
}
void Fx::floatText(float x, float y, float z, const char* s, uint32_t col, float size, float dur) {
  FloatText* o = nullptr; for (auto& q : ft) if (!q.on) { o = &q; break; } if (!o) o = &ft[0]; o->on = true; o->x = x + R2(*this, -0.3f, 0.3f); o->y = y; o->z = z; o->t = 0; o->dur = dur + 0.4f; o->size = size; o->col = col; snprintf(o->s, 16, "%s", s);
}
void Fx::update(float dt, float t, float cx, float cy, float cz, float drizzle, V3 mote) {
  time = t; int n = 0;
  for (auto& q : p) { if (q.t >= q.life) continue; q.t += dt; n++; float d = std::exp(-q.drag * dt); q.vx *= d; q.vz *= d; q.vy = q.vy * d - q.grav * dt; q.x += q.vx * dt; q.y += q.vy * dt; q.z += q.vz * dt; if (q.kind == 2 || q.kind == 3) { if (q.y < 0.02f) { q.y = 0.02f; q.vy = 0; q.vx *= 0.3f; q.vz *= 0.3f; } } }
  nLive = n;
  for (auto& r : rings) if (r.on) { r.t += dt; if (r.t >= r.dur) r.on = false; }
  for (auto& f : ft) if (f.on) { f.t += dt; f.y += dt * 0.9f; if (f.t >= f.dur) f.on = false; }
  if (fl.on) { fl.t += dt; if (fl.t >= fl.dur) fl.on = false; }
  // ambient motes (lantern-lit dust) and drizzle around the camera focus
  static float ma = 0, ra = 0; ma += dt * 14; while (ma >= 1) { ma -= 1; Particle& q = add(); q.kind = 0; q.x = cx + R2(*this, -14, 14); q.y = R2(*this, 0.2f, 5); q.z = cz + R2(*this, -12, 8); q.vx = R2(*this, -0.1f, 0.3f); q.vy = R2(*this, -0.05f, 0.12f); q.vz = R2(*this, -0.1f, 0.1f); q.life = R2(*this, 4, 9); q.size = R2(*this, 0.012f, 0.03f); q.r = mote.x * 0.9f; q.g = mote.y * 0.9f; q.b = mote.z * 0.9f; q.a = 0.5f; q.drag = 0.1f; }
  ra += dt * drizzle * 260; while (ra >= 1) { ra -= 1; Particle& q = add(); q.kind = 4; q.x = cx + R2(*this, -16, 16); q.y = R2(*this, 6, 12); q.z = cz + R2(*this, -14, 10); q.vx = -1.5f; q.vy = -20; q.vz = 0.5f; q.life = 0.65f; q.size = 0.006f; q.r = 0.35f; q.g = 0.45f; q.b = 0.6f; q.a = 0.5f; q.drag = 0; }
  (void)cy;
}
void Fx::clear() { for (auto& q : p) q.t = q.life; for (auto& r : rings) r.on = false; for (auto& f : ft) f.on = false; fl.on = false; }
void Fx::shift(float dx) { for (auto& q : p) q.x -= dx; for (auto& r : rings) r.x -= dx; for (auto& f : ft) f.x -= dx; fl.x -= dx; }
