// All GLSL (OpenGL 3.3 core). The post chain (fog/AO/in-scatter, bloom, DOF, AgX, grade) is a direct port of 40-post.js,
// the terrain / grass / sky shaders are ports of 30-world.js, lighting mimics three.js' MeshStandardMaterial (GGX, physical point-light falloff).
#pragma once
#define GLSL(...) "#version 330 core\n" #__VA_ARGS__
#define GLSLC(...) #__VA_ARGS__
namespace sh {
inline const char* TERR = GLSLC(
const float KW1 = 0.00153398;
float pathZ(float x){ return 2.4*sin(x*KW1*20.) + 1.3*sin(x*KW1*9.+1.7); }
float terrH(vec2 p){ float d = abs(p.y - pathZ(p.x)); float k = smoothstep(6.5,20.,d), far_ = smoothstep(30.,150.,d);
  float h1 = 1.6*sin(p.x*KW1*13.+p.y*.071+1.3) + 1.2*sin(p.x*KW1*31.-p.y*.113+4.1) + .5*sin(p.x*KW1*67.+p.y*.31) + .35*sin(-p.x*KW1*113.+p.y*.47+2.);
  float h2 = far_*far_*(14.+9.*sin(p.x*KW1*5.+p.y*.011)+6.*sin(p.x*KW1*11.-p.y*.023+1.));
  return k*h1*.6+h2; }
);
inline const char* LIGHT = GLSLC(
uniform vec3 uCam; uniform vec3 uMoonDir, uMoonCol, uHemiSky, uHemiGnd, uFillDir, uFillCol; uniform vec4 uPL[4]; uniform vec4 uPLC[4];
uniform mat4 uShadowVP; uniform sampler2DShadow uShadowTex; uniform float uShadowOn;
const float PI_ = 3.14159265;
float shadowAt(vec3 wp, vec3 n){
  if (uShadowOn < .5) return 1.;
  vec4 sp = uShadowVP * vec4(wp + n*0.09, 1.); vec3 p = sp.xyz/sp.w*.5+.5;
  if (p.z > 1. || p.x < 0. || p.y < 0. || p.x > 1. || p.y > 1.) return 1.;
  float s = 0.; float ts = 1./2048.;
  for (int i=-1;i<=1;i++) for (int j=-1;j<=1;j++) s += texture(uShadowTex, vec3(p.xy + vec2(i,j)*ts*1.5, p.z - 0.0012));
  return s/9.; }
vec3 envSky(vec3 n){ float h = n.y; vec3 c = mix(vec3(.035,.05,.085), vec3(.012,.018,.035), smoothstep(-.1,.8,h)); c = mix(c, vec3(.02,.016,.012), smoothstep(.0,-.4,h));
  c += vec3(.75,.85,1.1)*pow(max(dot(n,uMoonDir),0.),40.)*1.4; c += vec3(.5,.25,.08)*pow(max(dot(n,normalize(vec3(-.3,.1,.9))),0.),6.)*.22; return c; }
float D_GGX(float nh, float a){ float a2 = a*a; float d = nh*nh*(a2-1.)+1.; return a2/(PI_*d*d); }
float V_SmithJ(float nl, float nv, float a){ float a2=a*a; float gv = nl*sqrt(nv*nv*(1.-a2)+a2); float gl = nv*sqrt(nl*nl*(1.-a2)+a2); return .5/max(gv+gl,1e-4); }
vec3 brdf(vec3 diff, vec3 f0, float a, vec3 N, vec3 V, vec3 L){
  float nl = max(dot(N,L),0.); if (nl <= 0.) return vec3(0.);
  vec3 H = normalize(L+V); float nv = max(dot(N,V),1e-3), nh = max(dot(N,H),0.), vh = max(dot(V,H),0.);
  vec3 F = f0 + (1.-f0)*pow(1.-vh,5.);
  return (diff*(1.-F)/PI_ + F*D_GGX(nh,a)*V_SmithJ(nl,nv,a)) * nl; }
vec3 shade(vec3 albedo, float rough, float metal, vec3 N, vec3 P, float sh){
  vec3 V = normalize(uCam - P); vec3 f0 = mix(vec3(.04), albedo, metal); vec3 diff = albedo*(1.-metal); float a = max(rough*rough, .004);
  vec3 col = vec3(0.);
  col += brdf(diff,f0,a,N,V,uMoonDir) * uMoonCol * sh;
  col += brdf(diff,f0,a,N,V,normalize(uFillDir)) * uFillCol;
  col += diff/PI_ * mix(uHemiGnd, uHemiSky, .5+.5*N.y);
  col += diff * envSky(N) * .55 + f0 * envSky(reflect(-V,N)) * mix(1.,.15,rough) * (1.-.0*rough);
  for (int i=0;i<4;i++){ vec3 Lv = uPL[i].xyz - P; float d = length(Lv); if (uPLC[i].x+uPLC[i].y+uPLC[i].z <= 0.) continue; Lv/=d;
    float w = pow(clamp(1.-pow(d/uPL[i].w,4.),0.,1.),2.); float att = w/pow(max(d,.01), uPLC[i].w);
    col += brdf(diff,f0,a,N,V,Lv) * uPLC[i].rgb * att; }
  return col; }
vec3 bump(vec3 N, vec3 P, float h, float k){ vec3 dpx = dFdx(P), dpy = dFdy(P); float dhx = dFdx(h), dhy = dFdy(h);
  vec3 r1 = cross(dpy, N), r2 = cross(N, dpx); float det = dot(dpx, r1); if (abs(det) < 1e-12) return N; vec3 g = sign(det)*(dhx*r1 + dhy*r2); return normalize(abs(det)*N - k*g); }
float hsh(vec3 p){ p = fract(p*.1031); p += dot(p, p.yzx+33.33); return fract((p.x+p.y)*p.z); }
float vn(vec3 p){ vec3 i=floor(p), f=fract(p); f=f*f*(3.-2.*f); return mix(mix(mix(hsh(i),hsh(i+vec3(1,0,0)),f.x),mix(hsh(i+vec3(0,1,0)),hsh(i+vec3(1,1,0)),f.x),f.y),mix(mix(hsh(i+vec3(0,0,1)),hsh(i+vec3(1,0,1)),f.x),mix(hsh(i+vec3(0,1,1)),hsh(i+vec3(1,1,1)),f.x),f.y),f.z); }
);
// ---- instanced mesh (props, characters) ----
inline const char* MESH_VS = GLSL(
layout(location=0) in vec3 aPos; layout(location=1) in vec3 aNrm; layout(location=2) in vec3 aCol; layout(location=3) in vec3 aMat;
layout(location=4) in vec4 iM0; layout(location=5) in vec4 iM1; layout(location=6) in vec4 iM2; layout(location=7) in vec4 iM3; layout(location=8) in vec4 iTint; layout(location=9) in vec4 iMat;
uniform mat4 uVP; out vec3 vWp; out vec3 vN; out vec3 vCol; out vec3 vMat; out vec3 vLoc; out float vFlash;
void main(){ mat4 M = mat4(iM0,iM1,iM2,iM3); vec4 wp = M*vec4(aPos,1.); vWp = wp.xyz; mat3 m3 = mat3(M); vN = normalize(transpose(inverse(m3))*aNrm);
  vCol = aCol*iTint.rgb; vMat = aMat*iMat.xyz; vLoc = aPos; vFlash = iTint.a; gl_Position = uVP*wp; });
inline const char* MESH_FS_BODY = GLSLC(
in vec3 vWp; in vec3 vN; in vec3 vCol; in vec3 vMat; in vec3 vLoc; in float vFlash; out vec4 oC;
void main(){ vec3 N = normalize(vN); if (!gl_FrontFacing) N = -N;
  float n1 = vn(vLoc*7.), n2 = vn(vLoc*23.+3.); vec3 alb = vCol*(.78+.34*n1)*(.9+.2*n2);
  float sh = shadowAt(vWp, N); vec3 c = shade(alb, clamp(vMat.x,.04,1.), vMat.y, N, vWp, sh);
  c += alb*vMat.z + vFlash*vec3(1.,.72,.45)*.55; oC = vec4(c,1.); }
);
inline const char* SHADOW_VS = GLSL(
layout(location=0) in vec3 aPos; layout(location=4) in vec4 iM0; layout(location=5) in vec4 iM1; layout(location=6) in vec4 iM2; layout(location=7) in vec4 iM3; uniform mat4 uVP;
void main(){ gl_Position = uVP*(mat4(iM0,iM1,iM2,iM3)*vec4(aPos,1.)); });
inline const char* SHADOW_FS = GLSL(void main(){});
// ---- terrain ----
inline const char* TERR_VS_BODY = GLSLC(
layout(location=0) in vec2 aXZ; uniform mat4 uVP; uniform vec2 uOrigin; uniform float uY; out vec3 vWp; out vec3 vN;
void main(){ vec2 p = aXZ + uOrigin; float hC = terrH(p); float ge = .6;
  vN = normalize(vec3(terrH(p-vec2(ge,0.))-terrH(p+vec2(ge,0.)), 2.*ge, terrH(p-vec2(0.,ge))-terrH(p+vec2(0.,ge))));
  vWp = vec3(p.x, hC+uY, p.y); gl_Position = uVP*vec4(vWp,1.); }
);
inline const char* TERR_FS_BODY = GLSLC(
in vec3 vWp; in vec3 vN; out vec4 oC; uniform vec3 uGA, uGB, uMud; uniform float uWet;
float gh(vec2 p){ vec3 p3=fract(vec3(p.xyx)*.1031); p3+=dot(p3,p3.yzx+33.33); return fract((p3.x+p3.y)*p3.z); }
float gn(vec2 p, float per){ vec2 i=floor(p), f=fract(p); f=f*f*(3.-2.*f); float x0=mod(i.x,per), x1=mod(i.x+1.,per);
  return mix(mix(gh(vec2(x0,i.y)),gh(vec2(x1,i.y)),f.x),mix(gh(vec2(x0,i.y+1.)),gh(vec2(x1,i.y+1.)),f.x),f.y); }
float GN(vec2 p, float f){ return gn(p*f, 4096.*f); }
void main(){
  vec2 p = vWp.xz; float s = p.y - pathZ(p.x), dd = abs(s);
  float n0=GN(p,.125), n1=GN(p,.5), n2=GN(p,1.), n3=GN(p,4.), n4=GN(p,16.), n5=GN(p,64.);
  float edge = dd + (n2-.5)*1.5 + (n3-.5)*.5;
  float track = 1.-smoothstep(1.0, 2.5, edge);
  float rut = smoothstep(.2,.03, abs(dd-.62+(n2-.5)*.2)) * track;
  float hoof = smoothstep(.62,.8, n3) * track;
  float h = n3*.012 + n4*.012 + n5*.005 - rut*.05 - hoof*.03 + (1.-track)*(n2*.05 + n3*.03);
  vec3 heath = mix(uGA, uGB, smoothstep(.3,.7,n2)) * (.55+.9*n4);
  heath = mix(heath, vec3(.12,.1,.05), smoothstep(.55,.8,n0)*.45*(.5+n4));
  heath = mix(heath, vec3(.05,.035,.05)*1.2, smoothstep(.6,.75,GN(p,.25))*.5);
  vec3 mud = uMud*(.5+.7*n4)*(.8+.4*n1);
  vec3 col = mix(heath, mud, track*.92);
  float low = smoothstep(.62-.3*uWet, .78-.3*uWet, n1*.65+n0*.35);
  float wet = clamp(low*(.35+track*.8) + rut*uWet*.9 + hoof*uWet*.5, 0., 1.);
  float wn = wet + (n3-.5)*.05; float water = smoothstep(.74,.79, wn) * step(dd, 9.); float shore = smoothstep(.6,.79, wn) * step(dd, 9.);
  col *= mix(1., .5, wet); float rough = mix(.96, .42, wet*track + wet*.3);
  vec2 pw = p*11. + vec2(n3, n4)*.9; vec2 pc = floor(pw), pf = fract(pw); vec2 pk = vec2(mod(pc.x,45056.),pc.y); float ph = gh(pk); vec2 po = vec2(gh(pk+3.1), gh(pk+7.7))*.6+.2;
  float pr = .1 + .3*gh(pk+1.7); float pd = length((pf-po)*vec2(1., .7+.6*gh(pk+5.)))/pr; float peb = step(.9-.12*track, ph)*smoothstep(1.,.55,pd)*(1.-water);
  col = mix(col, vec3(.05,.05,.048)*(.4+1.2*gh(pk+9.)), peb*.9); h += peb*.022*(1.-pd*pd); rough = mix(rough, .85, peb);
  col *= mix(1., .6, shore); rough = mix(rough, .3, shore*.8); h = mix(h, -.012, shore);
  col = mix(col, vec3(.008,.011,.014), water); rough = mix(rough, .035, water);
  h = h*1.2*(1.-water);
  vec3 N = normalize(vN); N = bump(N, vWp, h, 90.);
  float sh = shadowAt(vWp, N); vec3 c = shade(col, rough, 0., N, vWp, sh); oC = vec4(c,1.); }
);
// ---- grass (instanced blades) ----
inline const char* GRASS_VS_BODY = GLSLC(
layout(location=0) in vec3 position; layout(location=1) in vec3 aBase; layout(location=2) in vec4 aRnd;
uniform mat4 uVP; uniform vec3 uCenter; uniform vec2 uTile; uniform float uTime, uGrassH, uHeath; uniform vec3 uGc1,uGc2,uGc3; out vec3 vGc; out vec3 vWp; out vec3 vN;
void main(){
  vec2 gxz = mod(aBase.xy - uCenter.xz + .5*uTile, uTile) - .5*uTile + uCenter.xz;
  float gdd = abs(gxz.y - pathZ(gxz.x));
  float gkeep = smoothstep(1.2, 3.0, gdd + (fract(aRnd.x*7.31)-.5)*1.8);
  vec2 gq = abs(gxz - uCenter.xz)/(.5*uTile); float gfade = 1.-smoothstep(.84,1.,max(gq.x,gq.y));
  float gt = position.y;
  float gH = aBase.z * gkeep * gfade * mix(1., uGrassH, step(.5, aRnd.z)) * (aRnd.z < 0. ? uHeath : 1.);
  vec2 gdir = vec2(cos(aRnd.x), sin(aRnd.x));
  float gwind = sin(uTime*1.1 + gxz.x*.31 + gxz.y*.2)*.5 + sin(uTime*2.3 + gxz.x*.9 - gxz.y*.6)*.3 + sin(uTime*5.1 + aRnd.x*9.)*.12;
  float glean = aRnd.y + gwind*.26;
  vec2 gld = normalize(gdir*.8 + vec2(.75,.2));
  float ghz = gH*gt*gt*sin(clamp(glean,-1.4,1.4))*.9, gvt = gH*gt*(1.-.3*gt*abs(sin(glean)));
  vN = normalize(vec3(gdir.y*.5*position.x + gld.x*.25*gt, 1., -gdir.x*.5*position.x + gld.y*.25*gt));
  vec3 transformed = vec3(gxz.x, terrH(gxz), gxz.y) + vec3(gdir.y,0.,-gdir.x)*position.x*aRnd.w*(1.-gt*.86) + vec3(gld.x*ghz, gvt, gld.y*ghz);
  vGc = mix(mix(uGc1, uGc2, smoothstep(0.,.5,aRnd.z)), uGc3, smoothstep(.5,1.,aRnd.z)) * mix(.22, 1.05, gt) * (.8+.4*fract(aRnd.x*3.7));
  if (aRnd.z < 0.) vGc = mix(vec3(.028,.02,.03), vec3(.07,.035,.06), -aRnd.z) * mix(.4, 1.3, gt);
  vWp = transformed; gl_Position = uVP*vec4(transformed,1.); }
);
inline const char* GRASS_FS_BODY = GLSLC(
in vec3 vGc; in vec3 vWp; in vec3 vN; out vec4 oC;
void main(){ vec3 N = normalize(vN); float sh = shadowAt(vWp, N); vec3 c = shade(vGc, .78, 0., N, vWp, sh); oC = vec4(c,1.); }
);
// ---- sky (fullscreen, depth = 1) ----
inline const char* SKY_VS = GLSL(
uniform mat4 uInvVP; uniform vec3 uCam; out vec3 vD;
void main(){ vec2 p = vec2((gl_VertexID==1)?3.:-1., (gl_VertexID==2)?3.:-1.); vec4 w = uInvVP*vec4(p,1.,1.); vD = w.xyz/w.w - uCam; gl_Position = vec4(p,1.,1.); });
inline const char* SKY_FS = GLSL(
in vec3 vD; out vec4 oC; uniform vec3 uMoon, uA, uB; uniform float uTime, uCloud, uMoonI;
float h(vec2 p){ vec3 p3=fract(vec3(p.xyx)*.1031); p3+=dot(p3,p3.yzx+33.33); return fract((p3.x+p3.y)*p3.z); }
float n(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.-2.*f); return mix(mix(h(i),h(i+vec2(1.,0.)),f.x),mix(h(i+vec2(0.,1.)),h(i+vec2(1.,1.)),f.x),f.y); }
float fb(vec2 p){ float s=0., a=.5; for(int i=0;i<5;i++){ s+=a*n(p); p=p*2.03+vec2(3.1,1.7); a*=.5; } return s; }
void main(){ vec3 d = normalize(vD); float hz = 1.-clamp(d.y,0.,1.);
  vec3 c = mix(uA, uB, pow(hz, 2.6));
  float m = max(dot(d, uMoon), 0.);
  vec2 uv = d.xz/(abs(d.y)+.18)*.9; float cl = fb(uv + vec2(uTime*.006, uTime*.002)); float cl2 = fb(uv*2.3 - vec2(uTime*.01,0.) + 9.);
  float dens = smoothstep(1.02-uCloud*.75, 1.25-uCloud*.6, cl+cl2*.45) * smoothstep(-.02,.08,d.y);
  float st = step(.9975, h(floor(d.xz/(abs(d.y)+.4)*420.))) * smoothstep(.05,.4,d.y);
  c += vec3(.55,.62,.8)*st*.5*(1.-dens);
  float disc = smoothstep(.99925,.99955,m);
  c += uMoonI*(vec3(.9,1.,1.25)*disc*4.*(1.-dens*.92) + vec3(.35,.45,.7)*(pow(m,400.)*.5 + pow(m,24.)*.11 + pow(m,4.)*.03));
  vec3 cc = mix(vec3(.012,.017,.03), vec3(.16,.2,.3)*uMoonI, pow(m,7.)*(1.-smoothstep(.2,.9,cl))*1.2 + .05);
  c = mix(c, cc, dens*.93);
  oC = vec4(c,1.); });
// ---- billboard particles ----
inline const char* PART_VS = GLSL(
layout(location=0) in vec2 aQ; layout(location=1) in vec4 iPS; layout(location=2) in vec4 iCol; layout(location=3) in vec4 iVel;
uniform mat4 uVP; uniform vec3 uCam; uniform vec3 uRight, uUp; out vec2 vUv; out vec4 vCol; out float vKind;
void main(){ vec3 pos = iPS.xyz; float size = iPS.w; vKind = iVel.w; vec3 w;
  if (vKind > 2.5) { vec3 ax = iVel.xyz; float sp = length(ax); vec3 toCam = normalize(uCam-pos); vec3 axn = sp>1e-3 ? ax/sp : vec3(0,1,0); vec3 side = normalize(cross(axn,toCam)); float L = vKind > 3.5 ? 0.35 : clamp(sp*.045, size*1.5, .55);
    w = pos + side*aQ.x*size + axn*aQ.y*L; }
  else w = pos + uRight*aQ.x*size + uUp*aQ.y*size;
  vUv = aQ; vCol = iCol; gl_Position = uVP*vec4(w,1.); });
inline const char* PART_FS = GLSL(
in vec2 vUv; in vec4 vCol; in float vKind; out vec4 oC; uniform float uAdd;
void main(){ float r2 = dot(vUv,vUv); if (vKind > 2.5) { float a = (1.-abs(vUv.x))*(1.-abs(vUv.y)*.6); a = pow(max(a,0.),1.5); if (uAdd > .5) oC = vec4(vCol.rgb*vCol.a*a, 1.); else oC = vec4(vCol.rgb, vCol.a*a); return; }
  if (r2 > 1.) discard; float a;
  if (vKind > 1.5) a = smoothstep(1.,.7,r2); else if (vKind > .5) a = exp(-r2*2.2); else a = exp(-r2*4.5);
  if (uAdd > .5) oC = vec4(vCol.rgb*vCol.a*a, 1.); else oC = vec4(vCol.rgb, vCol.a*a); });
// ---- ground marks (rings, zones, telegraphs) ----
inline const char* MARK_VS = GLSL(
layout(location=0) in vec2 aQ; layout(location=1) in vec4 iA; layout(location=2) in vec4 iB; layout(location=3) in vec4 iC;
uniform mat4 uVP; out vec2 vUv; out vec4 vB; out vec4 vC; out float vK;
void main(){ float R = iA.z*(iB.w > 0.5 ? 1.15 : 1.15); vec3 w = vec3(iA.x + aQ.x*R, iA.w, iA.y + aQ.y*R); vUv = aQ*1.15; vB = iB; vC = iC; vK = iC.w; gl_Position = uVP*vec4(w,1.); });
inline const char* MARK_FS = GLSL(
in vec2 vUv; in vec4 vB; in vec4 vC; in float vK; out vec4 oC; uniform float uTime;
float h2(vec2 p){ return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453); }
float n2(vec2 p){ vec2 i=floor(p), f=fract(p); f=f*f*(3.-2.*f); return mix(mix(h2(i),h2(i+vec2(1,0)),f.x),mix(h2(i+vec2(0,1)),h2(i+vec2(1,1)),f.x),f.y); }
void main(){ float d = length(vUv); if (d > 1.15) discard; vec3 col = vB.rgb; float a = 0.; float prog = vC.x, thick = vC.y, alpha = vC.z;
  if (vK < .5) { float rd = abs(d - 1.0); a = smoothstep(thick, 0., rd) * 1.0 + smoothstep(thick*4., 0., rd)*.25; a *= (1.-prog*prog); }
  else if (vK < 1.5) { float fill = smoothstep(1.,.97,d) * (.25 + .35*prog); float rim = smoothstep(.06,0.,abs(d-1.)); float pulse = .75+.25*sin(uTime*18.); a = (fill*pulse + rim*1.4); }
  else if (vK < 2.5) { float nn = n2(vUv*5.+vec2(0.,-uTime*2.)) * n2(vUv*9.+uTime); a = smoothstep(1.,.85,d) * (.18 + .55*nn) + smoothstep(.08,0.,abs(d-1.))*.5; }
  else if (vK < 3.5) { float nn = n2(vUv*7.); float cr = smoothstep(.06,0.,abs(nn-.5)); a = smoothstep(1.,.9,d)*(.12+.2*nn) + cr*.3*smoothstep(1.,.2,d) + smoothstep(.07,0.,abs(d-1.))*.6; }
  else { float rd = abs(d-1.); a = smoothstep(.12,0.,rd)*(1.-prog) + smoothstep(.5,0.,d)*.12*(1.-prog); }
  a *= alpha; oC = vec4(col*a, 1.); });
// ---- post ----
inline const char* FS_VS = GLSL(out vec2 vUv; void main(){ vec2 p = vec2((gl_VertexID==1)?3.:-1., (gl_VertexID==2)?3.:-1.); vUv = p*.5+.5; gl_Position = vec4(p,0.,1.); });
inline const char* FOG_FS = GLSL(
precision highp float; in vec2 vUv; out vec4 oC; uniform sampler2D tC, tD; uniform mat4 uInvProj, uCamW; uniform vec2 uNF, uRes; uniform vec3 uFog, uMoon; uniform float uDens, uAO, uTime, uMoonI;
uniform vec3 uLP[4]; uniform vec3 uLC[4];
float lin(float d){ return uNF.x*uNF.y / (uNF.y - d*(uNF.y-uNF.x)); }
float ign(vec2 p){ return fract(52.9829189*fract(dot(p, vec2(.06711056,.00583715)))); }
void main(){
  vec3 col = texture(tC, vUv).rgb; float d = texture(tD, vUv).x; float z = lin(d);
  vec4 vp = uInvProj * vec4(vUv*2.-1., 1., 1.); vec3 vdir = normalize(vp.xyz/vp.w); vec3 wdir = normalize((uCamW * vec4(vdir,0.)).xyz); vec3 ro = uCamW[3].xyz;
  float dist = z / max(-vdir.z, 1e-4); bool isSky = d >= .99999;
  float ao = 1.;
  if (!isSky && uAO > 0.) {
    float rad = clamp(.42 / z, .003, .06); float ang = ign(gl_FragCoord.xy) * 6.2832; float occ = 0.;
    for (int i = 0; i < 8; i++) {
      float a = ang + float(i)*2.39996; float r = rad * (.3 + .7*sqrt((float(i)+.5)/8.));
      vec2 o = vec2(cos(a), sin(a)) * r * vec2(uRes.y/uRes.x, 1.);
      float z1 = lin(texture(tD, vUv + o).x), z2 = lin(texture(tD, vUv - o).x);
      float d1 = z - z1, d2 = z - z2; float rng = .55;
      float d1c = clamp(d1, -rng, rng), d2c = clamp(d2, -rng, rng);
      float keep = 1. - smoothstep(rng*.8, rng*1.6, max(abs(d1), abs(d2)) - min(abs(d1), abs(d2)));
      occ += clamp((d1c + d2c) / .2, 0., 1.) * max(keep, step(max(abs(d1),abs(d2)), rng));
    }
    ao = 1. - uAO * clamp(occ / 8. * 1.5, 0., .85);
    float lum = dot(col, vec3(.3,.5,.2)); ao = mix(ao, 1., clamp(lum*.35, 0., .6));
  }
  col *= ao;
  float T = isSky ? 420. : dist;
  float hb = .11; float dy = wdir.y; float k = abs(dy) < 1e-3 ? T : (1.-exp(-hb*dy*T))/(hb*dy);
  float od = uDens * exp(-hb*max(ro.y,0.)) * k * 1.9 + uDens * T * .22;
  float fa = 1. - exp(-od);
  vec3 fogc = uFog * (1. + uMoonI * 1.2 * pow(max(dot(wdir, uMoon), 0.), 5.));
  if (isSky) fa *= smoothstep(.34, -.02, dy);
  col = mix(col, fogc, fa);
  float Tm = min(T, 90.);
  for (int i = 0; i < 4; i++) {
    vec3 q = ro - uLP[i]; float b = dot(wdir, q), c = dot(q, q); float s = 1. / sqrt(max(c - b*b, 1e-3));
    float I = s * (atan((Tm + b)*s) - atan(b*s));
    col += uLC[i] * I * uDens;
  }
  oC = vec4(max(col, vec3(0.)), z);
}
);
inline const char* DOWN_FS = GLSL(
precision highp float; in vec2 vUv; out vec4 oC; uniform sampler2D tC; uniform vec2 uTx; uniform float uThr;
vec3 pre(vec3 c){ if (uThr <= 0.) return c; float l = max(c.r, max(c.g, c.b)); float k = max(l - uThr, 0.); return c * (k / max(l, 1e-4)); }
void main(){ vec3 c = pre(texture(tC, vUv).rgb)*.25;
  c += pre(texture(tC, vUv + uTx*vec2(1.,1.)).rgb)*.1875; c += pre(texture(tC, vUv + uTx*vec2(-1.,1.)).rgb)*.1875;
  c += pre(texture(tC, vUv + uTx*vec2(1.,-1.)).rgb)*.1875; c += pre(texture(tC, vUv + uTx*vec2(-1.,-1.)).rgb)*.1875;
  oC = vec4(min(c, vec3(24.)), 1.); });
inline const char* FINAL_FS = GLSL(
precision highp float; in vec2 vUv; out vec4 oC; uniform sampler2D tC, tB1, tB2, tB3, tB4; uniform vec2 uRes; uniform float uTime, uExp, uFocus, uAp, uBloom, uVig, uFlash, uSat, uCon, uCol, uFade;
float coc(float z){ float c = (z - uFocus) / z; c = c > 0. ? max(c - .1, 0.) * 9. : max(-c - .22, 0.) * 5.; return min(c * uAp, 7.); }
float h(vec2 p){ vec3 p3=fract(vec3(p.xyx)*.1031); p3+=dot(p3,p3.yzx+33.33); return fract((p3.x+p3.y)*p3.z); }
vec3 agx(vec3 c){
  const mat3 mi = mat3(.842479062253094,.0423282422610123,.0423756549057051,.0784335999999992,.878468636469772,.0784336,.0792237451477643,.0791661274605434,.879142973793104);
  const mat3 mo = mat3(1.19687900512017,-.0528968517574562,-.0529716355144438,-.0980208811401368,1.15190312990417,-.0980434501171241,-.0990297440797205,-.0989611768448433,1.15107367264116);
  c = mi * c; c = clamp((log2(max(c, 1e-10)) + 12.47393) / 16.5, 0., 1.);
  vec3 c2 = c*c, c4 = c2*c2;
  c = 15.5*c4*c2 - 40.14*c4*c + 31.96*c4 - 6.868*c2*c + .4298*c2 + .1191*c - .00232;
  c = mo * c; return c; }
void main(){
  vec4 cz = texture(tC, vUv); vec3 col = cz.rgb; float z = cz.a; float cc = coc(z);
  if (uAp > 0.) {
    float acc = 1.; vec2 px = 1./uRes; float rmax = 7.*uAp; float a0 = h(gl_FragCoord.xy)*6.2832;
    for (int i = 0; i < 22; i++) {
      float fi = float(i)+.5; float r = sqrt(fi/22.) * rmax; float a = a0 + fi*2.39996;
      vec2 uv = vUv + vec2(cos(a), sin(a)) * r * px;
      vec4 s = texture(tC, uv); float sc = coc(s.a);
      if (s.a > z) sc = min(sc, cc*2.);
      float m = smoothstep(r-.5, r+.5, sc);
      col += mix(col/acc, s.rgb, m); acc += 1.;
    }
    col /= acc;
  }
  vec3 bl = texture(tB1, vUv).rgb*.34 + texture(tB2, vUv).rgb*.3 + texture(tB3, vUv).rgb*.22 + texture(tB4, vUv).rgb*.14;
  col += bl * uBloom;
  col *= uExp;
  vec2 q = vUv - .5; float vg = 1. - dot(q,q) * 1.25 * uVig; col *= clamp(vg, 0., 1.) * mix(1., vg, .5);
  col += uFlash * vec3(1., .82, .55);
  col = agx(col);
  float l = dot(col, vec3(.2126,.7152,.0722));
  col = mix(vec3(l), col, 1.22 * uSat);
  col = pow(max(col, 0.), vec3(1.32));
  col += vec3(-.004, .0, .003) * (1.-smoothstep(0., .35, l));
  col = pow(max(col, 0.), vec3(1./2.2));
  { float l2 = dot(col, vec3(.2126,.7152,.0722)), mx = max(col.r, max(col.g, col.b)), mn = min(col.r, min(col.g, col.b)), st = mx - mn;
    float mid = smoothstep(.03,.2,l2) * (1.-smoothstep(.6,.95,l2));
    float hr = (col.g - mn) / max(col.r - mn, 1e-4); float ob = (col.r >= col.g && col.g >= col.b) ? smoothstep(.2,.45,hr) * (1.-smoothstep(.8,1.,hr)) : 0.;
    col = mix(vec3(l2), col, 1. + uCol * .8 * (1.-.6*ob) * mid * (1.-smoothstep(.12,.55,st)));
    float warm = smoothstep(-.04,.12, col.r - col.b);
    col += uCol * vec3(-.022,.006,.034) * (1.-warm) * smoothstep(.0,.05,l2) * (1.-smoothstep(.1,.45,l2));
    col += uCol * vec3(.011,.006,-.008) * warm * smoothstep(.2,.7,l2);
    col = clamp(col, 0., 1.); }
  { col = min(col * (1. + .22*uCon), 1.); vec3 sc = col*col*(3.-2.*col); col = mix(col, sc, uCon); col = max(col - .17*uCon, 0.) / (1. - .17*uCon); }
  float g = h(gl_FragCoord.xy + fract(uTime*7.13)*913.) - .5; col += g * mix(.035, .012, smoothstep(0., .5, l));
  oC = vec4(col*uFade, 1.); });
// ---- UI ----
inline const char* UI_VS = GLSL(
layout(location=0) in vec2 aPos; layout(location=1) in vec2 aUv; layout(location=2) in vec4 aCol; uniform vec2 uRes; out vec2 vUv; out vec4 vCol;
void main(){ vUv = aUv; vCol = aCol; gl_Position = vec4(aPos.x/uRes.x*2.-1., 1.-aPos.y/uRes.y*2., 0., 1.); });
inline const char* UI_FS = GLSL(
in vec2 vUv; in vec4 vCol; out vec4 oC; uniform sampler2D tFont; void main(){ float a = vUv.x < 0. ? 1. : texture(tFont, vUv).r; oC = vec4(vCol.rgb, vCol.a*a); });
}  // namespace sh
