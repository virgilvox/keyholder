# KEYHOLDER handoff

State as of 2026-11-10: v1.0.2 is on GitHub (github.com/virgilvox/keyholder, public, MIT), on Pages (virgilvox.github.io/keyholder, from the gh-pages branch made by `tools/publish-pages.sh`), in the v1.0.2 release (`keyholder.html` and `keyholder.wasc`), and on the game station (`/userdata/roms/wasmcart/keyholder.wasc`, md5 `e65fb9121e5bc7c6a5d479bf40931d3d`, with a gamelist entry and `images/keyholder.png`; ops change log rows in `HeatSync Labs Operations 2026/docs/06-change-log.md`).

Read `docs/DESIGN.md` for the module contract and the art rules, and `docs/PLACES.md` for every downtown address.

## What the game is

One day hosting HeatSync Labs, 9 am to midnight, as Vee, Dex or Ruth (VRoid avatars from DOLLY). It has 12 chapters, 836 script beats, 521 voiced takes (about 35 minutes of Kokoro speech), 9 fights, 3 street side talks and 6 lab side talks. Choices move Trust, Safety, Vibe, Dues and Energy, plus affinity flags. Those change who backs you up, how hard fights are (`foe_scale`) and which ending you get. The day autosaves at each chapter, and Continue restores the player's position.

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
- Adding `-DKH_NOHUD` drops the HUD, which gives clean frames (the web backdrop came from one).
- Kokoro on this Mac (macOS 12): use a venv with Python 3.12, `onnxruntime==1.19.2` and `kokoro-onnx --no-deps`, plus `phonemizer-fork`, `espeakng-loader`, `colorlog`, `soundfile` and `numpy<2.3`. Symlink the files of `espeakng_loader/espeak-ng-data/` one level up, or espeak cannot find `phontab`. The model files come from the thewh1teagle/kokoro-onnx releases (model-files-v1.0). The last venv and the wav cache were under a session scratchpad and may be gone: rebuild them, then rebake everything (about 10 minutes).
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

- The whole day, start to end, under `KH_AUTO`: no soft locks, no missing places, no lines missing from the voice bank.
- Contact sheets of every scene.
- The web page in Chrome: PLAY, the title, Enter, the opening on Main Street with voiced dialogue.
- The station title screen runs at 60 fps at 1080p, with the frame presented in about 8.6 ms.
- Combat tiers were tuned with bots in `src/test_fight.c` (see the table in the commit history and below).

## Not verified, and known weak spots (the polish list)

- No human has played it on a pad or a keyboard. Story fights were only auto-won. Free roaming has only been teleported through.
- **Audio has not been heard.** Music is SMALL PARTS' synth moods, unchanged (`MOOD_TITLE`, `EXPLORE`, `FIGHT`, `BOSS`, `CINE`, `CLEAR`, `SILENT`), and is likely wrong for an anime game. The combat SFX were synthesized and only checked by peak level.
- **Dialogue is stiff.** Lines avoid contractions ("I do not", "It is"), which reads unnatural, and many scenes are long runs of talk with few beats of action. There is no ambient banter while walking, and the side talks are short.
- **Pacing:**
  - Roams are "walk to the marker".
  - There is nothing to do in the world besides side talks.
  - Chapters jump the clock, and between them there is no hosting gameplay (greeting visitors, tidying, fixing machines, answering questions).
- **Camera:**
  - The director picks a shot at the cut and then follows. It does not re-solve when actors move, so close-ups can end in someone's hair or face. Two known frames: Sam's eye filling the screen after "Look up", and a blocked "low" shot on Brant.
  - `low` shots can sit behind furniture in the lab.
  - The OTS shots over Vee are hard because of her tall hair.
  - The boom pulls in hard in tight spots.
  - There is no shot variety such as push-ins on reveals, whip pans into fights, or cutaways to hands and props.
- **Fights:**
  - Every fight is the same "beat the foes in a ring", with no objectives, environment, hazards or interaction.
  - There is no fight variety (protect someone, a timer, an object, a second phase that changes the arena).
  - The arenas have collision only on the ring and their pillars.
  - Only the Arc Blade special was looked at.
  - Burnout (470 hp) and Brant's group may be too hard; retry and talk-down exist.
- **World:**
  - About 446k triangles in the worst view.
  - Four venues are off-map: Republica, Hope's, Pizza Mart and Guadalupe.
  - Casa Copal's side of MacDonald is inferred.
  - The extras (7 actors) just walk the sidewalks and stand at Arduino Night.
  - Story actors walk in straight lines and teleport after a timeout.
- **Title:** the sky at 4:36 pm looks dim at the top.
- **Size:** the cart is 30 MB (voices 16 MB) and the web page 40 MB. Loading takes about 1 second for the first frame on the station.

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
