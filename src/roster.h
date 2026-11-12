// Playable roster: 6 native heroes + Polyfork heroes + bestiary bipeds across 6 distinct combat archetypes.
// Only the current and next extra hero figures are resident (lazy load, previous released).
#pragma once
#include "figure.h"
namespace roster {
enum Style { ST_BOW, ST_SWORD, ST_HEAVY, ST_FIRE, ST_ALCH, ST_FROST, NSTYLE };
enum Ability { AB_VOLLEY, AB_BULWARK, AB_SURGE, AB_EMBER, AB_TONIC, AB_FROST, AB_QUAKE, NAB };
enum Passive { PS_NONE, PS_STEADY, PS_STEEL, PS_BLOOD, PS_KINDLED, PS_MEDIC, PS_RIME, PS_TITAN };
enum WeaponId { WP_NONE = -1, WP_SWORD = 0, WP_GREATSWORD, WP_BOW, WP_KITE, WP_ROUND, WP_STAFF, WP_CLEAVER, WP_LANTERN, NWP };
struct Hero {
  const char *key, *name, *role, *weapon, *active, *activeDesc, *passive, *passiveDesc;
  float draw, stab, dmg, move, hp;
  int quiver, style, abil, pass;
  float cd, dur;
  int nativeIx, clsIx, wpR, wpL;
};
extern const int N;
const Hero& at(int i);
int sel();
void select(int i);
void step(int d);            // select() persists the choice and re-balances resident figures
const FigAsset* fig(int i);  // null if not resident
const FigAsset* cur();       // currently selected figure (never null once init() ran)
const char* styleName(int st);
const char* styleHint(int st);   // controls line for the HUD
const char* styleTip(int st);    // banner sub-title
void init();
void shutdown();
}
