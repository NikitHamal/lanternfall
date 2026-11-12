// Explorer world generator (see xworld.h). Pure CPU, deterministic from the seed, ~1 s at start.
#include "xworld.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
namespace xw {
std::vector<float> Hh, Wl; std::vector<uint16_t> waterTex; std::vector<int16_t> Wq; std::vector<uint8_t> roadQ, ecoF; std::vector<uint8_t> infoA, infoB, mapRGBA;
std::vector<Obj> objs; std::vector<Settle> settles; Nation nations[3]; std::vector<Fall> falls; std::vector<Poi> pois; std::vector<Light> lights; std::vector<Post> posts; std::vector<Road> roads; std::vector<Lair> lairs;
static std::vector<float> flowX, flowZ, roadD, gradeW, gradeY; static std::vector<uint8_t> fieldK; static std::vector<float> fieldA;
static Rng rng(1); static uint32_t SEED = 1; static bool done = false;
struct Col_ { float x, z, r, top; }; static float curTop = 1e9f; struct Deck { float x, z, c, s, hx, hz, y0, y1; }; static std::vector<Deck> decks; static std::vector<int> deckStart, deckList; int nDecks = 0; static std::vector<Col_> cols; static std::vector<int> colStart, colList; constexpr int CGN = 256; constexpr float CGS = 16.f;

// ------------------------------------------------------------------ noise
static uint32_t ih(int x, int y, uint32_t s) { uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + s * 2246822519u; h = (h ^ (h >> 13)) * 1274126177u; return h ^ (h >> 16); }
static float rn(int x, int y, uint32_t s) { return (ih(x, y, s) & 0xffffffu) / 16777216.f; }
static float vn(float x, float y, uint32_t s) { float fx = std::floor(x), fy = std::floor(y); int ix = (int)fx, iy = (int)fy; float tx = x - fx, ty = y - fy; tx = tx * tx * (3 - 2 * tx); ty = ty * ty * (3 - 2 * ty);
  return lerpf(lerpf(rn(ix, iy, s), rn(ix + 1, iy, s), tx), lerpf(rn(ix, iy + 1, s), rn(ix + 1, iy + 1, s), tx), ty); }
static float fb(float x, float y, int o, uint32_t s) { float a = 0.5f, sum = 0, n = 0; for (int i = 0; i < o; i++) { sum += a * vn(x, y, s + i * 17); n += a; x = x * 2.03f + 11.1f; y = y * 2.03f + 7.7f; a *= 0.5f; } return sum / n; }
static float ridge(float x, float y, int o, uint32_t s) { float a = 0.5f, sum = 0, n = 0; for (int i = 0; i < o; i++) { float v = 1 - std::fabs(2 * vn(x, y, s + i * 13) - 1); v *= v; sum += a * v; n += a; x = x * 2.1f + 5.3f; y = y * 2.1f + 1.9f; a *= 0.55f; } return sum / n; }
static float blob(float nx, float nz, float cx, float cz, float R) { float d = std::sqrt((nx - cx) * (nx - cx) + (nz - cz) * (nz - cz)); return smooth(R, R * 0.2f, d); }
static float smin(float a, float b, float k) { float h = std::max(k - std::fabs(a - b), 0.f) / k; return std::min(a, b) - h * h * k * 0.25f; }
static inline int ci(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline int cellOf(float v) { return (int)std::floor((v + HALF) / CS); }
static float urand(float a, float b) { return a + (b - a) * rng(); }

// ------------------------------------------------------------------ queries
float height(float x, float z) {
  float fx = (x + HALF) / CS - 0.5f, fz = (z + HALF) / CS - 0.5f; int ix = (int)std::floor(fx), iz = (int)std::floor(fz); float tx = fx - ix, tz = fz - iz;
  int x0 = ci(ix, 0, GS - 1), x1 = ci(ix + 1, 0, GS - 1), z0 = ci(iz, 0, GS - 1), z1 = ci(iz + 1, 0, GS - 1);
  return lerpf(lerpf(Hh[z0 * GS + x0], Hh[z0 * GS + x1], tx), lerpf(Hh[z1 * GS + x0], Hh[z1 * GS + x1], tx), tz);
}
float slopeAt(float x, float z) { float e = 2.5f; float dx = height(x + e, z) - height(x - e, z), dz = height(x, z + e) - height(x, z - e); return std::sqrt(dx * dx + dz * dz) / (2 * e); }
float waterAt(float x, float z) { int ix = ci(cellOf(x), 0, GS - 1), iz = ci(cellOf(z), 0, GS - 1); if (!Wq.empty()) { int16_t q = Wq[iz * GS + ix]; return q == -32768 ? -1e9f : q * (1.f / 32.f); } return Wl[iz * GS + ix]; }
bool inWater(float x, float z, float* depth) { float w = waterAt(x, z); if (w < -1e8f) return false; float d = w - height(x, z); if (depth) *depth = d; return d > 0.f; }
float roadDist(float x, float z) { int ix = ci(cellOf(x), 0, GS - 1), iz = ci(cellOf(z), 0, GS - 1); if (!roadQ.empty()) return roadQ[iz * GS + ix] * 0.125f; return roadD[iz * GS + ix]; }
bool flowery(float x, float z) { if (ecoF.empty()) return false; int ix = ci((int)((x + HALF) / (CS * 4)), 0, GS / 4 - 1), iz = ci((int)((z + HALF) / (CS * 4)), 0, GS / 4 - 1); return ecoF[iz * (GS / 4) + ix] != 0; }
void releaseGen() { std::vector<uint8_t>().swap(infoA); std::vector<uint8_t>().swap(infoB); std::vector<uint8_t>().swap(mapRGBA); std::vector<uint16_t>().swap(waterTex); }
int nationAt(float x, float z) { int b = 0; float bd = 1e30f; for (int i = 0; i < 3; i++) { float d = (x - nations[i].x) * (x - nations[i].x) + (z - nations[i].z) * (z - nations[i].z); if (d < bd) { bd = d; b = i; } } return b; }
int settleNear(float x, float z, float margin) { for (size_t i = 0; i < settles.size(); i++) { const Settle& s = settles[i]; float d = std::sqrt((x - s.x) * (x - s.x) + (z - s.z) * (z - s.z)); if (d < s.r + margin) return (int)i; } return -1; }
float highestPoint(float* hx, float* hz) { float best = -1e9f; int bi = 0; for (int i = 0; i < GS * GS; i++) if (Hh[i] > best) { best = Hh[i]; bi = i; } if (hx) *hx = -HALF + (bi % GS + 0.5f) * CS; if (hz) *hz = -HALF + (bi / GS + 0.5f) * CS; return best; }
static float forestMask(float x, float z, float h) {
  float f = fb(x * 0.0045f + 10, z * 0.0045f, 4, SEED + 40); float m = smooth(0.46f, 0.60f, f); m *= 1.f - smooth(85.f, 118.f, h); m = std::max(m, 0.03f + 0.1f * smooth(0.38f, 0.46f, f)); return h < 1.5f ? 0.f : m;
}
void zones(float x, float z, float h, float& ch, float& jg, float& bi, float& au) {
  float q = fb(x * 0.0022f + 50, z * 0.0022f, 3, SEED + 41); float low = smooth(60.f, 28.f, h);
  ch = (1.f - smooth(0.27f, 0.36f, q)) * low; au = smooth(0.30f, 0.34f, q) * (1.f - smooth(0.40f, 0.44f, q));
  jg = smooth(0.57f, 0.67f, fb(x * 0.0019f - 60, z * 0.0019f + 90, 3, SEED + 82)) * smooth(36.f, 14.f, h) * smooth(-120.f, 160.f, z) * (1.f - ch);
  bi = smooth(0.50f, 0.60f, fb(x * 0.0023f + 7, z * 0.0023f + 300, 3, SEED + 83)) * low * (1.f - ch) * (1.f - jg);
}
float forestAt(float x, float z) { return forestMask(x, z, height(x, z)); }
const char* regionName(float x, float z) {
  float h = height(x, z); int n = nationAt(x, z); float fm = forestMask(x, z, h); float q = fb(x * 0.0022f + 50, z * 0.0022f, 3, SEED + 41);
  static char b[48]; const char* nm = nations[n].name;
  if (h > 95) snprintf(b, 48, "THE IRONSPINE PEAKS"); else if (h > 55) snprintf(b, 48, "%s HIGHLANDS", nm); else if (fm > 0.6f && q < 0.30f) snprintf(b, 48, "BLOSSOM VALE"); else if (fm > 0.6f) snprintf(b, 48, "%s WOODS", nm);
  else if (h < 4.5f && z > 200) snprintf(b, 48, "SALT COAST"); else snprintf(b, 48, "%s HEARTLANDS", nm);
  for (char* c = b; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32; return b;
}

// ------------------------------------------------------------------ colliders
static void addCircle(float x, float z, float r) { cols.push_back({x, z, r, curTop}); }
static void addLocal(float cx, float cz, float ry, float lx, float lz, float r, float s = 1) { float c = std::cos(ry), sn = std::sin(ry); addCircle(cx + (lx * c + lz * sn) * s, cz + (-lx * sn + lz * c) * s, r * s); }
static void addDeck(float cx, float cz, float ry, float lx, float lz, float hx, float hz, float y0, float y1) {   // local frame like addLocal; centre offset (lx,lz)
  float c = std::cos(ry), sn = std::sin(ry); decks.push_back({cx + lx * c + lz * sn, cz - lx * sn + lz * c, c, sn, hx, hz, y0, y1}); }
static void addCollider(int k, float x, float z, float ry, float sx, float oy) {
  curTop = 1e9f;
  switch (k) {
    case K_WALL: curTop = oy + 5.2f * sx; for (int i = -1; i <= 1; i++) addLocal(x, z, ry, i * 2.2f, 0, 1.45f, sx); curTop = 1e9f; return;
    case K_STAIR: { for (int i = 0; i < 6; i++) { float lx = 4.f + i * 2.f; float top = 0.227f + 0.474f * lx; curTop = oy + std::min(5.9f, top) - 1.3f; addLocal(x, z, ry, lx, 0, 1.2f); } curTop = oy + 4.6f; addLocal(x, z, ry, 13.1f, 0, 1.4f); curTop = 1e9f; return; }
    case K_BIRCH: addCircle(x, z, 0.3f * sx); break; case K_CHERRY: addCircle(x, z, 0.4f * sx); break; case K_JUNGLE: addCircle(x, z, 0.9f * sx); break; case K_WILLOW: addCircle(x, z, 0.55f * sx); break; case K_PALM: addCircle(x, z, 0.25f * sx); break; case K_SPIRE: addCircle(x, z, 1.1f * sx); break;
    case K_PINE: addCircle(x, z, 0.5f * sx); break; case K_PINEY: addCircle(x, z, 0.3f * sx); break; case K_OAK: addCircle(x, z, 0.55f * sx); break; case K_DEAD: addCircle(x, z, 0.4f * sx); break;
    case K_BOULDER: addCircle(x, z, 1.0f * sx); break; case K_BIGROCK: addCircle(x, z, 0.85f * sx); break; case K_CAIRN: addCircle(x, z, 0.6f * sx); break; case K_WELL: addCircle(x, z, 1.0f * sx); break;
    case K_STALL: addLocal(x, z, ry, 0, 0, 0.8f); break; case K_BRAZIER: addCircle(x, z, 0.4f); break; case K_LAMP: addCircle(x, z, 0.22f); break; case K_POLE: case K_FLAGPOLE: addCircle(x, z, 0.4f); break;
    case K_BARREL: addCircle(x, z, 0.35f * sx); break; case K_CRATE: addCircle(x, z, 0.45f * sx); break; case K_HEAD: addCircle(x, z, 0.33f * sx); break; case K_RUNE: addCircle(x, z, 0.35f * sx); break;
    case K_CGATE: addLocal(x, z, ry, -1.8f, 0, 0.35f, sx); addLocal(x, z, ry, 1.8f, 0, 0.35f, sx); break;
    case K_LWALL: addLocal(x, z, ry, 0, 0, 0.8f, sx); break; case K_FWALL: case K_IFENCE: addLocal(x, z, ry, -1, 0, 0.5f, sx); addLocal(x, z, ry, 1, 0, 0.5f, sx); break;
    case K_COTTAGE: for (int i = -1; i <= 1; i++) addLocal(x, z, ry, i * 2.0f, 0, 2.1f, sx); break;
    case K_BARN: for (int i = -1; i <= 1; i++) addLocal(x, z, ry, 0, i * 2.4f, 2.1f, sx); break;
    case K_CHAPEL: for (int i = -1; i <= 1; i++) addLocal(x, z, ry, 0, i * 2.2f, 2.3f, sx); break;
    case K_WATCH: addLocal(x, z, ry, 0, 0, 2.4f, sx); break; case K_STOWER: case K_BEACON: case K_RGABLE: addLocal(x, z, ry, 0, 0, 2.5f, sx); break;
    case K_RWALL: addLocal(x, z, ry, -1.2f, 0, 1.0f, sx); addLocal(x, z, ry, 1.2f, 0, 1.0f, sx); break;
    case K_WTOWER: addCircle(x, z, 4.45f * sx); break;
    case K_GATE: addLocal(x, z, ry, -4.6f, 0, 2.7f, sx); addLocal(x, z, ry, 4.6f, 0, 2.7f, sx); addLocal(x, z, ry, -3.6f, 0, 1.4f, sx); addLocal(x, z, ry, 3.6f, 0, 1.4f, sx); break;
    case K_KEEP: for (int i = -2; i <= 2; i++) for (int j = -2; j <= 2; j++) if (std::max(std::abs(i), std::abs(j)) == 2) addLocal(x, z, ry, i * 4.4f, j * 4.4f, 3.5f, sx); break;   // v2.8: ring over the whole 22 m footprint (was one 8.6 m circle)
    case K_HOUSE: for (int i = -1; i <= 2; i++) for (int j = -1; j <= 1; j += 2) addLocal(x, z, ry, i * 2.0f - 1.0f, j * 1.45f, 2.55f, sx); break;
    case K_LONG: for (int i = -2; i <= 2; i++) for (int j = -1; j <= 1; j += 2) addLocal(x, z, ry, i * 2.6f, j * 1.7f, 2.9f, sx); break;
    case K_LOG: addLocal(x, z, ry, -1.2f, 0, 0.5f, sx); addLocal(x, z, ry, 0, 0, 0.5f, sx); addLocal(x, z, ry, 1.2f, 0, 0.5f, sx); break; case K_GRAVE: addCircle(x, z, 0.4f * sx); break; case K_THORN: addCircle(x, z, 0.55f * sx); break;
    case K_SFENCE: addLocal(x, z, ry, -0.9f, 0, 0.4f, sx); addLocal(x, z, ry, 0.9f, 0, 0.4f, sx); break; case K_TORCH: case K_SIGN: addCircle(x, z, 0.2f * sx); break; case K_ORE: addCircle(x, z, 0.9f * sx); break;
    case K_PALI: addLocal(x, z, ry, -1.f, 0, 0.7f, sx); addLocal(x, z, ry, 1.f, 0, 0.7f, sx); break; case K_TENT: addCircle(x, z, 2.1f * sx); break; case K_HAY: addCircle(x, z, 1.55f * sx); break; case K_MILL: addCircle(x, z, 3.2f * sx); break;
    default: break;
  }
}
static void buildColGrid() {
  colStart.assign(CGN * CGN + 1, 0); std::vector<int> cnt(CGN * CGN, 0); auto cell = [](float v) { return ci((int)std::floor((v + HALF) / CGS), 0, CGN - 1); };
  for (const Col_& c : cols) { int x0 = cell(c.x - c.r), x1 = cell(c.x + c.r), z0 = cell(c.z - c.r), z1 = cell(c.z + c.r); for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++) cnt[z * CGN + x]++; }
  for (int i = 0; i < CGN * CGN; i++) colStart[i + 1] = colStart[i] + cnt[i]; colList.assign(colStart[CGN * CGN], 0); std::vector<int> fill(colStart.begin(), colStart.end() - 1);
  { deckStart.assign(CGN * CGN + 1, 0); std::vector<int> dc(CGN * CGN, 0); auto bb = [&](const Deck& d, int& x0, int& x1, int& z0, int& z1) { float R = std::sqrt(d.hx * d.hx + d.hz * d.hz); x0 = cell(d.x - R); x1 = cell(d.x + R); z0 = cell(d.z - R); z1 = cell(d.z + R); };
    for (const Deck& d : decks) { int x0, x1, z0, z1; bb(d, x0, x1, z0, z1); for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++) dc[z * CGN + x]++; }
    for (int i = 0; i < CGN * CGN; i++) deckStart[i + 1] = deckStart[i] + dc[i]; deckList.assign(deckStart[CGN * CGN], 0); std::vector<int> df(deckStart.begin(), deckStart.end() - 1);
    for (size_t k = 0; k < decks.size(); k++) { int x0, x1, z0, z1; bb(decks[k], x0, x1, z0, z1); for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++) deckList[df[z * CGN + x]++] = (int)k; } nDecks = (int)decks.size(); }
  for (size_t k = 0; k < cols.size(); k++) { const Col_& c = cols[k]; int x0 = cell(c.x - c.r), x1 = cell(c.x + c.r), z0 = cell(c.z - c.r), z1 = cell(c.z + c.r); for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++) colList[fill[z * CGN + x]++] = (int)k; }
}
static inline float deckY(const Deck& d, float x, float z) { float dx = x - d.x, dz = z - d.z; float lx = dx * d.c - dz * d.s, lz = dx * d.s + dz * d.c; if (std::fabs(lx) > d.hx || std::fabs(lz) > d.hz) return -1e9f; return d.y0 + (d.y1 - d.y0) * (lx + d.hx) / (2.f * d.hx); }
float surfaceY(float x, float z, float fromY) {
  float best = height(x, z); if (deckStart.empty()) return best; int cx = ci((int)std::floor((x + HALF) / CGS), 0, CGN - 1), cz = ci((int)std::floor((z + HALF) / CGS), 0, CGN - 1);
  for (int q = deckStart[cz * CGN + cx]; q < deckStart[cz * CGN + cx + 1]; q++) { float t = deckY(decks[deckList[q]], x, z); if (t > best && t <= fromY + 0.75f) best = t; } return best; }
float deckTop(float x, float z) { float best = -1e9f; if (deckStart.empty()) return best; int cx = ci((int)std::floor((x + HALF) / CGS), 0, CGN - 1), cz = ci((int)std::floor((z + HALF) / CGS), 0, CGN - 1);
  for (int q = deckStart[cz * CGN + cx]; q < deckStart[cz * CGN + cx + 1]; q++) best = std::max(best, deckY(decks[deckList[q]], x, z)); return best; }
bool collide(float& x, float& z, float r, float ey) {
  if (colStart.empty()) return false; bool moved = false; int cx = ci((int)std::floor((x + HALF) / CGS), 0, CGN - 1), cz = ci((int)std::floor((z + HALF) / CGS), 0, CGN - 1);
  for (int it = 0; it < 2; it++) for (int j = std::max(0, cz - 1); j <= std::min(CGN - 1, cz + 1); j++) for (int i = std::max(0, cx - 1); i <= std::min(CGN - 1, cx + 1); i++)
    for (int q = colStart[j * CGN + i]; q < colStart[j * CGN + i + 1]; q++) { const Col_& c = cols[colList[q]]; if (ey > c.top - 0.4f) continue; float dx = x - c.x, dz = z - c.z, rr = c.r + r, d2 = dx * dx + dz * dz; if (d2 < rr * rr) { float d = std::sqrt(d2); if (d < 1e-4f) { dx = 1; dz = 0; d = 1; } x = c.x + dx / d * rr; z = c.z + dz / d * rr; moved = true; } }
  return moved;
}

// ------------------------------------------------------------------ terrain
static float baseH(float x, float z) {
  float nx = x / HALF, nz = z / HALF; float edge = std::max(std::fabs(nx), std::fabs(nz)), rr = std::sqrt(nx * nx + nz * nz); float land = 1.f - smooth(0.80f, 0.98f, edge * 0.55f + rr * 0.45f);
  float wx = x + (fb(x * 0.004f, z * 0.004f, 2, SEED + 91) - 0.5f) * 160.f, wz = z + (fb(x * 0.004f + 5, z * 0.004f, 2, SEED + 92) - 0.5f) * 160.f;
  float mt = std::max({smooth(-0.18f, -0.62f, nz + 0.12f * std::sin(nx * 3.1f)), blob(nx, nz, 0.64f, -0.08f, 0.34f) * 0.95f, blob(nx, nz, -0.74f, -0.32f, 0.26f) * 0.8f, blob(nx, nz, 0.18f, -0.30f, 0.20f) * 0.55f, blob(nx, nz, 0.78f, 0.52f, 0.2f) * 0.5f, blob(nx, nz, -0.30f, -0.70f, 0.24f) * 0.85f, blob(nx, nz, 0.40f, -0.62f, 0.22f) * 0.9f, blob(nx, nz, -0.82f, 0.10f, 0.20f) * 0.7f, blob(nx, nz, 0.10f, 0.66f, 0.17f) * 0.45f});
  float rid = ridge(wx * 0.0046f, wz * 0.0046f, 5, SEED + 3); float hillN = fb(x * 0.0021f + 31, z * 0.0021f - 17, 3, SEED + 8); float hills = smooth(0.42f, 0.66f, hillN); float roll = (fb(x * 0.0034f, z * 0.0034f, 4, SEED + 5) - 0.5f) * 52.f + (fb(x * 0.016f, z * 0.016f, 3, SEED + 6) - 0.5f) * 8.f + (ridge(wx * 0.0105f, wz * 0.0105f, 4, SEED + 11) - 0.45f) * 22.f * (1.f - mt * 0.5f) + hills * (ridge(wx * 0.0058f, wz * 0.0058f, 4, SEED + 12) * 46.f - 10.f) * (1.f - mt * 0.6f);   // rolling downs + foothill ridges
  float h = 8.f + roll * (1.f - mt * 0.4f) + mt * (36.f + 215.f * std::pow(rid, 0.92f) * (0.5f + 0.5f * mt));
  float gulf = blob(nx, nz, -0.64f, 0.62f, 0.42f); h = lerpf(h, -10.f, smooth(0.05f, 0.7f, gulf));
  float sea = std::max(gulf, 1.f - land); float floorH = lerpf(2.6f, -14.f, smooth(0.1f, 0.9f, sea)); h = std::max(h, floorH);
  return lerpf(-14.f, h, smooth(0.f, 1.f, land));
}
static float hSm(float x, float z) { return (height(x, z) * 2 + height(x + 14, z) + height(x - 14, z) + height(x, z + 14) + height(x, z - 14)) / 6.f; }
static void makeLake(float cx, float cz, float R, float forceLevel, const char* name, int poiKind) {
  float rim = 1e9f; for (int i = 0; i < 16; i++) { float a = i * TAU / 16; rim = std::min(rim, height(cx + std::cos(a) * R * 1.2f, cz + std::sin(a) * R * 1.2f)); }
  float level = forceLevel > -1e8f ? forceLevel : std::max(rim - 1.4f, 1.2f); int x0 = ci(cellOf(cx - R * 1.6f), 0, GS - 1), x1 = ci(cellOf(cx + R * 1.6f), 0, GS - 1), z0 = ci(cellOf(cz - R * 1.6f), 0, GS - 1), z1 = ci(cellOf(cz + R * 1.6f), 0, GS - 1);
  for (int j = z0; j <= z1; j++) for (int i = x0; i <= x1; i++) { float x = -HALF + (i + 0.5f) * CS, z = -HALF + (j + 0.5f) * CS; float wob = 1.f + (fb(x * 0.012f, z * 0.012f, 2, SEED + 77) - 0.5f) * 0.5f; float d = std::sqrt((x - cx) * (x - cx) + (z - cz) * (z - cz)) / (R * wob);
    float t = d; float prof = t < 0.9f ? level - 5.5f * (1 - (t / 0.9f) * (t / 0.9f)) : level + (t - 0.9f) * 5.5f; float& h = Hh[j * GS + i]; h = smin(h, prof, 2.f);
    if (t < 1.15f && h < level + 0.02f) Wl[j * GS + i] = level; }
  Poi p; memset(&p, 0, sizeof p); p.x = cx; p.z = cz; p.y = level; p.r = R * 0.9f; snprintf(p.name, 32, "%s", name); p.kind = poiKind; p.found = false; pois.push_back(p);
}

// ------------------------------------------------------------------ rivers
struct RP { float x, z, bed, w, lvl, dep; };
static std::vector<RP> rivers[18]; static int nRiv = 0;
static void traceRiver(float sx, float sz, int id) {
  std::vector<RP>& P = rivers[id]; float x = sx, z = sz, dx = 0, dz = 0; const float step = 6.f;
  for (int s = 0; s < 1900; s++) {
    float e = 22.f, gx = hSm(x + e, z) - hSm(x - e, z), gz = hSm(x, z + e) - hSm(x, z - e), gl = std::sqrt(gx * gx + gz * gz) + 1e-4f, ndx = -gx / gl, ndz = -gz / gl;
    float tx = -0.62f * HALF - x, tz = 0.60f * HALF - z, tl = std::sqrt(tx * tx + tz * tz) + 1e-3f; tx /= tl; tz /= tl;
    float ax = ndx * 0.55f + tx * 0.08f + dx * 0.62f - dz * 0.22f * std::sin(s * 0.06f + id), az = ndz * 0.55f + tz * 0.08f + dz * 0.62f + dx * 0.22f * std::sin(s * 0.06f + id); if (s == 0) { ax = ndx; az = ndz; }
    float al = std::sqrt(ax * ax + az * az) + 1e-4f; dx = ax / al; dz = az / al; x += dx * step; z += dz * step; if (std::fabs(x) > HALF - 10 || std::fabs(z) > HALF - 10) break;
    float h = (height(x, z) + hSm(x, z)) * 0.5f; P.push_back({x, z, h, 0, 0, 0}); if (h < 1.4f && s > 6) break;
    bool merged = false; if (s > 30) for (int k = 0; k < id && !merged; k++) for (const RP& q : rivers[k]) if ((q.x - x) * (q.x - x) + (q.z - z) * (q.z - z) < 49.f) { merged = true; break; } if (merged) break;
  }
}
static void carveRivers() {
  for (int id = 0; id < nRiv; id++) { std::vector<RP>& P = rivers[id]; int n = (int)P.size(); if (n < 40) { P.clear(); continue; } float prev = 1e9f;
    for (int i = 0; i < n; i++) { float t = (float)i / (n - 1); P[i].dep = 0.9f + 1.9f * std::sqrt(t); P[i].w = 6.f + 14.f * std::sqrt(t); float b = std::min(prev - 0.035f, P[i].bed - P[i].dep); P[i].bed = b; prev = b; }
    // one waterfall per river: steepest 7-sample window of bed in the upper reaches
    int bi = -1; float bd = 8.5f; for (int i = 6; i < n * 7 / 10 - 8; i++) if (height(P[i].x, P[i].z) > 12.f) { float d = P[i].bed - P[i + 7].bed; if (d > bd) { bd = d; bi = i; } }
    if (bi >= 0) { float upB = P[bi].bed, loB = P[bi + 7].bed; for (int j = bi + 1; j <= bi + 3; j++) P[j].bed = upB - 0.05f; for (int j = bi + 4; j <= bi + 7; j++) P[j].bed = loB; for (int j = bi + 4; j <= bi + 8 && j < n; j++) { P[j].w *= 1.5f; P[j].dep += 0.8f; P[j].bed -= 0.5f; }
      Fall f; memset(&f, 0, sizeof f); f.x = (P[bi + 3].x + P[bi + 4].x) * 0.5f; f.z = (P[bi + 3].z + P[bi + 4].z) * 0.5f; f.dx = P[bi + 4].x - P[bi + 3].x; f.dz = P[bi + 4].z - P[bi + 3].z; float fl = std::sqrt(f.dx * f.dx + f.dz * f.dz) + 1e-4f; f.dx /= fl; f.dz /= fl;
      f.yTop = upB + P[bi + 3].dep; f.yBot = loB - 0.5f + P[bi + 4].dep; f.w = P[bi + 3].w * 0.88f; snprintf(f.name, 28, "%s", id == 0 ? "Silverveil Falls" : id == 1 ? "Heron's Drop" : id == 2 ? "Thundermaw Falls" : id == 3 ? "Whisper Cascade" : id == 4 ? "Moonstep Falls" : id == 5 ? "Ravenfall" : id == 6 ? "Mistral Falls" : id == 7 ? "Aldwen Cascade" : id == 8 ? "Kestrel Leap" : id == 9 ? "Emberglass Falls" : id == 10 ? "Sorrow's Veil" : id == 11 ? "Larkspur Falls" : "Greywater Drop"); falls.push_back(f); }
    for (int i = 0; i < n; i++) P[i].lvl = P[i].bed + P[i].dep;
  }
  for (int id = 0; id < nRiv; id++) { const std::vector<RP>& P = rivers[id]; int n = (int)P.size(); for (int i = 0; i + 1 < n; i++) { const RP &a = P[i], &b = P[i + 1]; float sx = b.x - a.x, sz = b.z - a.z, sl2 = sx * sx + sz * sz, sl = std::sqrt(sl2); float R = std::max(a.w, b.w) * 0.5f + 16.f;
      int x0 = ci(cellOf(std::min(a.x, b.x) - R), 0, GS - 1), x1 = ci(cellOf(std::max(a.x, b.x) + R), 0, GS - 1), z0 = ci(cellOf(std::min(a.z, b.z) - R), 0, GS - 1), z1 = ci(cellOf(std::max(a.z, b.z) + R), 0, GS - 1);
      for (int j = z0; j <= z1; j++) for (int k = x0; k <= x1; k++) { float x = -HALF + (k + 0.5f) * CS, z = -HALF + (j + 0.5f) * CS; float t = clampf(((x - a.x) * sx + (z - a.z) * sz) / sl2, 0.f, 1.f); float px = a.x + sx * t - x, pz = a.z + sz * t - z; float d = std::sqrt(px * px + pz * pz);
        float w = lerpf(a.w, b.w, t) * 0.5f, bed = lerpf(a.bed, b.bed, t), dep = lerpf(a.dep, b.dep, t); if (d > w + 14.f) continue; float over = std::max(0.f, d - w);
        float prof = bed + 0.35f * (d / w) * (d / w) * (d < w ? 1.f : 0.f) + (d >= w ? 0.35f + over * 0.62f + over * over * 0.012f : 0.f); float& h = Hh[j * GS + k]; h = smin(h, prof, 1.6f);
        if (d < w + 1.0f) { float lvl = lerpf(a.lvl, b.lvl, t); if (a.lvl - b.lvl > 1.5f) lvl = (t < 0.5f && d < std::min(a.w, b.w) * 0.5f) ? a.lvl : b.lvl; if (h < lvl + 0.02f && lvl - h < 5.5f) { Wl[j * GS + k] = std::max(Wl[j * GS + k], lvl); flowX[j * GS + k] = sx / sl; flowZ[j * GS + k] = sz / sl; } } } } }
}

// ------------------------------------------------------------------ objects
static struct Circ { float x, z, r; } dummyC; static std::vector<Circ> placed;
static void addLight(float x, float y, float z, float r, float g, float b, float range) { lights.push_back({x, y, z, r, g, b, range}); }
static float gPutY = -1e9f; static float lastBase = 0;
static Obj* put(int kind, float x, float z, float ry, float s, float tr = 1, float tg = 1, float tb = 1, float yOff = 0, bool sink = false, float sy = 0) {
  Obj o; o.kind = (uint8_t)kind; o.x = x; o.z = z; o.y = gPutY > -1e8f ? gPutY : height(x, z) + yOff - (sink ? 0.25f : 0.f); o.ry = ry; o.sx = s; o.sz = s; o.sy = sy > 0 ? sy : s; o.r = (uint8_t)clampf(tr * 128.f, 0, 255); o.g = (uint8_t)clampf(tg * 128.f, 0, 255); o.b = (uint8_t)clampf(tb * 128.f, 0, 255); o.sway = 0;
  if (kind == K_PINE || kind == K_PINEY || kind == K_OAK || kind == K_BUSH || (kind >= K_BIRCH && kind <= K_REEDS) || kind == K_FBUSH) o.sway = 1; objs.push_back(o); addCollider(kind, x, z, ry, s, o.y);
  if (kind == K_LAMP) addLight(x, o.y + 2.3f, z, 1.0f, 0.62f, 0.30f, 20.f); else if (kind == K_BRAZIER) addLight(x, o.y + 1.2f, z, 1.4f, 0.65f, 0.25f, 24.f); else if (kind == K_CAMP) addLight(x, o.y + 0.9f, z, 1.3f, 0.6f, 0.25f, 22.f); else if (kind == K_TORCH) addLight(x, o.y + 0.8f, z, 0.9f, 0.5f, 0.2f, 12.f);
  return &objs.back();
}
static bool wetNear(float x, float z, float r) { for (int i = 0; i < 8; i++) { float a = i * TAU / 8; if (inWater(x + std::cos(a) * r, z + std::sin(a) * r)) return true; } return inWater(x, z); }
static bool spotOK(float x, float z, float r, bool allowRoad = false) {
  if (std::fabs(x) > HALF - 40 || std::fabs(z) > HALF - 40) return false; if (wetNear(x, z, r + 2.5f) || height(x, z) < 2.f) return false; if (slopeAt(x, z) > 0.28f) return false; if (!allowRoad && roadDist(x, z) < r + 2.f) return false;
  for (const Circ& c : placed) { float d = (x - c.x) * (x - c.x) + (z - c.z) * (z - c.z), rr = r + c.r; if (d < rr * rr) return false; } return true;
}
static void regPlaced(float x, float z, float r) { placed.push_back({x, z, r}); }
struct Foot { float hx, hz; }; static const Foot FOOT[NK] = {};
static void homePost(int si, float x, float z, float face, float dist, int type = 3) { Post p; memset(&p, 0, sizeof p); p.x = x + std::sin(face) * dist; p.z = z + std::cos(face) * dist; p.y = height(p.x, p.z); p.face = face; p.settle = si; p.type = type; posts.push_back(p); }
static bool putHouse(int si, int kind, float x, float z, float tx, float tz, float s, float rad, bool post = true, float tr = 1, float tg = 1, float tb = 1) {
  if (!spotOK(x, z, rad * s)) return false; float ry = std::atan2(tx - x, tz - z); put(kind, x, z, ry, s, tr, tg, tb, 0, true); regPlaced(x, z, rad * s); if (post) homePost(si, x, z, ry, rad * s * 0.8f + 1.4f); return true;
}
static float nationTint[3][3] = {{0.35f, 0.5f, 1.0f}, {1.0f, 0.28f, 0.22f}, {0.35f, 0.9f, 0.35f}};
static void addPoi(const char* name, float x, float z, float r, int kind) { Poi p; memset(&p, 0, sizeof p); p.x = x; p.z = z; p.y = height(x, z); p.r = r; snprintf(p.name, 32, "%s", name); p.kind = kind; pois.push_back(p); }
static void gatePoint(const Settle& s, float& gx, float& gz) { float d = s.type == ST_CAPITAL ? 52.f : s.type == ST_VILLAGE ? (s.size >= 3 ? 46.f : 10.f) : s.type == ST_FORT ? 22.f : s.type == ST_HAMLET ? 8.f : 6.f; gx = s.x + s.gateX * d; gz = s.z + s.gateZ * d; }
static Obj* lastWall = nullptr;
static std::vector<float> stairLog;
static void putStair(float sx0, float sz0, float ex, float ez, float baseY, float outx, float outz) { stairLog.push_back(sx0); stairLog.push_back(sz0); stairLog.push_back(ex); stairLog.push_back(ez); stairLog.push_back(baseY); float ry = std::atan2(-ez, ex); gPutY = baseY; Obj* o = put(K_STAIR, sx0, sz0, ry, 1.f); (void)o; gPutY = -1e9f;
  addDeck(sx0, sz0, ry, 5.98f, 0, 5.98f, 1.1f, baseY + 0.227f, baseY + 5.9f); float sgn = (outx * (-ez) + outz * ex) >= 0 ? 1.f : -1.f; addDeck(sx0, sz0, ry, 13.15f, sgn * 0.7f, 1.3f, 1.65f, baseY + 5.9f, baseY + 5.9f); }   // landing reaches across the 0.55 m gap onto the wall walk
static void wallRun(float ax, float az, float bx, float bz, float outNx, float outNz, float seg, int kind, float skipCx, float skipCz, float skipR, float sc = 1.f, bool stairs = false) {
  float dx = bx - ax, dz = bz - az, L = std::sqrt(dx * dx + dz * dz); int n = std::max(1, (int)(L / seg + 0.5f)); float dirx = outNz, dirz = -outNx; float ry = std::atan2(-dirz, dirx);
  float base = 0; for (int i = 0; i < n; i++) { float t = (i + 0.5f) / n; base += height(ax + dx * t, az + dz * t); } base = base / n - 0.2f; lastBase = base; float c = std::cos(ry), sn = std::sin(ry);
  for (int i = 0; i < n; i++) { float t = (i + 0.5f) / n, x = ax + dx * t, z = az + dz * t; if (skipR > 0 && std::sqrt((x - skipCx) * (x - skipCx) + (z - skipCz) * (z - skipCz)) < skipR) continue;
    gPutY = base; lastWall = put(kind, x, z, ry, sc, 1, 1, 1, -0.2f); gPutY = -1e9f;
    if (kind == K_WALL) addDeck(x, z, ry, 0, -0.275f * sc, L / n * 0.5f + 0.06f, 0.975f * sc, base + 5.9f * sc, base + 5.9f * sc); }
  (void)c; (void)sn;
  if (stairs && kind == K_WALL && L >= 24.f) { float ux = dx / L, uz = dz / L, inx = -outNx, inz = -outNz; float t1 = 0.07f; float p1x = ax + dx * t1 + inx * 2.35f, p1z = az + dz * t1 + inz * 2.35f;
    if (!(skipR > 0 && std::sqrt((p1x - skipCx) * (p1x - skipCx) + (p1z - skipCz) * (p1z - skipCz)) < skipR + 2.f)) putStair(p1x, p1z, ux, uz, base, outNx, outNz);
    if (L >= 40.f) { float t2 = 0.93f; float p2x = ax + dx * t2 + inx * 2.35f, p2z = az + dz * t2 + inz * 2.35f; if (!(skipR > 0 && std::sqrt((p2x - skipCx) * (p2x - skipCx) + (p2z - skipCz) * (p2z - skipCz)) < skipR + 2.f)) putStair(p2x, p2z, -ux, -uz, base, outNx, outNz); } }
}
static void flagAt(int nation, float x, float z, float yAdd, float s = 1.f) { Obj* o = put(K_FLAGPOLE, x, z, rng() * 0.6f, s, nationTint[nation][0], nationTint[nation][1], nationTint[nation][2], yAdd); (void)o; }
static void buildCapital(int si) {
  Settle& s = settles[si]; placed.clear(); float HS = 40.f; float gx = s.gateX, gz = s.gateZ; float rx = -gz, rz = gx;   // fwd (outward) = g, right = r
  auto P = [&](float u, float v, float& x, float& z) { x = s.x + rx * u + gx * v; z = s.z + rz * u + gz * v; };
  float x, z; int nat = s.nation; float tr = nationTint[nat][0], tg = nationTint[nat][1], tb = nationTint[nat][2];
  // curtain wall (outer face = outward normal), gate on the +v side
  for (int side = 0; side < 4; side++) { float ax, az, bx, bz, nx, nz; float u0, v0, u1, v1, nu, nv;
    if (side == 0) { u0 = -HS; v0 = HS; u1 = HS; v1 = HS; nu = 0; nv = 1; } else if (side == 1) { u0 = -HS; v0 = -HS; u1 = HS; v1 = -HS; nu = 0; nv = -1; } else if (side == 2) { u0 = HS; v0 = -HS; u1 = HS; v1 = HS; nu = 1; nv = 0; } else { u0 = -HS; v0 = -HS; u1 = -HS; v1 = HS; nu = -1; nv = 0; }
    P(u0, v0, ax, az); P(u1, v1, bx, bz); nx = rx * nu + gx * nv; nz = rz * nu + gz * nv; float gcx, gcz; P(0, HS, gcx, gcz); wallRun(ax, az, bx, bz, nx, nz, 5.0f, K_WALL, gcx, gcz, side == 0 ? 7.2f : 0.f, 1.f, true); }
  for (int cu = -1; cu <= 1; cu += 2) for (int cv = -1; cv <= 1; cv += 2) { P(cu * HS, cv * HS, x, z); put(K_WTOWER, x, z, rng() * 6, 1, 1, 1, 1, -0.3f); flagAt(nat, x, z, 11.2f); }
  { P(0, -HS, x, z); put(K_WTOWER, x, z, 0, 1, 1, 1, 1, -0.3f); P(HS, 0, x, z); put(K_WTOWER, x, z, 0, 1, 1, 1, 1, -0.3f); P(-HS, 0, x, z); put(K_WTOWER, x, z, 0, 1, 1, 1, 1, -0.3f); }
  P(0, HS, x, z); put(K_GATE, x, z, std::atan2(gx, gz), 1, 1, 1, 1, -0.3f); { float bx, bz; P(-8.5f, HS - 2, bx, bz); put(K_BANNER, bx, bz, std::atan2(gx, gz), 1, tr, tg, tb); P(8.5f, HS - 2, bx, bz); put(K_BANNER, bx, bz, std::atan2(gx, gz), 1, tr, tg, tb); P(-7.5f, HS + 3.5f, bx, bz); put(K_BRAZIER, bx, bz, 0, 1.2f); P(7.5f, HS + 3.5f, bx, bz); put(K_BRAZIER, bx, bz, 0, 1.2f); }
  float kx, kz; P(0, -12, kx, kz); put(K_KEEP, kx, kz, std::atan2(gx, gz), 1, 1, 1, 1, -0.3f); regPlaced(kx, kz, 11); flagAt(nat, kx, kz, 19.5f, 1.4f); { float bx, bz; P(-6, -2.5f, bx, bz); put(K_BANNER, bx, bz, std::atan2(gx, gz), 1, tr, tg, tb); P(6, -2.5f, bx, bz); put(K_BANNER, bx, bz, std::atan2(gx, gz), 1, tr, tg, tb); }
  // main street lamps, plaza well, market stalls
  for (int k = 0; k < 6; k++) for (int sd = -1; sd <= 1; sd += 2) { P(sd * 4.2f, HS - 6 - k * 9.f, x, z); if (k < 5) put(K_LAMP, x, z, 0, 1.3f); }
  P(0, 13, x, z); put(K_WELL, x, z, 0, 1.5f); regPlaced(x, z, 3.5f); for (int k = 0; k < 8; k++) { float a = k * TAU / 8; float bx = x + std::cos(a) * 5.6f, bz = z + std::sin(a) * 5.6f; if (k % 2 == 0) { put(K_BARREL, bx, bz, 0, 1); } else put(K_CRATE, bx, bz, a, 1); }
  for (int k = 0; k < 6; k++) { int sd = k % 2 ? 1 : -1; P(sd * 8.5f, 24 - (k / 2) * 6.5f, x, z); float ry = std::atan2(-sd * rx, -sd * rz); put(K_STALL, x, z, ry, 1.5f); regPlaced(x, z, 2.3f); Post p; memset(&p, 0, sizeof p); p.x = x + sd * rx * 1.4f; p.z = z + sd * rz * 1.4f; p.y = height(p.x, p.z); p.face = ry; p.settle = si; p.type = 4; posts.push_back(p); }
  // houses along the street
  { int nh = 0; for (int k = 0; k < 7; k++) for (int sd = -1; sd <= 1; sd += 2) { float v = 30 - k * 10.f; if (v < -2 && v > -22) continue; P(sd * (14.f + (k % 2) * 2.f), v, x, z); float tx, tz; P(0, v, tx, tz); int kind = (k + (sd > 0)) % 4 == 0 ? K_HOUSE : K_COTTAGE; float sc = kind == K_HOUSE ? 1.f : 1.35f; if (putHouse(si, kind, x, z, tx, tz, sc, kind == K_HOUSE ? 4.4f : 3.6f)) nh++; }
    P(-24, -22, x, z); { float tx, tz; P(0, -22, tx, tz); putHouse(si, K_CHAPEL, x, z, tx, tz, 1.35f, 4.4f); } P(24, -22, x, z); { float tx, tz; P(0, -22, tx, tz); putHouse(si, K_BARN, x, z, tx, tz, 1.5f, 5.f); } P(-26, 14, x, z); { float tx, tz; P(0, 14, tx, tz); putHouse(si, K_LONG, x, z, tx, tz, 1.f, 7.f, false); } P(26, 14, x, z); { float tx, tz; P(0, 14, tx, tz); putHouse(si, K_LONG, x, z, tx, tz, 1.f, 7.f, false); } (void)nh; }
  // keep guards, gate guards, wall patrols
  for (int sd = -1; sd <= 1; sd += 2) { Post p; memset(&p, 0, sizeof p); P(sd * 3.3f, HS - 1.5f, p.x, p.z); p.y = height(p.x, p.z); p.face = std::atan2(gx, gz); p.settle = si; p.type = 0; posts.push_back(p); Post q; memset(&q, 0, sizeof q); P(sd * 3.2f, -12 + 10.f, q.x, q.z); q.y = height(q.x, q.z); q.face = std::atan2(gx, gz); q.settle = si; q.type = 2; posts.push_back(q); }
  { float wy = s.y + 5.9f; for (int side = 0; side < 4; side++) { Post p; memset(&p, 0, sizeof p); float in = HS - 0.9f; float u0, v0, u1, v1; if (side == 0) { u0 = -HS + 5; v0 = in; u1 = -9; v1 = in; } else if (side == 1) { u0 = -HS + 4; v0 = -in; u1 = HS - 4; v1 = -in; } else if (side == 2) { u0 = in; v0 = -HS + 4; u1 = in; v1 = HS - 4; } else { u0 = -in; v0 = -HS + 4; u1 = -in; v1 = HS - 4; }
      P(u0, v0, p.ax, p.az); P(u1, v1, p.bx, p.bz); p.x = p.ax; p.z = p.az; { float dt1 = deckTop(p.ax, p.az); p.y = dt1 > -1e8f ? dt1 : wy; } p.settle = si; p.type = 1; posts.push_back(p); } }
  // outskirts: farmhouses, windmill, hay, fences
  for (int k = 0; k < 60 && k < 200; k++) { float a = rng() * TAU, d = 72.f + rng() * 80.f; x = s.x + std::cos(a) * d; z = s.z + std::sin(a) * d; int kind = rng() < 0.5f ? K_COTTAGE : (rng() < 0.4f ? K_BARN : K_HOUSE); float sc = kind == K_HOUSE ? 1.f : 1.3f; putHouse(si, kind, x, z, s.x, s.z, sc, kind == K_BARN ? 5.2f : (kind == K_HOUSE ? 4.4f : 3.6f), kind != K_BARN); if (objs.size() > 400000) break; }
  for (int k = 0; k < 40; k++) { float a = rng() * TAU, d = 66.f + rng() * 70.f; x = s.x + std::cos(a) * d; z = s.z + std::sin(a) * d; if (spotOK(x, z, 3.5f)) { put(K_MILL, x, z, rng() * TAU, 1.2f); regPlaced(x, z, 4.f); break; } }
  for (int k = 0; k < 14; k++) { float a = rng() * TAU, d = 60.f + rng() * 70.f; x = s.x + std::cos(a) * d; z = s.z + std::sin(a) * d; if (spotOK(x, z, 1.6f)) { put(K_HAY, x, z, rng() * TAU, 1.f + rng() * 0.4f); regPlaced(x, z, 1.8f); } }
}
static void buildVillage(int si) {
  Settle& s = settles[si]; placed.clear(); float R = s.r; float x, z; int nat = s.nation; (void)nat;
  put(K_WELL, s.x, s.z, 0, 1.3f); regPlaced(s.x, s.z, 3.f); put(K_CAMP, s.x + 5.2f, s.z + 2.f, 0, 1.2f); regPlaced(s.x + 5.2f, s.z + 2, 1.6f);
  for (int k = 0; k < 4; k++) { float a = k * TAU / 4 + 0.7f; put(K_LAMP, s.x + std::cos(a) * 9.f, s.z + std::sin(a) * 9.f, 0, 1.2f); }
  int nh = 4 + s.size * 3, got = 0; for (int tr = 0; tr < 260 && got < nh; tr++) { float a = rng() * TAU, d = 12.f + rng() * (R - 16.f); x = s.x + std::cos(a) * d; z = s.z + std::sin(a) * d; float p = rng(); int kind = p < 0.55f ? K_COTTAGE : p < 0.78f ? K_HOUSE : p < 0.9f ? K_BARN : K_LONG; if (kind == K_LONG && s.size < 2) kind = K_COTTAGE;
    float sc = kind == K_COTTAGE ? 1.3f : kind == K_BARN ? 1.4f : 1.f; float rad = kind == K_COTTAGE ? 3.6f : kind == K_BARN ? 5.f : kind == K_HOUSE ? 4.4f : 7.f; if (putHouse(si, kind, x, z, s.x, s.z, sc, rad, kind != K_BARN)) got++; }
  if (s.size >= 2) for (int tr = 0; tr < 60; tr++) { float a = rng() * TAU; x = s.x + std::cos(a) * (R * 0.62f); z = s.z + std::sin(a) * (R * 0.62f); if (putHouse(si, K_CHAPEL, x, z, s.x, s.z, 1.3f, 4.5f, false)) break; }
  if (s.size >= 3) for (int tr = 0; tr < 60; tr++) { float a = rng() * TAU; x = s.x + std::cos(a) * (R * 0.8f); z = s.z + std::sin(a) * (R * 0.8f); if (putHouse(si, K_WATCH, x, z, s.x, s.z, 1.3f, 3.f, false)) { homePost(si, x, z, 0, 0, 5); break; } }
  for (int k = 0; k < 2 + s.size; k++) { float a = k * 2.1f + rng(); x = s.x + std::cos(a) * 7.5f; z = s.z + std::sin(a) * 7.5f; if (spotOK(x, z, 2.f, true)) { put(K_STALL, x, z, std::atan2(s.x - x, s.z - z), 1.4f); regPlaced(x, z, 2.f); Post p; memset(&p, 0, sizeof p); p.x = x + (s.x - x) * 0.25f; p.z = z + (s.z - z) * 0.25f; p.y = height(p.x, p.z); p.face = std::atan2(s.x - x, s.z - z) + PI; p.settle = si; p.type = 4; posts.push_back(p); } }
  for (int k = 0; k < 8; k++) { float a = rng() * TAU, d = 14 + rng() * (R - 14); x = s.x + std::cos(a) * d; z = s.z + std::sin(a) * d; if (spotOK(x, z, 1.2f)) { int w = (int)(rng() * 3); put(w == 0 ? K_HAY : w == 1 ? K_BARREL : K_CRATE, x, z, rng() * TAU, w == 0 ? 1.1f : 1.f); regPlaced(x, z, 1.3f); } }
  if (s.size >= 3) { float gx = s.x + s.gateX * R, gz = s.z + s.gateZ * R; float ga = std::atan2(s.gateZ, s.gateX); int n = (int)(TAU * R / 4.f); for (int k = 0; k < n; k++) { float a = k * TAU / n; if (std::fabs(angDiff(a, ga)) < 0.2f) continue; x = s.x + std::cos(a) * R; z = s.z + std::sin(a) * R; if (inWater(x, z) || slopeAt(x, z) > 0.8f) continue; put(K_PALI, x, z, -(a + PI / 2), 1.05f, 1, 1, 1, -0.1f); }
    put(K_GATE, gx, gz, std::atan2(s.gateX, s.gateZ), 0.62f, 1.1f, 0.85f, 0.7f, -0.2f); for (int sd = -1; sd <= 1; sd += 2) { Post p; memset(&p, 0, sizeof p); p.x = gx + (-s.gateZ) * sd * 2.4f + s.gateX * 2.5f; p.z = gz + s.gateX * sd * 2.4f + s.gateZ * 2.5f; p.y = height(p.x, p.z); p.face = std::atan2(s.gateX, s.gateZ); p.settle = si; p.type = 0; posts.push_back(p); } }
  else if (s.size >= 2) { for (int sd = -1; sd <= 1; sd += 2) { Post p; memset(&p, 0, sizeof p); p.x = s.x + s.gateX * (R * 0.7f) + (-s.gateZ) * sd * 2.5f; p.z = s.z + s.gateZ * (R * 0.7f) + s.gateX * sd * 2.5f; p.y = height(p.x, p.z); p.face = std::atan2(s.gateX, s.gateZ); p.settle = si; p.type = 0; posts.push_back(p); } }
  Post q; memset(&q, 0, sizeof q); q.x = s.x - 3.5f; q.z = s.z + 3.5f; q.y = height(q.x, q.z); q.face = rng() * TAU; q.settle = si; q.type = 2; posts.push_back(q);
  (void)gatePoint;
}
static void buildHamlet(int si) {
  Settle& s = settles[si]; placed.clear(); put(K_WELL, s.x, s.z, 0, 1.2f); regPlaced(s.x, s.z, 3.f); put(K_CAMP, s.x + 4.f, s.z - 3.f, 0, 1.1f); int n = 3 + s.size * 2, got = 0; float x, z;
  for (int tr = 0; tr < 200 && got < n; tr++) { float a = rng() * TAU, d = 9.f + rng() * (s.r - 10.f); x = s.x + std::cos(a) * d; z = s.z + std::sin(a) * d; int kind = rng() < 0.7f ? K_COTTAGE : K_BARN; if (putHouse(si, kind, x, z, s.x, s.z, kind == K_COTTAGE ? 1.3f : 1.4f, kind == K_COTTAGE ? 3.6f : 5.f, kind == K_COTTAGE)) got++; }
  for (int k = 0; k < 6; k++) { float a = rng() * TAU, d = 8 + rng() * 12; x = s.x + std::cos(a) * d; z = s.z + std::sin(a) * d; if (spotOK(x, z, 1.2f)) { put(rng() < 0.5f ? K_HAY : K_BARREL, x, z, rng() * TAU, 1.1f); regPlaced(x, z, 1.3f); } }
}
static void buildFort(int si) {
  Settle& s = settles[si]; placed.clear(); float HS = 15.f; float gx = s.gateX, gz = s.gateZ, rx = -gz, rz = gx; auto P = [&](float u, float v, float& x, float& z) { x = s.x + rx * u + gx * v; z = s.z + rz * u + gz * v; }; float x, z; int nat = s.nation;
  for (int side = 0; side < 4; side++) { float u0, v0, u1, v1, nu, nv; if (side == 0) { u0 = -HS; v0 = HS; u1 = HS; v1 = HS; nu = 0; nv = 1; } else if (side == 1) { u0 = -HS; v0 = -HS; u1 = HS; v1 = -HS; nu = 0; nv = -1; } else if (side == 2) { u0 = HS; v0 = -HS; u1 = HS; v1 = HS; nu = 1; nv = 0; } else { u0 = -HS; v0 = -HS; u1 = -HS; v1 = HS; nu = -1; nv = 0; }
    float ax, az, bx, bz; P(u0, v0, ax, az); P(u1, v1, bx, bz); float gcx, gcz; P(0, HS, gcx, gcz); wallRun(ax, az, bx, bz, rx * nu + gx * nv, rz * nu + gz * nv, 5.f, K_WALL, gcx, gcz, side == 0 ? 6.f : 0.f, 1.f, true); }
  for (int cu = -1; cu <= 1; cu += 2) for (int cv = -1; cv <= 1; cv += 2) { P(cu * HS, cv * HS, x, z); put(K_WTOWER, x, z, 0, 0.9f, 1, 1, 1, -0.3f); flagAt(nat, x, z, 10.2f, 0.9f); }
  P(0, HS, x, z); put(K_GATE, x, z, std::atan2(gx, gz), 0.85f, 1, 1, 1, -0.3f); P(0, -6, x, z); put(K_WATCH, x, z, std::atan2(gx, gz), 1.4f); P(-7, 4, x, z); put(K_TENT, x, z, 0.5f, 1.f, 1, 1, 1); P(7, 4, x, z); put(K_TENT, x, z, -0.5f, 1.f); P(0, 4, x, z); put(K_CAMP, x, z, 0, 1.2f); P(-8, -8, x, z); put(K_CRATE, x, z, 0.3f, 1.1f); P(8, -8, x, z); put(K_BARREL, x, z, 0.3f, 1.2f); P(6, HS - 3, x, z); put(K_BRAZIER, x, z, 0, 1.1f);
  for (int k = 0; k < 4; k++) { Post p; memset(&p, 0, sizeof p); float a = k * TAU / 4; P(std::cos(a) * 7, std::sin(a) * 7 - 2, p.x, p.z); p.y = height(p.x, p.z); p.face = a; p.settle = si; p.type = k < 2 ? 0 : 2; if (k < 2) P(k == 0 ? -3.f : 3.f, HS - 2, p.x, p.z); posts.push_back(p); }
  Post w; memset(&w, 0, sizeof w); P(-HS + 3, -HS + 1.f, w.ax, w.az); P(HS - 3, -HS + 1.f, w.bx, w.bz); w.x = w.ax; w.z = w.az; w.y = s.y + 5.9f; w.settle = si; w.type = 1; posts.push_back(w);
}
static void buildCamp(int si) {
  Settle& s = settles[si]; float x, z; for (int k = 0; k < 4; k++) { float a = k * TAU / 4 + rng(); x = s.x + std::cos(a) * 9.f; z = s.z + std::sin(a) * 9.f; put(K_TENT, x, z, a + PI, 1.15f, 0.8f + rng() * 0.4f, 0.8f + rng() * 0.3f, 0.7f + rng() * 0.3f); }
  put(K_CAMP, s.x, s.z, 0, 1.4f); put(K_CAMP, s.x + 5, s.z - 5, 0, 1.0f); for (int k = 0; k < 8; k++) { float a = rng() * TAU, d = 4 + rng() * 14; x = s.x + std::cos(a) * d; z = s.z + std::sin(a) * d; int w = (int)(rng() * 4); put(w == 0 ? K_CRATE : w == 1 ? K_BARREL : w == 2 ? K_LOG : K_DEAD, x, z, rng() * TAU, w == 3 ? 1.2f : 1.1f); }
  lairs.push_back({s.x, s.z, 24, 1});
}
static void buildRuins(int si) {
  Settle& s = settles[si]; float x, z; for (int k = 0; k < 5; k++) { float a = k * TAU / 5 + rng() * 0.3f; x = s.x + std::cos(a) * 12.f; z = s.z + std::sin(a) * 12.f; put(K_RGABLE, x, z, a + PI, 1.3f, 0.9f, 0.95f, 0.9f, 0, true); }
  for (int k = 0; k < 8; k++) { float a = k * TAU / 8 + 0.2f; x = s.x + std::cos(a) * 18.f; z = s.z + std::sin(a) * 18.f; put(K_RWALL, x, z, a + PI / 2, 1.5f, 0.9f, 0.95f, 0.9f, 0, true); }
  put(K_RUNE, s.x, s.z, 0, 3.5f, 0.8f, 1.1f, 1.3f); for (int k = 0; k < 6; k++) { float a = rng() * TAU, d = 3 + rng() * 22; put(K_HEAD, s.x + std::cos(a) * d, s.z + std::sin(a) * d, rng() * TAU, 1.1f); }
  for (int k = 0; k < 4; k++) { float a = rng() * TAU, d = 6 + rng() * 25; put(K_DEAD, s.x + std::cos(a) * d, s.z + std::sin(a) * d, rng() * TAU, 1.2f); } addLight(s.x, height(s.x, s.z) + 1.2f, s.z, 0.35f, 0.7f, 0.9f, 22.f); lairs.push_back({s.x, s.z, 28, 2});
}
static void buildGrave(int si) {
  Settle& s = settles[si]; float x, z; put(K_CGATE, s.x + s.gateX * 17, s.z + s.gateZ * 17, std::atan2(s.gateX, s.gateZ) + PI / 2, 1.5f);
  for (int i = -3; i <= 3; i++) for (int j = -2; j <= 2; j++) { if (rng() < 0.25f) continue; x = s.x + i * 4.2f + (rng() - 0.5f) * 1.2f; z = s.z + j * 4.4f + (rng() - 0.5f) * 1.2f; if (rng() < 0.65f) put(K_HEAD, x, z, (rng() - 0.5f) * 0.4f, 1.15f); else put(K_GRAVE, x, z, rng() * 0.5f, 1.2f); }
  for (int k = 0; k < 6; k++) { float a = rng() * TAU, d = 12 + rng() * 10; put(K_DEAD, s.x + std::cos(a) * d, s.z + std::sin(a) * d, rng() * TAU, 1.3f); } put(K_IFENCE, s.x, s.z - 14, 0, 1.2f); put(K_CHAPEL, s.x, s.z + 12, PI, 1.1f, 0.8f, 0.8f, 0.85f);
  addLight(s.x, height(s.x, s.z) + 1.5f, s.z, 0.4f, 0.8f, 0.5f, 22.f); lairs.push_back({s.x, s.z, 26, 3}); (void)si;
}
static void buildBorder(int si, float dirx, float dirz) {   // gate across the road, walls to both sides
  Settle& s = settles[si]; float rx = -dirz, rz = dirx; Obj* g = put(K_GATE, s.x, s.z, std::atan2(dirx, dirz), 1.0f, 1, 1, 1, -0.3f); (void)g;
  for (int sd = -1; sd <= 1; sd += 2) { float ax = s.x + rx * sd * 8.f, az = s.z + rz * sd * 8.f, bx = s.x + rx * sd * 34.f, bz = s.z + rz * sd * 34.f; wallRun(ax, az, bx, bz, dirx, dirz, 5.f, K_WALL, 0, 0, 0, 1.f, true); put(K_WTOWER, bx, bz, 0, 0.9f, 1, 1, 1, -0.3f);
    float fx = s.x + rx * sd * 12.f + dirx * 3.5f, fz = s.z + rz * sd * 12.f + dirz * 3.5f; int nn = sd < 0 ? s.nation : (s.nation + 1) % 3; (void)nn; put(K_BANNER, fx, fz, std::atan2(dirx, dirz), 1.0f, nationTint[sd < 0 ? s.nation : s.poi][0], nationTint[sd < 0 ? s.nation : s.poi][1], nationTint[sd < 0 ? s.nation : s.poi][2]);
    put(K_BRAZIER, s.x + rx * sd * 5.f + dirx * 4.f, s.z + rz * sd * 5.f + dirz * 4.f, 0, 1.1f);
    for (int f = -1; f <= 1; f += 2) { Post p; memset(&p, 0, sizeof p); p.x = s.x + rx * sd * 3.2f + dirx * f * 4.f; p.z = s.z + rz * sd * 3.2f + dirz * f * 4.f; p.y = height(p.x, p.z); p.face = std::atan2(dirx * f, dirz * f); p.settle = si; p.type = 0; posts.push_back(p); } }
  put(K_TENT, s.x + rx * 14 - dirx * 6, s.z + rz * 14 - dirz * 6, 1.f, 1.1f);
}

// ------------------------------------------------------------------ roads
static void gradeRoad(const std::vector<V3>& poly) {
  // dense (2 m) resample
  std::vector<V3> d; for (size_t i = 0; i + 1 < poly.size(); i++) { float dx = poly[i + 1].x - poly[i].x, dz = poly[i + 1].z - poly[i].z, L = std::sqrt(dx * dx + dz * dz); int n = std::max(1, (int)(L / 2.f)); for (int k = 0; k < n; k++) d.push_back({poly[i].x + dx * k / n, 0, poly[i].z + dz * k / n}); } d.push_back(poly.back());
  int n = (int)d.size(); std::vector<float> hy(n), dry(n); std::vector<char> wet(n); for (int i = 0; i < n; i++) { hy[i] = height(d[i].x, d[i].z); float w = waterAt(d[i].x, d[i].z); wet[i] = w > -1e8f && hy[i] < w + 0.3f; }
  for (int i = 0; i < n; i++) { float s = 0; int c = 0; for (int k = -22; k <= 22; k++) { int j = i + k; if (j < 0 || j >= n || wet[j]) continue; s += hy[j]; c++; } dry[i] = c ? s / c : hy[i]; }
  // wet runs: linear between the dry neighbours, +0.9 for the deck
  for (int i = 0; i < n;) { if (!wet[i]) { i++; continue; } int j = i; while (j < n && wet[j]) j++; float a = i > 0 ? dry[i - 1] : dry[std::min(j, n - 1)], b = j < n ? dry[j] : a; for (int k = i; k < j; k++) dry[k] = lerpf(a, b, (float)(k - i + 1) / (j - i + 1)); i = j; }
  for (int i = 0; i < n; i++) { if (wet[i]) continue; float cx = d[i].x, cz = d[i].z; int x0 = ci(cellOf(cx - 9), 0, GS - 1), x1 = ci(cellOf(cx + 9), 0, GS - 1), z0 = ci(cellOf(cz - 9), 0, GS - 1), z1 = ci(cellOf(cz + 9), 0, GS - 1);
    for (int j = z0; j <= z1; j++) for (int k = x0; k <= x1; k++) { float x = -HALF + (k + 0.5f) * CS, z = -HALF + (j + 0.5f) * CS; float dd = std::sqrt((x - cx) * (x - cx) + (z - cz) * (z - cz)); roadD[j * GS + k] = std::min(roadD[j * GS + k], dd); if (Wl[j * GS + k] > -1e8f) continue;
      float w = 1.f - smooth(2.6f, 8.8f, dd); if (w > gradeW[j * GS + k]) { gradeW[j * GS + k] = w; gradeY[j * GS + k] = dry[i]; } } }
  // bridges over wet runs
  for (int i = 0; i < n;) { if (!wet[i]) { i++; continue; } int j = i; while (j < n && wet[j]) j++; int a = std::max(0, i - 2), b = std::min(n - 1, j + 1); float dx = d[b].x - d[a].x, dz = d[b].z - d[a].z, L = std::sqrt(dx * dx + dz * dz) + 1e-4f; float mx = (d[a].x + d[b].x) * 0.5f, mz = (d[a].z + d[b].z) * 0.5f; float deck = 0; for (int k = i; k < j; k++) deck = std::max(deck, dry[k]);
    float wl = -1e9f; for (int k = i; k < j; k++) wl = std::max(wl, waterAt(d[k].x, d[k].z)); deck = std::max(deck, wl + 0.9f); Obj o; o.kind = K_BRIDGE; o.x = mx; o.z = mz; o.y = deck; o.ry = std::atan2(dx / L, dz / L); o.sx = 1; o.sz = (L + 3.f) / 8.f; o.sy = 1; o.r = o.g = o.b = 128; o.sway = 0; objs.push_back(o); i = j; }
}

// ------------------------------------------------------------------ main generator
static void pickSites() {
  struct Want { int type, nation, size; const char* name; }; std::vector<Want> W;
  static const char* CAP[3] = {"Aldermere", "Korvath Hold", "Verdane"}; static const char* VIL[] = {"Thornby", "Mossgate", "Briar's Rest", "Hollowford", "Ashwick", "Stonebridge", "Willowmere", "Greywatch", "Dunmarrow", "Fenhallow", "Cloverdale", "Highmeadow", "Oakhaven", "Redmill", "Saltmarsh", "Eastlea", "Ravensford", "Kingsmead", "Copperfield", "Wyndham", "Harrowgate", "Lindenholt", "Marrowby", "Dawnreach"};
  static const char* HAM[] = {"Little Wren", "Pebble End", "Nettlebrook", "Foxglove", "Cobbler's Rest", "Lark Hollow", "Tansy Cross", "Brambleton", "Shepherd's Fold", "Reedwater", "Cinder Row", "Thistledown", "Mill End", "Wickerby"}; static const char* FORT[] = {"Fort Ember", "Ravenwatch", "Keeper's Spire", "Ironpost", "Wolfsgard", "Stormwatch", "Duskhold", "The Last Bastion", "Fort Halcyon"}; static const char* CAMPN[] = {"Bandit Hollow", "The Gnawing Camp", "Skullbreak Camp", "Blackthorn Den", "Rotfang Hideout", "Cutthroat Rise", "Gallows Camp", "Mudskull Camp"}; static const char* RUIN[] = {"Hollow Abbey", "Sunken Shrine", "The Broken Court", "Fallen Observatory", "Lost Amphitheatre", "Ashen Cloister", "The Weeping Arch"};
  for (int i = 0; i < 3; i++) W.push_back({ST_CAPITAL, i, 3, CAP[i]}); for (int i = 0; i < 24; i++) W.push_back({ST_VILLAGE, i % 3, 1 + (i % 3), VIL[i]}); for (int i = 0; i < 14; i++) W.push_back({ST_HAMLET, (i + 1) % 3, 1 + i % 2, HAM[i]});
  for (int i = 0; i < 9; i++) W.push_back({ST_FORT, i % 3, 1, FORT[i]}); for (int i = 0; i < 8; i++) W.push_back({ST_CAMP, i % 3, 1, CAMPN[i]}); for (int i = 0; i < 7; i++) W.push_back({ST_RUINS, i % 3, 1, RUIN[i]}); W.push_back({ST_GRAVE, 1, 1, "Weeping Hill Graveyard"}); W.push_back({ST_GRAVE, 0, 1, "The Quiet Acre"}); W.push_back({ST_GRAVE, 2, 1, "Hollowmoor Barrows"}); W.push_back({ST_GRAVE, 1, 1, "Saint Orrin's Rest"});
  const float AN[3][2] = {{-0.40f, 0.14f}, {0.56f, -0.10f}, {0.02f, 0.30f}}; nations[0] = {"ALDERMERE", "KINGDOM OF ALDERMERE", 0x3a6ea5, AN[0][0] * HALF, AN[0][1] * HALF, -1}; nations[1] = {"KORVATH", "REALM OF KORVATH", 0xa8342c, AN[1][0] * HALF, AN[1][1] * HALF, -1}; nations[2] = {"VERDANE", "FREE LANDS OF VERDANE", 0x3f8a3f, AN[2][0] * HALF, AN[2][1] * HALF, -1};
  int ni[3] = {0, 0, 0}; (void)ni; int vcount = 0; (void)vcount;
  for (const Want& w : W) { float R = w.type == ST_CAPITAL ? 62.f : w.type == ST_VILLAGE ? 30.f + w.size * 6.f : w.type == ST_HAMLET ? 24.f : w.type == ST_FORT ? 26.f : w.type == ST_CAMP ? 20.f : 28.f; float best = 1e30f, bx = 0, bz = 0; float cx = nations[w.nation].x, cz = nations[w.nation].z;
    float spread = (w.type == ST_CAPITAL ? 320.f : (w.type == ST_CAMP || w.type == ST_RUINS || w.type == ST_GRAVE) ? 1250.f : 780.f); for (int t = 0; t < 3400; t++) { float a = rng() * TAU, d = std::sqrt(rng()) * spread; float x = cx + std::cos(a) * d, z = cz + std::sin(a) * d; if (std::fabs(x) > HALF - 120 || std::fabs(z) > HALF - 120) continue;
      float h = height(x, z); if (h < (w.type == ST_CAPITAL ? 9.f : w.type == ST_VILLAGE ? 5.f : 3.2f) || h > (w.type == ST_CAPITAL ? 60.f : w.type == ST_FORT ? 75.f : (w.type == ST_RUINS || w.type == ST_CAMP) ? 60.f : 38.f)) continue; if (wetNear(x, z, R + 22.f)) continue; float mn = 1e9f, mx = -1e9f; for (int k = 0; k < 12; k++) { float a2 = k * TAU / 12; float hh = height(x + std::cos(a2) * R * 0.8f, z + std::sin(a2) * R * 0.8f); mn = std::min(mn, hh); mx = std::max(mx, hh); } float rangeH = mx - mn; if (rangeH > (w.type == ST_CAPITAL ? 24.f : 12.f)) continue;
      bool ok = true; for (const Settle& o : settles) { float dd = std::sqrt((x - o.x) * (x - o.x) + (z - o.z) * (z - o.z)); float need = (o.r + R) * 1.5f + (w.type == ST_CAPITAL || o.type == ST_CAPITAL ? 110.f : 70.f); if (dd < need) { ok = false; break; } } if (!ok) continue;
      float score = rangeH * 3.f + d * 0.02f + (w.type == ST_CAPITAL ? 0 : (nationAt(x, z) == w.nation ? 0 : 60.f)) + rng() * 4.f; if (score < best) { best = score; bx = x; bz = z; } }
    if (best > 1e29f) continue; Settle s; s.type = w.type; s.nation = w.nation; s.size = w.size; s.x = bx; s.z = bz; s.r = R; snprintf(s.name, 28, "%s", w.name); settles.push_back(s); if (w.type == ST_CAPITAL) nations[w.nation].capital = (int)settles.size() - 1; }
  // flatten
  for (Settle& s : settles) { float sum = 0; int c = 0; for (int k = 0; k < 20; k++) { float a = k * TAU / 20; sum += height(s.x + std::cos(a) * s.r * 0.6f, s.z + std::sin(a) * s.r * 0.6f); c++; } float y0 = std::max(sum / c, 3.4f); s.y = y0; float R = s.r * (s.type == ST_CAPITAL ? 1.f : 0.9f); int x0 = ci(cellOf(s.x - R * 1.6f), 0, GS - 1), x1 = ci(cellOf(s.x + R * 1.6f), 0, GS - 1), z0 = ci(cellOf(s.z - R * 1.6f), 0, GS - 1), z1 = ci(cellOf(s.z + R * 1.6f), 0, GS - 1);
    for (int j = z0; j <= z1; j++) for (int i = x0; i <= x1; i++) { float x = -HALF + (i + 0.5f) * CS, z = -HALF + (j + 0.5f) * CS; float d = std::sqrt((x - s.x) * (x - s.x) + (z - s.z) * (z - s.z)) / R; float w = 1.f - smooth(0.85f, 1.55f, d); if (w > 0) { float& h = Hh[j * GS + i]; h = lerpf(h, y0 + (fb(x * 0.05f, z * 0.05f, 2, 7) - 0.5f) * 0.5f, w); } } }
}
static void connect() {
  struct E { int a, b; }; std::vector<E> es; auto has = [&](int a, int b) { for (const E& e : es) if ((e.a == a && e.b == b) || (e.a == b && e.b == a)) return true; return false; }; int n = (int)settles.size();
  auto dist = [&](int a, int b) { return std::sqrt((settles[a].x - settles[b].x) * (settles[a].x - settles[b].x) + (settles[a].z - settles[b].z) * (settles[a].z - settles[b].z)); };
  for (int i = 0; i < n; i++) { std::vector<std::pair<float, int>> d; for (int j = 0; j < n; j++) if (j != i && settles[j].type <= ST_FORT) d.push_back({dist(i, j), j}); std::sort(d.begin(), d.end()); int want = settles[i].type <= ST_VILLAGE ? 2 : 1; for (int k = 0; k < (int)d.size() && k < want; k++) if (!has(i, d[k].second)) es.push_back({i, d[k].second}); }
  for (int a = 0; a < 3; a++) for (int b = a + 1; b < 3; b++) { int ca = nations[a].capital, cb = nations[b].capital; if (ca >= 0 && cb >= 0 && !has(ca, cb)) es.push_back({ca, cb}); }
  // gate directions: toward the first neighbour, capitals / forts / palisaded villages snap to an axis
  for (int i = 0; i < n; i++) { Settle& s = settles[i]; s.gateX = 0; s.gateZ = 1; for (const E& e : es) if (e.a == i || e.b == i) { int o = e.a == i ? e.b : e.a; float dx = settles[o].x - s.x, dz = settles[o].z - s.z, L = std::sqrt(dx * dx + dz * dz) + 1e-4f; s.gateX = dx / L; s.gateZ = dz / L; break; }
    if (s.type == ST_CAPITAL || s.type == ST_FORT || (s.type == ST_VILLAGE && s.size >= 2)) { if (std::fabs(s.gateX) > std::fabs(s.gateZ)) { s.gateX = s.gateX > 0 ? 1.f : -1.f; s.gateZ = 0; } else { s.gateZ = s.gateZ > 0 ? 1.f : -1.f; s.gateX = 0; } } }
  for (const E& e : es) { const Settle &A = settles[e.a], &B = settles[e.b]; float ax, az, bx, bz; gatePoint(A, ax, az); gatePoint(B, bx, bz); float dx = bx - ax, dz = bz - az, L = std::sqrt(dx * dx + dz * dz) + 1e-3f; int n2 = (int)(L / 8.f) + 2; float px = -dz / L, pz = dx / L;
    std::vector<V3> best; float bestC = 1e30f; for (int t = 0; t < 14; t++) { float amp = t == 0 ? 0.f : (rng() - 0.5f) * 2.f * std::min(160.f, L * 0.3f), ph = rng() * 6.28f, f2 = (float)(t % 3 + 1); std::vector<V3> pts(n2 + 1); float C = 0, prevH = 0;
      for (int i = 0; i <= n2; i++) { float u = (float)i / n2; float w = std::sin(u * PI) * amp + std::sin(u * PI * f2 * 2 + ph) * amp * 0.12f * std::sin(u * PI); float x = ax + dx * u + px * w, z = az + dz * u + pz * w; float h = height(x, z); pts[i] = {x, h, z};
        if (i > 0) { float dh = (h - prevH) / 8.f; C += dh * dh * 260.f + 8.f; } if (waterAt(x, z) > -1e8f && h < waterAt(x, z) + 0.3f) C += 70.f; if (h > 85) C += 40; if (std::fabs(x) > HALF - 40 || std::fabs(z) > HALF - 40) C += 500; prevH = h; }
      if (C < bestC) { bestC = C; best = pts; } }
    best.insert(best.begin(), V3{A.x, 0, A.z}); best.push_back(V3{B.x, 0, B.z}); for (V3& p : best) p.y = height(p.x, p.z); Road r; r.pts = best; r.a = e.a; r.b = e.b; roads.push_back(r); gradeRoad(best); }
  // apply grading
  for (int i = 0; i < GS * GS; i++) if (gradeW[i] > 0) Hh[i] = lerpf(Hh[i], gradeY[i], gradeW[i]);
  // border gates where the capital-capital roads change nation
  for (Road& r : roads) { int na = nationAt(r.pts[0].x, r.pts[0].z), nb = nationAt(r.pts.back().x, r.pts.back().z); if (na == nb || settles[r.a].type != ST_CAPITAL || settles[r.b].type != ST_CAPITAL) continue;
    for (size_t i = 1; i < r.pts.size(); i++) { int nn = nationAt(r.pts[i].x, r.pts[i].z); if (nn != na) { float dx = r.pts[i].x - r.pts[i - 1].x, dz = r.pts[i].z - r.pts[i - 1].z, L = std::sqrt(dx * dx + dz * dz) + 1e-4f; if (inWater(r.pts[i].x, r.pts[i].z)) break;
        Settle s; s.type = ST_BORDER; s.nation = na; s.poi = nn; s.x = r.pts[i].x; s.z = r.pts[i].z; s.y = height(s.x, s.z); s.r = 36.f; s.gateX = dx / L; s.gateZ = dz / L; snprintf(s.name, 28, "%s-%s Border", nations[na].name, nations[nn].name); s.poi = nn; settles.push_back(s); break; } } }
}
static void biomeTree(float x, float z, float zc, float zj) {
  float pr = rng(), s = 0.8f + rng() * 0.6f; float ry = rng() * TAU;
  if (zj > zc) {
    if (pr < 0.46f) put(K_JUNGLE, x, z, ry, s, 0.9f + rng() * 0.25f, 0.95f + rng() * 0.2f, 0.9f + rng() * 0.2f, 0, true, s * (0.9f + rng() * 0.5f));
    else if (pr < 0.74f) put(K_FERN, x, z, ry, 0.9f + rng() * 0.9f, 0.9f + rng() * 0.3f, 1.f, 0.9f, 0, false);
    else if (pr < 0.88f) put(K_FBUSH, x, z, ry, 0.9f + rng() * 0.7f, 1.f + rng() * 0.6f, 0.8f + rng() * 0.5f, 0.6f + rng() * 0.8f, 0, true);
    else put(K_PALM, x, z, ry, 0.9f + rng() * 0.4f, 1, 1, 1, 0, true, 0.95f + rng() * 0.5f);
  } else {
    if (pr < 0.60f) put(K_CHERRY, x, z, ry, s, 0.95f + rng() * 0.25f, 0.88f + rng() * 0.2f, 0.95f + rng() * 0.25f, 0, true, s * (0.9f + rng() * 0.35f));
    else if (pr < 0.76f) put(K_FBUSH, x, z, ry, 0.9f + rng() * 0.6f, 1.2f, 0.9f, 1.1f, 0, true);
    else if (pr < 0.90f) put(K_BIRCH, x, z, ry, 0.8f + rng() * 0.5f, 1, 1, 1, 0, true, 0.9f + rng() * 0.4f);
    else put(K_OAK, x, z, ry, 0.8f + rng() * 0.6f, 1.8f, 0.85f, 1.1f, 0, true);
  }
}
void generate(uint32_t seed) {
  if (done) return; done = true; SEED = seed; rng = Rng(seed * 9301u + 7u);
  Hh.assign(GS * GS, 0); Wl.assign(GS * GS, -1e9f); flowX.assign(GS * GS, 0); flowZ.assign(GS * GS, 0); roadD.assign(GS * GS, 99.f); gradeW.assign(GS * GS, 0); gradeY.assign(GS * GS, 0); fieldK.assign(GS * GS, 0); fieldA.assign(GS * GS, 0);
  for (int j = 0; j < GS; j++) for (int i = 0; i < GS; i++) Hh[j * GS + i] = baseH(-HALF + (i + 0.5f) * CS, -HALF + (j + 0.5f) * CS);
  makeLake(0.30f * HALF, 0.52f * HALF, 150.f, -1e9f, "Mirror Lake", 2); makeLake(-0.12f * HALF, -0.58f * HALF, 52.f, -1e9f, "Tarn of Stars", 2);
  for (int k = 0, made = 0; k < 900 && made < 12; k++) { float x = urand(-0.85f, 0.85f) * HALF, z = urand(-0.85f, 0.85f) * HALF; float h = height(x, z); if (h < 5.f || h > 55.f || slopeAt(x, z) > 0.2f) continue; if (std::sqrt((x - 0.30f * HALF) * (x - 0.30f * HALF) + (z - 0.52f * HALF) * (z - 0.52f * HALF)) < 400.f) continue; bool far = true; for (const Poi& p : pois) if ((p.x - x) * (p.x - x) + (p.z - z) * (p.z - z) < 380.f * 380.f) far = false; if (!far) continue; static const char* PN[] = {"Lily Pond", "Heron Mere", "Glasswater", "Kingfisher Tarn", "Swan Lake", "Duskmere", "Otter Pool", "Whisper Water", "Greenmere", "Sedge Pond", "Bluebell Mere", "Fenwater"}; makeLake(x, z, 26.f + urand(0.f, 46.f), -1e9f, PN[made % 12], 2); made++; }
  { int made = 0; for (int t = 0; t < 400 && made < 1; t++) { float x = urand(-0.6f, 0.6f) * HALF, z = urand(-0.75f, -0.35f) * HALF; if (height(x, z) > 60 && height(x, z) < 100 && slopeAt(x, z) < 0.25f) { makeLake(x, z, 36.f, -1e9f, "Frostmere", 2); made++; } } }
  // rivers from the high country
  { std::vector<V3> src; for (int t = 0; t < 9000 && nRiv < 15; t++) { float x = urand(-0.85f, 0.85f) * HALF, z = urand(-0.9f, 0.1f) * HALF; float h = height(x, z); if (h < 62.f || h > 175.f) continue; bool far = true; for (const V3& s : src) if ((s.x - x) * (s.x - x) + (s.z - z) * (s.z - z) < 260.f * 260.f) far = false; if (!far) continue; if (Wl[ci(cellOf(z), 0, GS - 1) * GS + ci(cellOf(x), 0, GS - 1)] > -1e8f) continue; src.push_back({x, 0, z}); traceRiver(x, z, nRiv); if (rivers[nRiv].size() > 60) nRiv++; else rivers[nRiv].clear(); } }
  carveRivers();
  for (int i = 0; i < GS * GS; i++) if (Hh[i] < SEA) Wl[i] = std::max(Wl[i], SEA);
  pickSites(); connect();
  // inner streets
  for (size_t i = 0; i < settles.size(); i++) { Settle& s = settles[i]; if (s.type > ST_FORT) continue; float gx, gz; gatePoint(s, gx, gz); Road r; V3 a{gx, height(gx, gz), gz}, b{s.x + s.gateX * (s.type == ST_CAPITAL ? 2.f : 0.f), 0, s.z + s.gateZ * (s.type == ST_CAPITAL ? 2.f : 0.f)}; b.y = height(b.x, b.z); r.pts.push_back(a); r.pts.push_back(b); r.a = r.b = (int)i; roads.push_back(r); std::vector<V3> pp = r.pts; gradeRoad(pp); }
  for (int i = 0; i < GS * GS; i++) if (gradeW[i] > 0) Hh[i] = lerpf(Hh[i], gradeY[i], gradeW[i]);
  // buildings
  for (size_t i = 0; i < settles.size(); i++) { switch (settles[i].type) { case ST_CAPITAL: buildCapital((int)i); break; case ST_VILLAGE: buildVillage((int)i); break; case ST_HAMLET: buildHamlet((int)i); break; case ST_FORT: buildFort((int)i); break; case ST_CAMP: buildCamp((int)i); break; case ST_RUINS: buildRuins((int)i); break; case ST_GRAVE: buildGrave((int)i); break; case ST_BORDER: buildBorder((int)i, settles[i].gateX, settles[i].gateZ); break; } }
  // fields around villages
  for (size_t i = 0; i < settles.size(); i++) { const Settle& s = settles[i]; if (s.type != ST_VILLAGE && s.type != ST_HAMLET) continue; int nf = s.type == ST_VILLAGE ? 4 + s.size : 3; for (int k = 0; k < nf; k++) for (int tr = 0; tr < 30; tr++) { float a = rng() * TAU, d = s.r + 10 + rng() * 55; float cx = s.x + std::cos(a) * d, cz = s.z + std::sin(a) * d; float fa = rng() * PI, hw = 12 + rng() * 8, hh = 9 + rng() * 6; float c = std::cos(fa), sn = std::sin(fa); bool ok = true; for (int q = 0; q < 9 && ok; q++) { float lx = (q % 3 - 1) * hw, lz = (q / 3 - 1) * hh; float x = cx + lx * c - lz * sn, z = cz + lx * sn + lz * c; if (wetNear(x, z, 3) || slopeAt(x, z) > 0.14f || roadDist(x, z) < 6.f || settleNear(x, z, 4) >= 0 && settleNear(x, z, 4) != (int)i) ok = false; }
      if (!ok) continue; for (const Obj& o : objs) if ((o.x - cx) * (o.x - cx) + (o.z - cz) * (o.z - cz) < (hw + 5) * (hw + 5) && o.kind >= K_COTTAGE) { ok = false; break; } if (!ok) continue; uint8_t kind = 1 + (uint8_t)(rng() * 3);
      int x0 = ci(cellOf(cx - hw - hh), 0, GS - 1), x1 = ci(cellOf(cx + hw + hh), 0, GS - 1), z0 = ci(cellOf(cz - hw - hh), 0, GS - 1), z1 = ci(cellOf(cz + hw + hh), 0, GS - 1); for (int j = z0; j <= z1; j++) for (int q = x0; q <= x1; q++) { float x = -HALF + (q + 0.5f) * CS - cx, z = -HALF + (j + 0.5f) * CS - cz; float lx = x * c + z * sn, lz = -x * sn + z * c; if (std::fabs(lx) < hw && std::fabs(lz) < hh) { fieldK[j * GS + q] = kind; fieldA[j * GS + q] = fa; } }
      for (int e = 0; e < 6; e++) { float lx = -hw + (e + 0.5f) * (2 * hw / 6); for (int sd = -1; sd <= 1; sd += 2) { float x = cx + lx * c - sd * hh * sn, z = cz + lx * sn + sd * hh * c; if (!inWater(x, z)) put(K_SFENCE, x, z, std::atan2(-sn * 0 - c * 0 + (-sn), c) * 0 + (-fa), 1.2f); } } break; } }
  // landmarks
  { float hx, hz; float hh = highestPoint(&hx, &hz); char nm[32]; snprintf(nm, 32, "Skyspire Peak"); addPoi(nm, hx, hz, 45.f, 4); put(K_BEACON, hx, hz, 0, 1.4f); put(K_BRAZIER, hx + 3, hz, 0, 1.4f); (void)hh; }
  for (const Fall& f : falls) { Poi p; memset(&p, 0, sizeof p); p.x = f.x; p.z = f.z; p.y = f.yTop; p.r = 45; snprintf(p.name, 32, "%s", f.name); p.kind = 3; pois.push_back(p); }
  for (size_t i = 0; i < settles.size(); i++) { Settle& s = settles[i]; if (s.type == ST_BORDER) continue; Poi p; memset(&p, 0, sizeof p); p.x = s.x; p.z = s.z; p.y = s.y; p.r = s.type == ST_CAPITAL ? 70.f : s.r + 8.f; snprintf(p.name, 32, "%s", s.name); p.kind = s.type == ST_CAPITAL ? 0 : s.type == ST_VILLAGE ? 1 : s.type == ST_HAMLET ? 1 : s.type == ST_FORT ? 5 : s.type == ST_CAMP ? 6 : 7; pois.push_back(p); s.poi = (int)pois.size() - 1; }
  { float x = 0, z = 0; int best = -1; for (int t = 0; t < 800 && best < 0; t++) { x = urand(-0.7f, 0.7f) * HALF; z = urand(-0.5f, 0.6f) * HALF; if (height(x, z) > 6 && height(x, z) < 40 && slopeAt(x, z) < 0.12f && !wetNear(x, z, 28) && roadDist(x, z) > 20 && settleNear(x, z, 40) < 0) best = 1; }
    if (best > 0) { for (int k = 0; k < 9; k++) { float a = k * TAU / 9; put(K_RUNE, x + std::cos(a) * 9.f, z + std::sin(a) * 9.f, a, 3.6f, 0.8f, 1.1f, 1.3f); } Obj* t = put(K_OAK, x, z, 0, 3.2f, 1.2f, 1.1f, 0.8f); t->sway = 1; addPoi("The Elder Tree", x, z, 22.f, 8); put(K_CAMP, x + 5, z + 5, 0, 1.2f); lairs.push_back({x, z, 30, 4}); } }
  for (size_t i = 0; i < settles.size(); i++) if (settles[i].type == ST_BORDER) { Poi p; memset(&p, 0, sizeof p); p.x = settles[i].x; p.z = settles[i].z; p.y = settles[i].y; p.r = 36; snprintf(p.name, 32, "%s", settles[i].name); p.kind = 9; pois.push_back(p); }
  // beacon towers on high ground between nations
  for (int t = 0, c = 0; t < 2400 && c < 12; t++) { float x = urand(-0.8f, 0.8f) * HALF, z = urand(-0.8f, 0.6f) * HALF; float h = height(x, z); if (h < 38 || h > 90 || slopeAt(x, z) > 0.3f || wetNear(x, z, 8) || settleNear(x, z, 60) >= 0 || roadDist(x, z) < 8) continue; put(K_BEACON, x, z, rng() * TAU, 1.3f); put(K_BRAZIER, x + 3, z, 0, 1.2f); c++; }
  // forests, bushes, rocks, big mountain rocks
  for (float gz = -HALF + 6; gz < HALF - 6; gz += 4.4f) for (float gx = -HALF + 6; gx < HALF - 6; gx += 4.4f) {
    float x = gx + (rng() - 0.5f) * 4.2f, z = gz + (rng() - 0.5f) * 4.2f; float h = height(x, z); if (h < 1.6f || Wl[ci(cellOf(z), 0, GS - 1) * GS + ci(cellOf(x), 0, GS - 1)] > -1e8f) continue; float rd = roadDist(x, z); if (rd < 6.5f) continue; if (settleNear(x, z, 12) >= 0) continue;
    float sl = slopeAt(x, z); float fm = forestMask(x, z, h); float q = fb(x * 0.0022f + 50, z * 0.0022f, 3, SEED + 41); float r1 = rng();
    if (sl > 0.95f) { if (r1 < 0.2f && h > 25) { float s = 3.f + rng() * 5.f; Obj* o = put(K_BIGROCK, x, z, rng() * TAU, s, 0.8f + rng() * 0.4f, 0.8f + rng() * 0.35f, 0.8f + rng() * 0.3f, -s * 0.15f); o->sy = s * (0.8f + rng() * 0.8f); } continue; }
    if (sl > 0.55f && h > 20 && r1 < 0.05f) { float s = 2.f + rng() * 6.f; Obj* o = put(K_BIGROCK, x, z, rng() * TAU, s, 0.8f + rng() * 0.4f, 0.8f + rng() * 0.3f, 0.8f + rng() * 0.3f, -s * 0.2f); o->sy = s * (0.7f + rng() * 0.7f); continue; }
    float zc0, zj0, zb0, za0; zones(x, z, h, zc0, zj0, zb0, za0);
    if (r1 < fm * 0.62f && sl < 0.8f && h < 52.f && (zc0 > 0.5f || zj0 > 0.5f)) biomeTree(x, z, zc0, zj0);
    else if (r1 < fm * 0.62f && sl < 0.8f) {
      float pr = rng(); bool high = h > 52.f || q > 0.64f;
      if (high) { if (h > 98.f && pr < 0.5f) continue; if (pr < 0.07f) put(K_DEAD, x, z, rng() * TAU, 0.9f + rng() * 0.6f, 0.9f, 0.9f, 0.9f, 0, true); else if (pr < 0.35f) put(K_PINEY, x, z, rng() * TAU, 0.8f + rng() * 0.8f, 0.85f + rng() * 0.25f, 0.9f + rng() * 0.25f, 0.85f, 0, true); else put(K_PINE, x, z, rng() * TAU, 0.75f + rng() * 0.85f, 0.85f + rng() * 0.3f, 0.9f + rng() * 0.25f, 0.85f + rng() * 0.15f, 0, true); }
      else if (q < 0.30f) { if (pr < 0.8f) put(K_OAK, x, z, rng() * TAU, 0.8f + rng() * 0.7f, 1.7f + rng() * 0.5f, 0.85f + rng() * 0.25f, 1.15f + rng() * 0.3f, 0, true); else put(K_PINEY, x, z, rng() * TAU, 0.8f + rng() * 0.5f, 1, 1, 1, 0, true); }
      else if (q < 0.43f) { if (pr < 0.75f) put(K_OAK, x, z, rng() * TAU, 0.8f + rng() * 0.7f, 1.8f + rng() * 0.4f, 1.0f + rng() * 0.3f, 0.35f + rng() * 0.25f, 0, true); else put(K_PINE, x, z, rng() * TAU, 0.8f + rng() * 0.6f, 1, 1, 1, 0, true); }
      else { if (pr < 0.5f) put(K_OAK, x, z, rng() * TAU, 0.8f + rng() * 0.7f, 0.85f + rng() * 0.35f, 1.0f + rng() * 0.25f, 0.8f + rng() * 0.25f, 0, true); else if (pr < 0.82f) put(K_PINE, x, z, rng() * TAU, 0.75f + rng() * 0.8f, 0.9f + rng() * 0.2f, 0.95f + rng() * 0.2f, 0.9f, 0, true); else put(K_PINEY, x, z, rng() * TAU, 0.8f + rng() * 0.6f, 1, 1.05f, 0.9f, 0, true); }
    } else if (r1 < fm * 0.62f + 0.03f && sl < 0.6f) { if (rng() < 0.7f) put(K_BUSH, x, z, rng() * TAU, 0.9f + rng() * 0.8f, q < 0.3f ? 1.6f : 0.9f + rng() * 0.3f, q < 0.3f ? 0.9f : 1.f, q < 0.3f ? 1.3f : 0.85f, 0, true); else put(rng() < 0.5f ? K_BOULDER : K_THORN, x, z, rng() * TAU, 0.8f + rng() * 1.6f, 0.9f, 0.9f, 0.9f, 0, true); }
    else if (r1 > 0.995f) { int w = (int)(rng() * 3); put(w == 0 ? K_BOULDER : w == 1 ? K_LOG : K_CAIRN, x, z, rng() * TAU, 0.9f + rng() * 1.3f, 1, 1, 1, 0, true); }
  }
  // v1.9 flora II: shore reeds + lily pads, willows, palms, birch groves, ferns + mushrooms, flowering shrubs, mountain spires
  for (float gz = -HALF + 8; gz < HALF - 8; gz += 3.4f) for (float gx = -HALF + 8; gx < HALF - 8; gx += 3.4f) {
    float x = gx + (rng() - 0.5f) * 3.2f, z = gz + (rng() - 0.5f) * 3.2f; int gi = ci(cellOf(z), 0, GS - 1) * GS + ci(cellOf(x), 0, GS - 1); float h = Hh[gi], lvl = Wl[gi], r = rng();
    if (lvl > -1e8f) { float depth = lvl - h; float fl = std::fabs(flowX[gi]) + std::fabs(flowZ[gi]);
      if (depth > 0.05f && depth < 0.6f && r < 0.14f) put(K_REEDS, x, z, rng() * TAU, 0.8f + rng() * 0.7f, 0.9f + rng() * 0.2f, 1.f, 0.9f, 0, false, 0.8f + rng() * 0.8f);
      else if (depth > 0.35f && depth < 1.7f && fl < 0.08f && r > 0.955f) { Obj* o = put(K_LILY, x, z, rng() * TAU, 0.8f + rng() * 0.6f, 1, 1, 1, 0, false); o->y = lvl + 0.02f; }
      continue; }
    if (h < 1.5f || r > 0.2f) continue; if (roadDist(x, z) < 5.f || settleNear(x, z, 10) >= 0) continue;
    float sl = slopeAt(x, z); float zc, zj, zb, za; zones(x, z, h, zc, zj, zb, za); float fm = forestMask(x, z, h); float rr = rng();
    if (sl > 0.65f && sl < 1.5f && h > 30.f) { if (rr < 0.006f && h > 80.f) { float s = 2.f + rng() * 2.5f; Obj* o = put(K_SPIRE, x, z, rng() * TAU, s, 0.85f + rng() * 0.3f, 0.85f + rng() * 0.25f, 0.85f + rng() * 0.25f, -s * 0.25f); o->sy = s * (0.8f + rng() * 0.9f); o->sx = o->sz = s * 0.6f; } continue; }
    if (sl > 0.7f) continue;
    if (h < 40.f && rr < 0.30f && wetNear(x, z, 6.f)) put(K_WILLOW, x, z, rng() * TAU, 0.9f + rng() * 0.6f, 0.95f + rng() * 0.2f, 1.f, 0.95f, 0, true, 0.9f + rng() * 0.4f);
    else if (h < 3.6f && sl < 0.3f && rr < 0.30f) put(K_PALM, x, z, rng() * TAU, 0.9f + rng() * 0.4f, 1, 1, 1, 0, true, 0.9f + rng() * 0.6f);
    else if (zb > 0.35f && rr < 0.55f * zb) put(K_BIRCH, x, z, rng() * TAU, 0.8f + rng() * 0.6f, 0.95f + rng() * 0.15f, 1.f, 0.95f, 0, true, 0.9f + rng() * 0.5f);
    else if (fm > 0.45f && rr < 0.26f) put(K_FERN, x, z, rng() * TAU, 0.7f + rng() * 0.9f, 0.9f + rng() * 0.3f, 1.f, 0.9f, 0, false);
    else if (fm > 0.55f && rr < 0.32f) put(K_MUSHROOM, x, z, rng() * TAU, 0.7f + rng() * 0.8f, 1, 1, 1, 0, false);
    else if (fm < 0.3f && rr < 0.04f && h < 60.f) { int c = (int)(rng() * 4); put(K_FBUSH, x, z, rng() * TAU, 0.8f + rng() * 0.6f, c == 0 ? 1.5f : c == 1 ? 1.1f : 0.9f, c == 0 ? 0.8f : c == 1 ? 1.3f : 1.f, c == 2 ? 1.4f : 0.8f, 0, true); }
  }
  // roadside: signposts + lamps at crossings; sign at each gate point
  for (size_t i = 0; i < settles.size(); i++) { const Settle& s = settles[i]; if (s.type > ST_FORT) continue; float gx, gz; gatePoint(s, gx, gz); put(K_SIGN, gx + s.gateZ * 4.f, gz - s.gateX * 4.f, std::atan2(s.gateX, s.gateZ), 1.2f); }
  // monster hot spots in the wilds
  for (int t = 0, c = 0; t < 1600 && c < 30; t++) { float x = urand(-0.85f, 0.85f) * HALF, z = urand(-0.85f, 0.7f) * HALF; float h = height(x, z); if (h < 6 || wetNear(x, z, 10) || settleNear(x, z, 120) >= 0 || roadDist(x, z) < 30) continue; lairs.push_back({x, z, 40.f, h > 60 ? 5 : 0}); c++; }
  buildColGrid(); if (getenv("LF_DUMP")) { for (size_t k = 0; k < settles.size() && k < 12; k++) fprintf(stderr, "SETTLE %zu type %d nat %d at %.1f %.1f y %.1f gate %.2f %.2f %s\n", k, settles[k].type, settles[k].nation, settles[k].x, settles[k].z, settles[k].y, settles[k].gateX, settles[k].gateZ, settles[k].name); for (size_t k = 0; k < falls.size(); k++) fprintf(stderr, "FALL %zu %s at %.1f %.1f top %.1f bot %.1f w %.1f\n", k, falls[k].name, falls[k].x, falls[k].z, falls[k].yTop, falls[k].yBot, falls[k].w); fprintf(stderr, "DECKS %d\n", nDecks);
    for (size_t k = 0; k + 4 < stairLog.size() && k < 40; k += 5) { float sx0 = stairLog[k], sz0 = stairLog[k + 1], ex = stairLog[k + 2], ez = stairLog[k + 3], by = stairLog[k + 4]; float y = height(sx0 - ex * 1.5f, sz0 - ez * 1.5f); float px = sx0 - ex * 1.5f, pz = sz0 - ez * 1.5f; int pushed = 0;
      for (float sd = -1.5f; sd < 18.f; sd += 0.06f) { float nx = sx0 + ex * sd, nz = sz0 + ez * sd; float ox = nx, oz = nz; collide(nx, nz, 0.42f, y); if (std::fabs(nx - ox) + std::fabs(nz - oz) > 0.01f) pushed++; y = surfaceY(nx, nz, y); px = nx; pz = nz; if (k == 0 && std::fmod(sd + 1.5f, 1.f) < 0.06f) fprintf(stderr, "  sd %.1f y-base %.2f deckTop-base %.2f push(%.2f,%.2f)\n", sd, y - by, deckTop(nx, nz) - by, nx - ox, nz - oz); }
      float bestTop = -1e9f; for (int sg = -1; sg <= 1; sg += 2) { float qx = sx0 + ex * 13.2f, qz = sz0 + ez * 13.2f, qy = by + 5.9f; float mxy = qy; for (float t = 0; t < 3.4f; t += 0.06f) { float nx = qx - ez * sg * 0.06f, nz = qz + ex * sg * 0.06f; collide(nx, nz, 0.42f, qy); qx = nx; qz = nz; qy = surfaceY(qx, qz, qy); if (t < 2.5f) mxy = std::min(mxy, qy); if (k == 0 && t > 2.0f && t < 2.07f && qy > by + 5.f) fprintf(stderr, "WALLPT %.1f %.1f dir %.2f %.2f y %.1f\n", qx, qz, ex, ez, qy); } bestTop = std::max(bestTop, mxy); }
      fprintf(stderr, "STAIRTEST %zu endY-base %.2f pushed %d wallwalkReach %.2f\n", k / 5, y - by, pushed, bestTop - by); } }
  // ---------------- textures
  std::vector<float>().swap(gradeW); std::vector<float>().swap(gradeY);
  waterTex.assign((size_t)GS * GS * 4, 0); std::vector<float> wb;
  { wb = Wl; std::vector<float> nb(GS * GS); for (int pass = 1; pass < 7; pass++) { nb = wb; for (int j = 1; j < GS - 1; j++) for (int i = 1; i < GS - 1; i++) { float m = wb[j * GS + i]; for (int dj = -1; dj <= 1; dj++) for (int di = -1; di <= 1; di++) m = std::max(m, wb[(j + dj) * GS + i + di]); nb[j * GS + i] = m; } wb.swap(nb); } }
  { auto enc = [](float v) { return (uint16_t)(clampf((v + 128.f) * (1.f / 1024.f), 0.f, 1.f) * 65535.f + 0.5f); }; auto encF = [](float v) { return (uint16_t)(clampf(v * 0.5f + 0.5f, 0.f, 1.f) * 65535.f + 0.5f); };
    for (int i = 0; i < GS * GS; i++) { float h = Hh[i]; float lvl = Wl[i]; float f = 0; if (lvl > -1e8f) f = 1; float outv = lvl > -1e8f ? lvl : (wb[i] > -1e8f ? std::min(h - 1.5f, wb[i]) : h - 1.5f); waterTex[i * 4] = enc(outv); waterTex[i * 4 + 1] = encF(flowX[i]); waterTex[i * 4 + 2] = encF(flowZ[i]); waterTex[i * 4 + 3] = (uint16_t)(f * 65535.f); } }
  std::vector<float>().swap(flowX); std::vector<float>().swap(flowZ);
  {
    infoA.assign((size_t)GS * GS * 4, 255); infoB.assign((size_t)GS * GS * 4, 0);
    for (int j = 0; j < GS; j++) for (int i = 0; i < GS; i++) { int id = j * GS + i; float x = -HALF + (i + 0.5f) * CS, z = -HALF + (j + 0.5f) * CS; float h = Hh[id]; float hxm = Hh[j * GS + std::max(0, i - 1)], hxp = Hh[j * GS + std::min(GS - 1, i + 1)], hzm = Hh[std::max(0, j - 1) * GS + i], hzp = Hh[std::min(GS - 1, j + 1) * GS + i]; float sl = std::sqrt((hxp - hxm) * (hxp - hxm) + (hzp - hzm) * (hzp - hzm)) / (2 * CS);
      float n1 = fb(x * 0.02f, z * 0.02f, 3, SEED + 21), n2 = fb(x * 0.2f, z * 0.2f, 2, SEED + 22), n3 = fb(x * 0.006f, z * 0.006f, 2, SEED + 23); float fm = forestMask(x, z, h); float q = fb(x * 0.0022f + 50, z * 0.0022f, 3, SEED + 41);
      V3 meadow = mix3(V3{0.25f, 0.38f, 0.13f}, V3{0.42f, 0.46f, 0.18f}, n1); if (q < 0.30f && fm > 0.3f) meadow = mix3(meadow, V3{0.50f, 0.58f, 0.30f}, 0.4f); V3 col = mix3(meadow, V3{0.16f, 0.27f, 0.10f}, fm * 0.75f); float grass = 1.f - 0.4f * fm;
      float alp = smooth(45.f, 88.f, h + (n1 - 0.5f) * 24.f); col = mix3(col, V3{0.40f, 0.45f, 0.28f}, alp * 0.75f);
      { float zc, zj, zb, za; zones(x, z, h, zc, zj, zb, za); col = mix3(col, V3{0.13f, 0.40f, 0.09f}, zj * 0.75f); col = mix3(col, V3{0.46f, 0.52f, 0.30f}, zc * 0.35f); if (n2 > 0.58f) col = mix3(col, V3{0.82f, 0.60f, 0.66f}, zc * 0.4f * smooth(0.58f, 0.7f, n2)); col = mix3(col, V3{0.46f, 0.56f, 0.22f}, zb * 0.35f); col = mix3(col, V3{0.58f, 0.42f, 0.16f}, za * 0.28f * fm); }
      float rk = smooth(0.50f, 0.92f, sl + (n2 - 0.5f) * 0.28f); V3 rock = mix3(V3{0.34f, 0.31f, 0.28f}, V3{0.50f, 0.47f, 0.42f}, n2); col = mix3(col, rock, rk); grass *= 1.f - rk;
      float sn = smooth(104.f, 128.f, h + (n1 - 0.5f) * 34.f - sl * 20.f); col = mix3(col, V3{0.92f, 0.94f, 0.97f}, sn); grass *= 1.f - sn;
      float wl = wb[id]; float dw = wl > -1e8f ? h - wl : 9.f; float sand = smooth(1.9f, 0.1f, dw) * (1.f - sn) * (1.f - rk) * (1.f - smooth(10.f, 26.f, h)); float wetB = smooth(1.5f, 0.05f, dw) * (1.f - sand); if (h < 3.4f && h > -1) sand = std::max(sand, smooth(3.4f, 1.f, h) * 0.9f); col = mix3(col, mix3(V3{0.78f, 0.70f, 0.48f}, V3{0.70f, 0.62f, 0.42f}, n2), sand); grass *= 1.f - sand; col = col * (1.f - 0.38f * wetB);
      if (Wl[id] > -1e8f && h < Wl[id]) { col = mix3(V3{0.30f, 0.26f, 0.19f}, V3{0.20f, 0.2f, 0.16f}, n2); grass = 0; }
      float rd = roadD[id]; float rm = smooth(2.9f, 1.6f, rd); if (rm > 0) { V3 rc = mix3(V3{0.50f, 0.41f, 0.29f}, V3{0.42f, 0.36f, 0.28f}, n2); col = mix3(col, rc, rm * 0.95f); grass *= 1.f - rm; }
      int si = settleNear(x, z, 0); if (si >= 0) { const Settle& s = settles[si]; float dd = std::sqrt((x - s.x) * (x - s.x) + (z - s.z) * (z - s.z)); if (s.type == ST_CAPITAL) { float w2 = smooth(s.r * 0.98f, s.r * 0.5f, dd); col = mix3(col, mix3(V3{0.46f, 0.42f, 0.36f}, V3{0.38f, 0.35f, 0.30f}, n2), w2 * 0.8f); grass *= 1.f - w2 * 0.9f; } else if (s.type == ST_VILLAGE || s.type == ST_HAMLET || s.type == ST_FORT) { float w2 = smooth(s.r * 0.55f, s.r * 0.15f, dd); col = mix3(col, V3{0.44f, 0.38f, 0.28f}, w2 * 0.55f); grass *= 1.f - w2 * 0.7f; } else if (s.type == ST_CAMP || s.type == ST_RUINS || s.type == ST_GRAVE) { float w2 = smooth(s.r * 0.9f, s.r * 0.3f, dd); col = mix3(col, V3{0.22f, 0.20f, 0.14f}, w2 * 0.5f); grass *= 1.f - w2 * 0.6f; } }
      float flower = smooth(0.50f, 0.66f, fb(x * 0.012f, z * 0.012f, 3, SEED + 31)) * (1.f - fm) * (1.f - alp * 0.8f) * grass; if (q < 0.30f && fm > 0.2f) flower = std::max(flower, 0.5f * grass);
      uint8_t fk = fieldK[id]; float fa = fieldA[id]; if (fk) { V3 fc = fk == 1 ? V3{0.42f, 0.30f, 0.17f} : fk == 2 ? V3{0.80f, 0.66f, 0.26f} : V3{0.36f, 0.56f, 0.20f}; col = mix3(col, fc, 0.92f); grass = 0; flower = 0; }
      (void)n3; auto b8 = [](float v) { return (uint8_t)clampf(v * 255.f + 0.5f, 0, 255); }; infoA[id * 4] = b8(col.x); infoA[id * 4 + 1] = b8(col.y); infoA[id * 4 + 2] = b8(col.z); infoA[id * 4 + 3] = b8(grass);
      infoB[id * 4] = b8(flower); infoB[id * 4 + 1] = b8(fm); infoB[id * 4 + 2] = b8(fa / PI); infoB[id * 4 + 3] = fk * 60; } }
  // minimap (4 m / px)
  mapRGBA.assign((size_t)MAPN * MAPN * 4, 255); for (int j = 0; j < MAPN; j++) for (int i = 0; i < MAPN; i++) { int gi = std::min(GS - 1, i * 2), gj = std::min(GS - 1, j * 2); float h = Hh[gj * GS + gi]; int id = gj * GS + gi; V3 c{infoA[id * 4] / 255.f, infoA[id * 4 + 1] / 255.f, infoA[id * 4 + 2] / 255.f};
    float hs = clampf(0.5f + (Hh[gj * GS + std::min(GS - 1, gi + 2)] - Hh[gj * GS + std::max(0, gi - 2)] + Hh[std::max(0, gj - 2) * GS + gi] - Hh[std::min(GS - 1, gj + 2) * GS + gi]) * 0.05f, 0.25f, 1.15f); c = c * (hs * 1.3f + 0.25f);
    if (Wl[id] > -1e8f && h < Wl[id] + 0.05f) c = mix3(V3{0.10f, 0.28f, 0.50f}, V3{0.18f, 0.42f, 0.62f}, clampf((Wl[id] - h) * -0.2f + 1.f, 0, 1)); else if (h < 0.f) c = V3{0.08f, 0.22f, 0.42f};
    mapRGBA[(j * MAPN + i) * 4] = (uint8_t)(clampf(c.x, 0, 1) * 255); mapRGBA[(j * MAPN + i) * 4 + 1] = (uint8_t)(clampf(c.y, 0, 1) * 255); mapRGBA[(j * MAPN + i) * 4 + 2] = (uint8_t)(clampf(c.z, 0, 1) * 255); }
  std::vector<float>().swap(wb);
  ecoF.assign((GS / 4) * (GS / 4), 0); for (int j = 0; j < GS / 4; j++) for (int i = 0; i < GS / 4; i++) { int id = (j * 4 + 2) * GS + i * 4 + 2; ecoF[j * (GS / 4) + i] = (infoB[id * 4] >= 55 && infoA[id * 4 + 3] >= 90) ? 1 : 0; }
  { float lo = 1e9f, hi = -1e9f; for (int i = 0; i < GS * GS; i += 7) { lo = std::min(lo, Hh[i]); hi = std::max(hi, Hh[i]); } fprintf(stderr, "world heights %.1f .. %.1f m (texture range -128..896)\n", lo, hi); }
  Wq.resize((size_t)GS * GS); for (size_t i = 0; i < Wq.size(); i++) Wq[i] = Wl[i] < -1e8f ? (int16_t)-32768 : (int16_t)clampf(Wl[i] * 32.f, -32000.f, 32000.f);
  roadQ.resize((size_t)GS * GS); for (size_t i = 0; i < roadQ.size(); i++) roadQ[i] = (uint8_t)clampf(roadD[i] * 8.f + 0.5f, 0.f, 255.f);
  { std::vector<float>* big[] = {&Wl, &flowX, &flowZ, &roadD, &gradeW, &gradeY, &fieldA}; for (auto* v : big) std::vector<float>().swap(*v); std::vector<uint8_t>().swap(fieldK); }
  fprintf(stderr, "world: %zu objects, %zu settlements, %zu roads, %zu falls, %zu pois, %zu posts, %zu lights, %zu colliders\n", objs.size(), settles.size(), roads.size(), falls.size(), pois.size(), posts.size(), lights.size(), cols.size());
}
}  // namespace xw
