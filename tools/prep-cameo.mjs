// The Chupacabra cameo's two bodies: Moheeb and Luis, the Coffee & Kill rigs from SMALL PARTS
// (games/small-parts/assets/models/{moheeb,luis}.glb, the user's own scans), copied into
// assets/models/cameo_{moheeb,luis}.glb for src/street.c.
//
//   node tools/prep-cameo.mjs [small-parts dir]
//
// The scans' texture is a 2048 px atlas of hundreds of small UV islands packed edge to edge, pale
// skin and shirt beside black hair and trousers. Mipmaps average across neighbouring islands and
// scatter pale specks over the dark parts at a distance. So the texture is rebuilt here at 512 px,
// each texel averaging only the source texels of the island that covers most of its block, the
// islands grown a few texels into the gaps (the same pass as SMALL PARTS' hero.c clean_skin, done
// once here instead of at load). The cart then samples it without mipmaps. 512 px is plenty for an
// 800x450 frame, and it keeps the decode and the upload small. The material loses its emissive copy
// of the texture (street.c draws them cel lit like everyone else).
import { readFileSync, writeFileSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import sharp from 'sharp';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const SP = process.argv[2] || join(ROOT, '..', 'small-parts');
const OUT_W = 512;

function readGlb(path) {
  const b = readFileSync(path);
  const jl = b.readUInt32LE(12);
  const J = JSON.parse(b.subarray(20, 20 + jl).toString());
  const bl = b.readUInt32LE(20 + jl);
  const BIN = b.subarray(20 + jl + 8, 20 + jl + 8 + bl);
  return { J, BIN };
}
function view(J, BIN, i) {
  const v = J.bufferViews[i];
  return BIN.subarray(v.byteOffset || 0, (v.byteOffset || 0) + v.byteLength);
}
function accessor(J, BIN, i) {
  const a = J.accessors[i], v = J.bufferViews[a.bufferView];
  const n = { SCALAR: 1, VEC2: 2, VEC3: 3, VEC4: 4 }[a.type];
  const T = { 5126: Float32Array, 5125: Uint32Array, 5123: Uint16Array, 5121: Uint8Array }[a.componentType];
  const o = (v.byteOffset || 0) + (a.byteOffset || 0);
  const stride = v.byteStride || n * T.BYTES_PER_ELEMENT;
  const out = new T(a.count * n);
  for (let k = 0; k < a.count; k++) {
    const src = new T(BIN.buffer.slice(BIN.byteOffset + o + k * stride, BIN.byteOffset + o + k * stride + n * T.BYTES_PER_ELEMENT));
    out.set(src, k * n);
  }
  return out;
}

// the island-aware downsample: W x H RGBA in, (W / D) x (H / D) RGB out
function clean(px, W, H, uv, idx) {
  const D = W / OUT_W, OW = OUT_W, OH = H / D, nv = uv.length / 2;
  const par = new Uint32Array(nv); for (let i = 0; i < nv; i++) par[i] = i;
  const find = (a) => { while (par[a] !== a) { par[a] = par[par[a]]; a = par[a]; } return a; };
  for (let t = 0; t + 2 < idx.length; t += 3) {
    const a = find(idx[t]), b = find(idx[t + 1]);
    par[b] = a; par[find(idx[t + 2])] = a;
  }
  const id = new Uint16Array(W * H);   // island + 1 per source texel, 0 = gap
  for (let t = 0; t + 2 < idx.length; t += 3) {
    const v = [idx[t], idx[t + 1], idx[t + 2]];
    const isl = (find(v[0]) % 65535) + 1;
    const x = v.map((k) => uv[k * 2] * W), y = v.map((k) => uv[k * 2 + 1] * H);   // glTF: v runs down the image
    const area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
    if (Math.abs(area) < 1e-6) continue;
    const x0 = Math.max(0, Math.floor(Math.min(...x))), x1 = Math.min(W - 1, Math.ceil(Math.max(...x)));
    const y0 = Math.max(0, Math.floor(Math.min(...y))), y1 = Math.min(H - 1, Math.ceil(Math.max(...y)));
    for (let py = y0; py <= y1; py++) for (let qx = x0; qx <= x1; qx++) {
      const cx = qx + 0.5, cy = py + 0.5;
      const w0 = (x[1] - cx) * (y[2] - cy) - (x[2] - cx) * (y[1] - cy);
      const w1 = (x[2] - cx) * (y[0] - cy) - (x[0] - cx) * (y[2] - cy);
      const w2 = (x[0] - cx) * (y[1] - cy) - (x[1] - cx) * (y[0] - cy);
      if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) id[py * W + qx] = isl;
    }
  }
  const out = Buffer.alloc(OW * OH * 3), oid = new Uint16Array(OW * OH);
  for (let by = 0; by < OH; by++) for (let bx = 0; bx < OW; bx++) {
    const at = (k) => (by * D + Math.floor(k / D)) * W + bx * D + (k % D);
    let best = 0, bestn = 0;
    for (let k = 0; k < D * D; k++) {
      const c = id[at(k)]; if (!c) continue;
      let n = 0; for (let j = 0; j < D * D; j++) n += id[at(j)] === c;
      if (n > bestn) { bestn = n; best = c; }
    }
    if (!best) continue;
    const acc = [0, 0, 0];
    for (let j = 0; j < D * D; j++) { const s = at(j); if (id[s] !== best) continue; for (let ch = 0; ch < 3; ch++) acc[ch] += px[s * 4 + ch]; }
    const o = by * OW + bx;
    for (let ch = 0; ch < 3; ch++) out[o * 3 + ch] = Math.round(acc[ch] / bestn);
    oid[o] = best;
  }
  // grow the islands into the gaps so bilinear taps at an edge stay on colour
  for (let pass = 0; pass < 4; pass++) {
    const filled = [];
    for (let y = 0; y < OH; y++) for (let x = 0; x < OW; x++) {
      const o = y * OW + x; if (oid[o]) continue;
      for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
        const nx = x + dx, ny = y + dy;
        if (nx < 0 || ny < 0 || nx >= OW || ny >= OH) continue;
        const q = ny * OW + nx; if (!oid[q]) continue;
        out.copy(out, o * 3, q * 3, q * 3 + 3); filled.push([o, oid[q]]); break;
      }
    }
    for (const [o, v] of filled) oid[o] = v;
  }
  return { out, OW, OH };
}

async function prep(who) {
  const { J, BIN } = readGlb(join(SP, 'assets/models', `${who}.glb`));
  const prim = J.meshes[0].primitives[0];
  const uv = accessor(J, BIN, prim.attributes.TEXCOORD_0);
  const idx = Uint32Array.from(accessor(J, BIN, prim.indices));
  const img = J.images[0];
  const { data, info } = await sharp(view(J, BIN, img.bufferView)).ensureAlpha().raw().toBuffer({ resolveWithObject: true });
  const { out, OW, OH } = clean(data, info.width, info.height, uv, idx);
  const jpg = await sharp(out, { raw: { width: OW, height: OH, channels: 3 } }).jpeg({ quality: 90 }).toBuffer();
  // repack the binary chunk with the new image in its view
  const views = J.bufferViews.map((v, i) => (i === img.bufferView ? jpg : view(J, BIN, i)));
  let off = 0; const parts = [];
  views.forEach((d, i) => {
    const pad = (4 - (off % 4)) % 4; if (pad) { parts.push(Buffer.alloc(pad)); off += pad; }
    J.bufferViews[i].byteOffset = off; J.bufferViews[i].byteLength = d.length; parts.push(d); off += d.length;
  });
  const bin = Buffer.concat(parts, off + ((4 - (off % 4)) % 4));
  J.buffers = [{ byteLength: bin.length }];
  img.mimeType = 'image/jpeg'; img.name = `${who}_512.jpg`;
  for (const m of J.materials) {
    delete m.emissiveTexture; delete m.emissiveFactor; delete m.extras;
    m.pbrMetallicRoughness = { baseColorTexture: m.pbrMetallicRoughness.baseColorTexture, metallicFactor: 0, roughnessFactor: 1 };
  }
  J.asset = { version: '2.0', generator: 'keyholder tools/prep-cameo.mjs' };
  let json = Buffer.from(JSON.stringify(J));
  json = Buffer.concat([json, Buffer.alloc((4 - (json.length % 4)) % 4, 0x20)]);
  const head = Buffer.alloc(12); head.write('glTF', 0); head.writeUInt32LE(2, 4); head.writeUInt32LE(12 + 8 + json.length + 8 + bin.length, 8);
  const ch = (len, type) => { const c = Buffer.alloc(8); c.writeUInt32LE(len, 0); c.write(type, 4); return c; };
  const file = Buffer.concat([head, ch(json.length, 'JSON'), json, ch(bin.length, 'BIN\0'), bin]);
  const dst = join(ROOT, 'assets/models', `cameo_${who}.glb`);
  writeFileSync(dst, file);
  console.log(`${dst}: ${(file.length / 1024).toFixed(0)} KB, texture ${OW}x${OH} ${(jpg.length / 1024).toFixed(0)} KB`);
}
for (const who of ['moheeb', 'luis']) await prep(who);
