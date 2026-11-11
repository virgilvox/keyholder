/* street.c: see street.h. Four parts, each on its own:
 *   the day's memory (who you met, what they said, the marks talks leave),
 *   the cast (which stranger an extra plays) and the talk (a small machine over ST_BEATS, shown with
 *     STORY_UI's dialogue box and choice menu, shot by the director),
 *   the doors (the walk in buildings of interior.h: a prompt, a fade, in and out),
 *   the cameo (Moheeb and Luis at the Chupacabra patio table: the Coffee & Kill bodies posed sitting,
 *     a beer, a laptop, their chatter and their talks). */
#include "street.h"
#include "story.h"
#include "script.h"
#include "player.h"
#include "voice.h"
#include "director.h"
#include "interior.h"
#include "world.h"
#include "audio.h"
#include "fx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the data (tools/script/street_compile.mjs): included here, so the build needs no new file */
#include "gen/street_data.c"

static t3_scene *scene; static t3_camera *cam; static void (*occluders)(void);
static v3 fwd(float yaw) { return v3_(sinf(yaw), 0, cosf(yaw)); }
static float flat(v3 a, v3 b) { return hypotf(a.x - b.x, a.z - b.z); }
static float yaw_to(v3 from, v3 to) { return atan2f(to.x - from.x, to.z - from.z); }
static v3 cam_pos(void) { const t3_object *o = (const t3_object *)cam; return v3_(o->position.x, o->position.y, o->position.z); }
static bool roaming(void) { return story_state() == SR_ROAM; }
#ifdef KH_STREET_TEST
/* a test run (with KH_AUTO and KH_GEAR_TEST, which hold the autoplay off each outdoor roam for 45 s): a
 * script per roam, by its hour, of teleports and presses at seconds into the roam, logged by frame */
static intent_t test_in; static float test_t; static float test_h = -1; static int test_step, test_ok2;
static void test_drive(float dt);
static const intent_t *keys(const intent_t *in) { (void)in; return &test_in; }
#else
static const intent_t *keys(const intent_t *in) { return in; }
#endif

/* ---- who is playing: the name, the voice and the colour of you ---- */
static int me_w(void) {
  const char *n = story_player_name();
  return !strcmp(n, CAST[W_DEX].name) ? W_DEX : !strcmp(n, CAST[W_RUTH].name) ? W_RUTH : W_VEE;
}
static int me_k(void) { int w = me_w(); return w == W_DEX ? 1 : w == W_RUTH ? 2 : 0; }   /* the text column */
static void expand(char *o, size_t cap, const char *s) {
  size_t n = 0; const char *nm = story_player_name();
  for (const char *p = s; *p && n + 16 < cap;) {
    if (!strncmp(p, "{me}", 4)) { size_t l = strlen(nm); memcpy(o + n, nm, l); n += l; p += 4; }
    else o[n++] = *p++;
  }
  o[n] = 0;
}

/* ---- the day's memory ---- */
#define MAXT ST_MAX_TYPES
#define MAXK ST_MAX_TALKS
#define GIVE_CAP 4          /* stat points strangers give in a day, all together */
static uint8_t had[MAXK], met_n[MAXT];
static int bound[MAXT];     /* the body a stranger wears for the day, -1 until first cast */
static const char *marks[ST_MAX_MARKS]; static int nmark;
static int given, met_people;
static bool cameo_met;
static bool has_mark(const char *m) { for (int i = 0; i < nmark; i++) if (!strcmp(marks[i], m)) return true; return false; }
static void set_mark(const char *m) { if (!has_mark(m) && nmark < ST_MAX_MARKS) marks[nmark++] = m; }

/* ---- the venue: which downtown place the room you are in belongs to ---- */
#define MAXI 32
static const char *venue_key(int i) {
  static const char *key[MAXI]; static bool known[MAXI];
  if (i < 0 || i >= MAXI) return NULL;
  if (known[i]) return key[i];
  known[i] = true;
  const char *id = interior_id(i);   /* the town names each room by its facade's place */
  if (id && *id) return key[i] = id;
  v3 o, in; float yo, yi;
  if (!interior_door(i, &o, &yo, &in, &yi)) return NULL;
  float best = 15;
  for (int k = 0; k < ST_NVENUE; k++) { v3 p; float y; if (world_place(ST_VENUES[k], &p, &y) && flat(p, o) < best) { best = flat(p, o); key[i] = ST_VENUES[k]; } }
  return key[i];
}
static bool place(const char *name, v3 *p, float *y) { float yy; return world_place(name, p, y ? y : &yy); }
static bool venue_place_of(int i, const char *suffix, v3 *p, float *y) {
  const char *k = venue_key(i);
  if (!k) return false;
  char n[64]; snprintf(n, sizeof n, "%s_%s", k, suffix);
  return place(n, p, y);
}
static bool venue_place(const char *suffix, v3 *p, float *y) { return venue_place_of(interior_current(), suffix, p, y); }

/* ---- conditions ---- */
static bool fits(const st_talk *t, bool met) {
  const story_ui_t *U = &STORY_UI;
  if (U->hour < t->h0 || U->hour >= t->h1 || !(t->parts >> story_part() & 1)) return false;
  if (t->met >= 0 && (t->met == 1) != met) return false;
  if (t->flag && story_flag(t->flag) == t->flag_not) return false;
  if (t->mark && has_mark(t->mark) == t->mark_not) return false;
  if (t->stat >= 0 && (t->stat_low ? U->stats[t->stat] > t->stat_v : U->stats[t->stat] < t->stat_v)) return false;
  if (t->venue) { const char *k = venue_key(interior_current()); if (!k || strcmp(k, t->venue)) return false; }
  return true;
}
/* the first talk that fits and has not been had; else the first that fits and may be had again */
static int pick(int k0, int n, bool met) {
  for (int k = k0; k < k0 + n; k++) if (k < MAXK && !had[k] && fits(&ST_TALKS[k], met)) return k;
  for (int k = k0; k < k0 + n; k++) if (ST_TALKS[k].again && fits(&ST_TALKS[k], met)) return k;
  return -1;
}
static bool fresh(int t) { const st_type *T = &ST_TYPES[t]; for (int k = T->t0; k < T->t0 + T->nt; k++) if (k < MAXK && !had[k] && !ST_TALKS[k].again && fits(&ST_TALKS[k], met_n[t] > 0)) return true; return false; }

/* ---- the cast: which stranger each extra plays ---- */
#define SLOTS 16
static int cast[SLOTS] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
int street_cast(int slot, actor_model body, int where, unsigned seed, bool *alt) {
  if (slot < 0 || slot >= SLOTS) return -1;
  cast[slot] = -1;
  float h = STORY_UI.hour;
  int cand[MAXT], nc = 0, best = -1;
  for (int t = 0; t < ST_NTYPE && t < MAXT; t++) {
    const st_type *T = &ST_TYPES[t];
    if (!(T->where & where) || h < T->h0 || h >= T->h1 || !(T->parts >> story_part() & 1) || !(T->bodies >> body & 1)) continue;
    if (bound[t] >= 0 && bound[t] != (int)body) continue;
    bool taken = false; for (int s = 0; s < SLOTS; s++) if (cast[s] == t) taken = true;
    if (taken) continue;
    int score = (fresh(t) ? 2 : 0) + (met_n[t] ? 0 : 1);   /* someone with something new to say first */
    if (score > best) { best = score; nc = 0; }
    if (score == best) cand[nc++] = t;
  }
  if (!nc) return -1;
  int t = cand[seed % (unsigned)nc];
  cast[slot] = t; bound[t] = body;
  if (alt) *alt = ST_TYPES[t].look != 0;
  return t;
}
void street_uncast(int slot) { if (slot >= 0 && slot < SLOTS) cast[slot] = -1; }

/* who is about this frame (story.c's extras say, after this frame's talk ran: so it is last frame's list) */
static struct { int slot; actor_t *a; const char *clip; } NEAR[SLOTS]; static int nnear;
void street_people_begin(void) { nnear = 0; }
void street_person(int slot, actor_t *a, const char *clip) { if (nnear < SLOTS && slot >= 0 && slot < SLOTS && cast[slot] >= 0) { NEAR[nnear].slot = slot; NEAR[nnear].a = a; NEAR[nnear].clip = clip; nnear++; } }

/* ---- the cameo's state, used by the talk (the cameo section is further down) ---- */
static struct cameo_t {
  bool loaded, failed, shown;
  v3 table, seat[2], patio; float yaw[2], table_y, seat_y;
  t3_object *root[2], *model[2], *laptop, *glass, *furniture;
  float hips_h[2];
  int chat, chat_line; float chat_t, chat_len, chat_gap; bool chat_on;
  int speaking; float look_amt;   /* 0 Moheeb, 1 Luis, -1 nobody */
  float drink_t, drink_next, type_t;
} C = { .chat = -1, .speaking = -1 };
static void cameo_angle(int k, v3 *pos, v3 *look);

/* ---- the talk ---- */
enum { TK_OFF, TK_LINE, TK_CHOICE };
static struct {
  int st, pc, type, slot, after;   /* after: the beat to go on at once a spoken option ends */
  actor_t *a;                      /* the stranger (NULL: the cameo) */
  const char *rest;                /* the clip they rest in: seated or busy people keep at it */
  int who; float t, len; int shots;
  float hold;                      /* a short rest after a talk, so its last press does not start another */
  char line[512], name[64], opt[5][160];
  const char *spk;
} TK;
static float letterbox;
static bool chat_had[ST_MAX_CHAT];
bool street_busy(void);
static void cut(int who) {
  if (!cam) return;
  dir_spec s; memset(&s, 0, sizeof s);
  if (!TK.a) {   /* the cameo has no actor for the director: fixed angles round the table */
    int k = who == SP_MOHEEB ? 1 : who == SP_LUIS ? 2 : 0;
    if (TK.shots++ == 0) k = 0;
    s.type = SH_FIXED; s.fov = k ? 34 : 42; cameo_angle(k, &s.fpos, &s.flook);
    director_cut(&s);
    return;
  }
  if (occluders) occluders();
  actor_t *me = player_actor();
  /* a sidewalk is tight (poles, planters, trees): a steady two shot that holds through your lines, and
   * every third of theirs a medium close up on them */
  int n = TK.shots++;
  if (n > 0 && who == SP_ME) return;
  if (who == SP_THEM && n % 3 == 2) { s.type = SH_MCU; s.subj[0] = TK.a; s.subj[1] = me; s.nsubj = 2; }
  else { s.type = SH_TWO; s.subj[0] = me; s.subj[1] = TK.a; s.nsubj = 2; }
  director_cut(&s);
}
static void say(int who, const char *text) {
  story_ui_t *U = &STORY_UI;
  expand(TK.line, sizeof TK.line, text);
  int w = me_w();
  const char *spk; const char *nm; uint32_t col;
  if (who == SP_ME) { spk = WHO_ID[w]; nm = CAST[w].name; col = CAST[w].color; }
  else if (who == SP_THEM) { const st_type *T = &ST_TYPES[TK.type]; spk = T->speaker; nm = T->name; col = T->color; }
  else { const st_person *P = &ST_CAMEO[who == SP_LUIS]; spk = P->speaker; nm = P->name; col = P->color; }
  snprintf(TK.name, sizeof TK.name, "%s", nm);
  TK.len = voice_say(spk, TK.line) ? voice_seconds(spk, TK.line) : 0;
  if (TK.len <= 0) TK.len = fmaxf(1.6f, strlen(TK.line) / 15.f);
  TK.spk = spk; TK.who = who; TK.t = 0; TK.st = TK_LINE;
  U->talking = true; U->speaker = TK.name; U->speaker_col = col; U->line = TK.line; U->line_t = 0;
  U->line_cps = fmaxf(20, strlen(TK.line) / fmaxf(.5f, TK.len * .94f)); U->line_done = false;
  actor_t *me = player_actor();
  if (TK.a) {
    const char *r = TK.rest ? TK.rest : "Idle_Loop";
    if (!strncmp(r, "Sitting", 7)) actor_play(TK.a, who == SP_THEM ? "Sitting_Talking_Loop" : r, 1, true, .3f);
    else if (!strncmp(r, "Idle", 4) || !strncmp(r, "Walk", 4)) actor_play(TK.a, who == SP_THEM ? "Idle_Talking_Loop" : "Idle_Loop", 1, true, .3f);
    if (me) actor_play(me, who == SP_ME ? "Idle_Talking_Loop" : "Idle_Loop", 1, true, .3f);
  } else C.speaking = who == SP_MOHEEB ? 0 : who == SP_LUIS ? 1 : -1;
  cut(who);
}
static void finish(void) {
  story_ui_t *U = &STORY_UI;
  U->talking = false; U->choosing = false;   /* the extras region puts the stranger back to what they were doing */
  actor_t *me = player_actor(); if (me) { actor_play(me, "Idle_Loop", 1, true, .3f); actor_look(me, actor_head_pos(me), 0); actor_mouth(me, 0, 0); }
  director_stop();
  player_camera_snap();   /* back behind you, as after a task's talk (story.c OP_RET): the talk's last lens may stand across a bench or a wall */
  C.speaking = -1;
  TK.st = TK_OFF; TK.a = NULL; TK.hold = .5f;
}
/* run beats until one waits (a line, a choice) or the talk ends */
static void run(void) {
  story_ui_t *U = &STORY_UI;
  for (int guard = 0; guard < 64; guard++) {
    const st_beat *b = &ST_BEATS[TK.pc];
    switch (b->op) {
      case SB_SAY: TK.pc++; say(b->who, b->text[me_k()]); return;
      case SB_CHOICE: {
        U->choosing = true; U->prompt = b->text[0]; U->nopt = b->a < 5 ? b->a : 5; U->sel = 0; U->choose_t = 0;
        for (int i = 0; i < U->nopt; i++) { expand(TK.opt[i], sizeof TK.opt[i], ST_BEATS[TK.pc + 1 + i].text[me_k()]); U->opt[i] = TK.opt[i]; }
        U->talking = false; TK.st = TK_CHOICE;
        if (TK.a) cut(SP_THEM);
        return;
      }
      case SB_JUMP: TK.pc = b->a; break;
      case SB_ADD: {
        int n = b->b; if (given + n > GIVE_CAP) n = GIVE_CAP - given;
        if (n > 0 && b->a >= 0 && b->a < ST_COUNT) {
          U->stats[b->a] += n; given += n;
          const char *nm = story_stat_name(b->a);
          if (nm) { U->toast = nm; U->toast_n = n; U->toast2 = NULL; U->toast_t = 2.6f; }
        }
        TK.pc++; break;
      }
      case SB_MARK: set_mark(b->text[0]); TK.pc++; break;
      case SB_NOTE: U->note = b->text[0]; U->note_t = 4; TK.pc++; break;
      default: finish(); return;   /* SB_END */
    }
  }
  finish();
}
static void begin(int type, int slot, actor_t *a, const char *rest, int k) {
  memset(TK.opt, 0, sizeof TK.opt);
  TK.type = type; TK.slot = slot; TK.a = a; TK.rest = rest; TK.pc = ST_TALKS[k].start; TK.shots = 0; TK.after = -1;
  if (k < MAXK) had[k] = 1;
  if (type >= 0) { if (!met_n[type]++) met_people++; }
  else cameo_met = true;
  /* face each other */
  v3 pp = player_pos(), at = a ? actor_pos(a) : C.table;
  player_teleport(pp, yaw_to(pp, at));   /* they turn to you in story.c's extras region, unless they sit */
  run();
}
static void talk_frame(float dt, const intent_t *in) {
  story_ui_t *U = &STORY_UI;
  in = keys(in);
  actor_t *me = player_actor();
  if (TK.a) {
    actor_look(TK.a, v3_add(actor_head_pos(me), v3_(0, -.05f, 0)), .9f);
    if (TK.st == TK_LINE && TK.who == SP_THEM) { float o, sh; voice_mouth(&o, &sh); actor_mouth(TK.a, o, sh); }
    else actor_mouth(TK.a, 0, 0);
  }
  if (me) actor_look(me, TK.a ? actor_head_pos(TK.a) : v3_add(C.table, v3_(0, 1.0f, 0)), .8f);
  if (TK.st == TK_LINE) {
    TK.t += dt; U->line_t += dt;
    if (U->line_t * U->line_cps >= (float)strlen(U->line)) U->line_done = true;
    bool next = false;
    if ((in->ok || in->interact) && TK.t > .25f) { if (!U->line_done) { U->line_t = 1e6f; U->line_done = true; } else next = true; }
    else if (TK.t > TK.len + .35f && U->line_done) next = true;
    if (next) {
      voice_stop(); U->talking = false;
      if (TK.after >= 0) { TK.pc = TK.after; TK.after = -1; }
      run();
    }
  } else if (TK.st == TK_CHOICE) {
    U->choose_t += dt;
    if (in->nav_y) { U->sel = (U->sel + in->nav_y + U->nopt) % U->nopt; sfx(SFX_UI, 0, 1); }
    if ((in->ok || in->interact) && U->choose_t > .3f) {
      sfx(SFX_UI_OK, 0, 1);
      U->choosing = false;
      const st_beat *o = &ST_BEATS[TK.pc + 1 + U->sel];
      if (o->say[0]) { TK.after = o->a; say(SP_ME, o->say[me_k()]); }   /* the option is said aloud first */
      else { TK.pc = o->a; run(); }
    }
  }
  director_update(dt);
}

/* ---- the doors ---- */
static struct { int phase; float t; int target; char prompt[96]; } DR;   /* phase 1 fading out, 2 holding black */
static void door_switch(void) {
  v3 o, in; float yo, yi;
  if (DR.target >= 0) {
    if (!interior_door(DR.target, &o, &yo, &in, &yi)) return;
    if (player_riding()) player_ride(false);
    interior_enter(DR.target);
    player_teleport(v3_add(in, v3_scale(fwd(yi), 1.2f)), yi);
  } else {
    int i = interior_current();
    if (i >= 0 && interior_door(i, &o, &yo, &in, &yi)) { interior_exit(); player_teleport(o, yo + DS_PI); }
    else interior_exit();
  }
  player_camera_snap();
}
static bool door_frame(float dt) {
  story_ui_t *U = &STORY_UI;
  if (!DR.phase) return false;
  DR.t += dt;
  if (DR.phase == 1) {
    U->fade = fmaxf(U->fade, ds_clamp(DR.t / .3f, 0, 1));
    if (DR.t >= .3f) { door_switch(); DR.phase = 2; DR.t = 0; }
  } else {
    U->fade = 1;
    if (DR.t >= .15f) { DR.phase = 0; TK.hold = .6f; }   /* story_update eases the fade back in */
  }
  return true;
}
/* the door prompt in reach: going in, or out. -2 none. */
static int door_near(void) {
  v3 p = player_pos(), o, in; float yo, yi;
  int cur = interior_current();
  if (cur >= 0) {
    if (interior_door(cur, &o, &yo, &in, &yi) && flat(p, in) < 1.1f) { snprintf(DR.prompt, sizeof DR.prompt, "Go out"); return -1; }
    return -2;
  }
  if (world_in_lab(p)) return -2;
  int best = -2; float bd = 1.7f;
  for (int i = 0; i < interior_count(); i++) if (interior_door(i, &o, &yo, &in, &yi)) {
    float d = flat(p, o);
    if (d < bd && fabsf(o.y - p.y) < 1.5f) { bd = d; best = i; }
  }
  if (best >= 0) snprintf(DR.prompt, sizeof DR.prompt, "Go in: %s", interior_name(best));
  return best;
}

/* ---- the roam frame ---- */
static void cameo_frame(float dt);
static bool cameo_reach(void);
static void cameo_stop_chat(void);
bool street_busy(void) { return TK.st != TK_OFF || DR.phase != 0; }
void street_roam(float dt, const intent_t *in) {
  story_ui_t *U = &STORY_UI;
  if (!roaming()) return;
#ifdef KH_STREET_TEST
  test_drive(dt);
#endif
  if (door_frame(dt)) return;
  if (TK.st != TK_OFF) { talk_frame(dt, in); return; }
  in = keys(in);
  if (TK.hold > 0) { TK.hold -= dt; return; }
  /* the roam's own prompts come first: a task, a side talk, the way on, a minigame, carrying */
  if (U->prompt_use || (U->mg && U->mg != MG_GREET && U->mg != MG_POINT) || U->talking || U->choosing) return;
  /* a door, a stranger, or the patio table: the nearest */
  int door = door_near();
  v3 p = player_pos(), f = fwd(player_yaw());
  int who = -1; float wd = 2.1f;
  for (int i = 0; i < nnear; i++) {
    v3 q = actor_pos(NEAR[i].a); float d = flat(q, p);
    if (fabsf(q.y - p.y) > 1.2f) continue;
    v3 to = v3_norm(v3_(q.x - p.x, 0, q.z - p.z));
    if (d > 1.1f && v3_dot(to, f) < .2f) continue;   /* in front of you, unless right beside you */
    if (d < wd) { wd = d; who = i; }
  }
  bool table = cameo_reach();
  if (door >= -1 && (who < 0 || flat(p, actor_pos(NEAR[who].a)) > 1.2f)) {
    U->prompt_use = DR.prompt;
    if (in->interact) { DR.phase = 1; DR.t = 0; DR.target = door; voice_stop(); sfx(SFX_UI_OK, 0, .6f); }
    return;
  }
  if (who >= 0) {
    int slot = NEAR[who].slot, t = cast[slot];
    U->prompt_use = "Talk";
    if (in->interact) {
      int k = pick(ST_TYPES[t].t0, ST_TYPES[t].nt, met_n[t] > 0);
      if (k >= 0) { voice_stop(); begin(t, slot, NEAR[who].a, NEAR[who].clip, k); }
    }
    return;
  }
  if (table) {
    U->prompt_use = "Talk";
    if (in->interact) {
      int k = pick(ST_CTALK0, ST_NCTALK, cameo_met);
      if (k >= 0) { cameo_stop_chat(); voice_stop(); begin(-1, -1, NULL, NULL, k); }
    }
  }
}
bool street_holds(int slot) { return TK.st != TK_OFF && TK.slot == slot && TK.a; }

void street_after(float dt) {
  story_ui_t *U = &STORY_UI;
  if (street_busy()) {
    U->roam = false;   /* the roam's HUD steps aside for the talk */
    letterbox = ds_damp(letterbox, TK.st != TK_OFF ? .55f : 0, 7, dt);
    U->letterbox = fmaxf(U->letterbox, letterbox);
  } else letterbox = U->letterbox;
  /* a room left open while the story moved you elsewhere closes behind you */
  if (!roaming() && !DR.phase && interior_current() >= 0 && !interior_contains(player_pos())) interior_exit();
  /* no riding indoors */
  if (interior_current() >= 0 && player_riding()) { player_ride(false); U->note = "The bike waits outside."; U->note_t = 2.5f; }
  cameo_frame(dt);
}

/* a task's spot: an errand at the door of a venue you can walk into is done at its counter, inside
 * (the marker leads in; the door's prompt is not hidden under the errand's) */
v3 street_task_point(v3 p) {
  for (int i = 0; i < interior_count(); i++) {
    v3 o, in, c, vp; float yo, yi, y;
    const char *k = venue_key(i);
    if (!k || !interior_door(i, &o, &yo, &in, &yi)) continue;
    bool near = flat(p, o) < 5 || (world_place(k, &vp, &y) && flat(p, vp) < 5);
    if (near && venue_place_of(i, "counter", &c, &y)) return c;
  }
  return p;
}

/* the room whose counter p is (a task's spot street_task_point moved inside), or -1 */
int street_room_of(v3 p) {
  for (int i = 0; i < interior_count(); i++) { v3 c; float y; if (venue_place_of(i, "counter", &c, &y) && flat(c, p) < .5f) return i; }
  return -1;
}
/* the people inside a venue: at its named spots (interior.h), staff behind the counter or the bar, the rest
 * where customers stand and sit; else spread out from the door. The spot you stand at to be served
 * (<id>_counter) stays free for you. pos.y is the floor, or a seat's top for a seat. */
int street_inside_spots(v3 *pos, float *yaw, bool *behind, int cap) {
  int n = 0, i = interior_current(); v3 p; float y;
  if (i < 0) return 0;
  for (int pass = 0; pass < 2; pass++)   /* the staff first, so the first person cast stands behind the counter */
    for (int k = 0; k < interior_spot_count(i) && n < cap; k++) {
      const char *nm = interior_spot_name(i, k);
      if (!nm || strstr(nm, "_inside") || strstr(nm, "_counter") || strstr(nm, "_stage") || !place(nm, &p, &y)) continue;
      bool staff = strstr(nm, "baker") || strstr(nm, "bartender") || strstr(nm, "box_office");
      if (staff != (pass == 0)) continue;
      pos[n] = p; yaw[n] = y; behind[n] = staff; n++;
    }
  v3 o, in; float yo, yi;
  if (n >= 2 || !interior_door(i, &o, &yo, &in, &yi)) return n;
  /* spread out in front of the door, facing the room's middle */
  static const float G[][2] = { {3.0f, -1.4f}, {4.2f, 1.3f}, {2.4f, 1.6f}, {5.0f, -.8f} };
  v3 f = fwd(yi), r = v3_(f.z, 0, -f.x);
  for (unsigned k = 0; k < sizeof G / sizeof *G && n < cap; k++) {
    v3 q = v3_add(in, v3_add(v3_scale(f, G[k][0]), v3_scale(r, G[k][1])));
    float g = world_floor_y(v3_(q.x, in.y + .6f, q.z));
    if (g < -1e8f || fabsf(g - in.y) > .4f) continue;
    q.y = g; pos[n] = q; yaw[n] = yaw_to(q, v3_add(in, v3_scale(f, 3.5f))); behind[n] = false; n++;
  }
  return n;
}

int street_end_lines(const char **out, int n, int cap) {
  static char buf[96];
  if (met_people > 0 && n < cap) {
    if (met_people == 1) snprintf(buf, sizeof buf, "You stopped to talk with a stranger on Main Street.");
    else snprintf(buf, sizeof buf, "You stopped to talk with %d people downtown who were not on the schedule.", met_people);
    out[n++] = buf;
  }
  if (cameo_met && n < cap) out[n++] = "Moheeb and Luis are still out on the Chupacabra patio, talking about solder.";
  return n;
}

void street_init(t3_scene *s, t3_camera *c, void (*occ)(void)) { scene = s; cam = c; occluders = occ; street_reset(false); }
void street_reset(bool keep_friends) {
  memset(had, 0, sizeof had); memset(met_n, 0, sizeof met_n); nmark = 0; given = 0; met_people = 0;
  if (!keep_friends) cameo_met = false;   /* Moheeb and Luis remember you from one evening to the next */
  for (int t = 0; t < MAXT; t++) bound[t] = -1;
  for (int s = 0; s < SLOTS; s++) cast[s] = -1;
  if (TK.st != TK_OFF) { STORY_UI.talking = STORY_UI.choosing = false; TK.st = TK_OFF; }
  TK.a = NULL; TK.hold = 0; DR.phase = 0; nnear = 0;
  C.chat = -1; C.chat_on = false; C.chat_gap = 3; memset(chat_had, 0, sizeof chat_had);
}
void street_stop(void) {
  if (TK.st != TK_OFF) { STORY_UI.talking = STORY_UI.choosing = false; TK.st = TK_OFF; TK.a = NULL; }
  DR.phase = 0;
  if (interior_current() >= 0) interior_exit();
  for (int i = 0; i < 2; i++) if (C.root[i]) C.root[i]->visible = false;
  if (C.laptop) C.laptop->visible = false;
  if (C.glass) C.glass->visible = false;
  if (C.furniture) C.furniture->visible = false;
  C.shown = false; C.chat_on = false;
}

/* ================================================================ the cameo
 * Moheeb and Luis are the Coffee & Kill rigs from SMALL PARTS (tools/prep-cameo.mjs makes
 * assets/models/cameo_*.glb from them), not VRoid bodies, so they skip actor.c: the glTF is parsed
 * on first approach (not at boot), drawn with the characters' cel lambert, and posed here every frame
 * from the rest pose. Each limb bone is turned in world space so its bone (from it to its child) points
 * where it should: thighs forward, shins down, arms to the laptop or the glass. That works whatever
 * axes the scan's rig uses. The heads look at whoever talks, and nod while they talk (no face morphs). */
enum { B_HIPS, B_SPINE, B_SPINE1, B_SPINE2, B_NECK, B_HEAD, B_LARM, B_LFORE, B_LHAND, B_RARM, B_RFORE, B_RHAND,
  B_LTHIGH, B_LSHIN, B_LFOOT, B_LTOE, B_RTHIGH, B_RSHIN, B_RFOOT, B_RTOE, NB };
static const char *const BONE[NB] = { "Hips", "Spine", "Spine01", "Spine02", "neck", "Head", "LeftArm", "LeftForeArm", "LeftHand",
  "RightArm", "RightForeArm", "RightHand", "LeftUpLeg", "LeftLeg", "LeftFoot", "LeftToeBase", "RightUpLeg", "RightLeg", "RightFoot", "RightToeBase" };
static t3_object *bone[2][NB]; static t3_quat rest[2][NB];
#define CAMEO_SCALE .97f    /* both scans stand 1.74 m; a touch under, beside a cast of 1.6 m */
#define SEAT_H .46f
#define TABLE_H .74f

static t3_quat world_rot(t3_object *o) {
  t3_mat4 m = o->matrix_world;
  for (int c = 0; c < 3; c++) {
    float l = sqrtf(m.e[c * 4] * m.e[c * 4] + m.e[c * 4 + 1] * m.e[c * 4 + 1] + m.e[c * 4 + 2] * m.e[c * 4 + 2]);
    if (l > 1e-8f) for (int r = 0; r < 3; r++) m.e[c * 4 + r] /= l;
  }
  return t3_quat_from_rotation_matrix(&m);
}
static t3_quat qinv(t3_quat q) { t3_quat r = {-q.x, -q.y, -q.z, q.w}; return r; }
static t3_quat from_to(t3_vec3 a, t3_vec3 b) {
  float r = t3_vec3_dot(a, b) + 1; t3_quat q;
  if (r < 1e-6f) q = fabsf(a.x) > fabsf(a.z) ? (t3_quat){-a.y, a.x, 0, 0} : (t3_quat){0, -a.z, a.y, 0};
  else { t3_vec3 c = t3_vec3_cross(a, b); q = (t3_quat){c.x, c.y, c.z, r}; }
  return t3_quat_normalize(q);
}
static t3_vec3 tv(v3 v) { return t3_v3(v.x, v.y, v.z); }
static v3 wpos(t3_object *o) { t3_vec3 w = t3_object_get_world_position(o); return v3_(w.x, w.y, w.z); }
/* set a bone's world rotation to nw (its parent's world matrix current) and bring its children along */
static void set_world(t3_object *b, t3_quat nw) {
  t3_object_set_quaternion(b, t3_quat_normalize(t3_quat_multiply(qinv(world_rot(b->parent)), nw)));
  t3_object_update_world_matrix(b, false, true);
}
/* turn bone b so the line from it to child points along dir (world) */
static void aim(t3_object *b, t3_object *child, v3 dir, float amount) {
  if (!b || !child || amount <= 0) return;
  v3 axis = v3_norm(v3_sub(wpos(child), wpos(b)));
  t3_quat qw = world_rot(b);
  set_world(b, t3_quat_slerp(qw, t3_quat_multiply(from_to(tv(axis), tv(v3_norm(dir))), qw), amount));
}
/* turn bone b about a world axis */
static void turn(t3_object *b, v3 axis, float ang) {
  if (!b || fabsf(ang) < 1e-5f) return;
  set_world(b, t3_quat_multiply(t3_quat_from_axis_angle(tv(v3_norm(axis)), ang), world_rot(b)));
}

static void ck_mesh(t3_object *o, void *ctx) {
  (void)ctx;
  if (o->type != T3_MESH && o->type != T3_SKINNED_MESH) return;
  o->frustum_culled = false; o->cast_shadow = false; o->receive_shadow = false;
  t3_mesh *m = (t3_mesh *)o;
  for (int i = 0; i < m->material_count; i++) {
    t3_material *old = m->materials[i]; if (!old) continue;
    t3_texture *T = old->map;
    if (T) { T->generate_mipmaps = false; T->min_filter = T3_LINEAR; T->mag_filter = T3_LINEAR; T->wrap_s = T->wrap_t = T3_CLAMP_TO_EDGE; T->version++; }   /* island packed: no mips; clamped like the cast's atlases (the same shader state) */
    t3_material *M = t3_mesh_lambert_material_new(0xffffff);   /* the characters' cel state: lambert+map+fog+skin+atest+ds */
    if (T) t3_material_set_map(M, T);
    M->alpha_test = .45f; M->side = T3_DOUBLE_SIDE; M->fog = true;
    m->materials[i] = M; t3_release(old);
  }
}
static bool cameo_load(void) {
  static const char *const GLB[2] = { "cameo_moheeb.glb", "cameo_luis.glb" };
  for (int k = 0; k < 2; k++) {
    size_t n; uint8_t *b = ds_asset(GLB[k], &n);
    t3_gltf *g = b ? t3_gltf_parse(b, n, NULL, NULL) : NULL;
    free(b);
    if (!g) { ds_log("street: no %s", GLB[k]); return false; }
    C.model[k] = g->scene;    /* the glTF record stays: it owns the parse */
    C.root[k] = t3_group_new(); t3_object_add(&scene->base, C.root[k]); t3_release(C.root[k]);
    t3_object_add(C.root[k], C.model[k]);
    t3_object_set_scale(C.model[k], CAMEO_SCALE, CAMEO_SCALE, CAMEO_SCALE);
    t3_object_traverse(C.model[k], ck_mesh, NULL);
    for (int i = 0; i < NB; i++) {
      bone[k][i] = t3_object_get_by_name(C.model[k], BONE[i]);
      if (bone[k][i]) rest[k][i] = bone[k][i]->quaternion; else ds_log("street: %s has no bone %s", GLB[k], BONE[i]);
    }
    t3_object_update_world_matrix(C.root[k], false, true);
    C.hips_h[k] = bone[k][B_HIPS] ? wpos(bone[k][B_HIPS]).y : .93f;
    C.root[k]->visible = false;
  }
  return true;
}
/* the props, PS2 style like the bike: vertex coloured boxes and cylinders */
static t3_object *prop_end(void) { t3_object *o = fxp_end(); o->visible = false; return o; }
static void cameo_props(bool furniture) {
  /* the laptop, open, in its own frame: you sit at -z, the screen rises at the back */
  fxp_begin();
  fxp_box(v3_(0, .009f, 0), v3_(.32f, .018f, .22f), 0x2c2f36);
  fxp_box(v3_(0, .019f, -.02f), v3_(.27f, .002f, .1f), 0x16181d);   /* keys */
  fxp_box(v3_(0, .019f, -.085f), v3_(.08f, .002f, .05f), 0x3a3e46); /* trackpad */
  fxp_quad(v3_(-.16f, .018f, .105f), v3_(.16f, .018f, .105f), v3_(.16f, .222f, .16f), v3_(-.16f, .222f, .16f), 0x2c2f36);   /* the lid's back */
  fxp_box(v3_(.05f, .14f, .14f), v3_(.06f, .06f, .004f), 0xe8642a);   /* a HeatSync orange sticker on the lid */
  fxp_layer(FXP_GLOW);
  fxp_quad(v3_(.15f, .028f, .1f), v3_(-.15f, .028f, .1f), v3_(-.15f, .21f, .148f), v3_(.15f, .21f, .148f), 0x8fb4d8);   /* the screen */
  C.laptop = prop_end();
  /* the beer: a pint glass of amber with a head of foam */
  fxp_begin();
  fxp_cyl(v3_(0, 0, 0), .13f, .029f, .036f, 10, 0xc98a26);
  fxp_cyl(v3_(0, .13f, 0), .022f, .036f, .037f, 10, 0xf2ead8);
  C.glass = prop_end(); C.glass->rotation.order = T3_YXZ;
  if (!furniture) return;
  /* the patio set, when the town has none: a round metal table and two chairs */
  fxp_begin();
  fxp_cyl(v3_(0, 0, 0), .02f, .24f, .24f, 12, 0x2a2c30);
  fxp_cyl(v3_(0, .02f, 0), TABLE_H - .05f, .03f, .03f, 8, 0x2a2c30);
  fxp_cyl(v3_(0, TABLE_H - .03f, 0), .03f, .4f, .4f, 16, 0x3a3d42);
  for (int s = 0; s < 2; s++) {
    v3 c = v3_sub(C.seat[s], C.table); float y = C.yaw[s];
    v3 f = fwd(y), r = v3_(f.z, 0, -f.x);
    v3 back = v3_sub(c, v3_scale(f, .2f));
    fxp_box(v3_add(c, v3_(0, SEAT_H - .02f, 0)), v3_(.44f, .04f, .44f), 0x3a3d42);
    for (int lx = -1; lx <= 1; lx += 2) for (int lz = -1; lz <= 1; lz += 2) {
      v3 l = v3_add(c, v3_add(v3_scale(r, lx * .19f), v3_scale(f, lz * .19f)));
      fxp_bar(l, v3_add(l, v3_(0, SEAT_H - .04f, 0)), .025f, 0x2a2c30);
    }
    for (int lx = -1; lx <= 1; lx += 2) { v3 l = v3_add(back, v3_scale(r, lx * .19f)); fxp_bar(v3_add(l, v3_(0, SEAT_H, 0)), v3_add(l, v3_(0, SEAT_H + .42f, 0)), .025f, 0x2a2c30); }
    v3 bt = v3_add(back, v3_(0, SEAT_H + .34f, 0));
    fxp_bar(v3_sub(bt, v3_scale(r, .2f)), v3_add(bt, v3_scale(r, .2f)), .06f, 0x3a3d42);
  }
  C.furniture = prop_end();
  t3_object_set_position(C.furniture, C.table.x, C.table.y, C.table.z);
}
/* where everything is. The town's picnic table (facades_town.inc): chupacabra_table is the top's centre
 * with y its height, chupacabra_seat_a and _b the ground under each bench's middle facing the table,
 * chupacabra_patio standing at the table's end. Without them: a table of our own on the sidewalk by the
 * taproom's door, a chair either side. */
static void cameo_layout(bool *has_table) {
  float y;
  v3 top;
  *has_table = world_place("chupacabra_table", &top, &y);
  bool sa = world_place("chupacabra_seat_a", &C.seat[0], &y), sb = world_place("chupacabra_seat_b", &C.seat[1], &y);
  if (*has_table && sa && sb) {
    C.table = v3_(top.x, C.seat[0].y, top.z); C.table_y = top.y;
    C.seat_y = .47f;   /* the bench top */
  } else {
    *has_table = false;
    v3 d; float dy;
    if (!world_place("chupacabra", &d, &dy)) { d = v3_(-221.9f, 0, -41.8f); dy = -DS_PI / 2; }
    /* the place stands in front of the door facing it: the table a step out from it and three along */
    v3 f = fwd(dy), r = v3_(f.z, 0, -f.x);
    C.table = v3_add(d, v3_add(v3_scale(f, -.6f), v3_scale(r, 3.2f)));
    float g = world_floor_y(v3_(C.table.x, C.table.y + .3f, C.table.z));
    if (g > -1e8f && fabsf(g - C.table.y) < .5f) C.table.y = g;
    C.table_y = C.table.y + TABLE_H;
    C.seat[0] = v3_add(C.table, v3_scale(r, -.68f)); C.seat[1] = v3_add(C.table, v3_scale(r, .68f));   /* across the table, along the storefront */
    C.seat_y = SEAT_H;
  }
  if (!world_place("chupacabra_patio", &C.patio, &y)) C.patio = C.table;
  for (int s = 0; s < 2; s++) C.yaw[s] = yaw_to(C.seat[s], C.table);
}
static void cameo_angle(int k, v3 *pos, v3 *look) {
  v3 a = C.seat[0], b = C.seat[1], u = v3_norm(v3_sub(b, a)), n = v3_(u.z, 0, -u.x);
  if (v3_dot(n, v3_sub(player_pos(), C.table)) < 0) n = v3_scale(n, -1);
  v3 head[2] = { v3_add(a, v3_(0, 1.18f, 0)), v3_add(b, v3_(0, 1.18f, 0)) };
  if (k == 0) {   /* a wide three quarter view from your end, off to the side so you do not stand in it */
    v3 side = v3_scale(v3_norm(v3_sub(player_pos(), C.table)), 1);
    float sgn = v3_dot(side, u) > 0 ? -1 : 1;
    *pos = v3_add(C.table, v3_add(v3_add(v3_scale(n, 2.6f), v3_scale(u, sgn * 2.0f)), v3_(0, 1.5f, 0))); *look = v3_add(C.table, v3_(0, .9f, 0));
  }
  else if (k == 1) { *pos = v3_add(b, v3_add(v3_add(v3_scale(u, .55f), v3_scale(n, .55f)), v3_(0, 1.32f, 0))); *look = head[0]; }   /* over Luis onto Moheeb */
  else { *pos = v3_add(a, v3_add(v3_add(v3_scale(u, -.55f), v3_scale(n, .55f)), v3_(0, 1.32f, 0))); *look = head[1]; }       /* over Moheeb onto Luis */
}
/* sit one of them down and set the arms to what they are doing; returns the right hand's place */
static v3 cameo_pose(int k, float t) {
  t3_object **B = bone[k];
  for (int i = 0; i < NB; i++) if (B[i]) t3_object_set_quaternion(B[i], rest[k][i]);
  float y = C.yaw[k];
  v3 f = fwd(y), r = v3_(-f.z, 0, f.x), up = v3_(0, 1, 0);   /* r: the sitter's right */
  /* the hips just over the seat: the root goes below the floor, the legs fold down to it */
  v3 at = v3_add(C.seat[k], v3_scale(f, -.04f));
  t3_object_set_position(C.root[k], at.x, C.seat[k].y + C.seat_y + .08f - C.hips_h[k], at.z);
  t3_object_set_rotation(C.root[k], 0, y, 0);
  t3_object_update_world_matrix(C.root[k], false, true);
  /* breathing, and Moheeb leaning in to the screen */
  float breathe = sinf(t * 1.7f + k) * .02f, lean = k == 0 ? .14f : .04f;
  if (k == 0 && C.speaking == 0) lean = .02f;
  turn(B[B_SPINE], r, -(lean + breathe));   /* about the right axis, a negative turn leans in */
  for (int s = 0; s < 2; s++) {
    float side = s ? 1 : -1;   /* left, right */
    v3 out = v3_scale(r, side);
    aim(B[s ? B_RTHIGH : B_LTHIGH], B[s ? B_RSHIN : B_LSHIN], v3_add(v3_add(f, v3_scale(out, .14f)), v3_scale(up, -.1f)), 1);
    aim(B[s ? B_RSHIN : B_LSHIN], B[s ? B_RFOOT : B_LFOOT], v3_add(v3_scale(up, -1), v3_add(v3_scale(f, .18f), v3_scale(out, .04f))), 1);
    aim(B[s ? B_RFOOT : B_LFOOT], B[s ? B_RTOE : B_LTOE], v3_add(f, v3_scale(up, -.35f)), .8f);
  }
  /* the arms */
  v3 hand = v3_add(at, v3_add(v3_scale(f, .35f), v3_(0, C.seat_y + .3f, 0)));
  for (int s = 0; s < 2; s++) {
    float side = s ? 1 : -1; v3 out = v3_scale(r, side);
    t3_object *ua = B[s ? B_RARM : B_LARM], *fa = B[s ? B_RFORE : B_LFORE], *hd = B[s ? B_RHAND : B_LHAND];
    v3 upper = v3_add(v3_scale(up, -1), v3_add(v3_scale(f, .42f), v3_scale(out, .14f)));
    v3 fore = v3_add(f, v3_add(v3_scale(up, -.18f), v3_scale(out, -.32f)));
    if (k == 0) {   /* typing: the hands in toward the keys, a little patter */
      float tp = C.speaking == 0 ? 0 : 1;
      fore = v3_add(fore, v3_scale(up, tp * .05f * sinf(t * (11 + s * 3.7f))));
      if (C.speaking == 0 && s == 1) { upper = v3_add(v3_scale(up, -1), v3_add(v3_scale(f, .5f), v3_scale(out, .3f))); fore = v3_add(v3_add(f, v3_scale(up, .55f + .15f * sinf(t * 2.3f))), v3_scale(out, .2f)); }   /* talking with a hand */
    } else if (s == 1) {   /* Luis's right hand holds the glass; now and then he drinks */
      float d = C.drink_t > 0 ? sinf(ds_clamp(C.drink_t / 2.4f, 0, 1) * DS_PI) : 0;
      fore = v3_lerp(v3_add(f, v3_add(v3_scale(up, .12f), v3_scale(out, -.1f))), v3_add(v3_scale(up, 1.2f), v3_scale(out, -.45f)), d);
      upper = v3_lerp(upper, v3_add(v3_scale(up, -.6f), v3_add(f, v3_scale(out, .1f))), d * .7f);
    } else fore = v3_add(f, v3_add(v3_scale(up, -.05f), v3_scale(out, -.4f)));   /* the other arm on the table */
    aim(ua, fa, upper, 1);
    aim(fa, hd, fore, 1);
    if (s == 1 && hd) hand = wpos(hd);
  }
  /* the head: at whoever is talking, or the screen, or you */
  v3 head = B[B_HEAD] ? wpos(B[B_HEAD]) : v3_add(C.seat[k], v3_(0, 1.2f, 0));
  v3 tgt;
  int other = 1 - k;
  if (TK.st != TK_OFF && !TK.a) tgt = v3_add(player_pos(), v3_(0, 1.45f, 0));
  else if (C.speaking == other || (C.chat_on && C.speaking < 0)) tgt = v3_add(C.seat[other], v3_(0, 1.15f, 0));
  else if (k == 1 && flat(player_pos(), C.table) < 3.2f) tgt = v3_add(player_pos(), v3_(0, 1.45f, 0));   /* Luis looks up when you come over */
  else if (k == 0) tgt = v3_add(C.table, v3_add(v3_scale(fwd(C.yaw[0]), -.05f), v3_(0, C.table_y - C.table.y + .1f, 0)));
  else tgt = v3_add(C.seat[other], v3_(0, 1.1f, 0));
  if (k == 1 && C.drink_t > .6f && C.drink_t < 1.8f) tgt = v3_add(head, v3_add(f, v3_(0, .5f, 0)));
  v3 d = v3_sub(tgt, head);
  float yw = ds_clamp(ds_wrap_pi(atan2f(d.x, d.z) - y), -1.1f, 1.1f);
  float pt = ds_clamp(atan2f(d.y, hypotf(d.x, d.z)), -.6f, .4f);
  float o = 0, sh; if (C.speaking == k && voice_busy()) voice_mouth(&o, &sh);
  pt += o * .1f;   /* a nod on the words */
  turn(B[B_NECK], up, yw * .4f); turn(B[B_HEAD], up, yw * .6f);
  v3 hr = v3_(-cosf(y + yw), 0, sinf(y + yw));   /* the right axis once turned: a pitch down is a turn about it */
  turn(B[B_HEAD], hr, pt * .8f);
  return hand;
}
static void cameo_show(bool on) {
  C.shown = on;
  for (int k = 0; k < 2; k++) if (C.root[k]) C.root[k]->visible = on;
  if (C.laptop) C.laptop->visible = on;
  if (C.glass) C.glass->visible = on;
  if (C.furniture) C.furniture->visible = on;
}
static bool cameo_hours(void) { float h = STORY_UI.hour; return h >= ST_CAMEO_H0 && h < ST_CAMEO_H1; }
static bool cameo_reach(void) {
  return C.shown && TK.st == TK_OFF && interior_current() < 0 && (flat(player_pos(), C.table) < 2.3f || flat(player_pos(), C.patio) < 1.6f);
}
static void cameo_stop_chat(void) { if (C.chat_on && voice_busy()) voice_stop(); C.chat_on = false; C.chat = -1; C.speaking = -1; C.chat_gap = 4; }
static void cameo_chat(float dt) {
  story_ui_t *U = &STORY_UI;
  bool near = roaming() && interior_current() < 0 && flat(player_pos(), C.table) < 9 && TK.st == TK_OFF;
  if (C.chat_on) {
    C.chat_t += dt;
    if (C.chat_t < C.chat_len + .45f) return;
    const st_talk *T = &ST_TALKS[ST_CHAT0 + C.chat];
    int pc = T->start + C.chat_line;
    if (!near || ST_BEATS[pc].op != SB_SAY) { C.chat_on = false; C.speaking = -1; C.chat_gap = ds_rr(7, 12); return; }
  } else {
    if (!near) return;
    if ((C.chat_gap -= dt) > 0 || U->sub_t > 0 || voice_busy() || U->talking) return;
    C.chat = -1;
    for (int i = 0; i < ST_NCHAT && i < ST_MAX_CHAT; i++) if (!chat_had[i] && fits(&ST_TALKS[ST_CHAT0 + i], cameo_met)) { C.chat = i; break; }
    if (C.chat < 0) return;
    chat_had[C.chat] = true; C.chat_on = true; C.chat_line = 0;
  }
  /* the next line: a subtitle and the voice */
  static char text[320];
  const st_beat *b = &ST_BEATS[ST_TALKS[ST_CHAT0 + C.chat].start + C.chat_line++];
  const st_person *P = &ST_CAMEO[b->who == SP_LUIS];
  expand(text, sizeof text, b->text[me_k()]);
  C.chat_len = voice_say(P->speaker, text) ? voice_seconds(P->speaker, text) : 0;
  if (C.chat_len <= 0) C.chat_len = fmaxf(1.6f, strlen(text) / 15.f);
  C.chat_t = 0; C.speaking = b->who == SP_LUIS;
  U->sub_name = P->name; U->sub_text = text; U->sub_col = P->color; U->sub_t = C.chat_len + .5f; U->sub_dur = C.chat_len + .5f;
}
static void cameo_frame(float dt) {
  if (C.failed || !scene) return;
  bool town = world_get_stage() == STAGE_TOWN;
  bool want = town && cameo_hours();
  if (want && !C.loaded) {
    /* load at the first town roam in their hours (not at boot), so they are already at the table when
     * you come down Robson or see the patio from Main */
    if (!roaming()) return;
    if (!cameo_load()) { C.failed = true; return; }
    bool has_table; cameo_layout(&has_table);
    cameo_props(!has_table);
    C.loaded = true; C.drink_next = 6;
    ds_log("street: the cameo is at the Chupacabra patio (%.1f, %.1f), %s table, top %.2f, seats (%.1f %.1f) (%.1f %.1f) %.2f high",
      C.table.x, C.table.z, has_table ? "the town's" : "a fallback", C.table_y, C.seat[0].x, C.seat[0].z, C.seat[1].x, C.seat[1].z, C.seat_y);
  }
  if (!C.loaded) return;
  bool vis = want && flat(cam_pos(), C.table) < 120;
  if (vis != C.shown) cameo_show(vis);
  if (!vis) { if (C.chat_on) cameo_stop_chat(); return; }
  static float t; t += dt;
  if (C.drink_t > 0) { C.drink_t += dt; if (C.drink_t > 2.4f) { C.drink_t = 0; C.drink_next = ds_rr(9, 18); } }
  else if (C.speaking != 1 && (C.drink_next -= dt) <= 0) C.drink_t = 1e-3f;
  cameo_chat(dt);
  if (C.speaking >= 0 && !voice_busy() && !C.chat_on && TK.st == TK_OFF) C.speaking = -1;
  if (flat(cam_pos(), C.table) > 45) return;   /* too far to see a pose change */
  cameo_pose(0, t);
  v3 hand = cameo_pose(1, t);
  /* the laptop on the table in front of Moheeb, the glass in Luis's hand */
  v3 lp = v3_add(C.table, v3_scale(fwd(C.yaw[0]), -.2f));
  t3_object_set_position(C.laptop, lp.x, C.table_y, lp.z); t3_object_set_rotation(C.laptop, 0, C.yaw[0], 0);
  float tilt = C.drink_t > 0 ? sinf(ds_clamp(C.drink_t / 2.4f, 0, 1) * DS_PI) * .9f : 0;
  v3 gp = v3_add(hand, v3_(0, -.06f, 0));
  if (gp.y < C.table_y) gp.y = C.table_y;
  t3_object_set_position(C.glass, gp.x, gp.y, gp.z); t3_object_set_rotation(C.glass, tilt, C.yaw[1], 0);
}

#ifdef KH_STREET_TEST
static int frame_now(void) { return (int)(ds_now() * 60 + .5); }
static void tp(v3 p, float yaw) { player_teleport(p, yaw); player_camera_snap(); ds_log("[street] frame %d: teleport %.1f %.1f", frame_now(), p.x, p.z); }
static void test_drive(float dt) {
  memset(&test_in, 0, sizeof test_in);
  { const intent_t *r = input_get(); test_in.nav_y = r->nav_y; test_in.ok = r->ok; test_in.interact = r->interact; }
  if (test_ok2 > 0 && --test_ok2 == 0) test_in.ok = true;   /* a second press: the first shows the whole line */
  static int last = -9;
  if (frame_now() > last + 2) { test_h = STORY_UI.hour; test_t = 0; test_step = 0; ds_log("[street] frame %d: roam at %.2f h", frame_now(), test_h); }
  last = frame_now();
  float h = test_h;
  test_t += dt;
  /* the steps: at t seconds, do what */
  typedef struct { float t; int act; } step_t;   /* act: 1 by the Nile stander, 2 to the Proof door, 3 to its counter, 4 to the way out,
                                                      5 to the patio, 6 to the electronics bench, 7 before the nearest stranger on Main,
                                                      8 before the nearest stranger in the room, 10 press use, 11 press ok (twice) */
  static const step_t LUNCH[] = { {.5f, 7}, {1.2f, 7}, {1.6f, 10}, {3.5f, 11}, {5.5f, 11}, {7.5f, 11}, {9.5f, 11}, {11.5f, 11}, {13.5f, 11}, {15.5f, 11}, {17.5f, 11},
    {20, 2}, {21.5f, 10}, {24, 3}, {25.5f, 10}, {27, 8}, {28, 10}, {30, 11}, {32, 11}, {34, 11}, {36, 4}, {37.5f, 10}, {-1, 0} };
  static const step_t PATIO[] = { {.5f, 5}, {16, 10}, {18, 11}, {20.5f, 11}, {23, 11}, {25.5f, 11}, {28, 11}, {30.5f, 11}, {33, 11}, {35.5f, 11}, {-1, 0} };
  static const step_t LAB[] = { {.5f, 6}, {1.5f, 10}, {4, 11}, {6.5f, 11}, {9, 11}, {11.5f, 11}, {-1, 0} };
  const step_t *S = h > 12.4f && h < 12.6f ? LUNCH : h > 17.9f && h < 18.1f ? PATIO : h > 8.9f && h < 9.1f ? LAB : NULL;
  if (!S) return;
  for (; S[test_step].t >= 0 && test_t >= S[test_step].t; test_step++) {
    int a = S[test_step].act; v3 p, o, in; float y, yo, yi;
    ds_log("[street] frame %d: step %d act %d (%s)", frame_now(), test_step, a, STORY_UI.prompt_use ? STORY_UI.prompt_use : "no prompt");
    if (a == 1 && world_place("nile", &p, &y)) { p = v3_add(p, v3_(-3.2f + 1.3f, 0, -.6f)); tp(p, -DS_PI / 2); }
    if (a == 2 && interior_find("proof") >= 0 && interior_door(interior_find("proof"), &o, &yo, &in, &yi)) tp(o, yo);
    if (a == 3 && world_place("proof_counter", &p, &y)) tp(v3_sub(p, v3_scale(fwd(y), .3f)), y);
    if (a == 4 && interior_current() >= 0 && interior_door(interior_current(), &o, &yo, &in, &yi)) tp(in, yi + DS_PI);
    if (a == 5 && C.loaded) { v3 u = v3_norm(v3_sub(C.seat[1], C.seat[0])), n = v3_(u.z, 0, -u.x); tp(v3_add(C.table, v3_scale(n, 1.9f)), yaw_to(v3_add(C.table, v3_scale(n, 1.9f)), C.table)); }
    if (a == 5 && !C.loaded) { v3 d; float dy; if (world_place("chupacabra", &d, &dy)) tp(d, dy); test_step--; test_t -= .5f; break; }   /* load first, then again */
    if (a == 6) tp(world_lab(18.9f, 6.75f), world_lab_yaw(0));
    if (a == 7) {   /* in front of the nearest stranger, away from the Nile's side talk and the bakery's errand */
      v3 n, pr; float ny; world_place("nile", &n, &ny); world_place("proof", &pr, &ny);
      int best = -1; float bd = 1e9f;
      for (int i = 0; i < nnear; i++) { v3 q = actor_pos(NEAR[i].a); if (flat(q, n) < 6 || flat(q, pr) < 6) continue; float d = flat(q, player_pos()); if (d < bd) { bd = d; best = i; } }
      if (best >= 0) { v3 q = actor_pos(NEAR[best].a); float qy = actor_yaw(NEAR[best].a); v3 at = v3_add(q, v3_scale(fwd(qy), 1.4f)); tp(at, yaw_to(at, q)); }
    }
    if (a == 8 && nnear > 0) {   /* the nearest stranger in the room */
      int best = 0; for (int i = 1; i < nnear; i++) if (flat(actor_pos(NEAR[i].a), player_pos()) < flat(actor_pos(NEAR[best].a), player_pos())) best = i;
      v3 q = actor_pos(NEAR[best].a); float qy = actor_yaw(NEAR[best].a); v3 at = v3_add(q, v3_scale(fwd(qy), 1.2f)); tp(at, yaw_to(at, q));
    }
    if (a == 10) test_in.interact = true;
    if (a == 11) { test_in.ok = true; test_ok2 = 6; }
  }
}
#endif
