# KEYHOLDER handoff

State as of 2026-11-11: v1.3.2 is on GitHub (github.com/virgilvox/keyholder), on Pages (virgilvox.github.io/keyholder) and in the v1.3.2 release (cart md5 `dbcadbfc184fb7874474e5fe1dc6fa8b`). The game station still runs 1.0.2: on 2026-11-10 and 2026-11-11 it did not answer at 172.22.111.83 (not on ARP from the R610 either; likely powered off). To update it, wait until `pidof retroarch` is empty, back up the old cart, copy the new one to `.incoming/` and `mv` it into place, check the md5, and add a row to `HeatSync Labs Operations 2026/docs/06-change-log.md`.

Read `docs/DESIGN.md` for the module contract and the art rules, and `docs/PLACES.md` for every downtown address.

## v1.3.2 (2026-11-11)

- **Lab hall:** the town's two dark panels inside the lab's back wall are gone (facades_town.inc), the kitchen curtains are cloth folds (prep-lab.mjs CURTAINS), and the rear corridor's open leaf lies on the diagonal kitchen wall (swings with `shift`). test_world shots 87 and 88 look down that corridor. The town's back-wall kit never places inside the lab (city.c inside_building).
- **Memory:** bodies keep their compressed images and the last part's cast is freed at each night: about 300 MB at boot, 380 MB at the end of the full run (was 566 and 744).
- **Scene solids:** world_solid (world.h) stands up a box for a scene's van or table; bodies, lenses and lines of sight stop at it. The crew truck uses the same list.
- **Scenes:** P('me') and FACE on the player now move the player too (story.c), so a later camera hand-off does not snap them back; FACE also turns the head. Kneeling or seated task partners get 1.6 m of room. A roam never sets the clock back past its chapter card, and the card hides the old goal and the wayfinder.
- **Street:** Moheeb and Luis sit on the Chupacabra patio from 11 am, load at the first roam in those hours, and remember you from the day into the nights; a new game or a night from the title starts fresh (street_reset(keep_friends)). The lunch roam's banter points at them.
- **Bike:** every way off the bike lets go of the grips at once (actor_reach with 0).
- **Cars:** five body types from one table, wheels in the arches, both end caps facing out, far versions with the same outlines; lot stalls never cross paint or each other.

## v1.3 (2026-11-12)

- **Nights:** tools/script/night1.mjs to night3.mjs (Open Hack Night, Hack Your Hackerspace, Second Friday) run after the day, with carry-over flags, a NIGHTS row on the title, and do-ocracy lessons in scenes. KH_AUTO plays everything and ends with `[auto] END FINAL KEYHOLDERS`.
- **Roams:** once required tasks are done the goal is shown and reached in a generous circle (some roams end on a timer). The wayfinder sticks to one labelled target and routes round walls.
- **Street:** src/street.c, tools/script/street.mjs (24 stranger types, documented in its header), `node tools/script/street_compile.mjs`, then bake. Doors into five interiors (src/interior.c, one table line per room). Moheeb and Luis on the Chupacabra patio (from 11 am since v1.3.2) (Kyutai voices via tools/voices_kyutai.py).
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
