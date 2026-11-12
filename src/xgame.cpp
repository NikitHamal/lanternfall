#include "explore.h"
#include "xworld.h"
#include "xrender.h"
#include "roster.h"
#include "settings.h"
#include "audio.h"
#include "render.h"
#include "game.h"
#include "xinterior.h"
#include "memstat.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#ifdef _WIN32
#include <direct.h>
#define TMKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define TMKDIR(p) mkdir(p, 0755)
#endif
namespace ex {
Hud H; Label labels[44]; int nLabels = 0; bool pending = false;
namespace {
using namespace xw;
static int wpnTier = 0, armorTier = 0, potions = 0;
static int dayIdx = 0; static int wxForce = 0; static float wxW = 0.f, lightT = 0.f; static float hfr(int n) { float v = std::sin((float)n * 12.9898f + 78.233f) * 43758.5453f; return v - std::floor(v); }
static void weatherUpdate(float dt);
bool on = false, built = false; Rng rng(77); float clockH = 9.5f, tMul = 1.f, tAbs = 0, sinceStart = 0;
enum Role { R_VILL, R_GUARD, R_KNIGHT, R_SOLDIER, R_MON, R_STAG, R_BOAR };
enum St { S_IDLE, S_WALK, S_PATROL, S_CHASE, S_ATK, S_FLEE, S_RETURN };
constexpr int MAXN = 1500;
struct RigDef { const FigAsset* a; int wR, wL; float scale; const char* title; bool own; };
struct Npc {
  bool used = false, alive = true, hitDone = false, lair = false, boss = false, onWall = false, vis = false, talking = false, asleep = false; int role = 0, rig = 0, mon = 0, state = 0, post = -1, road = -1, dir = 1, tgtKind = 0, tgtIdx = -1, settle = -1;
  float x = 0, y = 0, z = 0, face = 0, vx = 0, vz = 0, sc = 1, hp = 50, maxHp = 50, dmg = 10, spd = 1.5f, runSpd = 3.4f, hitT = 0, deadT = 0, cd = 0, atkT = -1, atkWind = 0.4f, atkRec = 0.5f, atkRange = 2.f, stT = 0, homeX = 0, homeZ = 0, tx = 0, tz = 0, aggro = 20, pathD = 0, wallY = 0, ax = 0, az = 0, bx = 0, bz = 0, moving = 0, ph = 0, look = 0, wander = 14, lastPlayerD = 99, angry = 0, limp = 0, panic = 0, stun = 0; bool brave = false, interior = false; char name[22] = {0}; FigAnim an;
};
Npc npcs[MAXN]; int monList[MAXN], nMonL = 0; RigDef rigs[40]; int nRigs = 0; int poolVill[10], nPoolVill = 0, poolGuard[4], nPoolGuard = 0, poolKnight[4], nPoolKnight = 0, poolSold[4], nPoolSold = 0;
FigAsset polyHeroes[6]; bool polyLoaded = false; float WID[FMAXB * 12];
struct PathInfo { std::vector<float> cum; float total = 0; }; std::vector<PathInfo> paths; std::vector<V3> campfires;
struct PlayerS { float x = 0, y = 0, z = 0, vy = 0, vx = 0, vz = 0, yaw = 0, pitch = 0.15f, face = 0, speed = 0, hp = 100, maxHp = 100, st = 100, atkT = -1, atkWind = 0.3f, atkRec = 0.36f, blockK = 0, hurtT = 0, deadT = 0, stagT = 0, rollSink = 0, camDist = 4.6f, comboT = 0, bobT = 0, regenWait = 0; bool ground = true, swim = false, fly = false, hitDone = false, block = false, tired = false; int combo = 0, view = 1; float dmgMul = 1; V3 eye, tgt; float fov = 1.16f; bool camInit = false; } P;
FigAnim animPl; float restT = 0, hurtFlash = 0, nextSpawn = 2.f; bool downKeys = false; int lastPoi = -1; float lastLocT = 0; int lastPlaceId = -99; bool wasInWater = false; float lairT = 0;
struct MonDef2 { const char* name; int mon; float hp, dmg, spd, aggro, range, wind, rec, cd, sc; };
const MonDef2 MD[] = {{"THORNLING", THORNLING, 42, 8, 3.6f, 22, 1.7f, 0.42f, 0.5f, 1.3f, 0}, {"PEATWIGHT", PEATWIGHT, 95, 14, 2.6f, 24, 2.1f, 0.55f, 0.6f, 1.7f, 0}, {"GLOAMHOUND", GLOAMHOUND, 60, 11, 6.2f, 30, 2.0f, 0.35f, 0.45f, 1.1f, 0}, {"GLOAM WISP", GLOAMWISP, 40, 9, 3.2f, 26, 2.2f, 0.5f, 0.5f, 1.6f, 0},
  {"RUSTBOUND", RUSTBOUND, 150, 18, 2.9f, 24, 2.3f, 0.6f, 0.65f, 1.9f, 0}, {"CAIRNBACK", CAIRNBACK, 280, 28, 2.1f, 24, 2.8f, 0.8f, 0.8f, 2.4f, 0}, {"BARROW KING", BARROWKING, 560, 36, 3.0f, 36, 3.2f, 0.8f, 0.8f, 2.0f, 0}, {"MOSSJAW", MOSSJAW, 480, 34, 2.7f, 34, 3.4f, 0.85f, 0.85f, 2.2f, 0},
  {"HOLLOW STAG", HOLLOWSTAG, 60, 0, 6.5f, 0, 0, 0, 0, 0, 0.8f}, {"EMBER HOUND", EMBERHOUND, 78, 13, 5.8f, 30, 2.0f, 0.38f, 0.45f, 1.1f, 0}, {"THORNBOAR", THORNBOAR, 85, 12, 4.6f, 9, 2.2f, 0.5f, 0.55f, 1.5f, 0.75f}, {"ROOT COLOSSUS", ROOTCOLOSSUS, 620, 42, 2.0f, 34, 3.8f, 0.95f, 0.9f, 2.4f, 0}};
const char* VN[] = {"Mara", "Tobin", "Elsbeth", "Garrick", "Wren", "Osric", "Hild", "Corwin", "Lissa", "Bram", "Isolde", "Percy", "Nell", "Alaric", "Maud", "Dunstan", "Rowan", "Edda", "Fenn", "Sybil", "Aldous", "Petra", "Hob", "Marta"};
const char* GN[] = {"Sir Halric", "Ser Vance", "Captain Orrin", "Dame Brisa", "Warden Kell", "Sergeant Tam", "Ser Beric", "Lt. Marek", "Dame Isane", "Corporal Dunn"};
const char* L_VILL[] = {"Fine day for a walk, traveller.", "Mind the woods after dark - the gloam stirs.", "The falls to the north are a sight. Follow a river upstream.", "Best bread in the realm at the market stalls.", "Our guards keep the road safe... mostly.", "Three kingdoms, and one road between them.", "The meadows bloom bright this season.", "I heard howling from the old graveyard last night.", "Stay by the lamps once the sun goes down.", "Have you seen the Elder Tree? Wanderers say it sings."};
const char* L_NAT[3][2] = {{"Aldermere's harbour feeds half the realm.", "Blue banners, strong walls - that is Aldermere."}, {"Korvath's mountains breed hard folk.", "The red banners hold the high passes."}, {"Verdane answers to no crown.", "Green country, free folk, and the finest honey."}};
const char* L_GUARD[] = {"Halt. State your business... ah, carry on.", "Roads are patrolled day and night.", "Beasts were seen near the ruins. Be careful.", "Keep to the roads after dusk.", "The gate is open to honest travellers.", "Monsters grow bolder each night."};
const char* L_KNIGHT[] = {"For the crown and the realm.", "I have fought the gloam since I was a boy.", "These walls have never fallen.", "Rest at a campfire if you are weary, wanderer."};
const char* L_SOLD[] = {"On patrol. Stay safe out there.", "The road to the next town is clear. For now.", "Seen anything strange in the woods?"};
inline float dist2(float ax, float az, float bx, float bz) { return (ax - bx) * (ax - bx) + (az - bz) * (az - bz); }
const FigAsset* asset(int i) { return rigs[i].a; }
int addRig(const FigAsset* a, int wR, int wL, float sc, const char* title) { if (!a || !a->ok || nRigs >= 40) return -1; rigs[nRigs] = {a, wR, wL, sc, title, false}; return nRigs++; }
float groundAt(float x, float z) { return height(x, z); }
bool badStep(float x, float z, float fromY) { float d; if (inWater(x, z, &d) && d > 0.75f) return true; float g = height(x, z); return g - fromY > 0.7f && slopeAt(x, z) > 1.1f; }
void onNewNpcSlot(int i);
struct Npc; void townOnKill(const Npc& t); void townOnAnimal();
Npc* newNpc() { for (int i = 0; i < MAXN; i++) if (!npcs[i].used) { Npc& n = npcs[i]; n = Npc(); n.used = true; n.an = FigAnim(); n.an.id = (uint32_t)i; n.an.t = rng() * 20.f; n.ph = rng() * 6.28f; onNewNpcSlot(i); return &n; } return nullptr; }
void setName(Npc& n, const char* s) { snprintf(n.name, sizeof n.name, "%s", s); }

// ------------------------------------------------------------------ spawning
Npc* spawnHuman(int role, float x, float z, float face, int settle, int pool = -1) {
  Npc* n = newNpc(); if (!n) return nullptr; n->role = role; n->x = x; n->z = z; n->y = groundAt(x, z); n->face = face; n->homeX = x; n->homeZ = z; n->settle = settle; int r;
  if (role == R_VILL) { int pi = (int)(rng() * nPoolVill) % std::max(1, nPoolVill); r = poolVill[pi]; n->hp = n->maxHp = 40; n->dmg = 4; n->spd = 1.15f; n->runSpd = 3.6f; setName(*n, VN[(int)(rng() * 24) % 24]); n->wander = 12 + rng() * 10; }
  else if (role == R_GUARD) { r = poolGuard[(int)(rng() * nPoolGuard) % nPoolGuard]; n->hp = n->maxHp = 150; n->dmg = 16; n->spd = 1.2f; n->runSpd = 4.8f; setName(*n, GN[(int)(rng() * 10) % 10]); n->aggro = 30; }
  else if (role == R_KNIGHT) { r = poolKnight[(int)(rng() * nPoolKnight) % nPoolKnight]; n->hp = n->maxHp = 280; n->dmg = 30; n->spd = 1.2f; n->runSpd = 5.0f; setName(*n, GN[(int)(rng() * 10) % 10]); n->aggro = 34; }
  else { r = poolSold[(int)(rng() * nPoolSold) % nPoolSold]; n->hp = n->maxHp = 190; n->dmg = 20; n->spd = 1.6f; n->runSpd = 4.6f; setName(*n, GN[(int)(rng() * 10) % 10]); n->aggro = 30; }
  if (pool >= 0) r = pool; n->rig = r; n->sc = rigs[r].scale * (0.94f + rng() * 0.12f); n->atkWind = 0.4f; n->atkRec = 0.5f; n->atkRange = role == R_VILL ? 1.9f : 2.4f; n->brave = rng() < 0.45f; if (role == R_VILL) n->dmg = 7; return n;
}
Npc* spawnMon(int type, float x, float z, bool lair, bool boss) {
  Npc* n = newNpc(); if (!n) return nullptr; const MonDef2& d = MD[type]; n->role = type == HOLLOWSTAG ? R_STAG : (type == THORNBOAR ? R_BOAR : R_MON); n->mon = type; n->x = x; n->z = z; n->y = groundAt(x, z); n->face = rng() * 6.28f; n->homeX = x; n->homeZ = z; n->hp = n->maxHp = d.hp * (boss ? 1.3f : 1.f); n->dmg = d.dmg; n->spd = d.spd * 0.45f; n->runSpd = d.spd; n->aggro = d.aggro; n->atkRange = d.range; n->atkWind = d.wind; n->atkRec = d.rec;
  const FigAsset& a = gFM[type]; n->sc = (d.sc > 0 ? d.sc : a.scale) * (boss ? 1.15f : 1.f) * (0.94f + rng() * 0.12f); n->lair = lair; n->boss = boss; n->wander = 18; setName(*n, d.name); n->state = S_IDLE; n->stT = rng() * 3; return n;
}
bool validSpot(float x, float z, float maxSlope = 0.7f) { if (std::fabs(x) > HALF - 30 || std::fabs(z) > HALF - 30) return false; float d; if (inWater(x, z, &d) || waterAt(x, z) > -1e8f) return false; if (height(x, z) < 1.f) return false; return slopeAt(x, z) < maxSlope; }
void populate() {
  for (Npc& n : npcs) n.used = false;
  // roads -> cumulative paths
  paths.assign(roads.size(), PathInfo()); for (size_t i = 0; i < roads.size(); i++) { PathInfo& p = paths[i]; p.cum.push_back(0); for (size_t k = 1; k < roads[i].pts.size(); k++) { const V3 &a = roads[i].pts[k - 1], &b = roads[i].pts[k]; p.total += std::sqrt(dist2(a.x, a.z, b.x, b.z)); p.cum.push_back(p.total); } }
  campfires.clear(); for (const Obj& o : objs) if (o.kind == K_CAMP) campfires.push_back({o.x, o.y, o.z});
  for (size_t i = 0; i < posts.size(); i++) { const Post& p = posts[i]; const Settle& s = settles[p.settle]; Npc* n = nullptr; bool cap = s.type == ST_CAPITAL;
    if (p.type == 0) { n = spawnHuman(cap && rng() < 0.5f ? R_KNIGHT : R_GUARD, p.x, p.z, p.face, p.settle); }
    else if (p.type == 1) { n = spawnHuman(R_GUARD, p.x, p.z, 0, p.settle); if (n) { n->onWall = true; n->wallY = p.y; n->y = p.y; n->ax = p.ax; n->az = p.az; n->bx = p.bx; n->bz = p.bz; n->tx = p.bx; n->tz = p.bz; n->state = S_PATROL; } }
    else if (p.type == 2) { n = spawnHuman(R_KNIGHT, p.x, p.z, p.face, p.settle); }
    else if (p.type == 3) { if (rng() < 0.6f) n = spawnHuman(R_VILL, p.x, p.z, p.face, p.settle); }
    else if (p.type == 4) { n = spawnHuman(R_VILL, p.x, p.z, p.face, p.settle); if (n) setName(*n, VN[(int)(rng() * 24) % 24]); }
    else { n = spawnHuman(R_GUARD, p.x, p.z, p.face, p.settle); }
    if (n) { n->post = (int)i; n->face = p.face; } }
  // road patrols (leader + two followers)
  for (size_t r = 0; r < roads.size(); r++) { if (roads[r].a == roads[r].b || paths[r].total < 160) continue; float start = rng() * paths[r].total; for (int k = 0; k < 3; k++) { Npc* n = spawnHuman(k == 0 ? R_KNIGHT : R_SOLDIER, 0, 0, 0, roads[r].a); if (!n) break; n->road = (int)r; n->state = S_PATROL; n->dir = 1; n->pathD = std::max(0.f, start - k * 3.2f); n->homeX = k; if (k == 0) n->hp = n->maxHp = 230; } }
  // stags + boars
  for (int t = 0, c = 0; t < 4000 && c < 34; t++) { float x = (rng() * 2 - 1) * (HALF - 120), z = (rng() * 2 - 1) * (HALF - 120); float h = height(x, z); if (h < 3 || h > 70 || !validSpot(x, z, 0.5f) || settleNear(x, z, 80) >= 0) continue; int gi = ((int)((z + HALF) / CS) * GS + (int)((x + HALF) / CS)); if (infoB[gi * 4 + 1] < 40) continue; spawnMon(c % 4 == 3 ? THORNBOAR : HOLLOWSTAG, x, z, true, false); c++; }
  // lairs
  for (const Lair& L : lairs) { int nm = 0; static const int T0[] = {THORNLING, THORNLING, PEATWIGHT, GLOAMHOUND}, T1[] = {RUSTBOUND, EMBERHOUND, RUSTBOUND, THORNLING}, T2[] = {PEATWIGHT, GLOAMWISP, GLOAMWISP, PEATWIGHT}, T3[] = {GLOAMWISP, RUSTBOUND, GLOAMWISP, RUSTBOUND}, T5[] = {CAIRNBACK, GLOAMHOUND, CAIRNBACK, EMBERHOUND}; const int* T = L.kind == 1 ? T1 : L.kind == 2 ? T2 : L.kind == 3 ? T3 : L.kind == 5 ? T5 : T0; if (L.kind == 4) { T = T0; }
    for (int k = 0; k < 4; k++) { float a = rng() * 6.28f, d = 3 + rng() * L.r * 0.6f; float x = L.x + std::cos(a) * d, z = L.z + std::sin(a) * d; if (!validSpot(x, z, 0.9f)) continue; spawnMon(T[k], x, z, true, false); nm++; }
    if (L.kind == 2 || L.kind == 3) spawnMon(BARROWKING, L.x, L.z, true, true); else if (L.kind == 4) spawnMon(ROOTCOLOSSUS, L.x + 3, L.z, true, true); else if (L.kind == 5 && rng() < 0.6f) spawnMon(MOSSJAW, L.x, L.z, true, true); }
}
// ------------------------------------------------------------------ combat helpers
void say(const char* who, const char* text) { snprintf(H.dlgName, sizeof H.dlgName, "%s", who); snprintf(H.dlgText, sizeof H.dlgText, "%s", text); H.dlgT = 5.5f; audio::play(S_CLICK, 0, 0.5f); }
void banner(const char* a, const char* b, float t = 4.f) { snprintf(H.banner, sizeof H.banner, "%s", a); snprintf(H.bannerSub, sizeof H.bannerSub, "%s", b); H.bannerT = t; }
void damagePlayer(float d, float fx, float fz) {
  d *= 1.f - 0.12f * (float)armorTier;
  if (P.deadT > 0 || P.hp <= 0) return; bool front = true; { float dx = fx - P.x, dz = fz - P.z, l = std::sqrt(dx * dx + dz * dz) + 1e-4f; front = (dx * std::sin(P.face) + dz * std::cos(P.face)) / l > 0.1f; }
  if (P.block && front) { d *= 0.25f; audio::play(S_HITSHIELD, 0, 0.8f); } else audio::play(S_HITSOFT, 0, 0.8f); P.hp -= d; P.hurtT = 0.4f; P.stagT = 0.25f; hurtFlash = std::min(1.f, hurtFlash + 0.5f); P.regenWait = 6.f;
  if (P.hp <= 0) { P.hp = 0; P.deadT = 0.01f; H.dead = true; }
}
#include "xgame_combat.inc"
// ------------------------------------------------------------------ NPC AI
float steer(Npc& n, float tx, float tz, float speed, float dt) {
  float dx = tx - n.x, dz = tz - n.z, d = std::sqrt(dx * dx + dz * dz); if (d < 0.05f) { n.moving = 0; return d; } float want = std::atan2(dx, dz); float da = angDiff(n.face, want); n.face += clampf(da, -dt * (speed > 3 ? 9.f : 5.f), dt * (speed > 3 ? 9.f : 5.f)); if (std::fabs(da) > 1.3f) speed *= 0.2f; if (n.limp > 0) speed *= 0.55f;
  float step = std::min(d, speed * dt); float nx = n.x + std::sin(n.face) * step, nz = n.z + std::cos(n.face) * step; if (n.interior) roomCollide(nx, nz, 0.4f * n.sc); else collide(nx, nz, 0.4f * n.sc, n.y); if (!n.onWall && !n.interior && badStep(nx, nz, n.y)) { n.moving = 0; n.stT = 0; n.state = S_IDLE; return d; }
  n.x = nx; n.z = nz; n.moving = speed; return d;
}
void faceToward(Npc& n, float tx, float tz, float dt) { float da = angDiff(n.face, std::atan2(tx - n.x, tz - n.z)); n.face += clampf(da, -dt * 4.f, dt * 4.f); }
Npc* nearestMonster(const Npc& n, float r) { Npc* best = nullptr; float bd = r * r; for (int k = 0; k < nMonL; k++) { Npc& m = npcs[monList[k]]; if (!m.alive || m.role != R_MON) continue; float d = dist2(n.x, n.z, m.x, m.z); if (d < bd) { bd = d; best = &m; } } return best; }
void tickAttack(Npc& n, float dt, bool vsPlayer, Npc* tn) {
  if (n.cd > 0) n.cd -= dt; if (n.atkT >= 0) { n.atkT += dt; if (!n.hitDone && n.atkT >= n.atkWind) { n.hitDone = true; if (vsPlayer) { if (dist2(n.x, n.z, P.x, P.z) < (n.atkRange * 1.5f + n.sc * 0.4f) * (n.atkRange * 1.5f + n.sc * 0.4f)) damagePlayer(n.dmg, n.x, n.z); } else if (tn && tn->alive) { if (dist2(n.x, n.z, tn->x, tn->z) < (n.atkRange * 1.6f + tn->sc * 0.4f) * (n.atkRange * 1.6f + tn->sc * 0.4f)) hurtNpc(*tn, n.dmg, n.x, n.z, false); } if (n.role >= R_MON && dist2(n.x, n.z, P.x, P.z) < 400.f) audio::play(S_MONATK, 0, 0.5f); }
    if (n.atkT >= n.atkWind + n.atkRec) { n.atkT = -1; n.cd = n.role >= R_MON ? MD[n.mon].cd : 1.1f; } }
}
#include "xkingdom.inc"
#include "xfauna.inc"
#include "xtown.inc"
#include "xanim.inc"
void onNewNpcSlot(int i) { KEX[i] = NpcK(); SCH[i] = Sch(); }
void aiHuman(Npc& n, float dt, float pd) {
  int nIdx = (int)(&n - npcs);
  if (n.asleep && townSleepTick(n)) return;
  if (aiSwornHuman(n, nIdx, dt, pd)) return;
  bool night = clockH > 21.5f || clockH < 5.f; Npc* th = nearestMonster(n, n.role == R_VILL ? 16.f : n.aggro);
  if (n.angry > 0 && n.angry < 1e8f) n.angry -= dt;
  { bool wanted = n.role >= R_GUARD && !n.onWall && bounty[natOf(n)] > 12.f && pd < n.aggro; bool inRange = P.hp > 0 && !P.fly && n.interior == CI.in;
    if (n.panic > 0) { n.panic -= dt; if (n.role == R_VILL && inRange) { float dx = n.x - P.x, dz = n.z - P.z, l = std::sqrt(dx * dx + dz * dz) + 1e-3f; steer(n, n.x + dx / l * 20, n.z + dz / l * 20, n.runSpd, dt); n.state = S_FLEE; n.atkT = -1; return; } }
    if ((n.angry > 0 || wanted) && !n.onWall && inRange) { huntCount++; float reach = n.atkRange + 0.7f; if (pd > n.aggro * 3.f) { n.angry = 0; n.state = n.role == R_VILL ? S_IDLE : S_RETURN; n.atkT = -1; }
      else { n.tgtKind = 1; if (pd > reach) { if (n.atkT < 0) steer(n, P.x, P.z, n.runSpd, dt); n.state = S_CHASE; } else { n.moving = 0; faceToward(n, P.x, P.z, dt); if (n.atkT < 0 && n.cd <= 0) { n.atkT = 0; n.hitDone = false; } n.state = S_ATK; } tickAttack(n, dt, true, nullptr); return; } }
    else if (n.state == S_CHASE && n.tgtKind == 1 && n.angry <= 0) { n.tgtKind = 0; n.state = n.role == R_VILL ? S_IDLE : S_RETURN; } }
  if (n.role == R_VILL) {
    if (th) { n.state = S_FLEE; n.stT = 3; } if (n.state == S_FLEE) { float dx = n.x - th->x, dz = n.z - th->z; if (!th) { n.stT -= dt; if (n.stT < 0) n.state = S_IDLE; } float hx = n.homeX, hz = n.homeZ; steer(n, hx, hz, n.runSpd, dt); if (dist2(n.x, n.z, hx, hz) < 9) { n.state = S_IDLE; } (void)dx; (void)dz; return; }
    if ((pd < 4.5f && !night) || (TM.open && TM.npc == nIdx)) { faceToward(n, P.x, P.z, dt); n.moving = 0; n.talking = true; return; } n.talking = false;
    if (townSchedule(n, nIdx, dt)) return;
    if (n.state == S_IDLE) { n.moving = 0; n.stT -= dt; if (n.stT <= 0) { float a = rng() * 6.28f, d = rng() * n.wander; float tx = n.homeX + std::cos(a) * d, tz = n.homeZ + std::sin(a) * d; if (night) { tx = n.homeX; tz = n.homeZ; } n.tx = tx; n.tz = tz; n.state = S_WALK; n.stT = 14; } }
    else if (n.state == S_WALK) { n.stT -= dt; float d = steer(n, n.tx, n.tz, n.spd, dt); if (d < 0.6f || n.stT <= 0) { n.state = S_IDLE; n.moving = 0; n.stT = 3 + rng() * 9; } } return; }
  // armed humans
  if (n.state == S_ATK || n.state == S_CHASE) { Npc* t = (n.tgtKind == 2 && n.tgtIdx >= 0) ? &npcs[n.tgtIdx] : nullptr; if (!t || !t->alive || !t->used) { n.state = S_RETURN; n.tgtKind = 0; } else { float d = std::sqrt(dist2(n.x, n.z, t->x, t->z)); float reach = n.atkRange + t->sc * 0.7f; if (d > n.aggro * 1.8f) { n.state = S_RETURN; n.tgtKind = 0; } else if (d > reach) { if (n.atkT < 0) { steer(n, t->x, t->z, n.runSpd, dt); } n.state = S_CHASE; } else { n.moving = 0; faceToward(n, t->x, t->z, dt); if (n.atkT < 0 && n.cd <= 0) { n.atkT = 0; n.hitDone = false; } n.state = S_ATK; } tickAttack(n, dt, false, t); return; } }
  if (th && !n.onWall) { n.tgtKind = 2; n.tgtIdx = (int)(th - npcs); n.state = S_CHASE; return; }
  if (n.onWall) { if (th) { faceToward(n, th->x, th->z, dt); n.moving = 0; return; } if (n.state == S_IDLE) { n.stT -= dt; n.moving = 0; if (n.stT <= 0) { n.state = S_PATROL; } } else { float d = steer(n, n.tx, n.tz, n.spd, dt); n.y = n.wallY; if (d < 0.6f) { bool atB = std::fabs(n.tx - n.bx) < 0.1f && std::fabs(n.tz - n.bz) < 0.1f; n.state = S_IDLE; n.stT = 2.5f; n.tx = atB ? n.ax : n.bx; n.tz = atB ? n.az : n.bz; } } return; }
  if (n.road >= 0) {   // road patrol: follow the polyline
    const Road& r = roads[n.road]; const PathInfo& p = paths[n.road]; if (n.state == S_RETURN) { float best = 1e30f; for (size_t k = 0; k < r.pts.size(); k++) { float d = dist2(n.x, n.z, r.pts[k].x, r.pts[k].z); if (d < best) { best = d; n.pathD = p.cum[k]; } } n.state = S_PATROL; }
    if (n.homeX > 0.5f) { /* follower: stay behind the leader */ }
    n.pathD += n.dir * n.spd * dt * 0.95f; if (n.pathD > p.total) { n.pathD = p.total; n.dir = -1; } if (n.pathD < 0) { n.pathD = 0; n.dir = 1; }
    size_t k = 1; while (k + 1 < p.cum.size() && p.cum[k] < n.pathD) k++; float seg = p.cum[k] - p.cum[k - 1] + 1e-4f, t = (n.pathD - p.cum[k - 1]) / seg; float tx = lerpf(r.pts[k - 1].x, r.pts[k].x, t) + 0.8f * (n.homeX == 1 ? 1.f : n.homeX == 2 ? -1.f : 0.f), tz = lerpf(r.pts[k - 1].z, r.pts[k].z, t);
    float d = std::sqrt(dist2(n.x, n.z, tx, tz)); if (d > 14.f) { n.x = tx; n.z = tz; } else steer(n, tx, tz, std::max(n.spd, d * 2.5f), dt); return; }
  // gate / plaza guard: hold the post, turn to face the player
  n.moving = 0; if (n.state == S_RETURN) { float d = steer(n, n.homeX, n.homeZ, n.runSpd * 0.5f, dt); if (d < 0.8f) n.state = S_IDLE; return; }
  if (pd < 5.5f) faceToward(n, P.x, P.z, dt); else if (n.post >= 0) { float da = angDiff(n.face, posts[n.post].face); n.face += clampf(da, -dt, dt); }
}
void aiMonster(Npc& n, float dt, float pd) {
  int mIdx = (int)(&n - npcs);
  if (n.role == R_MON && aiSiegeMonster(n, mIdx, dt)) return;
  const MonDef2& d = MD[n.mon]; bool day = clockH > 6.5f && clockH < 19.5f;
  if (n.role == R_STAG) { if (pd < 16.f) { float dx = n.x - P.x, dz = n.z - P.z, l = std::sqrt(dx * dx + dz * dz) + 1e-3f; steer(n, n.x + dx / l * 20, n.z + dz / l * 20, n.runSpd, dt); n.state = S_FLEE; return; } }
  else {
    float ar = n.aggro * (day && !n.lair && n.role == R_MON ? 0.55f : 1.f); Npc* target = nullptr; float td = 1e9f; int tk = 0;
    if (P.hp > 0 && !P.fly && pd < ar) { tk = 1; td = pd; }
    for (int i = 0; i < MAXN; i++) { Npc& h = npcs[i]; if (!h.used || !h.alive || h.role >= R_MON || h.onWall) continue; float dd = std::sqrt(dist2(n.x, n.z, h.x, h.z)); if (dd < ar * 0.8f && dd < td) { td = dd; target = &h; tk = 2; } }
    if (n.role == R_BOAR && tk == 2) { tk = 0; }
    if (tk && dist2(n.x, n.z, n.homeX, n.homeZ) < (n.lair ? 70.f * 70.f : 400.f * 400.f)) { n.tgtKind = tk; n.tgtIdx = tk == 2 ? (int)(target - npcs) : -1; }
    else if (n.tgtKind) { n.tgtKind = 0; n.state = S_RETURN; }
    if (n.tgtKind) { float tx, tz, tsc = 1; if (n.tgtKind == 1) { tx = P.x; tz = P.z; } else { Npc& t = npcs[n.tgtIdx]; if (!t.alive) { n.tgtKind = 0; return; } tx = t.x; tz = t.z; tsc = t.sc; } float dd = std::sqrt(dist2(n.x, n.z, tx, tz)); float reach = n.atkRange + tsc * 0.4f;
      if (dd > reach) { if (n.atkT < 0) { steer(n, tx, tz, n.runSpd, dt); n.state = S_CHASE; } } else { n.moving = 0; faceToward(n, tx, tz, dt); if (n.atkT < 0 && n.cd <= 0) { n.atkT = 0; n.hitDone = false; } n.state = S_ATK; } tickAttack(n, dt, n.tgtKind == 1, n.tgtKind == 2 ? &npcs[n.tgtIdx] : nullptr); if (n.atkT >= 0 || dd <= reach) return; return; } }
  (void)d; if (n.state == S_RETURN) { float dd = steer(n, n.homeX, n.homeZ, n.spd * 1.4f, dt); if (dd < 2) n.state = S_IDLE; return; }
  if (n.state == S_IDLE || n.state == S_FLEE) { n.moving = 0; n.stT -= dt; if (n.stT <= 0) { float a = rng() * 6.28f, dd = rng() * n.wander; n.tx = n.homeX + std::cos(a) * dd; n.tz = n.homeZ + std::sin(a) * dd; n.state = S_WALK; n.stT = 12; } }
  else if (n.state == S_WALK || n.state == S_CHASE) { n.stT -= dt; float dd = steer(n, n.tx, n.tz, n.spd, dt); if (dd < 0.8f || n.stT <= 0) { n.state = S_IDLE; n.stT = 2 + rng() * 6; n.moving = 0; } }
}
void spawner(float dt) {
  nextSpawn -= dt; if (nextSpawn > 0) return; nextSpawn = 2.2f; bool night = clockH > 20.f || clockH < 5.5f, dusk = (clockH > 17.5f && clockH <= 20.f) || (clockH >= 5.5f && clockH < 7.f); int want = night ? 26 : dusk ? 14 : 6; int cnt = 0; for (int k = 0; k < nMonL; k++) { Npc& m = npcs[monList[k]]; if (m.alive && !m.lair && m.role == R_MON) cnt++; }
  if (cnt >= want) return; for (int t = 0; t < 14; t++) { float a = rng() * 6.28f, d = 70 + rng() * 70; float x = P.x + std::cos(a) * d, z = P.z + std::sin(a) * d; if (!validSpot(x, z, 0.6f) || settleNear(x, z, 80) >= 0) continue; float h = height(x, z); static const int LOW[] = {THORNLING, THORNLING, GLOAMHOUND, PEATWIGHT, GLOAMWISP, RUSTBOUND}, HIGH[] = {CAIRNBACK, GLOAMHOUND, EMBERHOUND, RUSTBOUND}; int type = h > 60 ? HIGH[(int)(rng() * 4) % 4] : LOW[(int)(rng() * 6) % 6]; int pack = 1 + (int)(rng() * 3);
    for (int k = 0; k < pack; k++) { Npc* n = spawnMon(type, x + (rng() - 0.5f) * 6, z + (rng() - 0.5f) * 6, false, false); if (n) { n->aggro *= 1.1f; } } return; }
}
static void weatherUpdate(float dt) {
  float tgt = 0.f; if (wxForce) tgt = 1.f; else if (dayIdx > 0) { float r = hfr(dayIdx * 7 + 3); if (r < 0.5f) { float st = 6.5f + hfr(dayIdx * 13 + 1) * 11.f, ln = 2.5f + hfr(dayIdx * 29 + 5) * 4.5f; if (clockH > st && clockH < st + ln) tgt = 0.5f + 0.5f * hfr(dayIdx * 5 + 9); } }
  wxW += (tgt - wxW) * std::min(1.f, dt * (wxForce ? 5.f : 0.12f)); if (std::fabs(tgt - wxW) < 0.002f) wxW = tgt; H.rain = CI.in ? 0.f : wxW; xr::weather = wxW;
  if (lightT > 0) lightT = std::max(0.f, lightT - dt * 1.6f); else if (wxW > 0.8f && !CI.in && fr01() < dt * 0.05f) lightT = 1.f; }
}  // namespace

// ------------------------------------------------------------------ public
bool active() { return on; } float danger() { int c = 0; for (int k = 0; k < nMonL; k++) { const Npc& m = npcs[monList[k]]; if (m.alive && m.role == R_MON && dist2(m.x, m.z, P.x, P.z) < 35 * 35 && m.tgtKind == 1) c++; } return std::min(1.f, c * 0.35f); }
int poiCount() { return (int)pois.size(); } const char* poiName(int i) { return pois[i].name; } bool poiFound(int i) { return pois[i].found; } void poiPos(int i, float& x, float& z) { x = pois[i].x; z = pois[i].z; }
void resize(int w, int h) { if (built) xr::resize(w, h); }
void start() { gFigNoBaked = true;
  if (!built) { built = true; mem::mark("before worldgen"); generate(7); mem::mark("after worldgen"); buildEnters(); initKingdomWorld(); mem::mark("after kingdom init"); xr::init(); mem::mark("after renderer init");
    memset(WID, 0, sizeof WID); for (int b = 0; b < FMAXB; b++) { float* o = WID + b * 12; o[0] = o[5] = o[10] = 1; }
    static const char* PK[6] = {"polyfork/apothecary", "polyfork/lamplighter", "polyfork/rimebinder", "polyfork/warden", "polyfork/reaver", "polyfork/fletcher"}; for (int i = 0; i < 6; i++) figLoad(polyHeroes[i], PK[i]);
    int v;
    if ((v = addRig(&gFH[APOTHECARY], -1, -1, gFH[APOTHECARY].scale, "VILLAGER")) >= 0) poolVill[nPoolVill++] = v; if ((v = addRig(&polyHeroes[0], roster::WP_STAFF, roster::WP_NONE, 1.f, "HERBALIST")) >= 0) poolVill[nPoolVill++] = v; if ((v = addRig(&gFH[FLETCHER], -1, -1, gFH[FLETCHER].scale, "HUNTER")) >= 0) poolVill[nPoolVill++] = v;
    if ((v = addRig(&gFH[RIMEBINDER], -1, -1, gFH[RIMEBINDER].scale, "SCHOLAR")) >= 0) poolVill[nPoolVill++] = v; if ((v = addRig(&gFH[LAMPLIGHTER], -1, -1, gFH[LAMPLIGHTER].scale, "LAMPLIGHTER")) >= 0) poolVill[nPoolVill++] = v; if ((v = addRig(&polyHeroes[2], roster::WP_STAFF, roster::WP_NONE, 1.f, "ELDER")) >= 0) poolVill[nPoolVill++] = v;
    if ((v = addRig(&polyHeroes[5], roster::WP_BOW, roster::WP_NONE, 1.f, "WOODSMAN")) >= 0) poolVill[nPoolVill++] = v;
    if ((v = addRig(&gFH[WARDEN], -1, -1, gFH[WARDEN].scale, "GUARD")) >= 0) poolGuard[nPoolGuard++] = v; if ((v = addRig(&gFMAlt[RUSTBOUND], roster::WP_CLEAVER, roster::WP_ROUND, MON[RUSTBOUND].scale * 0.82f, "GATE WARDEN")) >= 0) poolGuard[nPoolGuard++] = v; if ((v = addRig(&polyHeroes[3], roster::WP_SWORD, roster::WP_KITE, 1.f, "GUARD")) >= 0) poolGuard[nPoolGuard++] = v;
    if ((v = addRig(&polyHeroes[3], roster::WP_SWORD, roster::WP_KITE, 1.08f, "KNIGHT")) >= 0) poolKnight[nPoolKnight++] = v; if ((v = addRig(&polyHeroes[4], roster::WP_GREATSWORD, roster::WP_NONE, 1.05f, "CHAMPION")) >= 0) poolKnight[nPoolKnight++] = v; if ((v = addRig(&gFH[REAVER], -1, -1, gFH[REAVER].scale, "CHAMPION")) >= 0) poolKnight[nPoolKnight++] = v;
    if ((v = addRig(&gFH[WARDEN], -1, -1, gFH[WARDEN].scale, "SOLDIER")) >= 0) poolSold[nPoolSold++] = v; if ((v = addRig(&polyHeroes[3], roster::WP_SWORD, roster::WP_KITE, 1.f, "SOLDIER")) >= 0) poolSold[nPoolSold++] = v; if ((v = addRig(&gFMAlt[RUSTBOUND], roster::WP_CLEAVER, roster::WP_ROUND, MON[RUSTBOUND].scale * 0.82f, "SOLDIER")) >= 0) poolSold[nPoolSold++] = v;
    if (!nPoolVill) poolVill[nPoolVill++] = 0; if (!nPoolGuard) poolGuard[nPoolGuard++] = 0; if (!nPoolKnight) poolKnight[nPoolKnight++] = 0; if (!nPoolSold) poolSold[nPoolSold++] = 0; mem::mark("after figures"); populate(); faunaInit(); releaseGen(); mem::mark("after npc+fauna"); H.discTotal = (int)pois.size(); }
  gfx::suspend(); on = true; memset(&P, 0, sizeof P); P = PlayerS(); const roster::Hero& hr = roster::at(roster::sel()); P.maxHp = P.hp = hr.hp; P.st = 100; P.dmgMul = hr.dmg;
  { const Settle& s = settles[nations[0].capital >= 0 ? nations[0].capital : 0]; float gx = s.x + s.gateX * 66.f, gz = s.z + s.gateZ * 66.f; P.x = gx; P.z = gz; P.y = height(gx, gz); P.yaw = std::atan2(-s.gateX, -s.gateZ); P.face = P.yaw; }
  P.view = 1; B = Bow(); B.weapon = hr.style == roster::ST_BOW ? 1 : 0; bounty[0] = bounty[1] = bounty[2] = 0; CI = Cell(); for (Arr& a : ARR) a.on = false; for (Npc& n : npcs) if (n.used && n.interior) n.used = false;
  resetKingdomSession();
  H = Hud(); H.discTotal = (int)pois.size(); for (const Poi& p : pois) if (p.found) H.disc++; H.view = 1; snprintf(H.hero, sizeof H.hero, "%s", hr.name); clockH = 9.5f; tMul = 1; restT = 0; animPl = FigAnim(); animPl.id = 9999; P.camInit = false; banner("EMBERVALE REALM", "BUILD YOUR KINGDOM - COMMAND TROOPS - SURVIVE THE NIGHT", 5.f); townStart();
}
void stop() { townSave(); on = false; gfx::resume(); }
void look(float dx, float dy) { if (H.map) return; const float k = 0.0023f * cfg::S.sens; P.yaw -= dx * k; P.pitch = clampf(P.pitch + (cfg::S.invertY ? 1.f : -1.f) * dy * k * 0.9f, -1.4f, 1.3f); P.pitch = clampf(P.pitch, -1.4f, 1.3f); }
void scroll(float dy) {
  if (K.buildMode) { K.bsel = (K.bsel + (dy > 0 ? -1 : 1) + NBT) % NBT; return; }
  P.camDist = clampf(P.camDist - dy * 0.6f, 1.8f, 14.f);
}
void keyPress(int key) {
  if (TM.open && townKey(key)) return;
  if (key == 'U') { townDraught(); return; }
  if (key == 27) { if (H.map) { H.map = false; return; } if (K.buildMode) { K.buildMode = false; return; } stop(); return; }
  if (key == 'M') { H.map = !H.map; return; }
  if (key == 'C' && !CI.in) { K.buildMode = !K.buildMode; B.draw = 0; if (K.buildMode) banner("KINGDOM BUILD MODE", "1-6 SELECT  R ROTATE  LMB BUILD  RMB REPAIR  C EXIT", 2.8f); return; }
  if (K.buildMode && key >= '1' && key <= '6') { K.bsel = key - '1'; return; }
  if (K.buildMode && key == 'R') { K.bYaw += PI * 0.125f; return; }
  if (key == 'B' || key == '1' || key == '2') { B.weapon = key == 'B' ? 1 - B.weapon : key == '1' ? 0 : 1; B.draw = 0; banner(B.weapon ? "BOW READY" : "WEAPON DRAWN", B.weapon ? "HOLD LMB TO DRAW - RELEASE TO LOOSE - RMB ZOOM" : "LMB STRIKE - RMB GUARD", 1.8f); return; }
  if (key == 'Q') { castSpell(); return; }
  if (key == 'Z') { K.spell = (K.spell + 1) % NSPELL; char sub[44]; snprintf(sub, sizeof sub, "MANA COST %d  -  PRESS Q TO CAST", (int)SP_COST[K.spell]); banner(SP_NAMES[K.spell], sub, 2.0f); audio::play(S_CLICK, 0, 0.5f); return; }
  if (key == 'G') { tryEnlistOrGather(); return; }
  if (key == 'X') { K.order = (K.order + 1) % 4; if (K.order == ORD_HOLD) { K.holdX = P.x + std::sin(P.yaw) * 3.5f; K.holdZ = P.z + std::cos(P.yaw) * 3.5f; K.holdYaw = P.yaw; } char sub[44]; snprintf(sub, sizeof sub, "%d SWORN TROOPS  -  %s", K.troops, ORD_NAMES[K.order]); banner("WARBAND COMMAND", sub, 2.2f); audio::play(S_CLICK, 0, 0.7f); return; }
  if (key == 'K') { if (!CI.in && !P.swim && !P.fly && P.deadT <= 0) { K.mounted = !K.mounted; if (K.mounted) rideH = false; banner(K.mounted ? "NOBLE HART SUMMONED" : "DISMOUNTED", K.mounted ? "GALLOP ACROSS THE REALM - BOW & SPELLS READY" : "", 2.2f); audio::play(S_LEVELUP, 0, 0.5f); } return; }
  if (key == 'V') { P.view = P.view ? 0 : 1; H.view = P.view; return; } if (key == 'F') { P.fly = !P.fly; H.fly = P.fly; if (P.fly) K.mounted = false; banner(P.fly ? "FLIGHT MODE" : "WALKING", P.fly ? "W/A/S/D FLY - SHIFT FAST" : "", 2.f); return; }
  if (key == 'T') { tMul = tMul < 2 ? 40.f : tMul < 100 ? 240.f : 1.f; H.timeMul = tMul; return; } if (key == 'N') { clockH = clockH < 5.5f ? 6.5f : clockH < 11 ? 12.f : clockH < 16 ? 18.3f : clockH < 21 ? 23.5f : 6.5f; return; } if (key == 290 || key == 'H') { H.show = !H.show; return; }
  if (key == ' ') { if (P.fly) return; if (P.ground && !P.swim && P.deadT <= 0) { P.vy = K.mounted ? 8.0f : 6.2f; P.ground = false; } return; }
  if (key == 'E') { if (restT > 0 || P.deadT > 0) return;
    if (!K.buildMode && faunaInteract()) return;
    int ri = nearestRift(4.5f, true); if (ri >= 0 && tryPurifyRift(ri)) return;
    if (!CI.in && nearestBarracks(5.5f) >= 0 && tryEnlistOrGather()) return;
    Npc* best = nullptr; float bd = 3.6f * 3.6f; for (Npc& n : npcs) { if (!n.used || !n.alive || n.role >= R_MON) continue; float d = dist2(n.x, n.z, P.x, P.z); if (d < bd) { bd = d; best = &n; } }
    if (best) { const char* line; char who[40]; const RigDef& rd = rigs[best->rig]; snprintf(who, sizeof who, "%s - %s", best->name, rd.title); if (best->role == R_VILL) { int k = (int)(rng() * 13); line = k < 10 ? L_VILL[k] : L_NAT[best->settle >= 0 ? settles[best->settle].nation : 0][k % 2]; } else if (best->role == R_GUARD) line = L_GUARD[(int)(rng() * 6) % 6]; else if (best->role == R_KNIGHT) line = L_KNIGHT[(int)(rng() * 4) % 4]; else line = L_SOLD[(int)(rng() * 3) % 3]; faceToward(*best, P.x, P.z, 1.f); if (townOpen(*best, (int)(best - npcs), who, line)) return; say(who, line); return; }
    if (CI.stage) return;
    if (CI.in) { const xi::Def& d = xi::DEFS[CI.idx]; float lx = P.x - d.cx, lz = P.z - d.cz; if (lz > d.hz - 2.1f && std::fabs(lx) < 1.8f) { CI.want = 0; CI.stage = 1; return; }
      if (d.chestX != 0 && dist2(lx, lz, d.chestX, d.chestZ) < 2.2f * 2.2f) { if (!looted[CI.house]) { looted[CI.house] = 1; int g = 15 + (int)(rng() * 55); gold += g; B.arrows = std::min(40, B.arrows + 10); char sub[40]; snprintf(sub, sizeof sub, "%d GOLD  +10 ARROWS", g); banner("TREASURE", sub, 3.5f); audio::play(S_LEVELUP, 0, 0.6f); } return; }
      if (CI.idx == 0 && dist2(lx, lz, d.bedX, d.bedZ) < 2.4f * 2.4f) { restT = 3.2f; return; } return; }
    int ei = nearEnter(); if (ei >= 0) { CI.ox = P.x; CI.oz = P.z; CI.want = ei + 1; CI.stage = 1; return; }
    for (const V3& c : campfires) if (dist2(c.x, c.z, P.x, P.z) < 3.8f * 3.8f) { restT = 3.2f; return; } }
}
void debugPlace(float x, float z, float yawDeg, float pitchDeg) { P.x = x; P.z = z; { float dk = deckTop(x, z); P.y = dk > -1e8f ? dk : height(x, z); } P.yaw = yawDeg * PI / 180.f; P.face = P.yaw; P.pitch = pitchDeg * PI / 180.f; P.camInit = false; } void debugHour(float h) { clockH = h; } void debugCam(float d) { P.camDist = d; } void debugFly(bool f) { P.fly = f; H.fly = f; } void debugView(int v) { P.view = v; H.view = v; } void debugMap(bool m) { H.map = m; }
void debugPoi(int i) { if (pois.empty()) return; const Poi& p = pois[((i % (int)pois.size()) + (int)pois.size()) % (int)pois.size()]; float back = p.kind == 3 ? 24.f : p.kind == 0 ? 120.f : p.kind == 4 ? 60.f : 38.f; float a = p.kind == 3 ? 0.f : 0.9f; float x = p.x + std::sin(a) * back, z = p.z + std::cos(a) * back; if (p.kind == 3 && !falls.empty()) { for (const Fall& f : falls) if (std::fabs(f.x - p.x) < 1 && std::fabs(f.z - p.z) < 1) { x = f.x + f.dx * 22.f + f.dz * 6.f; z = f.z + f.dz * 22.f - f.dx * 6.f; } } P.x = x; P.z = z; P.y = height(x, z); P.yaw = std::atan2(p.x - x, p.z - z); P.face = P.yaw; P.pitch = p.kind == 4 ? 0.05f : 0.12f; P.camInit = false; }
void debugCmd(const char* c) { if (const char* sq = strstr(c, "spd=")) dbgSpd = (float)atof(sq + 4); if (strstr(c, "clean")) { dbgClean = true; P.view = 1; H.view = 1; } if (strstr(c, "rain")) { wxForce = 1; wxW = 1.f; } if (strstr(c, "mapview")) H.map = true; if (const char* sk = strstr(c, "sky")) { P.fly = true; H.fly = true; P.y += (float)atof(sk + 3); }
  { static const char* AN[SP_N] = {"dog", "cat", "hen", "sheep", "cow", "horse", "deer", "rabbit"}; int want = -1; for (int q = 0; q < SP_N; q++) if (strstr(c, AN[q])) want = q; if (want >= 0) { int bi = -1; float bd = 1e30f; for (int i = 0; i < nFA; i++) if (FA[i].used && FA[i].alive && FA[i].sp == want) { float d = dist2(FA[i].x, FA[i].z, P.x, P.z); if (d < bd) { bd = d; bi = i; } }
    if (bi >= 0) { Fauna& f = FA[bi]; float dd = SPD[want].legLen < 0.3f ? 1.6f : want == SP_HORSE || want == SP_COW || want == SP_DEER ? 4.4f : 2.6f; if (const char* dp = strstr(c, "@")) dd = (float)atof(dp + 1); float ang = strstr(c, "front") ? 0.f : strstr(c, "back") ? PI : 1.1f; float x = f.x + std::sin(f.face + ang) * dd, z = f.z + std::cos(f.face + ang) * dd; P.x = x; P.z = z; P.y = height(x, z); P.yaw = std::atan2(f.x - x, f.z - z); P.face = P.yaw; P.pitch = 0.05f; P.camDist = dd; P.camInit = false; if (dbgClean) { dbgTrack = bi; dbgAng = ang; dbgDist = dd; if (const char* hp = strstr(c, "h=")) dbgHF = (float)atof(hp + 2); }
      if (strstr(c, "ride") && want == SP_HORSE) { P.x = f.x; P.z = f.z; P.y = height(f.x, f.z); P.face = f.face; P.yaw = f.face + 0.55f; P.pitch = 0.1f; P.camDist = 5.2f; rideH = true; K.mounted = true; rideF = f; rideF.used = true; rideF.sc = 1.f; rideF.act = 0; mountT = 1.f; dmT = 0.f; f.used = false; if (dbgClean) dbgTrack = -2; }
      if (strstr(c, "board") && want == SP_HORSE) faunaInteract(); if (strstr(c, "graze")) { f.act = 1; f.t = 999.f; f.state = 0; } if (strstr(c, "lie")) { f.act = 2; f.t = 999.f; f.state = 0; } } } }
  if (const char* tp = strstr(c, "tr_")) { int want = !strncmp(tp + 3, "oak", 3) ? xw::K_OAK : !strncmp(tp + 3, "pine", 4) ? xw::K_PINE : !strncmp(tp + 3, "piney", 5) ? xw::K_PINEY : !strncmp(tp + 3, "birch", 5) ? xw::K_BIRCH : !strncmp(tp + 3, "jungle", 6) ? xw::K_JUNGLE : !strncmp(tp + 3, "willow", 6) ? xw::K_WILLOW : !strncmp(tp + 3, "palm", 4) ? xw::K_PALM : xw::K_CHERRY; float dd = 11.f; if (const char* dp = strstr(tp, "d")) if (dp[1] >= '0' && dp[1] <= '9') dd = atof(dp + 1); int bi = -1; float bd = 1e30f; for (int i = 0; i < (int)objs.size(); i++) if (objs[i].kind == want) { float d = dist2(objs[i].x, objs[i].z, P.x, P.z); if (d < bd) { bd = d; bi = i; } }
    if (bi >= 0) { const Obj& o = objs[bi]; float x = o.x + dd * 0.8f, z = o.z + dd * 0.6f; P.x = x; P.z = z; P.y = height(x, z); P.yaw = std::atan2(o.x - x, o.z - z); P.face = P.yaw; P.pitch = 0.12f; P.camDist = 5.f; P.camInit = false; if (strstr(c, "hi")) { P.fly = true; H.fly = true; P.y = o.y + 13.f; P.pitch = -0.28f; } } }
  if (const char* fp = strstr(c, "fall")) if (fp[4] >= '0' && fp[4] <= '9' && !falls.empty()) { const Fall& f = falls[(fp[4] - '0') % (int)falls.size()]; float d = fp[5] >= '0' && fp[5] <= '9' ? (fp[5] - '0') * 6.f : 24.f; float x = f.x + f.dx * d + f.dz * 5.f, z = f.z + f.dz * d - f.dx * 5.f; P.x = x; P.z = z; P.y = height(x, z); P.yaw = std::atan2(f.x - x, f.z - z); P.face = P.yaw; P.pitch = 0.2f; P.camDist = 7.f; P.camInit = false; } if (strstr(c, "census")) dbgCensus = true;
  if (strstr(c, "talk") || strstr(c, "shop")) { bool shop = strstr(c, "shop") != nullptr; int bi = -1; float bd = 1e30f; const Settle& cs = settles[nations[0].capital >= 0 ? nations[0].capital : 0];
    for (int i = 0; i < MAXN; i++) { const Npc& n = npcs[i]; if (!n.used || !n.alive || n.asleep || n.role != R_VILL || (shop && !isMerchant(n))) continue; float d = dist2(n.x, n.z, cs.x, cs.z); if (d < bd) { bd = d; bi = i; } }
    if (bi >= 0) { Npc& n = npcs[bi]; P.x = n.x + std::sin(n.face) * 2.6f; P.z = n.z + std::cos(n.face) * 2.6f; P.y = height(P.x, P.z); P.yaw = std::atan2(n.x - P.x, n.z - P.z); P.face = P.yaw; P.pitch = 0.05f; P.camDist = 3.4f; P.camInit = false; n.state = S_IDLE; n.stT = 99; char who[40]; snprintf(who, sizeof who, "%s - %s", n.name, rigs[n.rig].title); townOpen(n, bi, who, L_VILL[0]);
      if (strstr(c, "quest")) for (int k = 0; k < TM.n; k++) if (TM.id[k] == O_WORK) townChoose(k); if (strstr(c, "close")) townClose(); } }
  if (strstr(c, "savenow")) townSave();
  if (strstr(c, "side")) dbgSide = 1.5708f; if (const char* fp = strstr(c, "fx")) dbgFx = atoi(fp + 2); if (strstr(c, "bow")) B.weapon = 1; if (strstr(c, "melee")) B.weapon = 0; if (strstr(c, "hold")) dbgDrawHold = true; if (strstr(c, "run")) dbgWalk = 2; else if (strstr(c, "walk")) dbgWalk = 1; if (const char* p = strstr(c, "shoot")) dbgShoot = atoi(p + 5) > 0 ? atoi(p + 5) : 30;
  if (strstr(c, "horde")) startKingdomSiege(true); if (strstr(c, "hostile")) { for (int k = 0; k < 3; k++) bounty[k] = 120; } if (strstr(c, "noai")) for (Npc& n : npcs) if (n.used && n.role >= R_MON && n.role == R_MON) n.used = false;
  if (const char* p = strstr(c, "enter")) { int want = atoi(p + 5), best = -1; float bd = 1e30f; for (size_t i = 0; i < enters.size(); i++) if (enters[i].idx == want) { float d = dist2(enters[i].x, enters[i].z, P.x, P.z); if (d < bd) { bd = d; best = (int)i; } } if (best >= 0) { CI.ox = enters[best].x + enters[best].R; CI.oz = enters[best].z; doEnter(best); } }
  if (strstr(c, "mount")) {
    // Place on open sunlit meadow road outside capital facing the countryside so full stag + warband shine
    const Settle& s = settles[nations[0].capital >= 0 ? nations[0].capital : 0];
    P.x = s.x + s.gateX * 92.f; P.z = s.z + s.gateZ * 92.f; P.y = height(P.x, P.z);
    P.yaw = std::atan2(s.gateX, s.gateZ); P.face = P.yaw; P.pitch = -0.06f; P.camDist = 5.8f; P.camInit = false;
    for (Poi& p : pois) if (dist2(p.x, p.z, P.x, P.z) < 160.f * 160.f) { p.found = true; H.disc++; }
    K.mounted = true;
    for (int k = 0; k < 3; k++) {
      float rx = std::cos(P.yaw), rz = -std::sin(P.yaw);
      Npc* sw = spawnHuman(k == 0 ? R_KNIGHT : R_SOLDIER, P.x - std::sin(P.yaw) * 3.2f + rx * (k - 1) * 2.4f, P.z - std::cos(P.yaw) * 3.2f + rz * (k - 1) * 2.4f, P.yaw, -1);
      if (sw) makeSworn(*sw);
    }
    banner("NOBLE HART SUMMONED", "GALLOP ACROSS THE REALM WITH YOUR SWORN WARBAND", 4.5f);
  }
  if (strstr(c, "buildmode")) {
    const Settle& s = settles[nations[0].capital >= 0 ? nations[0].capital : 0];
    P.x = s.x + s.gateX * 96.f; P.z = s.z + s.gateZ * 96.f; P.y = height(P.x, P.z);
    P.yaw = std::atan2(-s.gateX, -s.gateZ); P.face = P.yaw; P.pitch = -0.14f; P.camDist = 5.6f; P.camInit = false;
    for (Poi& p : pois) if (dist2(p.x, p.z, P.x, P.z) < 160.f * 160.f) { p.found = true; H.disc++; }
    float fx = std::sin(P.yaw), fz = std::cos(P.yaw), rx = std::cos(P.yaw), rz = -std::sin(P.yaw);
    placeBuilding(BT_WALL, P.x + fx * 8.5f - rx * 5.2f, P.z + fz * 8.5f - rz * 5.2f, P.yaw, true);
    placeBuilding(BT_TOWER, P.x + fx * 9.5f + rx * 6.2f, P.z + fz * 9.5f + rz * 6.2f, P.yaw, true);
    placeBuilding(BT_BARRACKS, P.x + fx * 14.f - rx * 8.5f, P.z + fz * 14.f - rz * 8.5f, P.yaw + 0.4f, true);
    placeBuilding(BT_BEACON, P.x + fx * 7.5f + rx * 3.2f, P.z + fz * 7.5f + rz * 3.2f, P.yaw, true);
    K.buildMode = true; K.bsel = BT_WALL; K.bYaw = 0; H.bannerT = 0.f;
  }
  if (strstr(c, "siege")) {
    clockH = 21.2f; prevHour = 21.2f;
    const Settle& s = settles[nations[0].capital >= 0 ? nations[0].capital : 0];
    P.x = s.x + s.gateX * 88.f; P.z = s.z + s.gateZ * 88.f; P.y = height(P.x, P.z);
    P.yaw = std::atan2(s.gateX, s.gateZ); P.face = P.yaw; P.pitch = -0.16f; P.camDist = 6.8f; P.camInit = false;
    for (Poi& p : pois) if (dist2(p.x, p.z, P.x, P.z) < 180.f * 180.f) { p.found = true; H.disc++; }
    float fx = std::sin(P.yaw), fz = std::cos(P.yaw), rx = std::cos(P.yaw), rz = -std::sin(P.yaw);
    placeBuilding(BT_GATE, P.x + fx * 9.5f, P.z + fz * 9.5f, P.yaw, true);
    placeBuilding(BT_WALL, P.x + fx * 9.5f - rx * 6.6f, P.z + fz * 9.5f - rz * 6.6f, P.yaw, true);
    placeBuilding(BT_WALL, P.x + fx * 9.5f + rx * 6.6f, P.z + fz * 9.5f + rz * 6.6f, P.yaw, true);
    placeBuilding(BT_TOWER, P.x + fx * 8.0f - rx * 10.2f, P.z + fz * 8.0f - rz * 10.2f, P.yaw, true);
    placeBuilding(BT_TOWER, P.x + fx * 8.0f + rx * 10.2f, P.z + fz * 8.0f + rz * 10.2f, P.yaw, true);
    placeBuilding(BT_BEACON, P.x + fx * 6.2f - rx * 3.8f, P.z + fz * 6.2f - rz * 3.8f, P.yaw, true);
    placeBuilding(BT_BEACON, P.x + fx * 6.2f + rx * 3.8f, P.z + fz * 6.2f + rz * 3.8f, P.yaw, true);
    for (int k = 0; k < 4; k++) { Npc* sw = spawnHuman(k < 2 ? R_KNIGHT : R_SOLDIER, P.x + fx * 7.8f + rx * (k - 1.5f) * 2.0f, P.z + fz * 7.8f + rz * (k - 1.5f) * 2.0f, P.yaw, -1); if (sw) makeSworn(*sw); }
    K.order = ORD_HOLD; K.holdX = P.x + fx * 8.2f; K.holdZ = P.z + fz * 8.2f; K.holdYaw = P.yaw;
    for (int k = 0; k < 9; k++) {
      float ox = (k - 4) * 2.4f, d = 17.5f + (k % 3) * 3.2f;
      int mt = k == 4 ? BARROWKING : (k % 3 == 0 ? CAIRNBACK : (k % 2 == 0 ? RUSTBOUND : PEATWIGHT));
      Npc* m = spawnMon(mt, P.x + fx * d + rx * ox, P.z + fz * d + rz * ox, false, k == 4);
      if (m) { int mi = (int)(m - npcs); KEX[mi].siegeUnit = true; m->homeX = P.x + fx * 9.5f; m->homeZ = P.z + fz * 9.5f; m->aggro = 60.f; }
    }
    K.nightWave = 1; K.siegeActive = true; K.siegeTotal = 9; K.siegeLeft = 9;
    banner("NIGHT 1 DEMON SIEGE", "DEFEND YOUR WALLS & ARCHER TOWERS!", 5.f);
  }
  if (strstr(c, "spells")) {
    for (Poi& p : pois) if (dist2(p.x, p.z, P.x, P.z) < 160.f * 160.f) { p.found = true; H.disc++; }
    float fx = std::sin(P.yaw), fz = std::cos(P.yaw), rx = std::cos(P.yaw), rz = -std::sin(P.yaw);
    for (int k = 0; k < 5; k++) { Npc* m = spawnMon(k == 2 ? CAIRNBACK : RUSTBOUND, P.x + fx * (10.f + (k % 2) * 2.5f) + rx * (k - 2) * 2.2f, P.z + fz * (10.f + (k % 2) * 2.5f) + rz * (k - 2) * 2.2f, false, false); (void)m; }
    K.spell = SP_FROST; K.mana = 100.f; castSpell();
    K.spell = SP_FIREBALL; K.spCd = 0.f; K.mana = 100.f; castSpell();
    banner("ARCANE SPELLCRAFT", "SOLAR FIREBALL & GLACIAL FROST NOVA", 4.f);
  }
  if (strstr(c, "rift")) {
    for (int i = 0; i < MAXRIFT; i++) if (rifts[i].used) {
      P.x = rifts[i].x - 3.6f; P.z = rifts[i].z + 3.6f; P.y = height(P.x, P.z);
      P.yaw = std::atan2(rifts[i].x - P.x, rifts[i].z - P.z); P.face = P.yaw; P.pitch = 0.08f; P.camInit = false;
      for (Poi& p : pois) if (dist2(p.x, p.z, P.x, P.z) < 160.f * 160.f) { p.found = true; H.disc++; }
      banner("CORRUPTED REALM SANCTUM", "SLAY THE GUARDIANS & PURIFY THE RIFT", 4.5f);
      break;
    }
  }
}
void debugKillDay() { for (Npc& n : npcs) if (n.used && n.role >= R_MON && n.role == R_MON && n.alive) n.used = false; }
bool mapToScreen(float wx, float wz, float& sx, float& sy) { if (!H.map) return false; sx = H.mapPx + (wx / HALF * 0.5f + 0.5f) * H.mapSize; sy = H.mapPy + (wz / HALF * 0.5f + 0.5f) * H.mapSize; return true; }

void update(float dtReal, const pl::Input& inRaw) {
  pl::Input in = inRaw; if (TM.open) { in.lmb = in.rmb = false; in.mx = in.mz = 0; in.sprint = false; }
  float dt = std::min(dtReal, 0.05f); tAbs += dt; sinceStart += dt; clockH += dt * tMul / 60.f; if (restT > 0) { clockH += dt * 2.f; } if (clockH >= 24.f) { clockH -= 24.f; dayIdx++; } H.hour = clockH; weatherUpdate(dt); H.hurt = hurtFlash; hurtFlash = std::max(0.f, hurtFlash - dt * 1.4f);
  if (H.bannerT > 0) H.bannerT -= dt; if (H.dlgT > 0) H.dlgT -= dt; if (H.bigT > 0) H.bigT -= dt; H.locT = std::max(0.f, H.locT - dt);
  const roster::Hero& hr = roster::at(roster::sel()); const FigAsset* pa = roster::cur(); float psc = pa ? pa->scale : 1.f;
  // ---- player
  if (P.deadT > 0) { P.deadT += dt; if (P.deadT > 3.2f) { if (CI.in) doExit(); int best = 0; float bd = 1e30f; for (size_t i = 0; i < settles.size(); i++) if (settles[i].type <= ST_VILLAGE) { float d = dist2(settles[i].x, settles[i].z, P.x, P.z); if (d < bd) { bd = d; best = (int)i; } } const Settle& s = settles[best]; float gx = s.x + s.gateX * (s.type == ST_CAPITAL ? 60.f : 14.f), gz = s.z + s.gateZ * (s.type == ST_CAPITAL ? 60.f : 14.f); P.x = gx; P.z = gz; P.y = gH(gx, gz); P.hp = P.maxHp; P.deadT = 0; H.dead = false; banner("YOU WERE RESCUED", s.name, 4.f); P.camInit = false; } }
  else if (restT > 0) { restT -= dt; P.hp = std::min(P.maxHp, P.hp + dt * P.maxHp * 0.4f); if (restT <= 0) { P.hp = P.maxHp; banner("RESTED", "THE NIGHT PASSES BY THE FIRE", 3.f); } }
  else if (!H.map || true) {
    float sy = std::sin(P.yaw), cy = std::cos(P.yaw); float mx = in.mx, mz = in.mz; if (H.map || CI.stage == 1) { mx = mz = 0; } if (dbgWalk) { mx = 0; mz = 1; }
    bool wantSprint = (in.sprint || dbgWalk == 2) && (mx != 0 || mz != 0); if (P.tired && P.st > 30.f) P.tired = false; if (P.st <= 0.5f) P.tired = true; bool sprint = wantSprint && !P.tired && !P.swim; if (P.fly) sprint = in.sprint;
    P.block = in.rmb && !P.fly && !H.map && B.weapon == 0 && !K.buildMode;
    if (in.lmb && !H.map && P.atkT < 0 && !P.fly && B.weapon == 0 && !K.buildMode && CI.stage != 1) playerAttack();
    if (!K.buildMode) bowUpdate(dt, in);
    if (dbgShoot >= 0 && ++dbgFrames == dbgShoot) fireArrow(0.9f);
    if (P.comboT > 0) { P.comboT -= dt; if (P.comboT <= 0) P.combo = 0; }
    float fwdx = sy, fwdz = cy, rx = -cy, rz = sy;
    float speed = 3.5f * hr.move * (K.mounted ? 2.15f : 1.f) * (sprint ? (K.mounted ? 1.55f : 1.95f) : 1.f) * (P.block ? 0.45f : 1.f) * (P.atkT >= 0 && !K.mounted ? 0.55f : 1.f) * (B.draw > 0 && !K.mounted ? 0.55f : 1.f);
    if (P.fly) { float cp = std::cos(P.pitch), sp = std::sin(P.pitch); float fs = sprint ? 130.f : 34.f; float vx = (fwdx * cp * mz + rx * mx) * fs, vz = (fwdz * cp * mz + rz * mx) * fs, vyy = sp * mz * fs; P.x += vx * dt; P.z += vz * dt; P.y += vyy * dt; P.y = std::max(P.y, gH(P.x, P.z) + 1.5f); P.vy = 0; P.speed = std::sqrt(vx * vx + vz * vz); P.ground = false; P.swim = false; }
    else {
      float depth = 0; bool wet = (!CI.in && inWater(P.x, P.z, &depth)); P.swim = wet && depth > 1.15f; if (wet && !P.swim) speed *= 1.f - 0.3f * clampf(depth, 0, 1.1f); if (P.swim) speed = 2.2f * hr.move;
      float tvx = (fwdx * mz + rx * mx), tvz = (fwdz * mz + rz * mx); float l = std::sqrt(tvx * tvx + tvz * tvz); if (l > 1.f) { tvx /= l; tvz /= l; } tvx *= speed; tvz *= speed; float k = std::min(1.f, dt * (P.ground ? 11.f : 3.f)); P.vx += (tvx - P.vx) * k; P.vz += (tvz - P.vz) * k;
      float nx = P.x + P.vx * dt, nz = P.z + P.vz * dt; collideAny(nx, nz, 0.42f, P.y); float g0 = gH(P.x, P.z), g1 = gH(nx, nz); bool okStep = !(P.ground && g1 - P.y > 0.55f + 0.0f && (g1 - g0) / std::max(0.05f, std::sqrt(dist2(nx, nz, P.x, P.z))) > 1.15f);
      if (!okStep) { float tx2 = P.x + P.vx * dt, tz2 = P.z; collideAny(tx2, tz2, 0.42f, P.y); float gx = gH(tx2, tz2); if (!(P.ground && gx - P.y > 0.55f && (gx - g0) / std::max(0.05f, std::fabs(tx2 - P.x)) > 1.15f)) { nx = tx2; nz = tz2; } else { float tx3 = P.x, tz3 = P.z + P.vz * dt; collideAny(tx3, tz3, 0.42f, P.y); float gz = gH(tx3, tz3); if (!(P.ground && gz - P.y > 0.55f && (gz - g0) / std::max(0.05f, std::fabs(tz3 - P.z)) > 1.15f)) { nx = tx3; nz = tz3; } else { nx = P.x; nz = P.z; P.vx = P.vz = 0; } } }
      P.x = std::max(-HALF + 6, std::min(HALF - 6, nx)); P.z = std::max(-HALF + 6, std::min(HALF - 6, nz)); P.speed = std::sqrt(P.vx * P.vx + P.vz * P.vz);
      float gnd = gH(P.x, P.z); if (P.swim) { float lvl = waterAt(P.x, P.z); float want = lvl - 1.2f + 0.05f * std::sin(tAbs * 2.2f); P.y += (want - P.y) * std::min(1.f, dt * 6.f); P.vy = 0; P.ground = false; }
      else { if (P.ground) { if (gnd < P.y - 0.02f) { float fall = P.y - gnd; if (fall < 0.7f) P.y = gnd; else { P.ground = false; } } else P.y = gnd; } if (!P.ground) { P.vy -= 15.5f * dt; P.y += P.vy * dt; if (P.y <= gnd) { P.y = gnd; if (P.vy < -14.f) damagePlayer(std::min(40.f, (-P.vy - 14.f) * 4.f), P.x, P.z); P.vy = 0; P.ground = true; } } else if (gnd > P.y) { P.y = gnd; } }
    }
    if (sprint && P.speed > 1.f && !P.fly) P.st = std::max(0.f, P.st - 20.f * dt); else P.st = std::min(100.f, P.st + 16.f * dt);
    if (P.speed > 0.3f) { float want = std::atan2(P.vx, P.vz); P.face += clampf(angDiff(P.face, want), -dt * 11.f, dt * 11.f); } if (P.view == 0 || P.atkT >= 0 || P.block || B.draw > 0 || B.rel >= 0 || B.zoom > 0.3f) P.face += clampf(angDiff(P.face, P.yaw), -dt * 14.f, dt * 14.f);
    if (P.atkT >= 0) { P.atkT += dt; if (!P.hitDone && P.atkT >= P.atkWind) { P.hitDone = true; playerHit(); } if (P.atkT >= P.atkWind + P.atkRec) P.atkT = -1; }
    P.hurtT = std::max(0.f, P.hurtT - dt); P.stagT = std::max(0.f, P.stagT - dt); if (P.regenWait > 0) P.regenWait -= dt; else P.hp = std::min(P.maxHp, P.hp + dt * 2.5f);
  }
  if (!CI.in && prevHour < 20.f && clockH >= 20.f && clockH < 21.f) startKingdomSiege(false);
  prevHour = clockH; arrowsUpdate(dt); cellUpdate(dt); updateKingdom(dt, in); faunaUpdate(dt); townTick(dt);
  for (int k = 0; k < 3; k++) bounty[k] = std::max(0.f, bounty[k] - dt * (huntCount > 0 ? 0.04f : 0.6f));
  { float mb = std::max(bounty[0], std::max(bounty[1], bounty[2])); H.wanted = mb > 12.f ? 1 + (int)(mb / 55.f) : 0; if (H.wanted > 4) H.wanted = 4; }
  H.weapon = B.weapon; H.draw = B.draw; H.arrows = B.arrows; H.gold = gold; H.inside = CI.in; H.zoom = B.zoom; H.hitT = std::max(0.f, H.hitT - dt);
  H.hp = P.hp; H.maxHp = P.maxHp; H.st = P.st; H.yaw = P.yaw; H.fade = P.deadT > 0 ? std::min(1.f, P.deadT / 1.5f) : 0.f;
  // ---- npcs
  huntCount = 0; nMonL = 0; for (int i = 0; i < MAXN; i++) if (npcs[i].used && npcs[i].role >= R_MON) monList[nMonL++] = i; int nCount = 0, mCount = 0;
  for (int i = 0; i < MAXN; i++) { Npc& n = npcs[i]; if (!n.used) continue; float pd = std::sqrt(dist2(n.x, n.z, P.x, P.z)); n.lastPlayerD = pd; if (n.role >= R_MON) mCount++; else nCount++;
    if (!n.alive) { n.deadT += dt; if (n.deadT > 7.f) { if (n.role >= R_MON && n.lair) { n.used = false; } else n.used = false; } continue; } if (n.hitT > 0) n.hitT -= dt; if (n.limp > 0) n.limp -= dt;
    if (!n.lair && n.role == R_MON && pd > 280.f) { n.used = false; continue; } if (pd > 320.f) continue;
    if (n.stun > 0) { n.stun -= dt; n.moving = 0; n.atkT = -1; } else if (n.role >= R_MON) aiMonster(n, dt, pd); else aiHuman(n, dt, pd);
    float gy = n.interior ? xi::BASEY : n.onWall ? n.wallY : surfaceY(n.x, n.z, n.y); float fly = (n.role >= R_MON && gFM[n.mon].flying) ? 1.3f : 0.f; if (n.role < R_MON || !gFM[n.mon].flying) { float dd; if (!n.onWall && !n.interior && inWater(n.x, n.z, &dd) && dd > 0.2f) gy -= std::min(0.5f, dd * 0.5f); } n.y += (gy + fly - n.y) * std::min(1.f, dt * 12.f); }
  H.nNpc = nCount; H.nMon = mCount; if (!CI.in) spawner(dt);
  // lair respawn when the player is far away
  lairT -= dt; if (lairT <= 0) { lairT = 20.f; for (const Lair& L : lairs) { if (dist2(L.x, L.z, P.x, P.z) < 380.f * 380.f) continue; int c = 0; for (int k = 0; k < nMonL; k++) { const Npc& m = npcs[monList[k]]; if (m.alive && m.lair && dist2(m.homeX, m.homeZ, L.x, L.z) < 2500.f) c++; } if (c < 3) { float a = rng() * 6.28f, d = 4 + rng() * 14; float x = L.x + std::cos(a) * d, z = L.z + std::sin(a) * d; if (validSpot(x, z, 0.9f)) spawnMon(L.kind == 1 ? RUSTBOUND : L.kind == 5 ? CAIRNBACK : L.kind >= 2 ? GLOAMWISP : THORNLING, x, z, true, false); } } }
  // ---- discoveries + place name
  for (size_t i = 0; i < pois.size(); i++) { Poi& p = pois[i]; if (p.found) continue; float d = std::sqrt(dist2(p.x, p.z, P.x, P.z)); if (d < p.r && std::fabs(P.y - p.y) < 90.f) { p.found = true; H.disc++; static const char* KIND[10] = {"CAPITAL DISCOVERED", "SETTLEMENT FOUND", "LAKE DISCOVERED", "WATERFALL DISCOVERED", "SUMMIT REACHED", "FORTRESS SIGHTED", "DANGER: CAMP", "RUINS FOUND", "WONDER DISCOVERED", "BORDER CROSSED"}; char sub[40]; snprintf(sub, sizeof sub, "%s  (%d/%d)", p.name, H.disc, H.discTotal); banner(KIND[p.kind % 10], sub, 5.f); audio::play(S_LEVELUP, 0, 0.7f); } }
  if (!CI.in) { int sid = settleNear(P.x, P.z, 6.f); int pid = sid >= 0 ? sid : -1 - nationAt(P.x, P.z) * 100 - (int)(height(P.x, P.z) / 40.f); if (pid != lastPlaceId && sinceStart > 0.5f) { lastPlaceId = pid; if (sid >= 0 && settles[sid].type != ST_BORDER) snprintf(H.place, sizeof H.place, "%s", settles[sid].name); else snprintf(H.place, sizeof H.place, "%s", regionName(P.x, P.z)); snprintf(H.nation, sizeof H.nation, "%s", nations[nationAt(P.x, P.z)].title); H.locT = 4.5f; } if (!H.place[0]) { snprintf(H.place, sizeof H.place, "%s", regionName(P.x, P.z)); snprintf(H.nation, sizeof H.nation, "%s", nations[nationAt(P.x, P.z)].title); } }
  // ---- prompt
  H.prompt[0] = 0; if (!H.map && P.deadT <= 0 && restT <= 0 && !K.buildMode) {
    int ri = nearestRift(6.0f, true);
    int bi = !CI.in ? nearestBarracks(5.5f) : -1;
    Npc* best = nullptr; int bestIdx = -1; float bd = 3.6f * 3.6f;
    for (int i = 0; i < MAXN; i++) { Npc& n = npcs[i]; if (!n.used || !n.alive || n.role >= R_MON) continue; float d = dist2(n.x, n.z, P.x, P.z); if (d < bd) { bd = d; best = &n; bestIdx = i; } }
    if (ri >= 0) snprintf(H.prompt, sizeof H.prompt, "E  PURIFY CORRUPTED RIFT");
    else if (bi >= 0) snprintf(H.prompt, sizeof H.prompt, "E / G  TRAIN SOLDIER (12G 4F)");
    else if (best) {
      if (!CI.in && !KEX[bestIdx].sworn) snprintf(H.prompt, sizeof H.prompt, "E TALK %s   G ENLIST", best->name);
      else snprintf(H.prompt, sizeof H.prompt, "E  TALK TO %s", best->name);
    } else if (CI.in) { const xi::Def& d = xi::DEFS[CI.idx]; float lx = P.x - d.cx, lz = P.z - d.cz; if (CI.stage == 0) { if (lz > d.hz - 2.1f && std::fabs(lx) < 1.8f) snprintf(H.prompt, sizeof H.prompt, "E  LEAVE"); else if (d.chestX != 0 && dist2(lx, lz, d.chestX, d.chestZ) < 2.2f * 2.2f) snprintf(H.prompt, sizeof H.prompt, "%s", looted[CI.house] ? "CHEST  EMPTY" : "E  OPEN CHEST"); else if (CI.idx == 0 && dist2(lx, lz, d.bedX, d.bedZ) < 2.4f * 2.4f) snprintf(H.prompt, sizeof H.prompt, "E  SLEEP IN BED"); } }
    else if (CI.stage == 0) {
      int ei = nearEnter();
      if (ei >= 0) snprintf(H.prompt, sizeof H.prompt, "E  ENTER %s", xi::DEFS[enters[ei].idx].name);
      else {
        bool camp = false; for (const V3& c : campfires) if (dist2(c.x, c.z, P.x, P.z) < 3.8f * 3.8f) { snprintf(H.prompt, sizeof H.prompt, "E  REST BY THE FIRE"); camp = true; break; }
        if (!camp) { GNode* gn = nearestGNode(P.x, P.z, 3.6f); if (gn) { static const char* GT[4] = {"G  CHOP WOOD", "G  MINE STONE", "G  MINE RICH ORE", "G  GATHER FOOD"}; snprintf(H.prompt, sizeof H.prompt, "%s", GT[gn->gtype & 3]); } }
      }
    }
  }
  if (!H.prompt[0] && !H.map && P.deadT <= 0 && restT <= 0 && !K.buildMode) faunaPrompt();
  { float dayF = smooth(5.2f, 7.5f, clockH) * (1.f - smooth(18.2f, 20.5f, clockH)); if (CI.in) dayF *= 0.2f; float wat = 0.f;
    if (!CI.in) { static const float RR[3] = {4.f, 10.f, 22.f}; for (int r = 0; r < 3; r++) for (int k = 0; k < 6; k++) { float a = k * 1.047f; if (inWater(P.x + std::cos(a) * RR[r], P.z + std::sin(a) * RR[r])) wat += 0.9f - 0.25f * r; } wat = clampf(wat / 2.5f, 0.f, 1.f);
      for (const Fall& fl : falls) { float d = std::sqrt(dist2(fl.x, fl.z, P.x, P.z)); wat = std::max(wat, clampf(1.f - d / 90.f, 0.f, 1.f)); } }
    audio::ambience(dayF * (1.f - 0.75f * wxW), std::max(wat, wxW * 0.8f), CI.in ? 0.f : forestAt(P.x, P.z)); }
  // boss bar
  H.bossHp = -1; for (int k = 0; k < nMonL; k++) { const Npc& m = npcs[monList[k]]; if (m.boss && m.alive && m.lastPlayerD < 60.f) { H.bossHp = m.hp / m.maxHp; snprintf(H.bossName, sizeof H.bossName, "%s", m.name); } }
  // ---- camera
  { float cp = std::cos(P.pitch), sp = std::sin(P.pitch); float cyw = P.yaw + (P.view == 0 ? 0.f : dbgSide); V3 look{std::sin(cyw) * cp, sp, std::cos(cyw) * cp}; float head = (pa ? pa->scale : 1.f) * 1.58f + (K.mounted ? (rideH ? riderLift() : 1.18f) : (dmT > 0.f ? riderLift() : 0.f)); V3 pv{P.x, P.y + head, P.z}; V3 eye, tg; float fov = 1.14f + (P.speed > 5.5f ? 0.07f : 0.f);
    if (P.view == 0 && !P.fly) { eye = pv + look * 0.12f; tg = eye + look * 6.f; fov = 1.22f; }
    else { float dist = (CI.in ? std::min(P.camDist, 3.4f) : P.camDist * (K.mounted ? 1.22f : 1.f) * (K.buildMode ? 1.35f : 1.f)) * (P.fly ? 1.8f : 1.f); V3 right{-std::cos(P.yaw), 0, std::sin(P.yaw)}; V3 piv = pv + right * (P.fly ? 0.f : 0.55f) + V3{0, K.buildMode ? 0.65f : 0.1f, 0}; float use = dist; for (float s = 0.5f; s <= dist; s += 0.35f) { V3 p = piv - look * s; float g = gH(p.x, p.z) + 0.45f; float wl = waterAt(p.x, p.z); if (wl > -1e8f) g = std::max(g, wl + 0.25f); float cx = p.x, cz = p.z; bool hit = p.y < g; if (!hit && p.y < gH(p.x, p.z) + 7.f && collideAny(cx, cz, 0.3f) && dist2(cx, cz, p.x, p.z) > 0.25f) hit = true; if (hit) { use = std::max(0.9f, s - 0.45f); break; } }
      eye = piv - look * use; float g = gH(eye.x, eye.z) + 0.5f; if (eye.y < g) eye.y = g; tg = piv + look * 10.f; if (CI.in) { const xi::Def& d = xi::DEFS[CI.idx]; eye.x = clampf(eye.x, d.cx - d.hx + 0.3f, d.cx + d.hx - 0.3f); eye.z = clampf(eye.z, d.cz - d.hz + 0.3f, d.cz + d.hz - 0.3f); eye.y = std::min(eye.y, xi::BASEY + d.h - 0.25f); } }
    if (dbgTrack != -1) { const Fauna& tf = dbgTrack >= 0 ? FA[dbgTrack] : rideF; float hy = ANA[tf.sp].bodyY * tf.sc, a = tf.face + dbgAng; eye = V3{tf.x + std::sin(a) * dbgDist, tf.y + hy * dbgHF, tf.z + std::cos(a) * dbgDist}; eye.y = std::max(eye.y, gH(eye.x, eye.z) + 0.9f); tg = V3{tf.x, tf.y + hy * 0.95f, tf.z}; fov = 0.8f; P.camInit = false; }
    if (!P.camInit) { P.eye = eye; P.tgt = tg; P.camInit = true; } float k = P.view == 0 ? 1.f : 1.f - std::exp(-dt * 16.f); P.eye = P.eye + (eye - P.eye) * k; P.tgt = P.tgt + (tg - P.tgt) * k; P.fov += (fov * (1.f - 0.4f * B.zoom) - P.fov) * std::min(1.f, dt * 5.f); (void)psc; }
}

void draw() {
  static FigDraw fd[260]; int nfd = 0; xr::lastDt = 0.016f; const float dtv = 1.f / 60.f; float psc = 1.f; const FigAsset* pa = roster::cur(); if (pa) psc = pa->scale; const roster::Hero& hr = roster::at(roster::sel());
  bool lampDay = (clockH < 6.2f || clockH > 18.4f) && !CI.in; auto weapon = [&](const FigAsset& rig, const FigAnim& an, bool left, int wp, const M4& um, const float* fl, float dim) { if (wp < 0 || wp >= NWEAP || !gFW[wp].ok || nfd >= 258) return; if ((wp == roster::WP_LANTERN || (wp == roster::WP_STAFF && &rig != roster::cur())) && !lampDay) return; FigDraw& w = fd[nfd++]; M4 m; figWeaponMat(rig, an, left, wp, um, tAbs, m); w.a = &gFW[wp]; w.model = m; w.mat = WID; w.dissolve = 0; w.disH = 1; w.glow = 1; w.shadow = true; w.dim = dim; w.lantern = true; w.vm = false; for (int k = 0; k < 3; k++) { w.flash[k] = fl[k]; w.disCol[k] = 0; } };
  // player
  bool lampOn = (clockH < 6.2f || clockH > 18.4f) && !CI.in; bool bowEq = B.weapon == 1, showBody = P.view == 1 || P.fly, fpvVM = !showBody; static V3 vmRef[2] = {{0.30f, 1.1f, 0.28f}, {-0.30f, 1.1f, 0.28f}};
  H.clean = dbgClean; if (!dbgClean && pa && pa->ok && (showBody || fpvVM)) { const FigAsset& a = *pa; FigAnim& an = animPl; if (an.id != 9999u) { an = FigAnim(); an.id = 9999u; }
    int ac = -1; float au = -1; bool bowAct = bowEq && (B.draw > 0 || B.rel >= 0);
    if (P.atkT >= 0) { static const int K3[3] = {A_BASIC, A_BASH, A_CLEAVE}; ac = figActClip(a, K3[P.combo % 3]); if (ac < 0) ac = figActClip(a, A_BASIC); float hit = ac >= 0 && a.clips[ac].hit > 0.01f ? a.clips[ac].hit : 0.5f; if (ac >= 0) au = P.atkT < P.atkWind ? P.atkT / std::max(0.01f, P.atkWind) * hit : std::min(1.f, hit + (P.atkT - P.atkWind) / std::max(0.01f, P.atkRec) * (1 - hit)); }
    else if (bowAct) { ac = figActClip(a, A_AIMED); if (ac >= 0) { float hit = a.clips[ac].hit > 0.01f ? a.clips[ac].hit : 0.5f; au = B.draw > 0 ? B.draw * hit : std::min(1.f, hit + B.rel / 0.4f * (1 - hit)); } }
    bool combat = P.atkT >= 0 || P.block || P.hurtT > 0 || bowAct || (bowEq && B.zoom > 0.3f); float spd = K.mounted ? 0.f : (P.swim ? P.speed * 0.8f : P.speed); figAnimUpdate(an, a, dtv, spd / a.scale, combat, ac, au, P.stagT > 0.1f ? 1.f : 0.f, P.deadT > 0, P.block ? 1.f : 0.f, -1.f, 0.f);
    if (P.swim) figAddMove(an, a, 0, 0, -0.5f, 0); if (!K.mounted) fxPlayerAir(an, a, P.ground, P.vy, P.swim || P.fly || CI.in, dtv); if (dbgFx >= 0 && P.speed < 0.5f) fxApply(an, a, dbgFx, tAbs, 1.f, 0.f);
    if ((K.mounted || dmT > 0.f) && !CI.in) { float pk = rideH || dmT > 0.f ? riderPose() : 1.f;
      int lt = figBone(a, "lThigh", -1), ls = figBone(a, "lShin", -1), rt = figBone(a, "rThigh", -1), rs2 = figBone(a, "rShin", -1);
      auto rotB = [&](int bi, float ax, float ay, float az, float ang) { if (bi < 0) return; float pp[3] = {an.pose[bi * 7 + 4], an.pose[bi * 7 + 5], an.pose[bi * 7 + 6]}; figAddRot(an, a, bi, ax, ay, az, ang, pp); };
      rotB(lt, 1, 0, 0, -0.95f * pk); rotB(lt, 0, 0, 1, 0.32f * pk); rotB(ls, 1, 0, 0, 0.88f * pk);
      rotB(rt, 1, 0, 0, -0.95f * pk); rotB(rt, 0, 0, 1, -0.32f * pk); rotB(rs2, 1, 0, 0, 0.88f * pk);
    }
    figAnimPose(an, a); float fl = P.hurtT > 0 ? P.hurtT * 0.5f : 0.f; float flash[3] = {fl * 1.2f, fl * 0.5f, fl * 0.3f}; int wR = bowEq ? -1 : hr.wpR, wL = bowEq ? roster::WP_BOW : hr.wpL;
    if (showBody && K.mounted && !rideH && !CI.in && gFM[HOLLOWSTAG].ok && nfd < 256) {
      const FigAsset& ma = gFM[HOLLOWSTAG]; float msc = std::max(1.55f, ma.scale * 0.76f);
      if (K.mountAnim.id != 8888u) { K.mountAnim = FigAnim(); K.mountAnim.id = 8888u; }
      figAnimUpdate(K.mountAnim, ma, dtv, P.speed / std::max(0.8f, msc * 0.65f), false, -1, -1.f, 0.f, false, 0.f, -1.f, 0.f); figAnimPose(K.mountAnim, ma);
      FigDraw& md = fd[nfd++]; md.a = &ma; md.mat = K.mountAnim.mat; md.dissolve = 0; md.disH = std::max(1.f, ma.height); md.glow = 1.4f; md.shadow = true; md.dim = 1.08f; md.lantern = false; md.vm = false;
      for (int k = 0; k < 3; k++) { md.disCol[k] = 0; md.flash[k] = flash[k] * 0.5f; }
      md.model = mul(translate(P.x, P.y, P.z), mul(rotY(P.face), scale4(msc, msc, msc)));
    }
    if (showBody) { float rideBob = CI.in ? 0.f : K.mounted ? (rideH ? riderLift() : (1.18f + 0.045f * std::sin(tAbs * (P.speed > 2.f ? 11.f : 2.f)))) : (dmT > 0.f ? riderLift() : 0.f);
      FigDraw& d = fd[nfd++]; d.a = &a; d.mat = an.mat; d.dissolve = 0; d.disH = std::max(1.f, a.height); d.glow = 1; d.shadow = true; d.dim = 1; d.lantern = a.lant && lampOn; d.vm = false; for (int k = 0; k < 3; k++) d.disCol[k] = a.disCol[k]; for (int k = 0; k < 3; k++) d.flash[k] = flash[k];
      d.model = mul(translate(P.x, P.y + rideBob, P.z), mul(rotY(P.face), scale4(a.scale, a.scale, a.scale))); weapon(a, an, false, wR, d.model, d.flash, 1.f); weapon(a, an, true, wL, d.model, d.flash, 1.f); }
    else {   // first person: camera-space held bow or melee weapon
      V3 look = norm(P.tgt - P.eye), xx = norm(cross(V3{0, 1, 0}, look)), yy = cross(look, xx); M4 cam = ident(); cam.m[0] = xx.x; cam.m[1] = xx.y; cam.m[2] = xx.z; cam.m[4] = yy.x; cam.m[5] = yy.y; cam.m[6] = yy.z; cam.m[8] = look.x; cam.m[9] = look.y; cam.m[10] = look.z;
      if (bowEq && gFW[roster::WP_BOW].ok && nfd < 258 && !K.buildMode) {
        float pull = B.draw, recoil = B.rel >= 0 ? std::sin(clampf(B.rel / 0.25f, 0, 1) * PI) * 0.06f : 0.f;
        float swayX = 0.008f * std::sin(tAbs * 1.9f), swayY = 0.008f * std::cos(tAbs * 1.5f);
        V3 anc = P.eye + xx * (0.20f + swayX) + yy * (-0.72f + swayY) + look * (0.66f - 0.05f * pull + recoil);
        float cant = -0.34f; float cc = std::cos(cant), sc2 = std::sin(cant);
        M4 rotC = ident(); rotC.m[0] = cc; rotC.m[1] = sc2; rotC.m[4] = -sc2; rotC.m[5] = cc;
        M4 w2 = mul(translate(anc.x, anc.y, anc.z), mul(cam, mul(rotC, scale4(0.88f, 0.88f, 0.88f))));
        FigDraw& w = fd[nfd++]; w.a = &gFW[roster::WP_BOW]; w.model = w2; w.mat = WID; w.dissolve = 0; w.disH = 1; w.glow = 1; w.shadow = false; w.dim = 1.f; w.lantern = false; w.vm = true;
        for (int k = 0; k < 3; k++) { w.flash[k] = flash[k]; w.disCol[k] = 0; }
      } else {
        M4 um0 = scale4(a.scale * 0.85f, a.scale * 0.85f, a.scale * 0.85f); bool idle = P.atkT < 0 && !bowAct;
        auto vmW = [&](bool left, int wp, V3 off) { if (wp < 0 || wp >= NWEAP || !gFW[wp].ok || nfd >= 258) return; M4 m; figWeaponMat(a, an, left, wp, um0, tAbs, m); V3& ref = vmRef[left ? 0 : 1]; V3 cur{m.m[12], m.m[13], m.m[14]}; if (idle) ref = ref + (cur - ref) * 0.15f;
          V3 anc = P.eye + xx * off.x + yy * off.y + look * off.z; M4 w2 = mul(translate(anc.x, anc.y, anc.z), mul(cam, mul(translate(-ref.x, -ref.y, -ref.z), m))); FigDraw& w = fd[nfd++]; w.a = &gFW[wp]; w.model = w2; w.mat = WID; w.dissolve = 0; w.disH = 1; w.glow = 1; w.shadow = false; w.dim = 1.f; w.lantern = false; w.vm = true; for (int k = 0; k < 3; k++) { w.flash[k] = flash[k]; w.disCol[k] = 0; } };
        vmW(true, wL, V3{0.34f, -0.26f, 0.68f}); vmW(false, wR, V3{-0.30f, -0.34f, 0.70f});
      }
    } }
  // npcs within range (nearest first by cell; capped)
  V3 cf = norm(P.tgt - P.eye); int drawn = 0; for (int pass = 0; pass < 2; pass++) for (int i = 0; i < MAXN && nfd < 230; i++) { Npc& n = npcs[i]; if (!n.used) continue; n.vis = false; if (n.asleep) continue; float dx = n.x - P.eye.x, dz = n.z - P.eye.z, d2 = dx * dx + dz * dz; float lim = n.role >= R_MON ? 150.f : 125.f; if (n.boss) lim = 320.f; if (d2 > lim * lim) continue; float d = std::sqrt(d2) + 1e-3f; if (d > 12.f && (dx * cf.x + dz * cf.z) / d < -0.05f) continue; if ((pass == 0) != (d < 60.f)) continue; if (drawn > 150) continue; drawn++; n.vis = true;
    const FigAsset* a; int wR = -1, wL = -1; if (n.role >= R_MON) a = &gFM[n.mon]; else { const RigDef& rd = rigs[n.rig]; a = rd.a; wR = rd.wR; wL = rd.wL; } if (!a || !a->ok) continue; FigAnim& an = n.an; int ac = -1; float au = -1;
    if (n.atkT >= 0 && n.alive) { ac = figActClip(*a, n.role >= R_MON ? A_MONATK : A_BASIC); if (ac >= 0) { float hit = a->clips[ac].hit > 0.01f ? a->clips[ac].hit : 0.5f; au = n.atkT < n.atkWind ? n.atkT / std::max(0.01f, n.atkWind) * hit : std::min(1.f, hit + (n.atkT - n.atkWind) / std::max(0.01f, n.atkRec) * (1 - hit)); } }
    bool combat = n.role >= R_MON ? (n.tgtKind != 0 && n.alive) : (n.state == S_CHASE || n.state == S_ATK); float spd = n.alive ? n.moving : 0.f; if (n.role == R_VILL && n.state == S_FLEE) combat = false;
    figAnimUpdate(an, *a, dtv, spd / std::max(0.3f, n.sc / std::max(0.01f, a->scale) * a->scale), combat, ac, au, n.hitT > 0.05f ? 1.f : 0.f, !n.alive, 0.f, -1.f, 0.f);
    if (n.alive && n.role < R_MON && n.hitT < 0.05f && n.state != S_FLEE && n.atkT < 0 && !combat) { int fm = dbgFx >= 0 ? dbgFx : fxPick(n, i, n.settle >= 0 ? SCH[i].phase : -1, tAbs); float iw = (1.f - an.moveW) * (1.f - an.readyW); if (fm != an.fxMode) { an.fxW -= dtv * 3.f; if (an.fxW <= 0.f) { an.fxW = 0.f; an.fxMode = fm; } } else an.fxW = std::min(1.f, an.fxW + dtv * 3.f); fxApply(an, *a, an.fxMode, tAbs, an.fxW * iw, (float)(i % 17) * 0.9f); }
    figAnimPose(an, *a);
    FigDraw& fdd = fd[nfd++]; fdd.a = a; fdd.mat = an.mat; fdd.dissolve = n.alive ? 0.f : clampf((n.deadT - 4.5f) / 2.f, 0, 1); fdd.disH = std::max(1.f, a->height); fdd.glow = n.role >= R_MON ? (n.boss ? 3.f : 2.2f) : 1.f; fdd.shadow = d < 70.f; fdd.dim = 1.f; fdd.lantern = a->lant && lampOn && ((i & 1) == 0); fdd.vm = false; for (int k = 0; k < 3; k++) fdd.disCol[k] = a->disCol[k];
    float fl = n.hitT * n.hitT * 4.f; fdd.flash[0] = fl * 1.2f; fdd.flash[1] = fl * 0.7f; fdd.flash[2] = fl * 0.4f; float sc = n.sc; fdd.model = mul(translate(n.x, n.y, n.z), mul(rotY(n.face), scale4(sc, sc, sc)));
    if (n.role < R_MON) { weapon(*a, an, false, wR, fdd.model, fdd.flash, 1.f); weapon(*a, an, true, wL, fdd.model, fdd.flash, 1.f); } }
  xr::Frame f; f.eye = P.eye; f.tgt = P.tgt; f.fov = P.fov; f.hour = clockH; f.time = tAbs; f.flash = lightT * lightT * 0.55f; f.fade = 1.f - H.fade * 0.85f * (P.deadT > 0 ? 1.f : 0.f) - (restT > 0 ? 0.6f * std::sin(clampf(restT / 3.2f, 0, 1) * PI) : 0.f);
  static xr::Dyn dyn[1400]; static xr::PtLight dlights[24]; int ndl = 0;
  int nd = buildDyn(dyn, 96);
  if (B.weapon == 1 && P.view == 0 && !P.fly && !K.buildMode && !CI.stage && B.arrows > 0 && B.rel < 0.1f && nd < 380) {
    V3 lk = norm(P.tgt - P.eye); V3 xx = norm(cross(V3{0, 1, 0}, lk)); V3 yy = cross(lk, xx); float zz = 0.52f - 0.1f * B.draw; V3 pos = P.eye + xx * 0.16f + yy * -0.13f + lk * zz;
    M4 m = ident(); m.m[0] = xx.x * 0.4f; m.m[1] = xx.y * 0.4f; m.m[2] = xx.z * 0.4f; m.m[4] = yy.x * 0.4f; m.m[5] = yy.y * 0.4f; m.m[6] = yy.z * 0.4f; m.m[8] = lk.x; m.m[9] = lk.y; m.m[10] = lk.z; m.m[12] = pos.x; m.m[13] = pos.y; m.m[14] = pos.z;
    dyn[nd].kind = K_ARROW; dyn[nd].mode = 0.f; dyn[nd].m = m; dyn[nd].tint[0] = dyn[nd].tint[1] = dyn[nd].tint[2] = 1.f; nd++;
  }
  nd = appendKingdomDyn(dyn, nd, 700, dlights, ndl, 24); nd = appendFaunaDyn(dyn, nd, 1390);
  f.dyn = dyn; f.ndyn = nd; f.dlights = dlights; f.ndlights = ndl; f.fade *= 1.f - CI.fade; xr::indoor = CI.in ? 1.f : 0.f;
  xr::render(f, fd, nfd);
  // labels (names / health bars)
  nLabels = 0;
  for (int i = 0; i < MAXN && nLabels < 24; i++) {
    Npc& n = npcs[i]; if (!n.used || !n.alive || !n.vis) continue;
    bool mon = n.role >= R_MON, sw = KEX[i].sworn;
    float d = n.lastPlayerD;
    if (mon ? (d > 55.f || (n.role == R_STAG && d > 14.f)) : (d > (sw ? 24.f : 7.5f))) continue;
    if (mon && n.tgtKind == 0 && !KEX[i].siegeUnit && n.hp >= n.maxHp && d > 24.f) continue;
    float sx, sy; const FigAsset* a = mon ? &gFM[n.mon] : rigs[n.rig].a; float hy = a ? a->hpY * n.sc / std::max(0.01f, a->scale) + 0.35f : 2.2f;
    if (!xr::project(n.x, n.y + hy, n.z, sx, sy)) continue;
    Label& L = labels[nLabels++]; L.x = sx; L.y = sy;
    if (mon) { snprintf(L.text, sizeof L.text, "%s", n.name); L.hp = n.hp / n.maxHp; L.col = n.boss ? 0xff8a50 : (n.role == R_MON ? 0xff6a5a : 0xd8d0b0); }
    else if (sw) { snprintf(L.text, sizeof L.text, "SWORN %s", n.name); L.hp = n.hp < n.maxHp * 0.99f ? n.hp / n.maxHp : -1.f; L.col = 0x7ad8ff; }
    else { snprintf(L.text, sizeof L.text, "%s", n.name); L.hp = n.hp < n.maxHp ? n.hp / n.maxHp : -1.f; L.col = n.angry > 0 ? 0xff7a6a : n.role == R_VILL ? 0xe8e0c8 : 0x9ac8ff; }
  }
  for (int i = 0; i < MAXB && nLabels < 30; i++) {
    const BuildObj& b = builds[i]; if (!b.used || b.hp >= b.maxHp * 0.99f) continue;
    if (dist2(b.x, b.z, P.x, P.z) > 42.f * 42.f) continue;
    float sx, sy; if (!xr::project(b.x, b.y + 4.2f, b.z, sx, sy)) continue;
    Label& L = labels[nLabels++]; L.x = sx; L.y = sy; snprintf(L.text, sizeof L.text, "%s", BDEFS[b.btype].shortN); L.hp = b.hp / b.maxHp; L.col = 0xf4d28a;
  }
  for (DmgTxt& x : DT) { if (x.t < 0) continue; x.t += 1.f / 60.f; if (x.t > 1.1f) { x.t = -1; continue; } float sx, sy; if (nLabels < 42 && xr::project(x.x, x.y + x.t * 0.8f, x.z, sx, sy)) { Label& L = labels[nLabels++]; L.x = sx; L.y = sy - x.t * 8.f; memcpy(L.text, x.txt, sizeof x.txt); L.text[19] = 0; L.hp = -1; L.col = x.col; } }
  float S = std::max(1.f, std::round(gfx::winH / 360.f)); float W = (float)gfx::winW, Hh = (float)gfx::winH;
  if (H.map) { float size = std::min(Hh * 0.86f, W * 0.7f); H.mapSize = size; H.mapPx = (W - size) / 2; H.mapPy = (Hh - size) / 2; xr::MapView mv{0, 0, HALF, 0, 1, H.mapPx, H.mapPy, size, P.yaw}; xr::drawMap(mv); float sx, sy; mapToScreen(P.x, P.z, sx, sy); (void)sx; (void)sy; }
  else if (H.show && !CI.in) { float size = 118.f * S; H.mapSize = size; H.mapPx = W - size - 8 * S; H.mapPy = 24 * S; float rot = PI - P.yaw; xr::MapView mv{P.x, P.z, 170.f, rot, 0, H.mapPx, H.mapPy, size, -rot}; xr::drawMap(mv); }
  else if (H.show && CI.in) { H.mapSize = 118.f * S; H.mapPx = W - H.mapSize - 8 * S; H.mapPy = 24 * S; }
  (void)psc;
}
}  // namespace ex
