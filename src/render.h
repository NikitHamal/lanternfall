#pragma once
#include "game.h"
#include "gl.h"
#include "meshes.h"
namespace gfx {
// play-mode hooks: camera override, world collision (props + terrain), instanced primitive batches (world / view-model)
struct CamO { bool on = false; V3 eye, tgt; float fov = 1.f, nearP = 0.12f, focus = 10, ap = 0; }; extern CamO camO;
bool rayWorld(float ax, float ay, float az, float bx, float by, float bz, float& t);   // first hit of a segment against props + terrain (t in 0..1)
void pushOut(float& x, float& z, float r);
constexpr int SHCAP = 640; extern Inst shW[4][SHCAP], shV[4][160]; extern int shWn[4], shVn[4];
bool init(int quality); void resize(int winW, int winH); void setQuality(int q); void shutdown();
void update(float dtReal, float vdt, bool paused, float alpha);
void draw();
bool project(float x, float y, float z, float& sx, float& sy);
bool pickGround(float px, float py, float& x, float& z);
Unit* pickUnit(float px, float py);
void addWaylight(float x, bool lit); void lightLastWaylight(); void clearWaylights(); void shiftWorld(float dx); void setReach(int i, bool instant); void resetCamera();
GLuint program(const char* vs, const char* fs, const char* name);
extern int winW, winH, quality, W, H; extern bool closeView; extern float uiDim, time_; extern const char* qualityName[4]; extern int drawCalls;
}
