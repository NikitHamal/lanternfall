#include "xrender.h"
#include "xworld.h"
#include "xmesh.h"
#include "render.h"
#include "shaders.h"
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cmath>
namespace xr {
float indoor = 0.f;
int drawCalls = 0; M4 viewM, projM, vpM; float lastDt = 0.016f;
namespace {
using namespace xw;
constexpr int NL = 7, TN = 128, NC = 32; constexpr float CELL = 64.f;
struct IData { float m[16]; float tint[4]; };
struct PMesh { GLuint vao = 0, vbo = 0, ibo = 0, inst = 0; int ni = 0, cap = 0; bool ok = false; };
struct KindDef { const char* key; int lo; float rHi, rLo; bool shadow; };
constexpr int SLOT_CONI = NK, SLOT_BROAD = NK + 1, NSLOT = NK + 2;
const KindDef KD[NK] = {
  {"polyfork/p_pine_tall", SLOT_CONI, 150, 560, true}, {"polyfork/p_pine_young", SLOT_CONI, 110, 420, true}, {"polyfork/p_twisted_oak", SLOT_BROAD, 140, 520, true}, {"polyfork/p_dead_tree", -1, 150, 0, true},
  {"polyfork/p_boulder", -1, 170, 0, true}, {"polyfork/p_cairn", -1, 120, 0, true}, {"polyfork/p_round_bush", -1, 80, 0, false}, {"polyfork/p_thorn_bush", -1, 80, 0, false}, {"polyfork/p_fallen_log", -1, 100, 0, false},
  {"polyfork/p_signpost", -1, 160, 0, true}, {"polyfork/p_campfire_tripod", -1, 220, 0, false}, {"polyfork/p_brazier", -1, 220, 0, true}, {"polyfork/p_lamp_post", -1, 200, 0, true}, {"polyfork/p_banner_pole", -1, 260, 0, true},
  {"polyfork/p_iron_fence", -1, 170, 0, false}, {"polyfork/p_barrel", -1, 110, 0, true}, {"polyfork/p_crate", -1, 110, 0, true}, {"polyfork/p_stone_well", -1, 220, 0, true}, {"polyfork/b_market_stall", -1, 240, 0, true},
  {"polyfork/p_headstone", -1, 170, 0, false}, {"polyfork/p_grave_mound", -1, 170, 0, false}, {"polyfork/p_rune_stone", -1, 220, 0, true}, {"polyfork/p_split_fence", -1, 140, 0, false}, {"polyfork/p_field_wall", -1, 170, 0, false},
  {"polyfork/p_low_wall", -1, 170, 0, false}, {"polyfork/p_wall_torch", -1, 100, 0, false}, {"polyfork/b_thatch_cottage", -1, 420, 0, true}, {"polyfork/b_timber_barn", -1, 420, 0, true}, {"polyfork/b_stone_chapel", -1, 480, 0, true},
  {"polyfork/b_watchtower", -1, 480, 0, true}, {"polyfork/b_stone_tower", -1, 420, 0, true}, {"polyfork/b_beacon_tower", -1, 800, 0, true}, {"polyfork/b_ruined_gable", -1, 480, 0, true}, {"polyfork/b_ruined_wall", -1, 380, 0, true},
  {"polyfork/p_cemetery_gate", -1, 260, 0, true},
  {nullptr, -1, 600, 0, true}, {nullptr, -1, 800, 0, true}, {nullptr, -1, 800, 0, true}, {nullptr, -1, 1400, 0, true}, {nullptr, -1, 420, 0, true}, {nullptr, -1, 460, 0, true}, {nullptr, -1, 360, 0, false}, {nullptr, -1, 600, 0, true},
  {nullptr, -1, 320, 0, true}, {nullptr, -1, 260, 0, false}, {nullptr, -1, 700, 0, true}, {nullptr, -1, 480, 0, true}, {nullptr, -1, 300, 0, true}, {nullptr, -1, 1000, 0, true}, {nullptr, -1, 700, 0, true}, {nullptr, -1, 300, 0, false}, {nullptr, -1, 260, 0, true}, {nullptr, -1, 260, 0, true}, {nullptr, -1, 260, 0, true},
  {nullptr, -1, 340, 0, true}, {nullptr, -1, 560, 0, true}, {nullptr, -1, 560, 0, true}, {nullptr, -1, 260, 0, false}, {nullptr, -1, 260, 0, false},
  // v1.9 flora: birch, cherry, jungle, willow, palm, fern, mushroom, reeds, lily, spire, flowering bush
  {nullptr, -1, 520, 0, true}, {nullptr, -1, 560, 0, true}, {nullptr, -1, 600, 0, true}, {nullptr, -1, 520, 0, true}, {nullptr, -1, 480, 0, true}, {nullptr, -1, 90, 0, false}, {nullptr, -1, 80, 0, false}, {nullptr, -1, 150, 0, false}, {nullptr, -1, 130, 0, false}, {nullptr, -1, 900, 0, true}, {nullptr, -1, 160, 0, false},
  // fauna: dog, cat, hen, sheep, cow, horse, bird, leg, wing, petal
  {nullptr, -1, 260, 0, true}, {nullptr, -1, 200, 0, true}, {nullptr, -1, 160, 0, true}, {nullptr, -1, 300, 0, true}, {nullptr, -1, 340, 0, true}, {nullptr, -1, 360, 0, true}, {nullptr, -1, 420, 0, false}, {nullptr, -1, 300, 0, true}, {nullptr, -1, 420, 0, false}, {nullptr, -1, 60, 0, false}};
static_assert(sizeof(KD) / sizeof(KD[0]) == NK, "kind table size");
PMesh pm[NSLOT]; FigAsset propAsset[NK]; std::vector<IData> data[NK]; std::vector<int> cellStart[NK]; std::vector<IData> stage[NSLOT]; int stageN[NSLOT];
GLuint pTerr, pWater, pSky, pProp, pPropShadow, pGrass, pFall, pMist, pFogP, pDownP, pFinalP, pMapP, pFigX, vaoEmpty;
GLuint texH, texInfoA, texInfoB, texWater, texMap, texSh, fboSh; GLuint terrIbo[2], terrVao; int terrNI[2];
GLuint grassVao, grassVbo, grassIbo, flowerVao, flowerVbo, flowerIbo, fallVao, fallVbo, mistVao, mistVbo; int grassNI, flowerNI, fallNI;
GLuint fboMS, texMSc, texMSd, fboScene, texScene, texDepth, fboA, texA, fboB[4], texB[4]; int bw[4], bh[4], W = 2, H = 2, curMsaa = -1, winW = 1280, winH = 720; bool inited = false;
constexpr int SHS = 2048; M4 shadowVP; Env E; V3 camPos; M4 camW, invProj, invVP; bool hasProj = false;
struct Q { int msaa; float grass; bool ao, shadow; float cap; }; const Q QS[4] = {{0, 0.35f, false, false, 1.2e6f}, {2, 0.6f, true, true, 2.1e6f}, {4, 1.f, true, true, 3.7e6f}, {4, 1.f, true, true, 8.3e6f}};

std::string V(const char* a, const char* b = "", const char* c = "", const char* d = "") { return std::string("#version 330 core\n") + a + b + c + d; }
GLint U(GLuint p, const char* n) { return glGetUniformLocation(p, n); }
void s1(GLuint p, const char* n, float v) { glUniform1f(U(p, n), v); } void s2(GLuint p, const char* n, float a, float b) { glUniform2f(U(p, n), a, b); } void s3(GLuint p, const char* n, V3 v) { glUniform3f(U(p, n), v.x, v.y, v.z); }
void si(GLuint p, const char* n, int v) { glUniform1i(U(p, n), v); } void sm(GLuint p, const char* n, const M4& m) { glUniformMatrix4fv(U(p, n), 1, GL_FALSE, m.m); }

// ------------------------------------------------------------------ shaders
std::string xlight() {   // the original LIGHT chunk with a daylight-aware environment function
  std::string L = sh::LIGHT; size_t a = L.find("vec3 envSky(vec3 n)"), b = L.find("float D_GGX"); if (a == std::string::npos || b == std::string::npos) return L;
  const char* env = "uniform vec3 uSkyZen, uSkyHor, uSkyGnd, uSunDirE, uSunColE;\nvec3 envSky(vec3 n){ float h = n.y; vec3 c = mix(uSkyHor, uSkyZen, smoothstep(0.,.9,h)); c = mix(c, uSkyGnd, smoothstep(0.,-.35,h)); c += uSunColE*pow(max(dot(n,uSunDirE),0.),20.)*.1; return c; }\n";
  return L.substr(0, a) + env + L.substr(b);
}
const char* TERR_VS = R"GLSL(
uniform sampler2D uHeight; uniform vec2 uCenter; uniform float uSp, uBias, uWorld; uniform int uN; uniform mat4 uVP; out vec3 vWp;
float Hf(vec2 xz){ return textureLod(uHeight, xz/uWorld+.5, 0.).r; }
void main(){ int id = gl_VertexID; int i = id % (uN+1), j = id / (uN+1); vec2 g = vec2(float(i - uN/2), float(j - uN/2)); vec2 xz = uCenter + g*uSp; float h = Hf(xz);
  vec2 u = abs(g)/(float(uN)*.5); float m = smoothstep(.70,.96,max(u.x,u.y));
  if (m > 0.) { vec2 ge = floor(g*.5)*2.; vec2 f = (g-ge)*.5; vec2 b = uCenter + ge*uSp; float s2 = 2.*uSp; float hc = mix(mix(Hf(b),Hf(b+vec2(s2,0.)),f.x), mix(Hf(b+vec2(0.,s2)),Hf(b+vec2(s2)),f.x), f.y); h = mix(h, hc, m); }
  vWp = vec3(xz.x, h - uBias, xz.y); gl_Position = uVP*vec4(vWp,1.); }
)GLSL";
const char* NOISE = R"GLSL(
float hs(vec2 p){ vec3 q = fract(vec3(p.xyx)*.1031); q += dot(q, q.yzx+33.33); return fract((q.x+q.y)*q.z); }
float hs3(vec3 p){ p = fract(p*.1031); p += dot(p, p.zyx+31.32); return fract((p.x+p.y)*p.z); }
float vn2(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.-2.*f); return mix(mix(hs(i),hs(i+vec2(1,0)),f.x),mix(hs(i+vec2(0,1)),hs(i+vec2(1,1)),f.x),f.y); }
float fbm2(vec2 p){ return .5*vn2(p)+.25*vn2(p*2.03+7.1)+.125*vn2(p*4.1+3.3)+.0625*vn2(p*8.3+1.7); }
)GLSL";
const char* TERR_FS = R"GLSL(
in vec3 vWp; out vec4 oC; uniform sampler2D uHeight, uInfoA, uInfoB; uniform float uWorld, uTime;
void main(){
  vec2 uv = vWp.xz/uWorld+.5; vec4 A = texture(uInfoA, uv), B = texture(uInfoB, uv); float e = 2.; float eu = e/uWorld;
  float hx = texture(uHeight, uv+vec2(eu,0.)).r - texture(uHeight, uv-vec2(eu,0.)).r, hz = texture(uHeight, uv+vec2(0.,eu)).r - texture(uHeight, uv-vec2(0.,eu)).r;
  vec3 N = normalize(vec3(-hx, 2.*e, -hz)); vec3 col = A.rgb*A.rgb; float d = length(vWp - uCam);
  float n1 = vn2(vWp.xz*.35), n2 = vn2(vWp.xz*1.7), n3 = vn2(vWp.xz*7.); float det = (.8 + .4*n1) * (.9 + .2*n2) * (.93 + .14*n3); det = mix(det, 1., smoothstep(90., 420., d)); col *= det;
  float slope = 1. - N.y; float strata = .5 + .5*sin(vWp.y*2.6 + n1*5.); col *= 1. - .16*strata*smoothstep(.35,.7,slope);
  if (B.a > .1) { float ang = B.b*3.14159; float st = .5 + .5*sin(dot(vWp.xz, vec2(cos(ang), sin(ang)))*5.9); col *= .78 + .36*st; }
  col *= 1. - .28*B.g; float sh = shadowAt(vWp, N); vec3 c = shade(col, .96, 0., N, vWp, sh); oC = vec4(c, 1.); }
)GLSL";
const char* SKY_VS = R"GLSL(
uniform mat4 uInvVP; uniform vec3 uCam; out vec3 vDir;
void main(){ vec2 p = vec2((gl_VertexID==1)?3.:-1., (gl_VertexID==2)?3.:-1.); vec4 w = uInvVP*vec4(p,1.,1.); vDir = w.xyz/w.w - uCam; gl_Position = vec4(p,1.,1.); }
)GLSL";
const char* SKY_FS = R"GLSL(
in vec3 vDir; out vec4 oC; uniform vec3 uSunDir, uMoonDir, uZen, uHor, uGnd, uSunCol; uniform float uNight, uDay, uTime, uCover, uSunVis, uTw;
void main(){ vec3 d = normalize(vDir); float h = d.y;
  vec3 col = mix(uHor, uZen, pow(clamp(h,0.,1.), .42)); col = mix(col, uGnd, smoothstep(0., -.12, h));
  float sd = max(dot(d, uSunDir), 0.); col += uSunCol*(pow(sd,5.)*.10 + pow(sd,48.)*.45 + smoothstep(.99935,.99965,sd)*40.)*uSunVis;
  float md = max(dot(d, uMoonDir), 0.); col += vec3(.70,.80,1.0)*(smoothstep(.9991,.9995,md)*4. + pow(md,60.)*.08)*uNight;
  if (uNight > .01 && h > 0.) { vec3 sp = floor(d*260.); float s = step(.9968, hs3(sp)); col += vec3(s)*(.4+.6*hs3(sp+3.))*uNight*smoothstep(0.,.25,h)*1.8; }
  if (h > 0.) { vec2 uv = d.xz/(h+.14)*.62 + uTime*vec2(.006,.003); float c = fbm2(uv*1.8); c = smoothstep(.55 - uCover*.22, .80, c) * smoothstep(0.,.2,h);
    vec3 lit = mix(vec3(.10,.11,.16), vec3(1.02,.99,.95), uDay) * (.82 + .35*fbm2(uv*4.+3.)); lit += uSunCol*uTw*.28*uSunVis; lit = mix(lit, lit*vec3(.8,.82,.95), 1.-uDay); col = mix(col, lit*(.6+.5*uDay), c*.88); }
  oC = vec4(col, 1.); }
)GLSL";
const char* WATER_VS = R"GLSL(
uniform sampler2D uWater; uniform vec2 uCenter; uniform float uSp, uWorld; uniform int uN; uniform mat4 uVP; out vec3 vWp;
void main(){ int id = gl_VertexID; int i = id % (uN+1), j = id / (uN+1); vec2 g = vec2(float(i - uN/2), float(j - uN/2)); vec2 xz = uCenter + g*uSp; float y = textureLod(uWater, xz/uWorld+.5, 0.).r; vWp = vec3(xz.x, y, xz.y); gl_Position = uVP*vec4(vWp,1.); }
)GLSL";
const char* WATER_FS = R"GLSL(
in vec3 vWp; out vec4 oC; uniform sampler2D uHeight, uWater, uInfoA; uniform float uWorld, uTime, uAmb; uniform vec3 uSunDir, uSunCol, uZen, uHor; uniform float uSunVis; uniform vec4 uFalls[8];
vec3 skyR(vec3 r){ vec3 c = mix(uHor, uZen, pow(clamp(r.y,0.,1.), .45)); float sd = max(dot(r,uSunDir),0.); c += uSunCol*(pow(sd,6.)*.12)*uSunVis; return c; }
void main(){
  vec2 uv = vWp.xz/uWorld+.5; float th = texture(uHeight, uv).r; float depth = vWp.y - th; if (depth < 0.) discard;
  vec4 Wt = texture(uWater, uv); vec2 flow = Wt.gb; float spd = length(flow); vec2 fd = spd > .02 ? flow/spd : vec2(0.); float river = smoothstep(.02,.6,spd);
  float t = uTime; vec2 p = vWp.xz; float v1 = mix(.25, 2.2, river), v2 = mix(.4, 3.4, river);
  vec2 q1 = p*.42 - fd*t*v1 + vec2(t*.05, t*.03), q2 = p*1.15 - fd*t*v2 + vec2(-t*.06, t*.04), q3 = p*3.1 - fd*t*v2*1.4;
  float e = .08; float a1 = mix(.35, .6, river);
  vec2 g1 = vec2(vn2(q1+vec2(e,0.))-vn2(q1-vec2(e,0.)), vn2(q1+vec2(0.,e))-vn2(q1-vec2(0.,e)));
  vec2 g2 = vec2(vn2(q2+vec2(e,0.))-vn2(q2-vec2(e,0.)), vn2(q2+vec2(0.,e))-vn2(q2-vec2(e,0.)))*0.;
  g2 = vec2(vn2(q2+vec2(e,0.))-vn2(q2-vec2(e,0.)), vn2(q2+vec2(0.,e))-vn2(q2-vec2(0.,e)));
  vec2 g3 = vec2(vn2(q3+vec2(e,0.))-vn2(q3-vec2(e,0.)), vn2(q3+vec2(0.,e))-vn2(q3-vec2(0.,e)));
  vec3 N = normalize(vec3(-(g1*a1 + g2*.5*a1 + g3*.25*a1).x*1.6, 1., -(g1*a1 + g2*.5*a1 + g3*.25*a1).y*1.6));
  vec3 V = normalize(uCam - vWp); float fres = .02 + .98*pow(1. - clamp(dot(N,V),0.,1.), 5.); fres = clamp(fres, .03, .92);
  vec3 bed = texture(uInfoA, uv).rgb; bed *= bed;
  vec3 shallow = vec3(.10,.30,.27), deep = vec3(.015,.09,.15); vec3 body = mix(shallow, deep, 1. - exp(-depth*.55)); body = mix(bed*.55, body, 1. - exp(-depth*1.4)) * uAmb;
  vec3 R = reflect(-V, N); vec3 refl = skyR(R); vec3 col = mix(body, refl, fres);
  vec3 Hh = normalize(uSunDir + V); float spec = pow(max(dot(N,Hh),0.), 220.) * 2.4 + pow(max(dot(N,Hh),0.), 28.)*.06; col += uSunCol*spec*uSunVis;
  float n3 = vn2(p*2.6 + vec2(t*.2, -t*.1)), n4 = vn2(p*7.1 - fd*t*1.5);
  float foam = smoothstep(.5, .0, depth) * (.35 + .65*smoothstep(.35,.7,n3));
  foam += river*smoothstep(.5,.85,vn2(p*.9 - fd*t*2.4)*vn2(p*2.2 - fd*t*3.1)*2.2)*.35;
  for (int i = 0; i < 8; i++) { if (uFalls[i].w < .5) continue; float dd = length(p - uFalls[i].xy); foam += smoothstep(uFalls[i].z, 1., dd) * (smoothstep(uFalls[i].z, 1., dd)) * (.5 + .5*n4) * .55; }
  foam = clamp(foam, 0., 1.); col = mix(col, vec3(.9,.94,.96)*uAmb*1.05 + uSunCol*.04*uSunVis, foam*.8);
  float alpha = mix(.45, .97, 1. - exp(-depth*1.5)); alpha = mix(alpha, 1., max(foam*.9, fres*.5)); oC = vec4(col, alpha); }
)GLSL";
const char* FALL_VS = R"GLSL(
layout(location=0) in vec2 aP; uniform mat4 uVP; uniform vec4 uPos; uniform vec4 uDir; uniform vec3 uRight, uUp; uniform int uMode; out vec2 vUv; out float vY; out vec3 vWp;
void main(){
  if (uMode == 0) { float u = aP.x, v = aP.y; float fall = (1.-v); float out_ = .5 + fall*uDir.w*.25 + sin(fall*3.14159*.5)*.6; vec3 p = vec3(uPos.x, mix(uDir.z, uPos.y, v), uPos.z); vec3 hd = vec3(uDir.x, 0., uDir.y); vec3 rt = vec3(-uDir.y, 0., uDir.x);
    p += rt*u*uPos.w + hd*out_; vWp = p; vUv = vec2(u*uPos.w, fall*(uPos.y-uDir.z)); vY = v; gl_Position = uVP*vec4(p,1.); }
  else { int id = gl_InstanceID; float fi = float(id); float a = fi*2.399, r = 1.2 + 2.6*fract(fi*.618); vec3 hd = vec3(uDir.x, 0., uDir.y); vec3 rt = vec3(-uDir.y, 0., uDir.x);
    vec3 base = vec3(uPos.x, uDir.z + 1.2, uPos.z) + hd*(1.8 + r*.7) + rt*cos(a)*r*.9; float rise = fract(fi*.37 + uPos.y*0.0 + float(uMode)*0.); base.y += 0.5 + 3.2*fract(fi*.211) ; float sz = (3.2 + 3.4*fract(fi*.77))*(uPos.w*.18+.55);
    vec3 p = base + (uRight*aP.x + uUp*aP.y)*sz; vWp = p; vUv = aP; vY = fi; gl_Position = uVP*vec4(p,1.); } }
)GLSL";
const char* FALL_FS = R"GLSL(
in vec2 vUv; in float vY; in vec3 vWp; out vec4 oC; uniform int uMode; uniform float uTime, uAmb; uniform vec3 uSunCol;
void main(){
  if (uMode == 0) { float t = uTime; float s1 = vn2(vec2(vUv.x*2.6, vUv.y*.55 - t*5.2)), s2 = vn2(vec2(vUv.x*6.3+3., vUv.y*.9 - t*7.3)), s3 = vn2(vec2(vUv.x*1.2, vUv.y*.25 - t*3.1));
    float streak = clamp(s1*.6 + s2*.5 + s3*.3, 0., 1.5); float edge = smoothstep(0., .12, 1. - abs(vUv.x)/max(abs(vUv.x), .001)*0.);
    vec3 col = mix(vec3(.45,.62,.72), vec3(.95,.98,1.), smoothstep(.45,1.0,streak)) * uAmb*1.05 + uSunCol*.03; float foam = smoothstep(.16, .0, vY); col = mix(col, vec3(1.)*uAmb*1.1, foam*.8);
    float a = clamp(.4 + streak*.5, .3, .92); oC = vec4(col, a); }
  else { float r = length(vUv); if (r > 1.) discard; float n = vn2(vUv*2.5 + vY); float a = pow(1.-r, 2.)*(.18 + .12*n); oC = vec4(vec3(.92,.96,1.)*uAmb*1.05, a); } }
)GLSL";
const char* PROP_VS = R"GLSL(
layout(location=0) in vec3 aPos; layout(location=1) in vec3 aNrm; layout(location=2) in vec4 aCol; layout(location=3) in vec2 aUv; layout(location=4) in vec4 aMat; layout(location=5) in vec4 aBn;
layout(location=6) in vec4 iM0; layout(location=7) in vec4 iM1; layout(location=8) in vec4 iM2; layout(location=9) in vec4 iM3; layout(location=10) in vec4 iT;
uniform mat4 uVP; uniform float uTime;
out vec3 vWp; out vec3 vN; out vec3 vCol; out vec3 vRest; out vec4 vMat; out vec2 vUvS; out float vAO; out float vCav;
void main(){ mat4 M = mat4(iM0,iM1,iM2,iM3); vec4 wp = M*vec4(aPos,1.);
  if (iT.a > .5) { float k = smoothstep(1.5, 9., aPos.y)*.07; wp.x += sin(uTime*1.25 + wp.x*.17 + wp.z*.11)*k*iM1.y*1.0; wp.z += cos(uTime*1.05 + wp.z*.15 + wp.x*.09)*k*iM1.y; }
  vWp = wp.xyz; vN = normalize(mat3(M)*aNrm); vCol = aCol.rgb*aCol.rgb*iT.rgb; vAO = aCol.a; vCav = aBn.w/255.*2.-1.; vMat = vec4(aMat.xy, aMat.z*255., aMat.w*2.);
  if (iT.a < -.5) { vCol = (aCol.rgb*aCol.rgb*.65 + vec3(.14)) * iT.rgb * (1.0 + .16*sin(uTime*6.)); vAO = mix(aCol.a, 1., .30); vMat = vec4(.45, .08, 8., 0.); }
  vUvS = aUv; vRest = aPos; gl_Position = uVP*wp; }
)GLSL";
const char* PROP_SHADOW_VS = R"GLSL(
layout(location=0) in vec3 aPos; layout(location=6) in vec4 iM0; layout(location=7) in vec4 iM1; layout(location=8) in vec4 iM2; layout(location=9) in vec4 iM3; uniform mat4 uVP;
void main(){ gl_Position = uVP*(mat4(iM0,iM1,iM2,iM3)*vec4(aPos,1.)); }
)GLSL";
const char* GRASS_VS = R"GLSL(
layout(location=0) in vec4 aP; uniform mat4 uVP; uniform vec2 uCam2; uniform float uCell, uTime, uMaxD, uWorld; uniform int uG; uniform int uMode; uniform sampler2D uHeight, uInfoA, uInfoB;
out vec3 vWp; out vec3 vCol; out vec3 vN; out float vFade;
float h21(vec2 p){ vec3 q = fract(vec3(p.xyx)*.1031); q += dot(q, q.yzx+33.33); return fract((q.x+q.y)*q.z); }
vec3 pal(float k){ k = fract(k)*7.; if (k < 1.) return vec3(.95,.12,.12); if (k < 2.) return vec3(1.,.82,.1); if (k < 3.) return vec3(.97,.97,.95); if (k < 4.) return vec3(.62,.25,.85); if (k < 5.) return vec3(.25,.45,.95); if (k < 6.) return vec3(1.,.45,.7); return vec3(1.,.55,.12); }
void main(){
  int id = gl_InstanceID; int gi = id % uG, gj = id / uG; vec2 cell = floor(uCam2/uCell) + vec2(float(gi - uG/2), float(gj - uG/2)); vec2 jit = vec2(h21(cell), h21(cell+17.3)); vec2 xz = (cell + jit)*uCell;
  vec2 uv = xz/uWorld + .5; float dist = length(xz - uCam2); vFade = 1. - smoothstep(uMaxD*.7, uMaxD, dist);
  vec4 A = textureLod(uInfoA, uv, 0.), B = textureLod(uInfoB, uv, 0.); float keep;
  if (uMode == 0) keep = step(h21(cell+3.1), A.a*.92); else keep = step(h21(cell+9.7), B.r*.55) * step(.05, A.a);
  if (keep < .5 || vFade <= 0.01 || uv.x < 0. || uv.y < 0. || uv.x > 1. || uv.y > 1.) { gl_Position = vec4(2.,2.,2.,1.); vWp = vec3(0.); vCol = vec3(0.); vN = vec3(0.,1.,0.); return; }
  float h0 = textureLod(uHeight, uv, 0.).r; float hr = h21(cell+5.5); float yaw = hr*6.2831; vec3 base = A.rgb*A.rgb; float wind = sin(uTime*1.6 + xz.x*.35 + xz.y*.22)*.5 + sin(uTime*2.7 + xz.y*.6)*.25;
  vec3 pos; vec3 col; vec3 nrm = vec3(0.,1.,0.);
  if (uMode == 0) {
    float bid = aP.w; float ang = h21(cell*1.7 + bid*3.3)*6.2831; vec2 off = (vec2(h21(cell+bid*1.3), h21(cell-bid*2.1))-.5)*.5; float Hh = (.16 + .30*h21(cell+bid*7.7)) * (1.-.4*B.g) * vFade * (1. + .5*hr); float w = .032*(1.+h21(cell+bid)*.6);
    float y = aP.y; float side = aP.x*w*(1.-y*.92); vec2 dir = vec2(cos(ang), sin(ang)); float lean = (.25 + .6*h21(cell+bid*4.4)) * y*y; vec2 bend = dir*lean*Hh*.8 + vec2(wind*.18, wind*.1)*y*y*Hh;
    pos = vec3(xz.x + off.x + side*dir.y + bend.x, h0 + y*Hh, xz.y + off.y - side*dir.x + bend.y);
    col = base*mix(.5, 1.05, y)*(.8 + .35*h21(cell+bid)); col = mix(col, col*vec3(1.25,1.15,.55), y*y*.55*h21(cell+1.9)); nrm = normalize(vec3(dir.x*.35, 1., dir.y*.35)); }
  else {
    vec3 lp = aP.xyz; float kind = aP.w; float sz = (.75 + .6*hr) * vFade; float c = cos(yaw), s = sin(yaw); vec3 q = vec3(lp.x*c - lp.z*s, lp.y, lp.x*s + lp.z*c)*sz; float sway = q.y*q.y*(wind*.35);
    pos = vec3(xz.x + q.x + sway, h0 + q.y, xz.y + q.z + sway*.5);
    float cm = h21(floor(xz*.09)); float ck = h21(cell+13.1); vec3 pc = pal(mix(cm, ck, .22)); col = kind < .5 ? vec3(.18,.36,.1)*mix(.7,1.2,lp.y*2.) : (kind < 1.5 ? pc : vec3(1.,.78,.15)*.8);
    nrm = kind > .5 ? normalize(vec3(0., 1., 0.)) : vec3(0.,1.,0.); }
  vWp = pos; vCol = col; vN = nrm; gl_Position = uVP*vec4(pos, 1.); }
)GLSL";
const char* GRASS_FS = R"GLSL(
in vec3 vWp; in vec3 vCol; in vec3 vN; in float vFade; out vec4 oC;
void main(){ vec3 N = normalize(vN); float sh = shadowAt(vWp, N); vec3 c = shade(vCol, .85, 0., N, vWp, sh); c += vCol*.12*max(dot(-normalize(uCam - vWp), uMoonDir), 0.)*dot(uMoonCol, vec3(.3)); oC = vec4(c, 1.); }
)GLSL";
const char* MAP_VS = R"GLSL(
uniform vec4 uRect; uniform vec2 uRes; out vec2 vL;
void main(){ vec2 c = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1)); vL = c*2.-1.; vec2 px = uRect.xy + c*uRect.z; gl_Position = vec4(px.x/uRes.x*2.-1., 1. - px.y/uRes.y*2., 0., 1.); }
)GLSL";
const char* MAP_FS = R"GLSL(
in vec2 vL; out vec4 oC; uniform sampler2D uMap; uniform vec2 uCenter; uniform float uHalf, uRot, uYaw, uTime; uniform int uMode; uniform vec4 uPoi[48]; uniform vec4 uNat[3]; uniform vec3 uNatC[3];
void main(){ vec2 l = vL; float rr = length(l); if (uMode == 0 && rr > 1.) discard;
  float cs = cos(uRot), sn = sin(uRot); vec2 w = vec2(l.x*cs - l.y*sn, l.x*sn + l.y*cs)*uHalf + uCenter; vec2 uv = w/2048. + .5; vec3 col = texture(uMap, uv).rgb; if (uv.x < 0. || uv.y < 0. || uv.x > 1. || uv.y > 1.) col = vec3(.05,.12,.22);
  float bd = 1e9; int bi = 0; for (int i = 0; i < 3; i++) { float d = length(w - uNat[i].xy); if (d < bd) { bd = d; bi = i; } } col = mix(col, uNatC[bi], .10);
  float px = uHalf/ (uMode==0 ? 62. : 300.);
  for (int i = 0; i < 48; i++) { vec4 P = uPoi[i]; if (P.w < -.5) continue; float d = length(w - P.xy); float r = (P.z < .5 ? 6. : P.z < 1.5 ? 4.2 : 3.2)*px; if (d < r + px*.9) { vec3 c = P.z < .5 ? vec3(1.,.82,.3) : P.z < 1.5 ? vec3(.95,.62,.25) : P.z < 3.5 ? vec3(.4,.8,1.) : vec3(.9,.9,.9); if (P.w < .5) c = mix(c, vec3(.3), .6); col = mix(col, vec3(.04), step(d, r+px*.9)); col = mix(col, c, step(d, r)); } }
  vec2 pl = w - uCenter; if (uMode == 1) { float pd = length(pl); float r = 5.*px; vec2 fwd = vec2(sin(uYaw), cos(uYaw)); if (pd < r) col = vec3(1.,.3,.25); }
  else { vec2 q = l; float a = uYaw - uRot*0.; /* arrow in screen space */ vec2 f = vec2(sin(uYaw + uRot), -cos(uYaw + uRot)); vec2 pr = vec2(q.x, q.y); float along = dot(pr, f), side = abs(dot(pr, vec2(-f.y, f.x))); float tri = step(-.06, along) * step(side, .11 - (along+.06)*.6) * step(along, .12); col = mix(col, vec3(1.,.25,.2), tri); }
  float ring = smoothstep(.93, .96, rr) * (uMode==0 ? 1. : 0.); col = mix(col, vec3(.35,.28,.14), ring);
  float a = uMode == 0 ? (1. - smoothstep(.985, 1., rr)) : 1.; oC = vec4(col, a*.96); }
)GLSL";

// ------------------------------------------------------------------ resources
void buildMeshSlot(int slot, const std::vector<LV>& v, const std::vector<uint16_t>& ix) {
  PMesh& m = pm[slot]; glGenVertexArrays(1, &m.vao); glBindVertexArray(m.vao); glGenBuffers(1, &m.vbo); glBindBuffer(GL_ARRAY_BUFFER, m.vbo); glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)v.size() * 36, v.data(), GL_STATIC_DRAW); glGenBuffers(1, &m.ibo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo); glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)ix.size() * 2, ix.data(), GL_STATIC_DRAW); m.ni = (int)ix.size();
}
void setupInstVao(PMesh& m, int cap) {   // attribute layout shared by .lfm vertices; instance data (mat4 + tint) in attributes 6..10
  glBindVertexArray(m.vao); glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
  glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 36, (void*)0); glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_BYTE, GL_TRUE, 36, (void*)12); glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, 36, (void*)16);
  glEnableVertexAttribArray(3); glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 36, (void*)20); glEnableVertexAttribArray(4); glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_TRUE, 36, (void*)28); glEnableVertexAttribArray(5); glVertexAttribPointer(5, 4, GL_UNSIGNED_BYTE, GL_FALSE, 36, (void*)32);
  glGenBuffers(1, &m.inst); glBindBuffer(GL_ARRAY_BUFFER, m.inst); glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cap * sizeof(IData), nullptr, GL_STREAM_DRAW); m.cap = cap;
  for (int a = 0; a < 5; a++) { glEnableVertexAttribArray(6 + a); glVertexAttribPointer(6 + a, 4, GL_FLOAT, GL_FALSE, sizeof(IData), (void*)(intptr_t)(a * 16)); glVertexAttribDivisor(6 + a, 1); } glBindVertexArray(0);
}
void tex2D(GLuint& t, int w, int h, GLenum ifmt, GLenum fmt, GLenum type, const void* d, GLint filter, GLint wrap, bool mips) {
  glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t); glPixelStorei(GL_UNPACK_ALIGNMENT, 1); glTexImage2D(GL_TEXTURE_2D, 0, ifmt, w, h, 0, fmt, type, d); if (mips) glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mips ? GL_LINEAR_MIPMAP_LINEAR : filter); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
}
void texAlloc(GLuint& t, int w, int h, GLenum ifmt, GLenum fmt, GLenum type, GLint filter) { if (!t) glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t); glTexImage2D(GL_TEXTURE_2D, 0, ifmt, w, h, 0, fmt, type, nullptr); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); }
void fboAttach(GLuint& f, GLenum att, GLenum target, GLuint tex) { if (!f) glGenFramebuffers(1, &f); glBindFramebuffer(GL_FRAMEBUFFER, f); glFramebufferTexture2D(GL_FRAMEBUFFER, att, target, tex, 0); }
void allocTargets() {
  const Q& q = QS[gfx::quality]; float s = std::min(1.f, std::sqrt(q.cap / std::max(1.f, (float)winW * winH))); W = std::max(2, (int)std::round(winW * s)); H = std::max(2, (int)std::round(winH * s));
  texAlloc(texScene, W, H, GL_RGBA16F, GL_RGBA, GL_FLOAT, GL_LINEAR); texAlloc(texDepth, W, H, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, GL_NEAREST); fboAttach(fboScene, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texScene); glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texDepth, 0);
  texAlloc(texA, W, H, GL_RGBA16F, GL_RGBA, GL_FLOAT, GL_LINEAR); fboAttach(fboA, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texA);
  int w = W, h = H; for (int i = 0; i < 4; i++) { w = std::max(2, w >> 1); h = std::max(2, h >> 1); bw[i] = w; bh[i] = h; texAlloc(texB[i], w, h, GL_RGBA16F, GL_RGBA, GL_FLOAT, GL_LINEAR); fboAttach(fboB[i], GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texB[i]); }
  curMsaa = q.msaa;
  if (q.msaa > 0) { if (!texMSc) glGenTextures(1, &texMSc); if (!texMSd) glGenTextures(1, &texMSd); glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, texMSc); glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, q.msaa, GL_RGBA16F, W, H, GL_TRUE); glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, texMSd); glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, q.msaa, GL_DEPTH_COMPONENT32F, W, H, GL_TRUE);
    fboAttach(fboMS, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, texMSc); glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, texMSd, 0); if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { fprintf(stderr, "MSAA FBO incomplete\n"); curMsaa = 0; } }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
}  // namespace

// ------------------------------------------------------------------ environment
Env env(float hour) {
  Env e; e.hour = hour; float a = (hour - 6.f) / 12.f * PI; V3 sun = norm({std::cos(a) * 0.85f, std::sin(a), -0.38f}); float sy = sun.y; e.sunDir = sun;
  e.day = smooth(-0.08f, 0.22f, sy); e.tw = std::exp(-(sy * sy) / (0.2f * 0.2f)); e.night = 1.f - smooth(-0.30f, 0.0f, sy); e.sunVis = smooth(-0.06f, 0.04f, sy);
  V3 zenD{0.07f, 0.20f, 0.55f}, horD{0.50f, 0.66f, 0.90f}, zenN{0.0025f, 0.005f, 0.018f}, horN{0.010f, 0.018f, 0.045f}, twH{1.0f, 0.42f, 0.18f};
  e.zen = mix3(zenN, zenD, e.day); e.hor = mix3(horN, horD, e.day); float tk = e.tw * smooth(-0.3f, 0.05f, sy); e.hor = mix3(e.hor, twH, tk * 0.75f); e.zen = mix3(e.zen, V3{0.10f, 0.14f, 0.34f}, tk * 0.4f * (1.f - e.night * 0.5f));
  e.gnd = e.hor * 0.55f; e.sunCol = mix3(V3{1.0f, 0.46f, 0.20f}, V3{1.0f, 0.93f, 0.82f}, smooth(0.0f, 0.45f, sy)); float sInt = smooth(-0.02f, 0.18f, sy) * 5.4f;
  V3 moonDir = norm({-sun.x * 0.7f, std::max(0.30f, -sy), 0.45f}); float wSun = smooth(-0.07f, 0.03f, sy); V3 moonCol{0.55f, 0.68f, 1.0f};
  e.keyDir = norm(sun * wSun + moonDir * (1.f - wSun)); e.keyCol = e.sunCol * (sInt * wSun) + moonCol * (0.55f * e.night * (1.f - wSun));
  e.hemiSky = mix3(V3{0.020f, 0.032f, 0.075f}, V3{0.50f, 0.66f, 0.95f}, e.day) * (1.f + tk * 0.2f); e.hemiGnd = mix3(V3{0.010f, 0.010f, 0.012f}, V3{0.20f, 0.17f, 0.12f}, e.day);
  e.fog = e.hor * (0.85f + 0.15f * e.day); e.dens = lerpf(0.0013f, 0.00072f, e.day); if (hour > 5.2f && hour < 8.8f) e.dens *= 1.f + 0.7f * smooth(8.8f, 7.4f, hour) * smooth(5.2f, 6.2f, hour);
  e.exposure = lerpf(1.75f, 0.74f, e.day); e.bloom = lerpf(0.5f, 0.28f, e.day); e.glow = lerpf(3.0f, 0.15f, e.day); e.cover = 0.5f + 0.2f * std::sin(hour * 0.7f); (void)moonCol; return e;
}

// ------------------------------------------------------------------ init
bool init() {
  if (inited) return true; inited = true; winW = gfx::winW; winH = gfx::winH; const Q& q = QS[gfx::quality]; (void)q;
  std::string XL = xlight();
  pTerr = gfx::program(V(TERR_VS).c_str(), V(XL.c_str(), NOISE, TERR_FS).c_str(), "x-terrain"); pWater = gfx::program(V(WATER_VS).c_str(), V("uniform vec3 uCam;", NOISE, WATER_FS).c_str(), "x-water"); pSky = gfx::program(V(SKY_VS).c_str(), V(NOISE, SKY_FS).c_str(), "x-sky");
  pProp = gfx::program(V(PROP_VS).c_str(), V(XL.c_str(), FIG_FS_BODY).c_str(), "x-prop"); pPropShadow = gfx::program(V(PROP_SHADOW_VS).c_str(), sh::SHADOW_FS, "x-propshadow"); pFigX = gfx::program(V(FIG_VS).c_str(), V(XL.c_str(), FIG_FS_BODY).c_str(), "x-figure");
  pGrass = gfx::program(V(GRASS_VS).c_str(), V(XL.c_str(), GRASS_FS).c_str(), "x-grass"); pFall = gfx::program(V(FALL_VS).c_str(), V(NOISE, FALL_FS).c_str(), "x-fall"); pMapP = gfx::program(V(MAP_VS).c_str(), V(MAP_FS).c_str(), "x-map");
  pFogP = gfx::program(sh::FS_VS, sh::FOG_FS, "x-fog"); pDownP = gfx::program(sh::FS_VS, sh::DOWN_FS, "x-down"); pFinalP = gfx::program(sh::FS_VS, sh::FINAL_FS, "x-final"); glGenVertexArrays(1, &vaoEmpty);
  // world textures
  tex2D(texH, GS, GS, GL_R32F, GL_RED, GL_FLOAT, Hh.data(), GL_LINEAR, GL_CLAMP_TO_EDGE, false); tex2D(texInfoA, GS, GS, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, infoA.data(), GL_LINEAR, GL_CLAMP_TO_EDGE, true); tex2D(texInfoB, GS, GS, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, infoB.data(), GL_LINEAR, GL_CLAMP_TO_EDGE, true);
  tex2D(texWater, GS, GS, GL_RGBA32F, GL_RGBA, GL_FLOAT, waterTex.data(), GL_LINEAR, GL_CLAMP_TO_EDGE, false); tex2D(texMap, MAPN, MAPN, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, mapRGBA.data(), GL_LINEAR, GL_CLAMP_TO_EDGE, true);
  // clipmap index buffers (full grid + ring with a hole)
  for (int r = 0; r < 2; r++) { std::vector<uint16_t> ix; for (int j = 0; j < TN; j++) for (int i = 0; i < TN; i++) { if (r == 1 && i >= TN / 4 + 2 && i < TN * 3 / 4 - 2 && j >= TN / 4 + 2 && j < TN * 3 / 4 - 2) continue; uint16_t a = (uint16_t)(j * (TN + 1) + i), b = a + 1, c = (uint16_t)(a + TN + 1), d = c + 1; uint16_t t[6] = {a, c, b, b, c, d}; ix.insert(ix.end(), t, t + 6); }
    glGenBuffers(1, &terrIbo[r]); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrIbo[r]); glBufferData(GL_ELEMENT_ARRAY_BUFFER, ix.size() * 2, ix.data(), GL_STATIC_DRAW); terrNI[r] = (int)ix.size(); }
  glGenVertexArrays(1, &terrVao);
  // props
  for (int k = 0; k < NK; k++) { std::vector<LV> v; std::vector<uint16_t> ix; if (KD[k].key) { FigAsset& a = propAsset[k]; if (!figLoad(a, KD[k].key)) { fprintf(stderr, "missing prop %s\n", KD[k].key); continue; } pm[k].vao = 0; pm[k].vbo = a.vbo; pm[k].ibo = a.ibo; pm[k].ni = a.ni; glGenVertexArrays(1, &pm[k].vao); glBindVertexArray(pm[k].vao); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, a.ibo); glBindVertexArray(0); pm[k].ok = true; }
    else if (xmBuild(k, v, ix)) { buildMeshSlot(k, v, ix); pm[k].ok = true; } }
  { std::vector<LV> v; std::vector<uint16_t> ix; if (xmBuild(XM_LOCONI, v, ix)) { buildMeshSlot(SLOT_CONI, v, ix); pm[SLOT_CONI].ok = true; } if (xmBuild(XM_LOBROAD, v, ix)) { buildMeshSlot(SLOT_BROAD, v, ix); pm[SLOT_BROAD].ok = true; } }
  // bucket instances per kind per 64 m cell
  std::vector<std::vector<int>> per(NK); for (size_t i = 0; i < objs.size(); i++) per[objs[i].kind].push_back((int)i);
  for (int k = 0; k < NK; k++) { cellStart[k].assign(NC * NC + 1, 0); std::vector<int>& L = per[k]; auto cellId = [&](const Obj& o) { int cx = std::min(NC - 1, std::max(0, (int)((o.x + HALF) / CELL))), cz = std::min(NC - 1, std::max(0, (int)((o.z + HALF) / CELL))); return cz * NC + cx; };
    for (int id : L) cellStart[k][cellId(objs[id]) + 1]++; for (int c = 0; c < NC * NC; c++) cellStart[k][c + 1] += cellStart[k][c]; data[k].resize(L.size()); std::vector<int> fill(cellStart[k].begin(), cellStart[k].end() - 1);
    for (int id : L) { const Obj& o = objs[id]; M4 m = mul(translate(o.x, o.y, o.z), mul(rotY(o.ry), scale4(o.sx, o.sy, o.sz))); IData& d = data[k][fill[cellId(o)]++]; memcpy(d.m, m.m, 64); d.tint[0] = o.r / 128.f; d.tint[1] = o.g / 128.f; d.tint[2] = o.b / 128.f; d.tint[3] = o.sway ? 1.f : 0.f; } }
  for (int s = 0; s < NSLOT; s++) { if (!pm[s].ok) continue; int cap = s < NK ? std::min<int>(7200, std::max<int>(s >= (int)K_FA_DOG ? 1000 : 160, (int)data[s].size() + 160)) : 14000; stage[s].resize(cap); setupInstVao(pm[s], cap); }
  // grass clump (5 blades x 7 verts) and flower meshes
  { std::vector<float> v; std::vector<uint16_t> ix; for (int b = 0; b < 5; b++) { int base = (int)v.size() / 4; for (int r = 0; r < 3; r++) { float y = r / 3.f; for (int sd = -1; sd <= 1; sd += 2) { v.push_back((float)sd); v.push_back(y); v.push_back(0); v.push_back((float)b); } } v.push_back(0); v.push_back(1); v.push_back(0); v.push_back((float)b);
      uint16_t t[15] = {0, 1, 2, 1, 3, 2, 2, 3, 4, 3, 5, 4, 4, 5, 6}; for (int k = 0; k < 15; k++) ix.push_back((uint16_t)(base + t[k])); }
    glGenVertexArrays(1, &grassVao); glBindVertexArray(grassVao); glGenBuffers(1, &grassVbo); glBindBuffer(GL_ARRAY_BUFFER, grassVbo); glBufferData(GL_ARRAY_BUFFER, v.size() * 4, v.data(), GL_STATIC_DRAW); glEnableVertexAttribArray(0); glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 16, nullptr);
    glGenBuffers(1, &grassIbo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, grassIbo); glBufferData(GL_ELEMENT_ARRAY_BUFFER, ix.size() * 2, ix.data(), GL_STATIC_DRAW); grassNI = (int)ix.size(); glBindVertexArray(0); }
  { std::vector<float> v; std::vector<uint16_t> ix; auto P = [&](float x, float y, float z, float k) { v.push_back(x); v.push_back(y); v.push_back(z); v.push_back(k); return (uint16_t)(v.size() / 4 - 1); };
    uint16_t a = P(-0.012f, 0, 0, 0), b = P(0.012f, 0, 0, 0), c = P(0.008f, 0.3f, 0, 0), d = P(-0.008f, 0.3f, 0, 0), e = P(0, 0.36f, 0, 0); ix.insert(ix.end(), {a, b, c, a, c, d, d, c, e});
    uint16_t ctr = P(0, 0.34f, 0, 2); for (int p = 0; p < 6; p++) { float an = p * TAU / 6, ca = std::cos(an), sa = std::sin(an), cb = std::cos(an + 0.9f), sb = std::sin(an + 0.9f); uint16_t t1 = P(ca * 0.07f, 0.355f, sa * 0.07f, 1), t2 = P(cb * 0.045f, 0.345f, sb * 0.045f, 1), t3 = P(std::cos(an - 0.9f) * 0.045f, 0.345f, std::sin(an - 0.9f) * 0.045f, 1); ix.insert(ix.end(), {ctr, t1, t2, ctr, t3, t1}); }
    glGenVertexArrays(1, &flowerVao); glBindVertexArray(flowerVao); glGenBuffers(1, &flowerVbo); glBindBuffer(GL_ARRAY_BUFFER, flowerVbo); glBufferData(GL_ARRAY_BUFFER, v.size() * 4, v.data(), GL_STATIC_DRAW); glEnableVertexAttribArray(0); glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 16, nullptr);
    glGenBuffers(1, &flowerIbo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, flowerIbo); glBufferData(GL_ELEMENT_ARRAY_BUFFER, ix.size() * 2, ix.data(), GL_STATIC_DRAW); flowerNI = (int)ix.size(); glBindVertexArray(0); }
  // waterfall sheet (grid) + mist quad
  { std::vector<float> v; std::vector<uint16_t> ix; const int cx = 8, cy = 14; for (int j = 0; j <= cy; j++) for (int i = 0; i <= cx; i++) { v.push_back((float)i / cx - 0.5f); v.push_back((float)j / cy); } for (int j = 0; j < cy; j++) for (int i = 0; i < cx; i++) { uint16_t a = (uint16_t)(j * (cx + 1) + i), b = a + 1, c = (uint16_t)(a + cx + 1), d = c + 1; uint16_t t[6] = {a, c, b, b, c, d}; ix.insert(ix.end(), t, t + 6); }
    glGenVertexArrays(1, &fallVao); glBindVertexArray(fallVao); glGenBuffers(1, &fallVbo); glBindBuffer(GL_ARRAY_BUFFER, fallVbo); glBufferData(GL_ARRAY_BUFFER, v.size() * 4, v.data(), GL_STATIC_DRAW); glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, nullptr);
    GLuint ib; glGenBuffers(1, &ib); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib); glBufferData(GL_ELEMENT_ARRAY_BUFFER, ix.size() * 2, ix.data(), GL_STATIC_DRAW); fallNI = (int)ix.size(); glBindVertexArray(0);
    const float qv[8] = {-1, -1, 1, -1, -1, 1, 1, 1}; glGenVertexArrays(1, &mistVao); glBindVertexArray(mistVao); glGenBuffers(1, &mistVbo); glBindBuffer(GL_ARRAY_BUFFER, mistVbo); glBufferData(GL_ARRAY_BUFFER, sizeof qv, qv, GL_STATIC_DRAW); glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, nullptr); glBindVertexArray(0); }
  // shadow map
  glGenTextures(1, &texSh); glBindTexture(GL_TEXTURE_2D, texSh); glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHS, SHS, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  { float bc[4] = {1, 1, 1, 1}; glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, bc); } glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL); fboAttach(fboSh, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texSh); glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE); glBindFramebuffer(GL_FRAMEBUFFER, 0);
  allocTargets(); return true;
}
void resize(int w, int h) { winW = std::max(2, w); winH = std::max(2, h); if (inited) allocTargets(); }

// ------------------------------------------------------------------ frame
namespace {
float planes[6][4];
void extractPlanes(const M4& m) {
  auto row = [&](int r, float* o) { for (int c = 0; c < 4; c++) o[c] = m.m[c * 4 + r]; }; float r0[4], r1[4], r2[4], r3[4]; row(0, r0); row(1, r1); row(2, r2); row(3, r3);
  for (int i = 0; i < 6; i++) { int ax = i / 2; const float* rr = ax == 0 ? r0 : ax == 1 ? r1 : r2; float sg = (i & 1) ? -1.f : 1.f; for (int c = 0; c < 4; c++) planes[i][c] = r3[c] + sg * rr[c]; float l = std::sqrt(planes[i][0] * planes[i][0] + planes[i][1] * planes[i][1] + planes[i][2] * planes[i][2]); for (int c = 0; c < 4; c++) planes[i][c] /= l; }
}
bool sphereVis(float x, float y, float z, float r) { for (int i = 0; i < 6; i++) if (planes[i][0] * x + planes[i][1] * y + planes[i][2] * z + planes[i][3] < -r) return false; return true; }
void setLitX(GLuint p, const float* pl, const float* plc) {
  glUseProgram(p); s3(p, "uCam", camPos); s3(p, "uMoonDir", E.keyDir); s3(p, "uMoonCol", E.keyCol); s3(p, "uHemiSky", E.hemiSky); s3(p, "uHemiGnd", E.hemiGnd); s3(p, "uFillDir", norm({-E.keyDir.x, 0.6f, -E.keyDir.z})); s3(p, "uFillCol", E.hemiSky * 0.18f);
  glUniform4fv(U(p, "uPL"), 4, pl); glUniform4fv(U(p, "uPLC"), 4, plc); sm(p, "uShadowVP", shadowVP); si(p, "uShadowTex", 5); s1(p, "uShadowOn", gfx::quality > 0 ? 1.f : 0.f); sm(p, "uVP", vpM);
  s3(p, "uSkyZen", E.zen); s3(p, "uSkyHor", E.hor); s3(p, "uSkyGnd", E.gnd); s3(p, "uSunDirE", E.sunDir); s3(p, "uSunColE", E.sunCol * E.sunVis);
}
void gather(const Frame& f, bool shadowOnly) {
  for (int s = 0; s < NSLOT; s++) stageN[s] = 0; float cx = f.eye.x, cz = f.eye.z;
  for (int k = 0; k < NK; k++) { if (!pm[k].ok || data[k].empty()) continue; const KindDef& kd = KD[k]; if (shadowOnly && !kd.shadow) continue; float rMax = (kd.lo >= 0 ? kd.rLo : kd.rHi); if (shadowOnly) rMax = 90.f; int c0x = std::max(0, (int)((cx - rMax - CELL + HALF) / CELL)), c1x = std::min(NC - 1, (int)((cx + rMax + CELL + HALF) / CELL)), c0z = std::max(0, (int)((cz - rMax - CELL + HALF) / CELL)), c1z = std::min(NC - 1, (int)((cz + rMax + CELL + HALF) / CELL));
    float hiR2 = (shadowOnly ? std::min(kd.rHi, 90.f) : kd.rHi) * (shadowOnly ? std::min(kd.rHi, 90.f) : kd.rHi), loR2 = kd.rLo * kd.rLo; bool hasLo = kd.lo >= 0 && !shadowOnly; int cap = (int)stage[k].size(), capLo = kd.lo >= 0 ? (int)stage[kd.lo].size() : 0;
    for (int cz2 = c0z; cz2 <= c1z; cz2++) for (int cx2 = c0x; cx2 <= c1x; cx2++) { int c = cz2 * NC + cx2; int a = cellStart[k][c], b = cellStart[k][c + 1]; if (a == b) continue; float mx = -HALF + (cx2 + 0.5f) * CELL, mz = -HALF + (cz2 + 0.5f) * CELL; float dc = std::sqrt((mx - cx) * (mx - cx) + (mz - cz) * (mz - cz)); if (dc - CELL * 0.75f > rMax) continue; if (!shadowOnly && !sphereVis(mx, f.eye.y > 400.f ? f.eye.y : 40.f, mz, CELL * 0.8f + 60.f)) continue;
      for (int i = a; i < b; i++) { const IData& d = data[k][i]; float dx = d.m[12] - cx, dz = d.m[14] - cz, d2 = dx * dx + dz * dz; if (d2 < hiR2) { if (stageN[k] < cap) stage[k][stageN[k]++] = d; } else if (hasLo && d2 < loR2) { if (stageN[kd.lo] < capLo) stage[kd.lo][stageN[kd.lo]++] = d; } } } }
  if (f.dyn) for (int i = 0; i < f.ndyn; i++) {
    int k = f.dyn[i].kind < 0 ? (int)K_ARROW : f.dyn[i].kind; if (k < 0 || k >= NK || !pm[k].ok) continue;
    if (shadowOnly) { if (!KD[k].shadow || f.dyn[i].mode < -0.5f) continue; float dx = f.dyn[i].m.m[12] - cx, dz = f.dyn[i].m.m[14] - cz; if (dx * dx + dz * dz > 95.f * 95.f) continue; }
    if (stageN[k] < (int)stage[k].size()) { IData& d = stage[k][stageN[k]++]; memcpy(d.m, f.dyn[i].m.m, 64); d.tint[0] = f.dyn[i].tint[0]; d.tint[1] = f.dyn[i].tint[1]; d.tint[2] = f.dyn[i].tint[2]; d.tint[3] = f.dyn[i].mode; }
  }
}
void drawProps(GLuint prog, bool shadow) {
  for (int s = 0; s < NSLOT; s++) { if (!pm[s].ok || !stageN[s]) continue; glBindVertexArray(pm[s].vao); glBindBuffer(GL_ARRAY_BUFFER, pm[s].inst); glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)stageN[s] * sizeof(IData), stage[s].data()); glDrawElementsInstanced(GL_TRIANGLES, pm[s].ni, GL_UNSIGNED_SHORT, nullptr, stageN[s]); drawCalls++; } (void)prog; (void)shadow;
}
}  // namespace
bool project(float x, float y, float z, float& sx, float& sy) { const float* m = vpM.m; float cx = m[0] * x + m[4] * y + m[8] * z + m[12], cy = m[1] * x + m[5] * y + m[9] * z + m[13], cw = m[3] * x + m[7] * y + m[11] * z + m[15]; if (cw <= 0.01f) return false; sx = (cx / cw * 0.5f + 0.5f) * winW; sy = (1 - (cy / cw * 0.5f + 0.5f)) * winH; return true; }

void render(const Frame& f, const FigDraw* figs, int nfig) {
  drawCalls = 0; const Q& q = QS[gfx::quality]; E = env(f.hour); if (indoor > 0.01f) { float k = indoor; E.keyCol = E.keyCol * (1 - 0.9f * k); E.sunCol = E.sunCol * (1 - 0.9f * k); E.hemiSky = mix3(E.hemiSky, V3{0.16f, 0.12f, 0.085f}, k * 0.85f); E.hemiGnd = mix3(E.hemiGnd, V3{0.10f, 0.07f, 0.045f}, k * 0.85f); E.night = std::max(E.night, 0.5f * k); E.dens = lerpf(E.dens, 0.0001f, k); } camPos = f.eye; float aspect = (float)W / H;
  camW = camWorld(f.eye, f.tgt); viewM = rigidInverse(camW); projM = perspective(f.fov, aspect, 0.25f, 9000.f); vpM = mul(projM, viewM); invVP = inverse(vpM); invProj = inverse(projM); extractPlanes(vpM);
  // lights: nearest four lamps / fires / braziers
  float pl[16] = {0}, plc[16] = {0}; { int bi[4] = {-1, -1, -1, -1}; float bd[4] = {1e30f, 1e30f, 1e30f, 1e30f}; int nStat = (int)lights.size(), nTot = nStat + (f.dlights ? f.ndlights : 0);
    for (int i = 0; i < nTot; i++) { float lx = i < nStat ? lights[i].x : f.dlights[i - nStat].x, ly = i < nStat ? lights[i].y : f.dlights[i - nStat].y, lz = i < nStat ? lights[i].z : f.dlights[i - nStat].z; float d = (lx - f.eye.x) * (lx - f.eye.x) + (ly - f.eye.y) * (ly - f.eye.y) + (lz - f.eye.z) * (lz - f.eye.z); for (int k = 0; k < 4; k++) if (d < bd[k]) { for (int m = 3; m > k; m--) { bd[m] = bd[m - 1]; bi[m] = bi[m - 1]; } bd[k] = d; bi[k] = i; break; } }
    float nk = E.night * 0.9f + 0.06f; for (int k = 0; k < 4; k++) if (bi[k] >= 0) { bool dynL = bi[k] >= nStat; Light L = dynL ? Light{f.dlights[bi[k] - nStat].x, f.dlights[bi[k] - nStat].y, f.dlights[bi[k] - nStat].z, f.dlights[bi[k] - nStat].r, f.dlights[bi[k] - nStat].g, f.dlights[bi[k] - nStat].b, f.dlights[bi[k] - nStat].range} : lights[bi[k]];
      float fall = 1.f - smooth(L.range * 0.9f, L.range * 2.4f, std::sqrt(bd[k])); pl[k * 4] = L.x; pl[k * 4 + 1] = L.y; pl[k * 4 + 2] = L.z; pl[k * 4 + 3] = L.range * 2.2f; float in = 60.f * (dynL ? std::max(nk, 0.52f) : nk) * fall * (0.9f + 0.1f * std::sin(f.time * 9.f + L.x)); plc[k * 4] = L.r * in; plc[k * 4 + 1] = L.g * in; plc[k * 4 + 2] = L.b * in; plc[k * 4 + 3] = 1.25f; } }
  // shadow map: key light, ortho box around the focus
  { float R = 54.f; V3 tg = {std::round(f.tgt.x / 0.5f) * 0.5f, std::round(f.tgt.y / 0.5f) * 0.5f, std::round(f.tgt.z / 0.5f) * 0.5f}; V3 lp = tg + E.keyDir * 150.f; shadowVP = mul(ortho(-R, R, -R, R, 1, 330), rigidInverse(camWorld(lp, tg)));
    if (q.shadow) { gather(f, true); glBindFramebuffer(GL_FRAMEBUFFER, fboSh); glViewport(0, 0, SHS, SHS); glClear(GL_DEPTH_BUFFER_BIT); glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glDisable(GL_CULL_FACE); glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(2.f, 4.f);
      glUseProgram(pPropShadow); sm(pPropShadow, "uVP", shadowVP); drawProps(pPropShadow, true); glUseProgram(pFigShadow); sm(pFigShadow, "uVP", shadowVP); { std::vector<FigDraw>* none = nullptr; (void)none; } figDraw(pFigShadow, figs, nfig, true); glDisable(GL_POLYGON_OFFSET_FILL); } }
  gather(f, false);
  GLuint target = curMsaa > 0 ? fboMS : fboScene; glBindFramebuffer(GL_FRAMEBUFFER, target); glViewport(0, 0, W, H); glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); glActiveTexture(GL_TEXTURE0 + 5); glBindTexture(GL_TEXTURE_2D, texSh); glActiveTexture(GL_TEXTURE0);
  glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_TRUE); glDisable(GL_CULL_FACE);
  // sky
  glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glUseProgram(pSky); sm(pSky, "uInvVP", invVP); s3(pSky, "uCam", f.eye); s3(pSky, "uSunDir", E.sunDir); s3(pSky, "uMoonDir", norm({-E.sunDir.x * 0.7f, std::max(0.3f, -E.sunDir.y), 0.45f})); s3(pSky, "uZen", E.zen); s3(pSky, "uHor", E.hor); s3(pSky, "uGnd", E.gnd); s3(pSky, "uSunCol", E.sunCol);
  s1(pSky, "uNight", E.night); s1(pSky, "uDay", E.day); s1(pSky, "uTime", f.time); s1(pSky, "uCover", E.cover); s1(pSky, "uSunVis", E.sunVis); s1(pSky, "uTw", E.tw); glBindVertexArray(vaoEmpty); glDrawArrays(GL_TRIANGLES, 0, 3); glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE);
  // textures: 0 height, 1 infoA, 2 infoB, 3 water
  glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texH); glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, texInfoA); glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, texInfoB); glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, texWater); glActiveTexture(GL_TEXTURE0);
  // terrain (fine levels first for early-z)
  setLitX(pTerr, pl, plc); si(pTerr, "uHeight", 0); si(pTerr, "uInfoA", 1); si(pTerr, "uInfoB", 2); s1(pTerr, "uWorld", WORLD); s1(pTerr, "uTime", f.time); si(pTerr, "uN", TN); glBindVertexArray(terrVao); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrIbo[0]);
  for (int L = 0; L < NL; L++) { float sp = CS * (float)(1 << L), sn = sp * 2.f; float cx = CS * 0.5f + std::floor((f.eye.x - CS * 0.5f) / sn + 0.5f) * sn, cz = CS * 0.5f + std::floor((f.eye.z - CS * 0.5f) / sn + 0.5f) * sn; s2(pTerr, "uCenter", cx, cz); s1(pTerr, "uSp", sp); s1(pTerr, "uBias", L == 0 ? 0.f : 0.03f * sp); int r = L == 0 ? 0 : 1; glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrIbo[r]); glDrawElements(GL_TRIANGLES, terrNI[r], GL_UNSIGNED_SHORT, nullptr); drawCalls++; }
  // props + figures
  setLitX(pProp, pl, plc); s1(pProp, "uTime", f.time); s1(pProp, "uDissolve", 0.f); s1(pProp, "uDisH", 1.f); glUniform3f(U(pProp, "uDisCol"), 0, 0, 0); glUniform3f(U(pProp, "uFlash"), 0, 0, 0); s1(pProp, "uDim", 1.f); s1(pProp, "uGlow", E.glow); drawProps(pProp, false);
  if (nfig > 0) { setLitX(pFigX, pl, plc); glDisable(GL_CULL_FACE); figDraw(pFigX, figs, nfig, false); drawCalls += nfig; }
  // grass + flowers
  { setLitX(pGrass, pl, plc); si(pGrass, "uHeight", 0); si(pGrass, "uInfoA", 1); si(pGrass, "uInfoB", 2); s1(pGrass, "uWorld", WORLD); s1(pGrass, "uTime", f.time); s2(pGrass, "uCam2", f.eye.x, f.eye.z);
    int G1 = (int)(128 * std::sqrt(q.grass)); G1 &= ~1; s1(pGrass, "uCell", 0.7f); s1(pGrass, "uMaxD", 0.7f * G1 * 0.5f); si(pGrass, "uG", G1); si(pGrass, "uMode", 0); glBindVertexArray(grassVao); glDrawElementsInstanced(GL_TRIANGLES, grassNI, GL_UNSIGNED_SHORT, nullptr, G1 * G1); drawCalls++;
    int G2 = (int)(96 * std::sqrt(q.grass)); G2 &= ~1; s1(pGrass, "uCell", 0.75f); s1(pGrass, "uMaxD", 0.75f * G2 * 0.5f); si(pGrass, "uG", G2); si(pGrass, "uMode", 1); glBindVertexArray(flowerVao); glDrawElementsInstanced(GL_TRIANGLES, flowerNI, GL_UNSIGNED_SHORT, nullptr, G2 * G2); drawCalls++; }
  // water
  glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glUseProgram(pWater); sm(pWater, "uVP", vpM); si(pWater, "uHeight", 0); si(pWater, "uInfoA", 1); si(pWater, "uWater", 3); s1(pWater, "uWorld", WORLD); s1(pWater, "uTime", f.time); s1(pWater, "uAmb", 0.22f + 0.7f * E.day + 0.1f * E.tw); s3(pWater, "uCam", f.eye);
  s3(pWater, "uSunDir", E.keyDir); s3(pWater, "uSunCol", E.keyCol * 0.5f); s3(pWater, "uZen", E.zen); s3(pWater, "uHor", E.hor); s1(pWater, "uSunVis", std::max(E.sunVis, E.night * 0.35f)); si(pWater, "uN", TN);
  { float fl[32] = {0}; for (size_t i = 0; i < falls.size() && i < 8; i++) { fl[i * 4] = falls[i].x + falls[i].dx * 3.5f; fl[i * 4 + 1] = falls[i].z + falls[i].dz * 3.5f; fl[i * 4 + 2] = 7.f; fl[i * 4 + 3] = 1.f; } glUniform4fv(U(pWater, "uFalls"), 8, fl); }
  glBindVertexArray(terrVao); for (int L = 0; L < NL; L++) { float sp = CS * (float)(1 << L), sn = sp * 2.f; float cx = CS * 0.5f + std::floor((f.eye.x - CS * 0.5f) / sn + 0.5f) * sn, cz = CS * 0.5f + std::floor((f.eye.z - CS * 0.5f) / sn + 0.5f) * sn; s2(pWater, "uCenter", cx, cz); s1(pWater, "uSp", sp); int r = L == 0 ? 0 : 1; glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrIbo[r]); glDrawElements(GL_TRIANGLES, terrNI[r], GL_UNSIGNED_SHORT, nullptr); drawCalls++; }
  // waterfalls + mist
  { glUseProgram(pFall); sm(pFall, "uVP", vpM); s1(pFall, "uTime", f.time); s1(pFall, "uAmb", 0.22f + 0.8f * E.day + 0.1f * E.tw); s3(pFall, "uSunCol", E.keyCol * 0.3f); V3 right{camW.m[0], camW.m[1], camW.m[2]}, up{camW.m[4], camW.m[5], camW.m[6]}; s3(pFall, "uRight", right); s3(pFall, "uUp", up);
    for (size_t i = 0; i < falls.size() && i < 8; i++) { const Fall& fa = falls[i]; float dd = (fa.x - f.eye.x) * (fa.x - f.eye.x) + (fa.z - f.eye.z) * (fa.z - f.eye.z); if (dd > 1100.f * 1100.f) continue; glUniform4f(U(pFall, "uPos"), fa.x, fa.yTop, fa.z, fa.w); glUniform4f(U(pFall, "uDir"), fa.dx, fa.dz, fa.yBot, fa.yTop - fa.yBot);
      si(pFall, "uMode", 0); glBindVertexArray(fallVao); glDrawElements(GL_TRIANGLES, fallNI, GL_UNSIGNED_SHORT, nullptr); si(pFall, "uMode", 1); glDepthMask(GL_FALSE); glBindVertexArray(mistVao); glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, 14); glDepthMask(GL_TRUE); drawCalls += 2; } }
  glDisable(GL_BLEND);
  if (curMsaa > 0) { glBindFramebuffer(GL_READ_FRAMEBUFFER, fboMS); glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboScene); glBlitFramebuffer(0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST); }
  // post
  glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glBindVertexArray(vaoEmpty); glBindFramebuffer(GL_FRAMEBUFFER, fboA); glViewport(0, 0, W, H); glUseProgram(pFogP);
  glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texScene); glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, texDepth); si(pFogP, "tC", 0); si(pFogP, "tD", 1); sm(pFogP, "uInvProj", invProj); sm(pFogP, "uCamW", camW); s2(pFogP, "uNF", 0.25f, 9000.f); s2(pFogP, "uRes", (float)W, (float)H);
  s3(pFogP, "uFog", E.fog); s1(pFogP, "uDens", E.dens); s1(pFogP, "uAO", q.ao ? 0.8f : 0.f); s1(pFogP, "uTime", f.time); s3(pFogP, "uMoon", E.sunDir); s1(pFogP, "uMoonI", 0.9f * E.sunVis * E.day + 0.2f * E.tw);
  { float lp[12], lc[12]; for (int i = 0; i < 4; i++) { lp[i * 3] = pl[i * 4]; lp[i * 3 + 1] = pl[i * 4 + 1]; lp[i * 3 + 2] = pl[i * 4 + 2]; float k = E.night * 0.012f; lc[i * 3] = plc[i * 4] * k * 0.02f; lc[i * 3 + 1] = plc[i * 4 + 1] * k * 0.02f; lc[i * 3 + 2] = plc[i * 4 + 2] * k * 0.02f; } glUniform3fv(U(pFogP, "uLP"), 4, lp); glUniform3fv(U(pFogP, "uLC"), 4, lc); }
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glUseProgram(pDownP); si(pDownP, "tC", 0); GLuint src = texA; int sw = W, sh2 = H; for (int i = 0; i < 4; i++) { glBindFramebuffer(GL_FRAMEBUFFER, fboB[i]); glViewport(0, 0, bw[i], bh[i]); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, src); s2(pDownP, "uTx", 1.f / sw, 1.f / sh2); s1(pDownP, "uThr", i == 0 ? 1.6f : 0.f); glDrawArrays(GL_TRIANGLES, 0, 3); src = texB[i]; sw = bw[i]; sh2 = bh[i]; }
  glBindFramebuffer(GL_FRAMEBUFFER, 0); glViewport(0, 0, winW, winH); glUseProgram(pFinalP); GLuint tx[5] = {texA, texB[0], texB[1], texB[2], texB[3]}; const char* nm[5] = {"tC", "tB1", "tB2", "tB3", "tB4"}; for (int i = 0; i < 5; i++) { glActiveTexture(GL_TEXTURE0 + i); glBindTexture(GL_TEXTURE_2D, tx[i]); si(pFinalP, nm[i], i); } glActiveTexture(GL_TEXTURE0);
  s2(pFinalP, "uRes", (float)W, (float)H); s1(pFinalP, "uTime", f.time); s1(pFinalP, "uExp", E.exposure); s1(pFinalP, "uFocus", 20.f); s1(pFinalP, "uAp", 0.f); s1(pFinalP, "uBloom", E.bloom); s1(pFinalP, "uVig", 0.9f); s1(pFinalP, "uFlash", f.flash); s1(pFinalP, "uSat", 1.12f); s1(pFinalP, "uCon", 0.55f); s1(pFinalP, "uCol", 1.f); s1(pFinalP, "uFade", f.fade);
  glDrawArrays(GL_TRIANGLES, 0, 3); glEnable(GL_DEPTH_TEST);
}

void drawMap(const MapView& v) {
  if (!inited) return; glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glUseProgram(pMapP); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texMap); si(pMapP, "uMap", 0); s2(pMapP, "uRes", (float)winW, (float)winH);
  glUniform4f(U(pMapP, "uRect"), v.px, v.py, v.size, 0); s2(pMapP, "uCenter", v.cx, v.cz); s1(pMapP, "uHalf", v.half); s1(pMapP, "uRot", v.rot); s1(pMapP, "uYaw", v.yaw); si(pMapP, "uMode", v.mode);
  static float poi[48 * 4]; for (int i = 0; i < 48; i++) { if (i < (int)pois.size()) { const Poi& p = pois[i]; poi[i * 4] = p.x; poi[i * 4 + 1] = p.z; poi[i * 4 + 2] = p.kind == 0 ? 0.f : (p.kind == 1 || p.kind == 5 ? 1.f : p.kind == 3 || p.kind == 2 ? 3.f : 4.f); poi[i * 4 + 3] = p.found ? 1.f : (v.mode == 1 ? 0.f : -1.f); } else { poi[i * 4 + 3] = -1.f; } } glUniform4fv(U(pMapP, "uPoi"), 48, poi);
  float nat[12], natc[9]; for (int i = 0; i < 3; i++) { nat[i * 4] = nations[i].x; nat[i * 4 + 1] = nations[i].z; nat[i * 4 + 2] = 0; nat[i * 4 + 3] = 0; natc[i * 3] = ((nations[i].col >> 16) & 255) / 255.f; natc[i * 3 + 1] = ((nations[i].col >> 8) & 255) / 255.f; natc[i * 3 + 2] = (nations[i].col & 255) / 255.f; } glUniform4fv(U(pMapP, "uNat"), 3, nat); glUniform3fv(U(pMapP, "uNatC"), 3, natc);
  glUniform4f(U(pMapP, "uRect"), v.px, v.py, v.size, 0); glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST);
}
}  // namespace xr
