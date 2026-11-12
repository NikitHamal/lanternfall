// Explorer renderer: clipmap terrain (vertex-pulled from a height texture), water, sky + day/night, instanced props with LOD, GPU grass + flowers,
// waterfalls, shadows, skinned figures and the HDR post chain (fog / AO / bloom / AgX).
#pragma once
#include "mathx.h"
#include "figure.h"
namespace xr {
struct Env { V3 sunDir, keyDir, keyCol, zen, hor, gnd, hemiSky, hemiGnd, fog, sunCol; float night = 0, day = 1, dens = 0.001f, exposure = 0.8f, bloom = 0.3f, glow = 0.2f, tw = 0, hour = 10, sunVis = 1, cover = 0.5f; };
Env env(float hour);
struct Frame { V3 eye, tgt; float fov = 1.1f, hour = 10, time = 0, flash = 0, fade = 1; };
struct MapView { float cx, cz, half, rot; int mode; float px, py, size; float yaw; };
bool init(); void resize(int w, int h); void render(const Frame& f, const FigDraw* figs, int nfig); void drawMap(const MapView& v);
bool project(float x, float y, float z, float& sx, float& sy);
extern int drawCalls; extern M4 viewM, projM, vpM; extern float lastDt;
}
