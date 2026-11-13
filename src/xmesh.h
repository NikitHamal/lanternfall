// Procedural static meshes for the explorer (walls, towers, gatehouses, keeps, houses, bridges, tents, tree LODs, rocks) in the .lfm vertex layout.
#pragma once
#include "mathx.h"
#include <vector>
#include <cstdint>
#pragma pack(push, 1)
struct LV { float p[3]; int8_t n[3]; int8_t pad; uint8_t c[4]; float uv[2]; uint8_t m[4]; uint8_t b[4]; };   // 36 bytes == .lfm vertex
#pragma pack(pop)
static_assert(sizeof(LV) == 36, "LV layout");
constexpr int XM_LOCONI = 200, XM_LOBROAD = 201, XM_LOBIRCH = 202, XM_LOCHERRY = 203, XM_LOJUNGLE = 204, XM_LOWILLOW = 205, XM_LOROCK = 206;
bool xmBuild(int kind, std::vector<LV>& v, std::vector<uint16_t>& idx);   // kinds are xw::Kind values or XM_*; false when the kind is a polyfork asset
bool xmBuildHuman(int variant, std::vector<LV>& v, std::vector<uint16_t>& idx);   // v2.8 citizen kit (variant < xmHumanCount()), skinned to the polyfork/warden 25-bone rig
int xmHumanCount();
