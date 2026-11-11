#pragma once
#include "game.h"
#include "gl.h"
namespace gfx {
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
