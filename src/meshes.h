// Procedural meshes: props (rocks, standing stones, trees, cairns, walls, fences, waylight), primitive shapes and the character part tables.
#pragma once
#include "gl.h"
#include "mathx.h"
#include "data.h"
#include <vector>
struct Vtx { float p[3], n[3], c[3], m[3]; };
struct Inst { float m[16]; float tint[4]; float mat[4]; };
struct Mesh { GLuint vao = 0, vbo = 0, ibo = 0, inst = 0; int nIdx = 0, cap = 0, count = 0; };
enum PropKind { PK_ROCK, PK_STAND, PK_TREE, PK_CAIRN, PK_WALL, PK_FENCE, PK_WAY, NPK };
inline const int PROP_VARIANTS[NPK] = {5, 4, 5, 3, 4, 4, 1};
extern Mesh gProp[NPK][5]; extern Mesh gShape[4];  // sphere box cyl cone
enum Shape { SH_SPHERE, SH_BOX, SH_CYL, SH_CONE };
enum Bone { BN_ROOT, BN_HEAD, BN_LARM, BN_RARM, BN_LLEG, BN_RLEG, BN_LLEG2, BN_RLEG2, BN_TAIL, BN_JAW, NBONE };
struct Part { uint8_t shape, bone; float x, y, z, sx, sy, sz, rx, rz; V3 col; float rough, metal, glow; };
struct Figure { std::vector<Part> parts; float pivot[NBONE][3]; float lanX = 0, lanY = 0, lanZ = 0; };
extern Figure gHero[NCLS], gMon[NMON];
void buildMeshes();
void meshUpload(Mesh& m, const Inst* d, int n);
void meshDraw(const Mesh& m);
