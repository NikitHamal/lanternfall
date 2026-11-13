# LANTERNFALL / EMBERVALE - living TODO + stats (update every session)

Vision: a proper fantasy immersive third-person game (Athran-style kingdom defence + exploration). Explore mode is THE game. One major pillar per session.

## Shipped this session (v2.2.0 - "Living Bodies")
- [x] FIX double bow: weapons baked into hero meshes (bones wL/wR/str/arw) are stripped via a second index buffer in explorer (`gFigNoBaked`); only the socketed bow is drawn
- [x] FIX FPV aim: outlined reticle (dot + ticks; bow tightens with draw, gold at full draw; melee gets a bracket reticle) readable on grass, sky and stone
- [x] Procedural personality layer (`src/xanim.inc`): villagers hammer / rake while working (08:00-17:30), gesture + nod while talking, look around, stretch, fold arms, drink; guards stand at attention; 3 s cross-fades; zero allocation, cached bone lookups
- [x] Player air layer: jump tuck, fall spread, landing crouch with hip drop
- [x] Debug: `--xcmd side` (profile camera), `--xcmd fxN` (force activity N on NPCs + player), `talkclose`
- [x] Local sysroot + Xvfb capture pipeline (see LANTERNFALL_STATUS.md v2.2.0)

## Shipped v3.0.0 - "Solid Realm"
- [x] FIX smooth pale terrain ramps (shader rock window now matches the CPU bake); debug hooks LF_TDBG / LF_ONLYK / LF_SKIPK / LF_NEAR
- [x] FIX walk-through gatehouse corners + invisible walls at barn/chapel ends: exact oriented-box colliders; LF_COLAUDIT mesh-vs-collider audit
- [x] verified: citizens are real clothed humans, people/animals/monsters are solid, lanterns hang from hooks/belts (night shot), app heap peak 104 MB
- [ ] next: box colliders for the remaining procedural kinds (mill, tent, ore) + derive colliders from mesh footprint automatically; chunk streaming; same-tree distance sweep for LOD parity

## Next up (ordered)
1. [ ] Walk/run/sprint polish: slope foot IK (raycast per foot), turn-in-place, start/stop steps, strafe + backpedal sets, crouch + sneak
2. [ ] Melee: full-body attack arcs (weight shift, foot slide), directional block, stagger types, finishers
3. [ ] Bow: body-aware draw pose (left arm extends, right elbow back), quiver reach, arrow nock animation in TPV
4. [ ] Remove dead Duel / Steward / Range / Waves code (game.cpp, render.cpp, play.cpp are only reachable from hero select)
5. [ ] Chunk streaming + bigger map, snow / desert / swamp biomes, caves
6. [ ] Blender rebuild of Polyfork assets (needs a Blender-capable notebook): new citizens, children, pets, trees, buildings with baked AO
7. [ ] Tavern / smithy / shop interiors, sit / sleep / cook interactions, loot containers
8. [ ] Castle upgrade tiers, siege engines, persist built fortifications + warband
9. [ ] Dialogue trees, escort / delivery quests, merchant stock rotation, animal species sounds, procedural exploration music

## Stats
| version | C++ lines (dense) | assets | binary | notes |
|---|---|---|---|---|
| v2.1.0 | ~7.2 k | 26 MB | ~1.7 MB | schedules, talk, merchants, quests, save |
| v2.2.0 | ~7.3 k + xanim.inc | 26 MB | ~1.7 MB | double-bow fix, reticle, NPC activity anims, air layer |

## v2.3.0 session (Realm Polish I) - env notes
- molab notebook ALIVE this session (20 cores, root, Debian trixie). Workflow: edit locally -> push -> molab `git pull` + `cmake --build -j20` -> Xvfb capture (JPEG pulled back as base64). Scripts recreated at /tmp/agentA_*.sh.
- User feedback (issue #1 + screenshots): trees/leaves/grass/flowers poor; animals unrealistic + flee (cannot approach / mount); water/rivers/ponds/waterfalls poor; terrain too small & flat; guards float, wall walkways unreachable (no stairs / gates).
- Plan: (1) calm animals + mount, (2) wall walkways + stairs + guards on real wall-top height, (3) terrain scale/relief, (4) flora + water art.

## v2.3.0 (Realm Polish I) - shipped
- [x] natural colour grade + grass palette / tufts / dry patches, terrain mottling + dirt patches
- [x] stair landing bridges onto wall walks (all 8 logged stairs verified headless + in-engine)
- [x] waterfall streak shader, pool foam / mist tuning, wider falls
- [x] debug placement commands: fall0..5, horse / cow / sheep
## Next session candidates (Realm Polish II)
1. [ ] Trees: replace faceted blobs by layered alpha-clustered leaf cards with translucency + proper trunk flare (K_OAK / K_CHERRY / K_PINE), find and fix the salmon-red branched tree kind
2. [ ] Water: pool colour (less icy), river banks (wet mud strip, foam at rocks), flow-mapped normals, shoreline depth fade, mist billboards
3. [ ] Rock / cliff: triplanar texture instead of derivative bump, scree + boulders at the foot of slopes
4. [ ] Verify animals in-engine (stand next to a horse / cow, approach, mount) with a dedicated camera spot away from walls; add animal species sounds
5. [ ] NPC guard patrols: check posts type 1 stand exactly on deck height and can use stairs (path via decks) - add NPC stair pathing
6. [ ] Terrain: chunk streaming + 4 km map, snow / desert / swamp biomes

## v2.4.0 (Realm Polish II - "The Living Canopy") - shipped
- [x] Procedural branching trees with alpha-tested leaf / fir-spray cards (oak, pine, young pine, birch, cherry, jungle, willow, palm), leaf-shaped shadows, alpha-to-coverage, LOD proxies per species
- [x] Water: capped Fresnel + tinted reflections + deeper colours
- [x] Debug: `--xcmd tr_<kind>[dN][hi]`, `LF_MESHSTAT=1`
## Next session candidates (Realm Polish III - pick ONE major pillar)
1. [ ] **Terrain**: bigger map (4 km, 3 m grid or chunk streaming), taller mountain massifs with cliffs / scree, plateaus, valleys, coast + beaches, snow line; triplanar rock texture
2. [ ] **Ground cover**: grass blade LOD + clumps, ferns / bracken cards (use `leafCard`), flower meadows by biome, fallen leaves, undergrowth under trees, wind sway on cards (vertex shader)
3. [ ] **Animals**: realistic horse / deer / cow / sheep bodies (blocked on a Blender notebook; else procedural quadruped skinning), calm approach, mount + dismount animation, hit reactions
4. [ ] **Walls / gates / stairs audit**: every settlement wall gets a gate and 2+ stairs, guards navigate stairs (NPC stair pathing), verify guards at deck height (`--xcmd` camera spot beside a tower, not inside the wall)
5. [ ] **Rivers / waterfalls**: bank mud strip, foam at rocks, flow-mapped normals, depth fade, pool mist
6. [ ] **Buildings**: roof / wall materials, interiors, villages with lanes and fields
7. [ ] Bark texture + trunk knots, tree falling when chopped, seasonal tint per biome
## Long-term vision checklist (Athran-grade fantasy kingdom game; hundreds of sessions)
- World: biomes (temperate, jungle, snow, desert, swamp, volcanic), lost kingdoms, caves, dungeons, day / night + weather
- Kingdom: build / upgrade castle tiers, workers with jobs, siege engines, persistent base
- Combat: melee, bow, spells, mounts, troop commands, bosses, demon hordes at night
- People: schedules, crafting, quests, dialogue, factions, crime
- Tech: chunk streaming, GPU culling, real-GPU profiling, audio mix, save / load for everything

## v2.5.0 (Realm Polish III - "The Great Continent") - shipped
- [x] 4 km world, taller/wider mountain ranges, hills, 12 more lakes, up to 15 rivers, 9 falls, 72 settlements
- [x] Water mesh flattened at banks (killed the ice-wall shorelines), falls pool levels, all falls rendered, calmer water colour
- [x] Sand only on gentle low ground, no 100 m needle spires
## Next session candidates (Realm Polish IV - pick ONE)
1. [ ] **Streaming + memory**: chunk the 4 km height / info / water textures (free the CPU copies, ~100 MB), bigger world or second continent, LOD tiles
2. [ ] **Rivers II**: bank mud strip, rocks + foam, flow-map normals, depth fade, remaining fall-lip slivers, bridges for the new rivers, river fords
3. [ ] **Settlement audit**: every wall gets 2 gates + 2 stairs, patrols path via stairs, guards verified at deck height (needs camera spot beside towers), roads between all 72 settlements
4. [ ] **Animals II**: deer / wolves / rabbits / fish, mount + dismount animation, Blender-quality bodies
5. [ ] **Biomes**: desert, swamp, tundra with unique flora + ground; weather; caves
6. [ ] Triplanar rock + cliff scree, boulders at slope feet

## v2.6.0 (Living Fauna) - shipped
- [x] Loft-based anatomy meshes for horse / cow / sheep / dog / cat / deer / rabbit / hen (+ saddle) with fur shader, countershading, patches
- [x] Gait rig: walk / trot / gallop with planted feet, 2-bone leg IK, graze / lie / look / alert animation layers
- [x] Calm approach, wild deer + rabbits, milk cows, mount hop + dismount hop, saddled ridden horse
## Next session candidates (Realm Polish V - pick ONE)
1. [ ] **Ground cover**: grass blade LOD + clumps, flower meadows per biome, ferns / bracken cards, fallen leaves, undergrowth (user: "grasses and flowers poor")
2. [ ] **Rivers II / water**: bank mud strip, rocks + foam, flow-map normals, depth fade, fall-lip slivers
3. [ ] **Settlement audit**: guards on deck height, NPC stair pathing, 2 gates + 2 stairs per wall, patrols using stairs
4. [ ] **Animals II**: wolves / foxes / bears (hostile + hunting), animal sounds, herd flocking, first-person horse neck, reins, dismount at gallop, rider pose retarget (thigh/shin)
5. [ ] **Terrain**: chunk streaming, biomes (desert / swamp / tundra), triplanar rock + scree
6. [ ] **People**: citizen variety, children, Blender rebuild of characters (needs a Blender notebook)
## Stats
| version | assets | binary | notes |
|---|---|---|---|
| v2.5.0 | 26 MB | 2.06 MB | 4 km continent, water fixes |
| v2.6.0 | 26 MB | ~2.5 MB (unstripped local build) | 23 procedural animal part meshes, gait rig, fur shader; 1310 animals |

## v2.7.0 (Lean Realm) - shipped
- [x] GPU: R16 height, RGBA16 water, R11F HDR targets, legacy targets suspended in Explorer (~-265 MB @1080p q2)
- [x] CPU: worldgen scratch freed, Wq/roadQ/ecoF runtime, instance ids + lazy staging (~-215 MB steady, -100 MB peak)
- [x] Water slab artefacts at waterfalls removed, fall curtain edge fade
## Next (memory + graphics round 2)
1. [ ] Ravenfall-style casc falls: dedicated plunge water mode instead of the clipmap surface; foam ring + flow-map normals on river banks
2. [ ] Hh int16 on the CPU, BC-compress infoA/infoB (or 1024^2 + detail noise), terrain chunk streaming
3. [ ] Sunset/night sweep with R11F (banding check), GPU frame-time on a real card

## v2.9.0 (Clear Horizon) - shipped
- [x] App-heap accounting (`LF_MEM=1`), peak app heap 179 -> 104 MB, steady 44.6 MB; ground textures baked straight to the GPU in strips
- [x] Per-pixel roads (R8 distance texture), forest floor on wooded slopes, no snow under woods
## Next session candidates
1. [ ] **Find the pale ramp** at probe (-784.3, 461.3) (see status, OPEN DEFECT) - add a render-pass bisect (`LF_SKIP=props,grass,figs,terrain`) and fix
2. [ ] True far/near parity test: camera orbit command with a real distance parameter; compare tree LOD swap (125/560 m) and terrain rock band-limit
3. [ ] Collision audit with a scripted walk-into test (NPCs, animals, houses, walls)
4. [ ] Hh as int16 on CPU, mesh CPU copies freed after upload, BC-compress infoA/infoB
5. [ ] Chunk streaming for a bigger map

## v3.1.0 (True Distance) - shipped
- [x] Dithered near/proxy LOD cross-fade for trees + rocks; cliff cracks only on cliffs; `LF_LODSCALE`, `tr_*eye`, staging-overflow report
- [x] Collision / citizens / lanterns re-audited (see status); app heap peak 109 MB, steady ~48 MB
## Next session candidates
1. [ ] Controlled A/B of proxy vs near tree colour/silhouette at 150-300 m (camera on a ray, fixed exposure) and re-tint proxies
2. [ ] Remove the thin gully lines baked into far hillsides (height-field upstream river channels): fade channel depth at source or widen/soften
3. [ ] Free Hh earlier (Hq strips during bake): peak heap 109 -> ~100 MB
4. [ ] Chunk streaming + free-model (CC0) citizen/NPC rebuild when a Blender notebook exists

## v3.2.0 - "No More Pits"
- [x] FIX river carve dug 70 m vertical pits around ponds/rivers (bed clamp + no carve in lakes); waterfalls 9 -> 13
- [x] grass: shorter on sparse ground, fuller blades
- [ ] next: audit all rivers for remaining over-steep banks (add a worldgen steepness census), water-sheet at fall pools, debug cameras that spawn inside water/trees, snow bake colour, chunk streaming

## v3.3.0 - "Stone & Leaf" (graphics refinement) - shipped
- [x] Shrubs (K_FBUSH) and ferns (K_FERN) rebuilt from alpha-tested leaf cards (`buildShrubN`, `buildFernN` in xtree.inc): leafy volumes with real blossom clusters instead of one faceted blob with hexagon flowers / flat neon blades
- [x] Dressed-stone masonry (31 cm courses, staggered joints, per-block tint) in the stone surface shader; mortar + fine grain fade with pixel footprint (`fwidth(rp)`), which also removed the black speckle on far walls
- [x] Cliff shader: jointed bedrock (wavy bedding planes + staggered vertical joints) instead of iso-contour "topographic" lines; grey-brown stone even on grass-green baked slopes; moss only in ledges; cracks only where rockK > .62
- [x] Grass: 8 thinner blades per clump (was 5 wide), calmer straw / dead-blade colours, darker roots
- [x] Verified: collision self-test `--xcmd bump0..6` (NPC, animal, cottage, barn, chapel, wall: all hold; gate-tower "FAIL" is the test probing with a 0.2 m radius, player is wider), lanterns hang from hooks/braziers only (night shots), app C++ heap steady 45-48 MB / peak 109.4 MB
## Next session candidates (pick ONE major pillar)
1. [ ] **Kingdom defence loop (Athran-style)**: nightly demon hordes at the capital, tower / wall building with persisted fortifications, troop commands, spells - the gameplay pillar the user wants most
2. [ ] Chunk streaming + LOD tiles; build Hq (u16) during bake so Hh (16 MB) frees earlier (-8 MB peak); compress infoA/infoB (BC) and pack water level/flow (-17 MB GPU)
3. [ ] Citizens: second body kit (children, elders, armour variants) + face detail; real free CC0 models need a Blender-capable machine (none available in the sandbox)
4. [ ] Water: dedicated waterfall-water mode at the lip (Ravenfall), slab artefacts at fall pools, camera inside water in debug placements
5. [ ] Tree canopy seen from inside (leaf cards become huge flat quads when the camera is in the crown): fade cards by camera distance
6. [ ] Foot IK, turn-in-place, crouch/sneak, directional block

| version | binary | assets | app heap steady / peak | notes |
|---|---|---|---|---|
| v3.2.0 | 2.6 MB | 26 MB | 45-49 / 109 MB | rivers without pits |
| v3.3.0 | 2.66 MB | 26 MB | 45-48 / 109 MB | masonry, bedrock, leaf-card shrubs/ferns, denser grass |
