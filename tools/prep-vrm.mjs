// Bakes the VRoid sample avatars (source/vrm/*.vrm, VRM 0.x) into what the cart loads:
//
//   assets/models/<id>.glb    one node tree (bones named by humanoid slot), canonical facing
//                             (+Z, y up), identity rests, three skinned meshes:
//                             "face"    the head's parts with its own atlas and the expression morphs
//                             "body"    everything else, simplified to PS2 budget, its own atlas
//                             "outline" inverted hull of body and face skin, no material of its own
//   assets/models/<id>.spr    the spring bone chains and colliders, as text
//   assets/anim/rig.bin       the mocap clips (CKAN, anim.c), hips in units of hip height
//   src/gen/cast_meta.h       each model's hip height, standing height and joint sockets
//
//   node tools/prep-vrm.mjs
//
// The motion in source/anim is Quaternius's CC0 Universal Animation Library, retargeted by
// DOLLY's tools/retarget.mjs onto the VRM humanoid in the same canonical frame.
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import sharp from 'sharp';
import { MeshoptSimplifier } from 'meshoptimizer';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const CAST = ['yyc_B', 'yyc_C', 'yyc_D', 'shino_light', 'fumiriya'];
const SHORT = { yyc_B: 'char_b', yyc_C: 'char_c', yyc_D: 'char_d', shino_light: 'char_s', fumiriya: 'char_f' };
const BODY_KEEP = 0.5;      // share of body triangles kept by the simplifier
const OUTLINE = 0.0052;     // hull push in metres
// the expression morphs, in the order actor.c reads them
const EXPR = ['blink', 'a', 'i', 'u', 'e', 'o', 'joy', 'angry', 'sorrow', 'fun'];
const CLIPS = `Idle_Loop Idle_Talking_Loop Idle_FoldArms_Loop Idle_No_Loop Idle_TalkingPhone_Loop Walk_Loop Walk_Formal_Loop
 Jog_Fwd_Loop Sprint_Loop Jump_Start Jump_Loop Jump_Land NinjaJump_Start NinjaJump_Idle_Loop NinjaJump_Land Roll Slide_Start Slide_Loop Slide_Exit
 Hit_Chest Hit_Head Hit_Knockback Death01 LayToIdle Sword_Idle Sword_Regular_A Sword_Regular_A_Rec Sword_Regular_B Sword_Regular_B_Rec
 Sword_Regular_C Sword_Regular_Combo Sword_Heavy_Combo Sword_Dash Sword_Block Sword_Attack Idle_Shield_Loop Shield_Dash Shield_OneShot Idle_Shield_Break
 Spell_Simple_Enter Spell_Simple_Idle_Loop Spell_Simple_Shoot Spell_Simple_Exit Punch_Jab Punch_Cross Melee_Hook Melee_Hook_Rec OverhandThrow
 Sitting_Enter Sitting_Exit Sitting_Idle_Loop Sitting_Talking_Loop Interact Fixing_Kneeling PickUp_Table Walk_Carry_Loop Yes Dance_Loop Consume
 Crouch_Idle_Loop Crouch_Fwd_Loop Zombie_Idle_Loop Zombie_Walk_Fwd_Loop Zombie_Scratch Push_Loop Chest_Open`.split(/\s+/);

function readGlb(path) {
  const b = readFileSync(path);
  const jl = b.readUInt32LE(12);
  const J = JSON.parse(b.subarray(20, 20 + jl).toString());
  const bs = 20 + jl + 8;
  const BIN = b.subarray(bs, bs + b.readUInt32LE(20 + jl));
  const COMP = { SCALAR: 1, VEC2: 2, VEC3: 3, VEC4: 4, MAT4: 16 };
  const TY = { 5126: Float32Array, 5125: Uint32Array, 5123: Uint16Array, 5122: Int16Array, 5121: Uint8Array };
  const acc = (i) => {
    const a = J.accessors[i], c = COMP[a.type], T = TY[a.componentType], n = a.count * c;
    if (a.bufferView === undefined) return new T(n);
    const v = J.bufferViews[a.bufferView], off = (v.byteOffset || 0) + (a.byteOffset || 0);
    const stride = v.byteStride || c * T.BYTES_PER_ELEMENT, out = new T(n);
    for (let e = 0; e < a.count; e++) for (let k = 0; k < c; k++) {
      const p = off + e * stride + k * T.BYTES_PER_ELEMENT;
      out[e * c + k] = T === Float32Array ? BIN.readFloatLE(p) : T === Uint32Array ? BIN.readUInt32LE(p) : T === Uint16Array ? BIN.readUInt16LE(p) : T === Int16Array ? BIN.readInt16LE(p) : BIN[p];
    }
    return out;
  };
  const view = (i) => { const v = J.bufferViews[i]; return BIN.subarray(v.byteOffset || 0, (v.byteOffset || 0) + v.byteLength); };
  return { J, acc, view };
}

// gather every primitive of a set of materials into one vertex soup
function gather(prims) {
  let nv = 0, ni = 0;
  for (const p of prims) { nv += p.pos.length / 3; ni += p.idx.length; }
  const M = { pos: new Float32Array(nv * 3), nor: new Float32Array(nv * 3), uv: new Float32Array(nv * 2), jnt: new Uint16Array(nv * 4), wgt: new Float32Array(nv * 4), idx: new Uint32Array(ni), mat: new Int32Array(ni / 3), morph: EXPR.map(() => new Float32Array(nv * 3)) };
  let v = 0, i = 0;
  for (const p of prims) {
    const n = p.pos.length / 3;
    M.pos.set(p.pos, v * 3); M.nor.set(p.nor, v * 3); M.uv.set(p.uv, v * 2); M.jnt.set(p.jnt, v * 4); M.wgt.set(p.wgt, v * 4);
    if (p.morph) p.morph.forEach((d, k) => d && M.morph[k].set(d, v * 3));
    for (let k = 0; k < p.idx.length; k++) M.idx[i + k] = p.idx[k] + v;
    for (let k = 0; k < p.idx.length / 3; k++) M.mat[i / 3 + k] = p.mat;
    v += n; i += p.idx.length;
  }
  M.nv = nv;
  return M;
}

// keep only the vertices the index list still uses
function compact(M) {
  const map = new Int32Array(M.nv).fill(-1); let n = 0;
  for (const i of M.idx) if (map[i] < 0) map[i] = n++;
  const O = { nv: n, idx: M.idx.map((i) => map[i]), mat: M.mat, pos: new Float32Array(n * 3), nor: new Float32Array(n * 3), uv: new Float32Array(n * 2), jnt: new Uint16Array(n * 4), wgt: new Float32Array(n * 4), morph: M.morph.map(() => new Float32Array(n * 3)) };
  for (let i = 0; i < M.nv; i++) {
    const t = map[i]; if (t < 0) continue;
    for (let k = 0; k < 3; k++) { O.pos[t * 3 + k] = M.pos[i * 3 + k]; O.nor[t * 3 + k] = M.nor[i * 3 + k]; }
    O.uv[t * 2] = M.uv[i * 2]; O.uv[t * 2 + 1] = M.uv[i * 2 + 1];
    for (let k = 0; k < 4; k++) { O.jnt[t * 4 + k] = M.jnt[i * 4 + k]; O.wgt[t * 4 + k] = M.wgt[i * 4 + k]; }
    M.morph.forEach((d, j) => { for (let k = 0; k < 3; k++) O.morph[j][t * 3 + k] = d[i * 3 + k]; });
  }
  return O;
}

// one atlas: shelf-pack the materials' textures at a common scale, remap the uvs
async function atlas(M, materials, size, name, J, view) {
  const used = [...new Set(M.mat)];
  const items = [];
  for (const m of used) {
    const mat = J.materials[m];
    const ti = mat.pbrMetallicRoughness.baseColorTexture?.index;
    const fac = mat.pbrMetallicRoughness.baseColorFactor || [1, 1, 1, 1];
    let raw, w, h;
    if (ti === undefined) { w = h = 4; raw = Buffer.alloc(64, 255); }
    else {
      const img = J.images[J.textures[ti].source];
      const r = await sharp(view(img.bufferView)).ensureAlpha().raw().toBuffer({ resolveWithObject: true });
      raw = r.data; w = r.info.width; h = r.info.height;
    }
    for (let p = 0; p < raw.length; p += 4) for (let k = 0; k < 3; k++) raw[p + k] = Math.min(255, raw[p + k] * fac[k]);
    items.push({ m, raw, w, h, name: mat.name });
  }
  const PAD = 3;
  let scale = 1, place;
  for (;;) {
    place = []; let x = 0, y = 0, sh = 0, ok = true;
    const order = [...items].sort((a, b) => b.h - a.h);
    for (const it of order) {
      const w = Math.max(4, Math.round(it.w * scale)), h = Math.max(4, Math.round(it.h * scale));
      if (x + w + PAD * 2 > size) { x = 0; y += sh; sh = 0; }
      if (y + h + PAD * 2 > size || w + PAD * 2 > size) { ok = false; break; }
      place.push({ it, x: x + PAD, y: y + PAD, w, h });
      x += w + PAD * 2; sh = Math.max(sh, h + PAD * 2);
    }
    if (ok) break;
    scale *= 0.92;
  }
  const out = Buffer.alloc(size * size * 4);
  const rects = {};
  for (const p of place) {
    const t = await sharp(p.it.raw, { raw: { width: p.it.w, height: p.it.h, channels: 4 } }).resize(p.w, p.h, { kernel: 'lanczos3' }).raw().toBuffer();
    // extrude PAD pixels so filtering cannot pick up a neighbour
    for (let y = -PAD; y < p.h + PAD; y++) for (let x = -PAD; x < p.w + PAD; x++) {
      const sx = Math.max(0, Math.min(p.w - 1, x)), sy = Math.max(0, Math.min(p.h - 1, y));
      t.copy(out, ((p.y + y) * size + p.x + x) * 4, (sy * p.w + sx) * 4, (sy * p.w + sx) * 4 + 4);
    }
    rects[p.it.m] = p;
  }
  // a vertex can be shared by triangles of one material only (prims never share)
  const seen = new Uint8Array(M.nv);
  for (let t = 0; t < M.idx.length / 3; t++) {
    const p = rects[M.mat[t]];
    for (let c = 0; c < 3; c++) {
      const v = M.idx[t * 3 + c];
      if (seen[v]) continue; seen[v] = 1;
      let u = M.uv[v * 2], w = M.uv[v * 2 + 1];
      u = u - Math.floor(u); if (M.uv[v * 2] === 1) u = 1;
      w = w - Math.floor(w); if (M.uv[v * 2 + 1] === 1) w = 1;
      M.uv[v * 2] = (p.x + u * p.w) / size; M.uv[v * 2 + 1] = (p.y + w * p.h) / size;
    }
  }
  const png = await sharp(out, { raw: { width: size, height: size, channels: 4 } }).png({ compressionLevel: 9, palette: false }).toBuffer();
  console.log(`  atlas ${name}: ${used.length} mats scale ${scale.toFixed(2)} ${(png.length / 1024) | 0} KB`);
  return { png, rects: Object.values(rects).map((p) => ({ name: p.it.name, x: p.x, y: p.y, w: p.w, h: p.h })) };
}

class GlbOut {
  constructor() { this.parts = []; this.len = 0; this.J = { asset: { version: '2.0', generator: 'keyholder prep-vrm' }, buffers: [{}], bufferViews: [], accessors: [], images: [], samplers: [{ magFilter: 9729, minFilter: 9729, wrapS: 33071, wrapT: 33071 }], textures: [], materials: [], meshes: [], nodes: [], skins: [], scenes: [{ nodes: [] }], scene: 0 }; }
  blob(buf) { const b = Buffer.from(buf.buffer ? new Uint8Array(buf.buffer, buf.byteOffset, buf.byteLength) : buf); const pad = (4 - (this.len % 4)) % 4; if (pad) { this.parts.push(Buffer.alloc(pad)); this.len += pad; } this.parts.push(b); const o = this.len; this.len += b.length; this.J.bufferViews.push({ buffer: 0, byteOffset: o, byteLength: b.length }); return this.J.bufferViews.length - 1; }
  acc(arr, type, comp, bounds, norm) { const v = this.blob(arr); const c = { SCALAR: 1, VEC2: 2, VEC3: 3, VEC4: 4, MAT4: 16 }[type]; const a = { bufferView: v, componentType: comp, count: arr.length / c, type }; if (norm) a.normalized = true; if (bounds) { const mn = Array(c).fill(1e30), mx = Array(c).fill(-1e30); for (let i = 0; i < arr.length; i++) { const k = i % c; if (arr[i] < mn[k]) mn[k] = arr[i]; if (arr[i] > mx[k]) mx[k] = arr[i]; } a.min = mn; a.max = mx; } this.J.accessors.push(a); return this.J.accessors.length - 1; }
  tex(png) { const v = this.blob(png); this.J.images.push({ bufferView: v, mimeType: 'image/png' }); this.J.textures.push({ source: this.J.images.length - 1, sampler: 0 }); return this.J.textures.length - 1; }
  write(path) { const js = Buffer.from(JSON.stringify((this.J.buffers[0].byteLength = this.len, this.J))); const jp = (4 - (js.length % 4)) % 4; const j = Buffer.concat([js, Buffer.alloc(jp, 32)]); const bp = (4 - (this.len % 4)) % 4; const b = Buffer.concat([...this.parts, Buffer.alloc(bp)]); const h = Buffer.alloc(20); h.writeUInt32LE(0x46546c67, 0); h.writeUInt32LE(2, 4); h.writeUInt32LE(12 + 8 + j.length + 8 + b.length, 8); h.writeUInt32LE(j.length, 12); h.writeUInt32LE(0x4e4f534a, 16); const bh = Buffer.alloc(8); bh.writeUInt32LE(b.length, 0); bh.writeUInt32LE(0x004e4942, 4); writeFileSync(path, Buffer.concat([h, j, bh, b])); }
}

const metas = [];
async function bake(file) {
  const { J, acc, view } = readGlb(join(ROOT, 'source/vrm', file + '.vrm'));
  const V = J.extensions.VRM;
  // bones by humanoid slot; other joints (hair, skirt, bust) keep a sanitized name
  const human = {}; for (const b of V.humanoid.humanBones) human[b.node] = b.bone;
  // VRM 0 has thumb proximal, intermediate, distal where the clips (VRM 1 names) have metacarpal, proximal, distal
  const v1 = (b) => b.replace('ThumbProximal', 'ThumbMetacarpal').replace('ThumbIntermediate', 'ThumbProximal');
  const nameOf = (i) => (human[i] ? v1(human[i]) : 's' + i);
  // world position of every node (VRoid rests are translation only: checked below)
  const parent = {}; J.nodes.forEach((n, i) => (n.children || []).forEach((c) => (parent[c] = i)));
  const world = (i) => { let x = 0, y = 0, z = 0; for (let n = i; n !== undefined; n = parent[n]) { const t = J.nodes[n].translation || [0, 0, 0], r = J.nodes[n].rotation || [0, 0, 0, 1]; if (Math.abs(r[0]) + Math.abs(r[1]) + Math.abs(r[2]) > 1e-4) throw new Error('rotated rest: ' + J.nodes[n].name); x += t[0]; y += t[1]; z += t[2]; } return [x, y, z]; };
  // skins are per mesh: unify to the first skin's joints
  const joints = J.skins[0].joints;
  const jointIndex = new Map(joints.map((j, i) => [j, i]));
  // binds for the fixed expression list
  const groups = V.blendShapeMaster.blendShapeGroups;
  const exprBinds = EXPR.map((e) => groups.find((g) => g.presetName === e)?.binds || []);
  // every primitive
  const facePrims = [], bodyPrims = [], meshNodes = J.nodes.filter((n) => n.mesh !== undefined);
  for (const node of meshNodes) {
    const skin = J.skins[node.skin];
    const remap = skin.joints.map((j) => { if (!jointIndex.has(j)) throw new Error('joint outside skin 0'); return jointIndex.get(j); });
    J.meshes[node.mesh].primitives.forEach((p) => {
      const at = p.attributes, pos = acc(at.POSITION), nor = acc(at.NORMAL), uv = acc(at.TEXCOORD_0), jn = acc(at.JOINTS_0), wg = acc(at.WEIGHTS_0);
      const idx = Uint32Array.from(acc(p.indices));
      const jnt = new Uint16Array(jn.length); for (let i = 0; i < jn.length; i++) jnt[i] = remap[jn[i]];
      // canonical facing: VRM 0 faces -Z, turn 180 about y
      for (let i = 0; i < pos.length; i += 3) { pos[i] = -pos[i]; pos[i + 2] = -pos[i + 2]; nor[i] = -nor[i]; nor[i + 2] = -nor[i + 2]; }
      let morph = null;
      if (p.targets?.length) {
        morph = EXPR.map((_, e) => {
          const d = new Float32Array(pos.length); let any = false;
          for (const b of exprBinds[e]) {
            if (b.mesh !== node.mesh || !p.targets[b.index]) continue;
            const t = acc(p.targets[b.index].POSITION), w = b.weight / 100; any = true;
            for (let i = 0; i < t.length; i += 3) { d[i] -= t[i] * w; d[i + 1] += t[i + 1] * w; d[i + 2] -= t[i + 2] * w; }
          }
          return any ? d : null;
        });
      }
      const rec = { pos, nor, uv, jnt, wgt: wg, idx, mat: p.material, morph };
      (p.targets?.length ? facePrims : bodyPrims).push(rec);
    });
  }
  const faceSrc = gather(facePrims), bodySrc = gather(bodyPrims);
  // simplify the body material by material so a jacket cannot eat a shoe
  await MeshoptSimplifier.ready;
  const kept = []; const matKept = [];
  for (const m of new Set(bodySrc.mat)) {
    const ids = []; for (let t = 0; t < bodySrc.mat.length; t++) if (bodySrc.mat[t] === m) ids.push(bodySrc.idx[t * 3], bodySrc.idx[t * 3 + 1], bodySrc.idx[t * 3 + 2]);
    let out = Uint32Array.from(ids);
    if (ids.length > 360) {
      // a private vertex list per material: the simplifier's error is relative to what it is given
      const map = new Map(), lp = [];
      const li = Uint32Array.from(ids, (v) => { if (!map.has(v)) { map.set(v, map.size); lp.push(bodySrc.pos[v * 3], bodySrc.pos[v * 3 + 1], bodySrc.pos[v * 3 + 2]); } return map.get(v); });
      const back = [...map.keys()];
      const [sm] = MeshoptSimplifier.simplify(li, Float32Array.from(lp), 3, Math.floor(ids.length * BODY_KEEP / 3) * 3, 0.05, []);
      console.log('   ', J.materials[m].name.slice(0, 20), ids.length / 3, '->', sm.length / 3, 'nan', lp.some((x) => !isFinite(x)));
      if (sm.length) out = Uint32Array.from(sm, (v) => back[v]);
    }
    kept.push(...out); for (let t = 0; t < out.length / 3; t++) matKept.push(m);
  }
  const body = compact({ ...bodySrc, idx: Uint32Array.from(kept), mat: Int32Array.from(matKept) });
  const face = compact(faceSrc);
  console.log(`${file}: body ${bodySrc.idx.length / 3} -> ${body.idx.length / 3} tris, ${body.nv} verts; face ${face.idx.length / 3} tris, ${face.nv} verts, joints ${joints.length}`);

  // face shading: normals off a head sphere so the cel band never cuts across the nose
  const headNode = Object.entries(human).find(([, v]) => v === 'head')[0];
  const hw = world(+headNode); const hc = [-hw[0], hw[1] + 0.06, -hw[2]];
  for (let i = 0; i < face.nv; i++) { const d = [face.pos[i * 3] - hc[0], face.pos[i * 3 + 1] - hc[1], face.pos[i * 3 + 2] - hc[2]]; const l = Math.hypot(...d) || 1; for (let k = 0; k < 3; k++) face.nor[i * 3 + k] = d[k] / l; }

  const faceAt = await atlas(face, J.materials, 1024, 'face', J, view);
  const bodyAt = await atlas(body, J.materials, 1024, 'body', J, view);

  // the hull: body and the face's skin (no eyes, lashes or mouth), pushed out and flipped
  const faceSkin = [...new Set(face.mat)].filter((m) => /Face_00/.test(J.materials[m].name));
  const hullFace = []; for (let t = 0; t < face.mat.length; t++) if (faceSkin.includes(face.mat[t])) hullFace.push(face.idx[t * 3] + body.nv, face.idx[t * 3 + 2] + body.nv, face.idx[t * 3 + 1] + body.nv);
  const hn = body.nv + face.nv, hp = new Float32Array(hn * 3), hnor = new Float32Array(hn * 3), hj = new Uint16Array(hn * 4), hw2 = new Float32Array(hn * 4), push = (S, o) => { for (let i = 0; i < S.nv; i++) { for (let k = 0; k < 3; k++) { hp[(o + i) * 3 + k] = S.pos[i * 3 + k] + S.nor[i * 3 + k] * OUTLINE; hnor[(o + i) * 3 + k] = S.nor[i * 3 + k]; } for (let k = 0; k < 4; k++) { hj[(o + i) * 4 + k] = S.jnt[i * 4 + k]; hw2[(o + i) * 4 + k] = S.wgt[i * 4 + k]; } } };
  push(body, 0); push(face, body.nv);
  const hullIdx = []; for (let t = 0; t < body.idx.length; t += 3) hullIdx.push(body.idx[t], body.idx[t + 2], body.idx[t + 1]); hullIdx.push(...hullFace);
  const hull = compact({ nv: hn, pos: hp, nor: hnor, uv: new Float32Array(hn * 2), jnt: hj, wgt: hw2, idx: Uint32Array.from(hullIdx), mat: new Int32Array(hullIdx.length / 3), morph: [] });

  // write
  const G = new GlbOut(), O = G.J;
  const add = (src, n, tex) => {
    const attrs = { POSITION: G.acc(src.pos, 'VEC3', 5126, true), NORMAL: G.acc(src.nor, 'VEC3', 5126), JOINTS_0: G.acc(src.jnt, 'VEC4', 5123), WEIGHTS_0: G.acc(src.wgt, 'VEC4', 5126) };
    let material;
    if (tex !== null) { attrs.TEXCOORD_0 = G.acc(src.uv, 'VEC2', 5126); O.materials.push({ name: n, alphaMode: 'MASK', alphaCutoff: 0.45, pbrMetallicRoughness: { baseColorTexture: { index: tex }, metallicFactor: 0, roughnessFactor: 1 } }); material = O.materials.length - 1; }
    else { O.materials.push({ name: n, pbrMetallicRoughness: { baseColorFactor: [0.08, 0.06, 0.08, 1], metallicFactor: 0, roughnessFactor: 1 } }); material = O.materials.length - 1; }
    const prim = { attributes: attrs, indices: src.nv < 65535 ? G.acc(Uint16Array.from(src.idx), 'SCALAR', 5123) : G.acc(Uint32Array.from(src.idx), 'SCALAR', 5125), material };
    if (n === 'face') prim.targets = EXPR.map((_, e) => ({ POSITION: G.acc(src.morph[e], 'VEC3', 5126, true) }));
    O.meshes.push({ name: n, primitives: [prim] });
    O.nodes.push({ name: n, mesh: O.meshes.length - 1, skin: 0 });
    return O.nodes.length - 1;
  };
  const meshIds = [add(body, 'body', G.tex(bodyAt.png)), add(face, 'face', G.tex(faceAt.png)), add(hull, 'outline', null)];
  // the skeleton as the source's hierarchy, translations flipped, rotations identity
  const newId = new Map();
  const walk = (i) => { const n = J.nodes[i]; const t = n.translation || [0, 0, 0]; const rec = { name: nameOf(i), translation: [-t[0], t[1], -t[2]] }; O.nodes.push(rec); const id = O.nodes.length - 1; newId.set(i, id); const ch = (n.children || []).filter((c) => J.nodes[c].mesh === undefined).map(walk); if (ch.length) rec.children = ch; return id; };
  const rootSrc = J.scenes[0].nodes.find((i) => J.nodes[i].name === 'Root');
  const skelRoot = walk(rootSrc);
  // inverse bind: pure translation, from the rest positions
  const ibm = new Float32Array(joints.length * 16);
  joints.forEach((j, i) => { const w = world(j); const m = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, w[0], -w[1], w[2], 1]; ibm.set(m, i * 16); });
  O.skins.push({ joints: joints.map((j) => newId.get(j)), inverseBindMatrices: G.acc(ibm, 'MAT4', 5126), skeleton: skelRoot });
  O.scenes[0].nodes = [...meshIds, skelRoot];
  const id = SHORT[file];
  G.write(join(ROOT, 'assets/models', id + '.glb'));

  // spring bones
  const lines = [];
  const S = V.secondaryAnimation;
  S.colliderGroups.forEach((g) => { for (const c of g.colliders) lines.push(`c ${nameOf(g.node)} ${c.radius} ${-c.offset.x} ${c.offset.y} ${-c.offset.z}`); });
  for (const g of S.boneGroups) {
    if (/bust/i.test(g.comment || '')) continue;   // no bust physics in this game
    if (!g.bones.length) continue;
    lines.push(`g ${g.stiffiness} ${g.gravityPower} ${g.dragForce} ${g.hitRadius} ${g.bones.length} ${g.bones.map(nameOf).join(' ')}`);
  }
  writeFileSync(join(ROOT, 'assets/models', id + '.spr'), lines.join('\n') + '\n');

  // sizes for the runtime
  const pick = (b) => world(+Object.entries(human).find(([, v]) => v === b)[0]);
  const hipsH = pick('hips')[1] - Math.min(pick('leftFoot')[1], pick('rightFoot')[1]);
  metas.push({ id, file, hipsH, height: pick('head')[1] + 0.12, tris: body.idx.length / 3 + face.idx.length / 3, hull: hull.idx.length / 3, rects: bodyAt.rects });
}

function rig() {
  const A = JSON.parse(readFileSync(join(ROOT, 'source/anim/anims_all.json')));
  const names = [...new Set(CLIPS.flatMap((c) => Object.keys(A.clips[c]?.bones || {})))].sort();
  if (names.length > 60) throw new Error('rig bones ' + names.length);
  const chunks = [];
  const u32 = (v) => { const b = Buffer.alloc(4); b.writeUInt32LE(v); chunks.push(b); };
  const f32 = (v) => { const b = Buffer.alloc(4); b.writeFloatLE(v); chunks.push(b); };
  const str = (s) => { chunks.push(Buffer.from([s.length]), Buffer.from(s)); };
  chunks.push(Buffer.from('CKAN')); u32(1); u32(names.length); names.forEach(str);
  const have = CLIPS.filter((c) => A.clips[c]);
  u32(have.length);
  for (const name of have) {
    const c = A.clips[name];
    str(name); f32(c.dur); u32(c.n);
    for (let i = 0; i < 8; i++) f32(0);
    const tr = {}; for (const [b, s] of Object.entries(c.bones)) tr[b] = Buffer.from(s, 'base64');
    // a one shot's bake samples its last frame back at the first pose: hold the frame before it instead
    const oneShot = !/Loop|Idle|Dance/.test(name);
    if (oneShot && c.n > 2) for (const b of Object.keys(tr)) tr[b].copy(tr[b], (c.n - 1) * 8, (c.n - 2) * 8, (c.n - 1) * 8);
    const q = Buffer.alloc(c.n * names.length * 8);
    for (let f = 0; f < c.n; f++) names.forEach((b, i) => { const o = (f * names.length + i) * 8; if (tr[b]) tr[b].copy(q, o, f * 8, f * 8 + 8); else { q.writeInt16LE(32767, o + 6); } });
    chunks.push(q);
    const hp = Buffer.from(c.hips, 'base64'), h = Buffer.alloc(c.n * 12);
    if (oneShot && c.n > 2) hp.copy(hp, (c.n - 1) * 6, (c.n - 2) * 6, (c.n - 1) * 6);
    for (let i = 0; i < c.n * 3; i++) h.writeFloatLE(hp.readInt16LE(i * 2) / 10000, i * 4);
    chunks.push(h);
    // clip speed: metres a second per unit of hip height, for foot-locked walking
    f32(c.speed || 0);
  }
  const out = Buffer.concat(chunks);
  writeFileSync(join(ROOT, 'assets/anim/rig.bin'), out);
  console.log(`rig: ${have.length} clips, ${names.length} bones, ${(out.length / 1048576).toFixed(2)} MB; missing: ${CLIPS.filter((c) => !A.clips[c]).join(' ') || 'none'}`);
}

mkdirSync(join(ROOT, 'assets/models'), { recursive: true });
mkdirSync(join(ROOT, 'assets/anim'), { recursive: true });
mkdirSync(join(ROOT, 'src/gen'), { recursive: true });
rig();
if (process.argv[2] === 'rig') process.exit(0);
for (const f of process.argv[2] ? [process.argv[2]] : CAST) await bake(f);
const kind = (n) => /hair/i.test(n) ? 0 : /tops/i.test(n) ? 1 : /bottoms/i.test(n) ? 2 : /shoes/i.test(n) ? 3 : /access/i.test(n) ? 1 : -1;
let h = '/* made by tools/prep-vrm.mjs */\n#ifndef CAST_META_H\n#define CAST_META_H\ntypedef struct { int kind, x, y, w, h; } cast_rect;   /* kind: 0 hair, 1 top, 2 bottom, 3 shoes */\ntypedef struct { const char *glb, *spr; float hips_h, height; int nrect; cast_rect rect[24]; } cast_meta;\nstatic const cast_meta CAST_META[] = {\n';
for (const m of metas) { const r = m.rects.filter((x) => kind(x.name) >= 0); h += `  { "${m.id}.glb", "${m.id}.spr", ${m.hipsH.toFixed(4)}f, ${m.height.toFixed(4)}f, ${r.length}, { ${r.map((x) => `{${kind(x.name)},${x.x},${x.y},${x.w},${x.h}}`).join(', ')} } },  /* ${m.file}: ${m.tris} tris + hull ${m.hull} */\n`; }
h += '};\n#endif\n';
writeFileSync(join(ROOT, 'src/gen/cast_meta.h'), h);
writeFileSync(join(ROOT, 'assets/models/atlas-rects.json'), JSON.stringify(Object.fromEntries(metas.map((m) => [m.id, m.rects])), null, 0));
