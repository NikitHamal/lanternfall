# LANTERNFALL native port - status & architecture decisions

Reference: https://github.com/loktar00/lanternfall (single-page three.js idle battler, ~350 KB of JS + 2.5 MB embedded music).

## Audit summary (what the original contains)
- `00-util` hash/noise/mulberry, road `pathZ`, terrain `terrH` (periodic over WORLD_L=4096)
- `30-world` procedural ground shader (road/mud/heath/water/pebbles), instanced grass (8600 clumps, wind), sky shader (clouds, stars, moon),
  prop generators (rock, standing stone, tree, wall, cairn, fence, waylight), 5 "reaches" (palettes, fog, props), moon+hemi+fill+lantern point light
- `40-post` HDR scene -> AO + exp-height fog + analytic point-light in-scatter -> 4-level bloom -> DOF + AgX tonemap + colour grade + grain
- `50-fx` particles (embers/sparks/dust/chips/frost/flash lights), ground marks (ring/tele/fire/frost), projectiles, floating text, loot
- `60-data` 6 hero classes x 3 skills, 9 monsters (3 bosses), cards, camp upgrades, boons (meta), gear grades
- `61-game` fixed 60 Hz sim: walk/fight/boss/breather/relight/choice/wipe state machine, hero & monster AI, skills, flare, steward auto-play, offline
- `90-main` spring camera (fov 30), shake, hit-stop, slow-mo, input (Enter/Space/M/C/+/-/click focus & rally)
- `70-ui`, `79-sfx`, `80-audio`, `78-music` DOM HUD, WebAudio synth SFX, embedded music

## Native architecture (C++17, OpenGL 3.3 core, GLFW only)
- own GL loader (`gl.h`, via glfwGetProcAddress), own math (`mathx.h`), own 5x7 bitmap font; miniaudio (single header, vendored) for audio
- everything GPU-instanced (`glDrawElementsInstanced`): props, characters (primitive parts), grass blades, particles, marks, UI quads
- zero heap allocation per frame: fixed pools & preallocated vectors
- shaders embedded in the executable (`shaders.h`) -> single portable binary
- post chain ported 1:1 from `40-post.js` (HDR RGBA16F, MSAA scene, AO+fog+in-scatter, 4 bloom mips, DOF, AgX, grade, grain)

## Progress
- [x] audit of reference (all src/*.js except embedded music data)
- [x] GL loader, math, data tables, simulation (game.cpp), fx, procedural audio, meshes, renderer + post chain, HUD
- [x] local build + xvfb smoke test (title + gameplay frames render, level-up panel works)
- [x] release workflow (Windows / Linux / macOS universal) -> GitHub Release v1.0.0
- [x] v1.1 character overhaul (below)
- [ ] follow-ups: point-light shadows, procedural music, offline progress, DOM-faithful HUD layout (panels/cards), portraits

## Decisions
- No GLAD/GLM/stb: own loader & math keep the binary ~0.8 MB. Font is a built-in 5x7 bitmap atlas.
- (v1, superseded) characters were instanced primitives. v1.1 uses real skinned meshes, see below.
- Steward panels use real-time countdowns (cards 6 s, choice 7 s, run end 8 s) exactly like the original.

## v1.1 - Blender character pipeline (this task)
Goal: characters identical to the three.js reference (`_ref_js/src/10-kit..22-monsters, 25-anim`), not approximations.
Pipeline (all scripts in `tools/`, reproducible on the molab notebook: Blender 4.3.2 + node):
1. `tools/bake.js <ref> <out>` runs the ORIGINAL generators (Geo/loft/tube kit, HW human, wardrobe, 6 heroes, 9 monsters, rigs) headless in node and dumps the exact
   skinned meshes (P,N,C,UV,M,BA,BB,W,I), rigs, attachment points, and **bakes the reference animation**: HumanAnim/QuadAnim/WispAnim including the two-bone leg IK with
   foot pinning, spring cloak and lantern pendulum -> world-space bone pose (quat+pos) per frame: idle, ready, walk@1.6, run@3.2, every action clip, stagger, downed.
2. `tools/blender_build.py` (one Blender process per figure, 9 in parallel on 20 cores, ~3-20 s each): builds a real mesh datablock with custom normals, UV, colour/material
   attributes, a real armature (28/21/9 bones, hierarchy from the original rig) and vertex groups from the skin weights; bakes per-vertex ambient occlusion (24 BVH rays/vertex
   incl. ground contact) and cavity (edge curvature); poses the armature from the baked frames and renders Cycles previews (`docs/previews/blender_*.jpg`); saves `.blend`;
   reads everything back out of Blender and writes the runtime asset `assets/<name>.lfm` (36 B/vertex, u16 indices, int16 quantised clips).
3. Native: `src/figure.cpp` loads `.lfm`, GPU skinning (two-bone blend like `Figure.skin()`), baked clip player (idle/ready/locomotion cadence follows real speed, action
   clips driven by the sim's windup/recover, stagger, downed, shatter rigid-body death + dissolve, burn/burst/gutter deaths), and the ported **surface material shader**
   (`SURF_FMAIN`/`SURF_FNORMAL`: skin, cloth weave, quilt, mail rings, leather, rusted plate, fur, wood, lichen stone, peat, glow, bone; derivative bump, mud, dissolve).
   Back-lantern geometry is stored once as an index range per hero (drawn only for the carrier).
4. UI: serif glyph atlas (`assets/font.bin`, Liberation Serif Bold rendered with PIL) replaces the 5x7 bitmap font (original uses Georgia).

Metrics: 15 figures, 124 k vertices, 7.2 MB assets (not embedded; `assets/` must sit next to the executable, searched in `./assets`, `../assets`, `../Resources/assets`); executable ~0.9 MB.
Caveats: no GPU in the sandbox - frames captured with Mesa llvmpipe under Xvfb (speckled terrain in captures is software-rasteriser derivative noise from the v1 ground shader, to be checked on hardware).
Where Blender "customisation" differs from the reference: AO + cavity baked into vertices (softened 0.4..1) and applied in the surface shader; everything else is 1:1.
