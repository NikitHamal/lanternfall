// Explorer mode: a living, day/night 3D world to walk through (villages, nations, castles, forests, mountains, rivers, waterfalls),
// populated by villagers, guards, knights, patrols, wildlife and monsters.
#pragma once
#include "play.h"
#include <cstdint>
namespace ex {
struct Label { float x = 0, y = 0; char text[44] = {0}; float hp = -1; uint32_t col = 0xffffff; };
struct Hud {
  float hp = 100, maxHp = 100, st = 100, hour = 10, bannerT = 0, promptT = 0, dlgT = 0, hurt = 0, timeMul = 1, mapPx = 0, mapPy = 0, mapSize = 0, yaw = 0, locT = 0, bigT = 0, fade = 0, bossHp = -1;
  float rain = 0, draw = 0, hitT = 0, zoom = 0, mana = 100, maxMana = 100; int hitZone = 0, weapon = 0, arrows = 30, wanted = 0, gold = 0, murders = 0; bool hitKill = false, inside = false, bowOut = false;
  int wood = 90, stone = 95, food = 40, spell = 0, bsel = 0, troops = 0, order = 0, siegeLeft = 0, nightWave = 0, reclaimed = 0, totalRifts = 0;
  bool buildMode = false, ghostOk = false, mounted = false;
  int kills = 0, disc = 0, discTotal = 0, view = 1, nNpc = 0, nMon = 0; bool map = false, fly = false, show = true, dead = false;
  bool talkOpen = false; int talkN = 0, potions = 0, wpnTier = 0, armorTier = 0, qN = 0; bool talkOk[8] = {0}, qReady[3] = {0};
  char talkName[44] = {0}, talkLine[140] = {0}, talkOpt[8][56] = {{0}}, qTitle[3][40] = {{0}}, qProg[3][56] = {{0}};
  char place[48] = {0}, nation[34] = {0}, banner[48] = {0}, bannerSub[68] = {0}, prompt[52] = {0}, dlgName[40] = {0}, dlgText[110] = {0}, hero[28] = {0}, bossName[28] = {0}, big[40] = {0};
};
extern Hud H; extern Label labels[44]; extern int nLabels; extern bool pending;
bool active(); void resize(int w, int h); void start(); void stop(); void update(float dt, const pl::Input& in); void draw(); void look(float dx, float dy); void scroll(float dy); void keyPress(int key); float danger();
void debugPlace(float x, float z, float yawDeg, float pitchDeg); void debugHour(float h); void debugPoi(int i); void debugCam(float dist); void debugFly(bool on); void debugView(int v); void debugKillDay(); void debugCmd(const char* c); void debugMap(bool on);
bool mapToScreen(float wx, float wz, float& sx, float& sy); int poiCount(); const char* poiName(int i); bool poiFound(int i); void poiPos(int i, float& x, float& z);
}
