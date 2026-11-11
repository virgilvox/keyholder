// Compiles tools/script/day.mjs into src/gen/script.c (the beats as C tables) and source/lines.json
// (every voiced line, per speaker, every name variant, for tools/bake_tts.py).
import { writeFileSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
const { DAY, FIGHTS, ENDINGS, STAT_LINES } = await import(process.env.DAY ? new URL(process.env.DAY, 'file://' + process.cwd() + '/').href : './day.mjs');
import { CAST, PLAYABLE, ROLES } from './cast.mjs';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '../..');
const WHO = ['me', 'm1', 'm2', 'shadow', ...Object.keys(CAST).filter((c) => !PLAYABLE.includes(c)), ...PLAYABLE];
const whoId = (w) => { const i = WHO.indexOf(w); if (i < 0) throw new Error('unknown actor ' + w); return i; };
const cstr = (s) => (s === null || s === undefined ? 'NULL' : JSON.stringify(s));
const STATS = ['trust', 'safety', 'vibe', 'dues', 'energy', 'aff_sam', 'aff_kai', 'aff_morgan', 'aff_m1', 'aff_m2'];
const WEAP = { ARC: 0, MAUL: 1, TWIN: 2, SPEAR: 3, STAFF: 4 };
const OPS = ['chapter', 'clock', 'place', 'hide', 'walk', 'waitwalk', 'say', 'shot', 'wait', 'face', 'look', 'clip', 'emote', 'objective', 'roam', 'fight', 'choice', 'option', 'jump', 'jump_not', 'flag', 'add', 'card', 'music', 'stage', 'give', 'draw', 'door', 'fx', 'note', 'epilogue', 'end', 'ret', 'bark'];
const FIGHT_IDS = Object.keys(FIGHTS);
const BARK_ON = ['start', 'phase2', 'foe_low', 'player_low', 'assist', 'obj_warn', 'hazard', 'finish', 'parry'];
const flags = new Set();
const EMOTES = { joy: 'EX_JOY', fun: 'EX_FUN', angry: 'EX_ANGRY', sorrow: 'EX_SORROW', relaxed: 'EX_FUN' };

const beats = [];
const sides = [];
const tasks = [];
const banter = [];
const TASK_KINDS = ['greet', 'point', 'answer', 'carry', 'fix', 'signin', 'errand'];
const FIX_MODES = ['timing', 'mash', 'hold', 'stick'];
const emit = (o) => { beats.push(o); return beats.length - 1; };
const mark = (m) => (m ? m : null);
function flat(list) { for (const b of list) { if (Array.isArray(b)) flat(b); else one(b); } }
function one(b) {
  switch (b.op) {
    case 'if': {
      const c = b.cond;
      if (c.flag) flags.add(c.flag);
      const j = emit({ op: 'jump_not', stat: c.stat || null, gte: c.gte ?? 0, flag: c.flag || null, target: 0 });
      flat(b.a);
      if (b.b && b.b.length) { const e = emit({ op: 'jump', target: 0 }); beats[j].target = beats.length; flat(b.b); beats[e].target = beats.length; }
      else beats[j].target = beats.length;
      return;
    }
    case 'choice': {
      emit({ op: 'choice', prompt: b.prompt, n: b.options.length });
      const opts = b.options.map((o) => emit({ op: 'option', text: o.text, say: o.say === undefined ? o.text : o.say, target: 0 }));
      const ends = [];
      b.options.forEach((o, i) => { beats[opts[i]].target = beats.length; flat(o.beats || []); ends.push(emit({ op: 'jump', target: 0 })); });
      ends.forEach((e) => (beats[e].target = beats.length));
      return;
    }
    case 'roam': {
      const tl = b.tasks || [], bl = b.banter || [];
      const entry = emit({ ...b, sides: undefined, tasks: undefined, banter: undefined, side0: sides.length, nside: (b.sides || []).length,
        t0: tasks.length, nt: tl.length, need: b.need ?? 0, k0: banter.length, nk: bl.length });
      const list = b.sides || [];
      const skip = (list.length || tl.length) ? emit({ op: 'jump', target: 0 }) : -1;
      for (const s of list) { const start = beats.length; flat(s.beats); emit({ op: 'ret' }); sides.push({ ...s, start }); }
      const names = tl.map((t) => t.name);
      for (const t of tl) {
        if (!TASK_KINDS.includes(t.kind)) throw new Error('task kind ' + t.kind);
        let start = -1, done = -1;
        if (t.before && t.before.length) { start = beats.length; flat(t.before); emit({ op: 'ret' }); }
        if (t.after && t.after.length) { done = beats.length; flat(t.after); emit({ op: 'ret' }); }
        if (t.requires && !names.includes(t.requires)) throw new Error('task requires ' + t.requires);
        tasks.push({ ...t, start, done, req: t.requires ? beats[entry].t0 + names.indexOf(t.requires) : -1 });
      }
      for (const k of bl) banter.push(k);
      if (skip >= 0) beats[skip].target = beats.length;
      return;
    }
    case 'flag': flags.add(b.flag); emit(b); return;
    case 'fight': if (b.win_flag) flags.add(b.win_flag); if (b.easier_flag) flags.add(b.easier_flag); emit(b); return;
    default: emit(b);
  }
}
flat(DAY);
for (const F of Object.values(FIGHTS)) for (const a of F.assists || []) if (a.flag) flags.add(a.flag);
for (const e of ENDINGS) flags.add(e.flag);
for (const s of STAT_LINES) { flags.add(s.flag); if (s.not) flags.add(s.not); }
const flagList = [...flags];
const flagId = (f) => (f ? flagList.indexOf(f) : -1);

const markC = (m) => {
  if (!m) return '{0}';
  const kind = { lab: 'MK_LAB', place: 'MK_PLACE', stage: 'MK_STAGE' }[m.k];
  return `{${kind}, ${cstr(m.name)}, ${(+m.x).toFixed(3)}f, ${(+m.z).toFixed(3)}f, ${m.yaw === null || m.yaw === undefined ? '-9999.f' : (+m.yaw).toFixed(2) + 'f'}}`;
};
const PART = { head: 'PART_HEAD', rhand: 'PART_RHAND', lhand: 'PART_LHAND', point: 'PART_POINT' };
const subj = (a = []) => `{${[0, 1, 2].map((i) => (a[i] !== undefined ? whoId(a[i]) : -1)).join(',')}}`;
let out = `/* GENERATED by tools/script/compile.mjs from tools/script/day.mjs. Do not edit. */\n#include "script.h"\n#include "fight.h"\n#include "actor.h"\n\n`;
out += `const char *const WHO_ID[WHO_COUNT] = {${WHO.map(cstr).join(', ')}};\n`;
out += `const char *const FLAG_NAMES[FLAG_COUNT] = {${flagList.map(cstr).join(', ')}};\n\n`;
const shotType = (t) => 'SH_' + t.toUpperCase();
out += 'const sc_beat SCRIPT[] = {\n';
beats.forEach((b, i) => {
  const f = { op: 'OP_' + b.op.toUpperCase(), who: -1, who2: -1, i: 0, i2: 0, f: 0, f2: 0, s: 'NULL', s2: 'NULL', mark: '{0}', mark2: '{0}', subj: '{-1,-1,-1}', target: 0, flag: -1, t0: 0, nt: 0, need: 0, k0: 0, nk: 0, f3: 0 };
  switch (b.op) {
    case 'chapter': f.f = b.hour; f.s = cstr(b.title); f.s2 = cstr(b.sub); break;
    case 'clock': f.f = b.hour; break;
    case 'place': f.who = whoId(b.who); f.mark = markC(b.mark); f.who2 = b.face ? whoId(b.face) : -1; break;
    case 'hide': f.who = whoId(b.who); break;
    case 'walk': f.who = whoId(b.who); f.mark = markC(b.mark); f.f = b.speed || (b.run ? 3.4 : 1.35); f.i = b.nowait ? 1 : 0; break;
    case 'waitwalk': f.who = whoId(b.who); break;
    case 'say': f.who = whoId(b.who); f.s = cstr(b.text); f.s2 = cstr(b.clip); f.i = b.emote ? EMOTES[b.emote] : -1; f.f = b.amount ?? 0; f.i2 = b.shot ? shotType(b.shot) : 'SH_AUTO'; f.who2 = b.to ? whoId(b.to) : -1; break;
    case 'shot': f.i = shotType(b.type); f.subj = subj(b.subjects); f.f = b.dist || 0; f.f2 = b.height || 0; if (b.type === 'fixed') { f.mark = markC(b.pos); f.mark2 = markC(b.look); f.f = b.fov; } else { f.i2 = Math.round((b.fov || 0) * 10); if (b.dur) f.target = Math.round(b.dur * 100); if (b.part) f.flag = PART[b.part]; if (b.at) f.mark = markC(b.at); } break;
    case 'wait': f.f = b.t; break;
    case 'face': f.who = whoId(b.who); f.who2 = whoId(b.target); break;
    case 'look': f.who = whoId(b.who); f.who2 = whoId(b.target); break;
    case 'clip': f.who = whoId(b.who); f.s = cstr(b.clip); f.i = b.loop ? 1 : 0; f.f = b.speed || 1; break;
    case 'emote': f.who = whoId(b.who); f.i = EMOTES[b.emote]; f.f = b.amount; break;
    case 'objective': f.s = cstr(b.text); break;
    case 'roam': f.s = cstr(b.place); f.f = b.radius || 0; f.s2 = cstr(b.prompt); f.f2 = b.hour || 0; f.i = b.side0; f.i2 = b.nside; f.who = b.follow ? whoId(b.follow) : -1; f.t0 = b.t0; f.nt = b.nt; f.need = b.need; f.k0 = b.k0; f.nk = b.nk; f.f3 = b.until || 0; f.flag = b.indoors ? 1 : 0; break;
    case 'bark': f.who = whoId(b.who); f.s = cstr(b.text); break;
    case 'fight': f.i = 'FI_' + b.id.toUpperCase(); f.i2 = (b.tutorial ? 1 : 0) | (b.cant_lose ? 2 : 0) | (b.lose_ok ? 4 : 0); f.flag = flagId(b.win_flag); f.target = flagId(b.easier_flag); break;
    case 'choice': f.s = cstr(b.prompt); f.i = b.n; break;
    case 'option': f.s = cstr(b.text); f.s2 = cstr(b.say); f.target = b.target; break;
    case 'jump': f.target = b.target; break;
    case 'jump_not': f.target = b.target; f.i = b.stat ? STATS.indexOf(b.stat) : -1; f.f = b.gte; f.flag = flagId(b.flag); break;
    case 'flag': f.flag = flagId(b.flag); break;
    case 'add': f.i = STATS.indexOf(b.stat); f.i2 = b.n; if (f.i < 0) throw new Error('stat ' + b.stat); break;
    case 'card': f.s = cstr(b.title); f.s2 = cstr(b.sub); f.f = b.dur; break;
    case 'music': f.i = 'MOOD_' + b.mood.toUpperCase(); break;
    case 'stage': f.i = { town: 'STAGE_TOWN', bench: 'STAGE_ARENA_BENCH', laser: 'STAGE_ARENA_LASER', fridge: 'STAGE_ARENA_FRIDGE', shop: 'STAGE_ARENA_SHOP', unfold: 'STAGE_ARENA_UNFOLD' }[b.name]; break;
    case 'give': f.i = WEAP[b.weapon]; f.s = cstr(b.line); break;
    case 'draw': f.who = whoId(b.who); f.i = b.weapon === -1 ? -1 : WEAP[b.weapon]; break;
    case 'door': f.f = b.open; break;
    case 'fx': f.i = 'FX_' + b.kind.toUpperCase(); f.f = b.t; break;
    case 'note': f.s = cstr(b.text); break;
  }
  out += `  /*${i}*/ {${f.op}, ${f.who}, ${f.who2}, ${f.i}, ${f.i2}, ${f.flag}, ${f.target}, ${(+f.f).toFixed(3)}f, ${(+f.f2).toFixed(3)}f, ${f.s}, ${f.s2}, ${f.mark}, ${f.mark2}, ${f.subj}, ${f.t0}, ${f.nt}, ${f.need}, ${f.k0}, ${f.nk}, ${(+f.f3).toFixed(3)}f},\n`;
});
out += '};\n';
out += `const int SCRIPT_COUNT = ${beats.length};\n\n`;
const stat2 = (r = {}) => { const e = Object.entries(r); if (e.length > 2) throw new Error('reward has more than two stats'); const st = [0, 1].map((i) => (e[i] ? STATS.indexOf(e[i][0]) : -1)); if (st.some((x, i) => e[i] && x < 0)) throw new Error('reward stat'); return { st, n: [0, 1].map((i) => (e[i] ? e[i][1] : 0)) }; };
out += 'const sc_task TASKS[] = {\n' + tasks.map((t) => { const r = stat2(t.reward); return `  {${cstr(t.name)}, TK_${t.kind.toUpperCase()}, ${t.mode ? FIX_MODES.indexOf(t.mode) : 0}, ${t.who ? whoId(t.who) : -1}, ${markC(t.at)}, ${markC(t.to)}, ${cstr(t.prompt || null)}, ${cstr(t.label || null)}, ${t.n || 1}, {${r.st}}, {${r.n}}, ${t.start}, ${t.done}, ${t.optional ? 'true' : 'false'}, ${t.req}, ${(+(t.delay || 0)).toFixed(2)}f},`; }).join('\n') + '\n  {0}\n};\n';
out += 'const sc_banter BANTER[] = {\n' + banter.map((k) => `  {${whoId(k.who)}, ${cstr(k.text)}, ${(+(k.delay || 0)).toFixed(2)}f, ${markC(k.near)}},`).join('\n') + '\n  {0}\n};\n\n';
out += 'const sc_side SIDES[] = {\n' + sides.map((s) => `  {${cstr(s.name)}, ${whoId(s.who)}, ${markC(s.mark)}, ${cstr(s.prompt)}, ${s.start}},`).join('\n') + '\n  {0}\n};\n\n';
// cast
const hue = (h) => `{${h.hue}, ${h.sat}, ${h.val}}`;
const pal = (p) => `{${hue(p.hair)}, ${hue(p.top)}, ${hue(p.bottom)}, ${hue(p.shoes)}}`;
out += 'const sc_cast CAST[WHO_COUNT] = {\n';
for (const w of WHO) {
  const c = CAST[w];
  if (!c) { out += `  {${cstr(w)}, NULL, AM_B, ${pal({ hair: { hue: 0, sat: 1, val: 1 }, top: { hue: 0, sat: 1, val: 1 }, bottom: { hue: 0, sat: 1, val: 1 }, shoes: { hue: 0, sat: 1, val: 1 } })}, 1, 0xffffff},\n`; continue; }
  out += `  {${cstr(w)}, ${cstr(c.name)}, AM_${c.model}, ${pal(c.pal)}, ${(+c.scale).toFixed(3)}f, 0x${c.color.toString(16)}},\n`;
}
out += '};\n\n';
// fights
out += 'const sc_fight FIGHT_TABLE[FI_COUNT] = {\n';
for (const id of FIGHT_IDS) {
  const F = FIGHTS[id];
  out += `  {${cstr(F.title)}, ${cstr(F.sub)}, ${cstr(F.where)}, ${(+F.radius).toFixed(2)}f, ${F.foes.length}, {`;
  out += F.foes.map((e) => `{${whoId(e.who)}, ${WEAP[e.weapon]}, AI_${e.ai}, ${(+e.hp).toFixed(1)}f, ${(+e.power).toFixed(3)}f, ${(+e.speed).toFixed(3)}f, ${(+e.at[0]).toFixed(2)}f, ${(+e.at[1]).toFixed(2)}f, ${cstr(e.palette || null)}, ${cstr(e.name || null)}}`).join(', ');
  const barks = F.barks || [], as = F.assists || [];
  if (barks.length > 10 || as.length > 3) throw new Error('fight ' + id + ': at most 10 barks and 3 assists');
  for (const k of barks) if (!BARK_ON.includes(k.on)) throw new Error('bark trigger ' + k.on);
  out += `}, FT_${(F.twist || 'none').toUpperCase()}, ${(+(F.twist_a || 0)).toFixed(2)}f, ${(+(F.twist_b || 0)).toFixed(2)}f, ${(+(F.obj ? F.obj[0] : 0)).toFixed(2)}f, ${(+(F.obj ? F.obj[1] : 0)).toFixed(2)}f, ${cstr(F.obj_label || null)}, ${F.protect ? whoId(F.protect) : -1}, `;
  out += `${barks.length}, {${barks.map((k) => `{FB_${k.on.toUpperCase()}, ${whoId(k.who)}, ${cstr(k.text)}}`).join(', ') || '{0}'}}, `;
  out += `${as.length}, {${as.map((a) => `{${whoId(a.who)}, ${flagId(a.flag)}, ${(+(a.trust || 0)).toFixed(1)}f}`).join(', ') || '{0}'}}, `;
  out += `${(+(F.phase2 || 0)).toFixed(3)}f, ${F.qte ? 'true' : 'false'}, ${F.tier || 1}},\n`;
}
out += '};\n\n';
out += 'const sc_ending ENDINGS[] = {\n' + ENDINGS.map((e) => `  {${flagId(e.flag)}, ${cstr(e.title)}, {${e.lines.map(cstr).join(', ')}}},`).join('\n') + '\n  {-1}\n};\n';
out += 'const sc_statline STAT_LINES[] = {\n' + STAT_LINES.map((s) => `  {${flagId(s.flag)}, ${flagId(s.not)}, ${cstr(s.text)}},`).join('\n') + '\n  {-1}\n};\n';
const GEN = process.env.GEN ? join(process.cwd(), process.env.GEN) : join(ROOT, 'src/gen');
mkdirSync(GEN, { recursive: true });
writeFileSync(join(GEN, 'script.c'), process.env.GEN ? out.replace('#include "script.h"', '#include "script.h"') : out);
let hdr = `/* GENERATED by tools/script/compile.mjs. */\n#ifndef SCRIPT_GEN_H\n#define SCRIPT_GEN_H\n`;
hdr += `enum { ${WHO.map((w, i) => `W_${w.toUpperCase()} = ${i}`).join(', ')}, WHO_COUNT = ${WHO.length} };\n`;
hdr += `enum { FLAG_COUNT = ${flagList.length} };\n`;
hdr += `enum { ${FIGHT_IDS.map((f) => 'FI_' + f.toUpperCase()).join(', ')}, FI_COUNT };\n`;
hdr += `enum { ${OPS.map((o) => 'OP_' + o.toUpperCase()).join(', ')} };\n`;
hdr += `enum { ${TASK_KINDS.map((k) => 'TK_' + k.toUpperCase()).join(', ')}, TK_COUNT };\n`;
hdr += `enum { FIX_TIMING, FIX_MASH, FIX_HOLD, FIX_STICK };\n`;
hdr += `enum { ${STATS.map((s) => 'ST_' + s.toUpperCase()).join(', ')}, ST_COUNT };\n#endif\n`;
writeFileSync(join(GEN, 'script_enum.h'), hdr);

// the lines: expand names, per playable character
const lines = new Map();
const addLine = (who, text, me) => {
  const R = ROLES[me];
  const speaker = who === 'me' || who === 'shadow' ? me : who === 'm1' ? R.m1 : who === 'm2' ? R.m2 : who;
  const t = text.replace(/\{me\}/g, CAST[me].name).replace(/\{m1\}/g, CAST[R.m1].name).replace(/\{m2\}/g, CAST[R.m2].name);
  const c = CAST[speaker];
  lines.set(speaker + '|' + t, { speaker, voice: c.voice, speed: c.speed, text: t });
};
for (const me of PLAYABLE) for (const b of beats) {
  if (b.op === 'say') addLine(b.who, b.text, me);
  if (b.op === 'option' && b.say) addLine('me', b.say, me);
  if (b.op === 'bark') addLine(b.who, b.text, me);
}
for (const me of PLAYABLE) {
  for (const k of banter) addLine(k.who, k.text, me);
  for (const F of Object.values(FIGHTS)) for (const k of F.barks || []) addLine(k.who, k.text, me);
}
mkdirSync(join(ROOT, 'source'), { recursive: true });
writeFileSync(process.env.GEN ? join(GEN, 'lines.json') : join(ROOT, 'source/lines.json'), JSON.stringify([...lines.values()], null, 0));
const chars = [...lines.values()].reduce((a, l) => a + l.text.length, 0);
console.log(`${beats.length} beats, ${sides.length} side talks, ${tasks.length} tasks, ${banter.length} banter, ${flagList.length} flags, ${FIGHT_IDS.length} fights, ${lines.size} voiced takes, ${chars} characters`);
