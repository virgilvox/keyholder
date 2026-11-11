// Compiles tools/script/street.mjs (the strangers and the Chupacabra cameo) into src/gen/street_data.c
// (included by src/street.c, the types in src/street.h) and source/street_lines.json (every spoken line, for
// tools/bake_tts.py). Run it after any change to street.mjs, then bake the voices.
//
//   node tools/script/street_compile.mjs
//
// It checks the text (no emdashes, no double dashes, no emoji), the bodies, the stats, and warns about
// story flags that day.mjs does not set (read from src/gen/script.c, so compile the day first).
import { readFileSync, writeFileSync, mkdirSync, existsSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { STRANGERS, CAMEO } from './street.mjs';
import { CAST, PLAYABLE } from './cast.mjs';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const OUT_C = process.env.STREET_GEN || join(ROOT, 'src/gen/street_data.c');
const OUT_J = process.env.STREET_LINES || join(ROOT, 'source/street_lines.json');

const BODIES = new Set(['B', 'C', 'D', 'S', 'F', 'H', 'M', 'O', 'P', 'U', 'K', 'E', 'V', 'G', 'W', 'R', 'A', 'I', 'J', 'L', 'N', 'Q', 'T', 'X', 'Y', 'Z',
  'A2', 'B2', 'C2', 'D2', 'E2', 'F2', 'G2', 'H2', 'I2', 'J2', 'K2', 'L2', 'M2', 'N2', 'O2', 'P2']);
const STATS = { trust: 'ST_TRUST', safety: 'ST_SAFETY', vibe: 'ST_VIBE', dues: 'ST_DUES', energy: 'ST_ENERGY' };
const WHERE = { street: 'SW_STREET', inside: 'SW_INSIDE', lab: 'SW_LAB' };
const WHO = { them: 'SP_THEM', me: 'SP_ME', moheeb: 'SP_MOHEEB', luis: 'SP_LUIS' };

let errors = 0;
const err = (m) => { console.error('street: ' + m); errors++; };
const warn = (m) => console.warn('street: warning: ' + m);

// the story's flags, to warn about a talk that waits on a flag nothing sets
const scriptC = join(ROOT, 'src/gen/script.c');
const FLAGS = existsSync(scriptC) ? new Set([...(readFileSync(scriptC, 'utf8').match(/FLAG_NAMES\[FLAG_COUNT\] = \{([^}]*)\}/) || ['', ''])[1].matchAll(/"([^"]+)"/g)].map((m) => m[1])) : null;
// the downtown place keys, to tell which venue a room is
const VENUES = [...readFileSync(join(ROOT, 'docs/PLACES.md'), 'utf8').matchAll(/^\| `([a-z0-9_]+)` \|/gm)].map((m) => m[1]);

const cstr = (s) => (s == null ? 'NULL' : JSON.stringify(s));
function checkText(s, where) {
  if (typeof s !== 'string') return;
  if (/—|–/.test(s)) err(`a dash in ${where}: ${s}`);
  if (/\S--\S|\s--\s/.test(s)) err(`a double dash in ${where}: ${s}`);
  if (/\p{Extended_Pictographic}/u.test(s)) err(`an emoji in ${where}: ${s}`);
}
// text by who is playing: a string, or { vee, dex, ruth, any }
function variants(t, where) {
  const pick = (p) => (typeof t === 'string' ? t : t[p] ?? t.any);
  const v = PLAYABLE.map(pick);
  v.forEach((s, i) => { if (s == null) err(`no text for ${PLAYABLE[i]} in ${where}`); checkText(s, where); });
  return v;
}
const expand = (s, p) => s.replaceAll('{me}', CAST[p].name);

// ---- the voices: every (speaker, text) once
const lines = new Map();
function voice(speaker, text, v, kyutai) {
  const k = `${speaker}|${text}`;
  if (!lines.has(k)) lines.set(k, { speaker, voice: v.voice, speed: v.speed, text, ...(kyutai ? { kyutai } : {}) });
}
function voiceSay(who, v3, owner) {
  if (who === 'me') { PLAYABLE.forEach((p, i) => voice(p, expand(v3[i], p), CAST[p])); return; }
  const spk = who === 'them' ? owner.speaker : who;
  const v = who === 'them' ? owner : CAMEO.people[who];
  const kyutai = who === 'moheeb' || who === 'luis' ? who : null;
  PLAYABLE.forEach((p, i) => voice(spk, expand(v3[i], p), v, kyutai));
}

// ---- the beats
const BEATS = [];
function emit(beats, owner, where) {
  for (const b of beats.flat(Infinity)) {
    if (b.op === 'say') {
      if (!(b.who in WHO)) { err(`who ${b.who} in ${where}`); continue; }
      if (owner.cameo ? b.who === 'them' : b.who === 'moheeb' || b.who === 'luis') err(`${b.who} cannot speak in ${where}`);
      const v = variants(b.text, where);
      voiceSay(b.who, v, owner);
      BEATS.push({ op: 'SB_SAY', who: WHO[b.who], text: v, clip: b.clip || null });
    } else if (b.op === 'choice') {
      checkText(b.prompt, where);
      const at = BEATS.length;
      BEATS.push({ op: 'SB_CHOICE', a: b.options.length, text: [b.prompt, b.prompt, b.prompt] });
      const opts = b.options.map((o) => {
        const ob = { op: 'SB_OPT', text: variants(o.text, where), say: o.act ? null : variants(o.say ?? o.text, where) };
        BEATS.push(ob); return ob;
      });
      if (b.options.length < 2 || b.options.length > 5) err(`a choice needs 2 to 5 options in ${where}`);
      const jumps = [];
      b.options.forEach((o, i) => {
        opts[i].a = BEATS.length;
        if (opts[i].say) PLAYABLE.forEach((p, k) => voice(p, expand(opts[i].say[k], p), CAST[p]));
        emit(o.beats || [], owner, where);
        const j = { op: 'SB_JUMP', a: -1 }; BEATS.push(j); jumps.push(j);
      });
      for (const j of jumps) j.a = BEATS.length;
      void at;
    } else if (b.op === 'add') {
      if (!STATS[b.stat]) err(`stat ${b.stat} in ${where}`);
      BEATS.push({ op: 'SB_ADD', a: STATS[b.stat] || 'ST_VIBE', b: b.n });
    } else if (b.op === 'mark') BEATS.push({ op: 'SB_MARK', text: [b.name, b.name, b.name] });
    else if (b.op === 'note') { checkText(b.text, where); BEATS.push({ op: 'SB_NOTE', text: [b.text, b.text, b.text] }); }
    else err(`unknown beat ${JSON.stringify(b).slice(0, 80)} in ${where}`);
  }
}

// ---- the talks
const TALKS = [];
const onOff = (s) => (s == null ? [null, false] : s.startsWith('!') ? [s.slice(1), true] : [s, false]);
function talk(t, owner, where) {
  const w = t.when || {};
  const [flag, flagNot] = onOff(w.flag), [mark, markNot] = onOff(w.mark);
  if (flag && FLAGS && !FLAGS.has(flag)) warn(`${where}: day.mjs sets no flag ${flag}`);
  if (w.venue && !VENUES.includes(w.venue)) warn(`${where}: no place ${w.venue} in docs/PLACES.md`);
  const [h0, h1] = w.hour || [0, 99];
  const sc = w.stat || w.low, st = sc ? STATS[sc[0]] : null;
  if (sc && !st) err(`stat ${sc[0]} in ${where}`);
  const rec = { h0, h1, met: w.met === true ? 1 : w.met === false ? 0 : -1, flag, flagNot, mark, markNot,
    stat: st || '-1', gte: sc ? sc[1] : 0, low: !!w.low, venue: w.venue || null, again: !!w.again, start: BEATS.length };
  emit(t.beats, owner, where);
  BEATS.push({ op: 'SB_END' });
  TALKS.push(rec);
}

const TYPES = [];
const ids = new Set();
for (const s of STRANGERS) {
  const where = `stranger ${s.id}`;
  if (ids.has(s.id)) err(`two strangers called ${s.id}`); ids.add(s.id);
  if (!/^[a-z0-9_]+$/.test(s.id)) err(`id ${s.id}: lower case, digits and _`);
  checkText(s.name, where);
  if (s.name.length > 18) err(`${where}: the name ${s.name} is longer than the dialogue box's tag (18)`);
  const bodies = [].concat(s.body);
  for (const b of bodies) if (!BODIES.has(b)) err(`${where}: no body ${b}`);
  const w = [].concat(s.where || 'street');
  for (const x of w) if (!WHERE[x]) err(`${where}: where ${x}`);
  const owner = { speaker: `st_${s.id}`, voice: s.voice, speed: s.speed || 1 };
  const t0 = TALKS.length;
  s.talks.forEach((t, i) => talk(t, owner, `${where} talk ${i + 1}`));
  if (!s.talks.some((t) => t.when && t.when.again)) warn(`${where}: no talk marked again, so they go quiet after the rest`);
  const [h0, h1] = s.hours || [0, 99];
  TYPES.push({ id: s.id, name: s.name, speaker: owner.speaker, color: s.color || 0xd8d0c0, bodies, where: w, look: s.look ? 1 : 0, h0, h1, t0, nt: TALKS.length - t0 });
}
// the cameo
const ch0 = TALKS.length;
CAMEO.chatter.forEach((c, i) => talk({ when: c.when, beats: c.lines.map(([who, text]) => ({ op: 'say', who, text })) }, { cameo: true }, `cameo chatter ${i + 1}`));
const ct0 = TALKS.length;
CAMEO.talks.forEach((t, i) => talk(t, { cameo: true }, `cameo talk ${i + 1}`));

if (errors) { console.error(`street: ${errors} errors, nothing written`); process.exit(1); }

// ---- write
const fl = (v) => `${Number(v).toFixed(3)}f`;
const t3 = (a) => (a ? `{${a.map(cstr).join(', ')}}` : '{NULL, NULL, NULL}');
let c = `/* gen/street_data.c: made by tools/script/street_compile.mjs from tools/script/street.mjs. Do not edit. */\n/* included by src/street.c */\n\n`;
c += `const st_beat ST_BEATS[] = {\n`;
BEATS.forEach((b, i) => {
  c += `  /*${i}*/ {${b.op}, ${b.who || 0}, ${b.a ?? 0}, ${b.b ?? 0}, ${t3(b.text)}, ${t3(b.say)}, ${cstr(b.clip)}},\n`;
});
c += `};\nconst st_talk ST_TALKS[] = {\n`;
for (const t of TALKS) c += `  {${fl(t.h0)}, ${fl(t.h1)}, ${t.met}, ${cstr(t.flag)}, ${t.flagNot}, ${cstr(t.mark)}, ${t.markNot}, ${t.stat}, ${t.gte}, ${t.low}, ${cstr(t.venue)}, ${t.again}, ${t.start}},\n`;
c += `};\nconst st_type ST_TYPES[] = {\n`;
for (const t of TYPES) {
  const bodies = t.bodies.map((b) => `(1ull << AM_${b})`).join(' | ');
  const where = t.where.map((w) => WHERE[w]).join(' | ');
  c += `  {${cstr(t.id)}, ${cstr(t.name)}, ${cstr(t.speaker)}, 0x${t.color.toString(16).padStart(6, '0')}, ${bodies}, ${where}, ${t.look}, ${fl(t.h0)}, ${fl(t.h1)}, ${t.t0}, ${t.nt}},\n`;
}
c += `};\nconst int ST_NTYPE = ${TYPES.length}, ST_NTALK = ${TALKS.length};\n`;
const P = CAMEO.people;
c += `const st_person ST_CAMEO[2] = {{${cstr(P.moheeb.name)}, "moheeb", 0x${P.moheeb.color.toString(16)}}, {${cstr(P.luis.name)}, "luis", 0x${P.luis.color.toString(16)}}};\n`;
c += `const float ST_CAMEO_H0 = ${fl(CAMEO.hours[0])}, ST_CAMEO_H1 = ${fl(CAMEO.hours[1])};\n`;
c += `const int ST_CHAT0 = ${ch0}, ST_NCHAT = ${ct0 - ch0}, ST_CTALK0 = ${ct0}, ST_NCTALK = ${TALKS.length - ct0};\n`;
c += `const char *const ST_VENUES[] = {${VENUES.map(cstr).join(', ')}};\nconst int ST_NVENUE = ${VENUES.length};\n`;
mkdirSync(dirname(OUT_C), { recursive: true });
writeFileSync(OUT_C, c);
const L = [...lines.values()];
writeFileSync(OUT_J, JSON.stringify(L, null, 1) + '\n');
const words = L.reduce((n, l) => n + l.text.split(/\s+/).length, 0);
console.log(`street: ${TYPES.length} strangers, ${TALKS.length} talks (${ct0 - ch0} cameo chatter, ${TALKS.length - ct0} cameo talks), ${BEATS.length} beats, ${L.length} voiced lines (${L.filter((l) => l.kyutai).length} Kyutai), about ${words} words`);
