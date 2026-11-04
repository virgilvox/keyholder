# KEYHOLDER design

A PS2-style action visual novel for the HeatSync Labs game station (wasmcart, Batocera, a PC) and the browser. You are a member of HeatSync Labs with the keyholder shift for one whole day, 9 am to midnight, at 108 West Main in downtown Mesa. Dialogue plays as cutscenes with choices, cinematic camera angles and voice acting (Kokoro TTS, pregenerated). Some situations escalate, and then it is an anime fight: light swords and fantasy weapons, on Main Street, in the lab, or in an arena based on the lab that opens up around you. In between you walk freely through the lab and a walkable piece of downtown Mesa at real scale.

Tone: a warm, dry, affectionate joke about running a volunteer hackerspace, played completely straight as a shonen anime. No gore; a beaten fighter sits down winded and then talks. No romance. Fictional cast; no real people; real downtown businesses appear only as neutral scenery.

## Rules for every source file and string

- C11 in the style of the surrounding code. No emdashes, no double dashes used as punctuation, no emoji, in code, comments or strings. Plain writing, no AI phrasing.
- No Claude or AI credit anywhere (commits, docs, comments).
- Parallel work: build with your own tree and only the sources you need, so another module mid change cannot break you: `OUT=build-x KH_SRC="main.c present.c anim.c actor.c audio.c voice.c input.c batch.c yours.c" ./build.sh` (the web page is only made for the default tree).
- `EXTRA_CFLAGS=-DKH_TEST_x` plus a `src/test_x.c` wrapped in `#ifdef KH_TEST_x` that defines `game_init`, `game_frame`, `game_draw`, with `game.c` left out of KH_SRC, is how a module runs alone. Keep test files out of the final game by the ifdef.
- Headless screenshots: `node tools/shot.mjs build-x/keyholder.wasc build-x/shot 60,300 [script.json]` (read the top of the file for the script format; `TIMING=1`). LOOK at the PNGs with the Read tool; compiling is not verifying.
- A line in the run log like `[cart] lambert+map+fog+skin+atest+ds+l1001` is a shader state missing from the program table: put such lines in a file and run `tools/regen.sh <file> [your OUT dir]` (it takes a lock; the table in `src/gen` is shared and only grows). Prefer few light counts: every state in the game uses the same lights (one hemisphere, one directional: `l1001`) unless there is a strong reason.
- Do not commit; the lead commits. Do not edit files you do not own: if you need a change in one, say so in your report, or make the smallest additive change to a header and say so.

## Scene and look

- Metres. Actors are about 1.6 m tall. yaw 0 faces +z.
- The scene renders at 800x450 (`DS_LOW_W/H`), is drawn up with bilinear filtering plus a bloom (`present_retro`: highlights above about 0.72 glow, so keep ordinary surfaces under it and let neon, blades and lamps exceed it), and the HUD is painted at 1280x720 over that. Then 1080p on the station.
- No tone mapping. Scene lights: `t3_hemisphere_light` + `t3_directional_light`, owned by `world.c` (`world_set_time` sets colours and intensities). Characters are cel lit: `tools/cel-patch.mjs` turns the lambert+skin states' direct light into two bands. Everything else should be vertex coloured or textured `lambert` or `basic`, PS2 style: simple geometry, baked colour, fog, small textures with linear filtering. Avoid standard (PBR) materials: they cost and do not look the part.
- Characters: `src/actor.h`. Five baked models, palette swaps, about 15k triangles plus hull.

## Modules and owners

| File | Owns | Owner |
|---|---|---|
| `main.c`, `present.c`, `anim.c`, `actor.c`, `input.c`, `voice.c`, `tools/prep-vrm.mjs` | base | lead |
| `game.c`, `director.c`, `story.c`, `hud*.c`, `tools/script/*`, `tools/bake_tts.py`, `audio.c` music | flow, cinematics, story, dialogue UI, voices | lead |
| `world.c`, `city*.c`, `facades*.inc`, `arena.c`, `tools/prep-lab.mjs`, `tools/prep-city.mjs`, `assets/models/{lab,city,arena}*`, `assets/signs` | the lab, downtown, arenas, collision, time of day | world |
| `fight.c`, `weapon.c`, `player.c`, `fx.c`, `hud_fight.c`, `audio.c` sfx list | combat, weapons, third person walk and camera, particles | combat |

The headers `world.h`, `fight.h`, `player.h`, `actor.h`, `input.h` are the contract. Additive changes are fine if reported.

## World (see world.h)

World axes: +x east, -z north, origin on the lab's front wall at the door, y 0 sidewalk level. Source of truth for placement: OpenStreetMap (`source/osm/mesa-overpass.json`, Overpass dump of the area; even numbers on the north side of Main, odd on the south; HeatSync Labs node 33.4153175, -111.834497; ODbL, credit it). Project with equirectangular at latitude 33.4153. Coverage: all of West Main from past Worth Takeaway (218 W) to past Center Street and Mesa City Plaza (20 E) / Mesa Arts Center's north edge (1 E), so about 900 m along x, plus about a block either way: north to about 1st Ave, south to about Pepper Pl / 1st Ave S. Cross streets from OSM (Robson, Macdonald, Center and so on), the alley behind the lab and the parking lots.

The lab (`world_lab`): HSL-108 plan x runs from the street (0) to the back (40.4 m), plan z from the west wall (0) to the east wall (9.4 m). The building fronts south on Main, so plan +x maps to world -z and plan +z maps to world +x: a proper 90 degree rotation about y. Its own storefront comes from the model (white pergola, columns), which is more faithful than a facade.

What Street View shows of West Main near the lab (Oct 2025), to build from:
- Main is wide: from the north curb going south, a sidewalk of tan and red brick pavers with black fluted lamp posts and small desert trees in planters, a parked car lane, a through lane, then the light rail: two tracks in a pale concrete bed in the middle of the street with raised low curb edges and gravel/desert planting at corners, teal green catenary poles every 40 m or so, each with a double curved lamp arm and overhead wires, then another through lane, a parked lane, the south sidewalk. Double yellow lines where cars cross.
- North side at the lab: the long white one storey block with a zigzag blue and white butterfly canopy over a tiled storefront immediately west of HeatSync; HeatSync's white column portico with timber pergola, a small "HeatSync Labs" sign on the white parapet; to its east a white trellis pergola at the corner of Macdonald; a tall steel lattice sculpture tower at that corner; across Macdonald the 4 storey beige and teal glass One Macdonald office with a colonnade; behind the lab white modern apartment blocks rise.
- South side opposite: The Nile (105 W, red brick two storey, THE NILE EST 1924 painted, copper pointed marquee, red umbrella patio), then a stucco block with a brick parapet and green awning to its west.
- 200 W corner, north side: a long white block with a green canopy and a bronze statue at the corner, a tall white fin sign above it. ORO further west.
- Center/Main station: light rail platforms in the median with timber trellis shelters, climbing plants, palms; the blue Mesa Arts Center mass to the south; palms on the Plaza to the north.
- Sky: hard desert blue. Palms in the distance.

Facades: monteslu's bikemesa (MIT, `github.com/monteslu/cartwheel-examples`, `games/bikemesa/mesa.inc`, `mesa-layout.h`, `CREDITS`, `landmark-references.json`, `assets/signs/`) builds nearly every Main Street storefront in code with the same t3 API, but all on one side of a road at made up track positions. Port it (keep its MIT notice; three.c is the same API, `cw_*` are thin wrappers), build each venue in its own group at the local origin, and place it at its real address, turned to face the street (north side faces south, south side faces north). Sign art carries credit lines into `CREDITS`. Downscale the huge sign PNGs. Anything on the blocks with no bikemesa facade becomes a generic extrusion of its OSM footprint with parapet, storefront band, windows and awnings, chosen by hash, with civic buildings given their real height and mass. Non street facing sides are plain.

Named places for `world_place` (the story uses these names; add more if useful): `lab_door`, `lab_back_door`, `lab_lot` (the lot behind), `nile`, `proof`, `chiwas`, `worth`, `burritoholics`, `gus`, `oro`, `chupacabra`, `main_burger`, `az_distilling`, `phantom_fox`, `que_chevere`, `espiritu`, `haunted`, `against_grain`, `organic_bean`, `casa_copal`, `diamond_dons`, `level1`, `mad_candy`, `mangos`, `republica`, `margaritas`, `lost_dutchman`, `hopes`, `pizza_mart`, `guadalupe`, `one_macdonald`, `macdonald_main` (corner), `center_main_station`, `city_plaza`, `arts_center`, `mid_block` (an open stretch of Main in front of the lab good for a street fight), `lot_fight` (an open bit of parking lot).

## Combat (see fight.h)

Action game in the PS2 mould: third person, lock on, light combo, heavy, dodge with i-frames, guard with a parry window, a special meter that fills on hits and parries and fires a cinematic finisher. Mocap clips (`tools/prep-vrm.mjs` CLIPS: `Sword_Regular_A/B/C` with `_Rec`, `Sword_Regular_Combo`, `Sword_Heavy_Combo`, `Sword_Dash`, `Sword_Block`, `Sword_Idle`, `Roll`, `Hit_*`, `Death01`, `LayToIdle`, `Spell_Simple_*`, `Shield_*`, `Punch_*`, `Melee_Hook`, `OverhandThrow`, `NinjaJump_*`, `Slide_*`). Hit stop, screen shake, swoosh trails, sparks, slow motion on the last hit. Five weapons, see `fight.h`. Enemy kinds in `ai_kind`. Fighters are actors with palette swaps. A beaten fighter drops to one knee (`Death01` or a held pose) and stays there, alive, for the story to take over.

## The day (lead)

Chapters run on the lab clock from 9 am to midnight: open up, the first visitor, lunch run along Main, afternoon incidents, Arduino Night crowd, closing. Choices move four numbers (Trust, Safety, Vibe, Dues) and affinity with the cast, and decide which fights happen and how hard they are. Facts follow the HeatSync bylaws and handbook (the `heatsync` skill) and DOLLY's tour script.
