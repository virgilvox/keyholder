# KEYHOLDER handoff

**Attic v2.0 (2026-11-12, in progress, not committed, not shipped):** built to docs/ATTIC.md, the design the user picked. Read that first. Verified headless on the real path with a `-DKH_ATTIC_PATH` build (a new game starts at the first lab roam at the foot of the back stairs; number keys 1/2/4/3 jump to the first clearing, the boss, a won boss, the next shelf): the stairs warning and prompt, the nightmare's arrival scene, walking and soft-board falls, the holiday fight (won), the SYSOP scene and fight, the wake into the blueprint room (SAFETY +1, MEMORIES 0/8), the Tear Apart and laser flashbacks in One Forty, and the return to the shelf with MEMORIES 1/8. Not yet seen: the other six flashbacks end to end, the terminal opened from the rack in the real build (its games were verified by their own harness), a won boss fight by real play, station frame rate.

Where things are: tools/script/attic.mjs (scenes and fights), src/story_scene.c + src/story_int.h (the story VM plays attic scenes with the day held; story.c only has hooks), src/memory_props.c (flashback props), src/present.c present_memory (the flashback grade), src/interior.c "atomic" (One Forty, a memory only room), src/attic.c (the mode) + attic_deep.c + attic_hub.c + attic_hud.c + attic_entrance.c, src/attic_env.c (the room, the deep maze and the desert as layouts of STAGE_ATTIC; tools/prep-attic*.mjs, tools/prep-memory-desert.mjs, tools/attic-kit.mjs), src/enemy.c (dressed foes; actor.h palette wash and sockets), src/terminal.c + term_*.c (four games). The attic's fights come after FI_BZ_COUNT and stay out of the Battle Zone (its save holds 16). Known: the stairs warning line is hard coded in story.c and unvoiced (from v1.4); story.c itself (2.6k lines) wants splitting into modules.

**Open problem (2026-11-12): the attic level is REJECTED by the user and needs a design rethink, not more code patches. See "v1.5 — The Attic (REJECTED)" below before touching it.**

State as of 2026-11-11: v1.5.0 is on GitHub (github.com/virgilvox/keyholder), on Pages (virgilvox.github.io/keyholder), in the v1.5.0 release and on the game station `batocerahsl` (cart md5 `45115205f79f7cf4710b17497563a318`, all four targets the same binary). v1.5.1 is an attic quality pass (cinematic memory sequences, the terminal on the rack, device-aware glyphs, a harder hack, a peaked roof, and attic_env.c split from attic.c); v1.5.0 rebuilt the attic into its own STAGE_ATTIC level + hub with a hacking terminal (see the v1.5 section below); v1.4.0 added the first floor-dodging attic minigame and the skippable orientation tutorial (hold START). The earlier game station note: on 2026-11-10 and 2026-11-11 it was off. Its address is from the pool and not reserved: on 2026-11-11 172.22.111.83 was a different machine (new SSH host key, no game API on :1234, `batocerahsl.local` silent), so find the station by `batocerahsl.local` and check the host key against known_hosts before sending the password. To update it, wait until `pidof retroarch` is empty, back up the old cart, copy the new one to `.incoming/` and `mv` it into place, check the md5, and add a row to `HeatSync Labs Operations 2026/docs/06-change-log.md`.

Read `docs/DESIGN.md` for the module contract and the art rules, and `docs/PLACES.md` for every downtown address.

## v1.5 — The Attic (REJECTED by the user — redesign needed)

**Status 2026-11-12: the attic is rejected.** It is built, deployed and technically works (the whole machine below functions headless), but the user's verdict after several passes is that it is "such a stupid level," "doesn't fit the vibe or quality of anything else," and the layout "looks stupid." Do not ship more patches on top of it. The next session should treat the *design* as the open problem, not the code quality, and should get the user's direction before building (see the kickoff brief). **Keep the plumbing, rethink the experience.** What stands as good, reusable infrastructure: STAGE_ATTIC as its own routed stage in world.c (the follow cam works, the real crawlspace mesh was correctly abandoned); the glitch→normal mood lerp; the save_t persistence fields; the rope/PVC entrance + member warning + stair barrier; attic_env.c / attic.c / terminal.c separation. **What the user keeps rejecting is the content and feel:** an arcade red-X dodge-run and a Johnny-Mnemonic node-trace hack are an action-game genre bolted onto what the rest of KEYHOLDER is (a slow, dialogue-driven PS2 anime visual novel about a day hosting the lab). They read as minigames, not as part of *this* game. The "memory sequences" and the room dressing have been iterated but still land flat. The real question for the redesign is tonal: what does going up into the attic *mean* in a narrative VN, and what would make it feel authored rather than like a bolted-on arcade mode. That is a design conversation to have with the user first, not another guess.

Everything from here down is the v1.5 build as it exists on disk and on the targets, kept for reference so the next session knows what is already wired up.

Shipped and verified headless (KH_TEST_ATTIC via tools/shot.mjs): the glitched entry, the red-X dodge run, a fall, the reboot "BACK ONLINE" with the room ramping to normal, the hub ("the attic is yours"), a memory flashback card, the pizza-box terminal booting its CRT, and the round-trip back to the lab (STAGE_TOWN). The terminal + node-trace hack (terminal.c) was verified by its own KH_TEST_TERMINAL harness. The real cart boots clean. Design below is as built; the in-lab rope/PVC sign and the roam trigger are coordinate-correct and boot clean but, like v1.4, sit in the cramped lab corner, so confirm them in live play.

v1.4's attic reused the real HSL-108 attic mesh, which is a 1.3 m crawlspace — too low, the follow camera collapsed, and the hazard "sign" was a janky board. v1.5 scraps that for a purpose-built attic.

**Why a new stage, not the real mesh.** Every non-`STAGE_TOWN` stage in world.c already gets a **flat floor at y=0** (world_move line ~740) and routes collision/raycast/mood through `arena_*`, keyed by `stage`. So the attic becomes its own stage, `STAGE_ATTIC`, a roomy flat room (~14×10 m, 3.6 m ceiling) with its own geometry, walls and lighting, and the ordinary third-person follow camera works with headroom. `arena.c` indexes its 5-element tables by `idx(s)=s-STAGE_ARENA_BENCH`, so `STAGE_ATTIC` (added after `STAGE_ARENA_UNFOLD`, before `STAGE_COUNT`) must be **routed to attic.c in world.c, never into arena.c**. Add a branch at each non-town dispatch site (world_set_stage, apply_lights, world_update, world_stage_floor, world_raycast, world_move): `if (stage==STAGE_ATTIC) attic_env_*(...) else arena_*(...)`. world.c gains `#include "attic.h"`; the env API lives there: `attic_env_show(bool)`, `attic_env_push`, `attic_env_raycast`, `attic_env_mood(arena_mood*)`, `attic_env_floor`, `attic_env_update(dt)` (returns true when the mood changed → world calls apply_lights).

**The glitch→normal beat.** `attic_env_mood` lerps between a "wrong" mood (dim, cold green/magenta cast, heavy fog, a flicker on the hemi light) at `env_normal=0` and a warm dusty-attic mood at `env_normal=1`. You enter the first time at 0; reaching the server **reboots it and the room snaps to normal** by ramping `env_normal` 0→1 (apply_lights repaints; glitch fx stop). The red Xs are **painted on the floorboards** (built once, always visible); they only *mean danger* during the dodge run.

**attic.c phase machine** (`A_PLAY` dodge run → `A_FELL`/`A_WON` → `A_HUB` free-roam → `A_MEMORY` self-contained flashback → `A_TERMINAL` the pizza-box terminal). After the first win the room is "normal" and re-entry lands straight in `A_HUB`. The player walks with the normal `player_update` (no more custom crawlspace camera, no `world_solid` fences — the walls are real geometry). Examinables are an array `{pos, r, kind, id}` (kind = memory / replay-run / terminal); nearest-within-r shows a `{USE}` prompt in attic's own HUD and fires on interact. **Memories play inside attic.c** (a card/voice/fx sequence like the A_WON banner), NOT via the story VM — handing control back to story.c would pop the player out of the attic.

**The pizza-box terminal** (`terminal.c`/`terminal.h`, a sub-phase driven by attic's `A_TERMINAL`, not a separate game.c mode): a green-CRT screen built with the hud_gl toolkit — `hg_rect` phosphor field, `hg_scan`/`hg_vignette`/`hg_vgrad`/`hg_stripes` for scanlines/bezel/roll, mono fonts `F_TYPE`/`F_MONO`, char-by-char streaming via the draw_dialogue trick (raster a line once into a layer keyed by its text, reveal a growing sub-rect with `hg_sub`), a selectable program menu (copy hud_battle draw_hub), and a node-trace **hacking minigame** (Johnny-Mnemonic style: a node grid with links drawn by the `cv` vector API since hg_* has no diagonal line; nav the cursor, `ok` claims a linked node under a per-move timer, reach the target). Re-key discipline: never fold time into a layer key; animate only with cheap hg primitives. `term_update(dt,in)` returns false to log off → back to `A_HUB`.

**Persistence** (interact-persist research): DS_SAVE.data is fully partitioned (story 0..519, battle 520..1019) — **no room for a third block**, so use story's save_t slack (480 bytes used, 40 free, assert ≤520). Append AFTER `carry_t start[NIGHTS_MAX]` (keeps `offsetof(save_t,bike)==300`): `uint8_t attic_state` (bit0 beaten, bit1 terminal unlocked), `uint16_t attic_found` (examinables fired), `uint16_t attic_games` (hacking minigames unlocked). Keep them **top-level, not in carry_t** (carry is the per-night snapshot; attic progress is global across the day). Sync in autosave/story_continue/story_new_game; migration zeroes new fields automatically. attic.c reaches them through small story.c accessors.

**Entrance.** Replace the hazard board with two stanchions + a rope across the stair mouth and a small PVC-pipe-framed sign ("DANGER — AUTHORIZED PERSONNEL ONLY"), built with fxp and hung under the town group (`world_add_prop`). The stair barrier + roam hotspot + member warning stay.

**Scope this pass:** the stage + room + glitch→normal + rope/PVC sign + the dodge run + the hub (replay, one examinable→memory) + the terminal with one node-trace hack + save. More memories/minigames are data adds. Verify headless with `KH_TEST_ATTIC` (and a `KH_TEST_TERMINAL` harness) via tools/shot.mjs.

## v1.3.4 (2026-11-11)
- Orientation tutorial: the parry lesson is optional now. Blocking a hit clears it, and if you do neither it moves on after 6 s (was a 18 s wait for a clean parry). Parry is still a bonus. The lesson reads "PARRY (OPTIONAL): TAP GUARD, OR JUST BLOCK". Verified with guard-only input through the KH_TEST_FIGHT tier 7: lesson 6 of 7 cleared without a timed parry.

## v1.3.3 (2026-11-11)
- Moheeb and Luis are on the Chupacabra patio all day and every night (CAMEO.hours [0, 30]); they load at the first roam, and the opening roam's banter says where they are.
- Checked on the real key path (KH_GEAR_TEST cart, keys B, B on the 1:05 PM roam): getting off the bike drops both arms at once.

## v1.3.2 (2026-11-11)

- **Lab hall:** the town's two dark panels inside the lab's back wall are gone (facades_town.inc), the kitchen curtains are cloth folds (prep-lab.mjs CURTAINS), and the rear corridor's open leaf lies on the diagonal kitchen wall (swings with `shift`). test_world shots 87 and 88 look down that corridor. The town's back-wall kit never places inside the lab (city.c inside_building).
- **Memory:** bodies keep their compressed images and the last part's cast is freed at each night: about 300 MB at boot, 380 MB at the end of the full run (was 566 and 744).
- **Scene solids:** world_solid (world.h) stands up a box for a scene's van or table; bodies, lenses and lines of sight stop at it. The crew truck uses the same list.
- **Scenes:** P('me') and FACE on the player now move the player too (story.c), so a later camera hand-off does not snap them back; FACE also turns the head. Kneeling or seated task partners get 1.6 m of room. A roam never sets the clock back past its chapter card, and the card hides the old goal and the wayfinder.
- **Street:** Moheeb and Luis sit on the Chupacabra patio all day and every night (hours [0, 30] in street.mjs), load at the first town roam, and remember you from the day into the nights; a new game or a night from the title starts fresh (street_reset(keep_friends)). The lunch roam's banter points at them.
- **Bike:** every way off the bike lets go of the grips at once (actor_reach with 0).
- **Cars:** five body types from one table, wheels in the arches, both end caps facing out, far versions with the same outlines; lot stalls never cross paint or each other.

## v1.3 (2026-11-12)

- **Title with a save (v2.2.2):** ENTER (A) on the keyholders opens a "You have a saved day" box showing the save's keyholder, hour and chapter: Continue (the default) or Start a new day as the picked keyholder; ESC (B) closes it. `hud_title_ask` in hud_vn.c, `story_save_info` in story.c. The old N (X) shortcut is gone.
- **Nights:** tools/script/night1.mjs to night3.mjs (Open Hack Night, Hack Your Hackerspace, Second Friday) run after the day, with carry-over flags, a NIGHTS row on the title, and do-ocracy lessons in scenes. KH_AUTO plays everything and ends with `[auto] END FINAL KEYHOLDERS`.
- **Roams:** once required tasks are done the goal is shown and reached in a generous circle (some roams end on a timer). The wayfinder sticks to one labelled target and routes round walls.
- **Street:** src/street.c, tools/script/street.mjs (24 stranger types, documented in its header), `node tools/script/street_compile.mjs`, then bake. Doors into five interiors (src/interior.c, one table line per room). Moheeb and Luis on the Chupacabra patio (always out since v1.3.3) (Kyutai voices via tools/voices_kyutai.py).
- **Town:** Mesa Arts Center, Chupacabra (14 N Robson), the lab's back alley, parked cars facing their stalls, original printer models in prep-lab.mjs. Sources in docs/PLACES.md.
- **Pages:** the one-file page is now 106 MB, over GitHub's 100 MB file limit, so tools/publish-pages.sh publishes a small loader page that fetches keyholder.wasc beside it. The one-file page ships in each release.
- **Memory:** about 586 MB at boot and 728 MB after the nights (voices are 44 MB). Too much for the station is likely; an actor free in actor.c would let each night release the day's bodies.

## v1.2 (2026-11-11)

- **Dialogue:** rewritten again against v1.0, the DOLLY mockup and the lab's own tour guides, with a reviewer pass using reader personas (`dialogue-review` notes were not kept in the repo). Full sentences, per-character text for Vee, Dex and Ruth (`{vee, dex, ruth, any}`), action choices (`A()`) that are never spoken. Facts: open to the public, membership is a donation, joining needs no vouch, only 24/7 card access needs a nomination. The lounge scene now builds from Ruth holding the blade to the weapons policy to the orientation question.
- **Cast:** 42 bodies (26 new, recombined from CC0 VRoid parts). See-through coat tails dropped in `prep-vrm.mjs`.
- **World and camera:** interior drawing follows the camera subject (`world_subject`), the kitchen door is a gathered black curtain, 63 real seats (`LAB_SEATS`, `sit_snap`), nobody stands on tables (`on_floor`), the follow camera uses a sphere probe and swings around walls, the director rejects cameras across walls, doors open with look-ahead. The Tech Tear Apart bin is on the short blue-bin shelf by member storage; the clamps go to the woodshop pegboard.
- **Gear:** draw or sheathe at any time in roams (G, d-pad up), the bike from the bike station on Main, summoned outdoors (B, d-pad down), saved in `save_t.bike`. `src/bike.c`.
- **Mouse and keyboard fights:** left click light (hold for heavy), right click guard, Shift tap dodge and hold sprint, Space jump, Q special, middle click lock on, wheel picks targets when locked and weapons otherwise. The web page sends mouse buttons as `KEY_MOUSE_*` codes (`input.h`); the pointer interface in `main.c` is the cleaner long-term route.

## What the game is

One day hosting HeatSync Labs, 9 am to midnight, as Vee, Dex or Ruth. It has 1177 script beats, 838 voiced takes (about 32 minutes of Kokoro speech), 10 fights, 30 hosting tasks, 35 banter lines, and side talks on Main and in the lab. Choices move Trust, Safety, Vibe, Dues and Energy, plus affinity flags. Those change who backs you up in fights (assists), how hard fights are and which ending you get. The day autosaves at each chapter.

v1.1 added:
- Hosting gameplay between scenes (`TASK` kinds greet, point, answer, carry, fix, signin, errand; fix minigames timing, mash, hold, stick), walking banter, a train every 22 s, traffic, and more extras.
- 25 bodies (`AM_*` in `src/actor.h`), 11 of them new CC0 VRoid samples (raw VRMs are kept out of git; `source/vrm-cc0/SOURCES.txt` says where they came from and `tools/slim-vrm.mjs` converts them). New cast: Wes, Hal, Reggie, Dana, Milo, Ines, Gloria.
- Film language in `src/director.c`: live re-checks of every shot, push, react, insert, hero, whip and top shots, and coverage picked by mood (`director_set_mood`, set from `MUSIC` in story.c).
- Fight twists (`fight_twist` in `src/fight.h`): timer, retrieve, score, hold, defend, protect, boss phases with arena changes, barks, assists (R or d-pad up when Trust is high), a QTE finisher, and a special per weapon.
- Battle Zone (`src/battle.c`, `src/hud_battle.c`): ladder, survival and single challenges with modifiers, unlocked by play. Its save is bytes 520 to 1019 of `DS_SAVE.data`.
- 11 music moods (title, hosting, tension, battle, boss, night, zone, final and more) and a sfx level table.

## Where things live

| Area | Files |
|---|---|
| Story | `tools/script/day.mjs` (all scenes, fights table, endings), `tools/script/cast.mjs` (cast, Kokoro voices, palettes), `tools/script/dsl.mjs` (the beat helpers) |
| Story compiler | `tools/script/compile.mjs` writes `src/gen/script.c`, `src/gen/script_enum.h`, `source/lines.json` |
| Voices | `tools/bake_tts.py` writes `assets/voice/voices.bin` (IMA ADPCM 16 kHz, keyed by FNV-1a of `speaker|text`). `src/voice.c` plays it and gives lip sync |
| Runtime story | `src/story.c` (the VM, roaming, side talks, fight glue, extras, saves, `KH_AUTO`), `src/director.c` (shot solver after DOLLY's director.js), `src/hud_vn.c` (dialogue box, choices, cards, roam HUD, title, pause, ending), `src/game.c` (title, play, pause, end) |
| Characters | `tools/prep-vrm.mjs` (VRM to glb, atlases, morphs, outline hull, spring data, `rig.bin`), `src/actor.c`, `src/anim.c`, `tools/cel-patch.mjs` |
| World | `src/world.c`, `src/city.c`, `src/arena.c`, `src/facades_*.inc`, `tools/prep-lab.mjs`, `tools/prep-city.mjs`, `tools/prep-arena.mjs`, `tools/prep-signs.mjs` |
| Combat | `src/fight.c`, `src/fight_int.h`, `src/weapon.c`, `src/player.c`, `src/fx.c`, `src/hud_fight.c`, `src/test_fight.c` (bot tiers and tuning) |
| Battle Zone | `src/battle.c`, `src/battle.h`, `src/hud_battle.c` (`KH_UNLOCK_ALL`, `KH_BZ_AUTO` test flags) |
| Web | `web/index.src.html` (the start page; inlined into `web/keyholder.html` by `build.sh`) |

## Build, test, ship

```sh
./build.sh                                   # build/keyholder.wasc + web/keyholder.html
node tools/script/compile.mjs                # after any change to tools/script/*
KOKORO=<dir with kokoro-v1.0.onnx, voices-v1.0.bin> CACHE=<wav cache> python3 tools/bake_tts.py   # after any line change
OUT=build-auto KH_SRC="$(cd src; ls *.c | grep -v test_ | tr '\n' ' ') gen/script.c" EXTRA_CFLAGS=-DKH_AUTO=9 ./build.sh
node tools/shot.mjs build-auto/keyholder.wasc build-auto/a/f 300,540,... script.json   # the whole day, headless
python3 tools/sheet.py out 5 5 384 216 frames...                                        # contact sheets
```

- The `KH_AUTO=<seconds>` flag autoplays the day. It picks choices by seed, teleports through roams and wins each fight after that many seconds. It logs `[auto]` lines and the end stats. Use it as a soft-lock test after any script change; a run takes about 4 minutes headless.
- Adding `-DKH_DIRLOG` prints a `[dir]` line for every cut and re-solve.
- Adding `-DKH_NOHUD` drops the HUD, which gives clean frames (the web backdrop came from one).
- Kokoro on this Mac (macOS 12): use a venv with Python 3.12, `onnxruntime==1.19.2` and `kokoro-onnx --no-deps`, plus `phonemizer-fork`, `espeakng-loader`, `colorlog`, `soundfile` and `numpy<2.3`. Symlink the files of `espeakng_loader/espeak-ng-data/` one level up, or espeak cannot find `phontab`. The model files come from the thewh1teagle/kokoro-onnx releases (model-files-v1.0). The venv, the model files and the wav cache now live in `~/.cache/kokoro`: `KOKORO=$HOME/.cache/kokoro CACHE=$HOME/.cache/kokoro/wav ~/.cache/kokoro/venv/bin/python tools/bake_tts.py`.
- Shader states: a `[cart] lambert+...` log line means a missing state. Put those lines in a file and run `tools/regen.sh <file>`. It runs the cel patch between two capture passes.
- Ship:
  1. Bump `VERSION` in `build.sh`, then build.
  2. Commit, push, tag, then run `tools/publish-pages.sh`.
  3. `gh release create vX web/keyholder.html build/keyholder.wasc`.
  4. Station, only when `pidof retroarch` is empty:
     - Copy the cart to `/userdata/roms/wasmcart/.incoming/`, then `mv` it into place.
     - Check the md5.
     - Add a change log row.
     - Access is in the ops folder's secrets; see the memory notes on the station.
- To test the web page in Chrome, serve `web/` locally. Synthetic key presses must be held about 120 ms (`keydown`, wait, `keyup`), because the cart reads edges frame to frame.

## Verified

- The whole day under `KH_AUTO`, start to end: no soft locks, no missing places, no missing shader states, no unvoiced lines. It ends at about frame 32000 (the run takes under 2 minutes headless).
- Contact sheets of the whole day every 200 frames, looked at after the director landed. Fixed: hand inserts on idle hands (they framed hips; now a medium close up), background extras walking into lenses (now occluders), a bad two shot in Sam's sit-down scene.
- The web page in Chrome over localhost: PLAY, the title, the opening on Main with voiced dialogue, no console errors.
- Stat pass rates were simulated (random choices, half the optional tasks): about 55 to 78 percent for each gate.

## Needs a human (the polish list)

- **Play feel:** nobody has played v1.1 on a pad or a keyboard. Fights were auto-won; minigames were auto-completed; Battle Zone was run by a bot.
- **Listening:** the 11 music moods, the sfx levels, and the new voices (Milo uses `af_alloy` at a child's speed; check it sounds right).
- **Motion:** the whip pans, the eased re-solves and LOD pops can only be judged at full frame rate.
- **Camera, known weak:** the pendant lamps over the electronics bench have no collision, so the director can sit behind them; packed Arduino Night shots sometimes keep a bystander at the frame edge; Vee's side hair covers her face in profile close ups; the follow camera in roams gets tight behind the player near the coworking chairs.
- **HUD at 1080p:** text is slightly soft because the HUD is drawn before the upscale. Drawing it after the upscale (main.c, hud_gl.c) would make it sharp.
- **World:** the laser scraps sign looks mirrored; four venues are still off-map (Republica, Hope's, Pizza Mart, Guadalupe).
- **Outfits:** body V (High Score) is a fitted bodysuit, body R (Gloria) a schoolgirl skirt, and Shino (char_s) has a bare midriff. Swap if they do not fit the people.
- **Station:** frame rate in the worst views with the new extras and train (the worst view is about 225k triangles, down from 446k).
- **Size and memory:** the cart is 59 MB (voices 19.7 MB) and the web page 75 MB. Wasm memory is about 540 MB at boot and 680 MB by the end of the day. That is a risk on the station; if it struggles, load bodies per chapter.
- **Bike and new controls:** bike feel (acceleration, turning circle, lean, camera at speed), the rider's hands rest on her lap, the 0.28 s hold for a heavy attack on left click, mouse sensitivity in pointer lock, the bell and sheathe levels.
- **Distant faces:** some bodies in lineups read pale at a distance.

## Combat tuning reference (bots in test_fight.c, 10 fights each)

| Foes | Poor bot wins, damage taken | Good bot wins, damage taken |
|---|---|---|
| one duelist, 130 hp | 10/10, 21 | 10/10, 10 |
| three mixed, about 100 hp each | 0/10, 102 | 6/10, 87 |
| boss, 400 hp, power 1.1, speed 1.2 | 1/10, 104 | 7/10, 86 |

## Rules that stay

- No AI credit anywhere: not in commits, PRs, credits or comments.
- No emdashes and no double dashes as punctuation, and no emoji.
- The HeatSync archives are used only for generalized dynamics. No real names, no real conflicts, nothing personal.
- Root's backstory is generic on purpose, and Brant's pitch avoids the real lease.
- Real businesses are neutral scenery only.
- Everyone is welcome; the game says HeatSync does not settle anything by sword.
