// Bakes the VRoid sample avatars (CAST below: source/vrm, source/vrm-cc0; VRM 0.x) into what the cart loads:
//
//   assets/models/<id>.glb    one node tree (bones named by humanoid slot), canonical facing
//                             (+Z, y up), identity rests, six skinned meshes on one skin padded to
//                             SKIN_JOINTS (one shader program set for the whole cast):
//                             "face"    the head's parts with its own atlas and the expression morphs
//                             "body"    everything else, simplified to PS2 budget, its own atlas
//                             "outline" inverted hull of body and face skin (and any glasses), no material
//                             "far", "farface", "faroutline": the light level of detail (about 5k triangles),
//                             same atlases, no morphs
//   options per body: girth (a heavier trunk), paint { age, grey, stubble }, glasses, and for kitbashes
//     drop (materials of the base left out), parts [{ from, take, scale }] (another sample's materials,
//     its hair or its whole head, moved onto the base's head with their spring bones grafted on),
//     shells (garments grown from the body skin and painted: paintShell), retex (bake time recolours),
//     skipPrims, dropBones, keep (share of body triangles kept), bodyAtlas and faceAtlas (512 for crowds)
//   every body: triangles whose texture is see-through everywhere are dropped (their outline hull has no
//     alpha test and would draw them as dark panels: the coat tails behind Vee, Dex and Ruth)
//   assets/models/cast-meta.json  each body's last bake, so one body can be baked alone
//   assets/models/<id>.spr    the spring bone chains and colliders, as text
//   assets/anim/rig.bin       the mocap clips (CKAN, anim.c), hips in units of hip height
//   src/gen/cast_meta.h       each model's hip height, standing height and joint sockets
//
//   node tools/prep-vrm.mjs [char_x ...]
//
// The motion in source/anim is Quaternius's CC0 Universal Animation Library, retargeted by
// DOLLY's tools/retarget.mjs onto the VRM humanoid in the same canonical frame.
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import sharp from 'sharp';
import { MeshoptSimplifier } from 'meshoptimizer';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
// limb groups for shells (paintShell below), by humanoid slot
const LIMB = {
  torso: ['hips', 'spine', 'chest', 'upperChest'],
  waist: ['spine', 'chest', 'upperChest'],
  shoulders: ['leftShoulder', 'rightShoulder'],
  upperArms: ['leftUpperArm', 'rightUpperArm'],
  lowerArms: ['leftLowerArm', 'rightLowerArm'],
  upperLegs: ['leftUpperLeg', 'rightUpperLeg'],
  lowerLegs: ['leftLowerLeg', 'rightLowerLeg'],
};
const limbs = (...k) => k.flatMap((x) => LIMB[x] || [x]);
// shell recipes: tops over the torso and arms (short: sleeves end at the elbow), bottoms over the hips and legs
const TOP = (name, color, o = {}) => ({ name: 'Shell_Tops_' + name, bones: limbs('waist', 'shoulders', 'upperArms', ...(o.short ? [] : ['lowerArms']), 'hips', 'neck'), yMinH: 1, neck: .3, push: .008, color, ...o });
const BOTTOM = (name, color, o = {}) => ({ name: 'Shell_Bottoms_' + name, bones: limbs('hips', 'upperLegs', ...(o.shorts ? [] : ['lowerLegs'])), push: .006, color, ...o });
// the bodies, in actor_model order (actor.h); src is under source/
const CAST = [
  { src: 'vrm/yyc_B.vrm', id: 'char_b' }, { src: 'vrm/yyc_C.vrm', id: 'char_c' }, { src: 'vrm/yyc_D.vrm', id: 'char_d' },
  { src: 'vrm/shino_light.vrm', id: 'char_s', drop: /Tops/,   // v1.2: a shirt over the crop top
    shells: [TOP('blouse', [236, 236, 240], { short: true, details: [{ kind: 'placket' }, { kind: 'hem' }] })] }, { src: 'vrm/fumiriya.vrm', id: 'char_f' },
  // v1.1: the VRoid Project CC0 samples (source/vrm-cc0/SOURCES.txt), some as bake time variants
  { src: 'vrm-cc0/HairSample_Male.vrm', id: 'char_h' },
  { src: 'vrm-cc0/Base_Male.vrm', id: 'char_m' },
  { src: 'vrm-cc0/Base_Male.vrm', id: 'char_o', paint: { age: 1, grey: 1 }, glasses: true },
  { src: 'vrm-cc0/Base_Male.vrm', id: 'char_p', girth: .16, paint: { stubble: .5, age: .35 } },
  { src: 'vrm-cc0/HairSample_Male.vrm', id: 'char_u', girth: .5, paint: { stubble: .7, stubbleColor: [40, 34, 40] } },
  { src: 'vrm-cc0/AvatarSample_D.vrm', id: 'char_k' },
  { src: 'vrm-cc0/AvatarSample_E.vrm', id: 'char_e' },
  { src: 'vrm-cc0/AvatarSample_F.vrm', id: 'char_v' },
  { src: 'vrm-cc0/AvatarSample_G.vrm', id: 'char_g' },
  { src: 'vrm-cc0/Base_Female.vrm', id: 'char_w' },
  { src: 'vrm-cc0/Base_Female.vrm', id: 'char_r', paint: { age: .8, grey: .8 }, glasses: { m: .004 } },
  // v1.2: a body for every named character. Kitbashes of the samples above: another sample's hair or
  // whole head (parts), garments dropped (drop) and grown back as painted shells, bake time recolours
  // (retex) and skin tones. Atlases are shared by the parts, so a body costs about what a sample does.
  { src: 'vrm-cc0/AvatarSample_D_Darkness.vrm', id: 'char_a' },                                   // Jo: white bob, long black coat dress
  { src: 'vrm-cc0/HairSample_Female.vrm', id: 'char_i', drop: /Tops|HAIR_02/, skipPrims: { mesh: /Hair/, prims: [0, 1, 3, 4] },                    // Pat: black twin tails, zip hoodie, jeans
    shells: [TOP('hoodie', [96, 140, 92], { pattern: 'heather', push: .011, details: [{ kind: 'zip' }, { kind: 'hem', h: .05 }, { kind: 'cuffs', w: .045 }] }),
      BOTTOM('jeans', [62, 84, 124], { pattern: 'denim', details: [{ kind: 'pockets' }] })] },
  { src: 'vrm-cc0/Base_Male.vrm', id: 'char_j', drop: /Tops|Accessory|Bottoms|HAIR/, parts: [{ from: 'vrm-cc0/Base_Female.vrm', take: /HAIR/ }],   // Lou: sandy crop, sage sweater, jeans
    retex: [{ mat: /HAIR/, tint: [176, 140, 96] }],
    shells: [TOP('sweater', [120, 140, 110], { pattern: 'knit', push: .01, details: [{ kind: 'hem', h: .045 }, { kind: 'cuffs' }] }),
      BOTTOM('jeans', [70, 92, 130], { pattern: 'denim', details: [{ kind: 'pockets' }] })] },
  { src: 'vrm-cc0/Base_Female.vrm', id: 'char_l', drop: /Tops|Accessory|Bottoms|HAIR/, parts: [{ from: 'vrm-cc0/AvatarSample_E.vrm', take: /HAIR/, scale: .94 }],   // Gloria: grey buns, glasses, cardigan, slacks
    paint: { age: .8, grey: .9 }, glasses: { m: .004 },
    shells: [TOP('cardigan', [150, 130, 168], { pattern: 'knit', push: .011, details: [{ kind: 'open', w: .05, color: [236, 228, 210], buttons: true }, { kind: 'hem', h: .04 }, { kind: 'cuffs' }] }),
      BOTTOM('slacks', [120, 108, 96], { pattern: 'heather' })] },
  { src: 'vrm-cc0/AvatarSample_E.vrm', id: 'char_n', drop: /Onepice|HAIR/, parts: [{ from: 'vrm-cc0/Base_Female.vrm', take: /HAIR/, scale: 1.04 }],   // Milo: short brown hair, striped tee, cargo shorts
    retex: [{ mat: /HAIR/, tint: [104, 66, 40] }],
    shells: [TOP('tee', [80, 150, 200], { short: true, details: [{ kind: 'hem' }, { kind: 'band', y0: 1.25, y1: 1.32, color: [240, 240, 240] }] }),
      BOTTOM('shorts', [150, 128, 88], { shorts: true, pattern: 'heather', details: [{ kind: 'hem' }] })] },
  { src: 'vrm/shino_light.vrm', id: 'char_q', drop: /Tops|Bottoms/,                              // Sam: long black hair, striped tee, overalls
    shells: [TOP('tee', [236, 232, 220], { short: true, pattern: 'stripe', c2: [190, 70, 60], period: .05, duty: .3,
      details: [{ kind: 'bib', hw: .1, top: 1.42, color: [70, 98, 140] }, { kind: 'hem' }] }),
      BOTTOM('overalls', [70, 98, 140], { pattern: 'denim', details: [{ kind: 'pockets' }, { kind: 'hem', h: .05 }] })] },
  { src: 'vrm-cc0/Base_Female.vrm', id: 'char_t', drop: /Tops|Accessory|Bottoms|HAIR/, parts: [{ from: 'vrm-cc0/AvatarSample_D.vrm', take: /HAIR/ }],   // Morgan: black bob, navy blazer, grey slacks
    shells: [TOP('blazer', [44, 52, 82], { push: .012, details: [{ kind: 'open', w: .045, taper: true, color: [238, 238, 240] }, { kind: 'cuffs', w: .02 }] }),
      BOTTOM('slacks', [104, 106, 112], { pattern: 'heather' })] },
  { src: 'vrm-cc0/Base_Female.vrm', id: 'char_x', drop: /FACE|EYE|Face_00|Tops|Accessory|Bottoms|HAIR/,   // Lim: black pixie cut, fire inspector's uniform
    parts: [{ from: 'vrm-cc0/HairSample_Female.vrm', take: /FACE|EYE|Face_00/ }, { from: 'vrm-cc0/HairSample_Male.vrm', take: /HAIR/, scale: .96 }],
    retex: [{ mat: /HAIR/, tint: [36, 32, 36] }, { mat: /SKIN/, sat: 2.3, val: .66 }],
    shells: [TOP('uniform', [46, 56, 80], { short: true, details: [{ kind: 'placket', color: [200, 170, 80] }, { kind: 'patch', x: .07, y: 1.33, r: .022, color: [210, 176, 70] }, { kind: 'sleeveBand', x0: .2, x1: .26, color: [170, 40, 36] }, { kind: 'hem' }] }),
      BOTTOM('slacks', [40, 46, 66], { details: [{ kind: 'hem', h: .02 }] })] },
  { src: 'vrm-cc0/AvatarSample_G.vrm', id: 'char_y', drop: /Onepice|HAIR/, parts: [{ from: 'vrm-cc0/AvatarSample_F.vrm', take: /HAIR/ }],   // High Score: silver hair, track jacket, joggers
    shells: [TOP('track', [40, 150, 160], { push: .011, details: [{ kind: 'zip', color: [240, 240, 240] }, { kind: 'sideStripe', arm: true, color: [240, 240, 240] }, { kind: 'hem', h: .045 }, { kind: 'cuffs' }] }),
      BOTTOM('joggers', [36, 38, 46], { pattern: 'heather', details: [{ kind: 'hem', h: .05 }] })] },
  { src: 'vrm-cc0/Base_Female.vrm', id: 'char_z', drop: /FACE|EYE|Face_00|Tops|Accessory|Bottoms|HAIR/,   // Bex: auburn side tail, tee, baker's apron, jeans
    parts: [{ from: 'vrm/shino_light.vrm', take: /FACE|EYE|Face_00/ }, { from: 'vrm-cc0/AvatarSample_G.vrm', take: /HAIR/ }],
    retex: [{ mat: /HAIR/, tint: [168, 74, 46] }, { mat: /SKIN/, sat: 1.5, val: .9 }],
    shells: [TOP('tee', [238, 238, 234], { short: true, details: [{ kind: 'apron', hw: .11, y0: .55, y1: 1.3, color: [128, 90, 58], pocket: true, straps: true, ties: true }, { kind: 'hem' }] }),
      BOTTOM('jeans', [64, 80, 116], { pattern: 'denim', details: [{ kind: 'apron', hw: .11, y0: .55, y1: 1.3, color: [128, 90, 58], pocket: true }] })] },
  { src: 'vrm-cc0/AvatarSample_D.vrm', id: 'char_a2', drop: /Tops|Accessory|Bottoms|HAIR/, parts: [{ from: 'vrm/shino_light.vrm', take: /HAIR/ }],   // Intern: long black hair, shirt and tie, charcoal slacks
    shells: [TOP('shirt', [240, 240, 242], { push: .008, details: [{ kind: 'open', w: .016, color: [40, 60, 110] }, { kind: 'placket' }, { kind: 'cuffs', w: .02 }] }),
      BOTTOM('slacks', [56, 58, 64], { pattern: 'heather' })] },
  { src: 'vrm-cc0/HairSample_Male.vrm', id: 'char_b2', drop: /Tops|Bottoms/, girth: .3, paint: { stubble: .6, stubbleColor: [120, 60, 30] },   // Dobbs: ginger mop, flannel, jeans
    retex: [{ mat: /HAIR|FaceBrow/, tint: [176, 92, 44] }],
    shells: [TOP('flannel', [170, 40, 36], { pattern: 'plaid', c2: [40, 30, 30], c3: [220, 200, 160], push: .01, details: [{ kind: 'placket', color: [60, 40, 40] }, { kind: 'cuffs', w: .03 }] }),
      BOTTOM('jeans', [58, 76, 112], { pattern: 'denim', details: [{ kind: 'pockets' }] })] },
  { src: 'vrm-cc0/Base_Male.vrm', id: 'char_c2', drop: /Tops|Accessory|Bottoms/, girth: .35,   // Cal: copper hair, canvas work jacket, work pants
    retex: [{ mat: /HAIR|FaceBrow/, tint: [184, 96, 52] }, { mat: /SKIN/, sat: 1.6, val: .86 }],
    shells: [TOP('workjacket', [168, 130, 80], { pattern: 'heather', push: .013, details: [{ kind: 'zip', color: [90, 70, 50] }, { kind: 'collar', color: [96, 62, 40], h: .04, w: .2 }, { kind: 'hem', h: .05 }, { kind: 'cuffs', w: .04 }] }),
      BOTTOM('workpants', [86, 72, 56], { pattern: 'heather', details: [{ kind: 'hem', h: .03 }] })] },
  { src: 'vrm/fumiriya.vrm', id: 'char_d2', drop: /Tops|Accessory|Bottoms|HAIR/, parts: [{ from: 'vrm-cc0/Base_Male.vrm', take: /HAIR/ }],   // Scrapper: buzz cut, charcoal tee, cargo pants
    retex: [{ mat: /HAIR/, tint: [34, 30, 30] }, { mat: /SKIN/, sat: 2.2, val: .6 }],
    shells: [TOP('tee', [70, 72, 78], { short: true, pattern: 'heather', details: [{ kind: 'hem' }] }),
      BOTTOM('cargo', [120, 116, 84], { pattern: 'heather', details: [{ kind: 'patch', x: .14, y: .62, r: .04, color: [104, 100, 72] }, { kind: 'hem', h: .03 }] })] },
  { src: 'vrm-cc0/HairSample_Male.vrm', id: 'char_e2', drop: /Tops|Bottoms|HAIR/, parts: [{ from: 'vrm/shino_light.vrm', take: /HAIR/ }],   // Vinyl Vic: long hair, open black jacket over a red tee, black jeans
    shells: [TOP('jacket', [34, 32, 36], { push: .013, details: [{ kind: 'open', w: .07, taper: true, color: [180, 40, 60] }, { kind: 'zip', color: [140, 140, 150] }, { kind: 'cuffs', w: .03, color: [60, 60, 66] }] }),
      BOTTOM('jeans', [36, 36, 42], { pattern: 'denim', details: [{ kind: 'pockets' }] })] },
  { src: 'vrm-cc0/Base_Male.vrm', id: 'char_f2', keep: .38, drop: /HAIR/, parts: [{ from: 'vrm/fumiriya.vrm', take: /HAIR/ }], glasses: true,   // Wes: brown hair, glasses, tan sweater vest, khakis
    retex: [{ mat: /HAIR/, tint: [110, 78, 52] }, { mat: /Tops/, key: [40, 44, 70], tol: 70, to: [176, 150, 104] }, { mat: /Bottoms/, tint: [176, 160, 120] }, { mat: /SKIN/, sat: 1.4, val: .92 }] },
  { src: 'vrm-cc0/Base_Male.vrm', id: 'char_g2', drop: /Tops|Accessory|Bottoms|HAIR/, parts: [{ from: 'vrm/yyc_D.vrm', take: /HAIR/ }],   // Brant: swept blond hair, blazer over an open collar, grey slacks
    retex: [{ mat: /HAIR/, tint: [222, 190, 120] }],
    shells: [TOP('blazer', [64, 70, 92], { push: .013, details: [{ kind: 'open', w: .05, taper: true, color: [240, 240, 236] }, { kind: 'cuffs', w: .02, color: [236, 236, 232] }] }),
      BOTTOM('slacks', [92, 94, 102], { details: [{ kind: 'hem', h: .02 }] })] },
  // background people: four more bodies for the crowds
  { src: 'vrm-cc0/Base_Male.vrm', id: 'char_h2', bodyAtlas: 512, faceAtlas: 512, drop: /Tops|Accessory|Bottoms|HAIR/, parts: [{ from: 'vrm-cc0/HairSample_Male.vrm', take: /HAIR/ }],
    retex: [{ mat: /HAIR/, tint: [30, 28, 30] }, { mat: /SKIN/, sat: 2.6, val: .5 }],
    shells: [TOP('tee', [220, 200, 80], { short: true, details: [{ kind: 'hem' }] }), BOTTOM('jeans', [50, 60, 90], { pattern: 'denim', details: [{ kind: 'pockets' }] })] },
  { src: 'vrm-cc0/Base_Female.vrm', id: 'char_i2', bodyAtlas: 512, faceAtlas: 512, drop: /Tops|Accessory|Bottoms|HAIR/, parts: [{ from: 'vrm-cc0/AvatarSample_G.vrm', take: /HAIR/ }],
    retex: [{ mat: /HAIR/, tint: [44, 34, 30] }, { mat: /SKIN/, sat: 2, val: .74 }],
    shells: [TOP('blouse', [90, 150, 140], { pattern: 'stripe', c2: [240, 236, 226], period: .04, duty: .35, details: [{ kind: 'hem' }] }), BOTTOM('jeans', [44, 50, 70], { pattern: 'denim' })] },
  { src: 'vrm/fumiriya.vrm', id: 'char_j2', keep: .38, bodyAtlas: 512, faceAtlas: 512, drop: /Tops|Accessory|Bottoms/,
    retex: [{ mat: /HAIR/, tint: [60, 46, 40] }, { mat: /SKIN/, sat: 1.7, val: .82 }],
    shells: [TOP('hoodie', [120, 60, 70], { pattern: 'heather', push: .012, details: [{ kind: 'hem', h: .05 }, { kind: 'cuffs', w: .045 }] }), BOTTOM('joggers', [80, 84, 90], { pattern: 'heather', details: [{ kind: 'hem', h: .05 }] })] },
  { src: 'vrm-cc0/HairSample_Female.vrm', id: 'char_k2', bodyAtlas: 512, faceAtlas: 512, drop: /Tops|HAIR_02/, skipPrims: { mesh: /Hair/, prims: [0, 1, 3, 4] },
    retex: [{ mat: /HAIR/, tint: [92, 52, 36] }, { mat: /SKIN/, sat: 1.6, val: .84 }],
    shells: [TOP('sweater', [200, 120, 60], { pattern: 'knit', push: .01, details: [{ kind: 'hem', h: .04 }, { kind: 'cuffs' }] }), BOTTOM('leggings', [40, 40, 48], { push: .004 })] },
  { src: 'vrm-cc0/Base_Male.vrm', id: 'char_l2', keep: .38, bodyAtlas: 512, faceAtlas: 512, drop: /Tops|Accessory|Bottoms|HAIR/, parts: [{ from: 'vrm/fumiriya.vrm', take: /HAIR/ }],
    paint: { age: 1, grey: 1 }, glasses: true, girth: .2,
    shells: [TOP('cardigan', [96, 110, 84], { pattern: 'knit', push: .011, details: [{ kind: 'open', w: .04, color: [210, 200, 180], buttons: true }, { kind: 'cuffs' }] }), BOTTOM('slacks', [70, 64, 60], { pattern: 'heather' })] },
  { src: 'vrm-cc0/Base_Female.vrm', id: 'char_m2', bodyAtlas: 512, faceAtlas: 512, drop: /Tops|Accessory|Bottoms|HAIR/, parts: [{ from: 'vrm-cc0/AvatarSample_D.vrm', take: /HAIR/ }],
    paint: { age: .9, grey: 1 }, retex: [{ mat: /SKIN/, sat: 1.8, val: .8 }],
    shells: [TOP('tunic', [170, 70, 80], { pattern: 'heather', details: [{ kind: 'hem' }, { kind: 'cuffs' }] }), BOTTOM('slacks', [50, 50, 60])] },
  { src: 'vrm-cc0/HairSample_Male.vrm', id: 'char_n2', bodyAtlas: 512, faceAtlas: 512, drop: /Tops|Bottoms|HAIR/, parts: [{ from: 'vrm-cc0/Base_Male.vrm', take: /HAIR/ }], girth: .45,
    paint: { stubble: .5, stubbleColor: [40, 34, 34] }, retex: [{ mat: /HAIR|FaceBrow/, tint: [60, 50, 44] }, { mat: /SKIN/, sat: 1.9, val: .74 }],
    shells: [TOP('workshirt', [90, 110, 140], { short: true, details: [{ kind: 'placket', color: [220, 220, 210] }, { kind: 'band', y0: 1.22, y1: 1.27, color: [220, 230, 70] }, { kind: 'patch', x: .08, y: 1.35, r: .02, color: [230, 230, 220] }] }),
      BOTTOM('workpants', [60, 62, 66], { pattern: 'heather', details: [{ kind: 'band', y0: .3, y1: .34, color: [220, 230, 70] }] })] },
  { src: 'vrm-cc0/AvatarSample_G.vrm', id: 'char_o2', bodyAtlas: 512, faceAtlas: 512, drop: /Onepice|HAIR/, parts: [{ from: 'vrm-cc0/AvatarSample_D.vrm', take: /HAIR/ }],
    retex: [{ mat: /HAIR/, tint: [150, 96, 60] }],
    shells: [TOP('denimjacket', [90, 120, 170], { pattern: 'denim', push: .012, details: [{ kind: 'open', w: .06, taper: true, color: [230, 220, 200] }, { kind: 'cuffs' }, { kind: 'hem', h: .04 }] }), BOTTOM('chinos', [180, 160, 120], { pattern: 'heather' })] },
  { src: 'vrm-cc0/Base_Male.vrm', id: 'char_p2', bodyAtlas: 512, faceAtlas: 512, drop: /Tops|Accessory|Bottoms|HAIR/, parts: [{ from: 'vrm/yyc_D.vrm', take: /HAIR/ }],
    retex: [{ mat: /HAIR/, tint: [40, 34, 30] }, { mat: /SKIN/, sat: 1.4, val: .9 }],
    shells: [TOP('polo', [60, 130, 110], { short: true, details: [{ kind: 'collar', color: [50, 110, 94], w: .09 }, { kind: 'placket', color: [240, 240, 240] }] }), BOTTOM('shorts', [140, 130, 110], { shorts: true, details: [{ kind: 'hem' }] })] },
];
const FAR_TRIS = 4200;
// every skin is padded to this many joints: the renderer bakes the bone count into the program, so one
// count means one set of programs for the whole cast (no program link when a new body walks on)
const SKIN_JOINTS = 192;
// the face's vertex count is baked into the morph program too: every face is padded to this many
const FACE_VERTS = 3456;      // the far body (body and face together), no hull
const BODY_KEEP = 0.5;      // share of body triangles kept by the simplifier
const OUTLINE = 0.0052;     // hull push in metres
// the expression morphs, in the order actor.c reads them
const EXPR = ['blink', 'a', 'i', 'u', 'e', 'o', 'joy', 'angry', 'sorrow', 'fun'];
const CLIPS = `Idle_Loop Idle_Talking_Loop Idle_FoldArms_Loop Idle_No_Loop Idle_TalkingPhone_Loop Walk_Loop Walk_Formal_Loop
 Jog_Fwd_Loop Sprint_Loop Jump_Start Jump_Loop Jump_Land NinjaJump_Start NinjaJump_Idle_Loop NinjaJump_Land Roll Slide_Start Slide_Loop Slide_Exit
 Hit_Chest Hit_Head Hit_Knockback Death01 LayToIdle Sword_Idle Sword_Regular_A Sword_Regular_A_Rec Sword_Regular_B Sword_Regular_B_Rec
 Sword_Regular_C Sword_Regular_Combo Sword_Heavy_Combo Sword_Dash Sword_Block Sword_Attack Idle_Shield_Loop Shield_Dash Shield_OneShot Idle_Shield_Break
 Spell_Simple_Enter Spell_Simple_Idle_Loop Spell_Simple_Shoot Spell_Simple_Exit Punch_Jab Punch_Cross Melee_Hook Melee_Hook_Rec OverhandThrow
 Sitting_Enter Sitting_Exit Sitting_Idle_Loop Sitting_Talking_Loop Driving_Loop Interact Fixing_Kneeling PickUp_Table Walk_Carry_Loop Yes Dance_Loop Consume
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
// paint on the face skin texture (VRoid faces share one layout: eyes at y .53, nose .64, mouth .75):
// age lines under the eyes, at the nose and on the brow, and stubble round the mouth and jaw
function paintFace(raw, w, h, o) {
  const blend = (x, y, rgb, a) => { x |= 0; y |= 0; if (x < 0 || y < 0 || x >= w || y >= h || a <= 0) return; const p = (y * w + x) * 4; for (let k = 0; k < 3; k++) raw[p + k] = raw[p + k] * (1 - a) + rgb[k] * a; };
  const tone = (x, y, f, a) => { x |= 0; y |= 0; if (x < 0 || y < 0 || x >= w || y >= h) return; const p = (y * w + x) * 4; blend(x, y, [raw[p] * f, raw[p + 1] * f * .96, raw[p + 2] * f * .94], a); };
  // a soft stroke along a quadratic curve, in texture fractions
  const stroke = (p0, p1, p2, width, f, a) => {
    for (let t = 0; t <= 1; t += 1 / (w * .6)) {
      const x = ((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0]) * w, y = ((1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1]) * h;
      const taper = Math.sin(Math.PI * t), r = width * w;
      for (let dy = -r; dy <= r; dy++) for (let dx = -r; dx <= r; dx++) { const d = Math.hypot(dx, dy) / r; if (d <= 1) tone(x + dx, y + dy, f, a * taper * (1 - d) * .5); }
    }
  };
  if (o.age) {
    const a = o.age;
    for (const s of [1, -1]) {
      const X = (x) => .5 + s * (x - .5);
      stroke([X(.27), .585], [X(.34), .625], [X(.42), .59], .004, .72, a);          // under the eye
      stroke([X(.26), .615], [X(.335), .655], [X(.41), .62], .003, .8, a * .7);
      stroke([X(.445), .675], [X(.405), .73], [X(.415), .79], .005, .7, a);         // nose to mouth
      stroke([X(.235), .5], [X(.215), .53], [X(.235), .565], .003, .78, a * .8);     // crow's feet
    }
    stroke([.38, .405], [.5, .39], [.62, .405], .003, .82, a * .7);                  // brow
    stroke([.4, .435], [.5, .422], [.6, .435], .003, .82, a * .55);
  }
  if (o.stubble) {
    // a speckle in the hair colour over the lip, chin and jaw, thinning at the edges
    const col = o.stubbleColor || [70, 52, 44];
    let seed = 7; const rnd = () => ((seed = (seed * 16807) % 2147483647) / 2147483647);
    for (let y = (h * .69) | 0; y < h * .97; y++) for (let x = (w * .1) | 0; x < w * .9; x++) {
      const u = x / w - .5, v = y / h;
      const jaw = Math.hypot(u / .36, (v - .74) / .2);                  // the lower face
      const mouth = Math.hypot(u / .085, (v - .752) / .016);            // keep the lips clear
      if (jaw > 1 || mouth < 1) continue;
      const edge = Math.min(1, (1 - jaw) * 4) * Math.min(1, (mouth - 1) * 3) * (v < .72 ? Math.max(0, 1 - Math.abs(u) / .12) : 1);
      if (rnd() < .55) blend(x, y, col, o.stubble * edge * (.5 + .5 * rnd()));
    }
  }
}

// a material's base colour as raw RGBA: its texture times its factor, or the raw a shell painted
async function rawOf(src) {
  if (src.raw) return { raw: Buffer.from(src.raw), w: src.w, h: src.h };
  const mat = src.mat, ti = mat.pbrMetallicRoughness.baseColorTexture?.index;
  const fac = mat.pbrMetallicRoughness.baseColorFactor || [1, 1, 1, 1];
  let raw, w, h;
  if (ti === undefined) { w = h = 4; raw = Buffer.alloc(64, 255); }
  else {
    const img = src.J.images[src.J.textures[ti].source];
    const r = await sharp(src.view(img.bufferView)).ensureAlpha().raw().toBuffer({ resolveWithObject: true });
    raw = r.data; w = r.info.width; h = r.info.height;
  }
  for (let p = 0; p < raw.length; p += 4) for (let k = 0; k < 3; k++) raw[p + k] = Math.min(255, raw[p + k] * fac[k]);
  return { raw, w, h };
}
// the mean colour of the middle of a face skin texture (cheeks and forehead, no eyes or mouth)
function skinTone(raw, w, h) {
  const s = [0, 0, 0]; let n = 0;
  for (let y = (h * .42) | 0; y < h * .5; y++) for (let x = (w * .3) | 0; x < w * .7; x++) { const p = (y * w + x) * 4; if (raw[p + 3] < 200) continue; for (let k = 0; k < 3; k++) s[k] += raw[p + k]; n++; }
  return s.map((v) => v / Math.max(1, n));
}
function rgb2hsv(r, g, b) { const mx = Math.max(r, g, b), mn = Math.min(r, g, b), d = mx - mn; let h = 0; if (d) h = mx === r ? ((g - b) / d) % 6 : mx === g ? (b - r) / d + 2 : (r - g) / d + 4; return [h * 60, mx ? d / mx : 0, mx]; }
function hsv2rgb(h, s, v) { h = ((h % 360) + 360) % 360; const c = v * s, x = c * (1 - Math.abs(((h / 60) % 2) - 1)), m = v - c; const [r, g, b] = h < 60 ? [c, x, 0] : h < 120 ? [x, c, 0] : h < 180 ? [0, c, x] : h < 240 ? [0, x, c] : h < 300 ? [x, 0, c] : [c, 0, x]; return [r + m, g + m, b + m]; }
// bake time recolours of a source material (entry.retex): { mat, hue, sat, val } turns the whole texture;
// with key: [r, g, b] and tol only the pixels near that colour change, to: [r, g, b] (their shading kept)
function retexture(raw, name, ops) {
  for (const o of ops || []) {
    if (!o.mat.test(name)) continue;
    const kh = o.key && rgb2hsv(...o.key), th = o.to && rgb2hsv(...o.to);
    if (o.tint) {   // the whole texture in one colour, its shading kept: each pixel's value against the mean
      let sum = 0, n = 0; for (let p = 0; p < raw.length; p += 4) if (raw[p + 3] > 115) { sum += Math.max(raw[p], raw[p + 1], raw[p + 2]); n++; }
      const mean = sum / Math.max(1, n) || 1, tc = rgb2hsv(...o.tint);
      for (let p = 0; p < raw.length; p += 4) { const v = Math.max(raw[p], raw[p + 1], raw[p + 2]) / mean; const out = hsv2rgb(tc[0], tc[1], Math.min(255, tc[2] * Math.pow(v, o.contrast || 1))); for (let k = 0; k < 3; k++) raw[p + k] = out[k]; }
      continue;
    }
    for (let p = 0; p < raw.length; p += 4) {
      const [h, s, v] = rgb2hsv(raw[p], raw[p + 1], raw[p + 2]);
      let out;
      if (o.key) {
        const d = Math.hypot(raw[p] - o.key[0], raw[p + 1] - o.key[1], raw[p + 2] - o.key[2]);
        if (d > (o.tol || 40)) continue;
        out = hsv2rgb(th[0], th[1], Math.min(255, th[2] * (kh[2] ? v / kh[2] : 1)));
      } else out = hsv2rgb(h + (o.hue || 0), Math.min(1, s * (o.sat ?? 1)), Math.min(255, v * (o.val ?? 1)));
      for (let k = 0; k < 3; k++) raw[p + k] = Math.max(0, Math.min(255, out[k]));
    }
  }
}

async function atlas(M, MATS, size, name, paint, retex) {
  const used = [...new Set(M.mat)];
  const items = [];
  for (const m of used) {
    const src = MATS[m], mat = src.mat;
    const { raw, w, h } = await rawOf(src);
    // a face from another sample: its skin brought to the base's tone, so it meets the neck
    if (src.tone && /Face_00/.test(mat.name)) { const t = skinTone(raw, w, h); for (let p = 0; p < raw.length; p += 4) for (let k = 0; k < 3; k++) raw[p + k] = Math.min(255, raw[p + k] * src.tone[k] / Math.max(1, t[k])); }
    retexture(raw, mat.name, retex);
    if (paint && /Face_00/.test(mat.name)) paintFace(raw, w, h, paint);
    // grey hair (and brows): the luminance lifted toward a silver, a trace of the old hue left
    if (paint?.grey && /hair|FaceBrow/i.test(mat.name)) for (let p = 0; p < raw.length; p += 4) {
      const L = .3 * raw[p] + .59 * raw[p + 1] + .11 * raw[p + 2], g = 70 + L * .75 * paint.grey + (1 - paint.grey) * L * .2;
      for (let k = 0; k < 3; k++) raw[p + k] = Math.min(255, g * (k === 2 ? 1.03 : 1) * .85 + raw[p + k] * .15);
    }
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
  remapLike(M, rects, size);
  // a 256 colour PNG: cel shaded art survives it, and it is a third of the size (stb_image reads it)
  const png = await sharp(out, { raw: { width: size, height: size, channels: 4 } }).png({ compressionLevel: 9, palette: true, colours: 256, dither: 0.6, effort: 10 }).toBuffer();
  console.log(`  atlas ${name} ${size}: ${used.length} mats scale ${scale.toFixed(2)} ${(png.length / 1024) | 0} KB`);
  return { png, size, place: rects, rects: Object.values(rects).map((p) => ({ name: p.it.name, x: p.x, y: p.y, w: p.w, h: p.h })) };
}
// move a mesh's uvs into the atlas rects of its materials
function remapLike(M, rects, size) {
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
}

class GlbOut {
  constructor() { this.parts = []; this.len = 0; this.J = { asset: { version: '2.0', generator: 'keyholder prep-vrm' }, buffers: [{}], bufferViews: [], accessors: [], images: [], samplers: [{ magFilter: 9729, minFilter: 9729, wrapS: 33071, wrapT: 33071 }], textures: [], materials: [], meshes: [], nodes: [], skins: [], scenes: [{ nodes: [] }], scene: 0 }; }
  blob(buf) { const b = Buffer.from(buf.buffer ? new Uint8Array(buf.buffer, buf.byteOffset, buf.byteLength) : buf); const pad = (4 - (this.len % 4)) % 4; if (pad) { this.parts.push(Buffer.alloc(pad)); this.len += pad; } this.parts.push(b); const o = this.len; this.len += b.length; this.J.bufferViews.push({ buffer: 0, byteOffset: o, byteLength: b.length }); return this.J.bufferViews.length - 1; }
  acc(arr, type, comp, bounds, norm) { const v = this.blob(arr); const c = { SCALAR: 1, VEC2: 2, VEC3: 3, VEC4: 4, MAT4: 16 }[type]; const a = { bufferView: v, componentType: comp, count: arr.length / c, type }; if (norm) a.normalized = true; if (bounds) { const mn = Array(c).fill(1e30), mx = Array(c).fill(-1e30); for (let i = 0; i < arr.length; i++) { const k = i % c; if (arr[i] < mn[k]) mn[k] = arr[i]; if (arr[i] > mx[k]) mx[k] = arr[i]; } a.min = mn; a.max = mx; } this.J.accessors.push(a); return this.J.accessors.length - 1; }
  tex(png) { const v = this.blob(png); this.J.images.push({ bufferView: v, mimeType: 'image/png' }); this.J.textures.push({ source: this.J.images.length - 1, sampler: 0 }); return this.J.textures.length - 1; }
  write(path) { const js = Buffer.from(JSON.stringify((this.J.buffers[0].byteLength = this.len, this.J))); const jp = (4 - (js.length % 4)) % 4; const j = Buffer.concat([js, Buffer.alloc(jp, 32)]); const bp = (4 - (this.len % 4)) % 4; const b = Buffer.concat([...this.parts, Buffer.alloc(bp)]); const h = Buffer.alloc(20); h.writeUInt32LE(0x46546c67, 0); h.writeUInt32LE(2, 4); h.writeUInt32LE(12 + 8 + j.length + 8 + b.length, 8); h.writeUInt32LE(j.length, 12); h.writeUInt32LE(0x4e4f534a, 16); const bh = Buffer.alloc(8); bh.writeUInt32LE(b.length, 0); bh.writeUInt32LE(0x004e4942, 4); writeFileSync(path, Buffer.concat([h, j, bh, b])); }
}

const metas = [];
// simplify material by material so a jacket cannot eat a shoe; keep is the share of triangles kept
function simplifyByMat(S, keep, err, label) {
  const kept = [], matKept = [];
  for (const m of new Set(S.mat)) {
    const ids = []; for (let t = 0; t < S.mat.length; t++) if (S.mat[t] === m) ids.push(S.idx[t * 3], S.idx[t * 3 + 1], S.idx[t * 3 + 2]);
    let out = Uint32Array.from(ids);
    if (ids.length > 360 && keep < 1) {
      // a private vertex list per material: the simplifier's error is relative to what it is given
      const map = new Map(), lp = [];
      const li = Uint32Array.from(ids, (v) => { if (!map.has(v)) { map.set(v, map.size); lp.push(S.pos[v * 3], S.pos[v * 3 + 1], S.pos[v * 3 + 2]); } return map.get(v); });
      const back = [...map.keys()];
      const [sm] = MeshoptSimplifier.simplify(li, Float32Array.from(lp), 3, Math.max(120, Math.floor(ids.length * keep / 3)) * 3, err, []);
      if (label) console.log('   ', label, String(m).padStart(3), ids.length / 3, '->', sm.length / 3);
      if (sm.length) out = Uint32Array.from(sm, (v) => back[v]);
    }
    kept.push(...out); for (let t = 0; t < out.length / 3; t++) matKept.push(m);
  }
  return compact({ ...S, idx: Uint32Array.from(kept), mat: Int32Array.from(matKept) });
}
// an inverted hull: the meshes' triangles (all, or those the filter keeps) pushed out along the normals and flipped
function makeHull(parts, push) {
  let nv = 0; for (const [S] of parts) nv += S.nv;
  const hp = new Float32Array(nv * 3), hn = new Float32Array(nv * 3), hj = new Uint16Array(nv * 4), hw = new Float32Array(nv * 4), idx = [];
  let o = 0;
  for (const [S, keepTri] of parts) {
    for (let i = 0; i < S.nv; i++) { for (let k = 0; k < 3; k++) { hp[(o + i) * 3 + k] = S.pos[i * 3 + k] + S.nor[i * 3 + k] * push; hn[(o + i) * 3 + k] = S.nor[i * 3 + k]; } for (let k = 0; k < 4; k++) { hj[(o + i) * 4 + k] = S.jnt[i * 4 + k]; hw[(o + i) * 4 + k] = S.wgt[i * 4 + k]; } }
    for (let t = 0; t < S.idx.length / 3; t++) if (!keepTri || keepTri(S.mat[t])) idx.push(S.idx[t * 3] + o, S.idx[t * 3 + 2] + o, S.idx[t * 3 + 1] + o);
    o += S.nv;
  }
  return compact({ nv, pos: hp, nor: hn, uv: new Float32Array(nv * 2), jnt: hj, wgt: hw, idx: Uint32Array.from(idx), mat: new Int32Array(idx.length / 3), morph: [] });
}

// glasses as inked lines: two rounded frames, a bridge and the arms, in the outline's dark flat colour,
// both sides, on the head joint. eyes are the eye bones (canonical frame), face the face mesh
function glassesMesh(face, isWhite, head, o = {}) {
  const pos = [], idx = [], ring = 32, th = o.t || .0026, m = o.m || .006;
  // each lens round its eye white, from the face mesh itself (the eye bones are the eyeballs' pivots, deep and close)
  const box = [[1e9, 1e9, -1e9, -1e9], [1e9, 1e9, -1e9, -1e9]];
  for (let t = 0; t < face.idx.length / 3; t++) if (isWhite(face.mat[t])) for (let c = 0; c < 3; c++) {
    const v = face.idx[t * 3 + c], x = face.pos[v * 3], y = face.pos[v * 3 + 1], B = box[x < 0 ? 0 : 1];
    B[0] = Math.min(B[0], x); B[1] = Math.min(B[1], y); B[2] = Math.max(B[2], x); B[3] = Math.max(B[3], y);
  }
  const lens = box.map((B) => ({ cx: (B[0] + B[2]) / 2, cy: (B[1] + B[3]) / 2 + .002, hw: (B[2] - B[0]) / 2 + m, hh: (B[3] - B[1]) / 2 + m * .8 }));
  // the lens plane: just in front of the skin inside the frames
  let zs = -1e9;
  for (let i = 0; i < face.nv; i++) for (const L of lens) if (Math.abs(face.pos[i * 3] - L.cx) < L.hw && Math.abs(face.pos[i * 3 + 1] - L.cy) < L.hh) zs = Math.max(zs, face.pos[i * 3 + 2]);
  const z = zs + (o.gap || .006);
  const quad = (a, b, c, d) => { const n = pos.length / 3; pos.push(...a, ...b, ...c, ...d); idx.push(n, n + 1, n + 2, n, n + 2, n + 3, n, n + 2, n + 1, n, n + 3, n + 2); };
  const rr = (L, t, grow) => { const a = t * Math.PI * 2, c = Math.cos(a), si = Math.sin(a), q = 2 / 5; return [L.cx + Math.sign(c) * Math.abs(c) ** q * (L.hw + grow), L.cy + Math.sign(si) * Math.abs(si) ** q * (L.hh + grow), z]; };
  for (const L of lens) for (let k = 0; k < ring; k++) quad(rr(L, k / ring, 0), rr(L, (k + 1) / ring, 0), rr(L, (k + 1) / ring, th), rr(L, k / ring, th));
  const [l, r] = lens[0].cx < lens[1].cx ? lens : [lens[1], lens[0]], y = (l.cy + r.cy) / 2 + l.hh * .35;
  quad([l.cx + l.hw, y, z], [r.cx - r.hw, y, z], [r.cx - r.hw, y + th, z], [l.cx + l.hw, y + th, z]);   // bridge
  for (const [L, s] of [[l, -1], [r, 1]]) { const x = L.cx + s * (L.hw + th), yy = L.cy + L.hh * .4; quad([x, yy, z], [x + s * .008, yy, z - .085], [x + s * .008, yy + th, z - .085], [x, yy + th, z]); }   // arms to the ears
  console.log(`  glasses: lenses ${lens.map((L) => `${L.cx.toFixed(3)},${L.cy.toFixed(3)} ${(L.hw * 2).toFixed(3)}x${(L.hh * 2).toFixed(3)}`).join(' ')} at z ${z.toFixed(3)}`);
  const nv = pos.length / 3, jnt = new Uint16Array(nv * 4), wgt = new Float32Array(nv * 4);
  for (let i = 0; i < nv; i++) { jnt[i * 4] = head; wgt[i * 4] = 1; }
  return { nv, pos: Float32Array.from(pos), nor: new Float32Array(nv * 3), uv: new Float32Array(nv * 2), jnt, wgt, idx: Uint32Array.from(idx), mat: new Int32Array(idx.length / 3), morph: [] };
}

// ---- garments made at bake time: "shells", the body skin of some bones pushed out along the normals
// and painted. A shell replaces the skin under it (so a lower level of detail cannot poke through) and
// takes a new material named Shell_Tops_* or Shell_Bottoms_* (the palette swap finds it by that name).
// Its uvs are a front projection: the left half of the texture is the front, the right half the back.
//   { name, bones: [humanoid slots], push (m), tex: [w, h], color, pattern, c2, c3, details: [...] , yMinH, yMaxH }
// yMinH and yMaxH limit it by height as a share of the hip height (0 the floor, 1 the hips).
function hash2(x, y) { let h = (x * 374761393 + y * 668265263) | 0; h = (h ^ (h >>> 13)) * 1274126177; return ((h ^ (h >>> 16)) >>> 0) / 4294967295; }
function paintShell(sp, w, h, g) {
  const raw = Buffer.alloc(w * h * 4);
  const C = sp.color, C2 = sp.c2 || C.map((v) => v * .7), C3 = sp.c3 || C.map((v) => Math.min(255, v * 1.25));
  const mix = (a, b, t) => a.map((v, k) => v + (b[k] - v) * t);
  for (let py = 0; py < h; py++) for (let px = 0; px < w; px++) {
    const side = px < w / 2 ? 0 : 1, uh = side ? 1 - (px - w / 2) / (w / 2) : px / (w / 2);
    const x = (uh - .5) * 2 * g.xmax, y = g.ymax - (py / (h - 1)) * (g.ymax - g.ymin), ax = Math.abs(x);
    let c = C.slice();
    const n = hash2(px, py) - .5;
    switch (sp.pattern) {
      case 'denim': { const tw = ((px + py) % 4) < 2 ? 1.04 : .94; c = c.map((v) => v * tw + n * 22); break; }
      case 'plaid': {   // a tartan: bands of c2 both ways, c3 pinstripes
        const P = sp.period || .09, fx = ((x + 10) / P) % 1, fy = ((y + 10) / P) % 1;
        const bx = fx < .32, by = fy < .32, lx = Math.abs(fx - .66) < .04, ly = Math.abs(fy - .66) < .04;
        if (bx && by) c = mix(C2, [0, 0, 0], .25); else if (bx || by) c = C2.slice();
        if (lx || ly) c = C3.slice();
        c = c.map((v) => v + n * 10); break; }
      case 'stripe': { const P = sp.period || .06; if (((y + 10) / P) % 1 < (sp.duty || .5)) c = C2.slice(); c = c.map((v) => v + n * 6); break; }
      case 'knit': { const f = Math.sin(px * Math.PI / 2.5) * .5 + .5; c = c.map((v) => v * (.93 + .07 * f) + n * 8); break; }
      case 'heather': c = c.map((v) => v + n * 26); break;
      default: c = c.map((v) => v + n * 5);
    }
    for (const d of sp.details || []) {
      switch (d.kind) {
        case 'open':   // an open front (cardigan, blazer, jacket): what is under it down the middle
          if (!side && ax < d.w * (d.taper ? Math.max(.25, Math.min(1, (y - g.ymin) / (g.ymax - g.ymin) * 1.3)) : 1) && y > g.ymin + (d.from || 0)) {
            c = d.color.slice();
            if (d.tie && ax < .016 && y > g.ymax - (d.tieLen || .3)) c = d.tie.slice();
            if (d.buttons && ax < .004) c = mix(c, [40, 40, 40], .5);
          }
          if (!side && Math.abs(ax - d.w) < .006 && y > g.ymin) c = mix(c, [0, 0, 0], .35);   // lapel edge
          break;
        case 'placket':   // a button line down the front
          if (!side && ax < .012 && y > g.ymin + .02) { c = mix(c, [0, 0, 0], .15); if (((g.ymax - y) / .075) % 1 < .18 && ax < .007) c = d.color || [235, 230, 220]; }
          break;
        case 'zip': if (!side && ax < .005 && y > g.ymin) c = d.color || [190, 190, 196]; break;
        case 'collar': if (y > g.ymax - (d.h || .035) && ax < (d.w || .11)) c = d.color.slice(); break;
        case 'band':   // a horizontal band at heights given as shares of the hip height
          if (y > d.y0 * g.hipsY && y < d.y1 * g.hipsY && (d.side === undefined || d.side === side)) c = d.color.slice(); break;
        case 'hem': if (y < g.ymin + (d.h || .035)) c = mix(c, d.color || [0, 0, 0], d.color ? 1 : .3); break;
        case 'cuffs': if (ax > g.xmax - (d.w || .05)) c = mix(c, d.color || [0, 0, 0], d.color ? 1 : .3); break;
        case 'sleeveBand': if (ax > d.x0 && ax < d.x1) c = d.color.slice(); break;
        case 'apron':   // a work apron over the front: x half width, y0..y1 shares of the hip height
          if (!side && ax < d.hw && y > d.y0 * g.hipsY && y < d.y1 * g.hipsY) { c = d.color.map((v) => v + n * 8); if (d.pocket && y > (d.y0 + .1) * g.hipsY && y < (d.y0 + .28) * g.hipsY && ax < d.hw * .7 && Math.abs(ax - d.hw * .7) < .004) c = mix(c, [0, 0, 0], .3); }
          if (!side && d.straps && Math.abs(ax - d.hw * .8) < .012 && y >= d.y1 * g.hipsY) c = d.color.slice();
          if (side && d.ties && Math.abs(y - d.y0 * g.hipsY * 1.6) < .012) c = d.color.slice();
          break;
        case 'bib':   // overalls: a bib on the front and two straps, over a shirt
          if (!side && ax < d.hw && y < d.top * g.hipsY) c = d.color.map((v) => v + n * 14);
          if (Math.abs(ax - d.hw * .75) < .016 && y >= d.top * g.hipsY) c = d.color.slice();
          break;
        case 'patch': if (!side && Math.abs(x - d.x) < d.r && Math.abs(y - d.y * g.hipsY) < d.r) c = d.color.slice(); break;
        case 'pockets':   // jeans: front pocket curves and a fly, back pockets
          if (!side && Math.abs(ax - .1) < .006 && y > g.ymax - .1 && y < g.ymax - .02) c = mix(c, [0, 0, 0], .3);
          if (!side && ax < .006 && y > g.ymax - .17) c = mix(c, [0, 0, 0], .25);
          if (side && ax > .03 && ax < .12 && y > g.ymax - .2 && y < g.ymax - .07 && (Math.abs(ax - .03) < .006 || Math.abs(ax - .12) < .006 || Math.abs(y - (g.ymax - .2)) < .006)) c = mix(c, [0, 0, 0], .3);
          if (y > g.ymax - .03) c = mix(c, [0, 0, 0], .12);   // the waistband
          break;
        case 'sideStripe':   // track pants and track jackets: a stripe down the outside of each leg or arm
          if (d.arm ? (Math.abs(y - g.armY) < .012) : (Math.abs(ax - d.x) < .012)) c = d.color.slice(); break;
      }
    }
    const o = (py * w + px) * 4;
    for (let k = 0; k < 3; k++) raw[o + k] = Math.max(0, Math.min(255, c[k]));
    raw[o + 3] = 255;
  }
  return raw;
}

// one source VRM. Each bake reads its own copy: grafts go onto the base's node list
function loadSrc(rel) {
  const { J, acc, view } = readGlb(join(ROOT, 'source', rel));
  const V = J.extensions.VRM;
  const human = {}; for (const b of V.humanoid.humanBones) human[b.node] = b.bone;
  const byBone = {}; for (const [n, b] of Object.entries(human)) byBone[b] = +n;
  const parent = {}; J.nodes.forEach((n, i) => (n.children || []).forEach((c) => (parent[c] = i)));
  // world position of a node (VRoid rests are translation only: checked here)
  const world = (i) => { let x = 0, y = 0, z = 0; for (let n = i; n !== undefined; n = parent[n]) { const t = J.nodes[n].translation || [0, 0, 0], r = J.nodes[n].rotation || [0, 0, 0, 1]; if (Math.abs(r[0]) + Math.abs(r[1]) + Math.abs(r[2]) > 1e-4) throw new Error('rotated rest: ' + J.nodes[n].name); x += t[0]; y += t[1]; z += t[2]; } return [x, y, z]; };
  const root = J.scenes[0].nodes.find((i) => J.nodes[i].name === 'Root');
  return { rel, J, acc, view, V, human, byBone, parent, world, root };
}
const subtree = (J, n, out = []) => { out.push(n); for (const c of J.nodes[n].children || []) subtree(J, c, out); return out; };

async function bake(entry) {
  const file = entry.src.replace(/^.*\//, '').replace(/\.vrm$/, '');
  const B = loadSrc(entry.src), { J, V, human, parent, world } = B;
  // VRM 0 has thumb proximal, intermediate, distal where the clips (VRM 1 names) have metacarpal, proximal, distal
  const v1 = (b) => b.replace('ThumbProximal', 'ThumbMetacarpal').replace('ThumbIntermediate', 'ThumbProximal');
  // bones by humanoid slot; other joints (hair, skirt, bust, grafts) keep a sanitized name
  const nameOf = (i) => (human[i] ? v1(human[i]) : 's' + i);
  // skins are per mesh: unify to one joint list (the base's skins, then any grafted joints)
  const joints = [...new Set(J.skins.flatMap((s) => s.joints))];
  const jointIndex = new Map(joints.map((j, i) => [j, i]));
  const jointOf = (n) => { if (!jointIndex.has(n)) { jointIndex.set(n, joints.length); joints.push(n); } return jointIndex.get(n); };
  // every material any part uses, from whichever file it came from
  const MATS = [], matIds = new Map();
  const matOf = (S, m, extra) => { const k = S.rel + '#' + m; if (!matIds.has(k)) { matIds.set(k, MATS.length); MATS.push({ mat: S.J.materials[m], J: S.J, view: S.view, ...extra }); } return matIds.get(k); };
  const mname = (m) => MATS[m].mat.name;

  // the parts: the base without entry.drop, then entry.parts, each { from, take, scale, lift }: the
  // materials of another VRoid sample matching take (its hair, or its whole head), moved from that
  // sample's head to the base's head. Their own bones (hair joints) are grafted onto the base's skeleton.
  const parts = [{ S: B, keep: (n) => !(entry.drop && entry.drop.test(n)), skip: entry.skipPrims }];
  const baseFace = J.materials.findIndex((m) => /Face_00_SKIN/.test(m.name));
  const baseTone = baseFace >= 0 ? rawOf({ mat: J.materials[baseFace], J, view: B.view }) : null;
  for (const p of entry.parts || []) {
    const S = loadSrc(p.from);
    const dh = S.world(S.byBone.head), bh = world(B.byBone.head), k = p.scale || 1, lift = p.lift || 0;
    const X = (q) => [(q[0] - dh[0]) * k + bh[0], (q[1] - dh[1]) * k + bh[1] + lift, (q[2] - dh[2]) * k + bh[2] + (p.fwd || 0)];
    let tone;
    if (baseTone) { const r = await baseTone; tone = skinTone(r.raw, r.w, r.h); }
    parts.push({ S, keep: (n) => p.take.test(n), X, k, tone, skip: p.skipPrims });
  }
  const graft = new Map();
  const mapNode = (P, n) => {
    const S = P.S;
    if (S === B) return n;
    if (S.human[n] !== undefined) { const t = B.byBone[S.human[n]]; if (t === undefined) throw new Error(`${file}: no ${S.human[n]} bone for a graft`); return t; }
    if (n === S.root || S.parent[n] === undefined) return B.root;
    const key = S.rel + '#' + n;
    if (graft.has(key)) return graft.get(key);
    const pn = mapNode(P, S.parent[n]), w = P.X(S.world(n)), pw = world(pn), id = J.nodes.length;
    J.nodes.push({ name: 'graft' + id, translation: [w[0] - pw[0], w[1] - pw[1], w[2] - pw[2]] });
    (J.nodes[pn].children ||= []).push(id); parent[id] = pn;
    graft.set(key, id);
    return id;
  };

  // every primitive of every part
  const facePrims = [], bodyPrims = [];
  for (const P of parts) {
    const S = P.S, SJ = S.J;
    const sJoints = new Set(SJ.skins.flatMap((s) => s.joints));
    const groups = S.V.blendShapeMaster.blendShapeGroups;
    const exprBinds = EXPR.map((e) => groups.find((g) => g.presetName === e)?.binds || []);
    for (const node of SJ.nodes.filter((n) => n.mesh !== undefined)) {
      const ni = SJ.nodes.indexOf(node);
      // an unskinned mesh (a hat on the head bone): rigid on its nearest joint, moved to world space
      let rigid = -1, off = [0, 0, 0];
      if (node.skin === undefined) { let p = ni; while (p !== undefined && !sJoints.has(p)) p = S.parent[p]; if (p === undefined) { console.log('  skip unskinned mesh', node.name); continue; } rigid = p; off = S.world(ni); }
      const srcJoints = node.skin === undefined ? [rigid] : SJ.skins[node.skin].joints;
      let remap = null;   // made on the first kept primitive, so a donor's unused bones are not grafted
      SJ.meshes[node.mesh].primitives.forEach((p, pi) => {
        const mn = SJ.materials[p.material].name;
        if (!P.keep(mn)) return;
        // skipPrims { mesh, prims }: single pieces of a mesh left out (a hair sample's cat ears)
        if (P.skip && P.skip.mesh.test(SJ.meshes[node.mesh].name || '') && P.skip.prims.includes(pi)) return;
        remap ||= srcJoints.map((j) => jointOf(mapNode(P, j)));
        const at = p.attributes, pos = S.acc(at.POSITION), nor = S.acc(at.NORMAL), uv = S.acc(at.TEXCOORD_0);
        const nv0 = pos.length / 3, jn = at.JOINTS_0 !== undefined ? S.acc(at.JOINTS_0) : new Uint16Array(nv0 * 4), wg = at.WEIGHTS_0 !== undefined ? S.acc(at.WEIGHTS_0) : Float32Array.from({ length: nv0 * 4 }, (_, i) => (i % 4 ? 0 : 1));
        if (rigid >= 0) for (let i = 0; i < pos.length; i += 3) { pos[i] += off[0]; pos[i + 1] += off[1]; pos[i + 2] += off[2]; }
        if (P.X) for (let i = 0; i < pos.length; i += 3) { const q = P.X([pos[i], pos[i + 1], pos[i + 2]]); pos[i] = q[0]; pos[i + 1] = q[1]; pos[i + 2] = q[2]; }
        const idx = Uint32Array.from(S.acc(p.indices));
        const jnt = new Uint16Array(jn.length); for (let i = 0; i < jn.length; i++) jnt[i] = remap[node.skin === undefined ? 0 : jn[i]];
        // canonical facing: VRM 0 faces -Z, turn 180 about y
        for (let i = 0; i < pos.length; i += 3) { pos[i] = -pos[i]; pos[i + 2] = -pos[i + 2]; nor[i] = -nor[i]; nor[i + 2] = -nor[i + 2]; }
        let morph = null;
        if (p.targets?.length) {
          const k = P.k || 1;
          morph = EXPR.map((_, e) => {
            const d = new Float32Array(pos.length); let any = false;
            for (const b of exprBinds[e]) {
              if (b.mesh !== node.mesh || !p.targets[b.index]) continue;
              const t = S.acc(p.targets[b.index].POSITION), w = b.weight / 100 * k; any = true;
              for (let i = 0; i < t.length; i += 3) { d[i] -= t[i] * w; d[i + 1] += t[i + 1] * w; d[i + 2] -= t[i + 2] * w; }
            }
            return any ? d : null;
          });
        }
        const rec = { pos, nor, uv, jnt, wgt: wg, idx, mat: matOf(S, p.material, P.tone ? { tone: P.tone } : undefined), morph };
        (p.targets?.length ? facePrims : bodyPrims).push(rec);
      });
    }
  }
  let faceSrc = gather(facePrims), bodySrc = gather(bodyPrims);
  // the sockets and sizes below read humanoid bones by slot
  const bone = (b) => +Object.entries(human).find(([, v]) => v === b)[0];

  // dropBones: the triangles carried by those bones (and their children), for a garment part that does
  // not read well (a long coat's tails); their spring chains go too, as nothing uses them any more
  if (entry.dropBones) {
    const dj = new Uint8Array(joints.length);
    joints.forEach((n, i) => { for (let p = n; p !== undefined; p = parent[p]) if (entry.dropBones.test(J.nodes[p].name || '')) { dj[i] = 1; break; } });
    const on = (v) => { let s = 0; for (let k = 0; k < 4; k++) if (dj[bodySrc.jnt[v * 4 + k]]) s += bodySrc.wgt[v * 4 + k]; return s; };
    const idx = [], mat = []; let gone = 0;
    for (let t = 0; t < bodySrc.idx.length / 3; t++) {
      const a = bodySrc.idx[t * 3], b = bodySrc.idx[t * 3 + 1], c = bodySrc.idx[t * 3 + 2];
      if ((on(a) + on(b) + on(c)) / 3 > .5) { gone++; continue; }
      idx.push(a, b, c); mat.push(bodySrc.mat[t]);
    }
    bodySrc = compact({ ...bodySrc, idx: Uint32Array.from(idx), mat: Int32Array.from(mat) });
    console.log(`  dropBones ${entry.dropBones}: ${gone} triangles`);
  }
  // a heavier build: the trunk's vertices pushed out from the spine, the belly most
  if (entry.girth) {
    const wOf = { hips: .7, spine: 1, chest: 1, upperChest: .6, leftUpperLeg: .35, rightUpperLeg: .35, neck: .25 };
    const jw = new Float32Array(joints.length); joints.forEach((j, i) => { jw[i] = wOf[human[j]] || 0; });
    const sp = world(bone('spine')), zc = -sp[2], yc = sp[1];
    for (let i = 0; i < bodySrc.nv; i++) {
      let w = 0; for (let k = 0; k < 4; k++) w += bodySrc.wgt[i * 4 + k] * jw[bodySrc.jnt[i * 4 + k]];
      if (w <= 0) continue;
      const y = bodySrc.pos[i * 3 + 1], dz = bodySrc.pos[i * 3 + 2] - zc;
      const belly = dz > 0 ? 1 + 1.2 * Math.max(0, 1 - Math.abs(y - yc + .02) / .16) : 1;
      bodySrc.pos[i * 3] *= 1 + entry.girth * w;
      bodySrc.pos[i * 3 + 2] = zc + dz * (1 + entry.girth * w * belly);
    }
  }
  // shells (paintShell): garments grown out of the body skin
  if (entry.shells?.length) {
    const foot = Math.min(world(bone('leftFoot'))[1], world(bone('rightFoot'))[1]), hipsY = world(bone('hips'))[1] - foot;
    const humanOf = joints.map((n) => { for (let p = n; p !== undefined; p = parent[p]) if (human[p]) return human[p]; return ''; });
    const S = bodySrc, nt = S.idx.length / 3;
    const dom = new Array(S.nv);
    for (let i = 0; i < S.nv; i++) { let big = 0; for (let k = 1; k < 4; k++) if (S.wgt[i * 4 + k] > S.wgt[i * 4 + big]) big = k; dom[i] = humanOf[S.jnt[i * 4 + big]]; }
    const skinMat = (m) => /Body_00_SKIN/.test(mname(m));
    const claim = new Int32Array(nt).fill(-1);
    entry.shells.forEach((sp, si) => {
      const set = new Set(sp.bones);
      const neckY = world(bone('neck'))[1], headY = world(bone('head'))[1];
      const yOk = (v) => { const y = (S.pos[v * 3 + 1] - foot) / hipsY; if (dom[v] === 'neck' && S.pos[v * 3 + 1] > neckY + (sp.neck || 0) * (headY - neckY)) return false; return y >= (sp.yMinH ?? -9) && y <= (sp.yMaxH ?? 99); };
      for (let t = 0; t < nt; t++) if (claim[t] < 0 && skinMat(S.mat[t])) {
        const v = [S.idx[t * 3], S.idx[t * 3 + 1], S.idx[t * 3 + 2]];
        if (v.every((q) => set.has(dom[q]) && yOk(q))) claim[t] = si;
      }
    });
    const add = { pos: [], nor: [], uv: [], jnt: [], wgt: [], idx: [], mat: [] };
    let base = S.nv;
    entry.shells.forEach((sp, si) => {
      const tris = []; for (let t = 0; t < nt; t++) if (claim[t] === si) tris.push(t);
      if (!tris.length) { console.log(`  shell ${sp.name}: nothing`); return; }
      let xmax = 0, ymin = 1e9, ymax = -1e9;
      for (const t of tris) for (let c = 0; c < 3; c++) { const v = S.idx[t * 3 + c]; xmax = Math.max(xmax, Math.abs(S.pos[v * 3])); ymin = Math.min(ymin, S.pos[v * 3 + 1]); ymax = Math.max(ymax, S.pos[v * 3 + 1]); }
      const [tw, th] = sp.tex || [512, 256];
      const g = { xmax, ymin, ymax, hipsY: hipsY + foot, armY: world(bone('leftUpperArm'))[1] };
      const raw = paintShell(sp, tw, th, g);
      const m = MATS.length; MATS.push({ mat: { name: sp.name, pbrMetallicRoughness: {} }, raw, w: tw, h: th });
      const key = new Map(), push = sp.push ?? .006;
      for (const t of tris) {
        const [a, b, c] = [S.idx[t * 3], S.idx[t * 3 + 1], S.idx[t * 3 + 2]];
        const e1 = [0, 1, 2].map((k) => S.pos[b * 3 + k] - S.pos[a * 3 + k]), e2 = [0, 1, 2].map((k) => S.pos[c * 3 + k] - S.pos[a * 3 + k]);
        const side = e1[0] * e2[1] - e1[1] * e2[0] >= 0 ? 0 : 1;   // face normal's z: front or back
        for (const v of [a, b, c]) {
          const kk = v * 2 + side;
          if (!key.has(kk)) {
            key.set(kk, base++);
            for (let k = 0; k < 3; k++) { add.pos.push(S.pos[v * 3 + k] + S.nor[v * 3 + k] * push); add.nor.push(S.nor[v * 3 + k]); }
            for (let k = 0; k < 4; k++) { add.jnt.push(S.jnt[v * 4 + k]); add.wgt.push(S.wgt[v * 4 + k]); }
            const uh = Math.max(0, Math.min(1, S.pos[v * 3] / xmax * .5 + .5)), vv = Math.max(0, Math.min(1, (ymax - S.pos[v * 3 + 1]) / (ymax - ymin || 1)));
            add.uv.push(side ? .5 + (1 - uh) * .5 : uh * .5, vv);
          }
          add.idx.push(key.get(kk));
        }
        add.mat.push(m);
      }
      console.log(`  shell ${sp.name}: ${tris.length} triangles`);
    });
    // the skin under the shells goes; the shells join the body
    const keepT = []; for (let t = 0; t < nt; t++) if (claim[t] < 0) keepT.push(t);
    const nv = S.nv + add.pos.length / 3;
    const grow = (a, k, extra) => { const o = new a.constructor(nv * k); o.set(a); o.set(extra, S.nv * k); return o; };
    bodySrc = compact({
      nv, pos: grow(S.pos, 3, add.pos), nor: grow(S.nor, 3, add.nor), uv: grow(S.uv, 2, add.uv), jnt: grow(S.jnt, 4, add.jnt), wgt: grow(S.wgt, 4, add.wgt),
      morph: S.morph.map(() => new Float32Array(nv * 3)),
      idx: Uint32Array.from([...keepT.flatMap((t) => [S.idx[t * 3], S.idx[t * 3 + 1], S.idx[t * 3 + 2]]), ...add.idx]),
      mat: Int32Array.from([...keepT.map((t) => S.mat[t]), ...add.mat]),
    });
  }
  // triangles that show nothing (every sample of their texture under the alpha cutoff: a coat's cut away
  // tails, a hidden part of a template) go, or their outline hull, which has no alpha test, would draw
  // them as dark panels
  {
    const tex = new Map();
    for (const m of new Set(bodySrc.mat)) if (MATS[m].mat.alphaMode === 'MASK') tex.set(m, await rawOf(MATS[m]));
    const S = bodySrc, idx = [], mat = []; let gone = 0;
    const alpha = (T, u, v) => { u -= Math.floor(u); v -= Math.floor(v); const x = Math.min(T.w - 1, (u * T.w) | 0), y = Math.min(T.h - 1, (v * T.h) | 0); return T.raw[(y * T.w + x) * 4 + 3]; };
    for (let t = 0; t < S.idx.length / 3; t++) {
      const T = tex.get(S.mat[t]), [a, b, c] = [S.idx[t * 3], S.idx[t * 3 + 1], S.idx[t * 3 + 2]];
      if (T) {
        let seen = false;
        for (const [wa, wb, wc] of [[1, 0, 0], [0, 1, 0], [0, 0, 1], [1 / 3, 1 / 3, 1 / 3], [.6, .2, .2], [.2, .6, .2], [.2, .2, .6]]) {
          if (alpha(T, S.uv[a * 2] * wa + S.uv[b * 2] * wb + S.uv[c * 2] * wc, S.uv[a * 2 + 1] * wa + S.uv[b * 2 + 1] * wb + S.uv[c * 2 + 1] * wc) >= 115) { seen = true; break; }
        }
        if (!seen) { gone++; continue; }
      }
      idx.push(a, b, c); mat.push(S.mat[t]);
    }
    if (gone) { bodySrc = compact({ ...S, idx: Uint32Array.from(idx), mat: Int32Array.from(mat) }); console.log(`  ${gone} see-through triangles dropped`); }
  }
  // only the joints something still hangs on stay in the skin (a dropped outfit's bones go), in their order
  {
    const keep = new Uint8Array(joints.length);
    for (const M of [bodySrc, faceSrc]) for (let i = 0; i < M.nv; i++) for (let k = 0; k < 4; k++) if (M.wgt[i * 4 + k] > 0) keep[M.jnt[i * 4 + k]] = 1;
    keep[jointIndex.get(bone('head'))] = 1;
    const re = new Int32Array(joints.length).fill(-1), kept = [];
    joints.forEach((n, i) => { if (keep[i]) { re[i] = kept.length; kept.push(n); } });
    for (const M of [bodySrc, faceSrc]) for (let i = 0; i < M.jnt.length; i++) M.jnt[i] = Math.max(0, re[M.jnt[i]]);
    joints.length = 0; joints.push(...kept); jointIndex.clear(); joints.forEach((n, i) => jointIndex.set(n, i));
  }
  await MeshoptSimplifier.ready;
  const body = simplifyByMat(bodySrc, entry.keep || BODY_KEEP, 0.05);
  const face = compact(faceSrc);
  console.log(`${file}: body ${bodySrc.idx.length / 3} -> ${body.idx.length / 3} tris, ${body.nv} verts; face ${face.idx.length / 3} tris, ${face.nv} verts, joints ${joints.length}`);

  // face shading: normals off a head sphere so the cel band never cuts across the nose
  const hw = world(bone('head')); const hc = [-hw[0], hw[1] + 0.06, -hw[2]];
  for (let i = 0; i < face.nv; i++) { const d = [face.pos[i * 3] - hc[0], face.pos[i * 3 + 1] - hc[1], face.pos[i * 3 + 2] - hc[2]]; const l = Math.hypot(...d) || 1; for (let k = 0; k < 3; k++) face.nor[i * 3 + k] = d[k] / l; }

  // the far body: the same atlases, the face without its mouth, eye extras or morphs
  const farFaceSrc = compact({ ...face, ...(() => { const idx = [], mat = []; for (let t = 0; t < face.mat.length; t++) if (!/FaceMouth|EyeExtra|EyeHighlight/i.test(mname(face.mat[t]))) { idx.push(face.idx[t * 3], face.idx[t * 3 + 1], face.idx[t * 3 + 2]); mat.push(face.mat[t]); } return { idx: Uint32Array.from(idx), mat: Int32Array.from(mat) }; })() });
  const bodyT = body.idx.length / 3, ffT = farFaceSrc.idx.length / 3, farKeep = Math.min(1, FAR_TRIS / (bodyT + ffT));
  const far = simplifyByMat(body, farKeep, 0.25), farFace = simplifyByMat(farFaceSrc, Math.max(farKeep, .45), 0.25);

  const faceAt = await atlas(face, MATS, entry.faceAtlas || 1024, 'face', entry.paint, entry.retex);
  // the far face shares the face atlas: same materials, so the same placement
  remapLike(farFace, faceAt.place, faceAt.size);
  const bodyAt = await atlas(body, MATS, entry.bodyAtlas || 1024, 'body', entry.paint, entry.retex);
  remapLike(far, bodyAt.place, bodyAt.size);

  // the hulls: body and the face's skin (no eyes, lashes or mouth), pushed out and flipped
  const isSkin = (m) => /Face_00/.test(mname(m)) || (/face/i.test(mname(m)) && /skin/i.test(mname(m)));
  const extra = [];
  if (entry.glasses) extra.push([glassesMesh(face, (m) => /EyeWhite/.test(mname(m)), jointIndex.get(bone('head')), entry.glasses === true ? {} : entry.glasses)]);
  // the glasses ride in the hull meshes (zero normals: no push), both near and mid
  const hull = makeHull([[body], [face, isSkin], ...extra], OUTLINE);
  const farHull = makeHull([[far], [farFace, isSkin], ...extra], OUTLINE * 1.5);

  // spring bones, worked out before the skeleton is written: a donor's chains graft their whole
  // subtree (the chain tips carry no skin), and a chain whose bones carry nothing any more is left out
  const used = new Uint8Array(J.nodes.length + 4096);
  for (const M of [body, face]) for (let i = 0; i < M.nv; i++) for (let k = 0; k < 4; k++) if (M.wgt[i * 4 + k] > .02) used[joints[M.jnt[i * 4 + k]]] = 1;
  const lines = [];
  const addGroups = (P) => {
    const SA = P.S.V.secondaryAnimation;
    for (const g of SA.boneGroups) {
      if (/bust/i.test(g.comment || '')) continue;   // no bust physics in this game
      const roots = [];
      for (const r of g.bones) {
        const tree = subtree(P.S.J, r);
        const live = P.S === B ? tree.some((n) => used[n]) : tree.some((n) => graft.has(P.S.rel + '#' + n) && used[graft.get(P.S.rel + '#' + n)]);
        if (!live) continue;
        if (P.S !== B) tree.forEach((n) => mapNode(P, n));
        roots.push(nameOf(mapNode(P, r)));
      }
      if (roots.length) lines.push(`g ${g.stiffiness} ${g.gravityPower} ${g.dragForce} ${g.hitRadius} ${roots.length} ${roots.join(' ')}`);
    }
  };
  const colliders = (P) => {
    for (const g of P.S.V.secondaryAnimation.colliderGroups) {
      if (P.S !== B && P.S.human[g.node] === undefined) continue;
      const n = mapNode(P, g.node), k = P.k || 1;
      for (const c of g.colliders) lines.push(`c ${nameOf(n)} ${c.radius * k} ${-c.offset.x * k} ${c.offset.y * k} ${-c.offset.z * k}`);
    }
  };
  colliders(parts[0]);
  if (!lines.length) for (const P of parts.slice(1)) colliders(P);
  for (const P of parts) addGroups(P);

  // write
  const G = new GlbOut(), O = G.J;
  // quantized attributes (the loader dequantizes): normals in bytes, uvs in shorts, joints and weights in bytes
  const qNormal = (nor) => { const nv = nor.length / 3, b = new Int8Array(nv * 4); for (let i = 0; i < nv; i++) { const l = Math.hypot(nor[i * 3], nor[i * 3 + 1], nor[i * 3 + 2]) || 1; for (let k = 0; k < 3; k++) b[i * 4 + k] = Math.round(nor[i * 3 + k] / l * 127); } const v = G.blob(b); O.bufferViews[v].byteStride = 4; O.accessors.push({ bufferView: v, componentType: 5120, normalized: true, count: nv, type: 'VEC3' }); return O.accessors.length - 1; };
  const qWeights = (w) => { const b = new Uint8Array(w.length); for (let i = 0; i < w.length; i += 4) { let s = 0, big = i; for (let k = 0; k < 4; k++) { b[i + k] = Math.round(w[i + k] * 255); s += b[i + k]; if (w[i + k] > w[big]) big = i + k; } b[big] += 255 - s; } return G.acc(b, 'VEC4', 5121, false, true); };
  const qJoints = (j) => (SKIN_JOINTS < 256 ? G.acc(Uint8Array.from(j), 'VEC4', 5121) : G.acc(j, 'VEC4', 5123));
  const qUv = (uv) => G.acc(Uint16Array.from(uv, (x) => Math.round(Math.min(1, Math.max(0, x)) * 65535)), 'VEC2', 5123, false, true);
  const pad = (S, n) => {
    if (S.nv > n) throw new Error(`${file}: face has ${S.nv} vertices, more than FACE_VERTS`);
    const grow = (a, k) => { const o = new a.constructor(n * k); o.set(a); return o; };
    const P = { ...S, nv: n, pos: grow(S.pos, 3), nor: grow(S.nor, 3), uv: grow(S.uv, 2), jnt: grow(S.jnt, 4), wgt: grow(S.wgt, 4), morph: S.morph.map((d) => grow(d, 3)) };
    for (let i = S.nv; i < n; i++) { P.wgt[i * 4] = 1; P.nor[i * 3 + 1] = 1; P.pos.set(S.pos.subarray(0, 3), i * 3); }   /* unused, parked on vertex 0 */
    return P;
  };
  const add = (src, n, tex) => {
    if (n === 'face') src = pad(src, FACE_VERTS);
    const attrs = { POSITION: G.acc(src.pos, 'VEC3', 5126, true), JOINTS_0: qJoints(src.jnt), WEIGHTS_0: qWeights(src.wgt) };
    if (tex !== null) attrs.NORMAL = qNormal(src.nor);   /* the hull is unlit */
    let material;
    if (tex !== null) { attrs.TEXCOORD_0 = qUv(src.uv); O.materials.push({ name: n, alphaMode: 'MASK', alphaCutoff: 0.45, pbrMetallicRoughness: { baseColorTexture: { index: tex }, metallicFactor: 0, roughnessFactor: 1 } }); material = O.materials.length - 1; }
    else { O.materials.push({ name: n, pbrMetallicRoughness: { baseColorFactor: [0.08, 0.06, 0.08, 1], metallicFactor: 0, roughnessFactor: 1 } }); material = O.materials.length - 1; }
    const prim = { attributes: attrs, indices: src.nv < 65535 ? G.acc(Uint16Array.from(src.idx), 'SCALAR', 5123) : G.acc(Uint32Array.from(src.idx), 'SCALAR', 5125), material };
    if (n === 'face') prim.targets = EXPR.map((_, e) => ({ POSITION: G.acc(src.morph[e], 'VEC3', 5126, true) }));
    O.meshes.push({ name: n, primitives: [prim] });
    O.nodes.push({ name: n, mesh: O.meshes.length - 1, skin: 0 });
    return O.nodes.length - 1;
  };
  const bodyTex = G.tex(bodyAt.png), faceTex = G.tex(faceAt.png);
  const meshIds = [add(body, 'body', bodyTex), add(face, 'face', faceTex), add(hull, 'outline', null), add(far, 'far', bodyTex), add(farFace, 'farface', faceTex), add(farHull, 'faroutline', null)];
  // the skeleton as the source's hierarchy, translations flipped, rotations identity
  const newId = new Map();
  const walk = (i) => { const n = J.nodes[i]; const t = n.translation || [0, 0, 0]; const rec = { name: nameOf(i), translation: [-t[0], t[1], -t[2]] }; O.nodes.push(rec); const id = O.nodes.length - 1; newId.set(i, id); const ch = (n.children || []).filter((c) => J.nodes[c].mesh === undefined).map(walk); if (ch.length) rec.children = ch; return id; };
  const rootSrc = J.scenes[0].nodes.find((i) => J.nodes[i].name === 'Root');
  const skelRoot = walk(rootSrc);
  // inverse bind: pure translation, from the rest positions
  if (joints.length > SKIN_JOINTS) throw new Error(`${file}: ${joints.length} joints, more than SKIN_JOINTS`);
  const ibm = new Float32Array(SKIN_JOINTS * 16);
  for (let i = 0; i < SKIN_JOINTS; i++) { const w = world(joints[Math.min(i, joints.length - 1)]); ibm.set([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, w[0], -w[1], w[2], 1], i * 16); }
  const padded = Array.from({ length: SKIN_JOINTS }, (_, i) => newId.get(joints[Math.min(i, joints.length - 1)]));
  O.skins.push({ joints: padded, inverseBindMatrices: G.acc(ibm, 'MAT4', 5126), skeleton: skelRoot });
  O.scenes[0].nodes = [...meshIds, skelRoot];
  const id = entry.id;
  G.write(join(ROOT, 'assets/models', id + '.glb'));

  writeFileSync(join(ROOT, 'assets/models', id + '.spr'), lines.join('\n') + '\n');

  // sizes for the runtime
  const pick = (b) => world(bone(b));
  const hipsH = pick('hips')[1] - Math.min(pick('leftFoot')[1], pick('rightFoot')[1]);
  const tris = { near: body.idx.length / 3 + face.idx.length / 3, hull: hull.idx.length / 3, far: far.idx.length / 3 + farFace.idx.length / 3, farHull: farHull.idx.length / 3 };
  console.log(`  ${id}: near ${tris.near} + hull ${tris.hull}, far ${tris.far} + hull ${tris.farHull}`);
  metas.push({ id, file, hipsH, height: pick('head')[1] + 0.12, tris, rects: bodyAt.rects });
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
// node tools/prep-vrm.mjs char_x char_y: bake only those; the others keep their last meta (assets/models/cast-meta.json)
const only = process.argv.slice(2);
for (const e of CAST) if (!only.length || only.includes(e.id)) await bake(e);
const metaPath = join(ROOT, 'assets/models/cast-meta.json');
let old = {}; try { old = JSON.parse(readFileSync(metaPath)); } catch { /* first bake */ }
for (const m of metas) old[m.id] = m;
writeFileSync(metaPath, JSON.stringify(old));
const all = CAST.map((e) => old[e.id]);
if (all.some((m) => !m)) throw new Error('bake every body once: ' + CAST.filter((e) => !old[e.id]).map((e) => e.id).join(' '));
const kind = (n) => /hair/i.test(n) ? 0 : /tops|onepi|onepice|outer|jacket/i.test(n) ? 1 : /bottoms/i.test(n) ? 2 : /shoes/i.test(n) ? 3 : /access/i.test(n) ? 1 : -1;
let h = '/* made by tools/prep-vrm.mjs */\n#ifndef CAST_META_H\n#define CAST_META_H\ntypedef struct { int kind, x, y, w, h; } cast_rect;   /* kind: 0 hair, 1 top, 2 bottom, 3 shoes */\ntypedef struct { const char *glb, *spr; float hips_h, height; int nrect; cast_rect rect[24]; } cast_meta;\nstatic const cast_meta CAST_META[] = {\n';
for (const m of all) { const r = m.rects.filter((x) => kind(x.name) >= 0).slice(0, 24); h += `  { "${m.id}.glb", "${m.id}.spr", ${m.hipsH.toFixed(4)}f, ${m.height.toFixed(4)}f, ${r.length}, { ${r.map((x) => `{${kind(x.name)},${x.x},${x.y},${x.w},${x.h}}`).join(', ')} } },  /* ${m.file}: ${m.tris.near} tris + hull ${m.tris.hull}, far ${m.tris.far} */\n`; }
h += '};\n#endif\n';
writeFileSync(join(ROOT, 'src/gen/cast_meta.h'), h);
