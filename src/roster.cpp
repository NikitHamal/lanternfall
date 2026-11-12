#include "roster.h"
#include "settings.h"
#include <cstdio>
namespace roster {
using H = Hero;
// key, name, role, weapon, active, activeDesc, passive, passiveDesc,  draw stab dmg move hp quiver abil pass cd dur nativeIx
static const H TAB[] = {
 {"fletcher", "FLETCHER", "RANGER", "LONGBOW", "RAPID VOLLEY", "DRAW SPEED X1.8 FOR 5 S", "STEADY HANDS", "AIM SWAY REDUCED BY A THIRD", 1.00f, 1.25f, 1.00f, 1.00f, 100, 16, AB_VOLLEY, PS_STEADY, 14, 5, FLETCHER},
 {"warden", "WARDEN", "TANK", "SWORD, SHIELD, LASHED BOW", "BULWARK", "TAKE 70% LESS FOR 4 S, SHOVE FOES BACK", "STEEL SKIN", "ALL DAMAGE TAKEN -15%", 0.85f, 1.00f, 1.15f, 0.92f, 135, 12, AB_BULWARK, PS_STEEL, 16, 4, WARDEN},
 {"reaver", "REAVER", "HEAVY MELEE", "GREATSWORD, HEAVY DRAW", "SURGE", "DASH 9 M WITH I-FRAMES, CRUSH FOES IN THE WAY", "BLOODLUST", "+12% MOVE SPEED, +20% HEAD DAMAGE", 0.90f, 0.85f, 1.35f, 1.05f, 120, 10, AB_SURGE, PS_BLOOD, 9, 0.45f, REAVER},
 {"lamplighter", "LAMPLIGHTER", "FIRE CASTER", "LANTERN POLE", "EMBER BURST", "RING OF FIRE: 40 DAMAGE WITHIN 7 M", "KINDLED", "ARROWS IGNITE FOES FOR EXTRA DAMAGE", 1.00f, 1.00f, 1.00f, 1.00f, 100, 14, AB_EMBER, PS_KINDLED, 12, 0.4f, LAMPLIGHTER},
 {"apothecary", "APOTHECARY", "SUPPORT", "FLASK BELT", "TONIC", "HEAL 40, REFILL STAMINA, REGEN 4 S", "FIELD MEDIC", "SLOW HEALTH REGENERATION", 1.05f, 1.10f, 0.85f, 1.05f, 100, 18, AB_TONIC, PS_MEDIC, 15, 4, APOTHECARY},
 {"rimebinder", "RIMEBINDER", "FROST CASTER", "SHARD STAFF", "FROST NOVA", "SLOW AND STAGGER FOES WITHIN 10 M", "RIME TIP", "ARROWS CHILL FOES ON HIT", 0.95f, 1.10f, 1.00f, 1.00f, 95, 14, AB_FROST, PS_RIME, 14, 4, RIMEBINDER},
 {"polyfork/fletcher", "ASHEN ASSASSIN", "RANGER", "SHORTBOW", "RAPID VOLLEY", "DRAW SPEED X1.8 FOR 5 S", "STEADY HANDS", "AIM SWAY REDUCED BY A THIRD", 1.15f, 1.00f, 0.95f, 1.08f, 90, 15, AB_VOLLEY, PS_STEADY, 13, 5, -1},
 {"polyfork/warden", "KNIGHT OF THE FORGE", "TANK", "SWORD AND KITE SHIELD", "BULWARK", "TAKE 70% LESS FOR 4 S, SHOVE FOES BACK", "STEEL SKIN", "ALL DAMAGE TAKEN -15%", 0.80f, 1.10f, 1.20f, 0.90f, 140, 12, AB_BULWARK, PS_STEEL, 16, 4, -1},
 {"polyfork/reaver", "VARKA", "HEAVY MELEE", "GREAT CLEAVER", "SURGE", "DASH 9 M WITH I-FRAMES, CRUSH FOES IN THE WAY", "BLOODLUST", "+12% MOVE SPEED, +20% HEAD DAMAGE", 0.85f, 0.90f, 1.40f, 1.05f, 125, 10, AB_SURGE, PS_BLOOD, 9, 0.45f, -1},
 {"polyfork/lamplighter", "FIRE CULTIST", "FIRE CASTER", "EMBER LANTERN", "EMBER BURST", "RING OF FIRE: 40 DAMAGE WITHIN 7 M", "KINDLED", "ARROWS IGNITE FOES FOR EXTRA DAMAGE", 1.00f, 0.95f, 1.05f, 1.00f, 95, 14, AB_EMBER, PS_KINDLED, 12, 0.4f, -1},
 {"polyfork/apothecary", "FOREST WITCH", "SUPPORT", "HERB FLASKS", "TONIC", "HEAL 40, REFILL STAMINA, REGEN 4 S", "FIELD MEDIC", "SLOW HEALTH REGENERATION", 1.10f, 1.05f, 0.85f, 1.08f, 95, 18, AB_TONIC, PS_MEDIC, 15, 4, -1},
 {"polyfork/rimebinder", "ORDER JUDGE", "FROST CASTER", "RIME STAFF", "FROST NOVA", "SLOW AND STAGGER FOES WITHIN 10 M", "RIME TIP", "ARROWS CHILL FOES ON HIT", 0.95f, 1.15f, 1.00f, 0.98f, 100, 14, AB_FROST, PS_RIME, 14, 4, -1},
 {"polyfork/thornling", "BLOOM ELEMENTAL", "SKIRMISHER", "THORN BOW", "RAPID VOLLEY", "DRAW SPEED X1.8 FOR 5 S", "LIGHT FOOT", "+15% MOVE SPEED", 1.20f, 0.90f, 0.80f, 1.15f, 80, 20, AB_VOLLEY, PS_BLOOD, 12, 5, -1},
 {"polyfork/peatwight", "BOG FIEND", "BRUISER", "BOG BOW", "QUAKE", "GROUND SLAM: 30 DAMAGE AND STAGGER WITHIN 8 M", "PEAT HIDE", "ALL DAMAGE TAKEN -15%", 0.80f, 0.95f, 1.25f, 0.92f, 150, 10, AB_QUAKE, PS_STEEL, 15, 0.5f, -1},
 {"polyfork/rustbound", "BELL WARDEN", "SENTINEL", "IRON BOW", "BULWARK", "TAKE 70% LESS FOR 4 S, SHOVE FOES BACK", "STEEL SKIN", "ALL DAMAGE TAKEN -15%", 0.78f, 1.20f, 1.30f, 0.90f, 150, 10, AB_BULWARK, PS_STEEL, 16, 4, -1},
 {"polyfork/cairnback", "CATHEDRAL STATUE", "SENTINEL", "STONE BOW", "QUAKE", "GROUND SLAM: 30 DAMAGE AND STAGGER WITHIN 8 M", "TITAN", "AIM SWAY REDUCED, HEAVY ARROWS", 0.75f, 1.30f, 1.35f, 0.88f, 160, 8, AB_QUAKE, PS_TITAN, 15, 0.5f, -1},
 {"polyfork/rootcolossus", "ROOT COLOSSUS", "JUGGERNAUT", "ROOT LONGBOW", "QUAKE", "GROUND SLAM: 30 DAMAGE AND STAGGER WITHIN 8 M", "TITAN", "AIM SWAY REDUCED, HEAVY ARROWS", 0.70f, 1.20f, 1.45f, 0.85f, 180, 8, AB_QUAKE, PS_TITAN, 15, 0.5f, -1},
};
const int N = (int)(sizeof TAB / sizeof TAB[0]);
static int cur_ = 0; static struct Slot { int idx = -1; FigAsset a; } slot[2]; static bool inited = false;
const Hero& at(int i) { return TAB[((i % N) + N) % N]; }
int sel() { return cur_; }
static FigAsset* resident(int i) { i = ((i % N) + N) % N; if (TAB[i].nativeIx >= 0) return gFH[TAB[i].nativeIx].ok ? &gFH[TAB[i].nativeIx] : nullptr; for (auto& s : slot) if (s.idx == i && s.a.ok) return &s.a; return nullptr; }
static void need(int i, int keep) {
  i = ((i % N) + N) % N; if (TAB[i].nativeIx >= 0 || resident(i)) return;
  Slot* s = nullptr; for (auto& q : slot) if (q.idx < 0) { s = &q; break; }
  if (!s) for (auto& q : slot) if (q.idx != keep) { s = &q; break; }
  if (!s) s = &slot[0]; if (s->a.ok || s->a.vao) figFree(s->a); s->idx = i;
  char path[128]; snprintf(path, sizeof path, "%s", TAB[i].key); if (!figLoad(s->a, path)) { fprintf(stderr, "roster: %s failed to load, falling back\n", path); s->idx = -1; }
}
void select(int i) {
  int prev = cur_;
  cur_ = ((i % N) + N) % N; cfg::S.hero = cur_; need(cur_, cur_); need(cur_ + 1, cur_);
  // release anything that is neither current nor next so memory stays flat
  for (auto& s : slot) if (s.idx >= 0 && s.idx != cur_ && s.idx != (cur_ + 1) % N) { figFree(s.a); s.idx = -1; }
  if (inited && cur_ != prev) cfg::save();
}
void step(int d) { select(cur_ + d); }
const FigAsset* fig(int i) { return resident(i); }
const FigAsset* cur() { const FigAsset* a = resident(cur_); return a ? a : &gFH[FLETCHER]; }
void init() { if (inited) return; inited = true; select(cfg::S.hero); }
void shutdown() { for (auto& s : slot) { figFree(s.a); s.idx = -1; } }
}
