// Shared by prep-lab.mjs, prep-arena.mjs and prep-city.mjs: reads the HSL-108 GLB part by
// part, welds and simplifies parts, and writes compact GLBs of vertex coloured chunks.
//
// Written vertices are 16 bytes: POSITION int16 normalized under a node transform
// (KHR_mesh_quantization), NORMAL int8 normalized, COLOR_0 uint8 normalized (linear).
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname } from 'node:path';
import { MeshoptSimplifier } from 'meshoptimizer';

export const MODEL = '/Users/obsidian/Projects/heatsync/hsl-108-model/model/HSL-108-full.glb';

export function readModel(path = MODEL) {
  const glb = readFileSync(path);
  const jlen = glb.readUInt32LE(12);
  const J = JSON.parse(glb.subarray(20, 20 + jlen).toString());
  const BIN = glb.subarray(20 + jlen + 8);
  const read = (i) => {
    const a = J.accessors[i], v = J.bufferViews[a.bufferView];
    const c = { SCALAR: 1, VEC2: 2, VEC3: 3, VEC4: 4 }[a.type];
    const T = { 5126: Float32Array, 5125: Uint32Array, 5123: Uint16Array, 5121: Uint8Array }[a.componentType];
    const o = (v.byteOffset || 0) + (a.byteOffset || 0);
    return new T(BIN.buffer.slice(BIN.byteOffset + o, BIN.byteOffset + o + a.count * c * T.BYTES_PER_ELEMENT));
  };
  const parts = [];
  for (const n of J.nodes) {
    if (n.mesh === undefined) continue;
    const ex = n.extras || {};
    for (const p of J.meshes[n.mesh].primitives) {
      const pos = read(p.attributes.POSITION);
      const idx = p.indices !== undefined ? Uint32Array.from(read(p.indices)) : Uint32Array.from({ length: pos.length / 3 }, (_, i) => i);
      const m = J.materials[p.material] || {};
      const c = (m.pbrMetallicRoughness || {}).baseColorFactor || [1, 1, 1, 1];
      const e = m.emissiveFactor || [0, 0, 0];
      const b = [1e9, 1e9, 1e9, -1e9, -1e9, -1e9];
      for (let i = 0; i < pos.length; i += 3) for (let k = 0; k < 3; k++) { b[k] = Math.min(b[k], pos[i + k]); b[k + 3] = Math.max(b[k + 3], pos[i + k]); }
      parts.push({ name: n.name || '', group: ex.group || '', area: ex.area || '', pos, idx, box: b, color: c.slice(0, 3), alpha: m.alphaMode === 'BLEND' ? c[3] : 1,
        emissive: e.some((v) => v > 0) ? e : null, matName: m.name || '' });
    }
  }
  return parts;
}

await MeshoptSimplifier.ready;
// weld by position, then simplify toward ratio with a small absolute error (metres)
export function simplify(part, ratio, err) {
  const map = new Map(), P = [], remap = new Uint32Array(part.pos.length / 3);
  for (let i = 0; i < remap.length; i++) {
    const k = `${Math.round(part.pos[i * 3] * 2000)},${Math.round(part.pos[i * 3 + 1] * 2000)},${Math.round(part.pos[i * 3 + 2] * 2000)}`;
    if (!map.has(k)) { map.set(k, P.length / 3); P.push(part.pos[i * 3], part.pos[i * 3 + 1], part.pos[i * 3 + 2]); }
    remap[i] = map.get(k);
  }
  const pos = Float32Array.from(P);
  const idx = Uint32Array.from(part.idx, (v) => remap[v]);
  const ext = Math.max(part.box[3] - part.box[0], part.box[4] - part.box[1], part.box[5] - part.box[2], 1e-3);
  const [out] = MeshoptSimplifier.simplify(idx, pos, 3, Math.max(12, Math.floor((idx.length * ratio) / 3) * 3), err / ext, ['LockBorder']);
  return { pos, idx: out };
}

// A chunk gathers triangles: add(pos[], idx[], rgb, matrixfn). Normals are computed per
// position by grouping faces within a crease angle.
export class Chunk {
  constructor(name, kind = 'opaque') { this.name = name; this.kind = kind; this.P = []; this.N = []; this.C = []; this.I = []; this.nv = 0; }
  add(pos, idx, rgb, xf) {
    const nvs = pos.length / 3, W = new Float32Array(pos.length);
    for (let i = 0; i < nvs; i++) { const q = xf ? xf(pos[i * 3], pos[i * 3 + 1], pos[i * 3 + 2]) : [pos[i * 3], pos[i * 3 + 1], pos[i * 3 + 2]]; W[i * 3] = q[0]; W[i * 3 + 1] = q[1]; W[i * 3 + 2] = q[2]; }
    const keyOf = (v) => `${Math.round(W[v * 3] * 2000)},${Math.round(W[v * 3 + 1] * 2000)},${Math.round(W[v * 3 + 2] * 2000)}`;
    const groups = new Map();
    const cosC = Math.cos((38 * Math.PI) / 180);
    for (let t = 0; t < idx.length; t += 3) {
      const a = idx[t], b = idx[t + 1], c = idx[t + 2];
      const ux = W[b * 3] - W[a * 3], uy = W[b * 3 + 1] - W[a * 3 + 1], uz = W[b * 3 + 2] - W[a * 3 + 2];
      const vx = W[c * 3] - W[a * 3], vy = W[c * 3 + 1] - W[a * 3 + 1], vz = W[c * 3 + 2] - W[a * 3 + 2];
      let nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
      const l = Math.hypot(nx, ny, nz); if (l < 1e-9) continue; nx /= l; ny /= l; nz /= l;
      const tri = [];
      for (const v of [a, b, c]) {
        const k = keyOf(v);
        let list = groups.get(k); if (!list) groups.set(k, (list = []));
        let g = list.find((q) => q.fx * nx + q.fy * ny + q.fz * nz > cosC);
        if (!g) { g = { fx: nx, fy: ny, fz: nz, sx: 0, sy: 0, sz: 0, v, id: -1 }; list.push(g); }
        g.sx += nx * l; g.sy += ny * l; g.sz += nz * l;
        tri.push(g);
      }
      (this._tris ||= []).push(tri);
    }
    const lin = rgb;
    for (const tri of this._tris || []) {
      for (const g of tri) if (g.id < 0) {
        g.id = this.nv++;
        const sl = Math.hypot(g.sx, g.sy, g.sz) || 1;
        this.P.push(W[g.v * 3], W[g.v * 3 + 1], W[g.v * 3 + 2]);
        this.N.push(g.sx / sl, g.sy / sl, g.sz / sl);
        this.C.push(lin[0], lin[1], lin[2]);
      }
      this.I.push(tri[0].id, tri[1].id, tri[2].id);
    }
    this._tris = null;
  }
  box(x0, y0, z0, x1, y1, z1, rgb) {
    const v = [x0, y0, z0, x1, y0, z0, x1, y1, z0, x0, y1, z0, x0, y0, z1, x1, y0, z1, x1, y1, z1, x0, y1, z1];
    this.add(Float32Array.from(v), [0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4, 3, 7, 6, 3, 6, 2, 0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5], rgb);
  }
  quad(a, b, c, d, rgb) { this.add(Float32Array.from([...a, ...b, ...c, ...d]), [0, 1, 2, 0, 2, 3], rgb); }
  get tris() { return this.I.length / 3; }
}

export const srgb = (h) => [(h >> 16) & 255, (h >> 8) & 255, h & 255].map((v) => Math.pow(v / 255, 2.2));

// 64 thousand vertex meshes per node at most: a chunk over that is split
export function writeGlb(path, chunks) {
  const views = [], accessors = [], meshes = [], nodes = [], bins = [];
  let off = 0;
  const push = (buf, target, stride) => {
    const pad = (4 - (buf.length % 4)) % 4;
    const v = { buffer: 0, byteOffset: off, byteLength: buf.length, target };
    if (stride) v.byteStride = stride;
    views.push(v); bins.push(buf, Buffer.alloc(pad)); off += buf.length + pad;
    return views.length - 1;
  };
  const materials = [{ name: 'vcol', pbrMetallicRoughness: { baseColorFactor: [1, 1, 1, 1], metallicFactor: 0, roughnessFactor: 1 } }];
  const emit = (name, P, N, C, I) => {
    const nv = P.length / 3;
    const mn = [1e9, 1e9, 1e9], mx = [-1e9, -1e9, -1e9];
    for (let i = 0; i < P.length; i += 3) for (let k = 0; k < 3; k++) { mn[k] = Math.min(mn[k], P[i + k]); mx[k] = Math.max(mx[k], P[i + k]); }
    const c = [0, 1, 2].map((k) => (mn[k] + mx[k]) / 2);
    const h = Math.max(...[0, 1, 2].map((k) => (mx[k] - mn[k]) / 2), 1e-3);
    const qp = Buffer.alloc(nv * 8), qn = Buffer.alloc(nv * 4), qc = Buffer.alloc(nv * 4);
    const qmn = [32767, 32767, 32767], qmx = [-32767, -32767, -32767];
    for (let i = 0; i < nv; i++) {
      for (let k = 0; k < 3; k++) {
        const q = Math.round(((P[i * 3 + k] - c[k]) / h) * 32767);
        qp.writeInt16LE(q, i * 8 + k * 2); qmn[k] = Math.min(qmn[k], q); qmx[k] = Math.max(qmx[k], q);
        qn.writeInt8(Math.round(N[i * 3 + k] * 127), i * 4 + k);
        qc.writeUInt8(Math.max(0, Math.min(255, Math.round(C[i * 3 + k] * 255))), i * 4 + k);
      }
    }
    const big = nv > 65535;
    const ib = big ? Buffer.from(Uint32Array.from(I).buffer) : Buffer.from(Uint16Array.from(I).buffer);
    const attributes = { POSITION: accessors.length };
    accessors.push({ bufferView: push(qp, 34962, 8), componentType: 5122, normalized: true, count: nv, type: 'VEC3', min: qmn, max: qmx });
    attributes.NORMAL = accessors.length;
    accessors.push({ bufferView: push(qn, 34962, 4), componentType: 5120, normalized: true, count: nv, type: 'VEC3' });
    attributes.COLOR_0 = accessors.length;
    accessors.push({ bufferView: push(qc, 34962, 4), componentType: 5121, normalized: true, count: nv, type: 'VEC3' });
    const ia = accessors.length;
    accessors.push({ bufferView: push(ib, 34963), componentType: big ? 5125 : 5123, count: I.length, type: 'SCALAR' });
    meshes.push({ name, primitives: [{ attributes, indices: ia, material: 0 }] });
    nodes.push({ name, mesh: meshes.length - 1, translation: c, scale: [h, h, h] });
  };
  let tris = 0;
  for (const ch of chunks) {
    if (!ch.I.length) continue;
    tris += ch.I.length / 3;
    emit(ch.name, ch.P, ch.N, ch.C, ch.I);
  }
  const out = { asset: { version: '2.0', generator: 'keyholder' }, extensionsUsed: ['KHR_mesh_quantization'], extensionsRequired: ['KHR_mesh_quantization'], scene: 0, scenes: [{ nodes: nodes.map((_, i) => i) }], nodes, meshes, materials, accessors, bufferViews: views, buffers: [{ byteLength: off }] };
  let js = Buffer.from(JSON.stringify(out));
  js = Buffer.concat([js, Buffer.alloc((4 - (js.length % 4)) % 4, 0x20)]);
  const bin = Buffer.concat(bins);
  const head = Buffer.alloc(12); head.write('glTF', 0); head.writeUInt32LE(2, 4); head.writeUInt32LE(12 + 8 + js.length + 8 + bin.length, 8);
  const c0 = Buffer.alloc(8); c0.writeUInt32LE(js.length, 0); c0.writeUInt32LE(0x4e4f534a, 4);
  const c1 = Buffer.alloc(8); c1.writeUInt32LE(bin.length, 0); c1.writeUInt32LE(0x004e4942, 4);
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, Buffer.concat([head, c0, js, c1, bin]));
  return { tris, bytes: off };
}
