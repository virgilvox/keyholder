// The city's sign atlas: assets/signs/city_signs.png and src/gen/sign_atlas.h.
// Logos are the venues' own artwork as collected by bikemesa (see CREDITS), scaled down; lettering
// is drawn here. Keys match the mesa_logo and mesa_name calls in src/facades_*.inc.
//
//   node tools/prep-signs.mjs <bikemesa/assets/signs>
import sharp from 'sharp';
import { writeFileSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const SRC = process.argv[2];
if (!SRC) { console.error('usage: node tools/prep-signs.mjs <bikemesa/assets/signs>'); process.exit(2); }
const W = 1024, H = 1024, PAD = 3;

const CREAM = '#f5ebd5', INK = '#141414';
// [text, ink colour, paper colour or null (letters on nothing), font weight, serif?, neon?]
const LB = {
  'HeatSync Labs': ['#2b2b2b', null], 'THE': ['#f5eed8', null, 1], 'NILE': ['#f5eed8', null, 1], 'EST. 1924': ['#f5eed8', null, 1], 'Concert': ['#f5eed8', null, 1],
  'BURRITOHOLICS': ['#e1ddb8', null], 'WORTH': [INK, null], 'TAKEAWAY': [INK, null], 'Tacos Chiwas': ['#285f40', null],
  'THE SACRED PINT': ['#f0d9a0', null], 'GUS\'S': ['#ffd27a', null, 0, 1], 'FRIED CHICKEN': ['#7dff8c', null, 0, 1],
  'DIAMOND DONS': ['#ebebeb', null, 1], 'LEGENDARY STEAKS & SPIRITS': ['#161616', null, 1], 'Lost Dutchman Coffee': ['#4a2d22', null, 1],
  'CHUPACABRA TAPROOM': ['#bc976c', null], 'ORO BREWERY & TAPROOM': [INK, CREAM], 'MAIN': ['#e6af41', null], 'BURGERS': ['#e6af41', null],
  'ORGANIC BEAN CAFE': [INK, CREAM], 'OBC': ['#e6afe6', null], 'B.R.I. TAPROOM & ARCADE': ['#37af41', null],
  'EAST VALLEY': ['#ebebeb', null, 1], 'PARANORMAL': ['#ebebeb', null, 1], "MESA'S HAUNTED MUSEUM": ['#ebebeb', null, 1],
  'AGAINST': ['#d6ebf8', null, 1, 1], 'THE GRAIN': ['#d6ebf8', null, 1, 1], 'BAKERY & CAFE': ['#d6ebf8', null, 1, 1],
  'CASA': ['#ebe0a6', null, 1], 'COPAL': ['#ebe0a6', null, 1],
  // written for this game
  "POMEROY'S": ['#39352c', null, 1], 'POMEROY BUILDING': ['#39352c', null, 1], 'OLD BRICK HOUSE VINTAGE MARKET': ['#3a3a3a', null], 'ONE MACDONALD': ['#2e3a40', null],
  'Naysayer Tattoo': ['#f2f2f2', '#17181b', 1], 'Blossom': ['#55443c', null, 1], "CRISMON'S FLOWERS": ['#6a5a2c', null, 1], 'Atomic Vintage': ['#ff7d3a', null, 0, 1],
  'History, by George!': ['#232323', '#e8e2cf', 1], 'Tre Bella': ['#39352c', null, 1], 'OneOhOne Gallery': ['#39352c', null], 'Le Studio': ['#39352c', null, 1], 'Linton Milano Music': ['#f2f2f2', '#21242a', 1],
  'THAI CUISINE': ['#ffd27a', '#7c1f22', 0], 'THE DREW': ['#efe6d0', null, 1], '12 WEST': ['#ffe9b8', null, 0, 1], 'RIBAUDO FINE ARTS': ['#303030', null, 1], 'CHIROPRACTIC': ['#303030', null], 'NAYSAYER': ['#f2f2f2', '#17181b', 0],
  'The Boutique': ['#55443c', null, 1], 'DE CESARE DESIGN': ['#303030', null], 'SHOP': [INK, CREAM], 'OPEN': ['#ff55aa', null, 0, 1], 'CENTER / MAIN': ['#f4f4f4', '#135e6a', 0], 'METRO': ['#f4f4f4', '#135e6a', 0],
  'MESA ARTS CENTER': ['#f0f4f6', null], 'CITY PLAZA': ['#e8e6e0', null], 'DONATE': ['#f5f2e6', '#b24a18', 0],
};
const LOGO = {
  'azdistilling.png': { neon: [255, 130, 100], max: 300 }, 'casacopal-official.png': { max: 200 }, 'chupacabra.png': { neon: [255, 85, 170], max: 200 },
  'diamonddons-official.jpg': { max: 96 }, 'espiritu-wordmark.png': { neon: [255, 218, 160], max: 300 }, 'goatandram.png': { max: 220 }, 'heatsync.png': { max: 128 },
  'levelone.jpg': { max: 220 }, 'lostdutchman-official.png': { max: 128 }, 'madcandy.png': { max: 220 }, 'mainburger.png': { max: 200 }, 'mangos-official.png': { max: 256 },
  'margaritas-official.png': { max: 340, recolor: true }, 'oro.png': { max: 180 }, 'phantomfox.png': { max: 320 }, 'proof.png': { max: 128 }, 'quechevere.png': { max: 256 }, 'worth.jpg': { max: 128 },
};
const items = [];
const esc = (s) => s.replace(/&/g, '&amp;').replace(/'/g, '&#39;');
for (const [text, [ink, paper, serif, neon]] of Object.entries(LB)) {
  const px = 34, wch = Math.ceil(text.length * px * 0.72) + 16, tall = px + 14;
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${wch}" height="${tall}">${paper ? `<rect width="100%" height="100%" fill="${paper}"/>` : ''}<text x="50%" y="${px}" text-anchor="middle" font-family="${serif ? 'Georgia, Times New Roman, serif' : 'Arial Black, Helvetica Neue, Arial, sans-serif'}" font-weight="700" font-size="${px}" fill="${ink}">${esc(text)}</text></svg>`;
  let img = sharp(Buffer.from(svg)).png();
  // trim to the ink when there is no paper so that signs fill their quads
  if (!paper) img = sharp(await img.toBuffer()).trim();
  const buf = await img.toBuffer();
  const meta = await sharp(buf).metadata();
  // no wider than the atlas allows
  const w = Math.min(meta.width, 360), h = Math.round((meta.height * w) / meta.width);
  items.push({ key: text, buf: await sharp(buf).resize(w, h).png().toBuffer(), w, h, neon: !!neon });
}
for (const [file, o] of Object.entries(LOGO)) {
  const m = await sharp(join(SRC, file)).ensureAlpha().metadata();
  const sc = Math.min(1, o.max * 0.8 / Math.max(m.width, m.height));
  const w = Math.round(m.width * sc), h = Math.round(m.height * sc);
  const { data } = await sharp(join(SRC, file)).ensureAlpha().resize(w, h).raw().toBuffer({ resolveWithObject: true });
  for (let i = 0; i < w * h; i++) {
    const p = i * 4;
    if (o.neon) { data[p + 3] = ((255 - data[p]) * data[p + 3]) / 255; data[p] = o.neon[0]; data[p + 1] = o.neon[1]; data[p + 2] = o.neon[2]; }
    if (o.recolor) { const px = i % w, py = Math.floor(i / w); const glass = px > w * 0.6 && px < w * 0.72 && py < h * 0.78; const face = data[p + 1] > data[p]; data[p] = glass ? 136 : face ? 223 : 68; data[p + 1] = glass ? 192 : face ? 65 : 76; data[p + 2] = glass ? 66 : face ? 61 : 38; }
  }
  items.push({ key: file, buf: await sharp(data, { raw: { width: w, height: h, channels: 4 } }).png().toBuffer(), w, h, neon: !!o.neon });
}
items.sort((a, b) => b.h - a.h);
let x = PAD, y = PAD, row = 0;
const comp = [];
for (const it of items) {
  if (x + it.w + PAD > W) { x = PAD; y += row + PAD; row = 0; }
  if (y + it.h + PAD > H) throw new Error(`atlas full at ${it.key}`);
  it.x = x; it.y = y; comp.push({ input: it.buf, left: x, top: y });
  x += it.w + PAD; row = Math.max(row, it.h);
}
mkdirSync(join(ROOT, 'assets/signs'), { recursive: true });
const out = await sharp({ create: { width: W, height: H, channels: 4, background: { r: 0, g: 0, b: 0, alpha: 0 } } }).composite(comp).png({ compressionLevel: 9 }).toBuffer();
writeFileSync(join(ROOT, 'assets/signs/city_signs.png'), out);
let h = '/* generated by tools/prep-signs.mjs: do not edit */\n#ifndef SIGN_ATLAS_H\n#define SIGN_ATLAS_H\ntypedef struct { const char *key; float u0, v0, u1, v1; int neon; } sign_rect;\n';
h += `#define SIGN_N ${items.length}\nstatic const sign_rect SIGNS[SIGN_N] = {\n`;
for (const it of items) h += `  {${JSON.stringify(it.key)},${(it.x / W).toFixed(5)}f,${(it.y / H).toFixed(5)}f,${((it.x + it.w) / W).toFixed(5)}f,${((it.y + it.h) / H).toFixed(5)}f,${it.neon ? 1 : 0}},\n`;
h += '};\n#endif\n';
writeFileSync(join(ROOT, 'src/gen/sign_atlas.h'), h);
console.log(`signs: ${items.length} in a ${W}x${H} atlas, last row ends at ${y + row}, ${(out.length / 1024).toFixed(0)} KB`);
