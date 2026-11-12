// Composites the KEYHOLDER station cover (1280x720) as a PS2-anime key visual and renders it with
// Playwright + the system Chrome. The cast's full-body battle-pose cutouts (assets/poster, baked by
// test_poster.c) are the heroes; Vee, Dex and Ruth lead, the rest fill the action behind them.
//
//   node tools/make-poster.mjs            # writes assets/cover.png
//
import { readFileSync, writeFileSync } from 'node:fs';
import { join, resolve, dirname } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { createRequire } from 'node:module';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const require = createRequire(join(ROOT, 'third_party', 'wasmcart', 'package.json'));
const { chromium } = require('playwright');

const b64 = (p) => 'data:image/png;base64,' + readFileSync(p).toString('base64');
const cut = (k) => b64(join(ROOT, 'assets', 'poster', `${k}.png`));

// background action crowd (dimmed, behind); x/y %, h px (height), flip, rot, op
const CROWD = [
  { k: 'cal',     x: 16, y: 58, h: 360, flip: 0, rot: -3, op: 0.5, blur: 1.4 },
  { k: 'champ',   x: 85, y: 56, h: 380, flip: 1, rot: 4,  op: 0.5, blur: 1.4 },
  { k: 'dobbs',   x: 30, y: 50, h: 330, flip: 1, rot: 2,  op: 0.42, blur: 2 },
  { k: 'rennick', x: 71, y: 50, h: 330, flip: 0, rot: -2, op: 0.42, blur: 2 },
  { k: 'vic',     x: 50, y: 44, h: 300, flip: 0, rot: 0,  op: 0.3, blur: 2.6 },
];
// the three leads (bottom-anchored, colour glow); x %, h px, z-order by array order
const LEADS = [
  { k: 'dex',  x: 26, h: 486, flip: 0, glow: '#6cb4ee', tag: 'DEX',  role: 'THE TOOL LIBRARIAN' },
  { k: 'ruth', x: 75, h: 486, flip: 1, glow: '#f07a52', tag: 'RUTH', role: 'ORIENTATION' },
  { k: 'vee',  x: 50, h: 560, flip: 0, glow: '#f2ab1e', tag: 'VEE',  role: 'THE KEYHOLDER' },
];

const html = `<!doctype html><html><head><meta charset="utf-8">
<link rel="preconnect" href="https://fonts.googleapis.com"><link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Anton&family=Archivo+Black&family=Barlow+Condensed:wght@600;700&display=swap" rel="stylesheet">
<style>
  * { margin:0; padding:0; box-sizing:border-box; }
  html,body { width:1280px; height:720px; overflow:hidden; }
  #stage { position:relative; width:1280px; height:720px; overflow:hidden; font-family:'Barlow Condensed',sans-serif;
     background:linear-gradient(176deg, #171a33 0%, #2a2340 30%, #5a3a4a 58%, #9a5a3a 80%, #e0953e 100%); }
  .bg { position:absolute; inset:0; }
  .sun { background:radial-gradient(44% 40% at 50% 84%, rgba(255,214,140,.85) 0%, rgba(240,150,60,.35) 30%, transparent 62%); }
  .rays { background:repeating-conic-gradient(from 0deg at 50% 86%, rgba(255,230,180,.07) 0deg 2.5deg, transparent 2.5deg 10deg); mix-blend-mode:screen; opacity:.7; }
  /* the cover's triangle bunting, as a faint skyline band */
  .bunting { position:absolute; left:0; right:0; top:120px; height:40px; opacity:.16;
     background:repeating-linear-gradient(90deg, transparent 0 18px, #0d1326 18px 20px);
     -webkit-mask:repeating-linear-gradient(135deg, #000 0 14px, transparent 14px 28px); }
  .haze { position:absolute; inset:0; background:radial-gradient(120% 80% at 50% 50%, transparent 46%, rgba(10,8,20,.72) 100%); }
  .ground { position:absolute; left:0; right:0; bottom:0; height:130px; background:linear-gradient(transparent, rgba(8,6,16,.85)); }
  img.ch { position:absolute; bottom:-8px; transform-origin:center bottom; }
  .crowd { filter:brightness(.62) saturate(.8) drop-shadow(0 0 2px rgba(0,0,0,.6)); }
  .lead { filter:drop-shadow(0 10px 18px rgba(0,0,0,.55)) drop-shadow(0 0 2px rgba(0,0,0,.85)); }
  .glow { position:absolute; bottom:30px; width:340px; height:360px; transform:translateX(-50%); border-radius:50%; filter:blur(20px); }
  .title { position:absolute; top:40px; left:0; right:0; text-align:center; }
  .title h1 { font-family:'Anton',sans-serif; font-size:118px; letter-spacing:4px; line-height:.9; color:#fdf4e2;
     -webkit-text-stroke:3px #241427; text-shadow:0 4px 0 #6a3a2a, 0 8px 0 #2a1626, 0 14px 26px rgba(0,0,0,.6); }
  .title .sub { margin-top:4px; font-family:'Barlow Condensed',sans-serif; font-weight:700; font-size:24px;
     letter-spacing:9px; color:#ffe6c0; text-indent:9px; text-shadow:0 2px 4px rgba(0,0,0,.85); }
  .tags { position:absolute; left:0; right:0; bottom:26px; display:flex; justify-content:center; gap:60px; }
  .tag { text-align:center; }
  .tag b { display:block; font-family:'Archivo Black',sans-serif; font-size:27px; color:#fff; text-shadow:0 2px 0 rgba(0,0,0,.6); }
  .tag small { font-family:'Barlow Condensed',sans-serif; font-weight:700; font-size:15px; letter-spacing:3px; color:#e8d7c2; }
  .edge { position:absolute; inset:0; box-shadow:inset 0 0 0 4px rgba(0,0,0,.5), inset 0 0 150px rgba(0,0,0,.5); }
</style></head>
<body><div id="stage">
  <div class="bg sun"></div><div class="bg rays"></div><div class="bunting"></div>
  ${CROWD.map(c => `<img class="ch crowd" src="${cut(c.k)}" style="left:${c.x}%; top:auto; height:${c.h}px; opacity:${c.op}; filter:brightness(.6) saturate(.8) blur(${c.blur}px); transform:translateX(-50%) rotate(${c.rot}deg) scaleX(${c.flip ? -1 : 1});">`).join('\n  ')}
  <div class="bg haze"></div><div class="ground"></div>
  ${LEADS.map(l => `<div class="glow" style="left:${l.x}%; background:radial-gradient(circle, ${l.glow}cc, transparent 68%);"></div>
  <img class="ch lead" src="${cut(l.k)}" style="left:${l.x}%; height:${l.h}px; transform:translateX(-50%) scaleX(${l.flip ? -1 : 1});">`).join('\n  ')}
  <div class="title"><h1>KEYHOLDER</h1><div class="sub">A DAY AT HEATSYNC LABS</div></div>
  <div class="tags">
    ${LEADS.slice().sort((a,b)=>a.x-b.x).map(l => `<div class="tag"><b>${l.tag}</b><small>${l.role}</small></div>`).join('\n    ')}
  </div>
  <div class="edge"></div>
</div></body></html>`;

writeFileSync('/tmp/kh-poster.html', html);
const browser = await chromium.launch({ executablePath: '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' });
const page = await browser.newPage({ viewport: { width: 1280, height: 720 }, deviceScaleFactor: 1 });
await page.goto(pathToFileURL('/tmp/kh-poster.html').href, { waitUntil: 'networkidle' });
await page.evaluate(() => document.fonts.ready);
await page.waitForTimeout(400);
const out = join(ROOT, 'assets', 'cover.png');
await page.locator('#stage').screenshot({ path: out });
await browser.close();
console.log('wrote', out);
