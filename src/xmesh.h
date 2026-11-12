// Procedural static meshes for the explorer (walls, towers, gatehouses, keeps, houses, bridges, tents, tree LODs, rocks) in the .lfm vertex layout.
#pragma once
#include "mathx.h"
#include <vector>
#include <cstdint>
#pragma pack(push, 1)
struct LV { float p[3]; int8_t n[3]; int8_t pad; uint8_t c[4]; float uv[2]; uint8_t m[4]; uint8_t b[4]; };   // 36 bytes == .lfm vertex
#pragma pack(pop)
static_assert(sizeof(LV) == 36, "LV layout");
constexpr int XM_LOCONI = 100, XM_LOBROAD = 101, XM_LOBIRCH = 102, XM_LOCHERRY = 103, XM_LOJUNGLE = 104, XM_LOWILLOW = 105;
bool xmBuild(int kind, std::vector<LV>& v, std::vector<uint16_t>& idx);   // kinds are xw::Kind values or XM_*; false when the kind is a polyfork asset
