// Kenney's Car Kit (CC0, source/kenney-car-kit) to assets/models/cars.glb, for the parked cars, the traffic
// on Main and the crew's truck behind the lab (src/city.c).
//
//   node tools/prep-cars.mjs [kit folder with the GLBs and Textures/colormap.png]
//
// Each model is read with its node transforms, its texture's colour taken per triangle (the kit paints by
// palette swatches, so one sample at the triangle's middle is the colour), the four 332 triangle wheels
// swapped for eight sided drums, the body simplified a little, scaled to the real length of its kind and
// turned so its length runs along +x (the front at +x) with the wheels on y 0. Two chunks per kind:
// car_<kind> (its own colours) and car_<kind>_paint (the body paint in white, so a material can recolour
// it per car). Kinds that keep their livery (taxi, police, delivery) have no paint chunk.
import sharp from 'sharp';
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { Chunk, writeGlb } from './glb-lib.mjs';
import { MeshoptSimplifier } from 'meshoptimizer';
await MeshoptSimplifier.ready;

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const SRC = process.argv[2] || join(ROOT, 'source/kenney-car-kit');
// [kind, file, real length in metres, recolour]: the order is the index city.c uses (CAR_KINDS there)
const KINDS = [
  ['sedan', 'sedan', 4.7, 1], ['suv', 'suv', 4.9, 1], ['hatch', 'hatchback-sports', 4.2, 1], ['sports', 'sedan-sports', 4.6, 1],
  ['luxury', 'suv-luxury', 5.0, 1], ['van', 'van', 5.2, 1], ['pickup', 'truck', 5.6, 1], ['taxi', 'taxi', 4.8, 0],
  ['police', 'police', 5.0, 0], ['delivery', 'delivery', 6.6, 0],
];

const tex = await sharp(join(SRC, 'Textures/colormap.png')).ensureAlpha().raw().toBuffer({ resolveWithObject: true });
const TW = tex.info.width, TH = tex.info.height;
const lin = (v) => Math.pow(v / 255, 2.2);
const sample = (u, v) => {
  const x = Math.min(TW - 1, Math.max(0, Math.floor((u - Math.floor(u)) * TW))), y = Math.min(TH - 1, Math.max(0, Math.floor((v - Math.floor(v)) * TH)));
  const p = (y * TW + x) * 4;
  return [tex.data[p], tex.data[p + 1], tex.data[p + 2]];
};

function readGlb(path) {
  const glb = readFileSync(path), jl = glb.readUInt32LE(12);
  const J = JSON.parse(glb.subarray(20, 20 + jl).toString()), BIN = glb.subarray(20 + jl + 8);
  const read = (i) => {
    const a = J.accessors[i], v = J.bufferViews[a.bufferView], c = { SCALAR: 1, VEC2: 2, VEC3: 3, VEC4: 4 }[a.type];
    const T = { 5126: Float32Array, 5125: Uint32Array, 5123: Uint16Array, 5121: Uint8Array }[a.componentType];
    const o = (v.byteOffset || 0) + (a.byteOffset || 0);
    return new T(BIN.buffer.slice(BIN.byteOffset + o, BIN.byteOffset + o + a.count * c * T.BYTES_PER_ELEMENT));
  };
  // node matrices, parents first (TRS: quaternion rotation)
  const mat = (n) => {
    const [tx, ty, tz] = n.translation || [0, 0, 0], [qx, qy, qz, qw] = n.rotation || [0, 0, 0, 1], [sx, sy, sz] = n.scale || [1, 1, 1];
    return [(1 - 2 * (qy * qy + qz * qz)) * sx, (2 * (qx * qy + qz * qw)) * sx, (2 * (qx * qz - qy * qw)) * sx, 0,
      (2 * (qx * qy - qz * qw)) * sy, (1 - 2 * (qx * qx + qz * qz)) * sy, (2 * (qy * qz + qx * qw)) * sy, 0,
      (2 * (qx * qz + qy * qw)) * sz, (2 * (qy * qz - qx * qw)) * sz, (1 - 2 * (qx * qx + qy * qy)) * sz, 0, tx, ty, tz, 1];
  };
  const mul = (a, b) => { const o = new Array(16).fill(0); for (let c = 0; c < 4; c++) for (let r = 0; r < 4; r++) for (let k = 0; k < 4; k++) o[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k]; return o; };
  const out = [];
  const walk = (i, parent) => {
    const n = J.nodes[i], m = mul(parent, mat(n));
    if (n.mesh !== undefined) for (const p of J.meshes[n.mesh].primitives) {
      const pos = read(p.attributes.POSITION), uv = read(p.attributes.TEXCOORD_0);
      const idx = p.indices !== undefined ? read(p.indices) : Uint32Array.from({ length: pos.length / 3 }, (_, k) => k);
      const P = new Float32Array(pos.length);
      for (let k = 0; k < pos.length; k += 3) for (let r = 0; r < 3; r++) P[k + r] = m[r] * pos[k] + m[4 + r] * pos[k + 1] + m[8 + r] * pos[k + 2] + m[12 + r];
      out.push({ name: n.name || '', pos: P, uv, idx: Array.from(idx), at: [m[12], m[13], m[14]] });
    }
    for (const c of n.children || []) walk(c, m);
  };
  for (const r of J.scenes[J.scene || 0].nodes) walk(r, [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]);
  return out;
}

const chunks = [];
let total = 0;
for (const [kind, file, length, recolour] of KINDS) {
  const parts = readGlb(join(SRC, `${file}.glb`));
  // the body and its extras: bounds from everything but the wheels
  const b = [1e9, 1e9, 1e9, -1e9, -1e9, -1e9];
  for (const p of parts) for (let i = 0; i < p.pos.length; i += 3) for (let k = 0; k < 3; k++) { b[k] = Math.min(b[k], p.pos[i + k]); b[k + 3] = Math.max(b[k + 3], p.pos[i + k]); }
  const S = length / (b[5] - b[2]), cz = (b[2] + b[5]) / 2, cx = (b[0] + b[3]) / 2;
  // kit (x, y, z) with +z forward to game (length along +x): game x = kit z, game z = -kit x
  const xf = (x, y, z) => [(z - cz) * S, (y - b[1]) * S, -(x - cx) * S];
  // triangles grouped by colour, with their area
  const groups = new Map();
  const wheels = [];
  for (const p of parts) {
    if (/^wheel/.test(p.name)) { wheels.push(p); continue; }
    for (let t = 0; t < p.idx.length; t += 3) {
      const [a, c, d] = [p.idx[t], p.idx[t + 1], p.idx[t + 2]];
      const u = (p.uv[a * 2] + p.uv[c * 2] + p.uv[d * 2]) / 3, v = (p.uv[a * 2 + 1] + p.uv[c * 2 + 1] + p.uv[d * 2 + 1]) / 3;
      // the colour map's swatches carry a soft gradient: snapped to a coarse palette, a panel is one colour and welds
      const rgb = sample(u, v).map((c) => Math.min(255, Math.round(c / 20) * 20)), key = rgb.join(',');
      if (!groups.has(key)) groups.set(key, { rgb, pos: [], idx: [], area: 0 });
      const g = groups.get(key), base = g.pos.length / 3;
      for (const q of [a, c, d]) g.pos.push(p.pos[q * 3], p.pos[q * 3 + 1], p.pos[q * 3 + 2]);
      g.idx.push(base, base + 1, base + 2);
      const ux = p.pos[c * 3] - p.pos[a * 3], uy = p.pos[c * 3 + 1] - p.pos[a * 3 + 1], uz = p.pos[c * 3 + 2] - p.pos[a * 3 + 2];
      const vx = p.pos[d * 3] - p.pos[a * 3], vy = p.pos[d * 3 + 1] - p.pos[a * 3 + 1], vz = p.pos[d * 3 + 2] - p.pos[a * 3 + 2];
      g.area += Math.hypot(uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx) / 2;
    }
  }
  // the paint: the largest coloured (not grey, not dark) swatch
  let paint = null;
  for (const g of groups.values()) {
    const [r, gg, bb] = g.rgb, mx = Math.max(r, gg, bb), mn = Math.min(r, gg, bb);
    if (mx < 60 || mx - mn < 40) continue;
    if (!paint || g.area > paint.area) paint = g;
  }
  const own = new Chunk(`car_${kind}`, 'opaque'), pc = recolour && paint ? new Chunk(`car_${kind}_paint`, 'opaque') : null;
  // the whole body as one mesh welded by position and colour, simplified together (the colour seams are
  // open borders the simplifier may slide a little; at car scale it does not show), then split back by colour
  const keys = [...groups.keys()], vmap = new Map(), P = [], VC = [], I = [];
  keys.forEach((key, gi) => {
    const g = groups.get(key);
    for (let i = 0; i < g.idx.length; i++) {
      const v = g.idx[i], k = `${gi}:${Math.round(g.pos[v * 3] * 4000)},${Math.round(g.pos[v * 3 + 1] * 4000)},${Math.round(g.pos[v * 3 + 2] * 4000)}`;
      if (!vmap.has(k)) { vmap.set(k, P.length / 3); P.push(g.pos[v * 3], g.pos[v * 3 + 1], g.pos[v * 3 + 2]); VC.push(gi); }
      I.push(vmap.get(k));
    }
  });
  const wp = Float32Array.from(P), wi = Uint32Array.from(I);
  const ext = Math.max(b[3] - b[0], b[4] - b[1], b[5] - b[2]);
  let [out] = MeshoptSimplifier.simplify(wi, wp, 3, Math.floor((wi.length * 0.35) / 3) * 3, 0.08 / ext, []);
  if (out.length < 3) out = wi;
  if (process.env.DEBUG) console.log(kind, "body", wi.length / 3, "->", out.length / 3, "verts", wp.length / 3, "colours", keys.length);
  const byColour = keys.map(() => []);
  for (let t = 0; t < out.length; t += 3) byColour[VC[out[t]]].push(out[t], out[t + 1], out[t + 2]);
  keys.forEach((key, gi) => {
    const g = groups.get(key), idx = byColour[gi];
    if (!idx.length) return;
    if (g === paint && pc) pc.add(wp, idx, [1, 1, 1], xf);
    else own.add(wp, idx, g.rgb.map(lin), xf);
  });
  // wheels: an eight sided drum each, dark tyre and a grey hub, across game z
  for (const w of wheels) {
    const wb = [1e9, 1e9, 1e9, -1e9, -1e9, -1e9];
    for (let i = 0; i < w.pos.length; i += 3) for (let k = 0; k < 3; k++) { wb[k] = Math.min(wb[k], w.pos[i + k]); wb[k + 3] = Math.max(wb[k + 3], w.pos[i + k]); }
    const r = (wb[4] - wb[1]) / 2, mid = [(wb[0] + wb[3]) / 2, (wb[1] + wb[4]) / 2, (wb[2] + wb[5]) / 2], half = (wb[3] - wb[0]) / 2;
    const drum = (rad, h0, h1, rgb) => {
      const P = [], I = [], n = 8;
      for (let i = 0; i < n; i++) { const a = (i / n) * Math.PI * 2; for (const xx of [h0, h1]) P.push(mid[0] + xx, mid[1] + rad * Math.cos(a), mid[2] + rad * Math.sin(a)); }
      for (let i = 0; i < n; i++) { const j = (i + 1) % n; I.push(i * 2, j * 2, j * 2 + 1, i * 2, j * 2 + 1, i * 2 + 1); }
      for (let i = 1; i + 1 < n; i++) { I.push(1, i * 2 + 1, (i + 1) * 2 + 1); I.push(0, (i + 1) * 2, i * 2); }
      own.add(Float32Array.from(P), I, rgb, xf);
    };
    drum(r, -half, half, [0.02, 0.02, 0.022]);
    const out = mid[0] > cx ? 1 : -1;
    // the hub: one octagon on the outer face
    const P = [], I = [], hx = mid[0] + out * (half + 0.004);
    for (let i = 0; i < 8; i++) { const a = (i / 8) * Math.PI * 2; P.push(hx, mid[1] + r * 0.55 * Math.cos(a), mid[2] + r * 0.55 * Math.sin(a)); }
    for (let i = 1; i + 1 < 8; i++) if (out > 0) I.push(0, i, i + 1); else I.push(0, i + 1, i);
    own.add(Float32Array.from(P), I, [0.35, 0.36, 0.38], xf);
  }
  chunks.push(own); if (pc) chunks.push(pc);
  const tris = own.tris + (pc ? pc.tris : 0); total += tris;
  console.log(`  ${kind.padEnd(9)} ${file.padEnd(17)} ${String(tris).padStart(5)} tris, ${S.toFixed(2)} x, ${pc ? 'paint ' + paint.rgb.join(',') : 'own livery'}`);
}
const r = writeGlb(join(ROOT, 'assets/models/cars.glb'), chunks);
console.log(`cars: ${KINDS.length} kinds, ${total} tris, ${(r.bytes / 1024).toFixed(0)} KB`);
