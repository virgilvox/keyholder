# KEYHOLDER

One day hosting at HeatSync Labs, 9 am to midnight. You hold the card. There is no staff and no janitor. A first-time visitor is standing at the door, someone has brought PVC to the laser, the kitchen has opinions, and the keyholder's job is to look up.

A PS2-style action visual novel for the HeatSync Labs game station (wasmcart, Batocera) and the browser. Pick Vee, Dex or Ruth. Dialogue plays as voiced cutscenes with cinematic angles and choices that stick. Some situations escalate, and then it is an anime fight with a light sword or something worse: in the lab, on Main Street, or in an arena the lab opens into. In between you walk freely through the real lab and a walkable stretch of downtown Mesa at real scale, placed from OpenStreetMap.

**Play it:** download `keyholder.html` from the latest release and open it, or play at https://virgilvox.github.io/keyholder (about 40 MB).

| | Pad | Keyboard and mouse |
|---|---|---|
| Move | left stick | WASD |
| Camera | right stick | mouse, arrows |
| Talk, use | X | E |
| Pick, advance | A | Enter, Space, left click |
| Light attack | X, RT | Q (or left click) |
| Heavy attack | Y | F |
| Guard, tap to parry | LT | C (or right click) |
| Dodge | B | Shift (tap) |
| Sprint | L3, or the stick pushed all the way | Shift (hold), Left Alt |
| Jump | A | Space |
| Special (meter full) | RB | X |
| Lock on | LB, R3 | Tab, middle click |
| Next target (locked on) | flick the right stick | mouse wheel |
| Camera zoom | LB or R3 in roams (three steps) | mouse wheel (not locked on) |
| Weapon | d-pad left, right | 1 to 5 |
| Call a friend (Trust high) | d-pad up | R |
| Draw or sheathe (walking) | d-pad up | G |
| Bike (outdoors, once found) | d-pad down | B |
| Skip scene | hold START | hold Enter |
| Pause | START | Esc, P |

The mouse is captured when you click the game; Esc lets it go and pauses. The settings button on the web page sets its sensitivity and smoothing. The bike waits at the lab's bike station: walk up to it once, and from then on you can call it up anywhere outdoors.

The day saves at each chapter. Trust, Safety, Vibe and Dues move with your choices and change who shows up, how hard the fights are, and how it ends.

On the station: copy `build/keyholder.wasc` to `/userdata/roms/wasmcart/`, back up `gamelist.xml` first if you add an entry.

## Build

```sh
git clone --recursive https://github.com/virgilvox/keyholder
./build.sh                 # build/keyholder.wasc and web/keyholder.html
```

Needs emsdk and Node 22. The pregenerated assets are committed; the tools that make them:

| Tool | Makes |
|---|---|
| `tools/prep-vrm.mjs` | `assets/models/char_*.glb`, `assets/anim/rig.bin`: VRoid avatars baked to PS2 budget with two atlases, expression morphs, an outline hull and spring bones |
| `tools/prep-lab.mjs`, `tools/prep-arena.mjs` | the lab and arenas from the HSL-108 model |
| `tools/prep-city.mjs`, `tools/prep-signs.mjs` | downtown from OpenStreetMap, see `docs/PLACES.md` |
| `tools/script/compile.mjs` | `src/gen/script.c` and `source/lines.json` from `tools/script/day.mjs`, the whole story |
| `tools/bake_tts.py` | `assets/voice/voices.bin` with Kokoro 82M; rerun after changing a line |

`EXTRA_CFLAGS=-DKH_AUTO=4 OUT=build-auto ./build.sh` plays the whole day by itself, headless, under `tools/shot.mjs`: a soft lock test and a way to see every scene. `docs/DESIGN.md` has the rest.

## Credits

See `CREDITS`. Characters are VRoid Project sample avatars, motion is Quaternius (CC0), voices are Kokoro 82M, storefronts follow BikeMesa by Luis Montes (MIT), and the map is OpenStreetMap (ODbL). Venue names and signs belong to their owners. Code and story are MIT.

This is a story. HeatSync Labs does not settle anything by sword. Everyone is welcome, open hours are free, 108 W Main St, Mesa.
