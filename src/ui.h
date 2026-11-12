#pragma once
#include "game.h"
namespace ui { bool init(); void draw(int w, int h, float dt); bool click(float x, float y); bool key(int c); bool captureKey(int k); bool optionsOpen(); void closeOptions(); }
