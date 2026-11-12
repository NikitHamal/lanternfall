# LANTERNFALL / EMBERVALE - living TODO + stats (update every session)

Vision: a proper fantasy immersive third-person game (Athran-style kingdom defence + exploration). Explore mode is THE game. One major pillar per session.

## Shipped this session (v2.2.0 - "Living Bodies")
- [x] FIX double bow: weapons baked into hero meshes (bones wL/wR/str/arw) are stripped via a second index buffer in explorer (`gFigNoBaked`); only the socketed bow is drawn
- [x] FIX FPV aim: outlined reticle (dot + ticks; bow tightens with draw, gold at full draw; melee gets a bracket reticle) readable on grass, sky and stone
- [x] Procedural personality layer (`src/xanim.inc`): villagers hammer / rake while working (08:00-17:30), gesture + nod while talking, look around, stretch, fold arms, drink; guards stand at attention; 3 s cross-fades; zero allocation, cached bone lookups
- [x] Player air layer: jump tuck, fall spread, landing crouch with hip drop
- [x] Debug: `--xcmd side` (profile camera), `--xcmd fxN` (force activity N on NPCs + player), `talkclose`
- [x] Local sysroot + Xvfb capture pipeline (see LANTERNFALL_STATUS.md v2.2.0)

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
