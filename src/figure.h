// Skinned figures: Blender-processed meshes (.lfm) + baked reference animation, GPU skinning, weapon sockets, static Polyfork props, surface material shader.
#pragma once
#include "gl.h"
#include "mathx.h"
#include "data.h"
constexpr int FMAXB = 32, NWEAP = 8, NPFP = 20;
enum PfPropId {
  PF_PINE_TALL, PF_PINE_YOUNG, PF_TWISTED_OAK, PF_DEAD_TREE, PF_BOULDER, PF_CAIRN, PF_THORN_BUSH, PF_ROUND_BUSH,
  PF_FALLEN_LOG, PF_PLANK_BRIDGE, PF_BRAZIER, PF_LAMP_POST, PF_SIGNPOST, PF_CAMPFIRE, PF_BANNER, PF_IRON_FENCE,
  PF_GATE, PF_CHAPEL, PF_WATCHTOWER, PF_COTTAGE
};
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
extern FigAsset gFH[NCLS], gFM[NMON], gFMAlt[9], gFW[NWEAP], gFP[NPFP];
extern const float IDENT_BONE[12];
bool figInit(const char* argv0); const char* figAssetDir();
void figShutdown();
bool figLoad(FigAsset& a, const char* key); void figFree(FigAsset& a); int figBone(const FigAsset& a, const char* name, int fallback);
struct FigAnim {
  uint32_t id = 0xffffffffu; float t = 0, ph = 0, moveW = 0, readyW = 0, downW = 0, guardW = 0, dodgeW = 0, actW = 0, actLastU = 0, stagK = 9, dim = 1;
  int actLast = -1, stagClip = -1;
  float pose[FMAXB * 7]; bool have = false, shat = false, dying = false;
  float fR[FMAXB * 9], fp[FMAXB * 3], fv[FMAXB * 3], fax[FMAXB * 3], fw[FMAXB], ffl[FMAXB]; bool frest[FMAXB];
  float mat[FMAXB * 12];
};
int figActClip(const FigAsset& a, int kind);
void figAnimUpdate(FigAnim& an, const FigAsset& a, float dt, float speed, bool combat, int actClip, float actU, float stagV, bool down, float guard = 0, float dodgeU = -1, float hitDir = 0);
void figAnimPose(FigAnim& an, const FigAsset& a);            // pose -> bone matrices
void figAddRot(FigAnim& an, const FigAsset& a, int root, float ax, float ay, float az, float ang, const float* pivot);
void figAddMove(FigAnim& an, const FigAsset& a, int root, float dx, float dy, float dz);
float figClipU(const FigAsset& a, const char* clip, float u, FigAnim& an, float w);
void figShatter(FigAnim& an, const FigAsset& a, float power, float dx, float dz);
void figStepFree(FigAnim& an, const FigAsset& a, float dt);
void figWeaponMat(const FigAsset& rig, const FigAnim& an, bool left, int wpId, const M4& unitModel, float time, M4& out);
struct FigDraw { const FigAsset* a; M4 model; const float* mat; float flash[3], dim, dissolve, disH, disCol[3], glow; bool lantern, shadow; };
void figPoint(const FigAsset& a, const float* mat, int bone, float x, float y, float z, float* out);
void figDraw(GLuint prog, const FigDraw* d, int n, bool shadowPass);
extern GLuint pFig, pFigShadow;
extern const char* FIG_VS; extern const char* FIG_FS_BODY; extern const char* FIG_SHADOW_VS;
