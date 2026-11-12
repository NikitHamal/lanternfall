// Persistent player settings + action map (keybinds). Saved to save/settings.json next to the working directory.
#pragma once
#include <cstdint>
namespace cfg {
enum Action { ACT_FWD, ACT_BACK, ACT_LEFT, ACT_RIGHT, ACT_SPRINT, ACT_ROLL, ACT_VIEW, ACT_ASSIST, ACT_COUNT };
struct Settings {
  float sens = 1.0f;        // look sensitivity multiplier (0.25 .. 3)
  float fov = 70.f;         // base first-person vertical FOV in degrees (55 .. 100)
  float shake = 1.0f;       // camera shake 0 .. 1
  float master = 0.8f;      // master volume 0 .. 1
  bool invertY = false, assist = true, reduceMotion = false, colorblind = false, toggleAim = false, subtitles = true;
  int reticle = 0;          // 0 cross, 1 dot, 2 ring, 3 chevrons
  int hero = 0;             // last picked roster index
  int key[ACT_COUNT];
};
extern Settings S;
void defaults();            // reset every value (incl. keybinds)
void defaultKeys();
void load();                // reads save/settings.json if present
void save();
const char* actionName(int a);
void keyName(int key, char* out, int n);
inline float effShake() { return S.reduceMotion ? 0.f : S.shake; }
}
