// Explorer world: procedural 2 km x 2 km continent (mountains, forests, meadows, rivers, lakes, waterfalls, three nations with
// capitals / villages / forts / camps / ruins, roads, bridges, border gates, walls). Everything is generated once at start.
#pragma once
#include "mathx.h"
#include <vector>
#include <cstdint>
namespace xw {
constexpr int GS = 2048; constexpr float CS = 2.f, HALF = 2048.f, SEA = 0.f, WORLD = 4096.f; constexpr int MAPN = 1024; constexpr float KS = HALF / 1024.f;   // KS: world scale vs the v2.4 2 km map
// object kinds (polyfork props/buildings + procedural meshes, see xrender.cpp kind table)
enum Kind : uint8_t {
  K_PINE, K_PINEY, K_OAK, K_DEAD, K_BOULDER, K_CAIRN, K_BUSH, K_THORN, K_LOG, K_SIGN, K_CAMP, K_BRAZIER, K_LAMP, K_POLE, K_IFENCE, K_BARREL, K_CRATE, K_WELL, K_STALL,
  K_HEAD, K_GRAVE, K_RUNE, K_SFENCE, K_FWALL, K_LWALL, K_TORCH, K_COTTAGE, K_BARN, K_CHAPEL, K_WATCH, K_STOWER, K_BEACON, K_RGABLE, K_RWALL, K_CGATE,
  K_WALL, K_WTOWER, K_GATE, K_KEEP, K_HOUSE, K_LONG, K_PALI, K_BRIDGE, K_TENT, K_HAY, K_MILL, K_BANNER, K_DOCK, K_BIGROCK, K_FLAGPOLE, K_ARROW, K_INHOME, K_INHALL, K_INCHAPEL,
  K_ORE, K_RIFT, K_PURIFIED, K_SPELL_FIRE, K_SPELL_FROST,
  // v1.9 living world: flora + articulated fauna parts (procedural, see xmesh_new.inc)
  K_BIRCH, K_CHERRY, K_JUNGLE, K_WILLOW, K_PALM, K_FERN, K_MUSHROOM, K_REEDS, K_LILY, K_SPIRE, K_FBUSH,
  K_FA_DOG, K_FA_CAT, K_FA_HEN, K_FA_SHEEP, K_FA_COW, K_FA_HORSE, K_FA_BIRD, K_FA_LEG, K_FA_WING, K_PETAL, K_STAIR,
  // v2.6 anatomy-driven fauna parts (xmesh_fauna.inc): bodies, neck+head, tails, 2-bone legs, saddle
  K_AN_BHORSE, K_AN_BCOW, K_AN_BSHEEP, K_AN_BDOG, K_AN_BCAT, K_AN_BDEER, K_AN_BHEN, K_AN_BRABBIT, K_AN_HHORSE, K_AN_HCOW, K_AN_HSHEEP, K_AN_HDOG, K_AN_HCAT, K_AN_HDEER,
  K_AN_THORSE, K_AN_TCOW, K_AN_TDOG, K_AN_TCAT, K_AN_UPF, K_AN_UPH, K_AN_LOH, K_AN_LOP, K_AN_SADDLE, NK
};
struct Obj { float x, y, z, ry, sx, sy, sz; uint8_t kind, r, g, b, sway; };      // tint bytes: 128 == 1.0
struct Settle { int type = 0, nation = 0, size = 1; float x = 0, z = 0, y = 0, r = 30, gateX = 0, gateZ = 1; char name[28] = {0}; int poi = -1; };   // type: 0 capital 1 village 2 hamlet 3 fort 4 camp 5 ruins 6 graveyard 7 border gate
enum { ST_CAPITAL, ST_VILLAGE, ST_HAMLET, ST_FORT, ST_CAMP, ST_RUINS, ST_GRAVE, ST_BORDER };
struct Nation { char name[20], title[28]; uint32_t col; float x, z; int capital = -1; };
struct Fall { float x, z, yTop, yBot, w, dx, dz; char name[28]; };
struct Poi { float x, y, z, r; char name[32]; int kind; bool found; };
struct Light { float x, y, z, r, g, b, range; };
struct Post { float x, y, z, face; int settle, type; float ax, az, bx, bz; };   // type: 0 gate guard, 1 wall patrol (ax,az -> bx,bz at height y), 2 plaza knight, 3 villager home, 4 vendor, 5 sentry
struct Road { std::vector<V3> pts; int a, b; };
struct Lair { float x, z, r; int kind; };      // monster hot spots (camps, ruins, graveyards, deep forest, mountains)
extern std::vector<float> Hh, Wl; extern std::vector<uint16_t> Hq;                   // heights, real water level (-1e9: none)
extern std::vector<uint8_t> mapRGBA;  // ground albedo + grass, flowers / forest floor / field angle / field kind; minimap
void bakeRows(int j0, int n, uint8_t* A, uint8_t* B, uint16_t* W); void bakeEnd(); extern std::vector<int16_t> Wq; extern std::vector<uint8_t> roadQ, ecoF; bool flowery(float x, float z); void releaseGen(); double cpuMB();                 // RGBA32F: level (dry: terrain-1.5), flow x, flow z, speed
extern std::vector<Obj> objs; extern std::vector<Settle> settles; extern Nation nations[3]; extern std::vector<Fall> falls; extern std::vector<Poi> pois;
extern std::vector<Light> lights; extern std::vector<Post> posts; extern std::vector<Road> roads; extern std::vector<Lair> lairs;
void generate(uint32_t seed);
float height(float x, float z); float slopeAt(float x, float z); float waterAt(float x, float z);   // waterAt: level or -1e9
bool inWater(float x, float z, float* depth = nullptr); float roadDist(float x, float z); int nationAt(float x, float z); int settleNear(float x, float z, float margin = 0);
const char* regionName(float x, float z); bool collide(float& x, float& z, float r, float y = -1e9f); void auditKind(int k, const float* vp, size_t nv, size_t stride, const uint16_t* ix, size_t ni);   // y: entity feet height; colliders with top below y-0.4 are stepped over (wall walks)
float surfaceY(float x, float z, float fromY); float deckTop(float x, float z); extern int nDecks;   // walkable decks: wall walks, stairs, landings
//   // push out of colliders (returns true if moved)
float highestPoint(float* hx, float* hz);
void zones(float x, float z, float h, float& cherry, float& jungle, float& birch, float& autumn);   // biome weights 0..1 (flora zones)
float forestAt(float x, float z);
}
