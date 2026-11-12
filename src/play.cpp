// Archery play modes: simulation. Press-and-hold draw with a force curve, sweet spot, over-hold sway, ballistic arrows (gravity + quadratic drag, swept
// collision), zone scoring, hit markers, kill feed, hit-stop, practice range, wave hunt, block, dodge roll with i-frames, directional hit reactions.
#include "play.h"
#include "render.h"
#include "settings.h"
#include "roster.h"
#include <cstdio>
#include <cstring>
namespace pl {
int pendingMode = 0; float selT = 0; static int gBiome = 0;
Zone zones[MAXZONE]; VMW vmw; Player P; Hud H; Foe foes[MAXFOE]; Arrow arrows[MAXARR]; Target tgts[MAXTGT]; CamState cam; Unit playerUnit; V3 dots[40]; int nDots = 0; V3 landPt; bool landOk = false;
namespace {
int mode = M_OFF; float dumT = 0.2f, ax0 = 60, tm = 0, hitStop = 0, slowMo = 0, slowK = 1, lagX = 0, lagY = 0, botT = 0, botWait = 0, waveT = 0, dbgDraw = -1; int waveState = 0, botTgt = 0; bool botRel = false; uint32_t uid = 5000;
const float GR = 9.0f, KD = 0.0028f, ARENA = 95.f;
Rng rr(777); float R() { return rr(); }
inline V3 fwdOf(float yaw, float pitch) { float c = std::cos(pitch); return {std::sin(yaw) * c, std::sin(pitch), std::cos(yaw) * c}; }
inline V3 rightOf(float yaw) { return {-std::cos(yaw), 0, std::sin(yaw)}; }
float nz(float t, float s) { return 0.5f * std::sin(t * 1.31f + s) + 0.3f * std::sin(t * 2.93f + s * 2.1f) + 0.2f * std::sin(t * 5.17f + s * 0.7f); }
void feed(const char* s, uint32_t c) { for (int i = MAXFEED - 1; i > 0; i--) H.feed[i] = H.feed[i - 1]; snprintf(H.feed[0].s, 56, "%s", s); H.feed[0].col = c; H.feed[0].t = 6.f; }
void banner(const char* a, const char* b = "") { snprintf(H.banner, 48, "%s", a); snprintf(H.sub, 48, "%s", b); H.bannerT = 2.8f; }
void setScore(int add) { H.score += add; H.lastPts = add; snprintf(H.scoreS, 16, "%d", H.score); H.comboT = 1.4f; }
void setAcc() { snprintf(H.accS, 24, "%d/%d  %d%%", H.hits, H.shots, H.shots ? H.hits * 100 / H.shots : 0); }
void setCombo() { snprintf(H.comboS, 16, "x%d", H.combo); if (H.combo > H.bestCombo) H.bestCombo = H.combo; }
inline void ballistic(V3& p, V3& v, float h) { float sp = len(v); v.y -= GR * h; v = v * (1 - std::min(0.5f, KD * sp * h)); p = p + v * h; }
float launchSpeed(float pw) { return lerpf(19.f, 58.f, pw); }
// ---------------------------------------------------------------- targets
void tpos(const Target& t, float time, float& x, float& y, float& z) {
  x = t.ox; y = t.oy; z = t.oz; float s = std::sin(t.ph + time * t.spd);
  if (t.kind == 1) z += t.amp * s; else if (t.kind == 2) y += t.amp * s * 0.5f; else if (t.kind == 3) { z += t.amp * s; x += t.amp * 0.6f * std::cos(t.ph + time * t.spd); }
}
void mkTarget(int i, int kind, float dx, float off, float y, float R_, float amp, float spd) {
  Target& t = tgts[i]; t = Target(); t.on = true; t.kind = kind; t.ox = ax0 + dx; t.oz = pathZ(t.ox) + off; t.oy = terrH(t.ox, t.oz) + y; t.R = R_; t.amp = amp; t.spd = spd; t.ph = R() * 6.28f; tpos(t, 0, t.x, t.y, t.z);
}
void buildRange() {
  for (auto& t : tgts) t = Target(); mkTarget(0, 0, 14, 0.0f, 1.25f, 0.9f, 0, 0); mkTarget(1, 0, 22, -4.0f, 1.3f, 0.85f, 0, 0); mkTarget(2, 0, 22, 4.0f, 1.3f, 0.85f, 0, 0); mkTarget(3, 1, 30, 0, 1.4f, 0.8f, 5.5f, 0.55f);
  mkTarget(4, 0, 40, -6.0f, 1.5f, 0.85f, 0, 0); mkTarget(5, 2, 38, 3.5f, 1.4f, 0.8f, 1.6f, 0.9f); mkTarget(6, 1, 50, 0, 1.5f, 0.75f, 8.f, 0.4f); mkTarget(7, 0, 60, 5.5f, 1.8f, 0.9f, 0, 0);
  mkTarget(8, 3, 70, 0, 1.6f, 0.85f, 3.f, 0.9f); mkTarget(9, 0, 85, -3.f, 2.0f, 1.0f, 0, 0); mkTarget(10, 1, 18, 0, 1.2f, 0.7f, 3.f, 0.9f);
}
// ---------------------------------------------------------------- swept tests (return t in 0..1 or 2)
float hitFoe(const Foe& f, V3 p0, V3 p1, float& yRel) {
  const Unit& u = f.u; float r = u.radius * 0.9f + 0.1f, H_ = f.headY, base = terrH(u.x, u.z); float dx = p1.x - p0.x, dz = p1.z - p0.z, ox = p0.x - u.x, oz = p0.z - u.z;
  float a = dx * dx + dz * dz, c = ox * ox + oz * oz - r * r, t; if (c <= 0) t = 0; else { if (a < 1e-9f) return 2; float b = 2 * (ox * dx + oz * dz), d = b * b - 4 * a * c; if (d < 0) return 2; t = (-b - std::sqrt(d)) / (2 * a); }
  if (t < 0 || t > 1) return 2; yRel = p0.y + (p1.y - p0.y) * t - base; return (yRel >= -0.1f && yRel <= H_ + 0.15f) ? t : 2;
}
float hitTarget(const Target& tg, V3 p0, V3 p1, float& dz, float& dy) {
  float d0 = p0.x - tg.x, d1 = p1.x - tg.x; if (d0 * d1 > 0 || d0 == d1) return 2; float t = d0 / (d0 - d1); dz = p0.z + (p1.z - p0.z) * t - tg.z; dy = p0.y + (p1.y - p0.y) * t - tg.y; return std::hypot(dz, dy) <= tg.R * 1.04f ? t : 2;
}
// ---------------------------------------------------------------- foes
struct FoeDef { float pref, wind, cd, psp; int vol; };
const FoeDef FD[NMON] = {{7, 0.55f, 2.6f, 16, 1}, {6, 0.9f, 3.4f, 11, 1}, {0, 0.5f, 1.8f, 0, 0}, {11, 0.8f, 2.6f, 13, 1}, {8, 0.9f, 3.0f, 14, 1}, {7, 1.0f, 3.4f, 12, 1}, {12, 1.0f, 3.4f, 14, 3}, {9, 1.2f, 3.6f, 12, 3}, {12, 0.9f, 2.8f, 17, 3}, {0, 0.4f, 1.4f, 0, 0}, {0, 0.7f, 2.4f, 0, 0}, {10, 1.1f, 3.2f, 13, 3}};
Foe* freeFoe() { for (auto& f : foes) if (!f.u.used) return &f; return nullptr; }
int liveFoes() { int n = 0; for (auto& f : foes) if (f.u.used && f.u.hp > 0) n++; return n; }
void spawnFoe(int type, float x, float z) {
  Foe* f = freeFoe(); if (!f) return; const MonDef& M = MON[type]; *f = Foe(); Unit& u = f->u; u.used = true; u.team = 1; u.cls = type; u.x = u.ox = x; u.z = u.oz = z; u.radius = M.radius; u.boss = M.boss; u.elite = M.elite; u.id = ++uid; u.seen = true; u.target = &playerUnit; u.slowK = 1;
  float wv = 1 + 0.12f * std::max(0, H.wave - 1); u.maxHp = M.hp * (M.boss ? 0.5f : M.elite ? 0.75f : 1.4f) * wv; u.hp = u.maxHp; u.dmg = M.dmg * (1 + 0.06f * std::max(0, H.wave - 1)); u.speed = M.speed * (0.55f + R() * 0.12f);
  u.face = std::atan2(P.x - x, P.z - z); u.faceT = u.face; f->cd = 1.5f + R() * 2.5f; f->strafe = R() < 0.5f ? -1.f : 1.f; f->boss = M.boss; f->headY = M.hpY; fx.ring(x, z, 0.3f, 1.6f, 0.6f, {0.7f, 1.3f, 0.9f}); fx.embers(x, 0.3f, z, 8, nullptr, 0.8f, 1.5f);
}
void startWave() {
  H.wave++; waveState = 1; int w = H.wave; bool boss = w % 5 == 0; int types[NMON], nt = 0; for (int k = 0; k < NMON; k++) if (!MON[k].boss && MON[k].from <= 1 + (int)(w * 0.9f)) types[nt++] = k;
  int n = std::min(15, 3 + w * 2); if (boss) n = 5; char b[48]; snprintf(b, 48, boss ? "BOSS WAVE %d" : "WAVE %d", w); banner(b, boss ? "THE DARK SENDS ITS KEEPER" : "HOLD THE ROAD"); snprintf(H.waveS, 32, "WAVE %d", w);
  for (int i = 0; i < n; i++) { int t = types[(int)(R() * nt) % nt]; if (MON[t].elite && R() < 0.5f) t = THORNLING; float a = P.yaw + (R() - 0.5f) * 2.4f, d = 26 + R() * 16; spawnFoe(t, P.x + std::sin(a) * d, P.z + std::cos(a) * d); }
  if (boss) spawnFoe(BOSS_ORDER[(w / 5 - 1) % 3], P.x + std::sin(P.yaw) * 36, P.z + std::cos(P.yaw) * 36);
  H.foesLeft = liveFoes(); feed(b, 0xe9c98a); audio::play(S_BOSSINTRO, 0, 0.4f);
}
// ---------------------------------------------------------------- player helpers
V3 eyePos() { float crouch = P.rollT > 0 ? 0.55f * std::sin(std::min(1.f, P.rollT / ROLL_T) * PI) : 0.f; if (P.down) crouch = 1.1f * std::min(1.f, P.downT * 2); return {P.x, P.y + 1.62f - crouch + std::sin(P.bob) * 0.03f * std::min(1.f, P.speed / 4), P.z}; }
V3 camDirRaw() { return fwdOf(P.yaw + P.swayX, P.pitch + P.swayY + P.kickP); }
bool third() { return H.view == VW_THIRD; }
V3 camEye() { return H.view == VW_FIRST || !cam.init ? eyePos() : cam.eye; }
V3 aimPoint(V3 eye, V3 d) {
  float tmax = 90, t; if (gfx::rayWorld(eye.x, eye.y, eye.z, eye.x + d.x * 90, eye.y + d.y * 90, eye.z + d.z * 90, t)) tmax = std::max(4.f, t * 90);
  auto test = [&](V3 c, float r) { V3 o = c - eye; float al = dot(o, d); if (al < 3 || al > tmax) return; if (len(o - d * al) < r) tmax = al; };
  for (auto& f : foes) if (f.u.used && f.u.hp > 0) test({f.u.x, terrH(f.u.x, f.u.z) + f.headY * 0.5f, f.u.z}, f.u.radius + 0.3f);
  for (auto& t : tgts) if (t.on) test({t.x, t.y, t.z}, t.R);
  return eye + d * tmax;
}
V3 muzzle(V3 eye) { V3 r = rightOf(P.yaw), f = fwdOf(P.yaw, 0); if (third()) return {P.x + r.x * 0.25f + f.x * 0.55f, P.y + 1.42f, P.z + r.z * 0.25f + f.z * 0.55f}; return eye + camDirRaw() * 0.6f + r * 0.14f + V3{0, -0.09f, 0}; }
V3 shotDirBase(V3& from) { V3 eye = camEye(), d = camDirRaw(); V3 ap = aimPoint(eye, d); from = muzzle(eye); return norm(ap - from); }
Arrow* freeArrow() { for (auto& a : arrows) if (!a.on) return &a; Arrow* o = &arrows[0]; for (auto& a : arrows) if (a.stuck && a.life < o->life) o = &a; return o; }
void hurtPlayer(float dmg, float sx, float sz);
// ---------------------------------------------------------------- shots
void loose() {
  float pw = curve(P.draw); bool sweet = P.draw >= SWEET0 && P.holdT < 0.9f; V3 from; V3 dir = shotDirBase(from);
  float cone = ((1 - pw) * 0.05f + (P.speed > 0.5f ? 0.01f : 0.f)) / P.stab; dir = norm(dir + V3{(R() - 0.5f) * cone, (R() - 0.5f) * cone, (R() - 0.5f) * cone});
  Arrow* a = freeArrow(); *a = Arrow(); a->on = true; a->own = 0; a->x = a->px = from.x; a->y = a->py = from.y; a->z = a->pz = from.z; V3 v = dir * launchSpeed(pw); a->vx = v.x; a->vy = v.y; a->vz = v.z;
  a->dmg = 26.f * P.dmgMul * (0.35f + 0.65f * pw) * (sweet ? 1.25f : 1.f); a->pw = pw; a->life = 7; a->lu = from.x; a->lv = from.z; a->drop = sweet ? 1.f : 0.f;
  P.draw = 0; P.drawing = false; P.holdT = 0; P.relT = 0; P.nockT = 0.2f / std::min(1.6f, P.dMul); P.quiver = std::max(0, P.quiver - 1); P.kickP += 0.010f + 0.022f * pw; P.kickY += (R() - 0.5f) * 0.012f; H.shots++; setAcc(); H.zoomK = std::max(H.zoomK, 0.25f * pw);
  audio::play(pw > 0.8f ? S_BOWHEAVY : S_BOW, 0, 0.5f + 0.4f * pw); if (sweet) { fx.sparks(from.x, from.y, from.z, 6, {2.6f, 1.9f, 0.7f}, 2.f); }
}
void markHit(bool head, bool kill) { P.hmT = 0.25f; P.hmKind = kill ? 2.f : head ? 1.f : 0.f; }
void longShotFx(float dist, bool kill) { if (dist < 32) return; hitStop = 0.08f; slowMo = 0.75f; H.zoomK = 1; char b[40]; snprintf(b, 40, "%s %dm", kill ? "LONG SHOT" : "FAR SHOT", (int)dist); banner(b, ""); P.fovK = 1; }
void killFoe(Foe& f, bool head, float dist) {
  Unit& u = f.u; const MonDef& M = MON[u.cls]; u.hp = 0; u.deadT = 0; H.kills++; H.combo++; setCombo(); int pts = (int)((M.cost * 12 + 20) * (1 + 0.1f * std::min(H.combo, 12)) * (head ? 1.5f : 1.f)); setScore(pts);
  char b[56]; snprintf(b, 56, "%s%s  %dm  +%d", M.name, head ? "  HEADSHOT" : "", (int)dist, pts); feed(b, head ? 0xffd27a : 0xe9c98a); H.foesLeft = liveFoes();
  float y = terrH(u.x, u.z) + M.hpY * 0.5f; const char* dt = gFM[u.cls].death;
  if (!strcmp(dt, "shatter")) { fx.chips(u.x, y, u.z, 22, {0.5f, 0.48f, 0.42f}, 5.f); V3 c{0.3f, 0.28f, 0.24f}; fx.dust(u.x, 0.4f, u.z, 8, &c, 1.2f, 1); }
  else if (!strcmp(dt, "burn")) { fx.embers(u.x, y, u.z, 30, nullptr, 1.2f, 2.5f); fx.flash(u.x, y, u.z, {1, 0.5f, 0.15f}, 60, 0.4f); }
  else if (!strcmp(dt, "burst")) { V3 g{0.6f, 2.4f, 0.8f}; fx.embers(u.x, y, u.z, 26, &g, 1.4f, 2); fx.glow(u.x, y, u.z, 3.2f, {1.2f, 2.4f, 1.0f}, 0.35f); fx.ring(u.x, u.z, 0.3f, 2.8f, 0.45f, {0.8f, 2.0f, 0.9f}); }
  else { V3 g{0.5f, 2.2f, 1.2f}; fx.sparks(u.x, y, u.z, 14, g, 3.f); fx.glow(u.x, y, u.z, 1.6f, {0.7f, 2.4f, 1.3f}, 0.5f); }
  audio::play(M.boss || M.elite ? S_DIEBIG : S_DIE, 0, 0.6f); if (M.boss) { hitStop = 0.12f; slowMo = 1.2f; H.shake = 1.f; }
  longShotFx(dist, true);
}
void arrowHitFoe(Arrow& a, Foe& f, float t, float yRel) {
  Unit& u = f.u; bool head = yRel > f.headY * 0.78f; float dmg = a.dmg * (head ? 2.f : 1.f) * (1 - MON[u.cls].armor); float dist = std::hypot(a.px - a.lu, a.pz - a.lv);
  V3 hp{a.px + (a.x - a.px) * t, a.py + (a.y - a.py) * t, a.pz + (a.z - a.pz) * t}; { int ps = roster::at(roster::sel()).pass; if (ps == roster::PS_BLOOD && head) dmg *= 1.2f; if (ps == roster::PS_KINDLED) { dmg *= 1.12f; fx.sparks(a.x, a.y, a.z, 4, {3.f, 1.4f, 0.4f}, 2.f); } if (ps == roster::PS_RIME) { u.slowT = 2.5f; u.slowK = 0.6f; u.chillT = 2.5f; } }
  u.hp -= dmg; u.flashV = 1; H.hits++; setAcc();
  V3 d = norm(V3{a.vx, 0, a.vz}); u.stagV = 1; if (dmg > 14 || head) { u.stagT = MON[u.cls].boss ? 0.12f : 0.36f; f.kx = d.x * (head ? 4.5f : 3.f) / (1 + u.radius); f.kz = d.z * (head ? 4.5f : 3.f) / (1 + u.radius); if (u.act.on && !u.boss) { u.act.on = false; f.shot = false; } }
  char b[16]; snprintf(b, 16, "%d", (int)dmg); fx.floatText(hp.x, hp.y + 0.4f, hp.z, b, head ? 0xffd24a : 0xfff0d0, head ? 17.f : 12.f, 0.9f); if (head) fx.floatText(hp.x, hp.y + 0.9f, hp.z, "HEAD", 0xffa030, 10, 0.8f);
  fx.sparks(hp.x, hp.y, hp.z, head ? 12 : 7, head ? V3{3.f, 2.f, 0.6f} : V3{2.2f, 1.2f, 0.5f}, 3.f, -d.x, -d.z); fx.chips(hp.x, hp.y, hp.z, 3, {0.3f, 0.2f, 0.15f}, 2.f);
  bool kill = u.hp <= 0; markHit(head, kill); audio::play(S_ARROWHIT, 0, 0.7f); H.shake = std::max(H.shake, head ? 0.35f : 0.18f);
  if (kill) killFoe(f, head, dist); else { H.combo++; setCombo(); setScore((int)dmg); if (head) longShotFx(dist, false); }
}
void arrowHitTarget(Arrow& a, int ti, float t, float dz, float dy) {
  Target& tg = tgts[ti]; float d = std::hypot(dz, dy) / tg.R; int pts; const char* nm; uint32_t col;
  if (d < 0.14f) { pts = 100; nm = "BULLSEYE"; col = 0xffd24a; } else if (d < 0.34f) { pts = 50; nm = "INNER"; col = 0xff8a6a; } else if (d < 0.6f) { pts = 30; nm = "MIDDLE"; col = 0x8ab4e8; } else if (d < 0.85f) { pts = 15; nm = "OUTER"; col = 0xd0c8b0; } else { pts = 5; nm = "EDGE"; col = 0x9a917c; }
  float dist = std::hypot(a.x - a.lu, a.z - a.lv); H.hits++; H.combo++; setCombo(); pts = (int)(pts * (1 + dist / 60.f) * (1 + 0.1f * std::min(H.combo - 1, 10))); setScore(pts); setAcc();
  char b[56]; snprintf(b, 56, "%s  %dm  +%d", nm, (int)dist, pts); feed(b, col); char n[16]; snprintf(n, 16, "+%d", pts); V3 hp{tg.x - 0.1f, tg.y + dy, tg.z + dz}; fx.floatText(hp.x, hp.y + 0.5f, hp.z, n, col, pts >= 100 ? 18.f : 13.f, 1.0f);
  fx.sparks(hp.x, hp.y, hp.z, pts >= 100 ? 14 : 6, pts >= 100 ? V3{3.f, 2.2f, 0.6f} : V3{1.8f, 1.4f, 0.8f}, 2.5f, -1, 0); fx.chips(hp.x, hp.y, hp.z, 4, {0.45f, 0.35f, 0.2f}, 2.f); if (pts >= 100) fx.glow(hp.x, hp.y, hp.z, 1.2f, {3, 2.2f, 0.6f}, 0.25f);
  tg.hitT = 0.6f; tg.tilt = (dz > 0 ? 1.f : -1.f) * 0.3f * (0.4f + a.pw); tg.hits++; markHit(d < 0.14f, false); audio::play(pts >= 100 ? S_COIN : S_HITWOOD, 0, 0.7f);
  a.stuck = true; a.tg = ti; a.lu = dz; a.lv = dy; a.life = 9; V3 v = norm(V3{a.vx, a.vy, a.vz}); a.vx = v.x; a.vy = v.y; a.vz = v.z; a.x = tg.x - 0.05f; a.y = tg.y + dy; a.z = tg.z + dz; H.shake = std::max(H.shake, 0.12f);
  if (pts >= 100 && dist >= 32) { longShotFx(dist, true); }
}
// ================================================================ hero combat kits: sword & shield, heavy, fire, alchemy, frost
V3 rotYv(V3 d, float a) { float c = std::cos(a), s = std::sin(a); return {d.x * c + d.z * s, d.y, -d.x * s + d.z * c}; }
float passBonus() { return roster::at(roster::sel()).pass == roster::PS_BLOOD ? 1.f + 0.35f * (1.f - P.hp / P.maxHp) : 1.f; }
void applyStatus(Foe& f, int s) {
  Unit& u = f.u; int ps = roster::at(roster::sel()).pass;
  if (s == 1) { f.burnT = ps == roster::PS_KINDLED ? 5.f : 3.5f; f.burnDps = 6.f * P.dmgMul * (ps == roster::PS_KINDLED ? 1.6f : 1.f); }
  else if (s == 2) { float k = ps == roster::PS_RIME ? 1.5f : 1.f; u.chillT = 2.8f * k; u.slowT = 2.8f * k; u.slowK = 0.55f; }
  else if (s == 3) { u.slowT = 3.f; u.slowK = 0.7f; f.burnT = std::max(f.burnT, 3.f); f.burnDps = std::max(f.burnDps, 4.f * P.dmgMul); }
}
bool hurtFoe(Foe& f, float dmg, float stag, float dx, float dz, float kb, bool quiet, uint32_t col) {
  Unit& u = f.u; if (!u.used || u.hp <= 0) return false;
  dmg *= 1 - MON[u.cls].armor; u.hp -= dmg; u.flashV = 1; u.stagV = 1; float l = std::max(1e-4f, std::hypot(dx, dz)); dx /= l; dz /= l;
  if (stag > 0) { u.stagT = std::max(u.stagT, u.boss ? stag * 0.3f : stag); f.kx = dx * kb / (1 + u.radius); f.kz = dz * kb / (1 + u.radius); if (u.act.on && !u.boss && stag >= 0.3f) { u.act.on = false; f.shot = false; } }
  if (!quiet) { char b[16]; snprintf(b, 16, "%d", (int)dmg); fx.floatText(u.x, terrH(u.x, u.z) + f.headY * 0.65f + 0.5f, u.z, b, col, 13, 0.8f); H.hits++; setAcc(); }
  if (u.hp <= 0) { if (!quiet) markHit(false, true); killFoe(f, false, std::hypot(u.x - P.x, u.z - P.z)); return true; }
  if (!quiet) { markHit(false, false); H.combo++; setCombo(); setScore((int)dmg); }
  return false;
}
int splash(float x, float z, float r, float dmg, float stag, float kb, int status, uint32_t col) {
  int n = 0;
  for (auto& f : foes) { Unit& u = f.u; if (!u.used || u.hp <= 0) continue; float dx = u.x - x, dz = u.z - z, d = std::hypot(dx, dz); if (d > r + u.radius) continue; float k = 1.f - 0.45f * std::min(1.f, d / (r + u.radius)); applyStatus(f, status); hurtFoe(f, dmg * k, stag, dx, dz, kb, false, col); n++; }
  return n;
}
void addZone(float x, float z, float r, float t, float dps, int kind) { for (auto& zn : zones) if (!zn.on) { zn = Zone(); zn.on = true; zn.x = x; zn.z = z; zn.r = r; zn.t = t; zn.dps = dps; zn.kind = kind; return; } }
void stepZones(float h) {
  for (auto& zn : zones) { if (!zn.on) continue; zn.t -= h; if (zn.t <= 0) { zn.on = false; continue; } zn.tick += h; if (zn.tick < 0.4f) continue; zn.tick = 0;
    float a = R() * 6.28f, d = std::sqrt(R()) * zn.r; fx.glow(zn.x + std::cos(a) * d, terrH(zn.x, zn.z) + 0.3f, zn.z + std::sin(a) * d, 0.7f, {0.5f, 2.2f, 0.7f}, 0.45f);
    for (auto& f : foes) { Unit& u = f.u; if (!u.used || u.hp <= 0) continue; if (std::hypot(u.x - zn.x, u.z - zn.z) < zn.r + u.radius) { u.slowT = 0.8f; u.slowK = 0.6f; hurtFoe(f, zn.dps * 0.4f, 0, 0, 0, 0, true, 0x9aff7a); } } }
}
int meleeArc(float range, float half, float dmg, float stag, float kb, float yaw, uint32_t col) {
  int n = 0;
  for (auto& f : foes) { Unit& u = f.u; if (!u.used || u.hp <= 0) continue; float dx = u.x - P.x, dz = u.z - P.z, d = std::max(0.05f, std::hypot(dx, dz)); if (d > range + u.radius) continue;
    float ang = std::fabs(angDiff(yaw, std::atan2(dx, dz))), tol = half + std::atan2(u.radius, std::max(0.3f, d)); if (ang > tol && d > u.radius + 0.5f) continue;
    bool crit = R() < 0.12f; n++; V3 hp{u.x - dx / d * u.radius, terrH(u.x, u.z) + f.headY * 0.6f, u.z - dz / d * u.radius};
    fx.sparks(hp.x, hp.y, hp.z, 9, {2.6f, 1.7f, 0.7f}, 3.f, -dx / d, -dz / d); fx.chips(hp.x, hp.y, hp.z, 3, {0.3f, 0.2f, 0.15f}, 2.f); audio::play(S_ARROWHIT, 0, 0.8f);
    if (crit) fx.floatText(hp.x, hp.y + 1.1f, hp.z, "CRIT", 0xffa030, 10, 0.8f);
    hurtFoe(f, dmg * (crit ? 1.5f : 1.f), stag, dx, dz, kb, false, crit ? 0xffd24a : col); }
  for (auto& t : tgts) { if (!t.on) continue; float dx = t.x - P.x, dz = t.z - P.z, d = std::hypot(dx, dz); if (d > range + t.R || std::fabs(angDiff(yaw, std::atan2(dx, dz))) > half) continue;
    t.hitT = 0.6f; t.tilt = 0.4f; H.hits++; H.combo++; setCombo(); setScore(25); setAcc(); fx.sparks(t.x - 0.2f, t.y, t.z, 8, {2.f, 1.4f, 0.6f}, 2.5f); audio::play(S_HITWOOD, 0, 0.7f); fx.floatText(t.x, t.y + 0.6f, t.z, "SMASH", 0xffd070, 12, 0.8f); n++; }
  if (n) { hitStop = std::max(hitStop, 0.035f + 0.03f * std::min(1.f, stag)); H.shake = std::max(H.shake, 0.18f + 0.25f * std::min(1.f, stag)); } else audio::play(S_SWING, 0, 0.4f);
  return n;
}
void pickYaw() {
  P.atkYaw = P.yaw; if (P.style != roster::ST_SWORD && P.style != roster::ST_HEAVY) return; float bd = 7.f;
  for (auto& f : foes) { const Unit& u = f.u; if (!u.used || u.hp <= 0) continue; float dx = u.x - P.x, dz = u.z - P.z, d = std::hypot(dx, dz); if (d < bd && std::fabs(angDiff(P.yaw, std::atan2(dx, dz))) < 1.0f) { bd = d; P.atkYaw = std::atan2(dx, dz); } }
}
void startAct(int kind, float wind, float rec, bool charged, float cf, int idx = 0) {
  float sp = std::max(0.5f, P.dMul) * (P.abilT > 0 && roster::at(roster::sel()).abil == roster::AB_SURGE ? 1.3f : 1.f);
  P.atkKind = kind; P.atkT = 0; P.atkWind = wind / sp; P.atkRec = rec / sp; P.atkOn = true; P.atkDone = false; P.atkCharged = charged; P.atkCf = cf; P.atkIdx = idx; pickYaw(); P.relT = 0; P.drawing = false; H.shots++; setAcc();
  float cost = P.style == roster::ST_HEAVY && charged ? 16.f : P.style == roster::ST_SWORD || P.style == roster::ST_HEAVY ? 7.f : 3.f; P.st = std::max(0.f, P.st - cost); P.stWait = std::max(P.stWait, 0.45f);
  if (P.style == roster::ST_SWORD || P.style == roster::ST_HEAVY) { V3 f = fwdOf(P.atkYaw, 0); P.lungeT = wind / sp; P.lungeX = f.x * (charged ? 2.6f : 3.4f); P.lungeZ = f.z * (charged ? 2.6f : 3.4f); }
}
Arrow* spawnProj(int kind, V3 from, V3 dir, float speed, float dmg, float grav, float life, int pierce, float rad) {
  Arrow* a = freeArrow(); *a = Arrow(); a->on = true; a->own = 0; a->kind = kind; a->x = a->px = from.x; a->y = a->py = from.y; a->z = a->pz = from.z; V3 v = dir * speed; a->vx = v.x; a->vy = v.y; a->vz = v.z;
  a->dmg = dmg; a->grav = grav; a->life = life; a->pierce = pierce; a->rad = rad; a->lu = from.x; a->lv = from.z; a->pw = 1; return a;
}
void spellImpact(Arrow& a, V3 hp, Foe* f) {
  V3 d = norm(V3{a.vx, 0, a.vz}); int ps = roster::at(roster::sel()).pass;
  switch (a.kind) {
    case 10: if (f) { applyStatus(*f, 1); hurtFoe(*f, a.dmg, 0.2f, d.x, d.z, 2.f, false, 0xffa050); } fx.sparks(hp.x, hp.y, hp.z, 10, {3.f, 1.4f, 0.4f}, 3.f); fx.flash(hp.x, hp.y, hp.z, {1.f, 0.55f, 0.2f}, 40, 0.15f); audio::play(S_HITSOFT, 0, 0.6f); a.on = false; break;
    case 11: splash(hp.x, hp.z, a.rad, a.dmg, 0.6f, 7.f, 1, 0xffa050); fx.ring(hp.x, hp.z, 0.3f, a.rad, 0.5f, {3.f, 1.3f, 0.3f}); fx.flash(hp.x, hp.y, hp.z, {1.f, 0.6f, 0.2f}, 100, 0.3f); fx.embers(hp.x, hp.y, hp.z, 26, nullptr, 1.2f, 2.5f); fx.glow(hp.x, hp.y, hp.z, a.rad * 0.8f, {2.4f, 1.2f, 0.4f}, 0.3f); audio::play(S_BOSSSLAM, 0, 0.45f); H.shake = std::max(H.shake, 0.4f); a.on = false; break;
    case 12: case 13: { int n = splash(hp.x, hp.z, a.rad, a.dmg, 0.3f, 3.f, 3, 0x9aff7a); fx.ring(hp.x, hp.z, 0.3f, a.rad, 0.5f, {0.6f, 2.2f, 0.7f}); fx.glow(hp.x, hp.y, hp.z, a.rad * 0.7f, {0.6f, 2.4f, 0.8f}, 0.4f); V3 g{0.5f, 2.2f, 0.7f}; fx.embers(hp.x, hp.y, hp.z, 18, &g, 1.f, 2.f); audio::play(S_HITSOFT, 0, 0.7f);
      if (a.kind == 13) addZone(hp.x, hp.z, a.rad * 0.85f, 4.5f, 11.f * P.dmgMul, 1); if (n > 0 && ps == roster::PS_MEDIC) { P.hp = std::min(P.maxHp, P.hp + 5.f); fx.floatText(P.x, P.y + 2.2f, P.z, "+5", 0x9aff7a, 11, 0.7f); } a.on = false; break; }
    case 14: if (f) { applyStatus(*f, 2); hurtFoe(*f, a.dmg, 0.15f, d.x, d.z, 2.f, false, 0xbfe8ff); } fx.sparks(hp.x, hp.y, hp.z, 8, {0.8f, 1.6f, 2.4f}, 3.f); audio::play(S_HITSOFT, 0, 0.5f); if (!f || --a.pierce < 0) a.on = false; break;
    case 15: { if (f) { bool shat = f->u.chillT > 0 || ps == roster::PS_RIME; applyStatus(*f, 2); hurtFoe(*f, a.dmg * (shat ? 1.5f : 1.f), 0.7f, d.x, d.z, 5.f, false, 0xbfe8ff); if (shat) { fx.chips(hp.x, hp.y, hp.z, 12, {0.6f, 0.85f, 1.f}, 4.f); fx.floatText(hp.x, hp.y + 1.f, hp.z, "SHATTER", 0x9fdcff, 11, 0.8f); } }
      fx.sparks(hp.x, hp.y, hp.z, 14, {0.8f, 1.8f, 2.6f}, 4.f); fx.flash(hp.x, hp.y, hp.z, {0.5f, 0.8f, 1.f}, 50, 0.2f); audio::play(S_ARROWHIT, 0, 0.6f); H.shake = std::max(H.shake, 0.2f); if (!f || --a.pierce < 0) a.on = false; break; }
  }
}
void spellFoe(Arrow& a, int idx, float t) { Foe& f = foes[idx]; a.hm |= 1u << idx; V3 hp{a.px + (a.x - a.px) * t, a.py + (a.y - a.py) * t, a.pz + (a.z - a.pz) * t}; spellImpact(a, hp, &f); }
void spellTrail(const Arrow& a, V3 p) {
  if ((int)(a.t * 45) == (int)((a.t + 0.008f) * 45)) return;
  switch (a.kind) { case 10: case 11: fx.embers(p.x, p.y, p.z, 1, nullptr, 0.3f, 0.7f); fx.glow(p.x, p.y, p.z, 0.6f, {2.f, 0.9f, 0.3f}, 0.12f); break; case 12: case 13: fx.sparks(p.x, p.y, p.z, 1, {0.5f, 1.8f, 0.6f}, 0.8f); break; default: fx.sparks(p.x, p.y, p.z, 1, {0.7f, 1.5f, 2.4f}, 0.8f); fx.glow(p.x, p.y, p.z, 0.5f, {0.5f, 1.0f, 2.0f}, 0.1f); }
}
void actEffect() {
  const int st = P.style; float dm = P.dmgMul * passBonus(); bool ch = P.atkCharged; float cf = P.atkCf, yaw = P.atkYaw; V3 f = fwdOf(yaw, 0); P.relT = 0;
  if (st == roster::ST_SWORD) {
    if (P.atkKind == A_BASH) { meleeArc(2.7f, 0.95f, 14 * dm, 1.1f, 9.f, yaw, 0xcfe0ff); fx.ring(P.x + f.x * 1.2f, P.z + f.z * 1.2f, 0.2f, 2.2f, 0.3f, {1.4f, 1.6f, 2.2f}); audio::play(S_HITSHIELD, 0, 0.7f); fx.sparks(P.x + f.x * 1.1f, P.y + 1.2f, P.z + f.z * 1.1f, 10, {1.8f, 2.f, 2.4f}, 3.f); }
    else { static const float DM[3] = {22, 24, 40}, RG[3] = {2.9f, 2.9f, 3.5f}, HF[3] = {0.95f, 0.95f, 1.4f}, SG[3] = {0.25f, 0.25f, 0.9f}, KB[3] = {2.5f, 2.5f, 8.f}; int i = P.atkIdx % 3; float rip = P.ripT > 0 ? 2.f : 1.f;
      if (rip > 1) { P.ripT = 0; fx.floatText(P.x, P.y + 2.4f, P.z, "RIPOSTE", 0xffe08a, 13, 0.9f); }
      int n = meleeArc(RG[i], HF[i], DM[i] * dm * rip, SG[i], KB[i], yaw, 0xfff0d0); (void)n; audio::play(S_SWING, 0, 0.5f); fx.sparks(P.x + f.x * 1.9f, P.y + 1.2f, P.z + f.z * 1.9f, 4, {2.f, 1.8f, 1.2f}, 2.f);
      if (i == 2) fx.ring(P.x + f.x * 1.5f, P.z + f.z * 1.5f, 0.3f, 3.2f, 0.4f, {2.f, 1.6f, 0.9f}); }
  } else if (st == roster::ST_HEAVY) {
    if (!ch) { meleeArc(3.6f, 1.5f, 34 * dm, 0.55f, 7.f, yaw, 0xfff0d0); audio::play(S_SWING, 0, 0.7f); fx.ring(P.x + f.x * 1.4f, P.z + f.z * 1.4f, 0.3f, 3.5f, 0.35f, {2.2f, 1.5f, 0.8f}); }
    else { float cx = P.x + f.x * 2.3f, cz = P.z + f.z * 2.3f, r = 3.3f + 1.7f * cf; splash(cx, cz, r, (40 + 64 * cf) * dm * (P.ripT > 0 ? 1.5f : 1.f), 0.9f, 9.f, 0, 0xffc070); P.ripT = 0;
      fx.ring(cx, cz, 0.3f, r, 0.55f, {2.6f, 1.6f, 0.6f}); fx.ring(cx, cz, 0.2f, r * 0.6f, 0.4f, {2.f, 1.2f, 0.5f}); V3 c{0.3f, 0.26f, 0.2f}; fx.dust(cx, P.y + 0.2f, cz, 14, &c, 0.6f, 1.2f); fx.chips(cx, P.y + 0.2f, cz, 14, {0.45f, 0.4f, 0.32f}, 5.f); fx.flash(cx, P.y + 0.6f, cz, {1.f, 0.7f, 0.3f}, 45, 0.2f);
      H.shake = std::max(H.shake, 0.7f); hitStop = std::max(hitStop, 0.07f); audio::play(S_BOSSSLAM, 0, 0.55f); }
  } else {
    V3 from; V3 dir = shotDirBase(from); P.kickP += 0.012f; fx.glow(from.x, from.y, from.z, 1.4f, st == roster::ST_FROST ? V3{0.8f, 1.8f, 2.6f} : st == roster::ST_ALCH ? V3{0.6f, 2.2f, 0.8f} : V3{2.6f, 1.3f, 0.4f}, 0.2f);
    if (st == roster::ST_FIRE) { if (!ch) { spawnProj(10, from, dir, 40.f, 20 * dm, 1.5f, 3.f, 0, 0); audio::play(S_GLOAMCAST, 0, 0.5f); } else { spawnProj(11, from, dir, 22.f, (36 + 48 * cf) * dm, 0.8f, 4.f, 0, 3.2f + 1.8f * cf); audio::play(S_BOWHEAVY, 0, 0.6f); } }
    else if (st == roster::ST_ALCH) {
      V3 eye = camEye(); V3 ap = aimPoint(eye, camDirRaw()); float want = ch ? 10.f + 18.f * cf : 9.f + 8.f * cf; V3 to = ap - from; float dist = std::max(3.f, std::min(len(to), want)); V3 tg = from + norm(to) * dist; float T = std::max(0.4f, dist / 20.f); V3 v = (tg - from) * (1.f / T); v.y += 0.5f * 9.f * T;
      Arrow* a = spawnProj(ch ? 13 : 12, from, norm(v), len(v), (ch ? 38.f + 32.f * cf : 20.f) * dm, 9.f, 4.f, 0, ch ? 3.8f + 1.2f * cf : 2.4f); (void)a; audio::play(S_SWING, 0, 0.45f); }
    else { if (!ch) { for (int k = -1; k <= 1; k++) spawnProj(14, from, norm(rotYv(dir, k * 0.07f)), 52.f, 13 * dm, 0.f, 1.6f, 1, 0); audio::play(S_BOW, 0, 0.55f); } else { spawnProj(15, from, dir, 70.f, (48 + 42 * cf) * dm, 0.f, 1.4f, 6, 0); audio::play(S_BOWHEAVY, 0, 0.7f); } }
  }
}
void combatStep(float h, const Input& in) {
  const int st = P.style; if (st == roster::ST_BOW) return;
  P.comboT = std::max(0.f, P.comboT - h); P.ripT = std::max(0.f, P.ripT - h); P.lungeT -= h; if (P.comboT <= 0 && !P.atkOn) P.atkCombo = 0;
  if (P.lungeT > 0) { P.x += P.lungeX * h; P.z += P.lungeZ * h; gfx::pushOut(P.x, P.z, 0.38f); P.y = terrH(P.x, P.z); }
  bool press = in.lmb && !P.lmbPrev; P.lmbPrev = in.lmb; bool canAct = !P.down && P.stagT <= 0 && P.rollT <= 0 && H.view != VW_FREE && P.st > 2.f;
  static const float CMIN[6] = {9, 9, 0.35f, 0.35f, 0.3f, 0.3f}, CFULL[6] = {9, 9, 1.f, 0.9f, 0.8f, 1.f};
  if (!canAct && !P.atkOn) P.charging = false;
  if (st == roster::ST_SWORD) {
    if (!P.atkOn && canAct && ((press && P.blocking > 0.5f && P.st >= 10) || (press && P.blocking <= 0.5f) || (in.lmb && P.blocking <= 0.5f && P.comboT > 0))) {
      if (P.blocking > 0.5f) startAct(A_BASH, 0.2f, 0.35f, false, 0, 3); else { static const int K[3] = {A_BASIC, A_QUICK, A_CLEAVE}; static const float W_[3] = {0.26f, 0.22f, 0.34f}, R_[3] = {0.28f, 0.25f, 0.45f}; int i = P.atkCombo % 3; P.atkCombo = (i + 1) % 3; startAct(K[i], W_[i], R_[i], false, 0, i); P.comboT = (W_[i] + R_[i]) / std::max(0.5f, P.dMul) + 0.5f; } }
  } else {
    if (!P.atkOn) {
      if (in.lmb && canAct && P.blocking < 0.3f) { if (!P.charging) { P.charging = true; P.chargeT = 0; audio::play(S_CLICK, 0, 0.25f); } float before = P.chargeT; P.chargeT = std::min(CFULL[st], P.chargeT + h * std::max(0.6f, P.dMul));
        if (before < CMIN[st] && P.chargeT >= CMIN[st]) { fx.ring(P.x, P.z, 0.2f, 1.6f, 0.3f, {1.0f, 0.8f, 0.4f}); audio::play(S_COIN, 0, 0.25f); } if (P.chargeT >= CFULL[st] && before < CFULL[st]) fx.flash(P.x, P.y + 1.4f, P.z, {1.4f, 1.2f, 0.8f}, 40, 0.15f); }
      else if (P.charging) { float ct = P.chargeT; bool charged = ct >= CMIN[st]; float cf = clampf((ct - CMIN[st]) / std::max(0.05f, CFULL[st] - CMIN[st]), 0, 1); P.charging = false; P.chargeT = 0;
        switch (st) { case roster::ST_HEAVY: if (!charged) startAct(A_CLEAVE, 0.3f, 0.45f, false, 0); else startAct(A_SUNDER, 0.2f, 0.65f, true, cf); break;
          case roster::ST_FIRE: if (!charged) startAct(A_KINDLE, 0.18f, 0.26f, false, 0); else startAct(A_CINDER, 0.26f, 0.4f, true, cf); break;
          case roster::ST_ALCH: if (!charged) startAct(A_QUICK, 0.18f, 0.26f, false, 0); else startAct(A_FIREPOT, 0.28f, 0.45f, true, cf); break;
          default: if (!charged) startAct(A_QUICK, 0.16f, 0.22f, false, 0); else startAct(A_LANCE, 0.28f, 0.45f, true, cf); } }
    } else P.charging = false;
  }
  if (P.atkOn) { P.atkT += h; if (!P.atkDone && P.atkT >= P.atkWind) { P.atkDone = true; actEffect(); } if (P.atkT >= P.atkWind + P.atkRec) P.atkOn = false; }
  if (P.atkOn || P.charging) { float ty = P.atkOn ? P.atkYaw : P.yaw; P.face += angDiff(P.face, ty) * (1 - std::exp(-18.f * h)); }
}
void spawnDummy(int slot) {
  static const int TY[4] = {THORNLING, PEATWIGHT, RUSTBOUND, CAIRNBACK}; static const float DX[4] = {7.f, 9.5f, 7.5f, 10.f}, OFF[4] = {-3.8f, -1.3f, 1.4f, 3.9f};
  Foe* f = freeFoe(); if (!f) return; float x = ax0 + DX[slot], z = pathZ(x) + OFF[slot]; spawnFoe(TY[slot], x, z); f->ai = 1; f->u.maxHp = f->u.hp = 320; f->u.speed = 0; f->hx = x; f->hz = z; f->cd = (float)slot;
}
void stepArrow(Arrow& a, float h) {
  if (a.stuck) { a.life -= h; if (a.tg >= 0 && tgts[a.tg].on) { const Target& t = tgts[a.tg]; a.x = t.x - 0.05f; a.y = t.y + a.lv; a.z = t.z + a.lu; } if (a.life <= 0) a.on = false; return; }
  a.px = a.x; a.py = a.y; a.pz = a.z; V3 p{a.x, a.y, a.z}, v{a.vx, a.vy, a.vz};
  if (a.own == 0 && a.kind >= 10) { v.y -= a.grav * h; p = p + v * h; spellTrail(a, p); } else if (a.own == 0) ballistic(p, v, h); else { v.y -= 3.f * h; p = p + v * h; }
  a.x = p.x; a.y = p.y; a.z = p.z; a.vx = v.x; a.vy = v.y; a.vz = v.z; a.t += h; a.life -= h; if (a.life <= 0) { a.on = false; return; }
  V3 p0{a.px, a.py, a.pz}, p1 = p; float best = 2; int kind = 0, idx = -1; float e0 = 0, e1 = 0;
  if (a.own == 0) {
    for (int i = 0; i < MAXFOE; i++) { Foe& f = foes[i]; if (!f.u.used || f.u.hp <= 0 || ((a.hm >> i) & 1u)) continue; float yr; float t = hitFoe(f, p0, p1, yr); if (t < best) { best = t; kind = 1; idx = i; e0 = yr; } }
    for (int i = 0; i < MAXTGT; i++) { Target& g = tgts[i]; if (!g.on) continue; float dz, dy; float t = hitTarget(g, p0, p1, dz, dy); if (t < best) { best = t; kind = 2; idx = i; e0 = dz; e1 = dy; } }
  } else {
    float ox = p0.x - P.x, oy = p0.y - (P.y + 1.1f), oz = p0.z - P.z, dx = p1.x - p0.x, dy = p1.y - p0.y, dz = p1.z - p0.z, l2 = dx * dx + dy * dy + dz * dz; float tt = l2 > 1e-9f ? clampf(-(ox * dx + oy * dy + oz * dz) / l2, 0, 1) : 0;
    float cx = ox + dx * tt, cy = oy + dy * tt, cz = oz + dz * tt, dd = std::sqrt(cx * cx + cy * cy + cz * cz);
    if (P.iT > 0 && dd < 1.1f && a.drop == 0 && !P.down) { a.drop = 1; fx.floatText(P.x, P.y + 2.2f, P.z, "DODGE", 0x9ae8ff, 13, 0.9f); slowMo = std::max(slowMo, 0.45f); feed("DODGE  - near miss", 0x9ae8ff); }
    else if (dd < 0.55f && !P.down && P.iT <= 0) { best = tt; kind = 4; }
  }
  float tw; if (gfx::rayWorld(p0.x, p0.y, p0.z, p1.x, p1.y, p1.z, tw) && tw < best) { best = tw; kind = 3; }
  if (best > 1) return;
  if (kind == 1) { if (a.kind >= 10) spellFoe(a, idx, best); else { arrowHitFoe(a, foes[idx], best, e0); a.on = false; } }
  else if (kind == 2) { if (a.kind >= 10) { V3 hp2 = p0 + (p1 - p0) * best; arrowHitTarget(a, idx, best, e0, e1); a.stuck = false; a.tg = -1; spellImpact(a, hp2, nullptr); a.pierce = -1; a.on = false; } else arrowHitTarget(a, idx, best, e0, e1); }
  else if (kind == 4) { hurtPlayer(a.dmg, a.px, a.pz); a.on = false; }
  else if (kind == 3 && a.own == 0 && a.kind >= 10) { V3 hp2 = p0 + (p1 - p0) * best; a.pierce = -1; spellImpact(a, hp2, nullptr); a.on = false; }
  else { V3 hp = p0 + (p1 - p0) * best; if (a.own == 0) { a.stuck = true; a.tg = -1; a.life = 7; V3 d = norm(v); a.vx = d.x; a.vy = d.y; a.vz = d.z; a.x = hp.x; a.y = hp.y; a.z = hp.z; if (H.combo > 0) { H.combo = 0; setCombo(); } V3 c{0.22f, 0.18f, 0.12f}; fx.dust(hp.x, hp.y, hp.z, 3, &c, 0.15f, 0.4f); }
    else { a.on = false; fx.sparks(hp.x, hp.y, hp.z, 5, {2.f, 1.1f, 0.4f}, 2.f); } }
}
// ---------------------------------------------------------------- damage taken / defence
void revive() {
  P.lives--; if (P.lives < 0) { P.over = true; H.ended = true; banner("THE LANTERN FALLS", "ENTER: BACK TO TITLE   R: TRY AGAIN"); H.resT = 0; return; }
  P.down = false; P.hp = P.maxHp * 0.6f; P.iT = 1.6f; P.downT = 0; P.st = 100; fx.ring(P.x, P.z, 0.3f, 6.f, 0.6f, {2.f, 1.4f, 0.5f}); fx.flash(P.x, P.y + 1, P.z, {1, 0.7f, 0.3f}, 80, 0.4f); audio::play(S_REVIVE, 0, 0.8f);
  for (auto& f : foes) if (f.u.used && f.u.hp > 0) { float dx = f.u.x - P.x, dz = f.u.z - P.z, d = std::max(0.5f, std::hypot(dx, dz)); if (d < 12) { f.kx = dx / d * 9; f.kz = dz / d * 9; f.u.stagT = 0.8f; f.u.stagV = 1; } }
  feed("RISEN - flames left shown", 0xffb878);
}
void hurtPlayer(float dmg, float sx, float sz) {
  if (mode != M_WAVES || P.down || P.iT > 0) return; float ang = angDiff(P.yaw, std::atan2(sx - P.x, sz - P.z)); dmg *= 1.f - P.armor; if (P.abilT > 0 && roster::at(roster::sel()).abil == roster::AB_BULWARK) dmg *= 0.3f;
  const int st = P.style; const bool shield = st == roster::ST_SWORD || st == roster::ST_HEAVY || st == roster::ST_BOW; bool arcOk = !shield || std::fabs(ang) < 1.25f;
  if (P.blocking > 0.6f && arcOk && P.st > 0) {
    bool parry = P.blockT < 0.3f && (st == roster::ST_SWORD || st == roster::ST_HEAVY);
    V3 f = fwdOf(P.yaw, 0); float bx = P.x + f.x * 0.7f, bz = P.z + f.z * 0.7f;
    if (parry) { dmg = 0; P.ripT = 1.8f; P.st = std::min(100.f, P.st + 12); P.blockK = 1; fx.ring(P.x, P.z, 0.3f, 3.f, 0.4f, {2.6f, 2.f, 0.8f}); fx.sparks(bx, P.y + 1.3f, bz, 16, {3.f, 2.4f, 1.f}, 4.f); fx.floatText(P.x, P.y + 2.4f, P.z, "PARRY", 0xffe08a, 14, 0.9f); feed("PARRY - riposte ready", 0xffe08a); audio::play(S_HITSHIELD, 0, 1.f); slowMo = std::max(slowMo, 0.35f); hitStop = std::max(hitStop, 0.06f); H.shake = std::max(H.shake, 0.3f);
      Foe* nf = nullptr; float nd = 4.5f; for (auto& fo : foes) if (fo.u.used && fo.u.hp > 0) { float d = std::hypot(fo.u.x - sx, fo.u.z - sz); if (d < nd) { nd = d; nf = &fo; } } if (nf) { nf->u.stagT = 1.0f; nf->u.act.on = false; nf->shot = false; nf->u.stagV = 1; nf->kx = (nf->u.x - P.x) * 3.f; nf->kz = (nf->u.z - P.z) * 3.f; } }
    else { static const float K[6] = {0.15f, 0.12f, 0.3f, 0.4f, 0.5f, 0.35f}; dmg *= K[st % 6]; P.st -= (st == roster::ST_SWORD || st == roster::ST_BOW) ? 16 : 22; P.stWait = 1.2f; P.blockK = 1;
      V3 col = st == roster::ST_FIRE ? V3{3.f, 1.4f, 0.4f} : st == roster::ST_ALCH ? V3{0.6f, 2.4f, 0.8f} : st == roster::ST_FROST ? V3{0.8f, 1.8f, 2.8f} : V3{2.4f, 1.8f, 0.8f}; fx.sparks(bx, P.y + 1.3f, bz, 10, col, 3.f); audio::play(S_HITSHIELD, 0, 0.8f); H.shake = std::max(H.shake, 0.2f);
      fx.floatText(P.x, P.y + 2.2f, P.z, shield ? "BLOCK" : "WARD", 0xd8e8ff, 12, 0.7f);
      Foe* nf = nullptr; float nd = st == roster::ST_FROST ? 6.f : 4.f; for (auto& fo : foes) if (fo.u.used && fo.u.hp > 0) { float d = std::hypot(fo.u.x - P.x, fo.u.z - P.z); if (d < nd) { nd = d; nf = &fo; } }
      if (nf && st == roster::ST_FIRE) { applyStatus(*nf, 1); fx.embers(nf->u.x, terrH(nf->u.x, nf->u.z) + 1.f, nf->u.z, 8, nullptr, 0.8f, 1.5f); } if (nf && st == roster::ST_FROST) { applyStatus(*nf, 2); nf->u.stagT = std::max(nf->u.stagT, 0.4f); fx.sparks(nf->u.x, terrH(nf->u.x, nf->u.z) + 1.f, nf->u.z, 10, {0.8f, 1.8f, 2.8f}, 3.f); } if (st == roster::ST_ALCH) P.hp = std::min(P.maxHp, P.hp + 5.f);
      if (P.st <= 0) { P.stagT = 0.5f; P.blocking = 0; } }
  }
  else { P.hurtT = 0.4f; P.hitDir = ang; P.hitK = 1; P.vig = 1; H.shake = std::max(H.shake, 0.5f); P.kickP += 0.05f; P.kickY += (ang > 0 ? 1.f : -1.f) * 0.04f; audio::play(S_HITSOFT, 0, 0.9f); fx.flash(P.x, P.y + 1.2f, P.z, {1.f, 0.2f, 0.1f}, 25, 0.15f);
    bool armored = st == roster::ST_HEAVY && (P.charging || P.atkOn);
    if (dmg >= 9 * P.stab && !armored) { P.stagT = 0.45f; P.drawing = false; P.draw = 0; P.holdT = 0; P.atkOn = false; P.charging = false; P.chargeT = 0; } H.combo = 0; setCombo(); }
  P.hp -= dmg; char b[16]; snprintf(b, 16, "-%d", (int)dmg); if (dmg > 0) fx.floatText(P.x, P.y + 2.0f, P.z, b, 0xff6a5a, 12, 0.8f);
  if (P.hp <= 0) { P.hp = 0; P.down = true; P.downT = 0; P.drawing = false; P.draw = 0; P.atkOn = false; P.charging = false; feed("YOU ARE DOWN", 0xff6a5a); audio::play(S_HERODOWN, 0, 0.9f); slowMo = 0.8f; }
}
void fireFoe(Foe& f) {
  Unit& u = f.u; const FoeDef& d = FD[u.cls]; float sy = terrH(u.x, u.z) + MON[u.cls].hpY * 0.6f; V3 o{u.x + std::sin(u.face) * (u.radius + 0.3f), sy, u.z + std::cos(u.face) * (u.radius + 0.3f)};
  float dist = std::hypot(P.x - o.x, P.z - o.z), T = std::max(0.25f, dist / d.psp); V3 tg{P.x + P.vx * T, P.y + 1.15f, P.z + P.vz * T}; V3 v = (tg - o) * (1 / T); v.y += 0.5f * 3.f * T;
  for (int k = 0; k < d.vol; k++) { Arrow* a = freeArrow(); if (!a) break; *a = Arrow(); a->on = true; a->own = 1; a->x = a->px = o.x; a->y = a->py = o.y; a->z = a->pz = o.z; float rot = (k - (d.vol - 1) * 0.5f) * 0.16f, c = std::cos(rot), s = std::sin(rot);
    a->vx = v.x * c - v.z * s; a->vz = v.x * s + v.z * c; a->vy = v.y; a->dmg = u.dmg * 0.8f; a->pw = (float)u.cls; a->life = 4; }
  audio::play(u.boss ? S_BOSSSLAM : S_GLOAMCAST, 0, 0.45f); fx.glow(o.x, o.y, o.z, 1.2f, {1.4f, 2.f, 1.2f}, 0.2f);
}
void stepFoe(Foe& f, float h) {
  Unit& u = f.u; if (!u.used) return; const MonDef& M = MON[u.cls];
  if (u.hp <= 0) { u.deadT += h; if (u.deadT > 5.2f) u.used = false; return; }
  if (u.chillT > 0) u.chillT -= h;
  if (f.burnT > 0) { f.burnT -= h; f.burnAcc += h; if (f.burnAcc >= 0.5f) { f.burnAcc = 0; fx.embers(u.x, terrH(u.x, u.z) + f.headY * 0.6f, u.z, 2, nullptr, 0.3f, 0.8f); if (hurtFoe(f, f.burnDps * 0.5f, 0, 0, 0, 0, true, 0xff9a40)) return; } }
  u.flashV *= std::exp(-9 * h); u.stagV *= std::exp(-6 * h); float dx = P.x - u.x, dz = P.z - u.z, dist = std::max(0.01f, std::hypot(dx, dz)); const FoeDef& D = FD[u.cls];
  float want = std::atan2(dx, dz); u.face += angDiff(u.face, want) * (1 - std::exp(-(u.act.on ? 4.f : 8.f) * h));
  if (u.stagT > 0) { u.stagT -= h; u.x += f.kx * h; u.z += f.kz * h; f.kx *= std::exp(-6 * h); f.kz *= std::exp(-6 * h); u.moving = 0; gfx::pushOut(u.x, u.z, u.radius); return; }
  if (f.ai == 1) { u.moving = 0; u.hp = std::min(u.maxHp, u.hp + 10 * h); u.x += (f.hx - u.x) * std::min(1.f, h * 1.5f); u.z += (f.hz - u.z) * std::min(1.f, h * 1.5f); return; }
  if (u.act.on) { u.act.t += h; u.moving = 0; if (!f.shot && u.act.t >= u.act.wind) { f.shot = true; if (D.pref > 0) fireFoe(f); else if (dist < u.radius + 3.2f) hurtPlayer(u.dmg, u.x, u.z); } if (u.act.t >= u.act.wind + u.act.rec) { u.act.on = false; f.cd = D.cd * (0.8f + R() * 0.5f); } return; }
  f.cd -= h; float sp = 0, mx = 0, mz = 0, ux = dx / dist, uz = dz / dist;
  if (D.pref == 0) { sp = u.speed * 1.5f; if (dist > u.radius + 1.6f) { mx = ux; mz = uz; } else sp = 0; if (f.cd <= 0 && dist < u.radius + 2.6f) { u.act = Act(); u.act.on = true; u.act.kind = A_MONATK; u.act.wind = D.wind; u.act.rec = 0.6f; f.shot = false; fx.ring(u.x, u.z, 0.2f, 2.4f, D.wind, {2.f, 0.6f, 0.2f}); } }
  else { sp = u.speed; if (dist > D.pref + 2) { mx = ux; mz = uz; } else if (dist < D.pref - 3) { mx = -ux; mz = -uz; sp *= 0.8f; } else { mx = -uz * f.strafe; mz = ux * f.strafe; sp *= 0.55f; if (R() < 0.002f) f.strafe = -f.strafe; }
    if (f.cd <= 0 && dist < D.pref * 1.8f + 6) { u.act = Act(); u.act.on = true; u.act.kind = A_MONATK; u.act.wind = D.wind; u.act.rec = 0.5f; f.shot = false; fx.ring(u.x, u.z, 0.2f, 1.2f, D.wind, {0.8f, 2.0f, 0.9f}); audio::play(S_MONATK, 0, 0.25f); } }
  if (u.slowT > 0) { u.slowT -= h; sp *= u.slowK; } u.x += mx * sp * h; u.z += mz * sp * h; u.moving = sp * (mx != 0 || mz != 0 ? 1.f : 0.f); for (auto& o : foes) if (&o != &f && o.u.used && o.u.hp > 0) { float ex = u.x - o.u.x, ez = u.z - o.u.z, d = std::hypot(ex, ez), r = u.radius + o.u.radius; if (d < r && d > 1e-4f) { u.x += ex / d * (r - d) * 0.5f; u.z += ez / d * (r - d) * 0.5f; } }
  gfx::pushOut(u.x, u.z, u.radius); (void)M;
}
// ---------------------------------------------------------------- bot (free-look showcase + --bot)
float solvePitch(V3 from, V3 to, float speed) {
  float dxz = std::max(1.f, std::hypot(to.x - from.x, to.z - from.z)), p = std::atan2(to.y - from.y, dxz);
  for (int it = 0; it < 5; it++) { V3 q = from, v = fwdOf(std::atan2(to.x - from.x, to.z - from.z), p) * speed; float yh = to.y; for (int i = 0; i < 400; i++) { V3 pq = q; ballistic(q, v, 1.f / 120); if (std::hypot(q.x - from.x, q.z - from.z) >= dxz) { float k = (dxz - std::hypot(pq.x - from.x, pq.z - from.z)) / std::max(1e-4f, std::hypot(q.x - pq.x, q.z - pq.z)); yh = pq.y + (q.y - pq.y) * k; break; } } p += (to.y - yh) / dxz; }
  return p;
}
void botThink(float dt, Input& in) {
  botT += dt; V3 tp{0, 0, 0}; bool have = false; V3 eye = eyePos();
  if (P.style != roster::ST_BOW) {
    Foe* bf = nullptr; float bd = 80; for (auto& f : foes) if (f.u.used && f.u.hp > 0) { float d = std::hypot(f.u.x - P.x, f.u.z - P.z); if (d < bd) { bd = d; bf = &f; } }
    if (!bf) { in.lmb = false; return; } const int st = P.style; float yd = std::atan2(bf->u.x - P.x, bf->u.z - P.z); P.yaw += angDiff(P.yaw, yd) * (1 - std::exp(-9 * dt));
    float ty = terrH(bf->u.x, bf->u.z) + bf->headY * 0.55f; float want = (st == roster::ST_SWORD || st == roster::ST_HEAVY) ? 2.0f : st == roster::ST_ALCH ? 11.f : 13.f; P.pitch += (std::atan2(ty - (P.y + 1.4f), std::max(1.f, bd)) * (st == roster::ST_ALCH ? 1.f : 0.9f) - P.pitch) * (1 - std::exp(-8 * dt));
    if (bd > want + 0.8f) in.mz = 1; else if (bd < want - 4.f && want > 5.f) in.mz = -1;
    if (mode == M_WAVES && bd < 3.5f && std::fmod(botT, 3.1f) < 0.05f) in.roll = true;
    float per = st == roster::ST_SWORD ? 0.34f : st == roster::ST_HEAVY ? 1.7f : 1.5f, hold = st == roster::ST_SWORD ? 0.25f : st == roster::ST_HEAVY ? 1.1f : ((int)(botT / per) % 2) ? 0.9f : 0.12f; float ph = std::fmod(botT, per); in.lmb = ph < hold && bd < want + 5.f && std::fabs(angDiff(P.yaw, yd)) < 0.25f;
    return;
  }
  if (mode == M_WAVES) { float bd = 70; for (auto& f : foes) if (f.u.used && f.u.hp > 0) { float d = std::hypot(f.u.x - P.x, f.u.z - P.z); if (d < bd) { bd = d; float T = d / 40.f; tp = {f.u.x + std::sin(f.u.face) * f.u.moving * T, terrH(f.u.x, f.u.z) + f.headY * 0.7f, f.u.z + std::cos(f.u.face) * f.u.moving * T}; have = true; } } if (bd < 7) in.mz = -1; }
  else { if (!tgts[botTgt].on || botT > 6) { botTgt = (botTgt + 1) % MAXTGT; botT = 0; } for (int k = 0; k < MAXTGT && !tgts[botTgt].on; k++) botTgt = (botTgt + 1) % MAXTGT; const Target& t = tgts[botTgt]; if (t.on) { float d = std::hypot(t.x - P.x, t.z - P.z), x, y, z; tpos(t, tm + d / 42.f, x, y, z); tp = {x, y, z}; have = true; } }
  if (!have) { in.lmb = false; return; }
  float yd = std::atan2(tp.x - P.x, tp.z - P.z), pd = solvePitch(eye, tp, launchSpeed(1.f)); P.yaw += angDiff(P.yaw, yd) * (1 - std::exp(-7 * dt)); P.pitch += (pd - P.pitch) * (1 - std::exp(-7 * dt));
  botWait -= dt; bool aligned = std::fabs(angDiff(P.yaw, yd)) < 0.03f && std::fabs(pd - P.pitch) < 0.03f;
  if (P.drawing) { in.lmb = !(P.draw >= 1.f && P.holdT > 0.1f + 0.1f * R() && aligned); if (!in.lmb) botWait = 0.5f; } else in.lmb = botWait <= 0 && aligned && P.nockT <= 0 && !P.down;
}
// ---------------------------------------------------------------- stepping
void stepPlayer(float h, Input in) {
  P.ox = P.x; P.oz = P.z; if (P.down) { P.downT += h; P.vx = P.vz = 0; P.speed = 0; if (P.downT > 4.2f && !P.over) revive(); return; }
  P.abilCd = std::max(0.f, P.abilCd - h); P.abilT -= h; if (!P.down && P.regen > 0) P.hp = std::min(P.maxHp, P.hp + P.regen * h); if (P.abilT > 0 && roster::at(roster::sel()).abil == roster::AB_TONIC) P.hp = std::min(P.maxHp, P.hp + 10.f * h);
  if (P.quiver < P.quiverMax) { P.quiverT += h; if (P.quiverT >= 1.1f) { P.quiverT = 0; P.quiver++; } }
  P.relT += h; P.iT -= h; P.nockT -= h; P.hurtT -= h; P.stagT -= h; P.hitK *= std::exp(-4 * h); P.blockK *= std::exp(-6 * h); P.vig *= std::exp(-2.2f * h);
  if (P.stWait > 0) P.stWait -= h; else P.st = std::min(100.f, P.st + 26 * h);
  bool rolling = P.rollT > 0, free_ = H.view == VW_FREE; float yaw = P.yaw;
  bool wantBlock = in.rmb && !rolling && P.st > 6 && P.stagT <= 0 && !P.atkOn && !P.charging && !P.drawing; if (wantBlock) P.blockT += h; else P.blockT = 0; P.blocking += ((wantBlock ? 1.f : 0.f) - P.blocking) * (1 - std::exp(-14 * h));
  if (in.roll && !rolling && P.st >= 20 && P.stagT <= 0) { V3 f = fwdOf(yaw, 0), r = rightOf(yaw); float dx = r.x * in.mx + f.x * in.mz, dz = r.z * in.mx + f.z * in.mz; if (std::fabs(in.mx) + std::fabs(in.mz) < 0.1f) { dx = -f.x; dz = -f.z; } float l = std::hypot(dx, dz); P.rollDx = dx / l; P.rollDz = dz / l; P.rollT = 1e-4f; P.iT = 0.42f; P.st -= 20; P.stWait = 0.7f; P.drawing = false; P.draw = 0; P.holdT = 0; audio::play(S_SWING, 0, 0.35f); V3 c{0.25f, 0.2f, 0.14f}; fx.dust(P.x, P.y + 0.1f, P.z, 6, &c, 0.4f, 0.6f); rolling = true; }
  if (rolling) { P.rollT += h; float u = P.rollT / ROLL_T, sp = 9.5f * (1 - u * u); P.vx = P.rollDx * sp; P.vz = P.rollDz * sp; if (P.rollT >= ROLL_T) { P.rollT = 0; } }
  else if (P.dashT > 0) { P.dashT -= h; P.vx = P.dashX * 26.f; P.vz = P.dashZ * 26.f; }
  else { V3 f = fwdOf(yaw, 0), r = rightOf(yaw); float mx = (free_ && P.style == roster::ST_BOW) ? 0 : in.mx, mz = (free_ && P.style == roster::ST_BOW) ? 0 : in.mz, l = std::hypot(mx, mz); if (l > 1) { mx /= l; mz /= l; }
    float top = P.mvMul * (in.sprint && !P.drawing ? 6.4f : 4.3f) * (P.drawing ? 0.5f : 1.f) * ((P.atkOn || P.charging) ? (P.style == roster::ST_HEAVY ? 0.4f : 0.55f) : 1.f) * (P.blocking > 0.5f ? 0.55f : 1.f) * (P.stagT > 0 ? 0.3f : 1.f); float tx = (r.x * mx + f.x * mz) * top, tz = (r.z * mx + f.z * mz) * top, k = 1 - std::exp(-(l > 0.05f ? 11.f : 14.f) * h); P.vx += (tx - P.vx) * k; P.vz += (tz - P.vz) * k; }
  P.x += P.vx * h; P.z += P.vz * h; gfx::pushOut(P.x, P.z, 0.38f); float ccx = ax0 + 22, ccz = pathZ(ccx), ddx = P.x - ccx, ddz = P.z - ccz, dd = std::hypot(ddx, ddz); if (dd > ARENA) { P.x = ccx + ddx / dd * ARENA; P.z = ccz + ddz / dd * ARENA; }
  P.y = terrH(P.x, P.z); P.speed = std::hypot(P.vx, P.vz); P.bob += P.speed * h * 1.7f;
  // face: toward the camera yaw while aiming / in first person; toward motion otherwise
  float fy = P.yaw; if (H.view == VW_THIRD && !P.drawing && P.blocking < 0.3f && P.speed > 0.8f && !rolling) fy = std::atan2(P.vx, P.vz); if (rolling) fy = std::atan2(P.rollDx, P.rollDz); P.face += angDiff(P.face, fy) * (1 - std::exp(-12 * h));
  // draw / release
  bool canDraw = P.style == roster::ST_BOW && P.quiver > 0 && P.stagT <= 0 && !rolling && P.blocking < 0.3f && P.nockT <= 0 && !(in.sprint && P.speed > 5.f);
  if (dbgDraw >= 0 && P.style == roster::ST_BOW) { P.drawing = true; P.draw = dbgDraw; P.holdT = 0; }
  else if (in.lmb && canDraw) { if (!P.drawing) { P.drawing = true; P.draw = 0; P.holdT = 0; audio::play(S_CLICK, 0, 0.3f); } P.draw = std::min(1.f, P.draw + h * P.dMul * (P.abilT > 0 && roster::at(roster::sel()).abil == roster::AB_VOLLEY ? 1.8f : 1.f) / DRAW_T); if (P.draw >= 1) { P.holdT += h; if (P.holdT > 0.9f) P.st = std::max(0.f, P.st - 7 * h); } }
  else if (P.drawing) { if (P.draw > 0.14f && canDraw && P.st > 0) loose(); else { P.drawing = false; P.draw = 0; P.holdT = 0; } if (P.st <= 0 && P.drawing) { P.drawing = false; } }
  if (P.drawing && P.st <= 0.01f && P.holdT > 0.9f && dbgDraw < 0) loose();
  combatStep(h, in);
  // sway: gentle noise that grows once the draw is held past the sweet window
  float amp = 0; if (P.drawing) { float over = P.holdT > 0.9f ? std::min(3.f, (P.holdT - 0.9f) / 0.7f) : 0.f; amp = (0.0010f + 0.0025f * (1 - P.draw) + 0.0045f * over + P.speed * 0.00045f) / P.stab; }
  float tx_ = amp * nz(tm * 1.9f, 0.3f) * 2, ty_ = amp * nz(tm * 2.3f, 4.1f) * 2; float k = 1 - std::exp(-10 * h); P.swayX += (tx_ - P.swayX) * k; P.swayY += (ty_ - P.swayY) * k;
  P.kickP *= std::exp(-7 * h); P.kickY *= std::exp(-7 * h); lagX *= std::exp(-6 * h); lagY *= std::exp(-6 * h);
  float cv = curve(P.draw); H.sweet = (P.drawing && P.draw >= SWEET0 && P.holdT < 0.9f) ? 1.f : 0.f; float spr = lerpf(18.f, 2.5f, P.drawing ? cv : 0.f) + P.speed * 0.9f + (P.holdT > 0.9f ? 2.5f * std::min(1.f, (P.holdT - 0.9f) / 1.f) * (0.6f + 0.4f * std::sin(tm * 17)) : 0.f); H.spread += (spr - H.spread) * (1 - std::exp(-16 * h));
  P.powerShown = cv;
  playerUnit.x = P.x; playerUnit.z = P.z;
}
void stepWorld(float h) {
  for (auto& t : tgts) if (t.on) { tpos(t, tm, t.x, t.y, t.z); t.hitT = std::max(0.f, t.hitT - h); t.tilt *= std::exp(-4 * h); }
  for (auto& a : arrows) if (a.on) stepArrow(a, h);
  stepZones(h);
  if (mode == M_RANGE) { dumT -= h; if (dumT <= 0) { dumT = 1.0f; for (int s = 0; s < 4; s++) { bool have = false; for (auto& f : foes) if (f.u.used && f.ai == 1 && (int)f.cd == s) { have = true; break; } if (!have) { spawnDummy(s); break; } } } }
  for (auto& f : foes) stepFoe(f, h);
  if (mode == M_WAVES && !P.over) { if (waveState == 1 && liveFoes() == 0) { waveState = 0; waveT = 4.5f; char b[40]; snprintf(b, 40, "WAVE %d CLEARED", H.wave); banner(b, "CATCH YOUR BREATH"); feed(b, 0x9fe8a0); P.hp = std::min(P.maxHp, P.hp + 25); P.st = 100; setScore(50 * H.wave); }
    else if (waveState == 0) { waveT -= h; if (waveT <= 0) startWave(); } }
}
void aoe(float r, float dmg, float stag, float kb) {
  for (auto& f : foes) { Unit& u = f.u; if (!u.used || u.hp <= 0) continue; float dx = u.x - P.x, dz = u.z - P.z, d = std::max(0.3f, std::hypot(dx, dz)); if (d > r + u.radius) continue;
    u.hp -= dmg * (1 - MON[u.cls].armor); u.flashV = 1; u.stagV = 1; u.stagT = std::max(u.stagT, stag); f.kx = dx / d * kb; f.kz = dz / d * kb; if (u.act.on && !u.boss) { u.act.on = false; f.shot = false; }
    char b[16]; snprintf(b, 16, "%d", (int)dmg); fx.floatText(u.x, terrH(u.x, u.z) + f.headY + 0.4f, u.z, b, 0xffc070, 13, 0.8f); if (u.hp <= 0) killFoe(f, false, d); }
}
// hero-select pedestal preview: turntable + idle -> draw -> release -> victory loop
void selectUpdate(float dt) {
  float t0 = std::fmod(selT, 8.f); selT += dt; float t = std::fmod(selT, 8.f); P.face = -PI / 2 + selT * 0.5f; P.yaw = -PI / 2; P.relT += dt;
  P.drawing = t >= 2.2f && t < 3.9f; P.draw = P.drawing ? std::min(1.f, (t - 2.2f) / 1.1f) : 0.f; P.holdT = 0; if (t0 < 3.9f && t >= 3.9f) P.relT = 0;
  float vt = (t >= 4.7f && t < 7.3f) ? 1.f : 0.f; P.vic += (vt - P.vic) * (1 - std::exp(-6 * dt));
}
}  // namespace
void heroStep(int d) { roster::step(d); selT = 0; P.vic = 0; P.relT = 9; P.drawing = false; }
void setBiome(int b) { gBiome = ((b % 3) + 3) % 3; static const int BR[3] = {4, 3, 1}; if (mode != M_OFF) gfx::setReach(BR[gBiome], true); }
int biome() { return gBiome; }
void useAbility() {
  if (mode == M_OFF || mode == M_SELECT || P.down || P.abilCd > 0) return; const roster::Hero& hr = roster::at(roster::sel()); P.abilCd = hr.cd; P.abilT = hr.dur; V3 f = fwdOf(P.yaw, 0);
  char b[48]; snprintf(b, 48, "%s", hr.active); feed(b, 0xffd27a); fx.floatText(P.x, P.y + 2.4f, P.z, hr.active, 0xffd27a, 12, 1.0f); audio::play(S_FLARE, 0, 0.5f);
  switch (hr.abil) {
    case roster::AB_VOLLEY: fx.ring(P.x, P.z, 0.3f, 2.5f, 0.9f, {2.f, 1.6f, 0.6f}); break;
    case roster::AB_BULWARK: fx.ring(P.x, P.z, 0.3f, 4.f, 0.5f, {1.4f, 1.6f, 2.2f}); aoe(3.2f, 8, 0.5f, 7.f); P.blockK = 1; break;
    case roster::AB_SURGE: { P.dashT = 0.3f; P.dashX = f.x; P.dashZ = f.z; P.iT = std::max(P.iT, 0.5f); V3 c{0.3f, 0.25f, 0.2f}; fx.dust(P.x, P.y + 0.2f, P.z, 8, &c, 0.4f, 0.6f);
      for (auto& fo : foes) { Unit& u = fo.u; if (!u.used || u.hp <= 0) continue; float dx = u.x - P.x, dz = u.z - P.z, al = dx * f.x + dz * f.z, pe = std::fabs(dx * f.z - dz * f.x); if (al > 0 && al < 9.f && pe < 1.6f + u.radius) { u.hp -= 34.f * P.dmgMul; u.flashV = 1; u.stagT = std::max(u.stagT, 0.5f); u.stagV = 1; fo.kx = f.x * 7; fo.kz = f.z * 7; if (u.hp <= 0) killFoe(fo, false, al); } } break; }
    case roster::AB_EMBER: fx.ring(P.x, P.z, 0.3f, 7.f, 0.8f, {3.f, 1.3f, 0.3f}); fx.flash(P.x, P.y + 1, P.z, {1, 0.6f, 0.2f}, 90, 0.35f); aoe(7.f, 40.f * P.dmgMul, 0.35f, 4.f); break;
    case roster::AB_TONIC: P.hp = std::min(P.maxHp, P.hp + 40.f); P.st = 100; fx.ring(P.x, P.z, 0.3f, 2.5f, 0.8f, {0.7f, 1.8f, 0.8f}); break;
    case roster::AB_FROST: fx.ring(P.x, P.z, 0.3f, 10.f, 0.9f, {0.7f, 1.3f, 2.2f}); for (auto& fo : foes) { Unit& u = fo.u; if (u.used && u.hp > 0 && std::hypot(u.x - P.x, u.z - P.z) < 10.f + u.radius) { u.slowT = 4.f; u.slowK = 0.4f; u.chillT = 4.f; u.stagT = std::max(u.stagT, 0.6f); u.stagV = 1; } } break;
    case roster::AB_QUAKE: fx.ring(P.x, P.z, 0.3f, 8.f, 0.7f, {1.8f, 1.2f, 0.6f}); aoe(8.f, 30.f * P.dmgMul, 0.9f, 8.f); H.shake = std::max(H.shake, 0.6f); break; }
}
// ================================================================ public API
float curve(float p) { p = clampf(p, 0, 1); return 1 - (1 - p) * (1 - p) * (1 - 0.35f * p); }   // fast early pull, flattens into the sweet spot
bool active() { return mode != M_OFF; }
bool freeDrag() { return H.view == VW_FREE; }
float timeScale() { return slowK; }
V3 aimDir() { return camDirRaw(); }
V3 lanternPos() { V3 f = fwdOf(P.face, 0); return {P.x - f.x * 0.25f, P.y + 2.0f, P.z - f.z * 0.25f}; }
void debugAim(float yaw, float pitch) { P.yaw = yaw; P.pitch = pitch; }
void debugDraw(float d) { dbgDraw = d; }
void debugBot(bool on) { H.bot = on; }
void debugBestiary() {
  for (auto& f : foes) f = Foe(); for (int i = 0; i < NMON; i++) { float row = i < 6 ? 8.5f : 14.5f, a = P.yaw + ((i % 6) - 2.5f) * 0.33f; float x = P.x + std::sin(a) * row, z = P.z + std::cos(a) * row; Foe* f = freeFoe(); if (!f) break; spawnFoe(i, x, z); f->ai = 1; f->u.speed = 0; f->hx = x; f->hz = z; f->u.maxHp = f->u.hp = 9999; f->cd = 99; }
  H.bannerT = 0;
}
void setView(int v) { v = ((v % NVIEW) + NVIEW) % NVIEW; if (cam.init) { cam.oldEye = cam.eye; cam.oldTgt = cam.tgt; cam.oldFov = cam.fov; cam.blend = 0; } H.view = v; cam.sm = cam.eye; if (v == VW_FREE) { cam.orbitYaw = P.yaw + 2.6f; cam.orbitPitch = 0.22f; cam.orbitD = 5.5f; cam.pivot = {P.x, P.y + 1.2f, P.z}; cam.idle = 0; } char b[40]; snprintf(b, 40, "VIEW: %s", v == VW_FIRST ? "FIRST PERSON" : v == VW_THIRD ? "THIRD PERSON" : "FREE LOOK - SHOWCASE"); feed(b, 0xb3a88e); }
void start(int m) {
  mode = m; tm = 0; hitStop = slowMo = 0; slowK = 1; dbgDraw = -1; waveState = 0; waveT = 2.f; H = Hud(); H.mode = m; H.view = roster::at(roster::sel()).style == roster::ST_BOW ? VW_FIRST : VW_THIRD; H.assist = cfg::S.assist; snprintf(H.scoreS, 16, "0"); setAcc(); setCombo(); H.combo = 0; setCombo(); P = Player(); cam = CamState(); roster::init();
  { const roster::Hero& hr = roster::at(roster::sel()); P.maxHp = P.hp = hr.hp; P.quiver = P.quiverMax = hr.quiver; P.dMul = hr.draw; P.stab = hr.stab * (hr.pass == roster::PS_STEADY ? 1.5f : hr.pass == roster::PS_TITAN ? 1.3f : 1.f); P.dmgMul = hr.dmg; P.mvMul = hr.move * (hr.pass == roster::PS_BLOOD ? 1.12f : 1.f); P.armor = hr.pass == roster::PS_STEEL ? 0.15f : 0.f; P.regen = hr.pass == roster::PS_MEDIC ? 1.5f : 0.f; P.abilMax = hr.cd; P.style = hr.style; P.atkOn = P.charging = false; P.atkT = P.chargeT = P.comboT = P.ripT = P.blockT = 0; P.atkCombo = 0; P.lmbPrev = false; }
  for (auto& f : foes) f = Foe(); for (auto& z : zones) z = Zone(); dumT = 0.2f; for (auto& a : arrows) a = Arrow(); for (auto& t : tgts) t = Target(); botTgt = 0; botT = 0; uid = 5000 + (uint32_t)(R() * 1000);
  P.x = P.ox = ax0; P.z = P.oz = pathZ(ax0); P.y = terrH(P.x, P.z); P.yaw = P.face = PI / 2; P.pitch = 0.0f; playerUnit = Unit(); playerUnit.used = true; playerUnit.team = 0; playerUnit.cls = FLETCHER; fx.clear();
  if (m == M_RANGE) { buildRange(); banner("PRACTICE RANGE", roster::styleTip(roster::at(roster::sel()).style)); feed("Boards, movers & live training dummies", 0xb3a88e); } else if (m == M_WAVES) { P.lives = 3; banner("WAVE HUNT", roster::styleTip(roster::at(roster::sel()).style)); H.wave = 0; waveT = 3.f; }
  if (m == M_SELECT) { H.view = VW_THIRD; P.face = P.yaw = -PI / 2; selT = 0; P.relT = 9; H.bannerT = 0; }
  gfx::resetCamera(); { static const int BR[3] = {4, 3, 1}; gfx::setReach(BR[gBiome % 3], true); }
}
void stop() { mode = M_OFF; slowK = 1; gfx::camO.on = false; }
void look(float dx, float dy) {
  if (mode == M_OFF || mode == M_SELECT) return;
  if (H.view == VW_FREE) { cam.orbitYaw -= dx * 0.0045f; cam.orbitPitch = clampf(cam.orbitPitch + dy * 0.0045f, -0.2f, 1.45f); cam.idle = 0; return; }
  if (P.down) return; float s = 0.0021f * cfg::S.sens * (P.drawing ? 0.62f : 1.f) * (P.blocking > 0.5f ? 0.8f : 1.f); if (cfg::S.invertY) dy = -dy; P.yaw -= dx * s; P.pitch = clampf(P.pitch - dy * s, -1.3f, 1.3f); lagX += dx * 0.0009f; lagY += dy * 0.0009f; lagX = clampf(lagX, -0.06f, 0.06f); lagY = clampf(lagY, -0.06f, 0.06f);
}
void scroll(float dy) { if (mode == M_SELECT) { heroStep(dy > 0 ? -1 : 1); return; } if (H.view == VW_FREE) cam.orbitD = clampf(cam.orbitD - dy * 0.8f, 2.2f, 45.f); else if (H.view == VW_THIRD) cam.dist = clampf(cam.dist - dy * 0.3f, 2.0f, 6.f); }
void keyPress(int k) {
  if (mode == M_OFF) return;
  if (k == 'T' || (mode == M_SELECT && k >= '1' && k <= '3')) { if (k == 'T') setBiome(gBiome + 1); else setBiome(k - '1'); static const char* BN[3] = {"ASHEN MOOR", "PINE HIGHLANDS", "DROWNED FEN"}; if (mode != M_SELECT) feed(BN[gBiome], 0xb3a88e); return; }
  if (mode == M_SELECT) { if (k == 263 || k == 'A') heroStep(-1); else if (k == 262 || k == 'D') heroStep(1); else if (k == 13 || k == 32) stop(); return; }
  if (k == 'E') { useAbility(); return; }
  if (k == cfg::S.key[cfg::ACT_VIEW]) setView(H.view + 1); else if (k == cfg::S.key[cfg::ACT_ASSIST]) { H.assist = !H.assist; feed(H.assist ? "Arc assist ON" : "Arc assist OFF", 0xb3a88e); } else if (k == 'B') { H.bot = !H.bot; feed(H.bot ? "Auto-archer ON" : "Auto-archer OFF", 0xb3a88e); }
  else if (k == 'R') start(mode); else if (k == 13 && H.ended) stop();
}
void update(float dtReal, const Input& in0) {
  if (mode == M_OFF) return; dtReal = std::min(dtReal, 0.05f); tm += dtReal; if (mode == M_SELECT) { selectUpdate(dtReal); return; } float ts = 1;
  if (hitStop > 0) { hitStop -= dtReal; ts = 0.03f; } else if (slowMo > 0) { slowMo -= dtReal; ts = lerpf(1.f, 0.3f, std::min(1.f, slowMo / 0.3f)); if (slowMo < 0.3f) ts = lerpf(1.f, 0.3f, slowMo / 0.3f); } slowK = ts;
  Input in = in0; bool botOn = H.bot || H.view == VW_FREE; if (botOn) { in.lmb = in.rmb = in.roll = false; botThink(dtReal, in); }
  float dt = dtReal * ts; int n = std::max(1, (int)std::ceil(dt / (1.f / 120))); float h = dt / n; for (int i = 0; i < n; i++) { stepPlayer(h, in); stepWorld(h); in.roll = false; }
  H.shake *= std::exp(-5 * dtReal); H.zoomK *= std::exp(-3.2f * dtReal); P.fovK *= std::exp(-3.f * dtReal); P.hmT = std::max(0.f, P.hmT - dtReal); H.bannerT = std::max(0.f, H.bannerT - dtReal); H.comboT = std::max(0.f, H.comboT - dtReal);
  for (auto& f : H.feed) f.t = std::max(0.f, f.t - dtReal); if (mode == M_WAVES) H.foesLeft = liveFoes(); if (H.combo > 0 && mode == M_RANGE && H.comboT <= 0 && P.relT > 5) { /* combo persists until a miss */ }
  // trajectory preview (same integrator as the arrows)
  nDots = 0; landOk = false;
  if (H.assist && P.drawing && P.draw > 0.12f && !P.down) {
    V3 from; V3 dir = shotDirBase(from); V3 p = from, v = dir * launchSpeed(curve(P.draw)); const float sh = 1.f / 40; Foe* dummy = nullptr; (void)dummy;
    for (int i = 0; i < 160 && !landOk; i++) { V3 p0 = p; ballistic(p, v, sh); float best = 2; float tw; if (gfx::rayWorld(p0.x, p0.y, p0.z, p.x, p.y, p.z, tw)) best = tw; float dz, dy, yr;
      for (auto& t : tgts) if (t.on) { float tt = hitTarget(t, p0, p, dz, dy); if (tt < best) best = tt; } for (auto& f : foes) if (f.u.used && f.u.hp > 0) { float tt = hitFoe(f, p0, p, yr); if (tt < best) best = tt; }
      if (best <= 1) { landPt = p0 + (p - p0) * best; landOk = true; } else if (i % 3 == 2 && nDots < 40) dots[nDots++] = p; }
  }
}
// ================================================================ camera
void camera(float dt) {
  if (mode == M_OFF) return; dt = std::min(dt, 0.05f); V3 eye, tgt; float fov; float kick = 0;
  float dk = P.drawing ? curve(P.draw) : 0.f; (void)kick;
  if (mode == M_SELECT) { const FigAsset* fa = roster::cur(); float sc = fa->scale; eye = {P.x - (3.4f + 1.5f * sc), P.y + 0.3f + 1.25f * sc, P.z + 0.9f}; tgt = {P.x, P.y + 0.3f + 1.0f * sc, P.z}; fov = 0.62f; }
  else if (H.view == VW_FIRST) {
    V3 d = camDirRaw(); float yaw = P.yaw + P.swayX + P.kickY + (P.hitDir > 0 ? 1.f : -1.f) * P.hitK * 0.02f, pitch = P.pitch + P.swayY + P.kickP; d = fwdOf(yaw, pitch); eye = eyePos(); if (P.down) { eye.y -= 0.0f; d = fwdOf(yaw, std::max(-0.9f, pitch - 0.9f * std::min(1.f, P.downT * 2))); }
    tgt = eye + d * 20.f; fov = cfg::S.fov * (PI / 180.f) - 0.22f * dk - 0.06f * H.zoomK + (P.speed > 5.5f ? 0.04f * (cfg::S.reduceMotion ? 0.f : 1.f) : 0.f);
    float sh = H.shake * 0.01f * cfg::effShake(); eye.x += (R() - 0.5f) * sh; eye.y += (R() - 0.5f) * sh; eye.z += (R() - 0.5f) * sh;
  } else if (H.view == VW_THIRD) {
    V3 r = rightOf(P.yaw), d = fwdOf(P.yaw + P.swayX + P.kickY, P.pitch + P.swayY + P.kickP); float aim = P.drawing ? dk : 0.f; float shoulder = lerpf(0.55f, 0.85f, aim) + (P.blocking > 0.5f ? 0.1f : 0.f); float D = lerpf(cam.dist, cam.dist * 0.62f, aim);
    float ph = 1.55f - (P.rollT > 0 ? 0.5f * std::sin(std::min(1.f, P.rollT / ROLL_T) * PI) : 0.f) - (P.down ? 0.9f : 0.f); V3 piv{P.x + r.x * 0.1f, P.y + ph, P.z + r.z * 0.1f}; V3 des = piv + r * shoulder - d * D;
    if (!cam.init) cam.sm = des; float k = 1 - std::exp(-17 * dt); cam.sm = cam.sm + (des - cam.sm) * k;   // spring-damped follow
    V3 tp = piv + r * shoulder * 0.9f; V3 seg = cam.sm - tp; float L = len(seg); float t; V3 ps = piv + r * (shoulder * 0.9f);
    if (L > 0.01f && gfx::rayWorld(ps.x, ps.y, ps.z, cam.sm.x, cam.sm.y, cam.sm.z, t)) { float nl = std::max(0.35f, t * L - 0.3f); cam.sm = ps + seg * (nl / L); }
    float gy = terrH(cam.sm.x, cam.sm.z) + 0.35f; if (cam.sm.y < gy) cam.sm.y = gy; eye = cam.sm; tgt = eye + fwdOf(P.yaw + P.swayX + P.kickY, P.pitch + P.swayY + P.kickP) * 25.f; fov = (0.95f * cfg::S.fov / 70.f) - 0.17f * dk - 0.05f * H.zoomK;
    float sh = H.shake * 0.012f * cfg::effShake(); eye.x += (R() - 0.5f) * sh; eye.y += (R() - 0.5f) * sh;
  } else {
    cam.idle += dt; if (cam.idle > 3.5f) cam.orbitYaw += 0.18f * dt; V3 want{P.x, P.y + 1.15f, P.z}; cam.pivot = cam.pivot + (want - cam.pivot) * (1 - std::exp(-4 * dt));
    float cp = std::cos(cam.orbitPitch); V3 off{std::sin(cam.orbitYaw) * cp, std::sin(cam.orbitPitch), std::cos(cam.orbitYaw) * cp}; eye = cam.pivot + off * cam.orbitD; float t; V3 pv = cam.pivot; if (gfx::rayWorld(pv.x, pv.y, pv.z, eye.x, eye.y, eye.z, t)) eye = pv + (eye - pv) * std::max(0.1f, t - 0.04f);
    float gy = terrH(eye.x, eye.z) + 0.4f; if (eye.y < gy) eye.y = gy; tgt = cam.pivot + V3{0, 0.1f, 0}; fov = 0.78f;
  }
  if (!cam.init) { cam.init = true; cam.blend = 1; }
  cam.eye = eye; cam.tgt = tgt; cam.fov = fov; if (cam.blend < 1) cam.blend = std::min(1.f, cam.blend + dt / 0.22f);
  float e = smooth(0, 1, cam.blend); gfx::CamO& c = gfx::camO; c.on = true; c.eye = mix3(cam.oldEye, eye, e); c.tgt = mix3(cam.oldTgt, tgt, e); c.fov = lerpf(cam.oldFov, fov, e); c.nearP = 0.1f;
  c.focus = len(c.eye - c.tgt); if (H.view == VW_FIRST) { c.focus = 12; c.ap = 0.f; } else if (H.view == VW_THIRD) { c.focus = len(eye - V3{P.x, P.y + 1.4f, P.z}); c.ap = 0.5f; } else { c.focus = len(eye - cam.pivot); c.ap = 0.7f; }
  if (cam.blend >= 1) { cam.oldEye = eye; cam.oldTgt = tgt; cam.oldFov = fov; }
}
// ================================================================ instanced primitive batches (arrows, targets, trajectory, first-person bow)
namespace {
using gfx::shW; using gfx::shV; using gfx::shWn; using gfx::shVn;
void put(Inst (*arr)[gfx::SHCAP], int* cnt, int sh, const M4& m, V3 col, float ro, float me, float gl) { if (cnt[sh] >= gfx::SHCAP) return; Inst& i = arr[sh][cnt[sh]++]; memcpy(i.m, m.m, 64); i.tint[0] = col.x; i.tint[1] = col.y; i.tint[2] = col.z; i.tint[3] = 0; i.mat[0] = ro; i.mat[1] = me; i.mat[2] = gl; i.mat[3] = 0; }
void putV(int sh, const M4& m, V3 col, float ro, float me, float gl) { if (shVn[sh] >= 160) return; Inst& i = shV[sh][shVn[sh]++]; memcpy(i.m, m.m, 64); i.tint[0] = col.x * 0.17f; i.tint[1] = col.y * 0.17f; i.tint[2] = col.z * 0.17f; i.tint[3] = 0; i.mat[0] = ro; i.mat[1] = std::min(me, 0.3f); i.mat[2] = gl * 0.4f; i.mat[3] = 0; }
M4 seg(V3 a, V3 b, float rx, float rz) { V3 d = b - a; float l = std::max(1e-5f, len(d)); V3 y = d * (1 / l), up = std::fabs(y.y) < 0.95f ? V3{0, 1, 0} : V3{1, 0, 0}; V3 x = norm(cross(up, y)), z = cross(x, y); V3 c = (a + b) * 0.5f; M4 m = ident(); m.m[0] = x.x * rx; m.m[1] = x.y * rx; m.m[2] = x.z * rx; m.m[4] = d.x; m.m[5] = d.y; m.m[6] = d.z; m.m[8] = z.x * rz; m.m[9] = z.y * rz; m.m[10] = z.z * rz; m.m[12] = c.x; m.m[13] = c.y; m.m[14] = c.z; return m; }
M4 ball(V3 c, float r) { return mul(translate(c.x, c.y, c.z), scale4(r, r, r)); }
void wCyl(V3 a, V3 b, float r, V3 col, float ro = 0.7f, float me = 0, float gl = 0) { put(shW, shWn, SH_CYL, seg(a, b, r, r), col, ro, me, gl); }
void arrowGeo(V3 tip, V3 d, float len_, bool view) {
  V3 tail = tip - d * len_; V3 shaft = C(0xcfa56a), head = C(0x9aa0a8), fl = C(0xd8d0b8), fl2 = C(0xb04a3a);
  M4 ms = seg(tail, tip - d * 0.05f, 0.0075f, 0.0075f); M4 mh = seg(tip - d * 0.07f, tip + d * 0.02f, 0.016f, 0.016f); V3 up = std::fabs(d.y) < 0.95f ? V3{0, 1, 0} : V3{1, 0, 0}; V3 s1 = norm(cross(up, d)), s2 = cross(s1, d); V3 f0 = tail + d * 0.06f, f1 = tail + d * 0.2f;
  M4 b1 = seg(f0 + s1 * 0.035f, f1 + s1 * 0.01f, 0.003f, 0.02f), b2 = seg(f0 - s1 * 0.035f, f1 - s1 * 0.01f, 0.003f, 0.02f), b3 = seg(f0 + s2 * 0.035f, f1 + s2 * 0.01f, 0.003f, 0.02f);
  if (view) { putV(SH_CYL, ms, shaft, 0.6f, 0, 0); putV(SH_CONE, mh, head, 0.4f, 0.8f, 0); putV(SH_BOX, b1, fl, 0.9f, 0, 0); putV(SH_BOX, b2, fl2, 0.9f, 0, 0); putV(SH_BOX, b3, fl, 0.9f, 0, 0); }
  else { put(shW, shWn, SH_CYL, ms, shaft, 0.6f, 0, 0); put(shW, shWn, SH_CONE, mh, head, 0.4f, 0.8f, 0); put(shW, shWn, SH_BOX, b1, fl, 0.9f, 0, 0); put(shW, shWn, SH_BOX, b2, fl2, 0.9f, 0, 0); put(shW, shWn, SH_BOX, b3, fl, 0.9f, 0, 0); }
}
}  // namespace
namespace {
void spellGeo(const Arrow& a) {
  V3 c{a.x, a.y, a.z}, d = norm(V3{a.vx, a.vy, a.vz});
  switch (a.kind) {
    case 10: put(shW, shWn, SH_SPHERE, ball(c, 0.17f), C(0xff8a2a), 0.4f, 0, 7.f); put(shW, shWn, SH_SPHERE, ball(c - d * 0.24f, 0.11f), C(0xffc060), 0.4f, 0, 5.f); put(shW, shWn, SH_SPHERE, ball(c - d * 0.44f, 0.07f), C(0xff5a20), 0.4f, 0, 4.f); break;
    case 11: { float r = 0.18f + 0.03f * a.rad; put(shW, shWn, SH_SPHERE, ball(c, r), C(0xffd070), 0.4f, 0, 9.f); put(shW, shWn, SH_SPHERE, ball(c, r * 1.55f), C(0xff5a18), 0.5f, 0, 4.f); put(shW, shWn, SH_SPHERE, ball(c - d * 0.5f, r * 0.8f), C(0xff7a28), 0.5f, 0, 4.f); break; }
    case 12: case 13: { float r = a.kind == 13 ? 0.16f : 0.11f; put(shW, shWn, SH_SPHERE, ball(c, r), C(0x7aff8a), 0.3f, 0, 3.f); wCyl(c + V3{0, r * 0.6f, 0}, c + V3{0, r * 1.7f, 0}, r * 0.35f, C(0x8a6a3a), 0.6f); break; }
    case 14: wCyl(c - d * 0.55f, c + d * 0.1f, 0.03f, C(0xbfe8ff), 0.3f, 0, 6.f); put(shW, shWn, SH_CONE, seg(c + d * 0.05f, c + d * 0.3f, 0.04f, 0.04f), C(0xe8f8ff), 0.3f, 0, 7.f); break;
    default: wCyl(c - d * 1.6f, c + d * 0.2f, 0.065f, C(0x9fdcff), 0.3f, 0, 7.f); wCyl(c - d * 1.2f, c + d * 0.3f, 0.025f, C(0xffffff), 0.3f, 0, 9.f); put(shW, shWn, SH_CONE, seg(c + d * 0.15f, c + d * 0.7f, 0.08f, 0.08f), C(0xe8f8ff), 0.3f, 0, 8.f);
  }
}
void fpvOther() {
  M4 cw = camWorld(gfx::camO.eye, gfx::camO.tgt); V3 rt{cw.m[0], cw.m[1], cw.m[2]}, up{cw.m[4], cw.m[5], cw.m[6]}, bk{cw.m[8], cw.m[9], cw.m[10]}, eye = gfx::camO.eye, fw = bk * -1.f;
  auto W = [&](float x, float y, float z) { return eye + rt * x + up * y + bk * z; };
  const roster::Hero& hr = roster::at(roster::sel()); const int st = P.style; const float mo = cfg::S.reduceMotion ? 0.2f : 1.f;
  int wR = hr.wpR, wL = hr.wpL; if (wR < 0 && wL < 0) { static const int DR[6] = {-1, 0, 1, 5, 5, 5}, DL[6] = {-1, 3, -1, 7, 7, -1}; wR = DR[st % 6]; wL = DL[st % 6]; }
  { auto off = [](int w) { return w == 3 || w == 4 || w == 7; }; if (off(wR) && wL >= 0 && !off(wL)) { int t = wR; wR = wL; wL = t; } if (wR < 0 && wL >= 0 && !off(wL)) { wR = wL; wL = -1; } }
  float cfull = st == roster::ST_FIRE ? 0.9f : st == roster::ST_ALCH ? 0.8f : 1.f, wind = 0, strike = 0, rec = 0;
  if (P.charging) wind = clampf(P.chargeT / cfull, 0, 1); else if (P.atkOn) { if (!P.atkDone) wind = smooth(0, 1, P.atkT / std::max(0.05f, P.atkWind)); else { wind = 1; strike = smooth(0, 0.14f, P.atkT - P.atkWind); rec = smooth(0.16f, 0.16f + std::max(0.1f, P.atkRec - 0.16f), P.atkT - P.atkWind); } }
  auto ang = [&](float idle, float wp, float sp) { return lerpf(lerpf(lerpf(idle, wp, wind), sp, strike), idle, rec); };
  float mv = std::min(1.f, P.speed / 4.f), bob = std::sin(P.bob) * 0.012f * mv * mo, sway = std::sin(tm * 1.3f) * 0.006f * mo, blk = P.blocking;
  V3 G, D; float sc = 0.9f;
  if (st == roster::ST_SWORD) { float sg = (P.atkIdx & 1) ? -1.f : 1.f, ph = ang(0.5f, 1.15f * sg, -1.0f * sg); G = W(0.26f + 0.09f * std::sin(ph), -0.30f + bob + sway, -0.58f - 0.10f * strike * (1 - rec)); D = norm(fw * 0.5f + (rt * std::sin(ph) + up * std::cos(ph)) * 0.85f); }
  else if (st == roster::ST_HEAVY) { float th = ang(0.95f, 2.3f, -0.5f); G = W(0.12f, -0.42f + 0.1f * std::sin(th) + bob, -0.62f); D = norm(fw * std::cos(th) + up * std::sin(th) + rt * 0.12f); }
  else { float th = ang(1.2f, 1.5f, 0.55f); G = W(0.28f, -0.34f + bob + sway, -0.55f - 0.18f * strike * (1 - rec) + 0.08f * wind); D = norm(fw * std::cos(th) + up * std::sin(th) + rt * 0.05f); }
  if (blk > 0.01f) { V3 gb = W(0.02f, -0.2f, -0.6f), Db = norm(rt * 0.9f + up * 0.35f + fw * 0.2f); G = mix3(G, gb, blk); D = norm(mix3(D, Db, blk)); }
  auto mk = [&](V3 pos, V3 yA, V3 zA) { yA = norm(yA); zA = norm(zA - yA * dot(zA, yA)); V3 xA = cross(yA, zA); M4 m = ident(); m.m[0] = xA.x * sc; m.m[1] = xA.y * sc; m.m[2] = xA.z * sc; m.m[4] = yA.x * sc; m.m[5] = yA.y * sc; m.m[6] = yA.z * sc; m.m[8] = zA.x * sc; m.m[9] = zA.y * sc; m.m[10] = zA.z * sc; m.m[12] = pos.x; m.m[13] = pos.y; m.m[14] = pos.z; return m; };
  static const float GRIP[8] = {0.12f, 0.18f, 0.f, 0.f, 0.f, 0.62f, 0.12f, 0.f};
  vmw.on = true; vmw.wp[0] = wR; vmw.wp[1] = wL;
  if (wR >= 0) vmw.m[0] = mk(G - D * (GRIP[wR] * sc), D, fw);
  V3 L = W(-0.32f, -0.30f + bob, -0.55f); bool hasL = wL >= 0;
  if (hasL) { if (wL == 3 || wL == 4) { V3 Ls = mix3(L, W(-0.04f, -0.12f, -0.62f), blk); V3 zA = norm(fw + rt * 0.45f); vmw.m[1] = mk(Ls - up * 0.46f * sc + zA * 0.07f, up, zA); L = Ls; } else if (wL == 7) { V3 y = norm(up + rt * 0.08f * std::sin(tm * 3.1f) + fw * 0.07f * std::cos(tm * 2.6f)); vmw.m[1] = mk(L - y * 0.34f * sc, y, fw); } else vmw.m[1] = mk(L - up * 0.1f, up, fw); }
  V3 leather = C(0x4a2f1c), sleeve = C(0x3b4a52);
  putV(SH_SPHERE, ball(G, 0.034f), leather, 0.8f, 0, 0); putV(SH_CYL, seg(G, W(0.50f, -0.62f, -0.05f), 0.034f, 0.034f), sleeve, 0.9f, 0, 0);
  if (st == roster::ST_HEAVY) { V3 G2 = G - D * 0.28f; putV(SH_SPHERE, ball(G2, 0.034f), leather, 0.8f, 0, 0); putV(SH_CYL, seg(G2, W(-0.45f, -0.62f, -0.05f), 0.034f, 0.034f), sleeve, 0.9f, 0, 0); }
  else if (hasL) { putV(SH_SPHERE, ball(L, 0.034f), leather, 0.8f, 0, 0); putV(SH_CYL, seg(L, W(-0.52f, -0.62f, -0.05f), 0.034f, 0.034f), sleeve, 0.9f, 0, 0); }
  if (st >= roster::ST_FIRE && wind > 0.05f) { V3 tip = G + D * 0.95f, c = st == roster::ST_FROST ? C(0xa8e0ff) : st == roster::ST_ALCH ? C(0x7aff8a) : C(0xff9a3a); putV(SH_SPHERE, ball(tip, (0.02f + 0.05f * wind) * (1 - strike)), c, 0.5f, 0, 8.f); }
}
}  // namespace
void buildShapes() {
  for (int k = 0; k < 4; k++) shWn[k] = shVn[k] = 0; vmw.on = false; if (mode == M_OFF) return;
  if (mode == M_SELECT) { V3 b{P.x, terrH(P.x, P.z), P.z}; wCyl(b - V3{0, 0.02f, 0}, b + V3{0, 0.2f, 0}, 1.0f, C(0x3a3a38), 0.9f); wCyl(b + V3{0, 0.2f, 0}, b + V3{0, 0.29f, 0}, 0.82f, C(0x55524a), 0.85f); wCyl(b + V3{0, 0.29f, 0}, b + V3{0, 0.30f, 0}, 0.9f, C(0xe0a33c), 0.5f, 0.3f, 0.5f); return; }
  // world arrows & projectiles
  for (const Arrow& a : arrows) { if (!a.on) continue;
    if (a.own == 0 && a.kind >= 10) spellGeo(a); else if (a.own == 0) { V3 d = a.stuck ? V3{a.vx, a.vy, a.vz} : norm(V3{a.vx, a.vy, a.vz}); V3 tp{a.x, a.y, a.z}; if (a.stuck) { float emb = a.tg >= 0 ? 0.12f : 0.34f, sink = a.life < 1.2f ? (1.2f - a.life) * 0.6f : 0.f; tp = tp + d * emb; tp.y -= sink; } arrowGeo(tp, d, 0.8f, false); }
    else { V3 c{a.x, a.y, a.z}; V3 col = C(MON[(int)a.pw].boss ? 0xff7a3a : MON[(int)a.pw].ranged ? 0x6aff9a : 0xa8c070); put(shW, shWn, SH_SPHERE, ball(c, 0.16f), col, 0.5f, 0, 6.f); put(shW, shWn, SH_SPHERE, ball(c - norm(V3{a.vx, a.vy, a.vz}) * 0.3f, 0.1f), col, 0.5f, 0, 4.f); } }
  // targets
  static const uint32_t RC[5] = {0xe8e0cc, 0x1c1c20, 0x2a5ea8, 0xc83a2a, 0xf0c030}; static const float RR[5] = {1.0f, 0.82f, 0.6f, 0.36f, 0.14f};
  for (const Target& t : tgts) { if (!t.on) continue; float ti = t.tilt * std::sin(t.hitT * 14.f); V3 n = norm(V3{-std::cos(ti), 0, std::sin(ti)}); V3 c{t.x, t.y, t.z}; V3 up{0, 1, 0};
    wCyl(c + n * -0.0f + V3{0.14f, 0, 0}, c + V3{0.02f, 0, 0}, t.R * 1.08f, C(0xb89a58), 0.95f);   // straw backing
    for (int i = 0; i < 5; i++) { V3 a0 = c + n * (0.02f + 0.014f * i) + V3{0, 0, 0}, a1 = a0 + n * 0.014f; put(shW, shWn, SH_CYL, seg(a0 - n * 0.0f, a1, t.R * RR[i], t.R * RR[i]), C(RC[i]), 0.8f, 0, i == 4 ? 0.25f : 0.f); }
    float gy = terrH(t.ox, t.oz); V3 foot{t.x + 0.06f, gy, t.z}; wCyl(foot + V3{0, 0, t.R * 0.5f}, c + V3{0.1f, -t.R * 0.7f, t.R * 0.15f}, 0.04f, C(0x4a3a28), 0.9f); wCyl(foot - V3{0, 0, t.R * 0.5f}, c + V3{0.1f, -t.R * 0.7f, -t.R * 0.15f}, 0.04f, C(0x4a3a28), 0.9f); (void)up; }
  for (const Zone& zn : zones) if (zn.on) { V3 b{zn.x, terrH(zn.x, zn.z), zn.z}; float fa = std::min(1.f, zn.t * 0.8f); wCyl(b + V3{0, 0.02f, 0}, b + V3{0, 0.05f, 0}, zn.r * (0.9f + 0.05f * std::sin(tm * 3.f)), C(0x3a8a3a) * fa, 0.8f, 0, 1.2f); }
  if (P.atkOn && !P.down && mode != M_SELECT && (P.style == roster::ST_SWORD || P.style == roster::ST_HEAVY) && P.atkDone && P.atkKind != A_BASH) {
    float p = clampf((P.atkT - P.atkWind) / 0.16f, 0, 1), fade = clampf(1.f - (P.atkT - P.atkWind - 0.08f) / 0.2f, 0, 1); if (fade > 0.01f) { bool hv = P.style == roster::ST_HEAVY; float sg = (P.atkIdx & 1) ? -1.f : 1.f, half = (hv ? 1.5f : P.atkIdx == 2 ? 1.4f : 1.0f), rr = hv ? 3.0f : 2.2f; V3 col = hv ? C(0xffc880) : C(0xe8f0ff); V3 prev{0, 0, 0}; bool vert = hv && P.atkCharged || (!hv && P.atkIdx == 2);
      for (int k = 0; k <= 8; k++) { float u = (float)k / 8.f; if (u > p) break; float a2 = P.atkYaw + sg * lerpf(half, -half, u); V3 q; if (vert) { float el = lerpf(2.4f, 0.1f, u); V3 f2 = fwdOf(P.atkYaw, 0); q = V3{P.x, P.y + 1.0f, P.z} + f2 * (std::sin(el) * rr) + V3{0, std::cos(el) * rr * 0.6f, 0}; } else q = V3{P.x + std::sin(a2) * rr, P.y + 0.95f + 0.35f * u * sg * 0.f + 0.25f * std::sin(u * PI), P.z + std::cos(a2) * rr};
        if (k > 0) put(shW, shWn, SH_CYL, seg(prev, q, 0.045f * (0.3f + u) * fade, 0.045f * (0.3f + u) * fade), col, 0.4f, 0, 5.f * fade); prev = q; } } }
  if (P.charging && !P.down && mode != M_SELECT && P.style >= roster::ST_FIRE) { V3 mz = muzzle(camEye()); float cf = clampf(P.chargeT / 0.9f, 0, 1); V3 c = P.style == roster::ST_FROST ? C(0xa8e0ff) : P.style == roster::ST_ALCH ? C(0x7aff8a) : C(0xff9a3a); put(shW, shWn, SH_SPHERE, ball(mz, 0.04f + 0.1f * cf), c, 0.5f, 0, 8.f); }
  // ballistic arc preview
  if (nDots) { for (int i = 0; i < nDots; i++) put(shW, shWn, SH_SPHERE, ball(dots[i], 0.035f + 0.00055f * (float)i), C(0xffe9a0), 0.5f, 0, 5.f); }
  if (landOk) put(shW, shWn, SH_SPHERE, ball(landPt, 0.1f), H.sweet > 0.5f ? C(0xffd24a) : C(0xffb070), 0.5f, 0, 7.f);
  // first-person view-model: bow (deformable limbs + string), arrow resting on the grip, gloved bow hand, draw hand hooked on the string
  if (H.view != VW_FIRST || P.down || mode == M_SELECT) return;
  if (P.style != roster::ST_BOW) { fpvOther(); return; }
  M4 cw = camWorld(gfx::camO.eye, gfx::camO.tgt); V3 rt{cw.m[0], cw.m[1], cw.m[2]}, up{cw.m[4], cw.m[5], cw.m[6]}, bk{cw.m[8], cw.m[9], cw.m[10]}, eye = gfx::camO.eye;
  auto W = [&](float x, float y, float z) { return eye + rt * x + up * y + bk * z; }; auto Wd = [&](float x, float y, float z) { return norm(rt * x + up * y + bk * z); };
  const float mo = cfg::S.reduceMotion ? 0.2f : 1.f;
  float e = P.drawing ? curve(P.draw) : 0.f, blk = P.blocking, spr = clampf((P.speed - 4.8f) / 2.f, 0, 1) * (P.drawing ? 0.f : 1.f), tmv = tm;
  float ease = e * e * (3.f - 2.f * e), lift = smooth(0.f, 0.3f, e), mv = std::min(1.f, P.speed / 4.f);
  float rl = P.relT < 0.6f ? P.relT : 9.f, ov = rl < 0.6f ? std::exp(-rl * 9.f) * std::cos(rl * 62.f) : 0.f, kick = rl < 0.5f ? std::exp(-rl * 14.f) : 0.f;
  float held = P.drawing && P.holdT > 0.9f ? std::min(1.f, (P.holdT - 0.9f) / 1.f) : 0.f;   // over-hold tremble
  float gx = lerpf(-0.27f, -0.14f, lift), gy = lerpf(-0.29f, -0.155f, lift), gz = lerpf(-0.66f, -0.76f, lift);
  gx = lerpf(gx, -0.03f, blk); gy = lerpf(gy, -0.10f, blk); gz = lerpf(gz, -0.64f, blk); gz += kick * 0.06f; gy += kick * 0.02f;
  gx += (0.010f * std::sin(tmv * 1.15f) - lagX * 0.5f + spr * 0.10f) * mo + held * 0.004f * nz(tmv * 24, 5);
  gy += (0.010f * std::sin(tmv * 1.55f + 1.f) + 0.004f * std::sin(tmv * 1.9f) + lagY * 0.5f - spr * 0.10f + std::sin(P.bob * 2.f) * 0.008f * mv) * mo + held * 0.004f * nz(tmv * 27, 6);
  gz += (spr * 0.05f + std::sin(P.bob) * 0.006f * mv) * mo;
  if (P.stagT > 0) { gx += 0.05f * nz(tmv * 30, 1); gy += 0.04f * nz(tmv * 33, 2); }
  V3 G = W(gx, gy, gz);
  V3 F = norm(mix3(Wd(0.10f, 0.05f + spr * 0.1f, -1.f), Wd(-0.073f, 0.015f, -0.39f), lift)); if (blk > 0.05f) F = norm(F + Wd(-0.1f, 0, 0) * blk);
  float ang = lerpf(lerpf(0.20f, -0.26f, lift), 1.35f, blk); V3 U = Wd(std::sin(ang), std::cos(ang), 0); U = norm(U - F * dot(U, F)); V3 side = norm(cross(F, U));
  float bend = 0.30f + 0.70f * e - 0.30f * ov, L = 0.38f; const int N = 8;
  V3 wood = C(0x7a4a26), tipc = C(0xd8cfae), leather = C(0x4a2f1c), sleeve = C(0x2e3a2e), strc = C(0x9c9078), brace = C(0x8a6a3a);
  auto limb = [&](float s) { float a_ = std::fabs(s); return G + U * (s * L * (1 - 0.1f * bend * a_ * a_)) - F * (bend * 0.2f * a_ * a_ * (0.7f + 0.3f * a_)); };
  for (int i = 0; i < N; i++) for (int sg = -1; sg <= 1; sg += 2) { float s0 = sg * (float)i / N, s1 = sg * (float)(i + 1) / N; V3 a0 = limb(s0), a1 = limb(s1); float th = 0.012f * (1 - 0.55f * (float)i / N); putV(SH_CYL, seg(a0, a1, th * 1.15f, th * 0.7f), wood, 0.5f, 0.f, 0.f); }
  V3 tp = limb(1), bt = limb(-1); putV(SH_SPHERE, ball(tp, 0.008f), tipc, 0.5f, 0, 0); putV(SH_SPHERE, ball(bt, 0.008f), tipc, 0.5f, 0, 0);
  putV(SH_CYL, seg(G - U * 0.08f, G + U * 0.08f, 0.027f, 0.027f), leather, 0.8f, 0, 0);   // riser wrap
  float dd = lerpf(0.09f, 0.40f, ease) - (rl < 0.6f ? 0.07f * ov : 0.f); V3 N0 = G - F * dd + side * (0.012f * ov) + up * (0.03f * std::sin(PI * e) * mo) + rt * (0.01f * std::sin(PI * e) * mo);
  putV(SH_CYL, seg(tp, N0, 0.002f, 0.002f), strc, 0.6f, 0, 0); putV(SH_CYL, seg(N0, bt, 0.002f, 0.002f), strc, 0.6f, 0, 0); putV(SH_CYL, seg(N0 - U * 0.022f, N0 + U * 0.022f, 0.0042f, 0.0042f), strc, 0.6f, 0, 0);
  bool nocked = P.nockT <= 0 && !(P.relT < 0.25f) && blk < 0.5f; if (nocked) arrowGeo(N0 + F * 0.62f, F, 0.62f, true);
  // draw hand: palm behind the string, three fingers hooked on it, opening on release; forearm + sleeve back to the elbow
  float op = rl < 0.22f ? 1.f - rl / 0.22f : 0.f; V3 HP = N0 - F * 0.05f - up * 0.004f;
  putV(SH_SPHERE, ball(HP, 0.024f), leather, 0.8f, 0, 0);
  for (int k = -1; k <= 1; k++) { V3 kn = HP + U * (k * 0.013f) + F * 0.012f, ti = N0 + U * (k * 0.011f) + (side * 0.02f - F * 0.02f + U * (k * 0.03f)) * op; putV(SH_CYL, seg(kn, ti, 0.0065f, 0.0065f), leather, 0.8f, 0, 0); }
  V3 Er = W(0.27f + 0.05f * e, -0.40f + 0.09f * e, -0.04f + 0.06f * e); putV(SH_CYL, seg(HP - F * 0.01f, Er, 0.027f, 0.027f), sleeve, 0.9f, 0, 0); putV(SH_CYL, seg(HP - F * 0.04f, HP - F * 0.09f, 0.031f, 0.031f), brace, 0.7f, 0, 0); putV(SH_CYL, seg(Er, W(0.52f, -0.78f, 0.35f), 0.036f, 0.036f), sleeve, 0.9f, 0, 0);
  // bow hand: fist around the riser (fingers in front, thumb across), forearm to the elbow
  putV(SH_SPHERE, ball(G - F * 0.004f, 0.029f), leather, 0.8f, 0, 0);
  for (int k = 0; k < 4; k++) putV(SH_SPHERE, ball(G + F * 0.03f + U * (-0.03f + 0.02f * k) - side * 0.004f, 0.01f), leather, 0.8f, 0, 0);
  putV(SH_SPHERE, ball(G + side * 0.032f - F * 0.008f + U * 0.018f, 0.011f), leather, 0.8f, 0, 0);
  V3 El = W(-0.34f - 0.02f * e, -0.43f, -0.24f); putV(SH_CYL, seg(G - F * 0.03f, El, 0.03f, 0.03f), sleeve, 0.9f, 0, 0); putV(SH_CYL, seg(G - F * 0.05f, G - F * 0.11f - U * 0.01f, 0.034f, 0.034f), brace, 0.7f, 0, 0); putV(SH_CYL, seg(El, W(-0.62f, -0.82f, 0.30f), 0.038f, 0.038f), sleeve, 0.9f, 0, 0);
}
}  // namespace pl
