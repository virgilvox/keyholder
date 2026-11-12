// The beats the day is written in. tools/script/compile.mjs turns them into src/gen/script.c.
// who: 'me' (the player's character), 'm1', 'm2' (the other two members), or a cast id.
// Text may use {me}, {m1}, {m2}: names are spliced in and each variant is voiced separately.
// Text may also be { vee, dex, ruth, any } for S and BARK: the line picked by who fills the slot ('me', 'm1', 'm2',
// 'shadow'), or by who is playing for any other speaker. 'any' is the fallback. Banter and fight barks take plain text.
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
export const CHOICE = (prompt, options) => ({ op: 'choice', prompt, options }); // {text, say, act, beats}
// O(text, beats, o): an option the player says. text is the menu line and, unless o.say gives other words, the line
// spoken aloud. o.say may be { vee, dex, ruth } for words that differ by who is playing.
export const O = (text, beats = [], o = {}) => ({ text, beats, ...o });
// A(text, beats, o): an action option ("Put it down. Walk over."). It is shown in the menu and never spoken.
export const A = (text, beats = [], o = {}) => ({ text, beats, ...o, act: true });
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

// ---- v1.1: hosting gameplay, banter, barks, film language
// ROAM(place, radius, prompt, { hour, until, sides, tasks, need, banter, indoors, follow })
//   tasks: TASK(...) list; need: how many must be done before the place's marker shows (place null: the roam
//   ends when they are done); until: the clock eases from hour to until as tasks finish; banter: BT(...) list.
// TASK(kind, name, o): kind 'greet' | 'point' | 'answer' | 'carry' | 'fix' | 'signin' | 'errand'
//   o.who: who it is with (a cast id, may be an extra like 'walkin1'); o.at: where it is; o.to: second mark (carry
//   destination, point target); o.prompt: the button prompt; o.label: the task list line; o.n: count (carry trips);
//   o.mode: fix minigame 'timing' | 'mash' | 'hold' | 'stick'; o.reward: { trust: 1, vibe: 1 } (two stats at most);
//   o.before / o.after: beats said before the minigame and after success; o.optional; o.requires: another task's
//   name that must be done first; o.delay: seconds into the roam before it appears.
export const TASK = (kind, name, o = {}) => ({ kind, name, ...o });
// BT(who, text, { delay, near }): an ambient line while walking, no camera cut, a small subtitle
export const BT = (who, text, o = {}) => ({ who, text, ...o });
// BARK(who, text): a voiced line that does not stop the scene (said over whatever happens next)
export const BARK = (who, text) => ({ op: 'bark', who, text });
// SHOT types added: push react insert hero whip top. o.part: 'rhand' | 'lhand' | 'head' | 'point'; o.at: a mark for 'point'
// MUSIC moods added: hosting tension night zone final

// ---- v1.3: the bike in scenes, the nights
// BIKE(mark): the lab's loaner bike parked at a mark in a scene; BIKE(null) puts it away. BIKE_WITH(who): someone
// pushes it along beside them. BIKE_BACK(): it is the player's to ride again (the loaner, after it is won back).
// The flag 'has_bike' is set while the player has the loaner (taken at the bike station by 9 am, or given back).
export const BIKE = (mark) => ({ op: 'bike', mode: mark ? 1 : 0, mark });
export const BIKE_WITH = (who) => ({ op: 'bike', mode: 2, who });
export const BIKE_BACK = () => ({ op: 'bike', mode: 3 });
// ROAM o.prompt is the goal's label once the work is done ("Head back to the lab"): the objective line and the
// marker show it, and the roam ends when the player gets within the radius (no button). o.auto: seconds after the
// work is done before it moves on by itself, for a roam whose walk adds nothing.
// TASK o.with: who stays by the task's person (a parent by a kid). o.where, o.where2: short names of the task's
// places for the wayfinder ("Kitchen", "Soda fridge"; a carry shows pick up at where, drop off at where2).
// Task labels stay under 35 characters (the HUD list); the compiler checks.
// A night file exports NIGHT = { n, hour, title, sub, beats, fights, endings, stat_lines }; see night1.mjs.
export const EPILOGUE = () => ({ op: 'epilogue' });
// PROP(name, mark, mode): a small prop the story moves. 'gerald' (Ines's robot): mode 'still', 'circle' (one motor
// wired backwards: slow circles to the left), 'straight' (out and back), 'hide'. mark null keeps it where it is.
// 'table': the fix-it table. 'weld_table': the lab's welding table on its casters, as the back lot fight draws it
// (stand it where a FIGHT with obj_prop 'welding_table' puts its object, and hide it for the fight). 'van': Copper
// Cal's work van, its back doors open toward the mark's facing. Each is 'still' or 'hide'.
export const PROP = (name, mark, mode = 'still') => ({ op: 'prop', name, mark, mode });

// ---- v2.0: the attic (tools/script/attic.mjs)
// Its scenes are played by attic.c through the story VM and end by putting the day back as it was.
// STAGE('attic') the blueprint room, STAGE('attic_deep') the nightmare, STAGE('desert') the balloon launch's set.
// GRADE(look, t): 'memory' fades the flashback look in over t seconds, 'none' fades it out.
export const GRADE = (look, t = 1.2) => ({ op: 'grade', look, t });
// PROP names for the flashbacks: 'balloon' (the desert set's balloon: mode 'still' on the ground, 'circle' fills it,
// 'straight' lets it go), 'pink_front' (one forty's storefront in its pink coat), 'laser_crate', 'stage_robot',
// 'move_boxes' (the move into 108), 'haxmas' (lights and a tree in the lab), 'rack_2018' (the rack on a dolly).
