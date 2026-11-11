// Bakes the HSL-108 model into the game's lab, at 1 unit = 1 metre in world coordinates:
//
//   assets/models/lab.glb   vertex coloured chunks: one opaque chunk per 4 m band along the room,
//                           and its far twin (lab_bNN_far: no small parts, no lettering, coarser),
//                           lab_glow (emissive parts), lab_glass_*, the front door's two leaves
//                           (door_a, door_b, each with door_*_glass), and the shell the model
//                           does not have: ceiling, roof, parapets and the street front's upper wall
//   src/gen/lab_areas.h     the model's area tags as boxes, in plan and in world coordinates
//
// World: plan x runs from the street (0) to the back (40.4 m), plan z from the west wall (0) to
// the east wall (9.4 m); plan +x is world -z, plan +z is world +x, y is up. Origin: plan
// (0, DOOR_Z) on the street line, in front of the recessed double doors at plan x 5.45.
//
// The Elegoo Centauri Carbon 2 printers in the model are converted from a GPL-3.0 CAD release,
// so they are dropped and plain printer boxes of the same size stand in their place.
//
//   node tools/prep-lab.mjs [HSL-108-full.glb]
import { writeFileSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { readModel, simplify, Chunk, writeGlb, srgb, MODEL } from './glb-lib.mjs';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const DOOR_Z = 2.775;           // centre of the double doors (plan z 1.88 to 3.67)
const DOOR_X = 5.45;
const BAND = 4;                 // chunk length along the room, metres
const toWorld = (px, y, pz) => [pz - DOOR_Z, y, -px];

const parts = readModel(process.argv[2] || MODEL);
const chunks = new Map();
const ch = (key, kind) => { if (!chunks.has(key)) chunks.set(key, new Chunk(key, kind)); return chunks.get(key); };
const band = (px) => `lab_b${String(Math.max(0, Math.floor(px / BAND))).padStart(2, '0')}`;

const isPrinter = (n) => /centauri/i.test(n);
const dropPart = (p) => p.name === 'LED cube LEDs' || p.name === 'Hacker backdrop peg holes' || p.name === 'Caution tape stripes';
const DOOR_PARTS = /^Entry door /;
const LETTERING = /text|letter|label|decal|vinyl|digit|plate|title|notice|sign\b/i;
// the rear exit leaves stand part way across their doorways: swing them flat against the hall wall
const swings = {
  'Rear corridor exit open leaf': { hinge: [40.41, 5.93], to: -Math.PI },        // along plan z = 5.93
  'Rear workshop exit open leaf': { hinge: [40.41, 7.41], to: -Math.PI },
  'Metalshop door open leaf': { hinge: [30.13, 5.48], to: Math.PI / 2 },       // and this one clear of its doorway
};

const printers = new Map(), areasW = new Map(), areasP = new Map();
const landmarks = new Map();
let keptT = 0, droppedT = 0;
const grow = (m, key, b) => {
  if (!m.has(key)) m.set(key, [1e9, 1e9, 1e9, -1e9, -1e9, -1e9]);
  const a = m.get(key);
  for (let k = 0; k < 3; k++) { a[k] = Math.min(a[k], b[k]); a[k + 3] = Math.max(a[k + 3], b[k + 3]); }
};
const worldBox = (b) => [b[2] - DOOR_Z, b[1], -b[3], b[5] - DOOR_Z, b[4], -b[0]];

for (const p of parts) {
  if (dropPart(p)) { droppedT += p.idx.length / 3; continue; }
  if (isPrinter(p.name)) { grow(printers, p.name, p.box); continue; }
  const ext = [p.box[3] - p.box[0], p.box[4] - p.box[1], p.box[5] - p.box[2]];
  const maxExt = Math.max(...ext);
  if (p.area && !/^(envelope|floor)$/.test(p.area)) { grow(areasP, p.area, p.box); grow(areasW, p.area, worldBox(p.box)); }
  if (p.area === 'envelope' || p.area === 'floor') { grow(areasP, p.area, p.box); grow(areasW, p.area, worldBox(p.box)); }
  if (maxExt < 0.03 && p.alpha === 1) { droppedT += p.idx.length / 3; continue; }   // hardware too small for 800 x 450

  let pos = p.pos, idx = p.idx;
  const t0 = idx.length / 3;
  if (t0 > 100 && !DOOR_PARTS.test(p.name)) {
    const err = p.group === 'signage' ? 0.012 : 0.008;
    const s = simplify(p, 0.35, err);
    if (s.idx.length / 3 < t0 && s.idx.length >= 3) { pos = s.pos; idx = s.idx; }
  }
  const sw = swings[p.name];
  let xf = (x, y, z) => toWorld(x, y, z);
  if (sw) {
    // the leaf's far edge from its hinge: its current angle, then turn it to the target
    let far = 0, fa = 0;
    for (let i = 0; i < pos.length; i += 3) { const d = Math.hypot(pos[i] - sw.hinge[0], pos[i + 2] - sw.hinge[1]); if (d > far) { far = d; fa = Math.atan2(pos[i + 2] - sw.hinge[1], pos[i] - sw.hinge[0]); } }
    const da = sw.to - fa, c = Math.cos(da), s = Math.sin(da);
    xf = (x, y, z) => { const dx = x - sw.hinge[0], dz = z - sw.hinge[1]; return toWorld(sw.hinge[0] + dx * c - dz * s, y, sw.hinge[1] + dx * s + dz * c); };
  }
  let rgb = p.color;
  // the Tech Tear Apart bins (the wire bin shelf at the end of member storage) are bright blue plastic
  if (p.name === 'Wire bin' && p.area === 'member storage and jewelry') rgb = srgb(0x2c6ac4);
  const cx = (p.box[0] + p.box[3]) / 2;
  let target;
  const leaf = (p.box[2] + p.box[5]) / 2 < 2.76 ? 'a' : 'b';
  const isDoor = (DOOR_PARTS.test(p.name) || p.name === 'Workplace rights notice') && p.box[0] > 5 && p.box[3] < 6;
  if (isDoor) target = p.alpha < 1 ? ch(`door_${leaf}_glass`, 'glass') : ch(`door_${leaf}`, 'opaque');
  else if (p.alpha < 1) target = ch(cx < 7 ? 'lab_glass_front' : 'lab_glass_in', 'glass');
  else if (p.emissive) { target = ch(cx < 14 ? 'lab_glow_f' : 'lab_glow_b', 'glow'); rgb = p.emissive.map((v, i) => Math.min(1, Math.max(v, p.color[i]))); }
  else if (p.name === 'Blackout curtain') { droppedT += p.idx.length / 3; continue; }   // the real ones hang pulled back: see CURTAINS
  else if (p.name === 'Ground slab') target = ch('lab_slab', 'opaque');          // the floor under every band, near or far
  else target = ch(band(cx), 'opaque');
  target.add(pos, idx, rgb, xf);
  // the far twin of a band: what reads from across the room
  // the storefront's big lettering stays: it reads from across the street
  const storefront = (target.name === 'lab_b00' || target.name === 'lab_b01') && maxExt >= 0.9;
  if (target.name.startsWith('lab_b') && maxExt >= 0.3 && (storefront || !LETTERING.test(p.name))) {
    let fp = pos, fi = idx;
    if (fi.length / 3 > 40 && maxExt < 3) { const s = simplify({ pos, idx, box: p.box }, 0.2, 0.025); if (s.idx.length >= 3 && s.idx.length < fi.length) { fp = s.pos; fi = s.idx; } }
    ch(`${target.name}_far`, 'opaque').add(fp, fi, rgb, xf);
  }
  keptT += idx.length / 3;
}

// The kitchen's doors came off long ago, and each opening has a black curtain, pulled back. The model
// hangs both blackout curtains closed across their doorways, where they read as flat black walls (and
// a follow camera behind one sees nothing). Here each is gathered against the wall beside its opening,
// inside the kitchen: three folds and a tie, in the band so it collides like the wall it hangs on.
// Plan metres: px from the street, pz from the west wall. The rods stay as the model has them.
const CURTAINS = [
  // the side door from the biolab (opening pz 4.19 to 5.10 in the wall at px 36.33 to 36.46): gathered west of it
  { folds: [[36.47, 3.94, 36.58, 4.03], [36.50, 4.01, 36.64, 4.12], [36.48, 4.10, 36.60, 4.19]] },
  // the hall door (opening px 36.87 to 37.80 in the wall at pz 5.28 to 5.55): gathered toward the back
  { folds: [[37.80, 5.15, 37.89, 5.27], [37.87, 5.12, 37.98, 5.26], [37.96, 5.16, 38.05, 5.27]] },
];
{
  const pbox = (c, px0, y0, pz0, px1, y1, pz1, rgb) => c.box(pz0 - DOOR_Z, y0, -px1, pz1 - DOOR_Z, y1, -px0, rgb);
  const FOLD = [srgb(0x161618), srgb(0x202024), srgb(0x121214)], TIE = srgb(0x45454a);
  for (const cu of CURTAINS) {
    const xs = cu.folds.flatMap((f) => [f[0], f[2]]), zs = cu.folds.flatMap((f) => [f[1], f[3]]);
    const c = ch(band((Math.min(...xs) + Math.max(...xs)) / 2), 'opaque');
    for (const k of [c, ch(`${c.name}_far`, 'opaque')]) cu.folds.forEach((f, i) => pbox(k, f[0], 0.02, f[1], f[2], 2.12, f[3], FOLD[i]));
    pbox(c, Math.min(...xs) - 0.01, 1.0, Math.min(...zs) - 0.01, Math.max(...xs) + 0.01, 1.06, Math.max(...zs) + 0.01, TIE);
  }
}

// The Tech Tear Apart bin: the wire bin shelf at the end of member storage (plan x 18.65 to 19.56, z 3.55 to
// 3.91), across the hall from the electronics bench. A label on its hallway face (plan z 3.91), above the top
// row of bins: blue block capitals on a white strip, like the cabinet labels. Plain boxes from a 3 x 5 pixel
// font; read from the hall, the text runs along plan +x.
{
  const GLYPH = {
    T: ['###', '.#.', '.#.', '.#.', '.#.'], E: ['###', '#..', '##.', '#..', '###'], C: ['###', '#..', '#..', '#..', '###'],
    H: ['#.#', '#.#', '###', '#.#', '#.#'], A: ['###', '#.#', '###', '#.#', '#.#'], R: ['##.', '#.#', '##.', '#.#', '#.#'],
    P: ['##.', '#.#', '##.', '#..', '#..'], ' ': ['...', '...', '...', '...', '...'],
  };
  const text = 'TECH TEAR APART', X0 = 18.67, X1 = 19.54, Y0 = 1.26, Y1 = 1.38, FACE = 3.91;
  const c = ch(band((X0 + X1) / 2), 'opaque');
  const pbox = (px0, y0, pz0, px1, y1, pz1, rgb) => c.box(pz0 - DOOR_Z, y0, -px1, pz1 - DOOR_Z, y1, -px0, rgb);
  pbox(X0, Y0, FACE - 0.012, X1, Y1, FACE, srgb(0xeceae2));
  const cols = text.length * 4 - 1, px = (X1 - X0 - 0.03) / cols, py = Math.min(px, (Y1 - Y0 - 0.03) / 5);
  const x0 = X0 + 0.015, y0 = (Y0 + Y1) / 2 + py * 2.5, ink = [0.19, 0.46, 0.81];   // the model's label blue (linear)
  [...text].forEach((ch_, i) => GLYPH[ch_].forEach((row, r) => {
    // runs of lit pixels along a row, one box each
    for (let k = 0; k < 3;) {
      if (row[k] !== '#') { k++; continue; }
      let e = k; while (e < 3 && row[e] === '#') e++;
      pbox(x0 + (i * 4 + k) * px, y0 - (r + 1) * py, FACE, x0 + (i * 4 + e) * px, y0 - r * py, FACE + 0.004, ink);
      k = e;
    }
  }));
}

// printers: a dark cabinet with a lighter door, no CAD
for (const b of printers.values()) {
  const w = worldBox(b), c = ch(band((b[0] + b[3]) / 2), 'opaque');
  for (const k of [c, ch(`${c.name}_far`, 'opaque')]) {
    k.box(...w, srgb(0x2e3034));
    k.box(w[0] + 0.03, w[1] + (w[4] - w[1]) * 0.25, w[2] + 0.04, w[3] - 0.03, w[4] - 0.05, w[2] - 0.005, srgb(0x96a0aa));
  }
}

// the shell the model leaves out. Ceiling over the hall, a higher one over the attic end, the
// roof deck and parapet wall on the outside, and troffers.
const shell = ch('lab_shell', 'opaque');
const W0 = toWorld(0, 0, -0.09)[0], W1 = toWorld(0, 0, 9.49)[0];
const SPLIT = 31.3, LOW = 3.05, HIGH = 4.55, PAR = 5.1, Z = (px) => -px;
const CEIL = srgb(0xa6a49c), WHITE = srgb(0xe9e6dc);
shell.box(W0, LOW, Z(SPLIT), W1, LOW + 0.05, 0.05, CEIL);
shell.box(W0, HIGH, Z(40.49), W1, HIGH + 0.05, Z(SPLIT), CEIL);
shell.box(W0, LOW, Z(SPLIT), W1, HIGH, Z(SPLIT) + 0.08, CEIL);
// outer walls above the model's 3.05 m, up to the parapet; the east and west ones hide against the neighbours
shell.box(W0 - 0.1, LOW, Z(40.6), W0, PAR, 0.1, WHITE);
shell.box(W1, LOW, Z(40.6), W1 + 0.1, PAR, 0.1, WHITE);
shell.box(W0 - 0.1, LOW, Z(40.6), W1 + 0.1, PAR, Z(40.49), WHITE);
shell.box(W0 - 0.1, LOW, -0.1, W1 + 0.1, PAR, 0.1, WHITE);
shell.box(W0, HIGH + 0.05, Z(40.49), W1, HIGH + 0.1, 0, srgb(0x8a8880));
// the back wall is only 3.05 m in the model, and so are the attic's sides from the inside
const glowF = ch('lab_lamps', 'glow');
for (let x = 7.5; x < SPLIT - 1; x += 2.4) for (const z of [2.0, 4.7, 7.4]) {
  const [wx, , wz] = toWorld(x, 0, z);
  glowF.box(wx - 0.3, LOW - 0.04, wz - 0.6, wx + 0.3, LOW, wz + 0.6, [1, 0.93, 0.78]);
}

// Seats, for the story's sit snap: every chair, stool, sofa cushion and camp chair in the model, with
// the height of its seat and the way a sitter faces (away from the backrest; a stool faces the nearest
// work surface). Written to lab_areas.h as LAB_SEATS in world coordinates.
const SEAT_KIND = [[/^Sofa seat cushion$/, 0.41], [/^Office chair$/, 0.5], [/^Chair$/, 0.46], [/^Bar stool$/, 0.74],
  [/^Keyboard stool$/, 0.49], [/^Camp chair seat$/, 0.46]];
const seats = [];
for (const p of parts) {
  const k = SEAT_KIND.find(([re]) => re.test(p.name));
  if (!k) continue;
  const cx = (p.box[0] + p.box[3]) / 2, cz = (p.box[2] + p.box[5]) / 2;
  const s = seats.find((q) => q.name === p.name && Math.hypot(q.cx - cx, q.cz - cz) < 0.25);
  if (s) { for (let i = 0; i < 3; i++) { s.box[i] = Math.min(s.box[i], p.box[i]); s.box[i + 3] = Math.max(s.box[i + 3], p.box[i + 3]); } continue; }
  seats.push({ name: p.name, top: k[1], cx, cz, box: [...p.box] });
}
const BACKISH = /chair|sofa back cushion|camp chair back/i, SURFACE = /top|table|desk|bench|counter/i;
for (const s of seats) {
  s.cx = (s.box[0] + s.box[3]) / 2; s.cz = (s.box[2] + s.box[5]) / 2;
  // the backrest: vertices well above the seat, near it
  let bx = 0, bz = 0, bn = 0;
  for (const p of parts) {
    if (!BACKISH.test(p.name) || p.box[4] < s.top + 0.2) continue;
    if (p.box[3] < s.box[0] - 0.15 || p.box[0] > s.box[3] + 0.15 || p.box[5] < s.box[2] - 0.15 || p.box[2] > s.box[5] + 0.15) continue;
    for (let i = 0; i < p.pos.length; i += 3) {
      if (p.pos[i + 1] < s.top + 0.2 || p.pos[i] < s.box[0] - 0.12 || p.pos[i] > s.box[3] + 0.12 || p.pos[i + 2] < s.box[2] - 0.12 || p.pos[i + 2] > s.box[5] + 0.12) continue;
      bx += p.pos[i]; bz += p.pos[i + 2]; bn++;
    }
  }
  let fx, fz;
  if (bn > 0 && Math.hypot(bx / bn - s.cx, bz / bn - s.cz) > 0.04) { fx = s.cx - bx / bn; fz = s.cz - bz / bn; }
  else {
    // no back: face the nearest work surface's closest point
    let best = 1e9;
    for (const p of parts) {
      if (!SURFACE.test(p.name) || p.box[4] < 0.65 || p.box[4] > 1.15 || p.box[4] - p.box[1] > 0.2) continue;
      const qx = Math.min(Math.max(s.cx, p.box[0]), p.box[3]), qz = Math.min(Math.max(s.cz, p.box[2]), p.box[5]);
      const d = Math.hypot(qx - s.cx, qz - s.cz);
      if (d > 0.02 && d < best) { best = d; fx = qx - s.cx; fz = qz - s.cz; }
    }
    if (fx === undefined) { fx = 1; fz = 0; }
  }
  // a sofa's cushions sit in a row along one straight back: square them to it
  if (/^Sofa/.test(s.name)) { if (Math.abs(fx) > Math.abs(fz)) fz = 0; else fx = 0; }
  const [wx, , wz] = toWorld(s.cx, 0, s.cz);
  s.wx = wx; s.wz = wz; s.wyaw = Math.atan2(fz, -fx);   // plan (fx, fz) is world (fz, -fx); yaw = atan2(x, z)
}
seats.sort((a, b) => a.cx - b.cx || a.cz - b.cz);

const list = [...chunks.values()].sort((a, b) => (a.name < b.name ? -1 : 1));
const r = writeGlb(join(ROOT, 'assets/models/lab.glb'), list);

const id = (s) => s.toUpperCase().replace(/[^A-Z0-9]+/g, '_').replace(/^_|_$/g, '');
const f = (v) => `${v.toFixed(2)}f`;
const names = [...areasW.keys()].sort();
let h = '/* generated by tools/prep-lab.mjs from the HSL-108 model: do not edit */\n#ifndef LAB_AREAS_H\n#define LAB_AREAS_H\n';
h += `#define LAB_DOOR_Z ${DOOR_Z}f\n#define LAB_DOOR_X ${DOOR_X}f\n#define LAB_AREA_N ${names.length}\n`;
h += '/* min and max: world metres; pmin and pmax: plan metres (x from the street, y up, z from the west wall) */\n';
h += 'typedef struct { const char *name; float min[3], max[3], pmin[3], pmax[3]; } lab_area_t;\nstatic const lab_area_t LAB_AREAS[LAB_AREA_N] = {\n';
for (const a of names) { const w = areasW.get(a), b = areasP.get(a); h += `  {"${a}", {${w.slice(0, 3).map(f).join(', ')}}, {${w.slice(3).map(f).join(', ')}}, {${b.slice(0, 3).map(f).join(', ')}}, {${b.slice(3).map(f).join(', ')}}},\n`; }
h += '};\n';
h += `/* seats (generated from the model's chairs, stools and sofa): world x, z, the seat's height, the world yaw a sitter faces, and soft (a sofa or a lounge chair) */\n#define LAB_SEAT_N ${seats.length}\n`;
h += 'static const struct { float x, z, top, yaw; int soft; } LAB_SEATS[LAB_SEAT_N] = {\n';
for (const q of seats) h += `  {${f(q.wx)}, ${f(q.wz)}, ${f(q.top)}, ${f(q.wyaw)}, ${/^(Sofa|Camp)/.test(q.name) ? 1 : 0}},   /* ${q.name}, plan ${q.cx.toFixed(2)} ${q.cz.toFixed(2)} */\n`;
h += '};\nenum {\n'; names.forEach((a, i) => { h += `  AREA_${id(a)} = ${i},\n`; }); h += '};\n#endif\n';
mkdirSync(join(ROOT, 'src/gen'), { recursive: true });
writeFileSync(join(ROOT, 'src/gen/lab_areas.h'), h);
let verts = 0; for (const c of list) verts += c.nv;
console.log(`lab: ${list.length} chunks, ${verts} verts, ${r.tris} tris, ${(r.bytes / 1e6).toFixed(2)} MB; ${printers.size} printers boxed, ${droppedT} tris dropped, ${names.length} areas`);
for (const c of list) console.log(`  ${c.name.padEnd(18)} ${c.kind.padEnd(7)} ${c.tris}`);
