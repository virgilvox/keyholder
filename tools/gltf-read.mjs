// Reads a CC0 prop (.glb or .gltf with its .bin and textures) into flat-coloured triangles, so it can be
// baked into a vertex coloured chunk with the game's look. Each triangle takes its material's base colour,
// times the texture sampled at the triangle's middle (Kenney and KayKit colour atlases are flat swatches,
// so one sample per face is exact). Node transforms are applied; the result is in the model's own units.
//
//   const m = await readProp(path);   // { pos: Float32Array, idx: Uint32Array, col: Float32Array (linear rgb per tri), box }
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import sharp from 'sharp';

const lin = (v) => Math.pow(v / 255, 2.2);
const imgCache = new Map();
async function loadImage(src, key) {
  if (imgCache.has(key)) return imgCache.get(key);
  const { data, info } = await sharp(src).ensureAlpha().raw().toBuffer({ resolveWithObject: true });
  const img = { data, w: info.width, h: info.height };
  imgCache.set(key, img);
  return img;
}
function sample(img, u, v) {
  u -= Math.floor(u); v -= Math.floor(v);
  const x = Math.min(img.w - 1, Math.floor(u * img.w)), y = Math.min(img.h - 1, Math.floor(v * img.h));
  const o = (y * img.w + x) * 4;
  return [lin(img.data[o]), lin(img.data[o + 1]), lin(img.data[o + 2])];
}

function mat4FromNode(n) {
  if (n.matrix) return n.matrix.slice();
  const [tx, ty, tz] = n.translation || [0, 0, 0];
  const [qx, qy, qz, qw] = n.rotation || [0, 0, 0, 1];
  const [sx, sy, sz] = n.scale || [1, 1, 1];
  const xx = qx * qx, yy = qy * qy, zz = qz * qz, xy = qx * qy, xz = qx * qz, yz = qy * qz, wx = qw * qx, wy = qw * qy, wz = qw * qz;
  return [
    (1 - 2 * (yy + zz)) * sx, 2 * (xy + wz) * sx, 2 * (xz - wy) * sx, 0,
    2 * (xy - wz) * sy, (1 - 2 * (xx + zz)) * sy, 2 * (yz + wx) * sy, 0,
    2 * (xz + wy) * sz, 2 * (yz - wx) * sz, (1 - 2 * (xx + yy)) * sz, 0,
    tx, ty, tz, 1];
}
function mul(a, b) {
  const o = new Array(16).fill(0);
  for (let c = 0; c < 4; c++) for (let r = 0; r < 4; r++) for (let k = 0; k < 4; k++) o[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
  return o;
}

export async function readProp(path) {
  const raw = readFileSync(path);
  let J, BIN;
  if (raw.readUInt32LE(0) === 0x46546c67) {   // 'glTF'
    const jlen = raw.readUInt32LE(12);
    J = JSON.parse(raw.subarray(20, 20 + jlen).toString());
    BIN = raw.subarray(20 + jlen + 8);
  } else {
    J = JSON.parse(raw.toString());
    const b = J.buffers[0];
    BIN = b.uri.startsWith('data:') ? Buffer.from(b.uri.split(',')[1], 'base64') : readFileSync(join(dirname(path), decodeURIComponent(b.uri)));
  }
  const read = (i) => {
    const a = J.accessors[i], v = J.bufferViews[a.bufferView];
    const c = { SCALAR: 1, VEC2: 2, VEC3: 3, VEC4: 4 }[a.type];
    const T = { 5126: Float32Array, 5125: Uint32Array, 5123: Uint16Array, 5121: Uint8Array }[a.componentType];
    const stride = v.byteStride || c * T.BYTES_PER_ELEMENT;
    const base = (v.byteOffset || 0) + (a.byteOffset || 0);
    const out = new Float64Array(a.count * c);
    for (let i = 0; i < a.count; i++) for (let k = 0; k < c; k++) {
      const o = base + i * stride + k * T.BYTES_PER_ELEMENT;
      out[i * c + k] = T === Float32Array ? BIN.readFloatLE(o) : T === Uint32Array ? BIN.readUInt32LE(o) : T === Uint16Array ? BIN.readUInt16LE(o) : BIN.readUInt8(o);
    }
    return out;
  };
  // a texture's image: a file beside the model, a data URI, or a buffer view inside the GLB
  const texImage = (ti) => {
    const t = J.textures[ti], im = J.images[t.source];
    if (im.uri && im.uri.startsWith('data:')) return loadImage(Buffer.from(im.uri.split(',')[1], 'base64'), `${path}#${t.source}`);
    if (im.uri) { const f = join(dirname(path), decodeURIComponent(im.uri)); return loadImage(f, f); }
    const v = J.bufferViews[im.bufferView];
    return loadImage(Buffer.from(BIN.subarray(v.byteOffset || 0, (v.byteOffset || 0) + v.byteLength)), `${path}#${t.source}`);
  };

  const P = [], I = [], C = [];
  const walk = async (ni, parent) => {
    const n = J.nodes[ni];
    const M = mul(parent, mat4FromNode(n));
    if (n.mesh !== undefined) {
      for (const p of J.meshes[n.mesh].primitives) {
        if (p.mode !== undefined && p.mode !== 4) continue;
        const pos = read(p.attributes.POSITION);
        const uv = p.attributes.TEXCOORD_0 !== undefined ? read(p.attributes.TEXCOORD_0) : null;
        const idx = p.indices !== undefined ? read(p.indices) : Float64Array.from({ length: pos.length / 3 }, (_, i) => i);
        const m = J.materials?.[p.material] || {};
        const pbr = m.pbrMetallicRoughness || {};
        const f = pbr.baseColorFactor || [1, 1, 1, 1];
        const img = pbr.baseColorTexture && uv ? await texImage(pbr.baseColorTexture.index) : null;
        const base = P.length / 3;
        for (let i = 0; i < pos.length; i += 3) {
          const x = pos[i], y = pos[i + 1], z = pos[i + 2];
          P.push(M[0] * x + M[4] * y + M[8] * z + M[12], M[1] * x + M[5] * y + M[9] * z + M[13], M[2] * x + M[6] * y + M[10] * z + M[14]);
        }
        for (let t = 0; t < idx.length; t += 3) {
          const a = idx[t], b = idx[t + 1], c = idx[t + 2];
          I.push(base + a, base + b, base + c);
          let rgb = [f[0], f[1], f[2]];
          if (img) {
            const s = sample(img, (uv[a * 2] + uv[b * 2] + uv[c * 2]) / 3, (uv[a * 2 + 1] + uv[b * 2 + 1] + uv[c * 2 + 1]) / 3);
            rgb = [rgb[0] * s[0], rgb[1] * s[1], rgb[2] * s[2]];
          }
          C.push(...rgb);
        }
      }
    }
    for (const c of n.children || []) await walk(c, M);
  };
  const scene = J.scenes[J.scene || 0];
  const ID = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1];
  for (const r of scene.nodes) await walk(r, ID);

  const box = [1e9, 1e9, 1e9, -1e9, -1e9, -1e9];
  for (let i = 0; i < P.length; i += 3) for (let k = 0; k < 3; k++) { box[k] = Math.min(box[k], P[i + k]); box[k + 3] = Math.max(box[k + 3], P[i + k]); }
  return { pos: Float32Array.from(P), idx: Uint32Array.from(I), col: Float32Array.from(C), box };
}
