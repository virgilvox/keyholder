// The nightmare attic (docs/ATTIC.md, the arc, step 2): the first visit's attic. The blueprint room's language
// (plywood and old boards, 2x4 shelving, totes, pink insulation, bare bulbs, spray paint) grown impossibly
// large: canyons of shelving stacked on shelving, rafters lost in the dark, one long route that bends through
// the zones in turn (holiday supplies, old member storage, robotics, metal stock and computer parts) to the
// boss chamber, where the rack stands in the far corner. Soft boards lie in runs between landings (the
// checkpoints); each zone ends in a fight clearing.
//
//   assets/models/attic_deep.glb   per zone: deep_<zone> (lit), glow_<zone> (bulbs and LEDs, unlit),
//                                  paint_<zone> (spray marks, unlit so they read in the dark); deep_roof_<k>;
//                                  topple_<k> (the boss chamber's towers, tipped in phase two about a pivot);
//                                  p2_boss (what falls in phase two)
//   src/gen/attic_deep_data.h      solids, soft boards, landings, clearings, the boss ring, entry, rack, terminal
//
//   node tools/prep-attic-deep.mjs
//
// Stage space as the room: +x east, -z north, y up, metres. The plan is drawn with the stair head at the
// origin and shifted west by OX on output, so no part of it lies where the lab's footprint would be
// (world_indoors() is positional and would give the follow camera the lab's low ceiling there).
import { writeFileSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { Chunk, writeGlb } from './glb-lib.mjs';
import {
  HOL, HAL, K, kit, into, solid, setSeed, rnd, R, pick, lin, vary, mix,
  PLY, BOARD, STUD, STUD_OLD, OSB, PINK, PINK_D, PAPER, STEEL, CHROME, BEIGE, ORANGE_PAINT, RED_PAINT, PINK_PAINT, LIDS,
  box, obox, beam, cyl, ball, quad, moved, prop, rack, sprayX, sprayRing, sprayWord, sprayArrow, LAYOUT_TYPES,
} from './attic-kit.mjs';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
setSeed(20261112);

// ---------------------------------------------------------------- the plan
const OX = -30;              // shift on output (see the header)
const W = 2.8, HW = W / 2;   // aisle width: the follow camera wants room
const Q = 6.4;               // a fight clearing's half size: a 5.5 m ring with a margin
const QB = 8.4;              // the boss chamber's half size: a 7 m ring
const D = 1.0;               // how deep a wall of shelving is
const ROOF0 = 11, ROOF1 = 15;   // the roof's eaves and ridge: far above, in the dark
const ZONES = ['holiday', 'members', 'robotics', 'metal', 'boss'];

// the route's centreline: through each clearing's middle, axis aligned
const ROUTE = [[0, 0], [0, -24], [12, -24], [18.4, -24], [18.4, -46], [6, -46], [6, -60.4], [-16, -60.4], [-16, -76.4], [-16, -98], [4.4, -98], [4.4, -124.4]];
const SEG_ZONE = ['holiday', 'holiday', 'holiday', 'members', 'members', 'members', 'robotics', 'robotics', 'metal', 'metal', 'boss'];
const CLEAR = [
  { zone: 'holiday', x: 18.4, z: -24, r: 5.5 },
  { zone: 'members', x: 6, z: -60.4, r: 5.5 },
  { zone: 'robotics', x: -16, z: -76.4, r: 5.5 },
  { zone: 'metal', x: 4.4, z: -98, r: 5.5 },
];
const BOSS = { x: 4.4, z: -124.4, r: 7, r2: 5.2 };
const HOLE = { x0: -0.45, x1: 0.45, z0: 0.75, z1: 2.75 };   // the stair you came up
const ENTRY = { x: 0, z: 0.1, yaw: Math.PI };                // facing north

// arc length along the route
const SEG = []; let ROUTE_LEN = 0;
for (let i = 0; i + 1 < ROUTE.length; i++) {
  const [ax, az] = ROUTE[i], [bx, bz] = ROUTE[i + 1], L = Math.hypot(bx - ax, bz - az);
  SEG.push({ ax, az, s0: ROUTE_LEN, L, dx: (bx - ax) / L, dz: (bz - az) / L, zone: SEG_ZONE[i] });
  ROUTE_LEN += L;
}
function at(s) {
  const g = SEG.find((q) => s <= q.s0 + q.L + 1e-6) || SEG[SEG.length - 1];
  const t = Math.min(g.L, s - g.s0);
  return { x: g.ax + g.dx * t, z: g.az + g.dz * t, dx: g.dx, dz: g.dz, zone: g.zone };
}
const yawOf = (dx, dz) => Math.atan2(dx, dz);          // the game's yaw: 0 faces +z
const textYaw = (dx, dz) => Math.atan2(-dx, -dz);      // sprayWord's yaw so a walker facing (dx, dz) reads it

// landings (checkpoints) and soft runs, by distance along the route; a run drops you to the landing before it
const LANDING_S = [0.5, 12.5, 30, 51, 62.5, 80.5, 100, 111.5, 121.6, 138, 149.3, 162.5, 180, 187.8];
const RUN_S = [[4, 10.5, 0], [15, 21.5, 1], [53.5, 60, 3], [67, 73.5, 4], [102.5, 109, 6], [114.6, 119.8, 7], [140.5, 147, 9], [153, 159.5, 10], [182.4, 186.2, 12]];

// ---------------------------------------------------------------- walkable rectangles
const WALK = [];
for (const g of SEG) {
  const bx = g.ax + g.dx * g.L, bz = g.az + g.dz * g.L;
  WALK.push({ x0: Math.min(g.ax, bx) - HW, x1: Math.max(g.ax, bx) + HW, z0: Math.min(g.az, bz) - HW, z1: Math.max(g.az, bz) + HW, zone: g.zone, kind: 'aisle' });
}
WALK[0].z1 = HOLE.z1 + 0.15;   // the dead end behind the stair head
for (const c of CLEAR) WALK.push({ x0: c.x - Q, x1: c.x + Q, z0: c.z - Q, z1: c.z + Q, zone: c.zone, kind: 'clear' });
WALK.push({ x0: BOSS.x - QB, x1: BOSS.x + QB, z0: BOSS.z - QB, z1: BOSS.z + QB, zone: 'boss', kind: 'boss' });
const inWalk = (x0, z0, x1, z1, pad = 0.01) => WALK.some((r) => x0 < r.x1 - pad && x1 > r.x0 + pad && z0 < r.z1 - pad && z1 > r.z0 + pad);
const walkAt = (x, z) => WALK.find((r) => x >= r.x0 && x <= r.x1 && z >= r.z0 && z <= r.z1);

// ---------------------------------------------------------------- chunks
const CH = {};
for (const z of ZONES) CH[z] = { lit: new Chunk('deep_' + z), glow: new Chunk('glow_' + z), paint: new Chunk('paint_' + z) };
const zoneInto = (z) => into(CH[z].lit, CH[z].glow, CH[z].paint);
const ROOF_CH = [];
const TOPPLE = [];            // { ch, px, py, pz, axis, angle }
const P2 = new Chunk('p2_boss');
// the boss chamber's screens and LEDs, twice: green, and a red twin the runtime swaps in for phase two
const SCR = new Chunk('glow_scr_boss'), RED = new Chunk('glow_red_boss');
let scrXf = null, redXf = null;
function screen(u0, y0, u1, y1, d, c) {
  if (!scrXf) return false;
  front(scrXf, u0, y0, u1, y1, d, c);
  front(redXf, u0, y0, u1, y1, d, mix(0xe03020, c, 0.15));
  return true;
}
const SOLIDS_P2 = [];
const BULBS = [];             // { x, y, z, k }: bare bulbs, for the baked light pools
const LANDINGS = [], SOFT = [];
const FLOOR_DARK = 0x2a2218;  // what the spray halos blend toward: the paint is drawn unlit over a dark floor

// ---------------------------------------------------------------- small builders in a unit's own frame
// A wall unit is built in local (u along the wall, y up, d back from its front edge) and placed by xf.
function unitXf(face, x0, z0, x1, z1) {
  switch (face) {
    case 's': return (u, y, d) => [x1 - u, y, z1 - d];   // its front on the south edge, opening south
    case 'n': return (u, y, d) => [x0 + u, y, z0 + d];
    case 'e': return (u, y, d) => [x1 - d, y, z0 + u];
    default: return (u, y, d) => [x0 + d, y, z1 - u];    // 'w'
  }
}
// a box seen only from the front, top and sides (its back is against the wall, its bottom on a shelf)
const FACE4 = [0, 2, 1, 0, 3, 2, 3, 7, 6, 3, 6, 2, 0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5];
function sbox(ch, u0, y0, d0, u1, y1, d1, c) {
  ch.add(Float32Array.from([u0, y0, d0, u1, y0, d0, u1, y1, d0, u0, y1, d0, u0, y0, d1, u1, y0, d1, u1, y1, d1, u0, y1, d1]), FACE4, Array.isArray(c) ? c : lin(c));
}
// a flat rectangle facing the aisle (-d), at depth d
function front(ch, u0, y0, u1, y1, d, c) { quad(ch, [u0, y0, d], [u0, y1, d], [u1, y1, d], [u1, y0, d], c); }
function top(ch, u0, d0, u1, d1, y, c) { quad(ch, [u0, y, d0], [u1, y, d0], [u1, y, d1], [u0, y, d1], c); }
// a 2x4 upright seen from the aisle: its face and two sides (its back is hidden behind what it holds)
function post(ch, u, y0, y1, c) {
  const a = u - 0.019, b = u + 0.019, f = 0.005, k = 0.095;
  front(ch, a, y0, b, y1, f, c);
  quad(ch, [a, y0, f], [a, y0, k], [a, y1, k], [a, y1, f], c);
  quad(ch, [b, y0, k], [b, y0, f], [b, y1, f], [b, y1, k], c);
}
// a tote's lid: its top and its front lip
function lidOn(ch, u0, u1, y, d0, d1, c) { top(ch, u0 - 0.01, d0 - 0.01, u1 + 0.01, d1, y, c); front(ch, u0 - 0.01, y - 0.045, u1 + 0.01, y, d0 - 0.01, c); }
// a cone standing at (x, y, z)
function cone(ch, x, y, z, r, h, seg, c) {
  const P = [x, y + h, z], I = [];
  for (let i = 0; i < seg; i++) { const a = (i / seg) * Math.PI * 2; P.push(x + Math.cos(a) * r, y, z + Math.sin(a) * r); }
  for (let i = 0; i < seg; i++) I.push(0, 1 + ((i + 1) % seg), 1 + i);
  ch.add(Float32Array.from(P), I, Array.isArray(c) ? c : lin(c));
}

// ---------------------------------------------------------------- what the shelves hold, zone by zone
const HOLIDAY_WRAP = [0xc02020, 0x1f7a3a, 0xd8b030, 0x2f5fc0, 0xe8e0d0, 0x8a2a8a];
const MEMBER_INSIDE = [0x7a6a58, 0x5a6a78, 0x9a7050, 0x50605a, 0x8a5050];
const PLATES = {
  holiday: { front: [0x1e2124, 0x2a2d30, 0xb08655, 0xc8d2d6], lid: [0xc02020, 0x208040, 0xc02020, 0x208040, 0xd8b030] },
  members: { front: [0x4a4d52, 0xc8d2d6, 0xb08655, 0x3a3d42], lid: LIDS },
  robotics: { front: [0x5c6064, 0x6a6e72, 0xc03030, 0x2f5fc0, 0xc8d2d6], lid: [0xd8d8d8, 0x9aa0a6, 0xc03030, 0x2f5fc0] },
  metal: { front: [BEIGE, 0x8f979e, 0x3a3e45, 0xb0b6ba, 0x6e767d], lid: [0x55595f, 0x9aa2a8, BEIGE] },
  boss: { front: [0x26292d, 0x3a3e45, BEIGE, 0x2a2d32], lid: [0x1e2125, 0x55595f] },
};
// one thing on a shelf in [u, u + w], standing at y, at most h tall
function item(ch, zone, u, w, y, h) {
  const d0 = R(0.04, 0.1), d1 = Math.min(D - 0.06, d0 + R(0.4, 0.62));
  const hh = (lo, hi) => Math.min(h, R(lo, hi));
  const r = rnd();
  if (zone === 'holiday') {
    if (r < 0.42) {   // a black tote, red or green lid
      const t = hh(0.3, 0.42), lid = pick([0xc02020, 0x208040]);
      sbox(ch, u, y, d0, u + w, y + t * 0.9, d1, vary(0x24272a, 0.1)); lidOn(ch, u, u + w, y + t, d0, d1, vary(lid, 0.08));
      if (rnd() < 0.6) front(ch, u + w * 0.3, y + t * 0.5, u + w * 0.7, y + t * 0.66, d0 - 0.003, 0xe8dcb0);
    } else if (r < 0.62) {   // a clear tote of lights or ornaments
      const t = hh(0.28, 0.4), inside = pick([0xd03030, 0x30a050, 0xd8b030, 0x4070d0]);
      sbox(ch, u, y, d0, u + w, y + t * 0.9, d1, mix(0xc8d2d6, inside, 0.45)); lidOn(ch, u, u + w, y + t, d0, d1, pick([0xc02020, 0x208040]));
    } else if (r < 0.84) {   // a wrapped present, ribbon round it
      const t = hh(0.2, 0.42), wrap = pick(HOLIDAY_WRAP), rib = pick([0xd8b030, 0xe8e0d0, 0xc02020]);
      const uu = u + R(0, Math.max(0, w - 0.4)), ww = Math.min(w, R(0.25, 0.45));
      sbox(ch, uu, y, d0, uu + ww, y + t, d1, vary(wrap, 0.08));
      front(ch, uu + ww * 0.45, y, uu + ww * 0.55, y + t, d0 - 0.003, rib); top(ch, uu, (d0 + d1) / 2 - 0.02, uu + ww, (d0 + d1) / 2 + 0.02, y + t + 0.003, rib);
    } else if (r < 0.95) {   // an artificial tree's long box, lying down
      const t = hh(0.24, 0.32);
      sbox(ch, u, y, d0, u + w, y + t, d1, vary(0xb08655, 0.1));
      quad(ch, [u + w * 0.2, y + t * 0.15, d0 - 0.003], [u + w * 0.5, y + t * 0.85, d0 - 0.003], [u + w * 0.5, y + t * 0.85, d0 - 0.003], [u + w * 0.8, y + t * 0.15, d0 - 0.003], 0x1f6a34);
    } else {   // Santa hats in a heap
      for (let k = 0; k < 3; k++) { const cx = u + 0.1 + k * Math.min(0.15, w / 3); cone(ch, cx, y, (d0 + d1) / 2, 0.08, Math.min(h, 0.22), 6, 0xc02020); }
    }
  } else if (zone === 'members') {
    if (r < 0.68) {   // a member's tote: any lid, masking tape name, maybe a red tag
      const t = hh(0.3, 0.42), clear = rnd() < 0.45;
      sbox(ch, u, y, d0, u + w, y + t * 0.9, d1, clear ? mix(0xc8d2d6, pick(MEMBER_INSIDE), 0.4) : vary(0x4a4d52, 0.1));
      lidOn(ch, u, u + w, y + t, d0, d1, vary(pick(LIDS), 0.08));
      front(ch, u + w * 0.25, y + t * 0.5, u + w * 0.75, y + t * 0.64, d0 - 0.003, 0xe8dcb0);
      front(ch, u + w * 0.32, y + t * 0.56, u + w * 0.6, y + t * 0.58, d0 - 0.005, 0x1a1a1a);
      if (rnd() < 0.3) front(ch, u + w * 0.78, y + t * 0.55, u + w * 0.9, y + t * 0.8, d0 - 0.006, RED_PAINT);
    } else if (rnd() < 0.8) {   // a cardboard box with a name on it
      const t = hh(0.25, 0.42);
      sbox(ch, u, y, d0, u + w, y + t, d1, vary(0xb08655, 0.12));
      top(ch, u + w * 0.46, d0, u + w * 0.54, d1, y + t + 0.003, 0xc9b27c);
      front(ch, u + w * 0.2, y + t * 0.55, u + w * 0.7, y + t * 0.6, d0 - 0.003, 0x1a1a1a);
      if (rnd() < 0.3) front(ch, u + w * 0.75, y + t * 0.5, u + w * 0.88, y + t * 0.75, d0 - 0.005, RED_PAINT);
    } else {   // a duffel bag
      const rr = Math.min(h / 2, 0.15);
      cyl(ch, [u + 0.02, y + rr, (d0 + d1) / 2], [u + w - 0.02, y + rr, (d0 + d1) / 2], rr, 6, vary(pick([0x2a3a5a, 0x5a2a2a, 0x2a4a2a, 0x1a1a1a]), 0.1));
    }
  } else if (zone === 'robotics') {
    if (r < 0.25) {   // foam field tiles, stacked
      const t = hh(0.2, 0.4);
      sbox(ch, u, y, d0, u + w, y + t, d1, vary(0x5c6064, 0.06));
      for (let k = 1; k < 5; k++) front(ch, u, y + t * k / 5 - 0.006, u + w, y + t * k / 5, d0 - 0.003, 0x3a3e42);
    } else if (r < 0.5) {   // a kit box, red or blue with a white band
      const t = hh(0.18, 0.36), c = pick([0xc03030, 0x2f5fc0]);
      sbox(ch, u, y, d0, u + w, y + t, d1, vary(c, 0.08));
      front(ch, u, y + t * 0.6, u + w, y + t * 0.75, d0 - 0.003, 0xe8e8e8);
    } else if (r < 0.75) {   // a clear parts tote of aluminium
      const t = hh(0.24, 0.34);
      sbox(ch, u, y, d0, u + w, y + t * 0.9, d1, mix(0xc8d2d6, 0xb9c0c6, 0.5)); lidOn(ch, u, u + w, y + t, d0, d1, pick([0xc03030, 0x2f5fc0, 0xd8d8d8]));
      front(ch, u + w * 0.1, y + t * 0.2, u + w * 0.9, y + t * 0.5, d0 - 0.003, 0x8a9096);
    } else if (r < 0.88) {   // omni wheels, stacked
      const n = Math.max(1, Math.floor(Math.min(h, 0.32) / 0.06));
      cyl(ch, [u + w / 2, y, (d0 + d1) / 2], [u + w / 2, y + n * 0.06, (d0 + d1) / 2], Math.min(w / 2, 0.12), 8, 0x2a2a2a);
    } else {   // a bundle of C-channel
      for (let k = 0; k < 3; k++) beam(ch, [u, y + 0.03 + k * 0.05, d0 + 0.05 + k * 0.08], [u + w, y + 0.03 + k * 0.05, d0 + 0.05 + k * 0.08], 0.05, 0.025, 0xb9c0c6);
    }
  } else if (zone === 'metal') {
    if (r < 0.3) {   // bar, tube and angle stock lying across the bay
      for (let k = 0; k < 4; k++) {
        const yy = y + 0.03 + (k % 2) * 0.05, dd = d0 + 0.05 + k * 0.1, c = vary(pick([STEEL, 0x6e767d, 0xa8a090, 0x7d5a3a]), 0.1);
        if (k % 2) cyl(ch, [u - 0.1, yy, dd], [u + w + 0.1, yy, dd], 0.025, 6, c, false); else beam(ch, [u - 0.1, yy, dd], [u + w + 0.1, yy, dd], 0.04, 0.04, c);
      }
    } else if (r < 0.48) {   // a beige tower
      const t = hh(0.34, 0.44), ww = Math.min(w, 0.22);
      sbox(ch, u, y, d0, u + ww, y + t, d1, vary(BEIGE, 0.06)); front(ch, u + 0.02, y + t * 0.7, u + ww - 0.02, y + t * 0.78, d0 - 0.003, 0x9a9278);
    } else if (r < 0.64) {   // a CRT monitor
      const t = hh(0.3, 0.4), ww = Math.min(w, 0.42);
      sbox(ch, u, y, d0 + 0.05, u + ww, y + t, d1, vary(BEIGE, 0.08)); front(ch, u + 0.04, y + 0.06, u + ww - 0.04, y + t - 0.06, d0 + 0.047, 0x2a3430);
    } else if (r < 0.8) {   // servers, stacked flat
      const n = Math.max(1, Math.floor(Math.min(h, 0.36) / 0.09));
      for (let k = 0; k < n; k++) sbox(ch, u, y + k * 0.09, d0, u + w, y + k * 0.09 + 0.085, d1, k % 2 ? 0x3a3e45 : 0x2b2f35);
    } else if (r < 0.9) {   // sheet metal, stacked
      sbox(ch, u, y, d0, u + w, y + 0.06, d1, vary(0xb0b6ba, 0.1));
    } else {   // a clear tote of cables
      const t = hh(0.26, 0.36);
      sbox(ch, u, y, d0, u + w, y + t * 0.9, d1, mix(0xc8d2d6, pick([0x2f6fd0, 0x22252a, 0xe0c020]), 0.45)); lidOn(ch, u, u + w, y + t, d0, d1, 0x2f6fd0);
    }
  } else {   // boss: servers and monitors, some still lit
    if (r < 0.5) {
      const n = Math.max(1, Math.floor(Math.min(h, 0.4) / 0.09));
      for (let k = 0; k < n; k++) { sbox(ch, u, y + k * 0.09, d0, u + w, y + k * 0.09 + 0.085, d1, k % 2 ? 0x2e3238 : 0x24272c); if (rnd() < 0.35) screen(u + w - 0.07, y + k * 0.09 + 0.035, u + w - 0.05, y + k * 0.09 + 0.05, d0 - 0.003, rnd() < 0.7 ? 0x40d070 : 0xffa020); }
    } else {
      const t = hh(0.3, 0.4), ww = Math.min(w, 0.42);
      sbox(ch, u, y, d0 + 0.05, u + ww, y + t, d1, vary(BEIGE, 0.1));
      // some of the dead monitors are not dead
      if (rnd() > 0.35 || !screen(u + 0.04, y + 0.06, u + ww - 0.04, y + t - 0.06, d0 + 0.045, pick([0x2a9a50, 0x1f7a40, 0x3aa070]))) front(ch, u + 0.04, y + 0.06, u + ww - 0.04, y + t - 0.06, d0 + 0.047, 0x1c2a22);
    }
  }
}
// the glow chunk seen through the unit's frame (set per unit, so LEDs land on the right shelf)
let glowXf = null;

// an upper shelf, seen from the aisle far below: fronts, lids and tops only
function plates(ch, zone, u0, u1, y, h) {
  const P = PLATES[zone];
  let u = u0 + 0.04;
  while (u < u1 - 0.25) {
    const high = y > 4.8;   // far overhead and half in the dark: bigger, plainer
    const w = Math.min(u1 - 0.04 - u, high ? R(0.6, 1.1) : R(0.4, 0.85));
    if (rnd() < 0.12) { u += w + 0.03; continue; }
    const t = Math.min(h, R(0.24, 0.44)), d0 = R(0.05, 0.12), f = vary(pick(P.front), 0.1);
    front(ch, u, y, u + w, y + t, d0, f);
    if (zone === 'boss' && rnd() < 0.3) screen(u + 0.05, y + 0.05, u + w - 0.05, y + t - 0.05, d0 - 0.006, pick([0x2a9a50, 0x1f7a40, 0x3aa070, 0x40d070]));   // a monitor left on
    else if (!high) front(ch, u - 0.005, y + t - 0.05, u + w + 0.005, y + t, d0 - 0.008, vary(pick(P.lid), 0.08));
    if (y + t < 4.4) top(ch, u, d0, u + w, Math.min(D - 0.08, d0 + 0.5), y + t, f);   // the follow camera never looks down on the higher ones
    u += w + 0.03;
  }
}

// ---------------------------------------------------------------- the wall units
// One unit of a canyon wall: 2x4 shelving 2.4 m a tier, tiers stacked on tiers (the upper ones set on top a
// little off, sometimes leaning), the bottom tier full of things, the upper ones dressed as plates, junk
// piled on the top. top: the height its solid reaches.
function canyonUnit(xf, L, top0, zone, o = {}) {
  const lit = moved(kit.lit, xf);
  glowXf = moved(kit.glow, xf);
  scrXf = zone === 'boss' && !o.still ? moved(SCR, xf) : null; redXf = scrXf ? moved(RED, xf) : null;
  const wood = zone === 'members' || zone === 'boss' ? STUD_OLD : STUD;
  const tiers = [[0, 2.42, [0.5, 1.0, 1.5, 2.0]]];
  if (top0 > 3.4) tiers.push([2.42, 4.84, [2.97, 3.52, 4.07, 4.6]]);
  if (top0 > 6.0) tiers.push([4.84, 7.26, [5.42, 6.0, 6.6]]);
  if (top0 > 8.0) tiers.push([7.26, 9.68, [7.84, 8.42, 9.0]]);
  const nb = Math.max(1, Math.round(L / 1.2)), bw = L / nb;
  const real = o.real || [0, 1, 2];   // the bottom tier's levels that hold real things; the rest are plates
  let lastTop = 0;
  tiers.forEach(([y0, y1, decks], ti) => {
    // the upper tiers lean a little now and then: overloaded, set on top of the one below by hand
    const lean = ti > 0 && zone !== 'boss' && rnd() < 0.35 ? R(-0.035, 0.05) : 0;   // (the boss's server walls stand straight: their screens are drawn unleaned)
    const q = lean ? moved(lit, (u, y, d) => { const dy = y - y0; return [u, y0 + d * Math.sin(lean) + dy * Math.cos(lean), d * Math.cos(lean) - dy * Math.sin(lean)]; }) : lit;
    const off = ti > 0 ? R(-0.1, 0.1) : 0;
    for (let i = 0; i <= nb; i++) {
      const u = Math.max(0.02, Math.min(L - 0.02, i * bw + off));
      post(q, u, y0, y1, vary(wood, 0.1));
    }
    const deckC = vary(zone === 'members' || zone === 'boss' ? 0x9c8156 : PLY[ti % PLY.length], 0.08);
    for (const y of [...decks, y1]) { top(q, 0, 0.02, L, D - 0.03, y, deckC); front(q, 0, y - 0.04, L, y, 0.02, deckC); }
    const levels = [y0, ...decks];
    levels.forEach((ly, li) => {
      const next = li + 1 < levels.length ? levels[li + 1] : y1, room = next - ly - 0.07;
      for (let b = 0; b < nb; b++) {
        const u0 = b * bw, u1 = u0 + bw;
        if (ti === 0 && real.includes(li)) { let u = u0 + 0.04; while (u < u1 - 0.3) { const w = Math.min(u1 - 0.04 - u, R(0.36, 0.62)); if (rnd() > 0.1) item(q, zone, u, w, ly + 0.005, room); u += w + 0.03; } }
        else plates(q, zone, u0, u1, ly, room);
      }
    });
    lastTop = y1;
  });
  // junk on top: boxes and totes heaped up past the last shelf
  let u = R(0, 0.3);
  while (u < L - 0.4) {
    const w = R(0.5, 1.1), h = R(0.3, 0.8), c = pick([0xb08655, 0x4a4d52, 0xb08655, ...PLATES[zone].front]);
    if (rnd() < 0.55) obox(lit, Math.min(L - 0.3, u + w / 2), lastTop + h / 2, R(0.3, 0.6), w / 2, h / 2, R(0.25, 0.4), R(-0.25, 0.25), vary(c, 0.12));
    if (rnd() < 0.15) obox(lit, Math.min(L - 0.3, u + w / 2), lastTop + h + 0.2, R(0.35, 0.6), w * 0.4, 0.2, 0.25, R(-0.5, 0.5), vary(pick(PLATES[zone].front), 0.12));
    u += w + R(0, 0.4);
  }
  // the back, and the ends of a run
  quad(lit, [0, 0, D - 0.01], [L, 0, D - 0.01], [L, lastTop, D - 0.01], [0, lastTop, D - 0.01], vary(0x3a2e22, 0.1));
  if (o.endA) quad(lit, [0.005, 0, 0], [0.005, 0, D], [0.005, lastTop, D], [0.005, lastTop, 0], vary(0x5a4934, 0.1));
  if (o.endB) quad(lit, [L - 0.005, 0, 0], [L - 0.005, 0, D], [L - 0.005, lastTop, D], [L - 0.005, lastTop, 0], vary(0x5a4934, 0.1));
  // now and then a ladder leans on the front
  if (o.ladder) {
    const lu = R(0.6, L - 0.6), lh = Math.min(lastTop - 0.3, R(3.4, 4.6)), AL = 0xb9c0c6;
    for (const s of [-0.22, 0.22]) beam(lit, [lu + s, 0, -0.42], [lu + s, lh, -0.05], 0.03, 0.07, AL);
    for (let y = 0.3; y < lh - 0.1; y += 0.3) { const dd = -0.42 + (0.37 * y) / lh; beam(lit, [lu - 0.22, y, dd], [lu + 0.22, y, dd], 0.035, 0.03, AL); }
    const a = xf(lu - 0.28, 0, -0.46), b = xf(lu + 0.28, 0, 0);
    solid(a[0], a[2], b[0], b[2], lh);
  }
  return lastTop;
}

// ---------------------------------------------------------------- the walls round the walkable rectangles
const RUNS = [];
const claimed = new Map();
function minus(list, a, b) { const out = []; for (const [p, q] of list) { if (b <= p || a >= q) { out.push([p, q]); continue; } if (a > p) out.push([p, a]); if (b < q) out.push([b, q]); } return out; }
for (const r of WALK) {
  for (const side of ['N', 'S', 'W', 'E']) {
    const xs = side === 'N' || side === 'S';
    const line = side === 'N' ? r.z0 : side === 'S' ? r.z1 : side === 'W' ? r.x0 : r.x1;
    const out = side === 'N' || side === 'W' ? -1 : 1;
    const b0 = line + out * 0.02, b1 = line + out * 0.25, bl = Math.min(b0, b1), bh = Math.max(b0, b1);
    let iv = [xs ? [r.x0, r.x1] : [r.z0, r.z1]];
    for (const o of WALK) {
      if (o === r) continue;
      if (xs ? (o.z0 < bh && o.z1 > bl) : (o.x0 < bh && o.x1 > bl)) iv = minus(iv, xs ? o.x0 : o.z0, xs ? o.x1 : o.z1);
    }
    const key = `${side}:${line.toFixed(3)}`;
    for (const [p, q] of claimed.get(key) || []) iv = minus(iv, p, q);
    const face = { N: 's', S: 'n', W: 'e', E: 'w' }[side];
    const dep = r.kind === 'boss' ? D + 0.2 : D;
    for (let [p, q] of iv) {
      if (q - p < 0.4) continue;
      claimed.set(key, [...(claimed.get(key) || []), [p, q]]);
      const d0 = Math.min(line, line + out * dep), d1 = Math.max(line, line + out * dep);
      // walls along x close the corners: they run on past the ends where nothing walkable is in the way
      if (xs) { if (!inWalk(p - dep, d0, p, d1)) p -= dep; if (!inWalk(q, d0, q + dep, d1)) q += dep; }
      RUNS.push(xs ? { x0: p, x1: q, z0: d0, z1: d1, face, zone: r.zone, kind: r.kind } : { x0: d0, x1: d1, z0: p, z1: q, face, zone: r.zone, kind: r.kind });
    }
  }
}
const HEIGHT = { holiday: [5.0, 6.3], members: [5.4, 7.4], robotics: [5.0, 6.4], metal: [5.6, 7.4], boss: [7.4, 8.6] };
function buildRun(run) {
  zoneInto(run.zone);
  const xs = run.face === 'n' || run.face === 's';
  const L = xs ? run.x1 - run.x0 : run.z1 - run.z0;
  const parts = [];
  let left = L;
  while (left > 0.01) { let w = pick([2.4, 3.6, 2.4, 4.8]); if (left - w < 1.2) w = left; parts.push(w); left -= w; }
  // walk the parts in u order; u runs one way or the other along the run depending on its face
  let u = 0;
  parts.forEach((w, i) => {
    const [lo, hi] = HEIGHT[run.zone];
    const t = R(lo, hi) + (run.kind === 'clear' ? 0.6 : 0);
    // this part's own rectangle, from where it sits in u
    let x0 = run.x0, x1 = run.x1, z0 = run.z0, z1 = run.z1;
    if (run.face === 's') { x1 = run.x1 - u; x0 = x1 - w; }
    else if (run.face === 'n') { x0 = run.x0 + u; x1 = x0 + w; }
    else if (run.face === 'e') { z0 = run.z0 + u; z1 = z0 + w; }
    else { z1 = run.z1 - u; z0 = z1 - w; }
    const xf = unitXf(run.face, x0, z0, x1, z1);
    const lt = canyonUnit(xf, w, t, run.zone, { endA: i === 0, endB: i === parts.length - 1, ladder: run.kind === 'aisle' && w >= 2.4 && rnd() < 0.14, real: run.kind === 'aisle' ? [0, 1, 2] : [0] });
    solid(x0, z0, x1, z1, Math.max(t, lt + 0.4));
    u += w;
  });
}

// ---------------------------------------------------------------- the floor
// Each walkable rectangle less the ones already floored: sheets of plywood the long way, or old boards across.
function subtract(a, b) {
  if (b.x0 >= a.x1 || b.x1 <= a.x0 || b.z0 >= a.z1 || b.z1 <= a.z0) return [a];
  const out = [];
  if (b.z0 > a.z0) out.push({ ...a, z1: b.z0 });
  if (b.z1 < a.z1) out.push({ ...a, z0: b.z1 });
  const z0 = Math.max(a.z0, b.z0), z1 = Math.min(a.z1, b.z1);
  if (b.x0 > a.x0) out.push({ ...a, z0, z1, x1: b.x0 });
  if (b.x1 < a.x1) out.push({ ...a, z0, z1, x0: b.x1 });
  return out;
}
const FLOOR_KIND = { holiday: 'ply', members: 'boards', robotics: 'ply', metal: 'osb', boss: 'boards' };
function floorPiece(p) {
  const ch = CH[p.zone].lit, along = p.x1 - p.x0 > p.z1 - p.z0 ? 'x' : 'z', kind = FLOOR_KIND[p.zone];
  const g = 0.006;
  const sheet = (x0, z0, x1, z1, c) => {   // split to about 1.2 m so the baked light has vertices
    const nx = Math.max(1, Math.round((x1 - x0) / 1.4)), nz = Math.max(1, Math.round((z1 - z0) / 1.4));
    for (let i = 0; i < nx; i++) for (let j = 0; j < nz; j++) {
      const a = x0 + ((x1 - x0) * i) / nx, b = x0 + ((x1 - x0) * (i + 1)) / nx, cc = z0 + ((z1 - z0) * j) / nz, d = z0 + ((z1 - z0) * (j + 1)) / nz;
      quad(ch, [a, 0, d], [b, 0, d], [b, 0, cc], [a, 0, cc], c);
    }
  };
  if (kind === 'boards') {   // old boards across the way, 14 cm
    const step = 0.14;
    if (along === 'z') for (let z = p.z0; z < p.z1 - 1e-3; z += step) sheet(p.x0, z + g, p.x1, Math.min(p.z1, z + step) - g, vary(pick(BOARD), 0.1));
    else for (let x = p.x0; x < p.x1 - 1e-3; x += step) sheet(x + g, p.z0, Math.min(p.x1, x + step) - g, p.z1, vary(pick(BOARD), 0.1));
  } else {
    const SW = 1.22, SL = 2.44, base = kind === 'osb' ? [OSB, 0x8e7448, 0xa4885a] : PLY;
    const [du, dv] = along === 'z' ? [SW, SL] : [SL, SW];
    for (let x = p.x0; x < p.x1 - 1e-3; x += du) for (let z = p.z0; z < p.z1 - 1e-3; z += dv) sheet(x + g, z + g, Math.min(p.x1, x + du) - g, Math.min(p.z1, z + dv) - g, vary(pick(base), 0.07));
  }
}
function floors() {
  let done = [HOLE];
  for (const r of WALK) {
    let pieces = [{ ...r }];
    for (const d of done) pieces = pieces.flatMap((p) => subtract(p, d));
    for (const p of pieces) if (p.x1 - p.x0 > 0.02 && p.z1 - p.z0 > 0.02) floorPiece(p);
    done.push(r);
  }
}

// ---------------------------------------------------------------- the stair head
function stairHead() {
  zoneInto('holiday');
  const room = kit.lit, { x0, x1, z0, z1 } = HOLE;
  box(room, x0 - 0.05, -0.25, z0 - 0.05, x0, 0, z1, STUD_OLD);
  box(room, x1, -0.25, z0 - 0.05, x1 + 0.05, 0, z1, STUD_OLD);
  box(room, x0, -0.25, z0 - 0.05, x1, 0, z0, STUD_OLD);
  for (let i = 0; i < 7; i++) {
    const zt = z0 + 0.05 + i * 0.225, y = -0.25 - i * 0.25;
    box(room, x0 + 0.02, y - 0.03, zt, x1 - 0.02, y, zt + 0.25, vary(0x6c5440));
    box(room, x0 + 0.02, y - 0.25, zt + 0.22, x1 - 0.02, y, zt + 0.25, 0x3a2e22);
  }
  box(room, x0 - 0.06, -2.0, z0, x0 - 0.05, -0.25, z1, 0x2a2018);
  box(room, x1 + 0.05, -2.0, z0, x1 + 0.06, -0.25, z1, 0x2a2018);
  for (const x of [x0 - 0.04, x1 + 0.04]) {
    for (const z of [z0 + 0.1, (z0 + z1) / 2, z1 - 0.12]) beam(room, [x, 0, z], [x, 1.0, z], 0.089, 0.038, vary(STUD));
    beam(room, [x, 1.0, z0 + 0.05], [x, 1.0, z1 - 0.05], 0.089, 0.038, vary(STUD));
    beam(room, [x, 0.5, z0 + 0.05], [x, 0.5, z1 - 0.05], 0.089, 0.038, vary(STUD));
  }
  solid(x0 - 0.1, z0 + 0.05, x1 + 0.1, z1, 1.05);
  for (let i = 0; i < 6; i++) box(room, x0 + i * 0.15, 0, z0 - 0.06, x0 + i * 0.15 + 0.15, 0.003, z0, i & 1 ? 0x1b1b1b : 0xf2c21b);
}

// ---------------------------------------------------------------- bulbs on long cords from the dark
function roofAt(x) { const xm = (XMIN + XMAX) / 2, hw = (XMAX - XMIN) / 2; return ROOF1 - (ROOF1 - ROOF0) * Math.min(1, Math.abs(x - xm) / hw); }
let XMIN = 0, XMAX = 0, ZMIN = 0, ZMAX = 0;
function bulb(zone, x, z, y, k = 1, reach = 7.5) {
  const C = CH[zone];
  cyl(C.lit, [x, y + 0.24, z], [x, y + 0.1, z], 0.045, 5, 0xe9e2d0, false);
  ball(C.glow, x, y + 0.03, z, 0.065, 0xfff0c8, 3);
  cyl(C.lit, [x, y + 0.24, z], [x + R(-0.1, 0.1), roofAt(x) - 0.3, z + R(-0.1, 0.1)], 0.008, 3, 0x1a1a1a, false);
  BULBS.push({ x, y, z, k, reach });
}

// ---------------------------------------------------------------- landings and soft runs
function landing(i, s) {
  const p = at(s), zone = p.zone, C = CH[zone];
  const ux = p.dz !== 0 ? 1 : 0, uz = p.dz !== 0 ? 0 : 1;   // across the way
  const yaw = yawOf(p.dx, p.dz);
  // a newer sheet of plywood screwed down over the old floor
  const hx = ux ? 1.25 : 1.1, hz = ux ? 1.1 : 1.25;
  quad(C.lit, [p.x - hx, 0.004, p.z + hz], [p.x + hx, 0.004, p.z + hz], [p.x + hx, 0.004, p.z - hz], [p.x - hx, 0.004, p.z - hz], vary(0xd2b88a, 0.05));
  for (const [sx, sz] of [[-1, -1], [1, -1], [1, 1], [-1, 1]]) box(C.lit, p.x + sx * (hx - 0.08) - 0.01, 0.004, p.z + sz * (hz - 0.08) - 0.01, p.x + sx * (hx - 0.08) + 0.01, 0.008, p.z + sz * (hz - 0.08) + 0.01, 0x8a8a8a);
  into(C.lit, C.glow, C.paint);
  sprayRing(p.x, p.z, 0.95, PINK_PAINT, FLOOR_DARK, 0.004);
  sprayWord('OK', p.x, p.z, 0.42, textYaw(p.dx, p.dz), PINK_PAINT, FLOOR_DARK, 0.004);
  bulb(zone, p.x + ux * 0.6, p.z + uz * 0.6, 2.9, 1.25);
  LANDINGS.push({ x: p.x, z: p.z, yaw });
  void i;
}
function softRun(s0, s1, drop) {
  const a = at(s0), zone = a.zone, C = CH[zone];
  into(C.lit, C.glow, C.paint);
  const ux = a.dz !== 0 ? 1 : 0, uz = a.dz !== 0 ? 0 : 1;   // across the way
  const len = s1 - s0, amp = 0.72, lam = R(4.2, 5.2), ph = R(0, Math.PI * 2);
  const path = (t) => amp * Math.sin((2 * Math.PI * t) / lam + ph);
  const r = 0.5;
  let row = 0;
  for (let t = 0.3; t <= len - 0.2; t += 0.9, row++) {
    const vs = row % 2 ? [-0.95, -0.32, 0.32, 0.95] : [-1.1, -0.63, 0, 0.63, 1.1];
    for (const v0 of vs) {
      const v = v0 + R(-0.05, 0.05);
      if (Math.abs(v - path(t)) < 0.8) continue;   // the way through stays unmarked
      const p = at(s0 + t), x = p.x + ux * v, z = p.z + uz * v;
      // the soft board: a darker, water stained patch, then the paint
      const sh = R(0.4, 0.48);
      quad(C.lit, [x - sh, 0.002, z + sh], [x + sh, 0.002, z + sh], [x + sh, 0.002, z - sh], [x - sh, 0.002, z - sh], vary(0x4a3826, 0.15));
      sprayX(x, z, R(0.2, 0.27), rnd() < 0.7 ? ORANGE_PAINT : RED_PAINT, FLOOR_DARK, 0, R(-0.45, 0.45));
      if (rnd() < 0.35) sprayRing(x, z, 0.4, ORANGE_PAINT, FLOOR_DARK);
      SOFT.push({ x, z, r, landing: drop });
    }
  }
  // a word at the start, facing whoever comes up the aisle
  const w0 = at(s0 - 0.9);
  sprayWord(pick(['SOFT', 'BAD', 'NO']), w0.x, w0.z, 0.5, textYaw(a.dx, a.dz), RED_PAINT, FLOOR_DARK);
  // and a pink arrow to the way in
  const p0 = at(s0 + 0.2), v = path(0.2);
  sprayArrow(w0.x + ux * v * 0.5 - a.dx * 0.3, w0.z + uz * v * 0.5 - a.dz * 0.3, p0.x + ux * v, p0.z + uz * v, PINK_PAINT, FLOOR_DARK);
}

// ---------------------------------------------------------------- the roof
function roof() {
  const xm = (XMIN + XMAX) / 2, BAND = 30;
  for (let zb = ZMIN; zb < ZMAX; zb += BAND) {
    const ch = new Chunk('deep_roof_' + ROOF_CH.length);
    ROOF_CH.push(ch);
    for (let z = zb; z < Math.min(ZMAX, zb + BAND) - 1e-3; z += 1.83) {
      const za = z, zc = Math.min(ZMAX, z + 1.83);
      for (const xe of [XMIN, XMAX]) {
        const A = [xe, ROOF0 + 0.14, za], B = [xm, ROOF1 + 0.14, za], C2 = [xm, ROOF1 + 0.14, zc], D2 = [xe, ROOF0 + 0.14, zc];
        quad(ch, A, B, C2, D2, vary(OSB, 0.08));
        beam(ch, [xe, ROOF0, za], [xm, ROOF1, za], 0.05, 0.2, vary(STUD_OLD, 0.06));
        if (rnd() < 0.65) {   // pink batts between the rafters, some sagging
          const S = 4;
          for (let i = 0; i < S; i++) {
            const t0 = i / S, t1 = (i + 1) / S, xa = xe + (xm - xe) * t0, xb = xe + (xm - xe) * t1;
            const ya = ROOF0 + (ROOF1 - ROOF0) * t0 + 0.02 - R(0, 0.1), yb = ROOF0 + (ROOF1 - ROOF0) * t1 + 0.02 - R(0, 0.1);
            quad(ch, [xa, ya, za + 0.05], [xb, yb, za + 0.05], [xb, yb, zc - 0.05], [xa, ya, zc - 0.05], i % 2 ? PINK : vary(PINK_D, 0.05));
          }
          if (rnd() < 0.15) {   // a batt torn loose, hanging down into the dark
            const xh = xe + (xm - xe) * R(0.2, 0.8), yh = roofAt(xh);
            quad(ch, [xh - 0.3, yh, za + 0.1], [xh + 0.3, yh, za + 0.1], [xh + 0.35, yh - R(1.5, 3), za + 0.3], [xh - 0.25, yh - R(1.5, 3), za + 0.3], PINK_D);
          }
        }
      }
      if (Math.round((z - ZMIN) / 1.83) % 3 === 0) beam(ch, [xm - 9, ROOF1 - 2.2, za], [xm + 9, ROOF1 - 2.2, za], 0.05, 0.2, vary(STUD_OLD, 0.06));
    }
    beam(ch, [xm, ROOF1, zb], [xm, ROOF1, Math.min(ZMAX, zb + BAND)], 0.05, 0.3, STUD_OLD);
    // the low side walls under the eaves, and the gables at the two ends: nothing past them but the dark
    const z1 = Math.min(ZMAX, zb + BAND);
    for (const xe of [XMIN, XMAX]) quad(ch, [xe, -0.05, zb], [xe, -0.05, z1], [xe, ROOF0 + 0.2, z1], [xe, ROOF0 + 0.2, zb], 0x241c14);
    for (const ze of [ZMIN, ZMAX]) if (ze >= zb - 1e-6 && ze <= z1 + 1e-6) {
      quad(ch, [XMIN, -0.05, ze], [XMAX, -0.05, ze], [XMAX, ROOF0 + 0.2, ze], [XMIN, ROOF0 + 0.2, ze], 0x241c14);
      ch.add(Float32Array.from([XMIN, ROOF0 + 0.2, ze, XMAX, ROOF0 + 0.2, ze, xm, ROOF1 + 0.2, ze]), [0, 1, 2], lin(0x241c14));
    }
  }
}

// ---------------------------------------------------------------- clearings and the boss chamber
// set pieces stand in the corners of a clearing's square and in the margin past its ring
const corner = (c, sx, sz, inset = 1.0) => ({ x: c.x + sx * (Q - inset), z: c.z + sz * (Q - inset) });
function solidAround(x, z, hx, hz, top) { solid(x - hx, z - hz, x + hx, z + hz, top); }
// a giant present: box, ribbons, a bow
function giantPresent(ch, x, z, s, yaw) {
  const wrap = pick(HOLIDAY_WRAP), rib = pick([0xd8b030, 0xe8e0d0, 0xc02020].filter((c) => c !== wrap));
  obox(ch, x, s / 2, z, s / 2, s / 2, s / 2, yaw, vary(wrap, 0.06));
  obox(ch, x, s / 2, z, s * 0.08, s / 2 + 0.01, s / 2 + 0.01, yaw, rib);
  obox(ch, x, s / 2, z, s / 2 + 0.01, s / 2 + 0.01, s * 0.08, yaw, rib);
  ball(ch, x, s + s * 0.08, z, s * 0.12, rib, 4);
}
// a string of lights sagging between two points, the bulbs in the glow chunk
function lightString(zone, a, b, sag, n) {
  const C = CH[zone];
  const P = (t) => [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t - sag * 4 * t * (1 - t), a[2] + (b[2] - a[2]) * t];
  const cols = [0xff4030, 0x40ff60, 0xffd040, 0x4080ff, 0xff60c0];
  for (let i = 0; i < 12; i++) cyl(C.lit, P(i / 12), P((i + 1) / 12), 0.008, 3, 0x1a3a1a, false);
  for (let i = 1; i < n; i++) { const p = P(i / n); box(C.glow, p[0] - 0.035, p[1] - 0.09, p[2] - 0.035, p[0] + 0.035, p[1] - 0.02, p[2] + 0.035, cols[i % cols.length]); }
}

async function holidayClearing(c) {
  zoneInto('holiday');
  const ch = kit.lit;
  // the far corner: a tree twice as tall as anyone, still decorated from the last HAXMAS
  const t = corner(c, 1, -1, 1.35);
  await prop(HOL('tree-decorated'), t.x, 0, t.z, { h: 5.2, yaw: 0.4 });
  solidAround(t.x, t.z, 1.0, 1.0, 5.2);
  // presents as big as crates heaped in another corner
  const p = corner(c, 1, 1, 1.0);
  giantPresent(ch, p.x, p.z, 1.6, 0.3); giantPresent(ch, p.x - 0.6, p.z - 0.2, 0.9, -0.4); giantPresent(ch, p.x + 0.1, p.z + 0.1, 0.8, 0.9);
  solidAround(p.x, p.z, 0.95, 0.95, 1.9);
  // a nutcracker the height of the shelving guards the way on
  const n = corner(c, -1, -1, 1.0);
  await prop(HOL('nutcracker'), n.x, 0, n.z, { h: 4.2, yaw: Math.PI * 0.25 });
  solidAround(n.x, n.z, 0.7, 0.7, 4.2);
  const s = corner(c, -1, 1, 1.0);
  await prop(HOL('snowman-hat'), s.x, 0, s.z, { h: 2.6, yaw: Math.PI * 0.75 });
  solidAround(s.x, s.z, 0.85, 0.85, 2.4);
  // candy canes like lamp posts either side of the openings
  for (const [x, z, yaw] of [[c.x - Q + 0.35, c.z - 2.0, 0], [c.x - Q + 0.35, c.z + 2.0, 0], [c.x - 2.0, c.z - Q + 0.35, Math.PI / 2], [c.x + 2.0, c.z - Q + 0.35, Math.PI / 2]]) {
    await prop(HOL(rnd() < 0.5 ? 'candy-cane-red' : 'candy-cane-green'), x, 0, z, { h: 3.2, yaw });
    solidAround(x, z, 0.25, 0.25, 3.2);
  }
  // a wreath hung high on the east wall, and strings of lights across the top
  await prop(HOL('wreath-decorated'), c.x + Q - 0.25, 3.0, c.z, { h: 2.4, yaw: -Math.PI / 2 });
  for (let k = 0; k < 4; k++) lightString('holiday', [c.x - Q, R(4.4, 5.2), c.z - Q + 1.5 + k * 3.2], [c.x + Q, R(4.4, 5.2), c.z - Q + 2.5 + k * 2.8], R(0.8, 1.4), 18);
  lightString('holiday', [c.x - Q + 1, 0.02, c.z + 3.5], [c.x + 1.5, 0.02, c.z + 4.6], 0, 10);   // one dropped on the floor
  // tinsel and baubles on the floor, nothing to trip on
  for (let k = 0; k < 30; k++) { const a = R(0, Math.PI * 2), rr = R(1, 5.2); ball(ch, c.x + Math.cos(a) * rr, 0.05, c.z + Math.sin(a) * rr, 0.05, pick(HOLIDAY_WRAP), 3); }
  for (const [dx, dz] of [[0, 0], [-3, -2.5], [3, 2.5], [3, -3]]) bulb('holiday', c.x + dx, c.z + dz, R(3.6, 4.3), 1.0, 9.5);
}

function giantTote(ch, x, y, z, w, d, h, yaw, lid, o = {}) {
  obox(ch, x, y + h * 0.43, z, w / 2 * 0.95, h * 0.43, d / 2 * 0.95, yaw, o.clear ? mix(0xc8d2d6, pick(MEMBER_INSIDE), 0.4) : vary(0x4a4d52, 0.08));
  obox(ch, x, y + h * 0.93, z, w / 2, h * 0.07, d / 2, yaw, vary(lid, 0.06));
  const cs = Math.cos(yaw), sn = Math.sin(yaw), fx = sn * (d / 2 * 0.95 + 0.01), fz = cs * (d / 2 * 0.95 + 0.01);
  obox(ch, x + fx, y + h * 0.6, z + fz, w * 0.22, h * 0.07, 0.005, yaw, 0xe8dcb0);
  if (o.tag) obox(ch, x + fx * 1.02 + cs * w * 0.32, y + h * 0.72, z + fz * 1.02 - sn * w * 0.32, w * 0.06, h * 0.1, 0.006, yaw, RED_PAINT);
}
function membersClearing(c) {
  zoneInto('members');
  const ch = kit.lit;
  // totes the size of cars, stacked in the corners, every one red tagged: UNCLAIMED
  for (const [sx, sz] of [[-1, -1], [1, -1], [1, 1], [-1, 1]]) {
    const p = corner(c, sx, sz, 1.0), yaw = Math.atan2(-sx, -sz) + R(-0.12, 0.12);   // their fronts to the ring
    let y = 0;
    for (let k = 0; k < 3; k++) { const h = R(0.9, 1.15); giantTote(ch, p.x + R(-0.1, 0.1), y, p.z + R(-0.1, 0.1), 1.7, 1.2, h, yaw + R(-0.1, 0.1), pick([0x2f6fd0, 0xc02020, 0x208040, 0xe0c020, 0x8040c0]), { clear: rnd() < 0.6, tag: true }); y += h; }
    solidAround(p.x, p.z, 0.95, 0.95, y);
  }
  // where the totes used to sit: masking tape squares on the floor with names in marker, most of them empty
  for (let i = -2; i <= 2; i++) for (let j = -2; j <= 2; j++) {
    const x = c.x + i * 1.7, z = c.z + j * 1.7;
    if (Math.hypot(x - c.x, z - c.z) > 4.6 || rnd() < 0.25) continue;
    const tape = (x0, z0, x1, z1) => quad(kit.paint, [x0, 0.004, z1], [x1, 0.004, z1], [x1, 0.004, z0], [x0, 0.004, z0], 0x8a8060);
    tape(x - 0.6, z - 0.42, x + 0.6, z - 0.36); tape(x - 0.6, z + 0.36, x + 0.6, z + 0.42); tape(x - 0.6, z - 0.42, x - 0.54, z + 0.42); tape(x + 0.54, z - 0.42, x + 0.6, z + 0.42);
    quad(kit.paint, [x - 0.3, 0.005, z + 0.06], [x + 0.25, 0.005, z + 0.06], [x + 0.25, 0.005, z + 0.02], [x - 0.3, 0.005, z + 0.02], 0x1a1a1a);
    if (rnd() < 0.3) quad(kit.paint, [x + 0.3, 0.005, z - 0.1], [x + 0.45, 0.005, z - 0.1], [x + 0.45, 0.005, z - 0.3], [x + 0.3, 0.005, z - 0.3], RED_PAINT);
  }
  // a cardboard box with a name on it leaning in the margin, and one tote spilled open
  obox(ch, c.x + Q - 0.45, 1.1, c.z + 2.6, 0.35, 1.1, 1.0, 0, vary(0xb08655, 0.08));
  obox(ch, c.x + Q - 0.09, 1.3, c.z + 2.6, 0.005, 0.08, 0.6, 0, 0x1a1a1a);
  solidAround(c.x + Q - 0.45, c.z + 2.6, 0.36, 1.0, 2.2);
  // labels and tags scattered over the floor
  for (let k = 0; k < 40; k++) { const a = R(0, Math.PI * 2), rr = R(0.5, 5.3), x = c.x + Math.cos(a) * rr, z = c.z + Math.sin(a) * rr, s = R(0.05, 0.1), q = R(0, 3); obox(ch, x, 0.003, z, s, 0.002, s * 0.6, q, rnd() < 0.3 ? RED_PAINT : 0xe8dcb0); }
  for (const [dx, dz] of [[0, 0], [-3, 3], [3, -3], [3, 3]]) bulb('members', c.x + dx, c.z + dz, R(3.6, 4.3), 1.0, 9.5);
}

function omniWheel(ch, x, z, rad, thick, yaw) {
  const cs = Math.cos(yaw), sn = Math.sin(yaw), ax = [cs, 0, -sn];
  const a = [x - ax[0] * thick / 2, rad, z - ax[2] * thick / 2], b = [x + ax[0] * thick / 2, rad, z + ax[2] * thick / 2];
  cyl(ch, a, b, rad * 0.82, 14, 0x8a8e94);
  cyl(ch, [a[0] - ax[0] * 0.02, rad, a[2] - ax[2] * 0.02], [b[0] + ax[0] * 0.02, rad, b[2] + ax[2] * 0.02], rad * 0.22, 8, 0x9aa0a6);
  for (let i = 0; i < 10; i++) {   // the rollers round the rim
    const t = (i / 10) * Math.PI * 2, py = rad + Math.sin(t) * rad * 0.9, pr = Math.cos(t) * rad * 0.9;
    const px = x + sn * pr, pz = z + cs * pr;
    cyl(ch, [px - ax[0] * thick * 0.35, py, pz - ax[2] * thick * 0.35], [px + ax[0] * thick * 0.35, py, pz + ax[2] * thick * 0.35], rad * 0.13, 6, 0x2a2c30);
  }
}
function roboticsClearing(c) {
  zoneInto('robotics');
  const ch = kit.lit;
  // the field: grey foam tiles over the whole clearing, the edge strips lighter
  for (let i = 0; i < 21; i++) for (let j = 0; j < 21; j++) {
    const x0 = c.x - Q + i * 0.61, z0 = c.z - Q + j * 0.61;
    quad(ch, [x0 + 0.004, 0.012, z0 + 0.606], [x0 + 0.606, 0.012, z0 + 0.606], [x0 + 0.606, 0.012, z0 + 0.004], [x0 + 0.004, 0.012, z0 + 0.004], (i + j) % 2 ? 0x7a7e82 : 0x84888c);
  }
  // giant omni wheels on their rims in two corners, game cubes the size of crates in the others
  const w1 = corner(c, 1, 1, 1.1), w2 = corner(c, -1, -1, 1.1);
  omniWheel(ch, w1.x, w1.z, 1.3, 0.55, Math.PI / 4); solidAround(w1.x, w1.z, 0.9, 0.9, 2.6);
  omniWheel(ch, w2.x, w2.z, 1.3, 0.55, -Math.PI / 4); solidAround(w2.x, w2.z, 0.9, 0.9, 2.6);
  for (const [sx, sz] of [[1, -1], [-1, 1]]) {
    const p = corner(c, sx, sz, 1.0);
    obox(ch, p.x - 0.3, 0.6, p.z, 0.6, 0.6, 0.6, R(-0.3, 0.3), 0xc03030); obox(ch, p.x + 0.45, 0.45, p.z + 0.4, 0.45, 0.45, 0.45, R(-0.3, 0.3), 0x2f5fc0);
    obox(ch, p.x - 0.1, 1.2 + 0.4, p.z + 0.1, 0.4, 0.4, 0.4, R(0, 1), 0x2f5fc0);
    cone(ch, p.x + 0.5, 0, p.z - 0.5, 0.4, 1.1, 8, 0xe0c020);
    solidAround(p.x, p.z, 0.95, 0.95, 2.0);
  }
  // a chassis the size of a car frame: C-channel rails, a crossbar, an arm reaching up out of the corner
  const AL = 0xb9c0c6, fx = c.x + Q - 0.5, fz = c.z;
  beam(ch, [fx - 0.1, 0.4, fz - 2.4], [fx - 0.1, 0.4, fz + 2.4], 0.14, 0.4, AL);
  beam(ch, [fx - 0.1, 0.4, fz], [fx - 0.2, 4.2, fz - 1.2], 0.18, 0.18, AL);
  beam(ch, [fx - 0.2, 4.2, fz - 1.2], [fx - 0.3, 3.6, fz - 2.2], 0.12, 0.12, AL);
  solid(fx - 0.35, fz - 2.5, fx + 0.2, fz + 2.5, 0.8);
  // field perimeter panels along the west margin
  for (let k = -2; k <= 2; k++) { box(ch, c.x - Q + 0.25, 0, c.z + k * 1.2 - 0.58, c.x - Q + 0.29, 0.6, c.z + k * 1.2 + 0.58, mix(0xc8d2d6, 0x8090a0, 0.3)); box(ch, c.x - Q + 0.22, 0, c.z + k * 1.2 - 0.6, c.x - Q + 0.32, 0.62, c.z + k * 1.2 - 0.56, 0x9aa0a6); }
  solid(c.x - Q + 0.2, c.z - 3.0, c.x - Q + 0.34, c.z + 3.0, 0.62);
  for (const [dx, dz] of [[0, 0], [-3, -3], [3, 3], [-3, 3]]) bulb('robotics', c.x + dx, c.z + dz, R(3.6, 4.3), 1.0, 9.5);
}

function metalClearing(c) {
  zoneInto('metal');
  const ch = kit.lit;
  // pipe and tube like culverts, lying along the south margin (the way on is north)
  for (const sz of [1]) {
    const z = c.z + sz * (Q - 0.42);
    cyl(ch, [c.x - 4.6, 0.36, z], [c.x + 4.6, 0.36, z], 0.36, 10, vary(STEEL, 0.08));
    cyl(ch, [c.x - 4.2, 0.95, z + sz * 0.05], [c.x + 3.4, 0.95, z + sz * 0.05], 0.24, 8, vary(0x6e767d, 0.08));
    solid(c.x - 4.6, z - 0.37, c.x + 4.6, z + 0.37, 1.2);
  }
  // a wall of CRTs in a corner, some still lit
  const p = corner(c, -1, -1, 1.0);
  for (let i = 0; i < 2; i++) for (let j = 0; j < 3; j++) {
    const x = p.x - 0.45 + i * 0.95, y = j * 0.82, z = p.z + R(-0.08, 0.08);
    box(ch, x - 0.44, y, z - 0.6, x + 0.44, y + 0.8, z + 0.25, vary(BEIGE, 0.08));
    box(rnd() < 0.6 ? kit.glow : ch, x - 0.34, y + 0.12, z + 0.252, x + 0.34, y + 0.68, z + 0.26, rnd() < 0.5 ? 0x1f6a3a : 0x204a30);
  }
  solidAround(p.x, p.z, 0.95, 0.65, 2.5);
  // beige towers as tall as a door in another corner
  const q = corner(c, 1, 1, 1.0);
  for (let k = 0; k < 3; k++) { const x = q.x - 0.6 + k * 0.6; box(ch, x - 0.25, 0, q.z - 0.8, x + 0.25, 2.1 - k * 0.2, q.z + 0.5, vary(BEIGE, 0.06)); box(ch, x - 0.2, 1.5 - k * 0.2, q.z + 0.505, x + 0.2, 1.62 - k * 0.2, q.z + 0.51, 0x9a9278); }
  solidAround(q.x, q.z, 0.95, 0.75, 2.1);
  // an I-beam leaning in a third, and a crate of offcuts
  const r = corner(c, 1, -1, 1.0);
  beam(ch, [r.x - 0.4, 0, r.z + 0.4], [r.x + 0.5, 4.6, r.z - 0.5], 0.3, 0.3, 0x5a6066);
  box(ch, r.x - 0.6, 0, r.z - 0.2, r.x + 0.2, 0.7, r.z + 0.6, 0x6a5a40);
  solidAround(r.x, r.z, 0.8, 0.8, 1.0);
  const s = corner(c, -1, 1, 1.0);
  for (let k = 0; k < 3; k++) obox(ch, s.x + k * 0.12, 1.3, s.z, 0.02, 1.3, 0.9, 0.3 + k * 0.1, vary(0xb0b6ba, 0.1));
  solidAround(s.x, s.z, 0.6, 0.9, 2.6);
  // shavings on the floor
  for (let k = 0; k < 50; k++) { const a = R(0, Math.PI * 2), rr = R(0.4, 5.2); obox(ch, c.x + Math.cos(a) * rr, 0.003, c.z + Math.sin(a) * rr, R(0.02, 0.06), 0.002, 0.008, R(0, 3), 0xc8ccd0); }
  for (const [dx, dz] of [[0, 0], [-3, 3], [3, -3], [3, 3]]) bulb('metal', c.x + dx, c.z + dz, R(3.6, 4.3), 1.0, 9.5);
}

// the boss chamber: tall walls of servers and dead monitors, the rack in the far west corner, cables over the
// floor, and four overloaded towers that tip in over the ring in phase two
const BOSS_OUT = {};
function bossChamber() {
  zoneInto('boss');
  const ch = kit.lit, c = BOSS;
  const rx = c.x - QB + 1.6, rz = c.z - QB + 0.75;
  BOSS_OUT.rack = { x: rx, z: rz };
  BOSS_OUT.term = rack(rx, rz).terminal;
  // the rack framed by stacks of monitors either side, all lit, throwing green on the floor in front
  scrXf = (u, y, d) => [u, y, d]; scrXf = moved(SCR, scrXf); redXf = moved(RED, (u, y, d) => [u, y, d]);
  for (const [dx, h] of [[-0.95, 2.1], [0.95, 2.1], [1.85, 1.3]]) {
    for (let y = 0; y < h; y += 0.42) {
      box(ch, rx + dx - 0.4, y, rz - 0.4, rx + dx + 0.4, y + 0.4, rz + 0.35, vary(BEIGE, 0.1));
      screen(rx + dx - 0.3, y + 0.06, rx + dx + 0.3, y + 0.34, rz + 0.36, rnd() < 0.7 ? 0x30d070 : 0x1f8a4a);
    }
    solid(rx + dx - 0.42, rz - 0.42, rx + dx + 0.42, rz + 0.38, h);
    BULBS.push({ x: rx + dx, y: h * 0.5, z: rz + 0.9, k: 0.9, reach: 4.5, tint: [0.35, 1.35, 0.6] });
  }
  scrXf = redXf = null;
  // cables: out of the rack, fanned over the floor to the walls and up into the dark
  const CC = [0x2f6fd0, 0xe0c020, 0xd03030, 0x30a050, 0x1a1a1a, 0xe8e8e8, 0xff8030];
  for (let k = 0; k < 26; k++) {
    const a = R(-0.15, Math.PI / 2 + 0.15), L = R(5, 15), cc = pick(CC), wob = R(0.5, 2), ph = R(0, 6);
    let px = rx + 0.1, pz = rz + 0.55;
    for (let i = 1; i <= 14; i++) {
      const t = i / 14, d = L * t, ang = a + Math.sin(t * 4 + ph) * 0.25 * wob / L;
      const nx = rx + 0.1 + Math.sin(ang) * d, nz = rz + 0.55 + Math.cos(ang) * d;
      cyl(ch, [px, 0.03, pz], [nx, 0.03, nz], 0.025, 4, cc, false);
      px = nx; pz = nz;
    }
  }
  for (let k = 0; k < 9; k++) {   // bundles out of the rack's top, up the back walls into the rafters
    const x0 = rx + R(-0.3, 0.3), z0 = rz + R(-0.4, 0.1), along = rnd() < 0.5, x1 = along ? x0 + R(2, 12) : x0 - R(0.2, 0.6), z1 = along ? z0 - R(0.2, 0.6) : z0 + R(2, 12);
    for (let i = 0; i < 8; i++) { const t0 = i / 8, t1 = (i + 1) / 8; cyl(ch, [x0 + (x1 - x0) * t0, 2.0 + 9 * t0 - 3 * Math.sin(t0 * Math.PI), z0 + (z1 - z0) * t0], [x0 + (x1 - x0) * t1, 2.0 + 9 * t1 - 3 * Math.sin(t1 * Math.PI), z0 + (z1 - z0) * t1], 0.03, 4, pick(CC), false); }
  }
  // a bare bulb hung right over the rack's front, so it stands out of the dark; a few more over the ring
  bulb('boss', rx + 0.2, rz + 1.3, 2.5, 1.7, 6);
  for (const [dx, dz] of [[0, 0], [-4, 4], [4, -4], [4, 4], [-4, -3]]) bulb('boss', c.x + dx, c.z + dz, R(4.2, 5.2), 0.9, 9);
  // the four towers: in the margin past the ring, each a separate chunk tipped in phase two about its base
  const T = [
    { x: c.x + 1.8, z: c.z - QB + 0.47, face: 's' }, { x: c.x + QB - 0.47, z: c.z - 1.0, face: 'w' },
    { x: c.x - QB + 0.47, z: c.z + 2.2, face: 'e' }, { x: c.x + QB - 0.47, z: c.z + 3.6, face: 'w' },
  ];
  T.forEach((t, i) => {
    const tch = new Chunk('topple_' + i);
    const L = 2.4, dep = 0.9, H = 7.26 + 0.5;
    let x0, z0, x1, z1;
    if (t.face === 's') { x0 = t.x - L / 2; x1 = t.x + L / 2; z1 = t.z + dep / 2; z0 = z1 - dep; }
    else if (t.face === 'w') { z0 = t.z - L / 2; z1 = t.z + L / 2; x0 = t.x - dep / 2; x1 = x0 + dep; }
    else { z0 = t.z - L / 2; z1 = t.z + L / 2; x1 = t.x + dep / 2; x0 = x1 - dep; }
    into(tch, tch, CH.boss.paint);   // its LEDs ride along with it
    canyonUnit(unitXf(t.face, x0, z0, x1, z1), L, H - 0.5, 'boss', { endA: true, endB: true, still: true });   // (still: no screens, they would not tip with it)
    zoneInto('boss');
    solid(x0, z0, x1, z1, 2.0);   // its base stays where it is; above head height it leans over the ring
    // the pivot is its front bottom edge; it tips toward the middle of the ring
    const px = t.face === 'e' ? x1 : t.face === 'w' ? x0 : t.x, pz = t.face === 's' ? z1 : t.z;
    const axis = t.face === 's' || t.face === 'n' ? 0 : 2;
    const ang = (t.face === 's' ? 1 : t.face === 'w' ? 1 : -1) * R(0.5, 0.6);   // radians, about +x or +z
    TOPPLE.push({ ch: tch, px, py: 0, pz, axis, angle: ang });
    // what falls off it: boxes, totes and monitors land in front of it, between the two rings
    const fx = t.face === 'e' ? 1 : t.face === 'w' ? -1 : 0, fz = t.face === 's' ? 1 : 0;
    const cx = px + fx * 1.15, cz = pz + fz * 1.15;
    for (let k = 0; k < 6; k++) {
      const ox = fx ? R(-0.35, 0.35) : R(-1.1, 1.1), oz = fz ? R(-0.35, 0.35) : R(-1.1, 1.1), s = R(0.25, 0.45);
      if (rnd() < 0.5) obox(P2, cx + ox, s, cz + oz, s * 1.2, s, s * 0.9, R(0, 3), vary(pick([0xb08655, BEIGE, 0x4a4d52]), 0.1));
      else { obox(P2, cx + ox, s * 0.9, cz + oz, s * 1.1, s * 0.9, s * 0.9, R(0, 3), vary(BEIGE, 0.08)); }
    }
    SOLIDS_P2.push(fx ? { x0: cx - 0.6, z0: cz - 1.4, x1: cx + 0.6, z1: cz + 1.4, top: 1.0 } : { x0: cx - 1.4, z0: cz - 0.6, x1: cx + 1.4, z1: cz + 0.6, top: 1.0 });
  });
}

// ---------------------------------------------------------------- aisles: what lines the way between the walls
function aisleDressing() {
  for (const g of SEG) {
    const zone = g.zone, C = CH[zone];
    // bulbs every eight metres or so, unless a landing has one
    for (let t = 4; t < g.L - 2; t += R(7, 9)) {
      const s = g.s0 + t;
      if (LANDING_S.some((ls) => Math.abs(ls - s) < 3)) continue;
      if (CLEAR.some((c) => Math.hypot(at(s).x - c.x, at(s).z - c.z) < Q + 1) || Math.hypot(at(s).x - BOSS.x, at(s).z - BOSS.z) < QB + 1) continue;
      const p = at(s);
      bulb(zone, p.x, p.z, R(3.4, 4.2), 1);
    }
    // an extension cord along one wall, draped from nail to nail
    const side = rnd() < 0.5 ? -1 : 1, ux = g.dz !== 0 ? 1 : 0, uz = g.dz !== 0 ? 0 : 1;
    for (let t = 0; t < g.L - 1; t += 1.5) {
      const a = [g.ax + g.dx * t + ux * side * (HW - 0.02), 2.6, g.az + g.dz * t + uz * side * (HW - 0.02)];
      const b = [g.ax + g.dx * (t + 1.5) + ux * side * (HW - 0.02), 2.6, g.az + g.dz * (t + 1.5) + uz * side * (HW - 0.02)];
      const m = [(a[0] + b[0]) / 2, 2.35, (a[2] + b[2]) / 2];
      cyl(C.lit, a, m, 0.009, 3, 0xe0601a, false); cyl(C.lit, m, b, 0.009, 3, 0xe0601a, false);
    }
    // in the holiday aisles, lights strung along the shelf fronts
    if (zone === 'holiday') for (let t = 0; t < g.L - 3; t += 3) {
      const a = [g.ax + g.dx * t - ux * (HW - 0.05), 2.5, g.az + g.dz * t - uz * (HW - 0.05)];
      const b = [g.ax + g.dx * (t + 3) - ux * (HW - 0.05), 2.5, g.az + g.dz * (t + 3) - uz * (HW - 0.05)];
      lightString('holiday', a, b, 0.5, 7);
    }
  }
}

// ---------------------------------------------------------------- bake: bulbs light pools, walls darken the floor
const WARM = [1.3, 1.02, 0.6];   // a bare bulb's light; the dark between is not warm
function bake(ch, k0 = 0.58) {
  for (let i = 0; i < ch.nv; i++) {
    const x = ch.P[i * 3], y = ch.P[i * 3 + 1], z = ch.P[i * 3 + 2];
    const pc = [0, 0, 0];   // light from the bulbs (warm) and the monitors (green), per channel
    for (const b of BULBS) {
      const dx = x - b.x, dz = z - b.z;
      if (Math.abs(dx) > b.reach || Math.abs(dz) > b.reach) continue;
      const d = Math.hypot(dx, y - b.y, dz);
      if (d >= b.reach) continue;
      const k = b.k * (1 - d / b.reach) ** 2, t = b.tint || WARM;
      for (let c = 0; c < 3; c++) pc[c] += k * t[c];
    }
    let f = 1;
    if (y < 0.05) {   // the floor near the walls
      let wd = 9;
      for (const s of kit.solids) { if (x > s.x0 - 1.4 && x < s.x1 + 1.4 && z > s.z0 - 1.4 && z < s.z1 + 1.4) wd = Math.min(wd, Math.hypot(Math.max(s.x0 - x, 0, x - s.x1), Math.max(s.z0 - z, 0, z - s.z1))); }
      f *= 0.7 + 0.3 * Math.min(1, wd / 1.2);
    }
    if (y > 4.5) f *= Math.max(0.6, 1 - 0.07 * (y - 4.5));   // the tops of the canyons fall away into the dark
    for (let c = 0; c < 3; c++) ch.C[i * 3 + c] *= f * (k0 + 1.15 * Math.min(1.6, pc[c]));
  }
}

// ---------------------------------------------------------------- build
for (const r of WALK) { XMIN = Math.min(XMIN, r.x0 - 3); XMAX = Math.max(XMAX, r.x1 + 3); ZMIN = Math.min(ZMIN, r.z0 - 3); ZMAX = Math.max(ZMAX, r.z1 + 3); }
floors();
stairHead();
for (const run of RUNS) buildRun(run);
LANDING_S.forEach((s, i) => landing(i, s));
for (const [s0, s1, drop] of RUN_S) softRun(s0, s1, drop);
await holidayClearing(CLEAR[0]);
membersClearing(CLEAR[1]);
roboticsClearing(CLEAR[2]);
metalClearing(CLEAR[3]);
bossChamber();
aisleDressing();
roof();
// under everything, a dark slab: seams and gaps read as depth
for (const z of ZONES) {
  const rs = WALK.filter((r) => r.zone === z);
  const x0 = Math.min(...rs.map((r) => r.x0)) - 1.5, x1 = Math.max(...rs.map((r) => r.x1)) + 1.5, z0 = Math.min(...rs.map((r) => r.z0)) - 1.5, z1 = Math.max(...rs.map((r) => r.z1)) + 1.5;
  quad(CH[z].lit, [x0, -0.03, z1], [x1, -0.03, z1], [x1, -0.03, z0], [x0, -0.03, z0], 0x120e0a);
}
for (const z of ZONES) bake(CH[z].lit);
for (const t of TOPPLE) bake(t.ch, 0.5);
bake(P2, 0.55);
for (const ch of ROOF_CH) bake(ch, 0.45);

// ---------------------------------------------------------------- checks: the way stays open
const problems = [];
for (const s of kit.solids) {
  for (const c of [...CLEAR, { ...BOSS }]) {
    const qx = Math.max(s.x0, Math.min(c.x, s.x1)), qz = Math.max(s.z0, Math.min(c.z, s.z1));
    if (Math.hypot(qx - c.x, qz - c.z) < c.r + 0.05) problems.push(`solid inside the ${c.zone || 'boss'} ring: ${JSON.stringify(s)}`);
  }
}
for (const g of SEG) for (let t = 0; t <= g.L; t += 0.25) {
  const x = g.ax + g.dx * t, z = g.az + g.dz * t;
  for (const s of kit.solids) if (x > s.x0 - 0.35 && x < s.x1 + 0.35 && z > s.z0 - 0.35 && z < s.z1 + 0.35) { problems.push(`route blocked at ${x.toFixed(1)},${z.toFixed(1)} by ${JSON.stringify(s)}`); break; }
}
if (problems.length) { console.log(problems.slice(0, 20).join('\n')); console.log(`${problems.length} problems`); }

// ---------------------------------------------------------------- write, shifted into stage space
const allLit = [];
for (const z of ZONES) allLit.push(CH[z].lit, CH[z].glow, CH[z].paint);
const chunks = [...allLit, ...ROOF_CH, ...TOPPLE.map((t) => t.ch), P2, SCR, RED];
for (const ch of chunks) for (let i = 0; i < ch.nv; i++) ch.P[i * 3] += OX;
mkdirSync(join(ROOT, 'assets/models'), { recursive: true });
const res = writeGlb(join(ROOT, 'assets/models/attic_deep.glb'), chunks);

const f3 = (v) => `${v.toFixed(3)}f`;
const bounds = (ch) => { let x0 = 1e9, z0 = 1e9, x1 = -1e9, z1 = -1e9; for (let i = 0; i < ch.nv; i++) { x0 = Math.min(x0, ch.P[i * 3]); x1 = Math.max(x1, ch.P[i * 3]); z0 = Math.min(z0, ch.P[i * 3 + 2]); z1 = Math.max(z1, ch.P[i * 3 + 2]); } return [x0, z0, x1, z1]; };
const H = [];
H.push('/* generated by tools/prep-attic-deep.mjs: the nightmare attic (docs/ATTIC.md, the arc, step 2). Do not edit. */');
H.push('#pragma once');
H.push(LAYOUT_TYPES);
H.push(`#define ATTIC_DEEP_ENTRY_X ${f3(ENTRY.x + OX)}`, `#define ATTIC_DEEP_ENTRY_Z ${f3(ENTRY.z)}`, `#define ATTIC_DEEP_ENTRY_YAW ${f3(ENTRY.yaw)}`);
H.push(`#define ATTIC_DEEP_RACK_X ${f3(BOSS_OUT.rack.x + OX)}`, `#define ATTIC_DEEP_RACK_Z ${f3(BOSS_OUT.rack.z)}`);
H.push(`#define ATTIC_DEEP_TERM_X ${f3(BOSS_OUT.term[0] + OX)}`, `#define ATTIC_DEEP_TERM_Z ${f3(BOSS_OUT.term[2])}`, `#define ATTIC_DEEP_TERM_YAW ${f3(Math.PI)}   /* facing the rack (north) */`);
H.push(`#define ATTIC_DEEP_BOSS_X ${f3(BOSS.x + OX)}`, `#define ATTIC_DEEP_BOSS_Z ${f3(BOSS.z)}`, `#define ATTIC_DEEP_BOSS_R ${f3(BOSS.r)}`, `#define ATTIC_DEEP_BOSS_R2 ${f3(BOSS.r2)}   /* the ring once phase two's debris is down */`);
H.push(`#define ATTIC_DEEP_ROOF ${f3(ROOF0 - 0.3)}   /* the camera stays under this */`);
H.push(`#define ATTIC_DEEP_X0 ${f3(XMIN + OX)}`, `#define ATTIC_DEEP_X1 ${f3(XMAX + OX)}`, `#define ATTIC_DEEP_Z0 ${f3(ZMIN)}`, `#define ATTIC_DEEP_Z1 ${f3(ZMAX)}`);
H.push(`#define ATTIC_DEEP_ROUTE_LEN ${f3(ROUTE_LEN)}`);
const sol = (s) => `  { ${f3(s.x0 + OX)}, ${f3(s.z0)}, ${f3(s.x1 + OX)}, ${f3(s.z1)}, ${f3(s.top)} },`;
H.push('/* solids: boxes from the floor to top, always there */');
H.push('static const attic_box_t ATTIC_DEEP_SOLIDS[] = {', ...kit.solids.map(sol), '};');
H.push('/* phase two only: what falls off the towers into the boss chamber */');
H.push('static const attic_box_t ATTIC_DEEP_SOLIDS_P2[] = {', ...SOLIDS_P2.map(sol), '};');
H.push('/* soft boards: centre, radius, and the landing a fall drops you back to */');
H.push('static const attic_drop_t ATTIC_DEEP_SOFT[] = {', ...SOFT.map((s) => `  { ${f3(s.x + OX)}, ${f3(s.z)}, ${f3(s.r)}, ${s.landing} },`), '};');
H.push('/* landings in route order (0 is the stair head): safe floor under a bulb, the checkpoints */');
H.push('static const attic_mark_t ATTIC_DEEP_LANDINGS[] = {', ...LANDINGS.map((l) => `  { ${f3(l.x + OX)}, ${f3(l.z)}, ${f3(l.yaw)} },`), '};');
H.push('/* fight clearings in route order: nothing solid inside the ring */');
H.push('static const attic_ring_t ATTIC_DEEP_CLEARINGS[] = {', ...CLEAR.map((c) => `  { ${f3(c.x + OX)}, ${f3(c.z)}, ${f3(c.r)}, "${c.zone}" },`), '};');
H.push('/* the route\'s centreline, stair head to the boss ring */');
H.push('static const attic_mark_t ATTIC_DEEP_ROUTE[] = {', ...ROUTE.map(([x, z], i) => { const n = ROUTE[Math.min(i + 1, ROUTE.length - 1)], p = ROUTE[Math.max(0, i - 1)], dx = i + 1 < ROUTE.length ? n[0] - x : x - p[0], dz = i + 1 < ROUTE.length ? n[1] - z : z - p[1]; return `  { ${f3(x + OX)}, ${f3(z)}, ${f3(yawOf(dx, dz))} },`; }), '};');
H.push('/* phase two: each tower chunk turns about the axis (0 x, 2 z) through its pivot by angle radians */');
H.push('static const attic_topple_t ATTIC_DEEP_TOPPLE[] = {', ...TOPPLE.map((t) => `  { "${t.ch.name}", ${f3(t.px + OX)}, ${f3(t.py)}, ${f3(t.pz)}, ${t.axis}, ${f3(t.angle)} },`), '};');
H.push('/* every chunk\'s footprint, for hiding the far ones */');
H.push('static const attic_chunk_t ATTIC_DEEP_CHUNKS[] = {', ...chunks.filter((c) => c.nv).map((c) => { const b = bounds(c); return `  { "${c.name}", ${f3(b[0])}, ${f3(b[1])}, ${f3(b[2])}, ${f3(b[3])} },`; }), '};');
writeFileSync(join(ROOT, 'src/gen/attic_deep_data.h'), H.join('\n') + '\n');
const per = chunks.filter((c) => c.nv).map((c) => `${c.name} ${c.tris}`).join(', ');
console.log(`attic_deep: ${res.tris} triangles, ${(res.bytes / 1e6).toFixed(2)} MB; ${kit.solids.length} solids (+${SOLIDS_P2.length} in phase two), ${SOFT.length} soft boards, ${LANDINGS.length} landings, route ${ROUTE_LEN.toFixed(0)} m`);
console.log(per);
