// LANTERNFALL asset bake: runs the ORIGINAL three.js figure generators headless (node) and dumps
//  - exact skinned meshes (P N C UV M BA BB W I) + rig + attachment points
//  - baked animation: world-space bone pose (quat + pos) per frame for idle / ready / walk / run / every action clip / stagger / down
// usage: node tools/bake.js <ref_js dir> <out dir>
const fs = require('fs'), path = require('path'), vm = require('vm');
const REF = path.resolve(process.argv[2] || '_ref_js'), OUT = path.resolve(process.argv[3] || 'out_raw');
fs.mkdirSync(OUT, { recursive: true });
global.window = global; global.location = { search: '' }; global.navigator = { userAgent: 'node' };
global.THREE = require(path.join(REF, 'three.min.js'));
if (!global.THREE.Mesh) global.THREE = global.window.THREE;
let src = '';
for (const f of ['00-util', '10-kit', '20-human', '20b-wardrobe', '21-heroes', '22-monsters', '25-anim']) src += fs.readFileSync(path.join(REF, 'src', f + '.js'), 'utf8').replace(/^'use strict';/, '') + '\n';
src += `
globalThis.__lf = { LF, Figure, HumanAnim, QuadAnim, WispAnim, ease, lerp, TAU, PI };
`;
vm.runInThisContext(src, { filename: 'lf-bundle.js' });
const { LF, HumanAnim, Figure, TAU, PI } = globalThis.__lf;
// deterministic
let seed = 12345; Math.random = () => { seed = (seed * 1664525 + 1013904223) >>> 0; return seed / 4294967296; };

const DT = 1 / 120;
function quatFromR(R, o) {
  const m00 = R[o], m01 = R[o + 1], m02 = R[o + 2], m10 = R[o + 3], m11 = R[o + 4], m12 = R[o + 5], m20 = R[o + 6], m21 = R[o + 7], m22 = R[o + 8];
  const tr = m00 + m11 + m22; let x, y, z, w;
  if (tr > 0) { const s = Math.sqrt(tr + 1) * 2; w = s / 4; x = (m21 - m12) / s; y = (m02 - m20) / s; z = (m10 - m01) / s; }
  else if (m00 > m11 && m00 > m22) { const s = Math.sqrt(1 + m00 - m11 - m22) * 2; w = (m21 - m12) / s; x = s / 4; y = (m01 + m10) / s; z = (m02 + m20) / s; }
  else if (m11 > m22) { const s = Math.sqrt(1 + m11 - m00 - m22) * 2; w = (m02 - m20) / s; x = (m01 + m10) / s; y = s / 4; z = (m12 + m21) / s; }
  else { const s = Math.sqrt(1 + m22 - m00 - m11) * 2; w = (m10 - m01) / s; x = (m02 + m20) / s; y = (m12 + m21) / s; z = s / 4; }
  const l = Math.hypot(x, y, z, w) || 1; return [x / l, y / l, z / l, w / l];
}
function capture(fig, out) {
  const nb = fig.nb; let prevQ = capture.prev;
  for (let b = 0; b < nb; b++) {
    const q = quatFromR(fig.R, b * 9);
    if (prevQ && prevQ.length === nb * 4 + 0 && true) { const d = q[0] * prevQ[b * 4] + q[1] * prevQ[b * 4 + 1] + q[2] * prevQ[b * 4 + 2] + q[3] * prevQ[b * 4 + 3]; if (d < 0) { q[0] = -q[0]; q[1] = -q[1]; q[2] = -q[2]; q[3] = -q[3]; } }
    out.push(q[0], q[1], q[2], q[3], fig.Wp[b * 3], fig.Wp[b * 3 + 1], fig.Wp[b * 3 + 2]);
  }
  const pq = new Array(nb * 4); for (let b = 0; b < nb; b++) for (let k = 0; k < 4; k++) pq[b * 4 + k] = out[out.length - nb * 7 + b * 7 + k]; capture.prev = pq;
}
function mkAnim(def, fig, face) {
  const an = new (def.animClass || HumanAnim)(fig, def.style); an.t = 3.1; an.ph = 0; an.xf = { x: 0, z: 0, f: face || 0, s: 1 };
  return an;
}
function step(an, n, speed, combat, forceT, adv) {
  for (let i = 0; i < n; i++) { if (adv) an.xf.z += speed * DT; an.update(DT, speed, combat, forceT); }
}
function bakeLoop(def, name, isHero, speed, combat, secs, fps, lanternVar, periodic) {
  const fig = def.build({ lantern: lanternVar }); const an = mkAnim(def, fig); an.ikOn = isHero;
  step(an, 360, speed, combat, undefined, true);
  let T = secs;
  if (periodic) { // one gait cycle: measure phase rate
    if (an.ph === undefined) return null;
    const p0 = an.ph; step(an, 120, speed, combat, undefined, true); const rate = (an.ph - p0) / (120 * DT); if (!(rate > 0.1)) return null; T = TAU / rate;
    // align start to phase 0
    let guard = 0; while (guard++ < 2000 && ((an.ph % TAU) + TAU) % TAU > 0.02) step(an, 1, speed, combat, undefined, true);
  }
  const N = Math.max(2, Math.round(T * fps)), h = T / N, out = []; capture.prev = null;
  for (let i = 0; i < N; i++) { const sub = Math.max(1, Math.round(h / DT)); step(an, sub, speed, combat, undefined, true); capture(an.f, out); }
  return { name, fps: N / T, frames: N, loop: 1, hit: 0, speed, T, nb: fig.nb, data: out };
}
function bakeAction(def, name, clip, isHero, lanternVar, face) {
  const fig = def.build({ lantern: lanternVar }); const an = mkAnim(def, fig); an.ikOn = isHero;
  step(an, 240, 0, true, undefined, false);
  const N = 40, out = []; capture.prev = null; const dur = 0.9;
  for (let i = 0; i < N; i++) {
    const u = i / (N - 1) * 0.999; an.act = clip === 'wisp' ? { hit: 0.5 } : clip; for (let k = 0; k < Math.round(dur / N / DT); k++) an.update(DT, 0, true, u);
    capture(an.f, out);
  }
  return { name, fps: 0, frames: N, loop: 0, hit: clip.hit === undefined ? 0.5 : clip.hit, speed: 0, T: 1, nb: fig.nb, data: out };
}
function bakeStag(def, isHero, lanternVar) {
  const fig = def.build({ lantern: lanternVar }); const an = mkAnim(def, fig); an.ikOn = isHero;
  step(an, 240, 0, true, undefined, false); an.stag = 1.15; an.stagDir = 1;
  const N = 20, out = []; capture.prev = null;
  for (let i = 0; i < N; i++) { for (let k = 0; k < 6; k++) an.update(DT, 0, true); capture(an.f, out); }
  return { name: 'stag', fps: 0, frames: N, loop: 0, hit: 0, speed: 0, T: 0.5, nb: fig.nb, data: out };
}
function bakeDown(def, lanternVar) {
  const fig = def.build({ lantern: lanternVar }); const an = mkAnim(def, fig); an.ikOn = true;
  step(an, 240, 0, true, undefined, false); an.down = true;
  const N = 24, out = []; capture.prev = null;
  for (let i = 0; i < N; i++) { for (let k = 0; k < 8; k++) an.update(DT, 0, true); capture(an.f, out); }
  step(an, 360, 0, false, undefined, false); capture(an.f, out); // settled
  return { name: 'down', fps: 0, frames: N + 1, loop: 0, hit: 0, speed: 0, T: 1.3, nb: fig.nb, data: out };
}

const names = [...LF.HERO_ORDER, ...LF.MON_ORDER]; const report = {};
for (const name of names) {
  const isHero = !!LF.HEROES[name], def = LF.HEROES[name] || LF.MONSTERS[name];
  const variants = isHero && name !== 'lamplighter' ? [false, true] : [false];
  for (const lan of variants) {
    const key = name + (lan ? '+L' : '');
    const fig = def.build({ lantern: lan }); const d = fig.d, rig = fig.rig, nv = d.P.length / 3;
    const clips = [];
    const add = c => { if (c) clips.push(c); };
    add(bakeLoop(def, 'idle', isHero, 0, false, 6, 10, lan, false));
    add(bakeLoop(def, 'ready', isHero, 0, true, 6, 10, lan, false));
    if (isHero) { add(bakeLoop(def, 'walk', true, 1.6, false, 0, 30, lan, true)); add(bakeLoop(def, 'run', true, 3.2, true, 0, 30, lan, true)); }
    else { const sp = Math.min(5, Math.max(1.2, (LF.DATA && LF.DATA.MON && LF.DATA.MON[name] && LF.DATA.MON[name].speed) || 2)); add(bakeLoop(def, 'run', false, sp, true, 0, 30, lan, true)); add(bakeLoop(def, 'walk', false, sp * 0.55, false, 0, 30, lan, true)); }
    for (const cn in (def.style.clips || {})) { if (def.style.clips[cn] === def.style.clips.shatter && cn === 'shatter') continue; add(bakeAction(def, cn, def.style.clips[cn], isHero, lan)); }
    if (def.animClass && def.animClass.name === 'WispAnim' && !(def.style.clips && def.style.clips.attack)) add(bakeAction(def, 'attack', { hit: 0.5 }, false, lan));
    add(bakeStag(def, isHero, lan)); if (isHero) add(bakeDown(def, lan));
    // blobs
    const blobs = []; let off = 0; const meta = {};
    const put = (k, arr, type) => { const buf = Buffer.from(arr.buffer, arr.byteOffset, arr.byteLength); meta[k] = { type, count: arr.length, off }; blobs.push(buf); off += buf.length; const pad = (4 - off % 4) % 4; if (pad) { blobs.push(Buffer.alloc(pad)); off += pad; } };
    put('P', new Float32Array(d.P), 'f32'); put('N', new Float32Array(d.N), 'f32'); put('C', new Float32Array(d.C), 'f32'); put('UV', new Float32Array(d.UV), 'f32'); put('M', new Float32Array(d.M), 'f32');
    put('BA', Uint8Array.from(d.BA), 'u8'); put('BB', Uint8Array.from(d.BB), 'u8'); put('W', new Float32Array(d.W), 'f32'); put('I', Uint32Array.from(d.I), 'u32');
    const cm = clips.map(c => { const k = 'clip_' + c.name; put(k, new Float32Array(c.data), 'f32'); return { name: c.name, fps: c.fps, frames: c.frames, loop: c.loop, hit: c.hit, speed: c.speed, T: c.T, blob: k }; });
    const names_ = Object.keys(rig.names).sort((a, b) => rig.names[a] - rig.names[b]);
    const fx = {}; for (const k in (fig.fx || {})) fx[k] = fig.fx[k];
    const hdr = { key, name, hero: isHero, lantern: lan, nv, ni: d.I.length, nb: rig.n, bones: names_, parent: Array.from(rig.parent), head: Array.from(rig.head), abs: Array.from(rig.abs), fx, hideB: Array.from(fig.hideB || []), scale: def.scale || 1, radius: def.radius || 0.4, hpY: def.hpY || 1.8, float: !!def.float, death: def.death || 'fall', disCol: def.disCol || null, clips: cm, blobs: meta, P: (fig.P && JSON.parse(JSON.stringify(fig.P))) || null, height: fig.height };
    const hj = Buffer.from(JSON.stringify(hdr)); const lenb = Buffer.alloc(4); lenb.writeUInt32LE(hj.length);
    const pad = (4 - hj.length % 4) % 4; const lenp = Buffer.alloc(4); lenp.writeUInt32LE(hj.length + pad);
    fs.writeFileSync(path.join(OUT, key + '.lfraw'), Buffer.concat([lenp, hj, Buffer.alloc(pad), ...blobs]));
    report[key] = { nv, ni: d.I.length / 3, bones: rig.n, clips: cm.map(c => c.name + ':' + c.frames) };
    console.log(key, JSON.stringify(report[key]));
  }
}
fs.writeFileSync(path.join(OUT, 'report.json'), JSON.stringify(report, null, 1));
