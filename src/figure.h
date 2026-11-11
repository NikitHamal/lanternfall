// Skinned figures: Blender-processed meshes (.lfm) + baked reference animation, GPU skinning, surface material shader.
#pragma once
#include "gl.h"
#include "mathx.h"
#include "data.h"
constexpr int FMAXB = 32;
struct FigClip { char name[13]; int frames, loop; float fps, hit, speed, T; const int16_t* d; };
struct FigFx { char name[13]; int bone; float p[3]; };
struct FigAsset {
  bool ok = false; char key[17] = {0}, death[13] = {0}; int nv = 0, ni = 0, nb = 0, nclips = 0, nfx = 0, baseNV = 0, baseNI = 0;
  float scale = 1, radius = 0.4f, hpY = 1.8f, height = 2, disCol[3] = {1, 0.45f, 0.1f}; bool hero = false, flying = false, lant = false;
  int8_t parent[FMAXB]; float head[FMAXB][3]; char bone[FMAXB][13]; FigFx fx[8]; FigClip clips[16]; int16_t* clipMem = nullptr;
  GLuint vao = 0, vbo = 0, ibo = 0;
  int clipIx(const char* n) const { for (int i = 0; i < nclips; i++) if (!strcmp(clips[i].name, n)) return i; return -1; }
  int fxIx(const char* n) const { for (int i = 0; i < nfx; i++) if (!strcmp(fx[i].name, n)) return i; return -1; }
};
extern FigAsset gFH[NCLS], gFM[NMON];
bool figInit(const char* argv0); const char* figAssetDir();
void figShutdown();
struct FigAnim {
  uint32_t id = 0xffffffffu; float t = 0, ph = 0, moveW = 0, readyW = 0, downW = 0, stagK = 9, dim = 1, pose[FMAXB * 7]; bool have = false, shat = false, dying = false;
  float fR[FMAXB * 9], fp[FMAXB * 3], fv[FMAXB * 3], fax[FMAXB * 3], fw[FMAXB], ffl[FMAXB]; bool frest[FMAXB];
  float mat[FMAXB * 12];
};
// kinds: A_* of ActKind. returns clip index or -1
int figActClip(const FigAsset& a, int kind);
void figAnimUpdate(FigAnim& an, const FigAsset& a, float dt, float speed, bool combat, int actClip, float actU, float stagV, bool down);
void figAnimPose(FigAnim& an, const FigAsset& a);            // pose -> bone matrices
void figShatter(FigAnim& an, const FigAsset& a, float power, float dx, float dz);
void figStepFree(FigAnim& an, const FigAsset& a, float dt);
struct FigDraw { const FigAsset* a; M4 model; const float* mat; float flash[3], dim, dissolve, disH, disCol[3], glow; bool lantern, shadow; };
void figPoint(const FigAsset& a, const float* mat, int bone, float x, float y, float z, float* out);  // model-space point rigidly attached to a bone
void figDraw(GLuint prog, const FigDraw* d, int n, bool shadowPass);
extern GLuint pFig, pFigShadow;
extern const char* FIG_VS; extern const char* FIG_FS_BODY; extern const char* FIG_SHADOW_VS;
