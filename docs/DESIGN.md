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

Named places for `world_place` (the story uses these names; add more if useful): `lab_door`, `lab_back_door`, `lab_lot` (the lot behind), `nile`, `proof`, `chiwas`, `worth`, `burritoholics`, `gus`, `oro`, `chupacabra`, `main_burger`, `az_distilling`, `phantom_fox`, `que_chevere`, `espiritu`, `haunted`, `against_grain`, `organic_bean`, `casa_copal`, `diamond_dons`, `level1`, `mad_candy`, `mangos`, `margaritas`, `lost_dutchman`, `one_macdonald`, `macdonald_main` (corner), `center_main_station`, `city_plaza`, `arts_center`, `mid_block` (an open stretch of Main in front of the lab good for a street fight), `lot_fight` (an open bit of parking lot). Republica Empanada, Hope's Frybread, Pizza Mart and The Guadalupe on Main are off the mapped stretch and have no place (docs/PLACES.md).

Cars are the game's own, built in `src/facades_keyholder.inc` (`fk_car`): rounded shells of rings along the length, three bodies (a sedan with a trunk, a box tail for the hatchback, SUV and minivan, a pickup with its cab and bed) and three glass cabins, scaled per kind by the `CAR_K` table. `car_measure` seats the cabin on the body, and the same numbers give the far stand in (`far_car` in `city.c`) and the collision box (`car_extent`), so a kind keeps its outline at every distance. Tyres sit just inside the body side. Parked cars face the way the lane beside them runs (Main: west on the north curb, east on the south), and `tools/prep-city.mjs` keeps each lot car square in its own stall, clear of other aisles' paint and of other cars. A KH_TEST_WORLD build with `-DKH_CAR_SHOWROOM` lines the five kinds up, near and far, in the clear lot at `lot_fight`.

Nothing of the town is placed inside the lab: `inside_building()` in `city.c` counts the lab as a building, and hand placed pieces in `facades_town.inc` stay outside its walls (its back wall is world z -40.41). From behind, in sight of its open back exits (the alley, or across the back lot up to about 90 m, within about 70 degrees of straight on), the lab draws through them (`at_backdoor` in `world.c`), so the town needs no backdrop there. Bands past 6.5 m draw their far twins: about 55k triangles from the alley, under what the storefront view already draws (300k to 330k in all).

## Follow camera, doors and the light rail

The roam camera (`player.c`) rises over the head when the arm is boxed in. Indoors it rises to the angle that gives the longest arm, the way a crowded room is shot from above. Outdoors only the ground gained behind the player counts and it stops at about 45 degrees: against a shopfront a steeper arm is longer only because it climbs the wall, and it looked straight down on the top of the head. Outdoors the lens also stays 0.2 m further off walls than the probe alone would, because shopfront door frames and glass stand up to 0.3 m proud of the wall the probe sees. A room's outside door point (`interior_door`) is 1.8 m out from its front, on the facade's own drawn door, so someone coming out has room for the camera behind them. The light rail stops lenses and lines of sight while it runs (`city_raycast`). It waits out of sight during a fight in town (`world_train_hold`), because the bike fight brings its own train as a hazard. A parked bike (`bike.c`) leans on its kickstand; while it is ridden or walked, the stand is up.

## Memory

The station is a 1080p box with modest RAM, and wasm memory only grows. Every body is parsed at boot (`actor_preload_all`), so a template keeps its two atlases as the glb's PNGs (about 0.15 MB each, not 4 MB decoded): the body atlas is decoded for each new palette, the face once by the model's first actor, and a texture drops its CPU copy once the GPU has it. At each night's start the story frees the last part's cast (`actor_free`, weapons first) and the night makes again whom it needs. A full KH_AUTO run peaks near 365 MB (it was 745 MB). The voice bank (`voices.bin`, about 46 MB) is read whole at boot; the town's and the lab's meshes are most of the rest.

## Combat (see fight.h)

Action game in the PS2 mould: third person, lock on, light combo, heavy, dodge with i-frames, guard with a parry window, a special meter that fills on hits and parries and fires a cinematic finisher. Mocap clips (`tools/prep-vrm.mjs` CLIPS: `Sword_Regular_A/B/C` with `_Rec`, `Sword_Regular_Combo`, `Sword_Heavy_Combo`, `Sword_Dash`, `Sword_Block`, `Sword_Idle`, `Roll`, `Hit_*`, `Death01`, `LayToIdle`, `Spell_Simple_*`, `Shield_*`, `Punch_*`, `Melee_Hook`, `OverhandThrow`, `NinjaJump_*`, `Slide_*`). Hit stop, screen shake, swoosh trails, sparks, slow motion on the last hit. Five weapons, see `fight.h`. Enemy kinds in `ai_kind`. Fighters are actors with palette swaps. A beaten fighter drops to one knee (`Death01` or a held pose) and stays there, alive, for the story to take over.

## The day (lead)

Chapters run on the lab clock from 9 am to midnight: open up, the first visitor, lunch run along Main, afternoon incidents, Arduino Night crowd, closing. Choices move four numbers (Trust, Safety, Vibe, Dues) and affinity with the cast, and decide which fights happen and how hard they are. Facts follow the HeatSync bylaws and handbook (the `heatsync` skill) and DOLLY's tour script.

## v1.1 polish: owners and contracts

The contracts are already in the headers (`fight.h`, `director.h`, `audio.h`, `input.h`, `world.h`, `script.h`) and in `tools/script/dsl.mjs` and `compile.mjs`. Read them before you start. Additive header changes are fine if you report them. Do not edit a file you do not own; ask the lead.

| Owner | Files |
|---|---|
| writer | `tools/script/day.mjs`, `tools/script/cast.mjs` (names, voices, palettes, models) |
| story runtime | `src/story.c`, `src/story.h`, `src/hud_vn.c` (all but `hud_title_draw`), hosting tasks and their minigames, banter, barks, extras and ambient life in the lab and on Main, actor pathing, the named lab places, fight glue (twists, barks, assists, checkpoints into `fight_def`), Battle Zone unlock calls |
| director | `src/director.c`, `src/director.h` |
| combat | `src/fight.c`, `src/fight_int.h`, `src/fight.h`, `src/weapon.c`, `src/weapon.h`, `src/player.c`, `src/fx.c`, `src/fx.h`, `src/hud_fight.c`, `src/test_fight.c`, the sfx list in `src/audio.c` (coordinate with audio) |
| battle | new `src/battle.c`, `src/battle.h`, `src/hud_battle.c`, `src/game.c`, `hud_title_draw` in `src/hud_vn.c` |
| audio | `src/audio.c` (music and sfx voices; the sfx enum is shared with combat) |
| world | `src/world.c`, `src/city.c`, `src/city.h`, `src/arena.c`, `src/arena.h`, `src/facades_*.inc`, `tools/prep-lab.mjs`, `tools/prep-city.mjs`, `tools/prep-arena.mjs`, `assets/models/{lab,city,arena}*` |
| models | `tools/prep-vrm.mjs` (and any new prep tool for non VRM rigs), `src/actor.c`, `src/actor.h`, `src/anim.c`, `source/vrm`, `assets/models/char_*`, `CREDITS` |
| lead | `tools/script/compile.mjs`, `tools/script/dsl.mjs`, `src/script.h`, `src/input.c`, `src/main.c`, `docs/`, `HANDOFF.md`, voices, shipping |

### Private builds of the script

`src/gen/script.c` is shared. To try a script without touching it: `DAY=path/to/day.mjs GEN=build-x/gen node tools/script/compile.mjs`, then build with `KH_SRC="... ../build-x/gen/script.c" EXTRA_CFLAGS="-DKH_GEN_ENUM='\"../build-x/gen/script_enum.h\"'"`. Only the writer and the lead write `src/gen/script.c`.

### Hosting (story runtime implements, writer uses)

`ROAM(place, radius, prompt, { hour, until, sides, tasks, need, banter })`, `TASK(kind, name, o)`, `BT(who, text, o)` and `BARK(who, text)`: see `dsl.mjs`. Task kinds:
- `greet`: a walk in at the door looks around; reach them and press while they look for someone (a shrinking window). A quick look up gives the reward; a slow one gives less.
- `point`: tour stops. The visitor follows; at the station, aim at it and press.
- `answer`: a visitor asks something; `before` beats hold a CHOICE.
- `carry`: pick up at `at`, put down at `to`, `n` trips (tools to the pegboard, soda to the fridge).
- `fix`: a minigame at `at` by `mode`: `timing` (press in the zone: printer unjam), `mash`, `hold` (the laser hood), `stick` (turn a knob).
- `signin`: a short button sequence at the sign in board with a visitor.
- `errand`: go there and press (Main Street).

Rewards are at most two stats. Optional tasks show in the task list in grey. Named lab places the story runtime provides (plan coordinates in `story.c`): `lab_welcome lab_laser lab_kitchen lab_cowork lab_shop lab_printer lab_laser_hood lab_soda lab_pegboard lab_signin lab_toolwall lab_elec lab_lounge lab_fab lab_scrap lab_couch lab_classroom` (it may add more and report them).

### Fights (combat implements, story runtime fills, writer authors)

In the FIGHTS table in `day.mjs`: `twist` (none tutorial timer retrieve score hold defend protect boss), `twist_a`, `twist_b`, `obj: [x, z]` (relative to the fight's centre), `obj_label`, `protect: 'sam'`, `barks: [{ on, who, text }]` (on: start phase2 foe_low player_low assist obj_warn hazard finish parry), `assists: [{ who, flag, trust }]` (callable when the flag is set or Trust is at least `trust`), `phase2` (hp fraction), `qte`, `tier` (1 to 5, the Battle Zone ladder). Barks are voiced: the compiler adds them to `lines.json`. `fight_phase()` gives the checkpoint for a retry; `fight_result_get()` gives the score.

### Film language

New shot types `push react insert hero whip top`; `SHOT(type, subjects, { part, at, dur })`. The director re-solves while a shot runs, so a close up never ends in hair or a face when someone moves. `director_whip()` is how strongly a whip is happening, for speed lines.

### Music

Moods `title explore hosting tension cine fight boss final night zone clear silent`. `explore` is the street by day, `hosting` the lab by day, `final` the last boss.

### Battle Zone

A replayable ladder from the title, unlocked as the day is played. Progress is kept in the last 500 bytes of `DS_SAVE.data` (bytes 520 to 1019), which the day's save never touches. `battle.h`: `void battle_unlock_fight(int fi); void battle_unlock_weapon(int w);` (story runtime calls them when a fight is won or a weapon is given).
