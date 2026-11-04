// The beats the day is written in. tools/script/compile.mjs turns them into src/gen/script.c.
// who: 'me' (the player's character), 'm1', 'm2' (the other two members), or a cast id.
// Text may use {me}, {m1}, {m2}: names are spliced in and each variant is voiced separately.
export const lab = (x, z, yaw = 0) => ({ k: 'lab', x, z, yaw });          // HSL-108 plan metres, plan yaw degrees
export const at = (name, dx = 0, dz = 0, yaw = null) => ({ k: 'place', name, x: dx, z: dz, yaw });   // offset from a world_place, world axes, world degrees
export const stage = (x = 0, z = 0, yaw = 0) => ({ k: 'stage', x, z, yaw });   // offset from the stage's centre

export const CHAPTER = (hour, title, sub) => ({ op: 'chapter', hour, title, sub });
export const CLOCK = (hour) => ({ op: 'clock', hour });
export const P = (who, mark, face) => ({ op: 'place', who, mark, face });      // appear at a mark, facing an actor
export const HIDE = (...who) => who.map((w) => ({ op: 'hide', who: w }));
export const W = (who, mark, o = {}) => ({ op: 'walk', who, mark, ...o });     // {speed, nowait, run}
export const WAITW = (who) => ({ op: 'waitwalk', who });
export const S = (who, text, o = {}) => ({ op: 'say', who, text, ...o });      // {emote, amount, clip, look, shot, to}
export const SHOT = (type, subjects = [], o = {}) => ({ op: 'shot', type, subjects, ...o });  // wide two ots mcu cu low dutch orbit crane track establish
export const FIX = (pos, look, fov = 40) => ({ op: 'shot', type: 'fixed', pos, look, fov });
export const WAIT = (t) => ({ op: 'wait', t });
export const FACE = (who, target) => ({ op: 'face', who, target });
export const LOOK = (who, target) => ({ op: 'look', who, target });
export const CLIP = (who, clip, o = {}) => ({ op: 'clip', who, clip, ...o });  // {loop, speed}
export const EMO = (who, emote, amount = 1) => ({ op: 'emote', who, emote, amount });
export const OBJ = (text) => ({ op: 'objective', text });
export const ROAM = (place, radius, prompt, o = {}) => ({ op: 'roam', place, radius, prompt, ...o }); // free walk until there; o.sides: side conversations
export const SIDE = (name, who, mark, prompt, beats, o = {}) => ({ name, who, mark, prompt, beats, ...o });
export const FIGHT = (id, o = {}) => ({ op: 'fight', id, ...o });
export const CHOICE = (prompt, options) => ({ op: 'choice', prompt, options }); // {text, say, beats}
export const O = (text, beats = [], o = {}) => ({ text, beats, ...o });
export const IF = (cond, a, b = []) => ({ op: 'if', cond, a, b });             // {stat, gte}, {flag}, {not}
export const SET = (flag) => ({ op: 'flag', flag });
export const ADD = (stat, n = 1, quiet = false) => ({ op: 'add', stat, n, quiet });  // trust safety vibe dues energy; aff_sam, aff_kai, aff_morgan, aff_m1, aff_m2
export const CARD = (title, sub = '', dur = 2.6) => ({ op: 'card', title, sub, dur });
export const MUSIC = (mood) => ({ op: 'music', mood });       // title explore fight boss clear cine silent
export const STAGE = (name) => ({ op: 'stage', name });       // town bench laser fridge shop unfold
export const GIVE = (weapon, line) => ({ op: 'give', weapon, line });
export const DRAW = (who, weapon) => ({ op: 'draw', who, weapon });  // weapon appears in hand with a flash, -1 puts it away
export const DOOR = (open) => ({ op: 'door', open });
export const FX = (kind, t = 0.5) => ({ op: 'fx', kind, t });  // fade_in fade_out flash slowmo shake lights_out
export const NOTE = (text) => ({ op: 'note', text });          // a small line on screen, no voice
export const END = () => ({ op: 'end' });
