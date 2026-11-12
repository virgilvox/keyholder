// The attic's building kit, shared by tools/prep-attic.mjs (the blueprint room), tools/prep-attic-deep.mjs (the
// nightmare attic) and tools/prep-memory-desert.mjs: lumber, boxes, 2x4 and wire shelving, totes, cardboard, the
// rack, CC0 props baked to vertex colour, spray paint, the palette and a cheap corner shading pass.
//
// The builders draw into the chunks named by into(lit, glow, paint): lit for lambert surfaces, glow for bulbs and
// LEDs (the runtime draws chunks named glow_* unlit), paint for spray marks (into lit when not given). Collision
// boxes go to kit.solids. One seeded random stream per process, so a tool's output is the same on every run.
import { readProp } from './gltf-read.mjs';
import { Chunk, srgb } from './glb-lib.mjs';

export const LIB = '/Users/obsidian/Projects/heatsync/3d-assets';
export const K = (n) => `${LIB}/kenney/furniture-kit/Models/GLTF format/${n}.glb`;
export const HOL = (n) => `${LIB}/kenney/holiday-kit/Models/GLB format/${n}.glb`;
export const HAL = (n) => `${LIB}/kaykit/halloween-bits/Assets/gltf/${n}.gltf`;
export const TOOL = (n) => `${LIB}/opengameart/low-poly-tools/gltf/${n}.gltf`;

// ---------------------------------------------------------------- where the builders draw
export const kit = { lit: null, glow: null, paint: null, solids: [] };
export function into(lit, glow, paint = null) { kit.lit = lit; kit.glow = glow; kit.paint = paint; }
export const solid = (x0, z0, x1, z1, top) => kit.solids.push({ x0: Math.min(x0, x1), z0: Math.min(z0, z1), x1: Math.max(x0, x1), z1: Math.max(z0, z1), top });

// ---------------------------------------------------------------- random and colour
let seed = 1234567;
export const setSeed = (s) => { seed = s >>> 0; };
export const rnd = () => { seed = (seed * 1664525 + 1013904223) >>> 0; return seed / 4294967296; };
export const R = (a, b) => a + (b - a) * rnd();
export const pick = (a) => a[Math.floor(rnd() * a.length)];
export const lin = (hex) => srgb(hex);
// colours are hex sRGB or arrays of linear rgb; these take either
export const col = (c) => (Array.isArray(c) ? c : lin(c));
export const vary = (hex, amt = 0.08) => { const c = col(hex), f = 1 + R(-amt, amt); return c.map((v) => Math.max(0, Math.min(1, v * f))); };
export const mix = (a, b, t) => { const A = col(a), B = col(b); return A.map((v, i) => v + (B[i] - v) * t); };

// ---------------------------------------------------------------- palette
export const PLY = [0xb89a6a, 0xa98c5e, 0xc2a676, 0x9c8156];   // plywood sheets
export const BOARD = [0x7a5f3e, 0x86694a, 0x6e5536];           // old boards
export const STUD = 0xc9ad7c, STUD_OLD = 0x8f7350;             // new 2x4, old framing
export const OSB = 0x9a7e52, PINK = 0xe59aa4, PINK_D = 0xc77f8a, PAPER = 0xb59a6a;
export const STEEL = 0x8f979e, CHROME = 0xb8c0c6, BLACK = 0x1d2024, BEIGE = 0xcfc4a6;
export const ORANGE_PAINT = 0xff5a14, RED_PAINT = 0xe0201c, PINK_PAINT = 0xff3c8c;
export const LIDS = [0x2f6fd0, 0xc02020, 0x208040, 0xe0c020, 0x2a2a2a, 0x8040c0, 0x30a0d0];   // tote lids, every colour

// ---------------------------------------------------------------- primitives
// an axis aligned box
export function box(ch, x0, y0, z0, x1, y1, z1, c) { ch.box(Math.min(x0, x1), Math.min(y0, y1), Math.min(z0, z1), Math.max(x0, x1), Math.max(y0, y1), Math.max(z0, z1), col(c)); }
// a box at a centre, turned by yaw about y
export function obox(ch, cx, cy, cz, hx, hy, hz, yaw, c) {
  const cs = Math.cos(yaw), sn = Math.sin(yaw);
  const p = [];
  for (const [x, y, z] of [[-1, -1, -1], [1, -1, -1], [1, 1, -1], [-1, 1, -1], [-1, -1, 1], [1, -1, 1], [1, 1, 1], [-1, 1, 1]]) {
    const lx = x * hx, lz = z * hz;
    p.push(cx + lx * cs + lz * sn, cy + y * hy, cz - lx * sn + lz * cs);
  }
  ch.add(Float32Array.from(p), [0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4, 3, 7, 6, 3, 6, 2, 0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5], col(c));
}
// a length of lumber (or anything square) from a to b, w across and d deep
export function beam(ch, a, b, w, d, c) {
  const dx = b[0] - a[0], dy = b[1] - a[1], dz = b[2] - a[2], L = Math.hypot(dx, dy, dz) || 1;
  const f = [dx / L, dy / L, dz / L];
  let s = [f[2], 0, -f[0]]; let sl = Math.hypot(...s);
  if (sl < 1e-4) { s = [1, 0, 0]; sl = 1; }
  s = s.map((v) => v / sl);
  const u = [s[1] * f[2] - s[2] * f[1], s[2] * f[0] - s[0] * f[2], s[0] * f[1] - s[1] * f[0]];
  const P = [];
  for (const e of [a, b]) for (const [i, j] of [[-1, -1], [1, -1], [1, 1], [-1, 1]]) P.push(...[0, 1, 2].map((k) => e[k] + s[k] * i * w / 2 + u[k] * j * d / 2));
  ch.add(Float32Array.from(P), [0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 0, 4, 5, 0, 5, 1, 1, 5, 6, 1, 6, 2, 2, 6, 7, 2, 7, 3, 3, 7, 4, 3, 4, 0], col(c));
}
// a cylinder from a to b
export function cyl(ch, a, b, r, seg, c, caps = true) {
  const dx = b[0] - a[0], dy = b[1] - a[1], dz = b[2] - a[2], L = Math.hypot(dx, dy, dz) || 1;
  const f = [dx / L, dy / L, dz / L];
  let s = Math.abs(f[1]) > 0.9 ? [1, 0, 0] : [f[2], 0, -f[0]]; const sl = Math.hypot(...s); s = s.map((v) => v / sl);
  const u = [s[1] * f[2] - s[2] * f[1], s[2] * f[0] - s[0] * f[2], s[0] * f[1] - s[1] * f[0]];
  const P = [], I = [];
  for (const e of [a, b]) for (let i = 0; i < seg; i++) { const t = (i / seg) * Math.PI * 2, cs = Math.cos(t) * r, sn = Math.sin(t) * r; P.push(...[0, 1, 2].map((k) => e[k] + s[k] * cs + u[k] * sn)); }
  for (let i = 0; i < seg; i++) { const j = (i + 1) % seg; I.push(i, j, seg + j, i, seg + j, seg + i); }
  if (caps) { P.push(...a, ...b); const ca = seg * 2, cb = ca + 1; for (let i = 0; i < seg; i++) { const j = (i + 1) % seg; I.push(ca, j, i, cb, seg + i, seg + j); } }
  ch.add(Float32Array.from(P), I, col(c));
}
export function ball(ch, cx, cy, cz, r, c, seg = 6) {
  const P = [], I = [];
  for (let i = 0; i <= seg; i++) { const v = (i / seg) * Math.PI; for (let j = 0; j < seg * 2; j++) { const u = (j / (seg * 2)) * Math.PI * 2; P.push(cx + Math.sin(v) * Math.cos(u) * r, cy + Math.cos(v) * r, cz + Math.sin(v) * Math.sin(u) * r); } }
  const W = seg * 2;
  for (let i = 0; i < seg; i++) for (let j = 0; j < W; j++) { const a = i * W + j, b = i * W + ((j + 1) % W), c2 = a + W, d = b + W; I.push(a, b, d, a, d, c2); }
  ch.add(Float32Array.from(P), I, col(c));
}
export function quad(ch, a, b, c, d, cc) { ch.quad(a, b, c, d, col(cc)); }

// a stand in for a chunk that moves everything drawn into it by xf(x, y, z) -> [x, y, z] first (a leaning shelf)
// (stand ins nest: box and quad go through this add, so every transform on the way down applies)
export function moved(ch, xf) {
  const p = {
    add: (pos, idx, rgb, xf2) => ch.add(pos, idx, rgb, (x, y, z) => (xf2 ? xf(...xf2(x, y, z)) : xf(x, y, z))),
    box(x0, y0, z0, x1, y1, z1, rgb) { Chunk.prototype.box.call(p, x0, y0, z0, x1, y1, z1, rgb); },
    quad(a, b, c, d, rgb) { Chunk.prototype.quad.call(p, a, b, c, d, rgb); },
  };
  return p;
}

// a CC0 prop, scaled so its height (or o.w its width) is the given size, standing at y on its own base
const propCache = new Map();
export async function prop(file, x, y, z, o = {}) {
  if (!propCache.has(file)) propCache.set(file, await readProp(file));
  const m = propCache.get(file);
  const b = m.box, mh = b[4] - b[1], mw = Math.max(b[3] - b[0], b[5] - b[2]);
  const s = o.h ? o.h / mh : o.w ? o.w / mw : o.s || 1;
  const yaw = o.yaw || 0, cs = Math.cos(yaw), sn = Math.sin(yaw);
  const cx = (b[0] + b[3]) / 2, cz = (b[2] + b[5]) / 2;
  const xf = (px, py, pz) => { const lx = (px - cx) * s, lz = (pz - cz) * s; return [x + lx * cs + lz * sn, y + (py - b[1]) * s, z - lx * sn + lz * cs]; };
  // one add per face colour, so a colour atlas keeps its flat swatches
  const groups = new Map();
  for (let t = 0; t < m.idx.length / 3; t++) {
    let c = [m.col[t * 3], m.col[t * 3 + 1], m.col[t * 3 + 2]];
    if (o.tint) { const T = lin(o.tint); c = c.map((v, i) => v * (1 - o.tintAmt) + T[i] * o.tintAmt); }
    if (o.dim) c = c.map((v) => v * o.dim);
    const key = c.map((v) => Math.round(v * 255)).join(',');
    if (!groups.has(key)) groups.set(key, { c, I: [] });
    groups.get(key).I.push(m.idx[t * 3], m.idx[t * 3 + 1], m.idx[t * 3 + 2]);
  }
  const into2 = o.into || (o.glow ? kit.glow : kit.lit);
  for (const g of groups.values()) into2.add(m.pos, g.I, g.c, xf);
  return { w: mw * s, h: mh * s };
}

// ---------------------------------------------------------------- shelving
// 2x4 shelving along a wall: uprights at each end and every ~1.2 m, plywood shelves at the given heights.
// face: the way the shelf opens ('n' 's' 'e' 'w'). Returns shelf boxes for the contents.
export function shelving2x4(x0, z0, x1, z1, heights, face, o = {}) {
  const room = kit.lit;
  const along = face === 'n' || face === 's' ? 'x' : 'z';
  const len = along === 'x' ? x1 - x0 : z1 - z0;
  const n = Math.max(2, Math.round(len / 1.2) + 1);
  const top = heights[heights.length - 1] + 0.05;
  const slots = [];
  for (let i = 0; i < n; i++) {
    const t = i / (n - 1);
    const px = along === 'x' ? x0 + (x1 - x0) * t : null, pz = along === 'z' ? z0 + (z1 - z0) * t : null;
    const ends = along === 'x' ? [[px, z0 + 0.03], [px, z1 - 0.03]] : [[x0 + 0.03, pz], [x1 - 0.03, pz]];
    for (const [ex, ez] of ends) beam(room, [ex, 0, ez], [ex, top, ez], 0.038, 0.089, vary(o.wood || STUD));
  }
  for (const h of heights) {
    // the ledgers under the shelf, then the plywood
    if (along === 'x') { beam(room, [x0, h - 0.06, z0 + 0.04], [x1, h - 0.06, z0 + 0.04], 0.038, 0.089, vary(o.wood || STUD)); beam(room, [x0, h - 0.06, z1 - 0.04], [x1, h - 0.06, z1 - 0.04], 0.038, 0.089, vary(o.wood || STUD)); }
    else { beam(room, [x0 + 0.04, h - 0.06, z0], [x0 + 0.04, h - 0.06, z1], 0.038, 0.089, vary(o.wood || STUD)); beam(room, [x1 - 0.04, h - 0.06, z0], [x1 - 0.04, h - 0.06, z1], 0.038, 0.089, vary(o.wood || STUD)); }
    box(room, x0, h - 0.018, z0, x1, h, z1, vary(o.deck || PLY[0], 0.08));
    slots.push({ x0, z0, x1, z1, y: h });
  }
  slots.unshift({ x0, z0, x1, z1, y: 0 });   // the floor under the bottom shelf is used too
  solid(x0, z0, x1, z1, top);
  return { slots, face, along };
}
// steel wire shelving: four chrome posts, wire decks
export function wireShelf(x0, z0, x1, z1, heights) {
  const room = kit.lit;
  const top = heights[heights.length - 1] + 0.05;
  for (const [x, z] of [[x0, z0], [x1, z0], [x0, z1], [x1, z1]]) cyl(room, [x + (x < 0.5 * (x0 + x1) ? 0.02 : -0.02), 0, z + (z < 0.5 * (z0 + z1) ? 0.02 : -0.02)], [x + (x < 0.5 * (x0 + x1) ? 0.02 : -0.02), top, z + (z < 0.5 * (z0 + z1) ? 0.02 : -0.02)], 0.014, 6, CHROME);
  const slots = [{ x0, z0, x1, z1, y: 0 }];
  for (const h of heights) {
    box(room, x0, h - 0.012, z0, x1, h, z1, 0x9aa2a8);
    for (let x = x0 + 0.05; x < x1; x += 0.08) box(room, x - 0.004, h, z0, x + 0.004, h + 0.006, z1, CHROME);
    slots.push({ x0, z0, x1, z1, y: h });
  }
  solid(x0, z0, x1, z1, top);
  return { slots };
}

// ---------------------------------------------------------------- things to fill shelves with
export function tote(cx, y, cz, w, d, h, lid, yaw = 0, o = {}) {
  const room = kit.lit;
  const body = o.clear ? 0xc8d2d6 : o.body || 0x4a4d52;
  obox(room, cx, y + h * 0.45, cz, w / 2 * 0.94, h * 0.45, d / 2 * 0.94, yaw, vary(body, 0.05));
  obox(room, cx, y + h * 0.93, cz, w / 2, h * 0.07, d / 2, yaw, vary(lid, 0.06));
  if (o.clear) obox(room, cx, y + h * 0.4, cz, w / 2 * 0.86, h * 0.3, d / 2 * 0.86, yaw, vary(o.inside || 0x7a6a58, 0.15));   // what is inside, seen through the plastic
  if (o.label !== false) {   // a strip of masking tape with a marker scrawl, on the front
    const cs = Math.cos(yaw), sn = Math.sin(yaw), fx = sn * (d / 2 * 0.95 + 0.004), fz = cs * (d / 2 * 0.95 + 0.004);
    obox(room, cx + fx, y + h * 0.62, cz + fz, w * 0.22, 0.03, 0.003, yaw, 0xe8dcb0);
    obox(room, cx + fx * 1.01, y + h * 0.62, cz + fz * 1.01, w * 0.15, 0.006, 0.002, yaw, 0x1a1a1a);
  }
  if (o.redtag) { const cs = Math.cos(yaw), sn = Math.sin(yaw); obox(room, cx + sn * (d / 2 + 0.006) + cs * w * 0.3, y + h * 0.8, cz + cs * (d / 2 + 0.006) - sn * w * 0.3, 0.035, 0.05, 0.002, yaw, RED_PAINT); }
}
export function cardboard(cx, y, cz, w, d, h, yaw = 0, open = false) {
  const room = kit.lit;
  const c = vary(0xb08655, 0.1);
  obox(room, cx, y + h / 2, cz, w / 2, h / 2, d / 2, yaw, c);
  if (open) { const cs = Math.cos(yaw), sn = Math.sin(yaw); obox(room, cx - sn * d * 0.5, y + h + 0.06, cz - cs * d * 0.5, w / 2 * 0.98, 0.06, 0.003, yaw, c); }
  else obox(room, cx, y + h + 0.001, cz, w / 2 * 0.06, 0.002, d / 2, yaw, 0xc9b27c);   // packing tape
}
export const yawFor = (face) => ({ s: 0, n: Math.PI, e: Math.PI / 2, w: -Math.PI / 2 }[face]);

// ---------------------------------------------------------------- the rack and the pizza box
// A standard 19 inch rack centred at (x, z), its front facing south (+z). Returns where to stand at the terminal.
export const RACK_W = 0.6, RACK_D = 1.0, RACK_H = 1.98;
export function rack(x, z) {
  const room = kit.lit, glow = kit.glow;
  const xa = x - RACK_W / 2, xb = x + RACK_W / 2, za = z - RACK_D / 2, zb = z + RACK_D / 2;
  // frame and side panels (front faces south, into the room)
  box(room, xa, 0.05, za, xa + 0.02, RACK_H, zb, 0x24272c);
  box(room, xb - 0.02, 0.05, za, xb, RACK_H, zb, 0x24272c);
  box(room, xa, RACK_H - 0.04, za, xb, RACK_H, zb, 0x1e2125);
  box(room, xa, 0.05, za, xb, 0.1, zb, 0x1e2125);
  for (const [cx, cz] of [[xa + 0.04, za + 0.04], [xb - 0.04, za + 0.04], [xa + 0.04, zb - 0.04], [xb - 0.04, zb - 0.04]]) cyl(room, [cx, 0, cz], [cx, 0.05, cz], 0.025, 6, 0x111111);
  // rails
  for (const rx of [xa + 0.05, xb - 0.05]) box(room, rx - 0.01, 0.1, zb - 0.06, rx + 0.01, RACK_H - 0.04, zb - 0.04, 0x55595f);
  // the gear, from the bottom: a UPS, 2U servers, a switch with patch cables, 1U servers, a blank, then the shelf
  const U = 0.0445;
  let y = 0.1;
  const unit = (n, face, body = 0x2a2d32) => {
    const h = n * U;
    box(room, xa + 0.03, y + 0.002, za + 0.05, xb - 0.03, y + h - 0.002, zb - 0.05, body);
    box(room, xa + 0.04, y + 0.004, zb - 0.05, xb - 0.04, y + h - 0.004, zb - 0.035, face);
    const yy = y; y += h; return yy;
  };
  const ups = unit(4, 0x2c2f33); box(glow, xa + 0.1, ups + 0.08, zb - 0.034, xa + 0.16, ups + 0.11, zb - 0.033, 0x3fd06a);
  for (let i = 0; i < 3; i++) { const yy = unit(2, i === 1 ? 0xb9b4a6 : 0x3a3e45); for (let k = 0; k < 4; k++) box(room, xa + 0.08 + k * 0.1, yy + 0.015, zb - 0.034, xa + 0.16 + k * 0.1, yy + 0.07, zb - 0.032, 0x222428); box(glow, xb - 0.1, yy + 0.05, zb - 0.034, xb - 0.09, yy + 0.06, zb - 0.033, i === 2 ? 0xffa020 : 0x40d070); }
  const sw = unit(1, 0x30343a);
  for (let k = 0; k < 16; k++) { box(glow, xa + 0.07 + k * 0.028, sw + 0.03, zb - 0.034, xa + 0.08 + k * 0.028, sw + 0.036, zb - 0.033, k % 3 ? 0x40d070 : 0xffb030); }
  // patch cables looping down out of the switch
  for (let k = 0; k < 9; k++) { const cx = xa + 0.08 + k * 0.05, cc = [0x2f6fd0, 0xe0c020, 0xd03030, 0x30a050][k % 4]; cyl(room, [cx, sw + 0.02, zb - 0.03], [cx + R(-0.05, 0.05), sw - 0.12 - R(0, 0.1), zb + 0.02], 0.004, 4, cc, false); }
  for (let i = 0; i < 4; i++) { const yy = unit(1, 0x3b4048); box(glow, xb - 0.1, yy + 0.02, zb - 0.034, xb - 0.09, yy + 0.026, zb - 0.033, 0x40d070); }
  unit(2, 0x18191c);
  // the pull out shelf at 1.12 m: the pizza box (a low flat workstation), its CRT on top, the keyboard on the slide
  const sy = Math.max(y + 0.02, 1.08);
  box(room, xa + 0.03, sy - 0.02, za + 0.15, xb - 0.03, sy, zb + 0.22, 0x2b2e33);
  box(room, xa + 0.06, sy, za + 0.3, xb - 0.06, sy + 0.075, zb - 0.06, 0x5c5f63);           // the pizza box
  box(room, xa + 0.07, sy + 0.01, zb - 0.061, xb - 0.07, sy + 0.065, zb - 0.058, 0x45484c);
  box(glow, xa + 0.1, sy + 0.03, zb - 0.057, xa + 0.13, sy + 0.04, zb - 0.056, 0x40d070);
  box(room, xa + 0.11, sy + 0.075, za + 0.38, xb - 0.11, sy + 0.42, zb - 0.12, 0x52555a);   // the CRT's case
  box(room, xa + 0.13, sy + 0.1, zb - 0.12, xb - 0.13, sy + 0.4, zb - 0.1, 0x3a3d41);      // bezel
  box(glow, xa + 0.15, sy + 0.12, zb - 0.099, xb - 0.15, sy + 0.38, zb - 0.098, 0x1f6a3a);  // the phosphor, dim green
  box(room, xa + 0.07, sy, zb + 0.02, xb - 0.07, sy + 0.025, zb + 0.2, 0x45484c);           // keyboard
  for (let r = 0; r < 4; r++) for (let k = 0; k < 9; k++) box(room, xa + 0.09 + k * 0.047, sy + 0.025, zb + 0.04 + r * 0.04, xa + 0.125 + k * 0.047, sy + 0.035, zb + 0.07 + r * 0.04, 0x6a6d72);
  // the remaining units above the shelf: dead 1U boxes
  for (let yy = sy + 0.48; yy + U * 2 < RACK_H - 0.06; yy += U * 2) box(room, xa + 0.03, yy, za + 0.05, xb - 0.03, yy + U * 2 - 0.004, zb - 0.04, 0x26292d);
  solid(xa, za, xb, zb + 0.2, RACK_H);
  return { terminal: [x, 0, zb + 0.75] };
}

// ---------------------------------------------------------------- spray paint on the floor
// A stroke font for the words the lab paints on bad boards. Each glyph is strokes in a 0..1 box.
export const GLYPH = {
  B: [[0, 0, 0, 1], [0, 1, 0.7, 1], [0.7, 1, 0.8, 0.8], [0.8, 0.8, 0.7, 0.55], [0, 0.5, 0.7, 0.55], [0.7, 0.55, 0.85, 0.3], [0.85, 0.3, 0.7, 0], [0.7, 0, 0, 0]],
  A: [[0, 0, 0.45, 1], [0.45, 1, 0.9, 0], [0.18, 0.4, 0.72, 0.4]],
  D: [[0, 0, 0, 1], [0, 1, 0.55, 1], [0.55, 1, 0.85, 0.6], [0.85, 0.6, 0.85, 0.4], [0.85, 0.4, 0.55, 0], [0.55, 0, 0, 0]],
  N: [[0, 0, 0, 1], [0, 1, 0.8, 0], [0.8, 0, 0.8, 1]],
  O: [[0.4, 0, 0.8, 0.3], [0.8, 0.3, 0.8, 0.7], [0.8, 0.7, 0.4, 1], [0.4, 1, 0, 0.7], [0, 0.7, 0, 0.3], [0, 0.3, 0.4, 0]],
  S: [[0.8, 0.9, 0.4, 1], [0.4, 1, 0, 0.8], [0, 0.8, 0.1, 0.55], [0.1, 0.55, 0.7, 0.45], [0.7, 0.45, 0.8, 0.2], [0.8, 0.2, 0.4, 0], [0.4, 0, 0, 0.1]],
  F: [[0, 0, 0, 1], [0, 1, 0.8, 1], [0, 0.55, 0.6, 0.55]],
  T: [[0, 1, 0.9, 1], [0.45, 1, 0.45, 0]],
  P: [[0, 0, 0, 1], [0, 1, 0.65, 1], [0.65, 1, 0.8, 0.8], [0.8, 0.8, 0.65, 0.55], [0.65, 0.55, 0, 0.55]],
  K: [[0, 0, 0, 1], [0, 0.45, 0.8, 1], [0.25, 0.62, 0.85, 0]],
  E: [[0.8, 0, 0, 0], [0, 0, 0, 1], [0, 1, 0.8, 1], [0, 0.52, 0.6, 0.52]],
  R: [[0, 0, 0, 1], [0, 1, 0.65, 1], [0.65, 1, 0.8, 0.8], [0.8, 0.8, 0.65, 0.55], [0.65, 0.55, 0, 0.55], [0.4, 0.55, 0.85, 0]],
  L: [[0, 1, 0, 0], [0, 0, 0.75, 0]],
  U: [[0, 1, 0, 0.25], [0, 0.25, 0.35, 0], [0.35, 0, 0.55, 0], [0.55, 0, 0.85, 0.25], [0.85, 0.25, 0.85, 1]],
  I: [[0.4, 0, 0.4, 1]],
  X: [[0, 0, 0.85, 1], [0, 1, 0.85, 0]],
  H: [[0, 0, 0, 1], [0.8, 0, 0.8, 1], [0, 0.5, 0.8, 0.5]],
  Y: [[0, 1, 0.42, 0.5], [0.85, 1, 0.42, 0.5], [0.42, 0.5, 0.42, 0]],
};
// a spray line: a crisp core over a soft halo blended into the floor
export function stroke(x0, z0, x1, z1, w, c, halo, y = 0) {
  const ch = kit.paint || kit.lit;
  const dx = x1 - x0, dz = z1 - z0, L = Math.hypot(dx, dz) || 1, nx = -dz / L, nz = dx / L;
  const q = (ww, yy, cc, ext) => quad(ch, [x0 + nx * ww - dx / L * ext, yy, z0 + nz * ww - dz / L * ext], [x1 + nx * ww + dx / L * ext, yy, z1 + nz * ww + dz / L * ext], [x1 - nx * ww + dx / L * ext, yy, z1 - nz * ww + dz / L * ext], [x0 - nx * ww - dx / L * ext, yy, z0 - nz * ww - dz / L * ext], cc);
  q(w * 1.9, y + 0.003, halo, w * 0.6);
  q(w / 2, y + 0.005, c, 0);
}
export function sprayX(x, z, s, hex, floorHex = PLY[0], y = 0, rot = 0) {
  const halo = mix(hex, floorHex, 0.62), c = vary(hex, 0.04);
  if (!rot) { stroke(x - s, z - s, x + s, z + s, 0.05, c, halo, y); stroke(x - s, z + s, x + s, z - s, 0.05, c, halo, y); return; }
  const a = Math.cos(rot) * s, b = Math.sin(rot) * s;   // turned by rot: sprayed by hand, not square to the boards
  stroke(x - a + b, z - a - b, x + a - b, z + a + b, 0.05, c, halo, y);
  stroke(x - a - b, z + a - b, x + a + b, z - a + b, 0.05, c, halo, y);
}
export function sprayRing(x, z, r, hex, floorHex = PLY[0], y = 0) {
  const halo = mix(hex, floorHex, 0.62), c = vary(hex, 0.04), N = 14;
  for (let i = 0; i < N; i++) { const a0 = (i / N) * Math.PI * 2, a1 = ((i + 1) / N) * Math.PI * 2 + (i === N - 1 ? 0.2 : 0); stroke(x + Math.cos(a0) * r, z + Math.sin(a0) * r, x + Math.cos(a1) * r, z + Math.sin(a1) * r, 0.04, c, halo, y); }
}
export function sprayWord(word, x, z, h, yaw, hex, floorHex = PLY[0], y = 0) {
  const halo = mix(hex, floorHex, 0.62), c = vary(hex, 0.04);
  const cs = Math.cos(yaw), sn = Math.sin(yaw), W = h * 0.75;
  let ox = -(word.length * W * 1.2) / 2;
  for (const ch of word) {
    for (const [a, b, cc, d] of GLYPH[ch] || []) {
      const P = (u, v) => { const lx = ox + u * W, lz = -(v - 0.5) * h; return [x + lx * cs + lz * sn, z - lx * sn + lz * cs]; };
      const [p0x, p0z] = P(a, b), [p1x, p1z] = P(cc, d);
      stroke(p0x, p0z, p1x, p1z, h * 0.09, c, halo, y);
    }
    ox += W * 1.2;
  }
}
export function sprayArrow(x0, z0, x1, z1, hex, floorHex = PLY[0], y = 0) {
  const halo = mix(hex, floorHex, 0.62), c = vary(hex, 0.04);
  stroke(x0, z0, x1, z1, 0.05, c, halo, y);
  const a = Math.atan2(z1 - z0, x1 - x0);
  for (const s of [-1, 1]) stroke(x1, z1, x1 - Math.cos(a + s * 0.5) * 0.25, z1 - Math.sin(a + s * 0.5) * 0.25, 0.05, c, halo, y);
}

// ---------------------------------------------------------------- bake
// cheap shading: darken low surfaces near the walls and under shelves (the corners of a dusty room are dark).
// wall(x, z) is the distance to the nearest wall; above `high` everything is a little darker (the roof is
// further from the bulbs than it looks).
export function shade(ch, wall, high) {
  for (let i = 0; i < ch.nv; i++) {
    const x = ch.P[i * 3], y = ch.P[i * 3 + 1], z = ch.P[i * 3 + 2];
    const w = wall(x, z);
    let f = 1;
    if (y < 0.6) f *= 0.68 + 0.32 * Math.min(1, w / 1.4) * (0.6 + 0.4 * Math.min(1, y / 0.6 + 0.4));
    if (y > high) f *= 0.85;
    for (let k = 0; k < 3; k++) ch.C[i * 3 + k] *= f;
  }
}

// ---------------------------------------------------------------- generated headers
// The types the layout headers (attic_deep_data.h, memory_desert_data.h) share, guarded so both can be included.
export const LAYOUT_TYPES = `#ifndef KH_ATTIC_LAYOUT_TYPES
#define KH_ATTIC_LAYOUT_TYPES
typedef struct { float x0, z0, x1, z1, top; } attic_box_t;                      /* a solid: from the floor up to top */
typedef struct { float x, z, r; int landing; } attic_drop_t;                    /* a soft board, and the landing a fall drops to */
typedef struct { float x, z, yaw; } attic_mark_t;                               /* a spot and a facing (yaw 0 faces +z) */
typedef struct { float x, z, r; const char *zone; } attic_ring_t;               /* a fight clearing */
typedef struct { const char *name; float px, py, pz; int axis; float angle; } attic_topple_t;
typedef struct { const char *name; float x0, z0, x1, z1; } attic_chunk_t;       /* a chunk's footprint */
typedef struct { const char *name; float x, y, z, yaw; } attic_anchor_t;        /* a named spot */
#endif`;
