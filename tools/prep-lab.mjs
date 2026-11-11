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
// so they are dropped; an original low poly printer of the same size stands in each one's place.
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

// printers. The model's Elegoo Centauri Carbon 2 meshes come from a GPL-3.0 CAD release and stay out; in
// each one's exact box stands an original low poly printer drawn here from photos of the real one: an
// enclosed CoreXY cube on four feet, black frame and side panels, a tinted glass door with a handle and a
// tinted glass lid, the touchscreen glowing faintly in the bezel under the door, the bed and the toolhead on
// its gantry seen through the glass, the multicolour feeder on top and a spool on the side. Some 400
// triangles each. One of them, Brenda (#2, top shelf, toward the back), is part way through a print.
// The far twin keeps the plain cabinet. Plan metres: x from the street, y up, z from the west wall; the
// printers stand against the west wall, so their fronts face plan +z.
{
  const pbox = (c, px0, y0, pz0, px1, y1, pz1, rgb) => c.box(pz0 - DOOR_Z, y0, -px1, pz1 - DOOR_Z, y1, -px0, rgb);
  // an n sided drum along plan x (a spool): side quads and two fan caps, wound outward
  const drum = (c, xa, xb, cy, cz, r, rgb, n = 8) => {
    const pos = [], idx = [];
    for (let i = 0; i < n; i++) { const a = (i / n) * Math.PI * 2; pos.push(xa, cy + r * Math.cos(a), cz + r * Math.sin(a), xb, cy + r * Math.cos(a), cz + r * Math.sin(a)); }
    for (let i = 0; i < n; i++) { const j = (i + 1) % n, A = i * 2, B = j * 2; idx.push(A, B, B + 1, A, B + 1, A + 1); }
    for (let i = 1; i + 1 < n; i++) { idx.push(1, i * 2 + 1, (i + 1) * 2 + 1); idx.push(0, (i + 1) * 2, i * 2); }
    c.add(Float32Array.from(pos), idx, rgb, toWorld);
  };
  const BLACK = srgb(0x1a1b1e), FRAME = srgb(0x2b2d31), PANEL = srgb(0x232428), STEEL = srgb(0x9aa0a6), SILVER = srgb(0xc4c8cc);
  const TINT = srgb(0x3a3630), PLATE = srgb(0x6a5f4c), TOOL = srgb(0x303236), TOOLF = srgb(0xd8d8d4), CANVAS = srgb(0xb8bbbe), SCREEN = srgb(0x3d6f96);
  const FIL = [srgb(0xe8e8e2), srgb(0xd8691e), srgb(0x2a63b8), srgb(0x2c2c2c)];
  const tint = ch('lab_glass_tint', 'glass'), glow = ch('lab_glow_f', 'glow');
  [...printers.entries()].sort((a, b) => (a[0] < b[0] ? -1 : 1)).forEach(([name, b], n) => {
    const c = ch(band((b[0] + b[3]) / 2), 'opaque');
    const brenda = /#2$/.test(name);
    const [x0, y0, z0, x1, y1, z1] = b, H = y1 - y0;
    const bx0 = x0, bx1 = x1 - 0.06, bz0 = z0 + 0.02, bz1 = z1 - 0.01, yb = y0 + 0.025, yt = y0 + H * 0.8, F = 0.022;
    // feet, the base with its bezel, the screen, a light logo bar
    for (const fx of [bx0 + 0.03, bx1 - 0.07]) for (const fz of [bz0 + 0.03, bz1 - 0.07]) pbox(c, fx, y0, fz, fx + 0.04, yb, fz + 0.04, BLACK);
    pbox(c, bx0, yb, bz0, bx1, yb + 0.07, bz1, BLACK);
    pbox(glow, bx1 - 0.16, yb + 0.012, bz1 + 0.001, bx1 - 0.035, yb + 0.06, bz1 + 0.004, SCREEN);
    pbox(c, bx0 + 0.04, yb + 0.03, bz1, bx0 + 0.13, yb + 0.04, bz1 + 0.003, STEEL);
    // the frame: four posts, the top rim; side and back panels
    for (const [px, pz] of [[bx0, bz0], [bx1 - F, bz0], [bx0, bz1 - F], [bx1 - F, bz1 - F]]) pbox(c, px, yb + 0.07, pz, px + F, yt, pz + F, FRAME);
    pbox(c, bx0, yt - 0.02, bz0, bx1, yt, bz0 + F, FRAME); pbox(c, bx0, yt - 0.02, bz1 - F, bx1, yt, bz1, FRAME);
    pbox(c, bx0, yt - 0.02, bz0, bx0 + F, yt, bz1, FRAME); pbox(c, bx1 - F, yt - 0.02, bz0, bx1, yt, bz1, FRAME);
    pbox(c, bx0, yb + 0.07, bz0 + F, bx0 + 0.008, yt - 0.02, bz1 - F, PANEL);
    pbox(c, bx1 - 0.008, yb + 0.07, bz0 + F, bx1, yt - 0.02, bz1 - F, PANEL);
    pbox(c, bx0 + F, yb + 0.07, bz0, bx1 - F, yt - 0.02, bz0 + 0.008, PANEL);
    // the tinted glass: lid and door, the door's thin frame and its handle
    pbox(tint, bx0 + F, yt - 0.008, bz0 + F, bx1 - F, yt - 0.002, bz1 - F, TINT);
    pbox(tint, bx0 + F, yb + 0.075, bz1 - 0.006, bx1 - F, yt - 0.02, bz1 - 0.001, TINT);
    pbox(c, bx0 + F, yt - 0.035, bz1 - 0.004, bx1 - F, yt - 0.02, bz1 + 0.002, FRAME);
    pbox(c, bx0 + F, yb + 0.07, bz1 - 0.004, bx1 - F, yb + 0.085, bz1 + 0.002, FRAME);
    pbox(c, bx1 - 0.055, yb + 0.2, bz1 + 0.002, bx1 - 0.043, yb + 0.36, bz1 + 0.02, SILVER);
    // inside: the gantry near the top (two rails front to back, the beam across), the toolhead on it
    const yg = yt - 0.07, zx = (bz0 + bz1) / 2 + (brenda ? 0.02 : -0.03), xt = brenda ? (bx0 + bx1) / 2 + 0.01 : bx0 + 0.11 + n * 0.03;
    pbox(c, bx0 + F, yg, bz0 + F, bx0 + F + 0.014, yg + 0.02, bz1 - F, STEEL);
    pbox(c, bx1 - F - 0.014, yg, bz0 + F, bx1 - F, yg + 0.02, bz1 - F, STEEL);
    pbox(c, bx0 + F, yg - 0.005, zx - 0.012, bx1 - F, yg + 0.02, zx + 0.012, FRAME);
    pbox(c, xt - 0.035, yg - 0.06, zx - 0.03, xt + 0.035, yg + 0.025, zx + 0.035, TOOL);
    pbox(c, xt - 0.03, yg - 0.05, zx + 0.035, xt + 0.03, yg + 0.015, zx + 0.04, TOOLF);
    pbox(c, xt - 0.006, yg - 0.075, zx - 0.006, xt + 0.006, yg - 0.06, zx + 0.006, STEEL);
    // the bed on its carriage and the Z rods behind it; Brenda's is up under the nozzle with a print on it
    const printH = 0.075, yp = brenda ? yg - 0.075 - printH - 0.012 : yb + 0.13;
    pbox(c, bx0 + 0.06, yp, bz0 + 0.06, bx1 - 0.06, yp + 0.012, bz1 - 0.05, PLATE);
    pbox(c, bx0 + 0.09, yp - 0.025, bz0 + 0.03, bx1 - 0.09, yp, bz0 + 0.07, FRAME);
    for (const rx of [bx0 + 0.08, bx1 - 0.09]) pbox(c, rx, yb + 0.07, bz0 + 0.03, rx + 0.01, yg, bz0 + 0.04, STEEL);
    if (brenda) {
      // a vase part way up: stepped rings of orange, the open top layer under the nozzle
      const cx = xt, cz = zx + 0.002, ORANGE = srgb(0xe2731f);
      for (let k = 0; k < 4; k++) {
        const r = 0.045 - k * 0.006, ya = yp + 0.012 + (k * printH) / 4, yb2 = ya + printH / 4;
        pbox(c, cx - r, ya, cz - r, cx + r, yb2, cz + r, ORANGE);
      }
    }
    // the feeder on top toward the right, its tube, and the spool on the right side toward the back
    pbox(c, bx1 - 0.2, yt, bz0 + 0.06, bx1 - 0.03, yt + 0.1, bz0 + 0.3, CANVAS);
    pbox(c, bx1 - 0.19, yt + 0.1, bz0 + 0.07, bx1 - 0.04, yt + 0.11, bz0 + 0.29, FRAME);
    pbox(c, bx1 - 0.12, yt + 0.11, bz0 + 0.17, bx1 - 0.11, yt + 0.16, bz0 + 0.18, SILVER);
    const sr = Math.min(0.1, (y1 - y0) * 0.16), scy = y0 + H * 0.45, scz = bz0 + 0.16;
    drum(c, bx1 + 0.004, x1 - 0.004, scy, scz, sr, FIL[(n + (brenda ? 1 : 0)) % 4]);
    drum(c, bx1, x1, scy, scz, 0.028, BLACK, 6);
    pbox(c, bx1 - 0.005, scy - 0.01, scz - 0.01, bx1 + 0.004, scy + 0.01, scz + 0.01, FRAME);
    // the far twin: the cabinet and its door, as before
    const w = worldBox(b), fc = ch(`${c.name}_far`, 'opaque');
    fc.box(...w, srgb(0x2e3034));
    fc.box(w[0] + 0.03, w[1] + (w[4] - w[1]) * 0.25, w[2] + 0.04, w[3] - 0.03, w[4] - 0.05, w[2] - 0.005, srgb(0x96a0aa));
  });
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
console.log(`lab: ${list.length} chunks, ${verts} verts, ${r.tris} tris, ${(r.bytes / 1e6).toFixed(2)} MB; ${printers.size} printers drawn, ${droppedT} tris dropped, ${names.length} areas`);
for (const c of list) console.log(`  ${c.name.padEnd(18)} ${c.kind.padEnd(7)} ${c.tris}`);
