// The five fight stages, built from the lab's real parts (HSL-108, same tags as tools/prep-lab.mjs):
// each area is cut out, scaled up, and stood on a ring around a wide clear floor.
//
//   assets/models/arena_{bench,laser,fridge,shop,unfold}.glb    vertex coloured chunks per sector
//   src/gen/arena_data.h                                        keep-in radius, pillars, clear radius
//
//   node tools/prep-arena.mjs
import { writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { readModel, simplify, Chunk, writeGlb, srgb } from './glb-lib.mjs';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const parts = readModel().filter((p) => !/centauri/i.test(p.name) && p.name !== 'LED cube LEDs' && !/peg hole/i.test(p.name) && p.alpha === 1 && !['slabs', 'structure', 'upper', 'basement', 'stairs', 'hatch', 'doors'].includes(p.group));
const byArea = new Map();
for (const p of parts) { if (!byArea.has(p.area)) byArea.set(p.area, []); byArea.get(p.area).push(p); }
console.log('areas:', [...byArea.keys()].join(', '));

const simpCache = new Map();
function simp(p, ratio) {
  const k = `${parts.indexOf(p)}:${ratio}`;
  if (simpCache.has(k)) return simpCache.get(k);
  let r = { pos: p.pos, idx: p.idx };
  if (p.idx.length / 3 > 60) { const s = simplify(p, ratio, 0.02); if (s.idx.length >= 3 && s.idx.length < p.idx.length) r = { pos: s.pos, idx: s.idx }; }
  simpCache.set(k, r);
  return r;
}
// a slice: the parts of some areas (or that match), scaled and swung to face the centre, standing at polar (r, ang)
function slice(ch, sel, { s, r, ang, ratio = 0.3, minExt = 0.06, y = 0, face = 0 }) {
  const list = sel.areas ? sel.areas.flatMap((a) => byArea.get(a) || []) : parts.filter(sel.match);
  const use = list.filter((p) => Math.max(p.box[3] - p.box[0], p.box[4] - p.box[1], p.box[5] - p.box[2]) >= minExt && (!sel.filter || sel.filter(p)));
  if (!use.length) { console.log('  empty slice', sel); return null; }
  const b = [1e9, 1e9, 1e9, -1e9, -1e9, -1e9];
  for (const p of use) for (let k = 0; k < 3; k++) { b[k] = Math.min(b[k], p.box[k]); b[k + 3] = Math.max(b[k + 3], p.box[k + 3]); }
  const cx = (b[0] + b[3]) / 2, cz = (b[2] + b[5]) / 2, y0 = Math.max(b[1], 0);
  // the longer horizontal axis lies along the ring
  const longX = b[3] - b[0] > b[5] - b[2];
  const a = (ang + (longX ? Math.PI / 2 : 0) + face);
  const c = Math.cos(a), sn = Math.sin(a), px = Math.cos(ang) * r, pz = Math.sin(ang) * r;
  const xf = (x, yy, z) => { const dx = (x - cx) * s, dz = (z - cz) * s; return [px + dx * c + dz * sn, y + (yy - y0) * s, pz - dx * sn + dz * c]; };
  for (const p of use) {
    const q = simp(p, ratio);
    const rgb = p.emissive ? p.emissive.map((v, i) => Math.max(v, p.color[i])) : p.color;
    ch.add(q.pos, q.idx, rgb, xf);
  }
  const half = Math.max(b[3] - b[0], b[5] - b[2]) * 0.5 * s;
  return { x: px, z: pz, half };
}
const poly = (ch, pts, rgb) => { const f = pts.flat(); const idx = []; for (let i = 1; i + 1 < pts.length; i++) idx.push(0, i, i + 1); ch.add(Float32Array.from(f), idx, rgb); };
// the floor: rings of tiles coloured by colf(ring, sector)
function floor(ch, R, rings, secs, colf) {
  const n = rings * 2, cs = (R * 2) / n;
  for (let j = 0; j < n; j++) for (let i = 0; i < n; i++) {
    const x0 = -R + i * cs, z0 = -R + j * cs;
    if (Math.hypot(x0 + cs / 2, z0 + cs / 2) > R + cs) continue;
    poly(ch, [[x0, 0, z0 + cs], [x0 + cs, 0, z0 + cs], [x0 + cs, 0, z0], [x0, 0, z0]], colf(Math.floor(Math.hypot(x0 + cs / 2, z0 + cs / 2) / cs), i + j, i, j));
  }
}
// a ring wall facing in, in horizontal bands [y0, y1, colour]
function wall(ch, R, secs, bands) {
  for (let i = 0; i < secs; i++) {
    const a0 = (i / secs) * Math.PI * 2, a1 = ((i + 1) / secs) * Math.PI * 2;
    for (const [y0, y1, c] of bands) {
      const col = typeof c === 'function' ? c(i) : c;
      poly(ch, [[Math.cos(a0) * R, y0, Math.sin(a0) * R], [Math.cos(a0) * R, y1, Math.sin(a0) * R], [Math.cos(a1) * R, y1, Math.sin(a1) * R], [Math.cos(a1) * R, y0, Math.sin(a1) * R]], col);
    }
  }
}
function ringGlow(ch, R, y, h, secs, rgb) { wall(ch, R, secs, [[y, y + h, rgb]]); }
const beam = (ch, x0, y0, z0, x1, y1, z1, rgb) => ch.box(x0, y0, z0, x1, y1, z1, rgb);

const outHeader = {};
function bake(name, build) {
  const A = new Chunk(`${name}_a`), B = new Chunk(`${name}_b`), C = new Chunk(`${name}_c`), F = new Chunk(`${name}_floor`), G = new Chunk(`glow_${name}`, 'glow');
  const info = build({ A, B, C, F, G }) || {};
  const r = writeGlb(join(ROOT, `assets/models/arena_${name}.glb`), [F, A, B, C, G]);
  console.log(`arena ${name}: ${r.tris} tris, ${(r.bytes / 1e6).toFixed(2)} MB (a ${A.tris} b ${B.tris} c ${C.tris} floor ${F.tris} glow ${G.tris})`);
  outHeader[name] = info;
}
const TAU = Math.PI * 2;

bake('bench', ({ A, B, C, F, G }) => {
  // The Long Bench: the hall opened out. Warm plank floor, benches and tables on a ring.
  const plank = [srgb(0xb8895a), srgb(0xa87a4e), srgb(0xc29465), srgb(0x9d7148)];
  floor(F, 34, 16, 0, (r, k, i, j) => plank[(i * 3 + (j >> 1) * 7) % 4]);
  ringGlow(G, 15.5, 0, .06, 64, [1, .75, .35]);
  const N = [['woodshop'], ['electronics'], ['coworking'], ['craft tables'], ['microcontroller alcove'], ['art, LEGO and soda wall']];
  N.forEach((areas, i) => slice([A, B, C][i % 3], { areas }, { s: 3.0, r: 22, ang: (i / N.length) * TAU, ratio: 0.3, minExt: 0.08 }));
  wall(A, 36, 48, [[0, 3, srgb(0xe6e2d4)], [3, 3.5, srgb(0xd6a03a)], [3.5, 8, srgb(0x7a7f62)], [8, 11, srgb(0x4a4238)]]);
  ringGlow(G, 35.6, 8.2, .25, 48, [1, .85, .55]);
  // troffers in a great ring above
  for (let i = 0; i < 12; i++) { const a = (i / 12) * TAU; const x = Math.cos(a) * 14, z = Math.sin(a) * 14; beam(G, x - 1.4, 11, z - 1.4, x + 1.4, 11.15, z + 1.4, [1, .9, .7]); }
  return { keep: 15.5, clear: 12, pillars: [] };
});
bake('laser', ({ A, B, C, F, G }) => {
  // Laser Corner: a dark floor with a cut pattern, ring under an oversized gantry; the real lasers and scrap racks around it.
  const dark = srgb(0x1b1c22), dark2 = srgb(0x24252d), hot = srgb(0x552038);
  floor(F, 34, 16, 0, (r, k) => (r === 7 ? hot : k % 2 ? dark : dark2));
  slice(A, { areas: ['laser corner'] }, { s: 4.2, r: 23, ang: -Math.PI / 2, ratio: 0.3 });
  slice(B, { areas: ['laser corner'] }, { s: 3.4, r: 23, ang: Math.PI / 2, ratio: 0.3 });
  slice(C, { areas: ['vinyl and sublimation'] }, { s: 4, r: 22, ang: 0, ratio: 0.4 });
  slice(C, { areas: ['member storage and jewelry'] }, { s: 3.2, r: 23, ang: Math.PI, ratio: 0.4 });
  // the gantry: two rails across the ring on legs, a cross beam with the head and a beam of light
  const STEEL = srgb(0x70747c), YEL = srgb(0xd8a81e);
  for (const z of [-15.5, 15.5]) { beam(A, -19, 8.5, z - .5, 19, 9.4, z + .5, YEL); for (const x of [-18, 18]) beam(A, x - .6, 0, z - .6, x + .6, 8.5, z + .6, STEEL); }
  beam(A, -2.6, 9.4, -15, -1.2, 10.4, 15, STEEL); beam(A, 1.2, 9.4, -15, 2.6, 10.4, 15, STEEL);
  beam(B, -1.6, 7.6, -2, 1.6, 10.2, 2, srgb(0x2a2d33));
  beam(G, -.12, .0, -.12, .12, 7.6, .12, [1, .15, .5]);
  wall(B, 36, 48, [[0, 2.5, srgb(0x25262c)], [2.5, 2.8, srgb(0xa02050)], [2.8, 9, srgb(0x2d2e36)]]);
  ringGlow(G, 15.6, .02, .12, 64, [1, .1, .45]);
  ringGlow(G, 35.5, 2.52, .22, 48, [.1, .8, 1]);
  return { keep: 15.2, clear: 12, pillars: [[-18, -15.5, .9], [18, -15.5, .9], [-18, 15.5, .9], [18, 15.5, .9]] };
});
bake('fridge', ({ A, B, C, F, G }) => {
  // The Fridge: kitchen tile, frosted. The real kitchen stands huge and pale round the edge.
  const w = srgb(0xdfe9f0), b = srgb(0xaec6d8), ice = srgb(0xf4fbff);
  floor(F, 34, 17, 0, (r, k) => (k % 2 ? w : b));
  for (let i = 0; i < 10; i++) { const a = i * 2.4, r = 3 + i * 1.0; const x = Math.cos(a) * r, z = Math.sin(a) * r; poly(F, [[x - 1.4, .01, z + .4], [x + 1.2, .01, z + 1], [x + 1.3, .01, z - .8], [x - 1.2, .01, z - 1.1]], ice); }
  const K = (p) => /Kitchen|Fridge|fridge|Counter|counter|Dry|Wash|sink|Sink|table|Table|Microwave|cabinet|Cabinet|shelf|Shelf/.test(p.name);
  slice(A, { areas: ['kitchen and darkroom'] }, { s: 3.2, r: 24, ang: Math.PI / 2, ratio: 0.4 });
  slice(B, { areas: ['kitchen and darkroom'] }, { s: 3.2, r: 24, ang: -Math.PI / 2, ratio: 0.4 });
  slice(C, { match: (p) => /soda|Soda|fridge|Fridge/.test(p.name) }, { s: 5, r: 20, ang: 0, ratio: 0.5, minExt: 0.03 });
  slice(C, { areas: ['biolab'] }, { s: 3, r: 23, ang: Math.PI, ratio: 0.5 });
  wall(A, 36, 48, [[0, 3, w], [3, 6, (i) => (i % 2 ? b : w)], [6, 9, ice], [9, 13, srgb(0xc4d8e8)]]);
  ringGlow(G, 35.6, 9, .3, 48, [.7, .92, 1]);
  for (let i = 0; i < 14; i++) { const a = (i / 14) * TAU, x = Math.cos(a) * 16.6, z = Math.sin(a) * 16.6; beam(B, x - .5, 0, z - .5, x + .5, 1.5 + (i % 4) * .9, z + .5, ice); }
  return { keep: 15.5, clear: 12, pillars: [] };
});
bake('shop', ({ A, B, C, F, G }) => {
  // Machine Shop: a steel floor, the lathes and mills as pillars around it.
  const s1 = srgb(0x4e545a), s2 = srgb(0x5a6066), lines = srgb(0xd8a81e);
  floor(F, 34, 14, 0, (r, k) => (r === 6 ? (k % 2 ? lines : srgb(0x24262a)) : k % 2 ? s1 : s2));
  const match = (p) => p.area === 'machine shop';
  const pillars = [];
  for (let i = 0; i < 6; i++) {
    const ang = (i / 6) * TAU + .26;
    const q = slice([A, B, C][i % 3], { match }, { s: 3.4, r: 22, ang, ratio: 0.35, minExt: 0.05 });
    // and a plate-steel pier in front of each, the size of a real machine scaled up
    const x = Math.cos(ang) * 14.6, z = Math.sin(ang) * 14.6;
    beam(A, x - 1.1, 0, z - 1.1, x + 1.1, 6.5, z + 1.1, srgb(0x383c42));
    beam(A, x - 1.3, 6.5, z - 1.3, x + 1.3, 7, z + 1.3, srgb(0xc8611c));
    beam(G, x - 1.13, 2.5, z - 1.13, x + 1.13, 2.65, z + 1.13, [1, .45, .08]);
    pillars.push([+x.toFixed(2), +z.toFixed(2), 1.45]);
    void q;
  }
  slice(C, { areas: ['metal fab bay'] }, { s: 3, r: 24, ang: 1.2, ratio: 0.4 });
  slice(B, { areas: ['metal fab bay'] }, { s: 3, r: 24, ang: 4.2, ratio: 0.4 });
  wall(C, 36, 48, [[0, 3, srgb(0x3a3e44)], [3, 3.5, srgb(0xc8611c)], [3.5, 12, srgb(0x2e3036)]]);
  ringGlow(G, 35.6, 3.1, .25, 48, [1, .5, .12]);
  return { keep: 16, clear: 12, pillars };
});
bake('unfold', ({ A, B, C, F, G }) => {
  // The Whole Lab, Unfolded: every room on a ring round a huge disc, the night sky over it.
  const list = ['window display: gallery bay', 'window display: robot counter', 'window display: hacker bay', 'welcome', 'coworking', 'lounge', 'bike station', 'AV station', 'music station', 'microcontroller alcove', 'electronics', 'woodshop', 'laser corner', '3D printing', 'art, LEGO and soda wall', 'sewing', 'craft tables', 'member storage and jewelry', 'machine shop', 'metal fab bay', 'kitchen and darkroom', 'biolab'];
  const t = [srgb(0xe8e1d0), srgb(0xd6cdb6)], o = srgb(0xd9702a);
  floor(F, 56, 22, 0, (r, k) => (r === 15 ? (k % 2 ? o : t[1]) : t[k % 2]));
  list.forEach((a, i) => slice([A, B, C][i % 3], { areas: [a] }, { s: 3.0, r: 44, ang: (i / list.length) * TAU, ratio: 0.18, minExt: 0.12 }));
  ringGlow(G, 56, -.2, .2, 96, [1, .55, .2]);
  // the disc's edge, dropping away
  wall(A, 56.05, 96, [[-8, 0, srgb(0x22202c)]]);
  return { keep: 33, clear: 22, pillars: [] };
});
const NAMES = ['bench', 'laser', 'fridge', 'shop', 'unfold'];
let h = '/* generated by tools/prep-arena.mjs: do not edit */\n#ifndef ARENA_DATA_H\n#define ARENA_DATA_H\ntypedef struct { float x, z, r; } arena_pillar;\ntypedef struct { const char *glb; float keep, clear; int np; arena_pillar p[8]; } arena_info;\nstatic const arena_info ARENA_INFO[5] = {\n';
for (const n of NAMES) { const i = outHeader[n]; const ff = (v) => Number(v).toFixed(2) + "f"; h += `  {"arena_${n}.glb", ${ff(i.keep)}, ${ff(i.clear)}, ${i.pillars.length}, {${i.pillars.map((p) => `{${ff(p[0])},${ff(p[1])},${ff(p[2])}}`).join(',') || '{0,0,0}'}}},\n`; }
h += '};\n#endif\n';
writeFileSync(join(ROOT, 'src/gen/arena_data.h'), h);
process.exit(0);
