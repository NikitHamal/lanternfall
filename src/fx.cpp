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

// ---- v1.5 effects library: all spawn into the same fixed particle pool (no allocation)
void Fx::slash(float x, float y, float z, float yaw, float reach, float half, float dirSign, float tilt, V3 col, int n) {
  for (int i = 0; i < n; i++) {
    float u = (i + rnd()) / n, a = yaw + dirSign * lerpf(-half, half, u), rr = reach * R2(*this, 0.8f, 1.02f); Particle& q = add(); q.kind = 3;
    q.x = x + std::sin(a) * rr; q.z = z + std::cos(a) * rr; q.y = y + tilt * (u - 0.5f) + 0.18f * std::sin(u * PI);
    float sp = dirSign * reach * 3.6f; q.vx = std::cos(a) * sp; q.vz = -std::sin(a) * sp; q.vy = tilt * 1.2f; q.life = R2(*this, 0.16f, 0.3f); q.size = 0.018f + 0.04f * u;
    float k = 0.3f + 0.75f * u; q.r = col.x * k; q.g = col.y * k; q.b = col.z * k; q.a = 1; q.grav = 0; q.drag = 3.5f;
  }
  Particle& g = add(); g.kind = 0; float a = yaw + dirSign * half; g.x = x + std::sin(a) * reach * 0.9f; g.z = z + std::cos(a) * reach * 0.9f; g.y = y + tilt * 0.5f; g.life = 0.14f; g.size = reach * 0.1f; g.r = col.x * 0.4f; g.g = col.y * 0.4f; g.b = col.z * 0.4f; g.a = 0.5f;
}
void Fx::impact(float x, float y, float z, float nx, float nz, V3 col, float power) {
  int n = 6 + (int)(16 * power); sparks(x, y, z, n, col, 3.f + 4.f * power, nx, nz);
  for (int i = 0; i < 4 + (int)(6 * power); i++) { Particle& q = add(); float a, b, c; dirv(*this, a, b, c); float s = R2(*this, 7.f, 13.f) * (0.6f + 0.6f * power); q.kind = 3; q.x = x; q.y = y; q.z = z; q.vx = a * s + nx * 4; q.vy = b * s * 0.7f; q.vz = c * s + nz * 4; q.life = R2(*this, 0.08f, 0.16f); q.size = 0.05f; q.r = col.x * 1.4f; q.g = col.y * 1.4f; q.b = col.z * 1.4f; q.a = 1; q.drag = 5; }
  glow(x, y, z, 0.45f + 0.9f * power, col * 0.7f, 0.1f + 0.06f * power);
}
void Fx::shock(float x, float y, float z, float r, V3 col, int n) {
  ring(x, z, 0.3f, r, 0.45f, col, 0.18f); ring(x, z, 0.2f, r * 0.55f, 0.3f, col * 0.7f, 0.12f);
  for (int i = 0; i < n; i++) { float a = (i + rnd()) / n * TAU; Particle& q = add(); q.kind = 3; q.x = x + std::sin(a) * 0.4f; q.y = y + 0.1f; q.z = z + std::cos(a) * 0.4f; q.vx = std::sin(a) * r * 3.2f; q.vz = std::cos(a) * r * 3.2f; q.vy = R2(*this, 0.f, 1.2f); q.life = R2(*this, 0.22f, 0.42f); q.size = 0.04f; q.r = col.x; q.g = col.y; q.b = col.z; q.a = 1; q.grav = 6; q.drag = 4.f; }
  V3 dc{0.2f, 0.17f, 0.13f}; for (int i = 0; i < n / 2; i++) { float a = (i + rnd()) / (n / 2) * TAU; Particle& q = add(); q.kind = 1; q.x = x + std::sin(a) * r * 0.3f; q.y = y + 0.1f; q.z = z + std::cos(a) * r * 0.3f; q.vx = std::sin(a) * r * 1.6f; q.vz = std::cos(a) * r * 1.6f; q.vy = R2(*this, 0.2f, 0.9f); q.life = R2(*this, 0.5f, 1.0f); q.size = R2(*this, 0.4f, 0.8f); q.r = dc.x; q.g = dc.y; q.b = dc.z; q.a = 0.45f; q.grav = -0.1f; q.drag = 2.6f; }
}
void Fx::thrust(float x, float y, float z, float dx, float dy, float dz, float len_, V3 col) {
  for (int i = 0; i < 9; i++) { float u = (i + rnd()) / 9; Particle& q = add(); q.kind = 3; float s = R2(*this, 0.02f, 0.14f); float ox = R2(*this, -1, 1) * 0.12f, oy = R2(*this, -1, 1) * 0.12f; q.x = x + dx * len_ * u + ox; q.y = y + dy * len_ * u + oy; q.z = z + dz * len_ * u + ox * 0.5f; q.vx = dx * 14 * (0.5f + s * 5); q.vy = dy * 14 * (0.5f + s * 5); q.vz = dz * 14 * (0.5f + s * 5); q.life = R2(*this, 0.1f, 0.2f); q.size = 0.022f; q.r = col.x; q.g = col.y; q.b = col.z; q.a = 1; q.drag = 5; }
}
