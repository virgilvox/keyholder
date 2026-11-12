// The attic as it really is (docs/ATTIC.md, "The blueprint room"): the HSL-108 rear attic floor, 9.4 m by
// 9.1 m, with the headroom raised so the follow camera works. Framing, plywood, insulation, 2x4 shelving
// and the rack are built with the shared kit (tools/attic-kit.mjs); the smaller things on the shelves are CC0
// props from the local library (~/Projects/heatsync/3d-assets), baked into vertex colours like the rest of the game.
//
//   assets/models/attic.glb     chunks: room (lit), glow_room (bulbs, LEDs, vents)
//   src/gen/attic_data.h        solids, soft boards, anchors and examine spots
//
//   node tools/prep-attic.mjs
//
// Stage space: +x east, -z north, y up, metres, origin mid floor. The stair comes up from the south through
// a hole near the west side and arrives facing north; the rack is in the north-west corner.
import { writeFileSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { Chunk, writeGlb } from './glb-lib.mjs';
import {
  K, HOL, HAL, TOOL, kit, into, solid, rnd, R, pick, vary,
  PLY, BOARD, STUD, STUD_OLD, OSB, PINK, PINK_D, PAPER, STEEL, BEIGE, ORANGE_PAINT, RED_PAINT, PINK_PAINT,
  box, obox, beam, cyl, ball, quad, prop, shelving2x4, wireShelf, tote, cardboard, rack,
  sprayX, sprayRing, sprayWord, sprayArrow, shade,
} from './attic-kit.mjs';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');

// ---------------------------------------------------------------- the room's measures
export const RX = 4.7, RZ = 4.55;          // half width (x), half depth (z)
const KNEE = 2.1, RIDGE = 3.6;             // knee wall top at x = +-RX, ridge at x = 0
const roofY = (x) => RIDGE - (RIDGE - KNEE) * Math.min(1, Math.abs(x) / RX);
const HOLE = { x0: -2.0, x1: -1.1, z0: 0.65, z1: RZ };   // the stair hole
const ENTRY = [-1.55, 0, 0.35];            // where you stand at the top of the stair, facing north
const RACK = { x: -4.15, z: -3.95 };       // the rack's middle

const room = new Chunk('room');
const glow = new Chunk('glow_room');
into(room, glow);
const SOLIDS = kit.solids;   // {x0, z0, x1, z1, top}

// ---------------------------------------------------------------- the floor
// 4x8 sheets of plywood the long way north to south, and a strip of the old boards along the east side, with
// gaps between sheets. Cut around the stair hole. Floor quads are 0.6 m so the corner shading has vertices.
function floor() {
  const sheets = [];
  for (let x = -RX; x < 3.0; x += 1.22) for (let z = -RZ; z < RZ; z += 2.44) sheets.push([x, z, Math.min(x + 1.22, 3.0), Math.min(z + 2.44, RZ), pick(PLY)]);
  for (let x = 3.0; x < RX; x += 0.14) sheets.push([x, -RZ, Math.min(x + 0.14, RX), RZ, pick(BOARD)]);
  const T = 0.6;
  for (const [x0, z0, x1, z1, c] of sheets) {
    const cc = vary(c, 0.06), g = 0.006;
    for (let x = x0 + g; x < x1 - g - 1e-4; x += T) for (let z = z0 + g; z < z1 - g - 1e-4; z += T) {
      const xa = x, xb = Math.min(x + T, x1 - g), za = z, zb = Math.min(z + T, z1 - g);
      // skip the stair hole
      const cxm = (xa + xb) / 2, czm = (za + zb) / 2;
      if (xb > HOLE.x0 && xa < HOLE.x1 && zb > HOLE.z0 && za < HOLE.z1) {
        // split around the hole: draw the parts outside it
        const parts = [];
        if (xa < HOLE.x0) parts.push([xa, za, HOLE.x0, zb]);
        if (xb > HOLE.x1) parts.push([HOLE.x1, za, xb, zb]);
        const ixa = Math.max(xa, HOLE.x0), ixb = Math.min(xb, HOLE.x1);
        if (za < HOLE.z0) parts.push([ixa, za, ixb, HOLE.z0]);
        for (const [a, b2, c2, d] of parts) quad(room, [a, 0, d], [c2, 0, d], [c2, 0, b2], [a, 0, b2], cc);
        continue;
      }
      void cxm; void czm;
      quad(room, [xa, 0, zb], [xb, 0, zb], [xb, 0, za], [xa, 0, za], cc);
    }
  }
  // under the floor: a dark slab so seams read as gaps
  quad(room, [-RX, -0.02, RZ], [RX, -0.02, RZ], [RX, -0.02, -RZ], [-RX, -0.02, -RZ], 0x1a140e);
  // nail lines down the joists
  for (let x = -RX + 0.4; x < RX; x += 0.406) for (let z = -RZ + 0.1; z < RZ; z += 0.3) {
    if (x > HOLE.x0 - 0.05 && x < HOLE.x1 + 0.05 && z > HOLE.z0 - 0.05) continue;
    if (rnd() < 0.5) box(room, x - 0.006, 0, z - 0.006, x + 0.006, 0.002, z + 0.006, 0x3b342c);
  }
}

// ---------------------------------------------------------------- the stair hole
function stairHole() {
  const { x0, x1, z0, z1 } = HOLE;
  // the joists and header around the hole, seen from above as a dark well
  box(room, x0 - 0.05, -0.25, z0 - 0.05, x0, 0, z1, STUD_OLD);
  box(room, x1, -0.25, z0 - 0.05, x1 + 0.05, 0, z1, STUD_OLD);
  box(room, x0, -0.25, z0 - 0.05, x1, 0, z0, STUD_OLD);
  // the top treads going down (the stair climbs north, so the top tread is at the north end)
  for (let i = 0; i < 6; i++) {
    const zt = z0 + 0.05 + i * 0.225, y = -0.25 - i * 0.25;
    box(room, x0 + 0.02, y - 0.03, zt, x1 - 0.02, y, zt + 0.25, vary(0x6c5440));
    box(room, x0 + 0.02, y - 0.25, zt + 0.22, x1 - 0.02, y, zt + 0.25, 0x3a2e22);
  }
  // the walls of the stairwell
  box(room, x0 - 0.06, -1.8, z0, x0 - 0.05, -0.25, z1, 0x564434);
  box(room, x1 + 0.05, -1.8, z0, x1 + 0.06, -0.25, z1, 0x564434);
  // a 2x4 guard rail on the east and west sides, open at the north end where you step off
  for (const x of [x0 - 0.04, x1 + 0.04]) {
    for (const z of [z0 + 0.1, (z0 + z1) / 2, z1 - 0.12]) beam(room, [x, 0, z], [x, 1.0, z], 0.089, 0.038, vary(STUD));
    beam(room, [x, 1.0, z0 + 0.05], [x, 1.0, z1 - 0.05], 0.089, 0.038, vary(STUD));
    beam(room, [x, 0.5, z0 + 0.05], [x, 0.5, z1 - 0.05], 0.089, 0.038, vary(STUD));
  }
  solid(x0 - 0.1, z0 + 0.05, x0 + 0.01, z1, 1.05);
  solid(x1 - 0.01, z0 + 0.05, x1 + 0.1, z1, 1.05);
  solid(x0, z0 + 0.25, x1, z1, 1.05);   // you cannot walk into the hole (the stairs are the way down)
  // a strip of yellow and black tape on the edge you step off
  for (let i = 0; i < 6; i++) box(room, x0 + i * 0.15, 0, z0 - 0.06, x0 + i * 0.15 + 0.15, 0.003, z0, i & 1 ? 0x1b1b1b : 0xf2c21b);
}

// ---------------------------------------------------------------- walls and roof
function shell() {
  // knee walls east and west: studs over old plywood
  for (const sx of [-1, 1]) {
    const x = sx * RX;
    quad(room, [x, 0, -RZ], [x, 0, RZ], [x, KNEE, RZ], [x, KNEE, -RZ], sx < 0 ? 0x5f4d38 : 0x5a4934);
    for (let z = -RZ + 0.2; z < RZ; z += 0.61) beam(room, [x - sx * 0.05, 0, z], [x - sx * 0.05, KNEE, z], 0.038, 0.089, vary(STUD_OLD));
    beam(room, [x - sx * 0.05, KNEE - 0.02, -RZ], [x - sx * 0.05, KNEE - 0.02, RZ], 0.089, 0.038, STUD_OLD);
  }
  // gable walls north and south: plywood up to the roof line, studs on them
  for (const sz of [-1, 1]) {
    const z = sz * RZ, N = 16;
    for (let i = 0; i < N; i++) {
      const xa = -RX + (2 * RX * i) / N, xb = -RX + (2 * RX * (i + 1)) / N;
      const a = [xa, 0, z], b = [xb, 0, z], c = [xb, roofY(xb), z], d = [xa, roofY(xa), z];
      const cc = vary(sz < 0 ? 0x655139 : 0x5e4b35, 0.05);
      sz < 0 ? quad(room, a, b, c, d, cc) : quad(room, b, a, d, c, cc);
    }
    for (let x = -RX + 0.3; x < RX; x += 0.61) beam(room, [x, 0, z - sz * 0.05], [x, roofY(x) - 0.05, z - sz * 0.05], 0.038, 0.089, vary(STUD_OLD));
    // a louvred vent high in the gable, daylight through the slats
    const vy = RIDGE - 0.9;
    box(glow, -0.45, vy - 0.35, z - sz * 0.01, 0.45, vy + 0.35, z - sz * 0.012, sz < 0 ? 0xc9b48a : 0xb8a27a);
    for (let k = 0; k < 6; k++) beam(room, [-0.47, vy - 0.3 + k * 0.12, z - sz * 0.04], [0.47, vy - 0.3 + k * 0.12, z - sz * 0.04], 0.1, 0.012, 0x3c3026);
    box(room, -0.5, vy - 0.38, z - sz * 0.06, 0.5, vy - 0.34, z - sz * 0.01, STUD_OLD);
    box(room, -0.5, vy + 0.34, z - sz * 0.06, 0.5, vy + 0.38, z - sz * 0.01, STUD_OLD);
  }
  // the roof: sheathing, rafters every 61 cm, pink batts between, some torn down
  for (const sx of [-1, 1]) {
    const xk = sx * RX, xr = 0;
    for (let z = -RZ; z < RZ; z += 0.61) {
      const za = z, zb = Math.min(z + 0.61, RZ);
      quad(room, [xk, KNEE + 0.14, sx < 0 ? za : zb], [xr, RIDGE + 0.14, sx < 0 ? za : zb], [xr, RIDGE + 0.14, sx < 0 ? zb : za], [xk, KNEE + 0.14, sx < 0 ? zb : za], vary(OSB, 0.07));
      // the batt: four lumpy strips just under the sheathing; one in five hangs loose
      const torn = rnd() < 0.2, gone = !torn && rnd() < 0.12;
      if (!gone) {
        const S = 5;
        for (let i = 0; i < S; i++) {
          const t0 = i / S, t1 = (i + 1) / S;
          const xa = xk + (xr - xk) * t0, xb = xk + (xr - xk) * t1;
          const sag = torn && i < 2 ? (2 - i) * 0.22 : R(0, 0.03);
          const ya = roofY(xa) + 0.02 - sag, yb = roofY(xb) + 0.02 - (torn && i < 1 ? 0.22 : R(0, 0.03));
          const c = i % 2 ? PINK : vary(PINK_D, 0.05);
          const A = [xa, ya, za + 0.03], B = [xb, yb, za + 0.03], C = [xb, yb, zb - 0.03], D = [xa, ya, zb - 0.03];
          sx < 0 ? quad(room, A, D, C, B, c) : quad(room, A, B, C, D, c);
        }
        // kraft paper facing edge
        beam(room, [xk, roofY(xk) - 0.01, za + 0.03], [xr, roofY(xr) - 0.01, za + 0.03], 0.02, 0.015, PAPER);
      }
      // the rafter at za
      beam(room, [xk, KNEE - 0.02, za], [xr, RIDGE - 0.02, za], 0.038, 0.14, vary(STUD_OLD, 0.05));
    }
  }
  beam(room, [0, RIDGE - 0.02, -RZ], [0, RIDGE - 0.02, RZ], 0.038, 0.19, STUD_OLD);   // the ridge board
  // collar ties
  for (let z = -RZ + 0.61; z < RZ; z += 1.83) beam(room, [-1.3, 3.05, z], [1.3, 3.05, z], 0.038, 0.14, vary(STUD_OLD));
}

// ---------------------------------------------------------------- lights: bare bulbs on the ridge and an extension cord
function lights() {
  const BULBS = [[0, -3.0], [0, 0.2], [0, 3.2]];
  for (const [x, z] of BULBS) {
    cyl(room, [x, RIDGE - 0.1, z], [x, RIDGE - 0.22, z], 0.045, 8, 0xe9e2d0);    // porcelain socket
    ball(glow, x, RIDGE - 0.29, z, 0.06, 0xfff0c8);
    beam(room, [x + 0.03, RIDGE - 0.2, z], [x + 0.03, RIDGE - 0.6, z], 0.004, 0.004, 0x8a8a8a);   // pull chain
  }
  // the orange cord along the ridge and down the north gable to the rack
  const cord = [[0.08, RIDGE - 0.12, 3.2], [0.08, RIDGE - 0.12, -RZ + 0.1], [-2.0, 2.5, -RZ + 0.1], [-3.6, 1.9, -RZ + 0.15]];
  for (let i = 0; i + 1 < cord.length; i++) cyl(room, cord[i], cord[i + 1], 0.009, 5, 0xe0601a, false);
  // a clamp work light on the parts shelving, aimed at the rack
  cyl(room, [-3.1, 1.95, -4.0], [-3.35, 1.85, -3.85], 0.07, 8, 0xb8bcc0);
  ball(glow, -3.36, 1.84, -3.84, 0.04, 0xfff3d6);
}

// ---------------------------------------------------------------- spray paint on the floor
const SOFT = [];   // {x, z, r}: boards that give
function paint() {
  // the bad boards: an X each, some circled; a few with words
  const bad = [[-0.2, -1.4], [1.3, -2.6], [2.2, 0.6], [0.5, 2.6], [-3.0, -1.6], [1.6, 3.4], [-0.6, 1.4], [2.8, -1.2]];
  for (const [x, z] of bad) { sprayX(x, z, 0.24, pick([ORANGE_PAINT, ORANGE_PAINT, RED_PAINT])); if (rnd() < 0.5) sprayRing(x, z, 0.42, ORANGE_PAINT); SOFT.push({ x, z, r: 0.42 }); }
  sprayWord('BAD', 1.3, -1.95, 0.28, 0, RED_PAINT);
  sprayWord('NO', 2.2, 1.25, 0.3, 0.2, ORANGE_PAINT);
  sprayWord('SOFT', 0.5, 3.15, 0.22, -0.1, ORANGE_PAINT);
  // the good path: pink arrows from the stair to the rack
  sprayArrow(-1.5, -0.2, -1.6, -1.2, PINK_PAINT);
  sprayArrow(-1.8, -1.9, -3.4, -2.6, PINK_PAINT);
  sprayWord('OK', -1.2, -1.6, 0.18, 0, PINK_PAINT);
}

// ---------------------------------------------------------------- the zones
const EXAM = [];   // {x, z, r, zone, id, label}
const exam = (zone, x, z, label, r = 1.1) => EXAM.push({ x, z, r, zone, id: EXAM.length, label });

async function computerParts() {
  // north wall, west half, beside the rack: chrome wire shelving of computer and server parts
  const S = wireShelf(-3.7, -RZ + 0.05, -1.25, -RZ + 0.55, [0.42, 0.82, 1.22, 1.62]);
  const [f, s1, s2, s3, s4] = S.slots;
  // the floor: two beige towers and a dead UPS
  for (const x of [-3.45, -3.1]) { box(room, x - 0.1, 0, f.z0 + 0.06, x + 0.1, 0.38, f.z1 - 0.06, vary(BEIGE)); box(room, x - 0.09, 0.26, f.z1 - 0.07, x + 0.09, 0.3, f.z1 - 0.059, 0x9a9278); }
  box(room, -2.75, 0, f.z0 + 0.1, -2.35, 0.2, f.z1 - 0.1, 0x2c2e31);
  cardboard(-1.75, 0, (f.z0 + f.z1) / 2, 0.45, 0.4, 0.32, 0.1, true);
  // shelf one: a row of 1U servers stacked flat
  for (let k = 0; k < 4; k++) box(room, -3.6, s1.y + k * 0.045, s1.z0 + 0.05, -2.2, s1.y + k * 0.045 + 0.042, s1.z1 - 0.03, k % 2 ? 0x3a3e45 : 0x2b2f35);
  await prop(K('computerKeyboard'), -1.65, s1.y, (s1.z0 + s1.z1) / 2, { w: 0.45, yaw: 0.1 });
  // shelf two: CRTs and monitors
  await prop(K('televisionVintage'), -3.3, s2.y, (s2.z0 + s2.z1) / 2, { h: 0.34, tint: BEIGE, tintAmt: 0.5 });
  await prop(K('computerScreen'), -2.6, s2.y, (s2.z0 + s2.z1) / 2, { h: 0.36 });
  await prop(K('computerScreen'), -1.9, s2.y, (s2.z0 + s2.z1) / 2, { h: 0.34, yaw: -0.2 });
  // shelf three: totes of cables and drives
  tote(-3.25, s3.y, (s3.z0 + s3.z1) / 2, 0.6, 0.42, 0.3, 0x2f6fd0, 0, { clear: true, inside: 0x22252a });
  tote(-2.5, s3.y, (s3.z0 + s3.z1) / 2, 0.6, 0.42, 0.3, 0x2f6fd0, 0, { clear: true, inside: 0x3a5ea0 });
  tote(-1.75, s3.y, (s3.z0 + s3.z1) / 2, 0.55, 0.42, 0.3, 0xd03030, 0, { clear: true, inside: 0x5a6060 });
  // shelf four: keyboards on end, a laptop
  for (let k = 0; k < 6; k++) obox(room, -3.55 + k * 0.05, s4.y + 0.08, (s4.z0 + s4.z1) / 2, 0.012, 0.08, 0.2, 0, k % 2 ? BEIGE : 0x2a2a2a);
  await prop(K('laptop'), -2.4, s4.y, (s4.z0 + s4.z1) / 2, { w: 0.34, yaw: 0.3 });
  await prop(K('speaker'), -1.7, s4.y, (s4.z0 + s4.z1) / 2, { h: 0.3 });
  exam('parts', -2.5, -3.5, 'THE PARTS SHELF');
}

async function electronics() {
  // north wall, east half: 2x4 shelving of electronics
  const S = shelving2x4(-0.9, -RZ + 0.05, 2.9, -RZ + 0.6, [0.5, 1.0, 1.5], 's');
  const [f, s1, s2, s3] = S.slots, zc = (a) => (a.z0 + a.z1) / 2;
  // small parts drawer cabinets (rows of little drawers)
  for (const [x, slot] of [[-0.55, s1], [0.05, s1], [-0.55, s2]]) {
    box(room, x - 0.24, slot.y, slot.z0 + 0.12, x + 0.24, slot.y + 0.38, slot.z1 - 0.04, 0x5a6b7a);
    for (let r = 0; r < 6; r++) for (let c = 0; c < 4; c++) box(room, x - 0.22 + c * 0.11, slot.y + 0.02 + r * 0.06, slot.z1 - 0.045, x - 0.13 + c * 0.11, slot.y + 0.07 + r * 0.06, slot.z1 - 0.035, pick([0xd8d4c4, 0xc4cfd8, 0xe0d0a0]));
  }
  // an old oscilloscope with a round green screen
  box(room, 0.65, s2.y, zc(s2) - 0.18, 1.15, s2.y + 0.3, zc(s2) + 0.2, 0x6f7a6e);
  box(glow, 0.7, s2.y + 0.08, zc(s2) + 0.201, 0.92, s2.y + 0.24, zc(s2) + 0.203, 0x1c5a34);
  for (let k = 0; k < 6; k++) cyl(room, [0.98 + (k % 2) * 0.08, s2.y + 0.08 + Math.floor(k / 2) * 0.07, zc(s2) + 0.2], [0.98 + (k % 2) * 0.08, s2.y + 0.08 + Math.floor(k / 2) * 0.07, zc(s2) + 0.23], 0.018, 6, 0x222222);
  // soldering stations and a bench supply
  box(room, 1.4, s1.y, zc(s1) - 0.1, 1.68, s1.y + 0.14, zc(s1) + 0.16, 0x2b6cb0);
  box(glow, 1.44, s1.y + 0.06, zc(s1) + 0.161, 1.56, s1.y + 0.1, zc(s1) + 0.163, 0xff5a3a);
  cyl(room, [1.8, s1.y, zc(s1)], [1.8, s1.y + 0.1, zc(s1)], 0.05, 8, 0x333333);
  box(room, 2.0, s1.y, zc(s1) - 0.12, 2.4, s1.y + 0.18, zc(s1) + 0.16, 0x3a3e45);
  box(glow, 2.05, s1.y + 0.1, zc(s1) + 0.161, 2.2, s1.y + 0.14, zc(s1) + 0.163, 0xff3030);
  // wire spools on a dowel
  for (let k = 0; k < 6; k++) cyl(room, [0.35 + k * 0.12, s3.y + 0.09, zc(s3) - 0.08], [0.35 + k * 0.12, s3.y + 0.09, zc(s3) + 0.08], 0.07, 10, [0xd03030, 0x222222, 0x2f6fd0, 0xe0c020, 0x30a050, 0xeeeeee][k]);
  await prop(K('radio'), -0.45, s3.y, zc(s3), { h: 0.22 });
  tote(1.7, s3.y, zc(s3), 0.55, 0.42, 0.28, 0xe0c020, 0, { clear: true, inside: 0x40705a });
  tote(2.4, s3.y, zc(s3), 0.5, 0.42, 0.28, 0x30a050, 0, { clear: true, inside: 0x705a40 });
  // the floor: a box of donated keyboards and a balloon payload box (white foam with a cut out)
  cardboard(-0.4, 0, zc(f), 0.5, 0.42, 0.35, 0.05, true);
  box(room, 0.4, 0, zc(f) - 0.18, 0.8, 0.3, zc(f) + 0.18, 0xeeeeea);
  box(room, 0.5, 0.3, zc(f) - 0.1, 0.7, 0.302, zc(f) + 0.1, 0x9aa2a8);
  tote(1.8, 0, zc(f), 0.6, 0.42, 0.34, 0x2a2a2a, 0, { body: 0x2a2a2a });
  exam('elec_balloon', 0.6, -3.4, 'A FOAM BOX');
  exam('elec_arduino', -0.3, -3.4, 'THE PARTS DRAWERS');
}

async function metalStock() {
  // east wall, north half: a 2x4 cantilever rack of metal stock
  const x0 = RX - 0.65, x1 = RX - 0.05, z0 = -3.9, z1 = -0.4;
  for (let z = z0; z <= z1 + 1e-3; z += (z1 - z0) / 3) beam(room, [x1 - 0.05, 0, z], [x1 - 0.05, 1.9, z], 0.089, 0.038, vary(STUD));
  for (const y of [0.35, 0.8, 1.25, 1.7]) {
    for (let z = z0; z <= z1 + 1e-3; z += (z1 - z0) / 3) beam(room, [x1 - 0.05, y, z], [x0, y + 0.05, z], 0.038, 0.089, vary(STUD));
    // bars, tubes, angle and flat lying across the arms
    for (let k = 0; k < 7; k++) {
      const x = x0 + 0.06 + k * 0.075, yy = y + 0.07 + (k % 2) * 0.02, c = vary(pick([STEEL, 0x6e767d, 0xa8a090, 0x7d5a3a]), 0.1);
      if (k % 3 === 0) cyl(room, [x, yy, z0 - 0.3], [x, yy, z1 + 0.2], 0.018, 6, c);
      else if (k % 3 === 1) beam(room, [x, yy, z0 - 0.2], [x, yy, z1 + 0.3], 0.03, 0.03, c);
      else { beam(room, [x, yy, z0 - 0.25], [x, yy, z1 + 0.1], 0.04, 0.005, c); beam(room, [x - 0.018, yy + 0.018, z0 - 0.25], [x - 0.018, yy + 0.018, z1 + 0.1], 0.005, 0.04, c); }
    }
  }
  // sheet metal leaning on the wall below, and a crate of offcuts
  for (let k = 0; k < 3; k++) obox(room, x1 - 0.12 - k * 0.03, 0.55, -2.2 + k * 0.4, 0.008, 0.55, 0.45, 0.08 + k * 0.03, vary(0xb0b6ba, 0.1));
  box(room, x0 - 0.45, 0, -3.6, x0 - 0.05, 0.32, -3.1, 0x6a5a40);
  for (let k = 0; k < 8; k++) beam(room, [x0 - 0.4 + R(0, 0.3), 0.32, -3.55 + R(0, 0.4)], [x0 - 0.4 + R(0, 0.3), 0.45 + R(0, 0.1), -3.55 + R(0, 0.4)], 0.02, 0.02, vary(STEEL, 0.15));
  for (const [n, z] of [['hammer01', -1.0], ['wrench01', -0.7], ['clamp01', -1.4]]) await prop(TOOL(n), x0 - 0.25, 0, z, { w: 0.3, yaw: 1.2 });
  solid(x0 - 0.5, z0 - 0.3, x1, z1 + 0.3, 1.9);
  exam('metal', x0 - 0.8, -2.0, 'THE METAL RACK');
}

async function artsCrafts() {
  // east wall, south half: 2x4 shelves of arts and crafts
  const S = shelving2x4(RX - 0.6, 0.0, RX - 0.05, 3.6, [0.45, 0.95, 1.45], 'w');
  const [f, s1, s2, s3] = S.slots, xc = (a) => (a.x0 + a.x1) / 2;
  // clear totes of yarn, fabric and paint, lids every colour
  for (const [slot, z, lid, inside] of [[s1, 0.5, 0xd03080, 0xc04070], [s1, 1.2, 0x30a0d0, 0x5080c0], [s2, 0.5, 0xe0c020, 0xa08040], [s2, 1.25, 0x8040c0, 0x60a060], [s3, 0.6, 0x30a050, 0xb05030]]) tote(xc(slot), slot.y, z, 0.42, 0.6, 0.3, lid, Math.PI / 2, { clear: true, inside, label: true });
  // yarn balls loose on a shelf
  for (let k = 0; k < 7; k++) ball(room, xc(s1) + R(-0.15, 0.15), s1.y + 0.07, 1.9 + k * 0.2, 0.07, pick([0xd03050, 0xe0a030, 0x30a0c0, 0x8050c0, 0xf0f0f0]));
  // paint cans and spray cans
  for (let k = 0; k < 6; k++) cyl(room, [xc(s2) + R(-0.12, 0.12), s2.y, 1.9 + k * 0.18], [xc(s2) + R(-0.12, 0.12), s2.y + 0.18, 1.9 + k * 0.18], 0.08, 10, vary(pick([0xb8b8b8, 0xd8d0c0])));
  for (let k = 0; k < 8; k++) cyl(room, [xc(s3) + R(-0.1, 0.1), s3.y, 1.6 + k * 0.1], [xc(s3) + R(-0.1, 0.1), s3.y + 0.2, 1.6 + k * 0.1], 0.03, 8, pick([ORANGE_PAINT, PINK_PAINT, 0x30a0d0, 0x222222, 0xf0f0f0]));
  // rolls of craft paper on the floor shelf, and a stack of foam board
  for (let k = 0; k < 4; k++) cyl(room, [xc(f) - 0.15 + k * 0.1, 0.06, 0.2], [xc(f) - 0.15 + k * 0.1, 0.06, 1.1], 0.05, 8, [0xd8c8a0, 0xd04040, 0x4070d0, 0xe0e0e0][k]);
  for (let k = 0; k < 6; k++) box(room, xc(f) - 0.25, k * 0.012, 1.8, xc(f) + 0.2, k * 0.012 + 0.01, 3.4, k % 2 ? 0xf2f2ee : 0xe8e6de);
  // a mannequin torso from the costume shelf, standing at the end (it watches the room)
  cyl(room, [RX - 0.9, 0, 3.95], [RX - 0.9, 1.0, 3.95], 0.02, 6, 0x222222);
  cyl(room, [RX - 0.9, 0, 3.95], [RX - 0.9, 0.03, 3.95], 0.18, 8, 0x222222);
  obox(room, RX - 0.9, 1.25, 3.95, 0.17, 0.25, 0.1, 0.4, 0xe8e0d4);
  ball(room, RX - 0.9, 1.6, 3.95, 0.11, 0xe8e0d4);
  solid(RX - 1.05, 3.8, RX - 0.75, 4.1, 1.7);
  exam('crafts', RX - 1.2, 1.5, 'THE CRAFT SHELF');
}

async function robotics() {
  // south wall, east of the stair: VEX
  const S = shelving2x4(-0.7, RZ - 0.6, 1.8, RZ - 0.05, [0.5, 1.05], 'n');
  const [f, s1, s2] = S.slots, zc = (a) => (a.z0 + a.z1) / 2;
  // foam field tiles stacked on the floor shelf, grey with interlocking edges
  for (let k = 0; k < 9; k++) box(room, -0.6 + (k % 2) * 0.02, k * 0.016, f.z0 + 0.0, 0.0 + (k % 2) * 0.02, k * 0.016 + 0.015, f.z1 - 0.0, k % 2 ? 0x6a6e72 : 0x5c6064);
  // a field perimeter section leaning
  box(room, 0.15, 0, zc(f) - 0.02, 1.4, 0.3, zc(f) + 0.02, 0xc8ced2);
  for (let k = 0; k < 6; k++) box(room, 0.18 + k * 0.22, 0.02, zc(f) - 0.03, 0.2 + k * 0.22, 0.28, zc(f) + 0.03, 0x9aa0a6);
  // the robot itself: a C-channel chassis, four omni wheels, an arm with a claw, a brain with a screen
  const rx = 0.4, ry = s1.y, rz = zc(s1);
  const AL = 0xb9c0c6;
  for (const sx of [-1, 1]) { beam(room, [rx + sx * 0.18, ry + 0.07, rz - 0.18], [rx + sx * 0.18, ry + 0.07, rz + 0.18], 0.025, 0.05, AL); }
  for (const sz of [-1, 1]) beam(room, [rx - 0.18, ry + 0.09, rz + sz * 0.15], [rx + 0.18, ry + 0.09, rz + sz * 0.15], 0.025, 0.025, AL);
  for (const sx of [-1, 1]) for (const sz of [-1, 1]) cyl(room, [rx + sx * 0.2, ry + 0.05, rz + sz * 0.14], [rx + sx * 0.23, ry + 0.05, rz + sz * 0.14], 0.05, 10, 0x2a2a2a);
  box(room, rx - 0.07, ry + 0.11, rz - 0.06, rx + 0.07, ry + 0.16, rz + 0.06, 0x202020);
  box(glow, rx - 0.04, ry + 0.161, rz - 0.03, rx + 0.04, ry + 0.163, rz + 0.03, 0x40a0ff);
  beam(room, [rx, ry + 0.12, rz - 0.12], [rx, ry + 0.38, rz - 0.3], 0.025, 0.025, AL);
  beam(room, [rx - 0.05, ry + 0.38, rz - 0.3], [rx - 0.05, ry + 0.3, rz - 0.42], 0.015, 0.015, AL);
  beam(room, [rx + 0.05, ry + 0.38, rz - 0.3], [rx + 0.05, ry + 0.3, rz - 0.42], 0.015, 0.015, AL);
  // a second robot half taken apart, and the kit boxes
  for (let k = 0; k < 5; k++) beam(room, [1.0 + R(0, 0.3), s1.y + 0.02, zc(s1) + R(-0.15, 0.15)], [1.2 + R(0, 0.3), s1.y + 0.02, zc(s1) + R(-0.15, 0.15)], 0.025, 0.012, AL);
  for (let k = 0; k < 4; k++) cyl(room, [1.55, s1.y + 0.05, zc(s1) - 0.15 + k * 0.1], [1.58, s1.y + 0.05, zc(s1) - 0.15 + k * 0.1], 0.05, 10, 0x2a2a2a);
  tote(-0.35, s2.y, zc(s2), 0.6, 0.42, 0.3, 0xd03030, Math.PI, { clear: true, inside: AL });
  tote(0.35, s2.y, zc(s2), 0.6, 0.42, 0.3, 0x2f6fd0, Math.PI, { clear: true, inside: AL });
  // game pieces: red and blue cubes and cones
  for (let k = 0; k < 6; k++) { const c = k % 2 ? 0xd03030 : 0x2f6fd0; obox(room, 1.0 + (k % 3) * 0.2, s2.y + 0.07, zc(s2) - 0.08 + Math.floor(k / 3) * 0.16, 0.07, 0.07, 0.07, R(0, 1), c); }
  exam('robots', 0.6, RZ - 1.4, 'THE ROBOT SHELF');
}

async function holiday() {
  // south-east corner: holiday supplies, heaped on and around a 2x4 shelf
  const S = shelving2x4(2.0, RZ - 0.6, RX - 1.15, RZ - 0.05, [0.6, 1.2], 'n');
  const [f, s1, s2] = S.slots, zc = (a) => (a.z0 + a.z1) / 2;
  // the bins: red and green lids, the colours every lab buys
  tote(2.3, 0, zc(f), 0.6, 0.42, 0.4, 0xc02020, Math.PI, { clear: false, body: 0x2a2d30 });
  tote(2.95, 0, zc(f), 0.6, 0.42, 0.4, 0x208040, Math.PI, { clear: false, body: 0x2a2d30 });
  tote(2.3, 0.4, zc(f), 0.6, 0.42, 0.4, 0x208040, Math.PI, { clear: false, body: 0x2a2d30, redtag: false });
  tote(2.4, s1.y, zc(s1), 0.6, 0.42, 0.34, 0xc02020, Math.PI, { clear: true, inside: 0xd0b040 });
  await prop(HOL('lights-colored'), 3.05, s1.y, zc(s1), { w: 0.55, yaw: 0.2 });
  await prop(HOL('present-a-cube'), 2.25, s2.y, zc(s2), { h: 0.26 });
  await prop(HOL('present-b-rectangle'), 2.6, s2.y, zc(s2), { h: 0.2, yaw: 0.4 });
  await prop(HOL('wreath-decorated'), 3.1, s2.y + 0.02, zc(s2), { w: 0.42 });
  await prop(HOL('nutcracker'), 2.85, s2.y, zc(s2) - 0.1, { h: 0.42 });
  // the tree, half in its box, and a pumpkin crowd from October
  await prop(HOL('tree-decorated'), RX - 0.6, 0, RZ - 0.7, { h: 1.55 });
  cardboard(RX - 1.4, 0, RZ - 1.35, 0.5, 0.35, 0.3, 0.6, true);
  await prop(HAL('pumpkin_orange_jackolantern'), 2.6, 0, RZ - 1.0, { h: 0.3, yaw: 2.6 });
  await prop(HAL('pumpkin_orange'), 2.95, 0, RZ - 1.15, { h: 0.26 });
  await prop(HAL('pumpkin_yellow_small'), 2.35, 0, RZ - 1.2, { h: 0.18 });
  await prop(HAL('skull'), 3.35, s1.y, zc(s1), { h: 0.16, yaw: 3.3 });
  await prop(HOL('reindeer'), RX - 1.25, 0, RZ - 2.05, { h: 0.85, yaw: -2.2 });
  await prop(HOL('sock-red-cane'), 3.4, s2.y, zc(s2), { h: 0.3 });
  solid(RX - 1.1, RZ - 1.2, RX - 0.1, RZ - 0.1, 1.5);
  solid(2.1, RZ - 1.4, 3.3, RZ - 0.6, 0.5);
  solid(RX - 1.6, RZ - 2.4, RX - 0.9, RZ - 1.7, 0.9);
  exam('holiday', 2.9, RZ - 1.6, 'THE HOLIDAY BINS');
}

async function memberStorage() {
  // the west wall: a long run of 2x4 shelving full of old member boxes, three high
  const S = shelving2x4(-RX + 0.05, -1.75, -RX + 0.65, RZ - 0.1, [0.5, 1.0, 1.5], 'e', { wood: STUD_OLD, deck: 0x9c8156 });
  const lids = [0x2f6fd0, 0xc02020, 0x208040, 0xe0c020, 0x2a2a2a, 0x8040c0, 0x30a0d0];
  for (const slot of S.slots) {
    for (let z = slot.z0 + 0.35; z < slot.z1 - 0.3; z += 0.62) {
      if (rnd() < 0.12) continue;   // a gap where somebody took theirs home
      const clear = rnd() < 0.5;
      tote((slot.x0 + slot.x1) / 2, slot.y, z + R(-0.03, 0.03), 0.42, 0.58, R(0.3, 0.38), pick(lids), Math.PI / 2 + R(-0.05, 0.05), { clear, inside: vary(pick([0x7a6a58, 0x5a6a78, 0x9a7050, 0x50605a]), 0.2), redtag: rnd() < 0.25 });
    }
  }
  // overflow on the floor in front: a couple of cardboard boxes with names on them
  cardboard(-RX + 1.0, 0, 2.0, 0.45, 0.4, 0.38, 0.3);
  cardboard(-RX + 0.95, 0.38, 2.05, 0.4, 0.35, 0.3, -0.2, true);
  solid(-RX + 0.7, 1.75, -RX + 1.25, 2.3, 0.7);
  exam('members', -RX + 1.1, 0.2, 'OLD MEMBER BOXES');
}

async function floorClutter() {
  // the middle of the room: what got set down and never moved. Keep the path from the stair to the rack.
  await prop(K('chairDesk'), 1.0, 0, 0.2, { h: 0.95, yaw: 2.4, tint: 0x2a2c30, tintAmt: 0.85 });
  cardboard(1.9, 0, -0.4, 0.55, 0.45, 0.42, 0.25);
  cardboard(1.95, 0.42, -0.35, 0.45, 0.4, 0.32, -0.15);
  cardboard(0.6, 0, 1.6, 0.5, 0.5, 0.3, 0.7, true);
  // a box fan and a dehumidifier by the stair
  cyl(room, [-0.6, 0.3, 0.4], [-0.6, 0.3, 0.52], 0.28, 12, 0xe6e2d8);
  box(room, -0.68, 0, 0.4, -0.52, 0.04, 0.52, 0xe6e2d8);
  // an extension cord loop and a coffee can of screws
  for (let k = 0; k < 10; k++) { const a0 = k * 0.7, a1 = a0 + 0.7; cyl(room, [2.5 + Math.cos(a0) * 0.3, 0.01, 1.8 + Math.sin(a0) * 0.2], [2.5 + Math.cos(a1) * 0.3, 0.01, 1.8 + Math.sin(a1) * 0.2], 0.01, 4, 0xe0601a, false); }
  cyl(room, [-0.2, 0, -0.6], [-0.2, 0.16, -0.6], 0.07, 10, 0x8a3a20);
  solid(0.7, -0.1, 1.3, 0.5, 1.0);
  solid(1.6, -0.7, 2.2, -0.1, 0.75);
  solid(0.35, 1.35, 0.85, 1.85, 0.3);
  solid(-0.9, 0.25, -0.3, 0.6, 0.6);
}

floor();
stairHole();
shell();
lights();
const R0 = rack(RACK.x, RACK.z);
await computerParts();
await electronics();
await metalStock();
await artsCrafts();
await robotics();
await holiday();
await memberStorage();
await floorClutter();
paint();
shade(room, (x, z) => Math.min(RX - Math.abs(x), RZ - Math.abs(z)), KNEE);

mkdirSync(join(ROOT, 'assets/models'), { recursive: true });
writeGlb(join(ROOT, 'assets/models/attic.glb'), [room, glow]);

const f3 = (v) => `${v.toFixed(3)}f`;
const H = [];
H.push('/* generated by tools/prep-attic.mjs: the blueprint attic (docs/ATTIC.md). Do not edit. */');
H.push('#pragma once');
H.push(`#define ATTIC_RX ${f3(RX)}`, `#define ATTIC_RZ ${f3(RZ)}`, `#define ATTIC_KNEE ${f3(KNEE)}`, `#define ATTIC_RIDGE ${f3(RIDGE)}`);
H.push(`#define ATTIC_ENTRY_X ${f3(ENTRY[0])}`, `#define ATTIC_ENTRY_Z ${f3(ENTRY[2])}`);
H.push(`#define ATTIC_RACK_X ${f3(RACK.x)}`, `#define ATTIC_RACK_Z ${f3(RACK.z)}`);
H.push(`#define ATTIC_TERM_X ${f3(R0.terminal[0])}`, `#define ATTIC_TERM_Z ${f3(R0.terminal[2])}`);
H.push('typedef struct { float x0, z0, x1, z1, top; } attic_solid_t;');
H.push(`static const attic_solid_t ATTIC_SOLIDS[] = {`);
for (const s of SOLIDS) H.push(`  { ${f3(s.x0)}, ${f3(s.z0)}, ${f3(s.x1)}, ${f3(s.z1)}, ${f3(s.top)} },`);
H.push('};');
H.push('typedef struct { float x, z, r; } attic_soft_t;');
H.push('static const attic_soft_t ATTIC_SOFT[] = {');
for (const s of SOFT) H.push(`  { ${f3(s.x)}, ${f3(s.z)}, ${f3(s.r)} },`);
H.push('};');
H.push('typedef struct { float x, z, r; const char *zone, *label; } attic_exam_t;');
H.push('static const attic_exam_t ATTIC_EXAM[] = {');
for (const e of EXAM) H.push(`  { ${f3(e.x)}, ${f3(e.z)}, ${f3(e.r)}, "${e.zone}", "${e.label}" },`);
H.push('};');
writeFileSync(join(ROOT, 'src/gen/attic_data.h'), H.join('\n') + '\n');
console.log(`attic: ${room.tris + glow.tris} triangles (${room.tris} lit, ${glow.tris} glow), ${SOLIDS.length} solids, ${SOFT.length} soft boards, ${EXAM.length} examine spots`);
