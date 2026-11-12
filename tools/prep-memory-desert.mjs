// The flashback set for the balloon launch (docs/ATTIC.md, Flashbacks): the Sonoran desert outside Gila Bend at
// dawn, 2010. Creosote flats about 60 m across fading into haze, a ring of low mountains on the horizon, a dirt
// pull-off by the highway, a pickup, a folding table with a laptop and a radio, helium tanks, the white foam
// payload with its antenna, and the balloon on its tarp. Built in the game's own vertex coloured look.
//
//   assets/models/memory_desert.glb   chunks: desert (lit), glow_desert (the laptop's screen), sky_desert (the
//                                     mountains: unlit, unfogged, their haze baked in), and the parts the
//                                     runtime moves: balloon (the envelope, its neck at the fill spot),
//                                     balloon_payload (its base at the table spot), balloon_chute, balloon_line
//                                     (one metre tall, scaled to the gap)
//   src/gen/memory_desert_data.h      solids, the keep-in circle, named anchors, the balloon's and payload's
//                                     spots, the sun's direction, the wind
//
//   node tools/prep-memory-desert.mjs
//
// Stage space: +x east, -z north, y up, metres. Drawn about the launch area and shifted west by DX on output,
// clear of where the lab's footprint would be (world_indoors() is positional).
import { writeFileSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { Chunk, writeGlb } from './glb-lib.mjs';
import { setSeed, rnd, R, pick, lin, vary, mix, box, obox, beam, cyl, ball, quad, LAYOUT_TYPES } from './attic-kit.mjs';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
setSeed(2010);

const DX = -40;
const SUN = (() => { const v = [0.95, 0.12, 0.3], l = Math.hypot(...v); return v.map((c) => c / l); })();   // east south east, just up
const SH = (() => { const l = Math.hypot(SUN[0], SUN[2]); return [-SUN[0] / l, -SUN[2] / l]; })();         // shadows fall this way
const SH_K = 4.2;                 // shadow length per metre of height (shorter than the real sun's: it reads better)
const WIND = [-0.55, -0.83];      // the balloon drifts north west
const KEEP = { x: 2, z: 0, r: 24 };
const LINE = 6;                   // payload hangs this far under the balloon's neck
const NECK = { x: 1.5, y: 0.55, z: -3.4 };      // the fill spot: the neck tied to the filler
const TABLE = { x: 7.6, z: 3.2, top: 0.74 };    // folding table, long side east to west
const PAY = { x: TABLE.x - 0.55, y: TABLE.top, z: TABLE.z + 0.05 };
const TRUCK = { x: 12.2, z: 0.6 };              // facing north on the pull-off, tailgate south

const land = new Chunk('desert'), glow = new Chunk('glow_desert'), sky = new Chunk('sky_desert');
const balloon = new Chunk('balloon'), payload = new Chunk('balloon_payload'), chute = new Chunk('balloon_chute'), line = new Chunk('balloon_line');
const SOLIDS = [];
const solid = (x0, z0, x1, z1, top) => SOLIDS.push({ x0: Math.min(x0, x1), z0: Math.min(z0, z1), x1: Math.max(x0, x1), z1: Math.max(z0, z1), top });
const ANCH = [];
const anchor = (name, x, y, z, yaw) => ANCH.push({ name, x, y, z, yaw });
const col = (c) => (Array.isArray(c) ? c : lin(c));
const noise = (x, z) => Math.sin(x * 0.31 + z * 0.17) * 0.5 + Math.sin(x * 0.07 - z * 0.13 + 1.3) * 0.8 + Math.sin(x * 1.3 + z * 0.9) * 0.15;

// ---------------------------------------------------------------- the ground
const PAVE = 0xb09478, DIRT = 0xc6aa86, RUT = 0x8e765c;
const FOG = 0xc8947e;   // the runtime's fog colour for the desert (attic_env.c, attic_env_mood): keep the two the same
const ROAD_X = 25;
function pulloff(x, z) { return x > 3.5 && x < ROAD_X - 3.2 && Math.abs(z - 0.5) < 7 - Math.max(0, x - 18) * 0.4 ? 1 : 0; }
function groundColor(x, z) {
  let c = mix(PAVE, 0x9c8068, Math.max(0, Math.min(1, 0.5 + noise(x, z) * 0.35)));
  if (pulloff(x, z)) c = mix(DIRT, 0xb89c78, Math.max(0, Math.min(1, 0.5 + noise(x * 2, z * 2) * 0.4)));
  return c;
}
function ground() {
  // the near flats: a 2 m grid out to 44 m; each quad takes the colour at its middle
  const N = 44, S = 2;
  for (let x = -N; x < N; x += S) for (let z = -N; z < N; z += S) {
    if (x + S > ROAD_X - 3.6 && x < ROAD_X + 3.6) continue;   // the road takes this strip
    quad(land, [x, 0, z + S], [x + S, 0, z + S], [x + S, 0, z], [x, 0, z], vary(groundColor(x + S / 2, z + S / 2), 0.04));
  }
  // out past that, rings to the horizon (the fog has them by then)
  const RINGS = [44 * 1.42, 80, 130, 200, 290], SEG = 36;
  let inner = null;
  for (let ri = 0; ri < RINGS.length; ri++) {
    const r1 = RINGS[ri];
    for (let i = 0; i < SEG; i++) {
      const a0 = (i / SEG) * Math.PI * 2, a1 = ((i + 1) / SEG) * Math.PI * 2;
      // the first ring starts at the square's edge
      const r0a = inner ? inner : Math.min(N / Math.max(Math.abs(Math.cos(a0)), Math.abs(Math.sin(a0))), N * 1.42), r0b = inner ? inner : Math.min(N / Math.max(Math.abs(Math.cos(a1)), Math.abs(Math.sin(a1))), N * 1.42);
      const c = vary(mix(PAVE, 0xc09a84, ri / RINGS.length), 0.04);
      quad(land, [Math.cos(a0) * r0a, 0, Math.sin(a0) * r0a], [Math.cos(a0) * r1, -0.02 * ri, Math.sin(a0) * r1], [Math.cos(a1) * r1, -0.02 * ri, Math.sin(a1) * r1], [Math.cos(a1) * r0b, 0, Math.sin(a1) * r0b], c);
    }
    inner = r1;
  }
  // the square's corners left between the grid and the first ring
  // (the first ring starts on the square's edge, so nothing is missing)
  // tyre ruts from the road onto the pull-off, round to where the truck stands
  for (const off of [-0.8, 0.8]) {
    let px = ROAD_X - 3.4, pz = -4 + off;
    for (let t = 1; t <= 10; t++) {
      const u = t / 10, nx = ROAD_X - 3.4 - (ROAD_X - 3.4 - TRUCK.x) * u, nz = -4 + off + (TRUCK.z - 2.5 + 4) * u * u;
      const dx = nx - px, dz = nz - pz, l = Math.hypot(dx, dz), qx = -dz / l * 0.16, qz = dx / l * 0.16;
      quad(land, [px - qx, 0.006, pz - qz], [px + qx, 0.006, pz + qz], [nx + qx, 0.006, nz + qz], [nx - qx, 0.006, nz - qz], vary(RUT, 0.06));
      px = nx; pz = nz;
    }
  }
  // gravel and pebbles on the pull-off, desert pavement stones out on the flats
  for (let k = 0; k < 260; k++) {
    const x = R(-40, 40), z = R(-40, 40); if (x > ROAD_X - 4 && x < ROAD_X + 4) continue;
    const s = R(0.03, 0.09);
    obox(land, x, s * 0.3, z, s, s * 0.4, s * 0.8, R(0, 3), vary(pick([0x7a6450, 0x5a4a3e, 0x9a8470, 0x6a4a3a]), 0.1));
  }
}
function road() {
  // a two lane highway running north and south past the pull-off, fading into the haze both ways
  const x0 = ROAD_X - 3.6, x1 = ROAD_X + 3.6;
  for (let z = -300; z < 300; z += 6) {
    quad(land, [x0, 0.01, z + 6], [x1, 0.01, z + 6], [x1, 0.01, z], [x0, 0.01, z], vary(0x4a4646, 0.04));
    if (Math.abs(z) < 120) {
      quad(land, [x0 + 0.15, 0.015, z + 6], [x0 + 0.27, 0.015, z + 6], [x0 + 0.27, 0.015, z], [x0 + 0.15, 0.015, z], 0xd8d0c0);
      quad(land, [x1 - 0.27, 0.015, z + 6], [x1 - 0.15, 0.015, z + 6], [x1 - 0.15, 0.015, z], [x1 - 0.27, 0.015, z], 0xd8d0c0);
      if (Math.floor(z / 6) % 2) quad(land, [ROAD_X - 0.06, 0.015, z + 4], [ROAD_X + 0.06, 0.015, z + 4], [ROAD_X + 0.06, 0.015, z], [ROAD_X - 0.06, 0.015, z], 0xd8b030);
    }
  }
  // shoulders of gravel, a mile marker, a few fence posts beyond
  for (const sx of [-1, 1]) for (let z = -120; z < 120; z += 6) quad(land, [ROAD_X + sx * 3.6, 0.008, z + 6], [ROAD_X + sx * 4.4, 0.004, z + 6], [ROAD_X + sx * 4.4, 0.004, z], [ROAD_X + sx * 3.6, 0.008, z], vary(0x8a7a6a, 0.05));
  box(land, ROAD_X - 4.3, 0, -9.05, ROAD_X - 4.2, 1.2, -8.95, 0x2a6a3a);
  box(land, ROAD_X - 4.32, 0.9, -9.07, ROAD_X - 4.18, 1.15, -8.93, 0x2a7a42);
  for (let z = -80; z < 80; z += 4) beam(land, [ROAD_X + 9, 0, z], [ROAD_X + 9 + R(-0.05, 0.05), 1.1, z], 0.08, 0.08, vary(0x5a4a3a, 0.15));
  for (const y of [0.6, 0.9]) beam(land, [ROAD_X + 9, y, -80], [ROAD_X + 9, y, 80], 0.01, 0.01, 0x6a6a6a);
}

// ---------------------------------------------------------------- long shadows on the ground
function hullFan(pts, y, c) {
  // a convex hull (gift wrap), drawn as a fan
  pts = pts.slice().sort((a, b) => a[0] - b[0] || a[1] - b[1]);
  const cross = (o, a, b) => (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]);
  const lo = [], up = [];
  for (const p of pts) { while (lo.length >= 2 && cross(lo[lo.length - 2], lo[lo.length - 1], p) <= 0) lo.pop(); lo.push(p); }
  for (let i = pts.length - 1; i >= 0; i--) { const p = pts[i]; while (up.length >= 2 && cross(up[up.length - 2], up[up.length - 1], p) <= 0) up.pop(); up.push(p); }
  const H = lo.slice(0, -1).concat(up.slice(0, -1));
  const P = [], I = [];
  for (const [x, z] of H) P.push(x, y, z);
  for (let i = 1; i + 1 < H.length; i++) I.push(0, i + 1, i);
  land.add(Float32Array.from(P), I, col(c));
}
function shadowBox(x0, z0, x1, z1, h, k = 1) {
  const L = h * SH_K, ox = SH[0] * L, oz = SH[1] * L, base = [[x0, z0], [x1, z0], [x1, z1], [x0, z1]];
  const mid = base.map(([x, z]) => [x + ox * 0.55, z + oz * 0.55]), far = base.map(([x, z]) => [x + ox, z + oz]);
  const g = groundColor((x0 + x1) / 2 + ox / 2, (z0 + z1) / 2 + oz / 2);
  hullFan([...base, ...mid], 0.012, g.map((v) => v * (0.5 + 0.1 * (1 - k))));
  hullFan([...mid, ...far], 0.011, g.map((v) => v * (0.68 + 0.1 * (1 - k))));
}
function shadowBlob(x, z, r, h) { shadowBox(x - r * 0.7, z - r * 0.7, x + r * 0.7, z + r * 0.7, h, 0.5); }

// ---------------------------------------------------------------- plants
function creosote(x, z, s, far = false) {
  // an open round shrub: dark olive blobs on thin grey stems, the bare ground showing between
  const G = [0x4e5a2c, 0x5a6630, 0x46522a, 0x667038];
  if (far) {   // a low dome, all that reads at this distance
    const P0 = land.nv; ball(land, x, s * 0.3, z, s * 0.6, vary(pick(G), 0.1), 3);
    for (let i = P0; i < land.nv; i++) land.P[i * 3 + 1] = Math.max(0, s * 0.3 + (land.P[i * 3 + 1] - s * 0.3) * 0.7);
    return;
  }
  const n = 3 + Math.floor(rnd() * 3);
  for (let k = 0; k < n; k++) {
    const a = R(0, Math.PI * 2), rr = R(0.1, 0.45) * s, bx = x + Math.cos(a) * rr, bz = z + Math.sin(a) * rr, by = R(0.45, 0.85) * s, br = R(0.25, 0.4) * s;
    beam(land, [x + R(-0.05, 0.05), 0, z + R(-0.05, 0.05)], [bx, by - br * 0.5, bz], 0.025, 0.025, 0x6a5e50);
    // a squashed blob: a ball, flattened a little
    const P0 = land.nv; ball(land, bx, by, bz, br, vary(pick(G), 0.12), 3);
    for (let i = P0; i < land.nv; i++) land.P[i * 3 + 1] = by + (land.P[i * 3 + 1] - by) * 0.75;
  }
  shadowBlob(x, z, s * 0.5, s * 0.9);
}
function bursage(x, z) {   // low grey-green mounds between the creosote
  const P0 = land.nv; ball(land, x, 0.12, z, R(0.18, 0.3), vary(0x8a8c6a, 0.1), 3);
  for (let i = P0; i < land.nv; i++) land.P[i * 3 + 1] = Math.max(0, 0.12 + (land.P[i * 3 + 1] - 0.12) * 0.6);
}
function saguaro(x, z, h) {
  const c = vary(0x3e5a34, 0.08);
  cyl(land, [x, 0, z], [x, h, z], 0.22, 8, c); ball(land, x, h, z, 0.22, c, 3);
  for (const [a, y0, len] of [[R(0, 6), h * 0.45, h * 0.3], [R(0, 6), h * 0.55, h * 0.25]]) {
    const ox = Math.cos(a) * 0.55, oz = Math.sin(a) * 0.55;
    cyl(land, [x, y0, z], [x + ox, y0, z + oz], 0.15, 6, c); cyl(land, [x + ox, y0, z + oz], [x + ox, y0 + len, z + oz], 0.16, 6, c); ball(land, x + ox, y0 + len, z + oz, 0.16, c, 3);
  }
  shadowBlob(x, z, 0.4, h);
  solid(x - 0.3, z - 0.3, x + 0.3, z + 0.3, h);
}
function plants() {
  const clear = (x, z) => {   // the set itself stays open
    if (pulloff(x, z) && x > 5) return false;
    if (Math.hypot(x - NECK.x, z - NECK.z) < 4.5) return false;
    if (Math.hypot(x + 6, z + 8) < 4) return false;   // where it is let go
    if (Math.abs(x - ROAD_X) < 6) return false;
    return true;
  };
  for (let k = 0; k < 150; k++) {
    const a = R(0, Math.PI * 2), r = 4 + Math.sqrt(rnd()) * 44, x = 1 + Math.cos(a) * r, z = Math.sin(a) * r;
    if (!clear(x, z)) continue;
    const s = R(0.9, 1.7);
    creosote(x, z, s);
    if (r < KEEP.r - 0.5) solid(x - s * 0.3, z - s * 0.3, x + s * 0.3, z + s * 0.3, s * 0.8);
  }
  for (let k = 0; k < 120; k++) { const x = R(-44, 44), z = R(-44, 44); if (clear(x, z)) bursage(x, z); }
  for (let k = 0; k < 110; k++) { const a = R(0, Math.PI * 2), r = R(48, 120), x = Math.cos(a) * r, z = Math.sin(a) * r; if (Math.abs(x - ROAD_X) > 7) creosote(x, z, R(1.2, 2.0), true); }
  saguaro(-21, -26, 5.2); saguaro(-34, 10, 4.1); saguaro(30, -38, 6.0);
}

// ---------------------------------------------------------------- the mountains: a low ring on the horizon
function ridge(r, seed, lo, hi, nearC, farC) {
  const N = 120, hs = [];
  for (let i = 0; i < N; i++) {
    const a = (i / N) * Math.PI * 2;
    let h = 0.5 + 0.5 * Math.sin(a * 3 + seed) * 0.6 + 0.3 * Math.sin(a * 7 + seed * 2) + 0.2 * Math.sin(a * 17 + seed * 3) + 0.08 * Math.sin(a * 41 + seed);
    h = Math.max(0, Math.min(1, h));
    hs.push(lo + (hi - lo) * h * h);
  }
  const HAZE = FOG;   // the foot of the range is the fog's colour, so it rises out of the haze
  for (let i = 0; i < N; i++) {
    const a0 = (i / N) * Math.PI * 2, a1 = ((i + 1) / N) * Math.PI * 2, h0 = hs[i], h1 = hs[(i + 1) % N];
    // toward the sun the range is a dark silhouette; away from it, it catches the first pink light
    const lit = Math.max(0, -(Math.cos((a0 + a1) / 2) * SUN[0] + Math.sin((a0 + a1) / 2) * SUN[2]));
    const top = mix(nearC, farC, lit);
    const P = [Math.cos(a0) * r, -12, Math.sin(a0) * r, Math.cos(a1) * r, -12, Math.sin(a1) * r, Math.cos(a1) * r, h1, Math.sin(a1) * r, Math.cos(a0) * r, h0, Math.sin(a0) * r];
    // two bands so the foot melts into the haze: the lower half the haze colour, the upper the rock
    const m0 = [Math.cos(a0) * r, h0 * 0.18, Math.sin(a0) * r], m1 = [Math.cos(a1) * r, h1 * 0.18, Math.sin(a1) * r];
    quad(sky, [P[0], P[1], P[2]], [P[3], P[4], P[5]], m1, m0, HAZE);
    quad(sky, m0, m1, [P[6], P[7], P[8]], [P[9], P[10], P[11]], mix(HAZE, top, 0.55));
    quad(sky, [m0[0] * 0.999, m0[1] + 0.3, m0[2] * 0.999], [m1[0] * 0.999, m1[1] + 0.3, m1[2] * 0.999], [P[6] * 0.999, P[7], P[8] * 0.999], [P[9] * 0.999, P[10], P[11] * 0.999], top);
  }
}

// ---------------------------------------------------------------- the pickup
// built in its own frame: a along the length (+ toward the front), y up, c across (+ to its right)
function truck() {
  const yaw = Math.PI;   // facing north
  const T = (a, y, c) => { const cs = Math.cos(yaw), sn = Math.sin(yaw); return [TRUCK.x + c * cs + a * sn, y, TRUCK.z - c * sn + a * cs]; };
  const B = (a0, y0, c0, a1, y1, c1, cc) => {   // a box in the truck's frame
    const P = []; for (const [a, y, c] of [[a0, y0, c0], [a1, y0, c0], [a1, y1, c0], [a0, y1, c0], [a0, y0, c1], [a1, y0, c1], [a1, y1, c1], [a0, y1, c1]]) P.push(...T(a, y, c));
    land.add(Float32Array.from(P), [0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4, 3, 7, 6, 3, 6, 2, 0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5], col(cc));
  };
  const Q4 = (p, c) => quad(land, T(...p[0]), T(...p[1]), T(...p[2]), T(...p[3]), c);
  const PAINT = 0x8a3a30, PAINT_D = 0x6e2c26, GLASS = 0x2a3440, TRIM = 0x2a2a2c, CHR = 0xa8acb0;
  // wheels, the frame between them
  for (const a of [1.65, -1.55]) for (const c of [-0.82, 0.82]) {
    cyl(land, T(a, 0.38, c - 0.14), T(a, 0.38, c + 0.14), 0.38, 12, 0x1c1c1e);
    cyl(land, T(a, 0.38, c + Math.sign(c) * 0.145), T(a, 0.38, c + Math.sign(c) * 0.15), 0.2, 10, CHR);
  }
  B(-2.5, 0.32, -0.6, 2.5, 0.5, 0.6, TRIM);
  // the body: front clip and hood, the cab, the bed
  B(1.0, 0.5, -0.95, 2.62, 1.02, 0.95, PAINT);
  Q4([[2.62, 1.02, -0.92], [1.0, 1.12, -0.92], [1.0, 1.12, 0.92], [2.62, 1.02, 0.92]], vary(PAINT, 0.04));   // the hood's top
  B(2.62, 0.45, -0.98, 2.72, 0.7, 0.98, CHR);                         // bumper
  B(2.6, 0.75, -0.7, 2.64, 0.98, 0.7, 0x3a3a3c);                      // grille
  for (const c of [-0.78, 0.78]) B(2.6, 0.82, c - 0.13, 2.645, 0.95, c + 0.13, 0xd8d4c4);   // headlights
  B(-0.35, 0.5, -0.95, 1.0, 1.08, 0.95, PAINT);                       // the cab's lower half
  B(-0.32, 1.08, -0.9, 0.55, 1.82, 0.9, PAINT_D);                     // the cab's roof block
  Q4([[0.55, 1.82, -0.86], [1.0, 1.1, -0.86], [1.0, 1.1, 0.86], [0.55, 1.82, 0.86]], GLASS);   // windscreen
  for (const c of [-0.905, 0.905]) Q4([[-0.2, 1.14, c], [0.5, 1.14, c], [0.5, 1.74, c], [-0.2, 1.74, c]], GLASS);
  Q4([[-0.325, 1.16, -0.75], [-0.325, 1.16, 0.75], [-0.325, 1.72, 0.75], [-0.325, 1.72, -0.75]], GLASS);   // back window
  for (const c of [-1.0, 1.0]) B(0.9, 1.2, c - 0.04, 1.0, 1.36, c + 0.04, TRIM);   // mirrors
  // the bed: floor, sides, and the tailgate let down
  B(-2.5, 0.5, -0.95, -0.38, 0.9, 0.95, PAINT);
  B(-2.5, 0.9, -0.95, -0.38, 1.18, -0.86, PAINT); B(-2.5, 0.9, 0.86, -0.38, 1.18, 0.95, PAINT);
  B(-0.42, 0.9, -0.95, -0.38, 1.18, 0.95, PAINT);
  B(-2.5, 0.86, -0.86, -0.4, 0.9, 0.86, 0x3a3a3a);   // bed liner
  B(-3.02, 0.84, -0.9, -2.5, 0.9, 0.9, PAINT_D);     // the tailgate, down
  for (const c of [-0.9, 0.9]) beam(land, T(-2.5, 1.16, c), T(-3.0, 0.88, c), 0.01, 0.01, 0x6a6a6a);   // its cables
  for (const c of [-0.88, 0.88]) B(-2.52, 0.95, c - 0.07, -2.48, 1.15, c + 0.07, 0xb02020);   // tail lights
  B(-2.62, 0.4, -0.98, -2.5, 0.58, 0.98, CHR);
  // in the bed: a tank lying down in a strap, a cooler, a box of gear
  cyl(land, T(-2.2, 1.06, -0.4), T(-0.7, 1.06, -0.4), 0.115, 8, 0x6a6e72);
  B(-2.3, 0.9, 0.2, -1.6, 1.3, 0.8, 0x2f6fd0); B(-2.32, 1.3, 0.18, -1.58, 1.36, 0.82, 0xe8e8e8);
  B(-1.4, 0.9, 0.25, -0.8, 1.2, 0.8, 0x6a5a40);
  // on the tailgate: a second payload box (the spare), a roll of duct tape
  B(-2.95, 0.9, 0.3, -2.7, 1.12, 0.55, 0xeeeeea);
  cyl(land, T(-2.85, 0.9, -0.4), T(-2.85, 0.95, -0.4), 0.06, 8, 0x9a9a9a);
  solid(...(() => { const a = T(-3.05, 0, -1.0), b = T(2.75, 0, 1.0); return [a[0], a[2], b[0], b[2]]; })(), 1.85);
  // its shadow, long across the flats
  const s0 = T(-2.6, 0, -0.95), s1 = T(2.6, 0, 0.95);
  shadowBox(Math.min(s0[0], s1[0]), Math.min(s0[2], s1[2]), Math.max(s0[0], s1[0]), Math.max(s0[2], s1[2]), 1.6);
  anchor('truck', TRUCK.x, 0, TRUCK.z, yaw);
  const tg = T(-3.6, 0, 0); anchor('tailgate', tg[0], 0.88, tg[2], 0);
}

// ---------------------------------------------------------------- the folding table and what is on it
function table() {
  const { x, z, top } = TABLE, hx = 0.92, hz = 0.38;
  box(land, x - hx, top - 0.04, z - hz, x + hx, top, z + hz, 0xd8d6cc);
  for (const sx of [-1, 1]) {
    beam(land, [x + sx * (hx - 0.12), 0, z - hz + 0.06], [x + sx * (hx - 0.1), top - 0.04, z - hz + 0.06], 0.025, 0.025, 0x6a6e72);
    beam(land, [x + sx * (hx - 0.12), 0, z + hz - 0.06], [x + sx * (hx - 0.1), top - 0.04, z + hz - 0.06], 0.025, 0.025, 0x6a6e72);
    beam(land, [x + sx * (hx - 0.12), 0.05, z - hz + 0.06], [x + sx * (hx - 0.12), 0.05, z + hz - 0.06], 0.02, 0.02, 0x6a6e72);
  }
  solid(x - hx, z - hz, x + hx, z + hz, top);
  shadowBox(x - hx, z - hz, x + hx, z + hz, top);
  // the laptop, open, its screen toward the south (the chairs)
  const lx = x + 0.15, lz = z - 0.05;
  box(land, lx - 0.18, top, lz - 0.13, lx + 0.18, top + 0.025, lz + 0.13, 0x2a2c30);
  box(land, lx - 0.18, top + 0.025, lz - 0.15, lx + 0.18, top + 0.26, lz - 0.13, 0x2a2c30);
  box(glow, lx - 0.16, top + 0.04, lz - 0.128, lx + 0.16, top + 0.245, lz - 0.126, 0x7a98b8);
  box(land, lx - 0.15, top + 0.026, lz - 0.08, lx + 0.15, top + 0.03, lz + 0.05, 0x4a4c50);
  anchor('laptop', lx, top, lz + 0.5, Math.PI);
  // a ham radio base set with a hand mic, and a handheld with its rubber antenna
  const rx = x + 0.62, rz = z - 0.12;
  box(land, rx - 0.12, top, rz - 0.12, rx + 0.12, top + 0.07, rz + 0.12, 0x1e2022);
  box(glow, rx - 0.09, top + 0.025, rz + 0.121, rx - 0.01, top + 0.05, rz + 0.123, 0xffa040);
  cyl(land, [rx + 0.06, top + 0.035, rz + 0.122], [rx + 0.06, top + 0.035, rz + 0.13], 0.018, 8, 0x3a3a3a);
  box(land, rx + 0.16, top, rz + 0.02, rx + 0.2, top + 0.09, rz + 0.07, 0x1e2022);
  cyl(land, [rx + 0.18, top + 0.09, rz + 0.045], [rx + 0.18, top + 0.24, rz + 0.045], 0.006, 4, 0x111111);
  cyl(land, [rx - 0.1, top + 0.07, rz - 0.1], [rx - 0.14, top + 1.4, rz - 0.18], 0.004, 3, 0x9a9a9a, false);   // a whip antenna clamped to the table
  anchor('radio', rx, top, rz + 0.5, Math.PI);
  // a thermos, a notebook, a GPS
  cyl(land, [x - 0.2, top, z + 0.2], [x - 0.2, top + 0.3, z + 0.2], 0.04, 8, 0x2a6a4a);
  box(land, x - 0.85, top, z - 0.05, x - 0.62, top + 0.012, z + 0.25, 0xe8e0c8);
  box(land, x + 0.35, top, z + 0.15, x + 0.43, top + 0.025, z + 0.28, 0xd8c020);
  anchor('table', x, top, z + 0.9, Math.PI);
  // two camp chairs south of the table
  for (const cx of [x - 0.5, x + 0.55]) {
    const cz = z + 0.95, c = vary(0x2a4a7a, 0.1);
    for (const sx of [-1, 1]) for (const sz of [-1, 1]) beam(land, [cx + sx * 0.25, 0, cz + sz * 0.22], [cx - sx * 0.25, 0.45, cz - sz * 0.22], 0.015, 0.015, 0x3a3a3a);
    quad(land, [cx - 0.25, 0.45, cz - 0.22], [cx + 0.25, 0.45, cz - 0.22], [cx + 0.25, 0.4, cz + 0.22], [cx - 0.25, 0.4, cz + 0.22], c);
    quad(land, [cx - 0.25, 0.45, cz + 0.24], [cx + 0.25, 0.45, cz + 0.24], [cx + 0.27, 0.9, cz + 0.32], [cx - 0.27, 0.9, cz + 0.32], c);
    solid(cx - 0.28, cz - 0.25, cx + 0.28, cz + 0.32, 0.9);
    shadowBox(cx - 0.25, cz - 0.22, cx + 0.25, cz + 0.3, 0.8, 0.6);
  }
}

// ---------------------------------------------------------------- helium, the tarp, the filler
function fillSite() {
  // a blue tarp the balloon is laid out on, held down by rocks
  const { x, z } = NECK;
  quad(land, [x - 1.4, 0.03, z + 1.6], [x + 1.4, 0.03, z + 1.6], [x + 1.4, 0.03, z - 2.2], [x - 1.4, 0.03, z - 2.2], 0x2a5aa0);   // above the shadows
  for (const [sx, sz] of [[-1, -1], [1, -1], [1, 1], [-1, 1]]) obox(land, x + sx * 1.35, 0.08, z + (sz < 0 ? -2.15 : 1.55), 0.14, 0.09, 0.12, R(0, 3), 0x6a5a4a);
  // three tanks: two standing chained to a hand truck, one with the regulator and hose to the filler
  const tx = x + 2.6, tz = z + 1.4;
  for (const [ox, oz] of [[0, 0], [0.3, 0.1], [-0.28, 0.12]]) {
    cyl(land, [tx + ox, 0, tz + oz], [tx + ox, 1.3, tz + oz], 0.115, 10, vary(0x7a7e82, 0.05));
    cyl(land, [tx + ox, 1.3, tz + oz], [tx + ox, 1.42, tz + oz], 0.07, 8, 0x8a6a40);
    cyl(land, [tx + ox, 1.42, tz + oz], [tx + ox, 1.48, tz + oz], 0.03, 6, 0xb0a070);
  }
  beam(land, [tx - 0.45, 0.02, tz + 0.35], [tx + 0.5, 0.02, tz + 0.35], 0.05, 0.05, 0x2a2a2a);
  for (const y of [0.4, 1.0]) beam(land, [tx - 0.45, y, tz + 0.25], [tx + 0.5, y, tz + 0.25], 0.012, 0.012, 0x9a9a9a);
  box(land, tx - 0.08, 1.48, tz - 0.06, tx + 0.08, 1.58, tz + 0.06, 0xb08a40);   // regulator
  cyl(land, [tx, 1.6, tz], [tx, 1.66, tz + 0.08], 0.03, 6, 0x111111);
  // the hose from the regulator over the ground to the filler, a short PVC pipe at the neck
  const H = [[tx, 1.55, tz + 0.05], [tx - 0.5, 0.6, tz - 0.2], [tx - 1.2, 0.04, z + 0.2], [x + 0.3, 0.04, z + 0.15], [x, 0.3, z]];
  for (let i = 0; i + 1 < H.length; i++) cyl(land, H[i], H[i + 1], 0.012, 4, 0x1a1a1a, false);
  cyl(land, [x, 0.3, z], [x, NECK.y, z], 0.025, 6, 0xe8e8e0);
  solid(tx - 0.45, tz - 0.15, tx + 0.45, tz + 0.4, 1.6);
  shadowBox(tx - 0.4, tz - 0.12, tx + 0.4, tz + 0.25, 1.45);
  anchor('tanks', tx - 0.6, 0, tz - 0.5, Math.PI / 2);
  anchor('fill', x, NECK.y, z, 0);
}

// ---------------------------------------------------------------- the balloon and its train
function envelope() {
  // latex, pale and a little see through: a sphere drawn down to the neck at the bottom. Its neck is at NECK;
  // the runtime scales it from there as it fills
  const RING = 12, SEG = 16, Rr = 1.0;
  const P = [], I = [];
  for (let i = 0; i <= RING; i++) {
    const v = i / RING, th = Math.PI * v;   // 0 at the top, pi at the bottom
    let r = Math.sin(th) * Rr, y = (1 - Math.cos(th)) * Rr;
    if (v > 0.72) { const k = (v - 0.72) / 0.28; r *= 1 - 0.85 * k * k; }   // drawn in toward the neck
    y = 2 * Rr + 0.18 - y;
    for (let j = 0; j < SEG; j++) { const a = (j / SEG) * Math.PI * 2; P.push(NECK.x + Math.cos(a) * Math.max(r, 0.035), NECK.y + y, NECK.z + Math.sin(a) * Math.max(r, 0.035)); }
  }
  for (let i = 0; i < RING; i++) for (let j = 0; j < SEG; j++) { const a = i * SEG + j, b = i * SEG + ((j + 1) % SEG); I.push(a, b, b + SEG, a, b + SEG, a + SEG); }
  balloon.add(Float32Array.from(P), I, lin(0xe8dccc));
  cyl(balloon, [NECK.x, NECK.y, NECK.z], [NECK.x, NECK.y + 0.2, NECK.z], 0.035, 6, 0xd8c8b4);   // the neck, tied off
}
function train() {
  // the payload: a white foam box, duct taped, an antenna up and a camera's eye on one side
  const { x, y, z } = PAY, s = 0.13;
  box(payload, x - s, y, z - s, x + s, y + 0.22, z + s, 0xeeeeea);
  for (const dy of [0.04, 0.17]) box(payload, x - s - 0.003, y + dy, z - s - 0.003, x + s + 0.003, y + dy + 0.03, z + s + 0.003, 0x9a9a9a);
  cyl(payload, [x + 0.05, y + 0.22, z], [x + 0.05, y + 0.62, z], 0.006, 4, 0x111111);
  cyl(payload, [x, y + 0.12, z + s], [x, y + 0.12, z + s + 0.03], 0.025, 8, 0x111111);
  box(payload, x - 0.03, y + 0.22, z - 0.03, x + 0.03, y + 0.24, z + 0.03, 0xd03030);   // the line's tie point
  // the parachute: a red and orange canopy, its lines gathered below
  const cx = NECK.x, cy = NECK.y - LINE * 0.4, cz = NECK.z, cr = 0.45;
  const P = [cx, cy, cz], I = [];
  for (let j = 0; j < 10; j++) { const a = (j / 10) * Math.PI * 2; P.push(cx + Math.cos(a) * cr, cy - 0.25, cz + Math.sin(a) * cr); }
  for (let j = 0; j < 10; j++) I.push(0, 1 + j, 1 + ((j + 1) % 10));
  chute.add(Float32Array.from(P), I, lin(0xe04020));
  for (let j = 0; j < 10; j += 2) cyl(chute, [cx + Math.cos((j / 10) * Math.PI * 2) * cr, cy - 0.25, cz + Math.sin((j / 10) * Math.PI * 2) * cr], [cx, cy - 0.9, cz], 0.003, 3, 0xd8d8d8, false);
  // one metre of line; the runtime stretches it from the payload up to the neck
  cyl(line, [NECK.x, 0, NECK.z], [NECK.x, 1, NECK.z], 0.004, 3, 0xe8e8e8, false);
}

// ---------------------------------------------------------------- the people's marks and the camera's
function marks() {
  anchor('entry', 9.5, 0, 10.5, Math.atan2(-0.5, -1));
  anchor('stand_a', TABLE.x - 0.3, 0, TABLE.z + 0.75, Math.PI);           // at the laptop
  anchor('stand_b', TABLE.x + 1.25, 0, TABLE.z - 0.1, -Math.PI / 2);      // at the radio end, facing west
  anchor('stand_c', NECK.x + 1.1, 0, NECK.z + 0.6, -Math.PI / 2 - 0.3);   // at the filler
  anchor('stand_d', NECK.x - 1.0, 0, NECK.z + 0.9, Math.PI / 2 + 0.3);    // holding the balloon's other side
  anchor('stand_e', TRUCK.x - 1.4, 0, TRUCK.z + 3.6, -Math.PI / 2 - 0.6);  // by the tailgate
  anchor('launch', -6, 0, -8, Math.PI * 0.85);
  anchor('cam_wide', -12, 3.2, 13, Math.atan2(20, -16));                  // from the south west, the sunrise behind the set
  anchor('cam_low', NECK.x - 3.2, 0.5, NECK.z + 3.4, Math.atan2(3.2, -3.4));   // low, up at the balloon and the sky
  anchor('road', ROAD_X - 4.5, 0, -6, -Math.PI / 2);
}

ground();
road();
plants();
ridge(205, 1.7, 15, 30, 0x6a5a7c, 0xb88a80);
ridge(265, 4.2, 22, 44, 0x9a809a, 0xd0a090);
truck();
table();
fillSite();
envelope();
train();
marks();

// ---------------------------------------------------------------- write, shifted into stage space
const chunks = [land, glow, sky, balloon, payload, chute, line];
for (const ch of chunks) for (let i = 0; i < ch.nv; i++) ch.P[i * 3] += DX;
mkdirSync(join(ROOT, 'assets/models'), { recursive: true });
const res = writeGlb(join(ROOT, 'assets/models/memory_desert.glb'), chunks);

const f3 = (v) => `${v.toFixed(3)}f`;
const H = [];
H.push('/* generated by tools/prep-memory-desert.mjs: the desert at dawn, for the balloon launch flashback. Do not edit. */');
H.push('#pragma once');
H.push(LAYOUT_TYPES);
const entry = ANCH.find((a) => a.name === 'entry');
H.push(`#define MEMORY_DESERT_ENTRY_X ${f3(entry.x + DX)}`, `#define MEMORY_DESERT_ENTRY_Z ${f3(entry.z)}`, `#define MEMORY_DESERT_ENTRY_YAW ${f3(entry.yaw)}`);
H.push(`#define MEMORY_DESERT_KEEP_X ${f3(KEEP.x + DX)}`, `#define MEMORY_DESERT_KEEP_Z ${f3(KEEP.z)}`, `#define MEMORY_DESERT_KEEP_R ${f3(KEEP.r)}   /* you stay within this of the launch */`);
H.push(`#define MEMORY_DESERT_BALLOON_X ${f3(NECK.x + DX)}`, `#define MEMORY_DESERT_BALLOON_Y ${f3(NECK.y)}`, `#define MEMORY_DESERT_BALLOON_Z ${f3(NECK.z)}   /* the neck at the fill spot */`);
H.push(`#define MEMORY_DESERT_PAYLOAD_X ${f3(PAY.x + DX)}`, `#define MEMORY_DESERT_PAYLOAD_Y ${f3(PAY.y)}`, `#define MEMORY_DESERT_PAYLOAD_Z ${f3(PAY.z)}   /* its base, on the table */`);
H.push(`#define MEMORY_DESERT_LINE ${f3(LINE)}   /* the payload hangs this far under the neck */`);
H.push(`#define MEMORY_DESERT_SUN_X ${f3(SUN[0])}`, `#define MEMORY_DESERT_SUN_Y ${f3(SUN[1])}`, `#define MEMORY_DESERT_SUN_Z ${f3(SUN[2])}`);
H.push(`#define MEMORY_DESERT_WIND_X ${f3(WIND[0])}`, `#define MEMORY_DESERT_WIND_Z ${f3(WIND[1])}`);
H.push('static const attic_box_t MEMORY_DESERT_SOLIDS[] = {', ...SOLIDS.map((s) => `  { ${f3(s.x0 + DX)}, ${f3(s.z0)}, ${f3(s.x1 + DX)}, ${f3(s.z1)}, ${f3(s.top)} },`), '};');
H.push('static const attic_anchor_t MEMORY_DESERT_ANCHORS[] = {', ...ANCH.map((a) => `  { "${a.name}", ${f3(a.x + DX)}, ${f3(a.y)}, ${f3(a.z)}, ${f3(a.yaw)} },`), '};');
writeFileSync(join(ROOT, 'src/gen/memory_desert_data.h'), H.join('\n') + '\n');
console.log(`memory_desert: ${res.tris} triangles, ${(res.bytes / 1e6).toFixed(2)} MB; ${SOLIDS.length} solids, ${ANCH.length} anchors; ` + chunks.map((c) => `${c.name} ${c.tris}`).join(', '));
