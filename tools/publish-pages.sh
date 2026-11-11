#!/usr/bin/env bash
# Publishes the built one-file player to GitHub Pages (the gh-pages branch):
# web/keyholder.html becomes index.html, with the cart beside it.
#
#   ./build.sh && tools/publish-pages.sh
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
[ -f "$ROOT/web/keyholder.html" ] || { echo "build first: web/keyholder.html is missing" >&2; exit 1; }
WT="$(mktemp -d)"
trap 'git -C "$ROOT" worktree remove --force "$WT" >/dev/null 2>&1 || true' EXIT
if git -C "$ROOT" ls-remote --exit-code --heads origin gh-pages >/dev/null 2>&1; then
  git -C "$ROOT" fetch -q origin gh-pages
  git -C "$ROOT" worktree add -q -B gh-pages "$WT" origin/gh-pages
else
  git -C "$ROOT" worktree add -q --detach "$WT"
  git -C "$WT" checkout -q --orphan gh-pages
  git -C "$WT" rm -rq --cached . && git -C "$WT" clean -fdxq
fi
node "$ROOT/tools/inline-html.mjs" "$ROOT/web/index.src.html" "$ROOT/web/vendor/host-bundle.js" - "$WT/index.html" >/dev/null   # a loader page: the cart is fetched beside it
cp "$ROOT/build/keyholder.wasc" "$WT/keyholder.wasc"
touch "$WT/.nojekyll"   # serve the files as they are
git -C "$WT" add -A
if git -C "$WT" diff --cached --quiet; then echo "pages already up to date"; exit 0; fi
git -C "$WT" commit -q -m "Publish $(git -C "$ROOT" describe --tags --always) to Pages"
git -C "$WT" push -q origin gh-pages
echo "published"
