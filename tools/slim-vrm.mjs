// Shrinks a VRM 0.x file for source/: every image larger than MAX on a side is scaled down and
// stored as WebP; the meshes, skins, morphs and VRM extension stay as they are. The bake packs
// the textures into 1024 atlases anyway, so this loses nothing the game can show.
//
//   node tools/slim-vrm.mjs in.vrm out.vrm [max 1024]
import { readFileSync, writeFileSync } from 'node:fs';
import sharp from 'sharp';

const [, , src, dst, maxArg] = process.argv;
const MAX = +(maxArg || 1024);
const b = readFileSync(src);
const jl = b.readUInt32LE(12);
const J = JSON.parse(b.subarray(20, 20 + jl).toString());
const BIN = b.subarray(20 + jl + 8, 20 + jl + 8 + b.readUInt32LE(20 + jl));
const thumb = J.extensions?.VRM?.meta?.texture !== undefined ? J.textures[J.extensions.VRM.meta.texture]?.source : J.extensions?.VRMC_vrm?.meta?.thumbnailImage;

const views = J.bufferViews.map((v) => BIN.subarray(v.byteOffset || 0, (v.byteOffset || 0) + v.byteLength));
for (const [i, im] of J.images.entries()) {
  if (im.bufferView === undefined) continue;
  const lim = i === thumb ? 256 : MAX;
  const img = sharp(views[im.bufferView]);
  const { width, height } = await img.metadata();
  if (Math.max(width, height) <= lim && im.mimeType !== 'image/png') continue;
  const out = await sharp(views[im.bufferView]).resize({ width: Math.min(width, lim), height: Math.min(height, lim), fit: 'inside' }).webp({ quality: 90, alphaQuality: 100, effort: 6 }).toBuffer();
  if (out.length < views[im.bufferView].length) { views[im.bufferView] = out; im.mimeType = 'image/webp'; }
}
const parts = []; let len = 0;
J.bufferViews.forEach((v, i) => { const pad = (4 - (len % 4)) % 4; if (pad) { parts.push(Buffer.alloc(pad)); len += pad; } v.byteOffset = len; v.byteLength = views[i].length; v.buffer = 0; parts.push(views[i]); len += views[i].length; });
J.buffers = [{ byteLength: len }];
const js = Buffer.from(JSON.stringify(J)); const jp = (4 - (js.length % 4)) % 4; const jb = Buffer.concat([js, Buffer.alloc(jp, 32)]);
const bp = (4 - (len % 4)) % 4; const bb = Buffer.concat([...parts, Buffer.alloc(bp)]);
const h = Buffer.alloc(20); h.writeUInt32LE(0x46546c67, 0); h.writeUInt32LE(2, 4); h.writeUInt32LE(12 + 8 + jb.length + 8 + bb.length, 8); h.writeUInt32LE(jb.length, 12); h.writeUInt32LE(0x4e4f534a, 16);
const bh = Buffer.alloc(8); bh.writeUInt32LE(bb.length, 0); bh.writeUInt32LE(0x004e4942, 4);
writeFileSync(dst, Buffer.concat([h, jb, bh, bb]));
console.log(`${src} -> ${dst}: ${(b.length / 1048576).toFixed(1)} MB -> ${((28 + jb.length + bb.length) / 1048576).toFixed(1)} MB`);
