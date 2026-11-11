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
- [x] audit
- [ ] engine/render
- [ ] sim
- [ ] ui/audio
- [ ] CI release
