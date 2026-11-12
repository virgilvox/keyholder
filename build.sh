#!/usr/bin/env bash
# Builds KEYHOLDER as a wasmcart GL cart (build/keyholder.wasc) and the one-file
# web player (web/keyholder.html, the cart inside it).
#
#   ./build.sh                 # full build
#   REGEN_STATES=1 ./build.sh  # regenerate the program table (src/gen) from tools/states.txt
#                              # plus build/missing.txt (states a test run could not draw)
#   OUT=build-x ./build.sh     # a separate build tree, for parallel work
#
# Needs: emcc (emsdk) and node >= 22. three.c and wasmcart are git submodules
# (fetched on the first build); three.c gets patches/three.c.patch applied.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
THREE="$ROOT/third_party/three.c"
WASMCART="$ROOT/third_party/wasmcart"
OUT="${OUT:-$ROOT/build}"
TOBJ="$OUT/obj/three_core"
GOBJ="$OUT/obj/game"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"
NAME=keyholder
VERSION=2.2.0

if ! command -v emcc >/dev/null; then
  [ -f "$HOME/emsdk/emsdk_env.sh" ] && source "$HOME/emsdk/emsdk_env.sh" >/dev/null 2>&1
fi
command -v emcc >/dev/null || { echo "emcc not found: install emsdk (https://emscripten.org)" >&2; exit 1; }

[ -f "$THREE/include/three.h" ] || git -C "$ROOT" submodule update --init
[ -d "$WASMCART/node_modules" ] || (cd "$WASMCART" && npm install --silent)
# three.c carries a small local patch (16 lights per type, hex light counts in
# state names); apply it once, and say so if it neither applies nor is applied
if git -C "$THREE" apply --check "$ROOT/patches/three.c.patch" 2>/dev/null; then
  git -C "$THREE" apply "$ROOT/patches/three.c.patch"
elif ! git -C "$THREE" apply --reverse --check "$ROOT/patches/three.c.patch" 2>/dev/null; then
  echo "patches/three.c.patch does not apply to third_party/three.c" >&2; exit 1
fi

mkdir -p "$TOBJ" "$GOBJ" "$ROOT/src/gen"
OPT="-O2 -DNDEBUG -msimd128 -msse2"

# three.c, built once and cached
pids=()
for s in math core geometry curves animation raycaster loaders gltf renderer backend_gles gen_program gen/programs_core_gl gen/programs_dfg; do
  o="$TOBJ/${s//\//_}.o"
  if [ ! -f "$o" ] || [ "$THREE/src/$s.c" -nt "$o" ]; then
    emcc -c "$THREE/src/$s.c" -o "$o" $OPT -std=c99 -DT3_WASMCART -DT3_TABLE_CORE \
      -I "$WASMCART/include" -I "$THREE/include" -I "$THREE/src" &
    pids+=($!)
    while [ "$(jobs -r | wc -l)" -ge "$JOBS" ]; do sleep 0.05; done
  fi
done
fail=0; for p in "${pids[@]+"${pids[@]}"}"; do wait "$p" || fail=1; done
[ $fail = 0 ] || { echo "three.c compile failed" >&2; exit 1; }

# the r186 programs this game draws, as a C table (committed; regenerate on request)
if [ "${REGEN_STATES:-0}" = 1 ] || [ ! -f "$ROOT/src/gen/ds_gl.c" ]; then
  miss=(); [ -s "$OUT/missing.txt" ] && miss=(--missing "$OUT/missing.txt")
  gen() { (cd "$ROOT" && node "$THREE/tools/gen-project-table.mjs" --name ds --out src/gen --states tools/states.txt --cache build/t3-captures "$@"); }
  gen "${miss[@]+"${miss[@]}"}"     # captures anything new from three.js
  node "$ROOT/tools/cel-patch.mjs" "$ROOT/build/t3-captures"
  gen                               # emits the table with the patched text
fi

# KH_SRC: a subset of src/*.c (names) so one module can be built while another is mid change
if [ -n "${KH_SRC:-}" ]; then SRCS=""; for f in $KH_SRC; do SRCS="$SRCS $ROOT/src/$f"; done; else SRCS="$ROOT/src/*.c $ROOT/src/gen/script.c"; fi
GAMEF="-DWC_USE_GL ${EXTRA_CFLAGS:-}"
if [ ! -f "$GOBJ/cv.o" ] || [ "$ROOT/src/cv.cpp" -nt "$GOBJ/cv.o" ]; then
  em++ -c "$ROOT/src/cv.cpp" -o "$GOBJ/cv.o" $OPT -std=c++17 -fno-exceptions -fno-rtti -ffp-contract=off $GAMEF \
    -I "$ROOT/src" -I "$ROOT/third_party/canvas_ity" -I "$WASMCART/include"
fi

emcc $OPT -std=gnu11 -Wall -Wshadow -Wno-unused-function -ffp-contract=off -DT3_WASMCART $GAMEF \
  -I "$WASMCART/include" -I "$THREE/include" -I "$THREE/src" -I "$ROOT/src" -I "$ROOT/third_party"  \
  $SRCS "$ROOT/src/gen/ds_gl.c" "$GOBJ/cv.o" "$TOBJ"/*.o \
  -sSTANDALONE_WASM=1 -sEXPORTED_FUNCTIONS='["_wc_init","_wc_render","_wc_get_info","_wc_set_seed","_malloc","_free"]' \
  -sERROR_ON_UNDEFINED_SYMBOLS=0 -sINITIAL_MEMORY=268435456 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576 \
  --no-entry -o "$OUT/$NAME.wasm"

WASMCART="$WASMCART" node "$THREE/wasm/check-imports.mjs" "$OUT/$NAME.wasm"

rm -rf "$OUT/assets" && mkdir -p "$OUT/assets"
cp "$ROOT"/assets/fonts/*.ttf "$OUT/assets/"
cp "$ROOT"/assets/models/*.glb "$OUT/assets/"
cp "$ROOT"/assets/anim/*.bin "$OUT/assets/"
cp "$ROOT"/assets/voice/*.bin "$OUT/assets/" 2>/dev/null || true
cp "$ROOT"/assets/models/*.spr "$OUT/assets/"
cp "$ROOT"/assets/signs/* "$OUT/assets/" 2>/dev/null || true

node "$WASMCART/bin/wasmcart-pack.js" --wasm "$OUT/$NAME.wasm" --assets "$OUT/assets" \
  --name "KEYHOLDER" --version "$VERSION" --width 1280 --height 720 --players 1 \
  --controls dpad,left_stick,right_stick,a,b,x,y,l,r,left_trigger,right_trigger,start,select \
  --output "$OUT/$NAME.wasc" >/dev/null
echo "built $OUT/$NAME.wasc ($(wc -c < "$OUT/$NAME.wasc" | tr -d ' ') bytes)"

# the one-file web player: shell + wasmcart web host + the cart as base64
if [ "$OUT" = "$ROOT/build" ] && [ -f "$ROOT/web/index.src.html" ] && [ -f "$ROOT/web/vendor/host-bundle.js" ]; then
  node "$ROOT/tools/inline-html.mjs" "$ROOT/web/index.src.html" "$ROOT/web/vendor/host-bundle.js" "$OUT/$NAME.wasc" "$ROOT/web/$NAME.html"
fi
