/* story.c: see story.h. */
#include "story.h"
#include "script.h"
#include "player.h"
#include "voice.h"
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

story_ui_t STORY_UI;
static t3_scene *scene; static t3_camera *cam;
static int me_id = W_VEE, m1_id = W_DEX, m2_id = W_RUTH;

typedef struct {
  actor_t *a;
  bool walking; v3 dest; float dest_yaw; bool has_yaw; float speed, walk_t;
  float turn_to; bool turning;
  const char *base;       /* the clip it rests in */
  actor_expr ex; float ex_t;
  int weapon;
  bool in_fight;
} slot_t;
static slot_t A[WHO_COUNT];

static int pc, call_ret = -1;
static enum { B_NONE, B_WAIT, B_SAY, B_CHOICE, B_WALK, B_ROAM, B_FIGHT, B_CARD, B_EPILOGUE, B_DONE } block;
static float wait_t, say_t, say_len;
static int say_who;
static bool flags[FLAG_COUNT];
static unsigned weapons = 1;
static float hour = 9, hour_target = 9;
static char line_buf[512], name_buf[64], obj_buf[160];
static int choice_first;
/* roam */
static const sc_beat *roam;
static v3 roam_pos; static bool side_done[8]; static int side_near = -1;
static int roam_follow = -1;
/* fight */
static int fight_flags, fight_id;
static const sc_beat *fight_beat;
/* coverage */
static int last_speaker = -1, lines_in_shot;
static bool skip_scene;
static float fade_to, fade_rate = 2, flash_rate = 2, dim_to;

/* ---- who is who ---- */
static int canon(int w) {
  if (w == W_ME) return me_id;
  if (w == W_M1) return m1_id;
  if (w == W_M2) return m2_id;
  return w;
}
static const char *speaker_id(int w) { w = canon(w); return w == W_SHADOW ? WHO_ID[me_id] : WHO_ID[w]; }
static const char *display_name(int w) { w = canon(w); return w == W_SHADOW ? "You" : CAST[w].name; }
static actor_t *actor_of(int w) {
  w = canon(w);
  slot_t *s = &A[w];
  if (!s->a) {
    const sc_cast *c = &CAST[w];
    if (w == W_SHADOW) {
      const sc_cast *m = &CAST[me_id];
      actor_palette p = { {250, .35f, .28f}, {250, .4f, .26f}, {250, .4f, .22f}, {250, .4f, .2f} };
      s->a = actor_new(m->model, &p, m->scale, scene);
    } else s->a = actor_new(c->model, &c->pal, c->scale, scene);
    s->base = "Idle_Loop"; s->weapon = -1;
    actor_visible(s->a, false);
  }
  return s->a;
}
static void expand(char *o, size_t cap, const char *s) {
  size_t n = 0;
  for (const char *p = s; *p && n + 16 < cap;) {
    const char *r = NULL; int skip = 0;
    if (!strncmp(p, "{me}", 4)) { r = CAST[me_id].name; skip = 4; }
    else if (!strncmp(p, "{m1}", 4)) { r = CAST[m1_id].name; skip = 4; }
    else if (!strncmp(p, "{m2}", 4)) { r = CAST[m2_id].name; skip = 4; }
    if (r) { size_t l = strlen(r); memcpy(o + n, r, l); n += l; p += skip; }
    else o[n++] = *p++;
  }
  o[n] = 0;
}

/* ---- marks ---- */
static bool lab_place(const char *n, v3 *p, float *y) {
  static const struct { const char *n; float x, z, yaw; } T[] = {
    {"lab_welcome", 7.4f, 4.4f, 60}, {"lab_laser", 25.1f, 2.6f, 110}, {"lab_kitchen", 36.8f, 3.6f, -20},
    {"lab_cowork", 11.6f, 5.4f, 40}, {"lab_shop", 31.4f, 7.2f, -90},
  };
  for (unsigned i = 0; i < sizeof T / sizeof T[0]; i++) if (!strcmp(T[i].n, n)) {
    *p = world_lab(T[i].x, T[i].z); *y = world_lab_yaw(T[i].yaw); return true;
  }
  return false;
}
static bool place_of(const char *n, v3 *p, float *y) {
  if (lab_place(n, p, y)) return true;
  return world_place(n, p, y);
}
static v3 resolve(const sc_mark *m, float *yaw, bool *has_yaw) {
  v3 p = v3_(0, 0, 0); float y = 0; bool hy = m->yaw > -9000;
  switch (m->kind) {
    case MK_LAB: p = world_lab(m->x, m->z); y = hy ? world_lab_yaw(m->yaw) : 0; break;
    case MK_PLACE: {
      v3 b; float by = 0;
      if (!place_of(m->name, &b, &by)) ds_log("story: no place %s", m->name);
      p = v3_add(b, v3_(m->x, 0, m->z));
      y = hy ? m->yaw * DS_PI / 180 : by; hy = true;
      break;
    }
    case MK_STAGE: { v3 c; float r; world_stage_floor(world_get_stage(), &c, &r); p = v3_add(c, v3_(m->x, 0, m->z)); y = hy ? m->yaw * DS_PI / 180 : 0; break; }
    default: break;
  }
  float f = world_floor_y(v3_(p.x, p.y + 1.2f, p.z));
  if (f > -1e8f) p.y = f;
  if (yaw) *yaw = y;
  if (has_yaw) *has_yaw = hy;
  return p;
}
static void face_now(int who, int target) {
  actor_t *a = actor_of(who), *b = actor_of(target);
  v3 d = v3_sub(actor_pos(b), actor_pos(a));
  actor_move(a, actor_pos(a), atan2f(d.x, d.z));
}

/* ---- camera helpers ---- */
static int resolve_subj(const int *subj, actor_t **out) {
  int n = 0;
  for (int i = 0; i < 3; i++) if (subj[i] >= 0) out[n++] = actor_of(subj[i]);
  return n;
}
/* the nearest other visible actor: who a line is said to */
static int listener_of(int who) {
  int w = canon(who), best = -1; float bd = 7;
  v3 p = actor_pos(actor_of(w));
  for (int i = 0; i < WHO_COUNT; i++) {
    if (i == w || !A[i].a || !actor_is_visible(A[i].a)) continue;
    float d = v3_len(v3_sub(actor_pos(A[i].a), p));
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}
static void occluders(void) {
  static actor_t *L[WHO_COUNT]; int n = 0;
  for (int i = 0; i < WHO_COUNT; i++) if (A[i].a && actor_is_visible(A[i].a)) L[n++] = A[i].a;
  director_set_occluders(L, n);
}
static void cut_to(sh_type t, int who, int other) {
  occluders();
  dir_spec s; memset(&s, 0, sizeof s);
  s.type = t; s.subj[0] = actor_of(who); s.nsubj = 1;
  if (other >= 0) { s.subj[1] = actor_of(other); s.nsubj = 2; }
  director_cut(&s);
}
static void auto_cover(int who, int to, sh_type asked) {
  int w = canon(who), l = to >= 0 ? canon(to) : listener_of(w);
  if (asked != SH_AUTO) { cut_to(asked, w, (asked == SH_TWO || asked == SH_OTS) ? l : l); lines_in_shot = 0; last_speaker = w; return; }
  /* same speaker going on: hold a line or two, then change the angle */
  if (w == last_speaker && lines_in_shot < 1) { lines_in_shot++; return; }
  if (l < 0) { cut_to(SH_MCU, w, -1); }
  else {
    int r = (int)(ds_rand() * 10);
    if (r < 5) cut_to(SH_OTS, l, w);        /* over the listener, on the speaker */
    else if (r < 8) cut_to(SH_MCU, w, l);
    else cut_to(SH_TWO, w, l);
  }
  last_speaker = w; lines_in_shot = 0;
}

/* ---- saves ---- */
typedef struct { uint32_t magic; int pc, me; unsigned weapons; float hour, x, y, z, yaw; int stats[ST_COUNT]; uint8_t flags[64]; char objective[160]; } save_t;
static void autosave(void) {
  save_t *s = (save_t *)DS_SAVE.data;
  s->magic = DS_SAVE_MAGIC; s->pc = pc; s->me = me_id; s->weapons = weapons; s->hour = hour_target;
  { actor_t *m = actor_of(W_ME); v3 p = actor_pos(m); s->x = p.x; s->y = p.y; s->z = p.z; s->yaw = actor_yaw(m); }
  memcpy(s->stats, STORY_UI.stats, sizeof s->stats);
  for (int i = 0; i < FLAG_COUNT && i < 64; i++) s->flags[i] = flags[i];
  snprintf(s->objective, sizeof s->objective, "%s", obj_buf);
  DS_SAVE.magic = DS_SAVE_MAGIC;
}
bool story_has_save(void) { return DS_SAVE.magic == DS_SAVE_MAGIC && ((save_t *)DS_SAVE.data)->magic == DS_SAVE_MAGIC; }

static void set_cast(int m) {
  me_id = m;
  m1_id = m == W_VEE ? W_DEX : W_VEE;
  m2_id = m == W_RUTH ? W_DEX : W_RUTH;
}
static void extras_update(float dt);
static void extras_reset(void);
static void hide_all(void) { for (int i = 0; i < WHO_COUNT; i++) if (A[i].a) { actor_visible(A[i].a, false); A[i].walking = false; } }
void story_init(t3_scene *s, t3_camera *c) { scene = s; cam = c; director_init(c); }
static void boot(void) {
  block = B_NONE; call_ret = -1; last_speaker = -1; skip_scene = false; roam = NULL;
  memset(side_done, 0, sizeof side_done);
  STORY_UI.fade = 1; fade_to = 1; dim_to = 0;
  world_set_stage(STAGE_TOWN);
  hide_all();
  player_set_actor(actor_of(W_ME));
}
void story_new_game(int p) {
  static const int PL[3] = { W_VEE, W_DEX, W_RUTH };
  memset(&STORY_UI, 0, sizeof STORY_UI); memset(flags, 0, sizeof flags);
  set_cast(PL[p]);
  for (int i = 0; i < ST_COUNT; i++) STORY_UI.stats[i] = 0;
  STORY_UI.stats[ST_TRUST] = 5; STORY_UI.stats[ST_SAFETY] = 5; STORY_UI.stats[ST_VIBE] = 5; STORY_UI.stats[ST_DUES] = 5; STORY_UI.stats[ST_ENERGY] = 5;
  obj_buf[0] = 0; STORY_UI.objective = NULL;
  weapons = 0; hour = hour_target = 8.9f; world_set_time(hour);
  pc = 0;
  boot();
}
void story_continue(void) {
  save_t *s = (save_t *)DS_SAVE.data;
  memset(&STORY_UI, 0, sizeof STORY_UI);
  set_cast(s->me); pc = s->pc; weapons = s->weapons; hour = hour_target = s->hour;
  memcpy(STORY_UI.stats, s->stats, sizeof s->stats);
  for (int i = 0; i < FLAG_COUNT && i < 64; i++) flags[i] = s->flags[i] != 0;
  snprintf(obj_buf, sizeof obj_buf, "%s", s->objective);
  STORY_UI.objective = obj_buf[0] ? obj_buf : NULL;
  world_set_time(hour);
  boot();
  { actor_t *m = actor_of(W_ME); actor_visible(m, true); actor_place(m, v3_(s->x, s->y, s->z), s->yaw); }
  fade_to = 0; fade_rate = 1;
}
void story_stop(void) { voice_stop(); hide_all(); director_stop(); extras_reset(); }
story_run story_state(void) { return block == B_ROAM ? SR_ROAM : block == B_FIGHT ? SR_FIGHT : block == B_EPILOGUE ? SR_EPILOGUE : block == B_DONE ? SR_DONE : SR_RUNNING; }
const char *story_player_name(void) { return CAST[me_id].name; }

/* ---- fights ---- */
static fight_def FD;
static void begin_fight(const sc_beat *b) {
  const sc_fight *F = &FIGHT_TABLE[b->i];
  memset(&FD, 0, sizeof FD);
  FD.title = F->title; FD.sub = F->sub; FD.count = F->count; FD.radius = F->radius;
  v3 c; float yaw, r;
  if (!strcmp(F->where, "stage")) world_stage_floor(world_get_stage(), &c, &r);
  else if (!strcmp(F->where, "lounge")) c = world_lab(15.6f, 4.8f);
  else if (!place_of(F->where, &c, &yaw)) c = actor_pos(actor_of(W_ME));
  FD.center = c;
  for (int i = 0; i < F->count; i++) {
    const sc_foe *e = &F->foes[i];
    fighter_def *d = &FD.foes[i];
    bool shadow = e->palette && !strcmp(e->palette, "shadow");
    bool alt = e->palette && !strcmp(e->palette, "alt");
    int w = shadow ? W_SHADOW : canon(e->who);
    const sc_cast *cc = w == W_SHADOW ? &CAST[me_id] : &CAST[w];
    d->model = cc->model; d->pal = cc->pal; d->scale = cc->scale;
    d->name = e->name ? e->name : cc->name;
    d->weapon = (weapon_id)e->weapon; d->ai = (ai_kind)e->ai;
    d->hp = e->hp; d->power = e->power; d->speed = e->speed; d->place_x = e->x; d->place_z = e->z;
    if (alt) { d->pal.top.hue += 40; d->pal.hair.hue += 40; d->actor = NULL; }
    else { d->actor = actor_of(w); actor_visible(d->actor, true); A[w].in_fight = true; }
  }
  /* the story's numbers lean on the fight */
  int rep = STORY_UI.stats[ST_TRUST] + STORY_UI.stats[ST_SAFETY] + STORY_UI.stats[ST_VIBE] - 15;
  FD.foe_scale = ds_clamp(1.0f - rep * .015f, .8f, 1.2f);
  FD.player_hp = ds_clamp(.7f + STORY_UI.stats[ST_ENERGY] * .06f, .6f, 1.0f);
  FD.weapons = weapons | 1u; FD.start_weapon = 0;
  for (int k = WP_COUNT - 1; k >= 0; k--) if (A[canon(W_ME)].weapon == k) FD.start_weapon = k;
  FD.tutorial = (b->i2 & 1) != 0;
  FD.easy = b->target >= 0 && flags[b->target];
  fight_beat = b; fight_flags = b->i2; fight_id = b->i;
  STORY_UI.cine = false; STORY_UI.talking = false;
  actor_t *me = actor_of(W_ME); actor_visible(me, true); A[canon(W_ME)].in_fight = true;
  fight_start(&FD, me);
  block = B_FIGHT;
}

/* ---- the machine ---- */
static const char *stat_name(int i) {
  switch (i) { case ST_TRUST: return "TRUST"; case ST_SAFETY: return "SAFETY"; case ST_VIBE: return "VIBE"; case ST_DUES: return "DUES"; case ST_ENERGY: return "ENERGY"; default: return NULL; }
}
static void step(void) {
  while (block == B_NONE && pc < SCRIPT_COUNT) {
    const sc_beat *b = &SCRIPT[pc];
    switch (b->op) {
      case OP_CHAPTER:
        hour = hour_target = b->f; world_set_time(hour); STORY_UI.hour = hour;
        pc++; autosave(); pc--;
        STORY_UI.chapter = true; STORY_UI.ch_title = b->s; STORY_UI.ch_sub = b->s2; STORY_UI.ch_t = 0; STORY_UI.ch_dur = 2.8f; STORY_UI.ch_hour = b->f;
        break;
      case OP_CLOCK: hour = hour_target = b->f; world_set_time(hour); break;
      case OP_PLACE: {
        actor_t *a = actor_of(b->who); slot_t *s = &A[canon(b->who)];
        float y; bool hy; v3 p = resolve(&b->mark, &y, &hy);
        actor_visible(a, true); actor_hands_up(a, 0); actor_place(a, p, y); s->walking = false; s->base = "Idle_Loop";
        actor_play(a, "Idle_Loop", 1, true, .1f);
        if (b->who2 >= 0) { actor_t *t = actor_of(b->who2); if (actor_is_visible(t)) face_now(b->who, b->who2); actor_look(a, actor_head_pos(t), .8f); }
        break;
      }
      case OP_HIDE: { int w = canon(b->who); if (A[w].a) { actor_visible(A[w].a, false); A[w].walking = false; } break; }
      case OP_WALK: {
        actor_t *a = actor_of(b->who); slot_t *s = &A[canon(b->who)];
        float y; bool hy; v3 p = resolve(&b->mark, &y, &hy);
        actor_visible(a, true);
        if (skip_scene) { actor_place(a, p, y); break; }
        s->walking = true; s->dest = p; s->dest_yaw = y; s->has_yaw = hy; s->speed = b->f; s->walk_t = 0;
        actor_play(a, b->f > 2.4f ? "Sprint_Loop" : b->f > 1.7f ? "Jog_Fwd_Loop" : "Walk_Loop", 1, true, .18f);
        if (!b->i) { say_who = canon(b->who); block = B_WALK; }
        break;
      }
      case OP_WAITWALK: if (A[canon(b->who)].walking) { say_who = canon(b->who); block = B_WALK; } break;
      case OP_SAY: {
        int w = canon(b->who);
        actor_t *a = actor_of(w);
        if (b->i >= 0) { actor_expr_clear(a); actor_expr_set(a, (actor_expr)b->i, b->f); A[w].ex = (actor_expr)b->i; A[w].ex_t = 4; }
        if (b->s2) { actor_play(a, b->s2, 1, !strstr(b->s2, "Interact"), .25f); A[w].base = b->s2; }
        else if (A[w].base && !strcmp(A[w].base, "Idle_Loop")) actor_play(a, "Idle_Talking_Loop", 1, true, .3f);
        expand(line_buf, sizeof line_buf, b->s);
        snprintf(name_buf, sizeof name_buf, "%s", display_name(w));
        int l = b->who2 >= 0 ? canon(b->who2) : listener_of(w);
        if (l >= 0 && A[l].a) {
          actor_look(a, actor_head_pos(A[l].a), .9f);
          actor_look(A[l].a, actor_head_pos(a), .9f);
          /* a speaker turns to whoever they are talking to, a little */
          v3 d = v3_sub(actor_pos(A[l].a), actor_pos(a));
          if (!A[w].walking && d.x * d.x + d.z * d.z > .01f) { A[w].turn_to = atan2f(d.x, d.z); A[w].turning = true; }
        }
        auto_cover(w, b->who2, (sh_type)b->i2);
        if (skip_scene) break;
        voice_say(speaker_id(w), line_buf);
        say_len = voice_seconds(speaker_id(w), line_buf);
        if (say_len <= 0) say_len = fmaxf(1.4f, strlen(line_buf) / 16.f);
        say_t = 0; say_who = w; block = B_SAY;
        STORY_UI.talking = true; STORY_UI.speaker = name_buf; STORY_UI.line = line_buf; STORY_UI.line_t = 0;
        STORY_UI.line_cps = fmaxf(20, strlen(line_buf) / fmaxf(.5f, say_len * .94f)); STORY_UI.line_done = false;
        STORY_UI.speaker_col = w == W_SHADOW ? 0x8878c8 : CAST[w].color;
        break;
      }
      case OP_SHOT: {
        dir_spec s; memset(&s, 0, sizeof s);
        s.type = (sh_type)b->i;
        if (s.type == SH_FIXED) {
          sc_mark m1 = b->mark, m2 = b->mark2;
          s.fpos = resolve(&m1, NULL, NULL); s.flook = resolve(&m2, NULL, NULL); s.fpos.y += 1.45f; s.flook.y += 1.2f; s.fov = b->f;
        } else {
          s.nsubj = resolve_subj(b->subj, s.subj); s.dist = b->f; s.height = b->f2; s.fov = b->i2 / 10.f; s.dur = b->target / 100.f;
        }
        occluders(); director_cut(&s); last_speaker = -1;
        break;
      }
      case OP_WAIT: if (!skip_scene) { wait_t = b->f; block = B_WAIT; } break;
      case OP_FACE: face_now(b->who, b->who2); break;
      case OP_LOOK: actor_look(actor_of(b->who), actor_head_pos(actor_of(b->who2)), .9f); break;
      case OP_CLIP: {
        actor_t *a = actor_of(b->who);
        actor_play(a, b->s, b->f, b->i != 0, .25f);
        if (b->i) A[canon(b->who)].base = b->s;
        break;
      }
      case OP_EMOTE: actor_expr_set(actor_of(b->who), (actor_expr)b->i, b->f); break;
      case OP_OBJECTIVE:
        snprintf(obj_buf, sizeof obj_buf, "%s", b->s); STORY_UI.objective = obj_buf;
        break;
      case OP_ROAM: {
        roam = b; roam_follow = b->who >= 0 ? canon(b->who) : -1;
        if (b->f2 > 0) { hour_target = b->f2; }
        v3 p; float y;
        if (!place_of(b->s, &p, &y)) { ds_log("story: no roam place %s", b->s); p = actor_pos(actor_of(W_ME)); }
        roam_pos = p;
        /* put the side conversations' people out */
        for (int i = 0; i < b->i2 && i < 8; i++) if (!side_done[i]) {
          const sc_side *sd = &SIDES[b->i + i];
          actor_t *ac = actor_of(sd->who);
          float yy; bool hy; v3 q = resolve(&sd->mark, &yy, &hy);
          actor_visible(ac, true); actor_place(ac, q, yy);
        }
        director_stop();
        world_door_auto(true);
        player_set_actor(actor_of(W_ME));
        player_teleport(actor_pos(actor_of(W_ME)), actor_yaw(actor_of(W_ME)));
        player_camera_snap();
        STORY_UI.talking = false;
        skip_scene = false;
        block = B_ROAM;
        break;
      }
      case OP_FIGHT:
        if (skip_scene) { skip_scene = false; }
        begin_fight(b);
        break;
      case OP_CHOICE: {
        skip_scene = false;
        STORY_UI.choosing = true; STORY_UI.prompt = b->s; STORY_UI.nopt = b->i; STORY_UI.sel = 0; STORY_UI.choose_t = 0;
        choice_first = pc + 1;
        for (int i = 0; i < b->i && i < 5; i++) STORY_UI.opt[i] = SCRIPT[pc + 1 + i].s;
        STORY_UI.talking = false;
        block = B_CHOICE;
        break;
      }
      case OP_OPTION: break;
      case OP_JUMP: pc = b->target - 1; break;
      case OP_JUMP_NOT: {
        bool ok = b->flag >= 0 ? flags[b->flag] : (b->i >= 0 ? STORY_UI.stats[b->i] >= b->f : true);
        if (!ok) pc = b->target - 1;
        break;
      }
      case OP_FLAG: flags[b->flag] = true; break;
      case OP_ADD:
        STORY_UI.stats[b->i] += b->i2;
        if (stat_name(b->i)) { STORY_UI.toast = stat_name(b->i); STORY_UI.toast_n = b->i2; STORY_UI.toast_t = 2.4f; }
        break;
      case OP_CARD:
        STORY_UI.card = true; STORY_UI.card_title = b->s; STORY_UI.card_sub = b->s2; STORY_UI.card_t = 0; STORY_UI.card_dur = b->f;
        if (!skip_scene) { wait_t = b->f; block = B_WAIT; } else STORY_UI.card = false;
        break;
      case OP_MUSIC: music_mood((mood_t)b->i); break;
      case OP_STAGE: world_set_stage((world_stage)b->i); last_speaker = -1; break;
      case OP_GIVE: weapons |= 1u << b->i; STORY_UI.got = b->s; STORY_UI.got_t = 3; break;
      case OP_DRAW: {
        actor_t *a = actor_of(b->who);
        A[canon(b->who)].weapon = b->i;
        weapon_equip(a, b->i);
        if (b->i >= 0 && !skip_scene) weapon_draw_flash(a);
        break;
      }
      case OP_DOOR: world_door(b->f); break;
      case OP_FX:
        switch (b->i) {
          case FX_FADE_IN: STORY_UI.fade = 1; fade_to = 0; fade_rate = 1 / fmaxf(.05f, b->f); break;
          case FX_FADE_OUT: fade_to = 1; fade_rate = 1 / fmaxf(.05f, b->f); if (!skip_scene) { wait_t = b->f; block = B_WAIT; } else STORY_UI.fade = 1; break;
          case FX_FLASH: STORY_UI.flash = 1; flash_rate = 1 / fmaxf(.05f, b->f); dim_to = 0; break;
          case FX_LIGHTS_OUT: dim_to = .5f; break;
          default: break;
        }
        break;
      case OP_NOTE: STORY_UI.note = b->s; STORY_UI.note_t = 4; break;
      case OP_EPILOGUE: block = B_EPILOGUE; music_mood(MOOD_CLEAR); STORY_UI.cine = false; break;
      case OP_END: block = B_DONE; break;
      case OP_RET: pc = call_ret - 1; call_ret = -1; block = B_ROAM; director_stop(); player_camera_snap(); skip_scene = false; STORY_UI.talking = false; break;
      default: break;
    }
    pc++;
  }
}

static void update_actors(float dt) {
  for (int i = 0; i < WHO_COUNT; i++) {
    slot_t *s = &A[i];
    if (!s->a || !actor_is_visible(s->a)) continue;
    if (block == B_FIGHT && s->in_fight) continue;   /* fight.c drives its fighters */
    bool is_player = canon(W_ME) == i;
    if (s->walking) {
      v3 p = actor_pos(s->a), d = v3_sub(s->dest, p); d.y = 0;
      float l = v3_len(d);
      s->walk_t += dt;
      if (l < .12f || s->walk_t > l / fmaxf(.3f, s->speed) + 6) {
        s->walking = false;
        actor_place(s->a, s->dest, s->has_yaw ? s->dest_yaw : actor_yaw(s->a));
        actor_play(s->a, "Idle_Loop", 1, true, .2f); s->base = "Idle_Loop";
      } else {
        v3 dir = v3_scale(d, 1 / l), vel = v3_scale(dir, s->speed);
        bool g; vel.y = -2;
        world_move(&p, &vel, .25f, 1.5f, dt, &g);
        float y = actor_yaw(s->a), want = atan2f(dir.x, dir.z);
        y += ds_wrap_pi(want - y) * fminf(1, dt * 9);
        actor_move(s->a, p, y); actor_set_floor(s->a, p.y);
      }
    } else if (s->turning) {
      float y = actor_yaw(s->a), e = ds_wrap_pi(s->turn_to - y);
      if (fabsf(e) < .03f) s->turning = false;
      else actor_move(s->a, actor_pos(s->a), y + e * fminf(1, dt * 6));
    }
    if (s->ex_t > 0) { s->ex_t -= dt; if (s->ex_t <= 0 && block != B_SAY) actor_expr_clear(s->a); }
    /* lip sync */
    if (block == B_SAY && say_who == i) { float o, sh; voice_mouth(&o, &sh); actor_mouth(s->a, o, sh); }
    else actor_mouth(s->a, 0, 0);
    (void)is_player;
    if (!(STORY_UI.roam && is_player)) actor_update(s->a, dt);
  }
}

/* ---- the extras: people on Main Street, and the Arduino Night crowd in the lab ---- */
typedef struct { actor_t *a; float x, z, dir, speed; int mode; } extra_t;
#define EXTRAS 7
static extra_t X[EXTRAS];
static int extras_mode = -1;     /* 0 town, 1 lab crowd, 2 gone */
static const struct { actor_model m; actor_palette p; } EX_LOOK[EXTRAS] = {
  { AM_S, {{20, .9f, 1.2f}, {200, .9f, .9f}, {40, .7f, .9f}, {0, 1, 1}} },
  { AM_D, {{10, .6f, .9f}, {120, .5f, 1.5f}, {210, .7f, .9f}, {0, 1, 1}} },
  { AM_C, {{-150, .8f, 1.3f}, {60, .8f, 1.1f}, {90, .6f, .8f}, {0, 1, 1}} },
  { AM_F, {{40, .9f, .9f}, {330, .8f, 1.1f}, {210, .4f, .7f}, {0, 1, 1}} },
  { AM_B, {{130, .7f, 1.1f}, {30, 1, 1.1f}, {180, .8f, .8f}, {0, 1, 1}} },
  { AM_D, {{70, .8f, 1.3f}, {260, .7f, 1.2f}, {30, .6f, .7f}, {0, 1, 1}} },
  { AM_S, {{300, .7f, .7f}, {100, .7f, .9f}, {300, .6f, .6f}, {0, 1, 1}} },
};
/* the crowd at Arduino Night: plan positions around the coworking tables and the lounge */
static const float CROWD[EXTRAS][3] = { {11.2f, 3.6f, 90}, {12.6f, 7.4f, 0}, {14.4f, 2.8f, 45}, {10.4f, 7.6f, 180}, {16.8f, 3.4f, -60}, {9.2f, 6.9f, 70}, {17.6f, 7.2f, -130} };
static void extras_reset(void) { for (int i = 0; i < EXTRAS; i++) if (X[i].a) actor_visible(X[i].a, false); extras_mode = -1; }
static void extras_update(float dt) {
  int want = world_get_stage() != STAGE_TOWN ? 2 : (hour >= 18.9f && hour < 21.8f) ? 1 : hour >= 21.8f ? 2 : 0;
  if (block == B_FIGHT && want == 1) want = 2;
  if (want != extras_mode) {
    extras_mode = want;
    for (int i = 0; i < EXTRAS; i++) {
      if (want != 2 && !X[i].a) { X[i].a = actor_new(EX_LOOK[i].m, &EX_LOOK[i].p, 1, scene); actor_visible(X[i].a, false); }
      if (!X[i].a) continue;
      actor_visible(X[i].a, want != 2);
      if (want == 1) {
        v3 p = world_lab(CROWD[i][0], CROWD[i][1]);
        actor_place(X[i].a, p, world_lab_yaw(CROWD[i][2]));
        actor_play(X[i].a, i % 3 == 0 ? "Fixing_Kneeling" : i % 3 == 1 ? "Idle_Talking_Loop" : "Idle_Loop", .9f + i * .03f, true, 0);
      } else if (want == 0) {
        /* along the two sidewalks, either way */
        X[i].dir = i & 1 ? 1 : -1; X[i].speed = ds_rr(1.05f, 1.45f); X[i].x = -90 + i * 34;
        v3 d, n; float y;
        if (!world_place("lab_door", &d, &y)) d = v3_(0, 0, 2);
        if (!world_place("nile", &n, &y)) n = v3_(0, 0, 28);
        X[i].z = i % 3 == 0 ? n.z - .4f : d.z + .9f;
        actor_play(X[i].a, "Walk_Loop", X[i].speed / 1.3f, true, 0);
      }
    }
  }
  if (extras_mode == 2) return;
  v3 foc = v3_(((t3_object *)cam)->position.x, 0, ((t3_object *)cam)->position.z);
  for (int i = 0; i < EXTRAS; i++) {
    extra_t *e = &X[i];
    if (!e->a) continue;
    if (extras_mode == 0) {
      e->x += e->dir * e->speed * dt;
      /* they stay around you: past 110 m they come back round the other way */
      if (e->x - foc.x > 110) e->x -= 220; else if (e->x - foc.x < -110) e->x += 220;
      v3 p = v3_(e->x, 0, e->z);
      float f = world_floor_y(v3_(p.x, 1.2f, p.z));
      if (f > -1e8f) p.y = f;
      if (fabsf(e->x - foc.x) > 70) { actor_visible(e->a, false); actor_move(e->a, p, e->dir > 0 ? DS_PI / 2 : -DS_PI / 2); continue; }
      actor_visible(e->a, true);
      actor_move(e->a, p, e->dir > 0 ? DS_PI / 2 : -DS_PI / 2); actor_set_floor(e->a, p.y);
    }
    actor_update(e->a, dt);
  }
}

static void project(v3 w, bool *behind, float *x, float *y) {
  t3_object_update_world_matrix(cam, true, false);
  const float *m = ((t3_object *)cam)->matrix_world.e;
  v3 r = v3_(m[0], m[1], m[2]), u = v3_(m[4], m[5], m[6]), f = v3_(-m[8], -m[9], -m[10]), c = v3_(m[12], m[13], m[14]);
  v3 d = v3_sub(w, c);
  float z = v3_dot(d, f);
  *behind = z <= .05f;
  float t = tanf(cam->fov * DS_PI / 360);
  float sx = v3_dot(d, r) / fmaxf(.05f, fabsf(z)) / (t * (float)DS_W / DS_H), sy = v3_dot(d, u) / fmaxf(.05f, fabsf(z)) / t;
  *x = DS_W * (.5f + sx * .5f); *y = DS_H * (.5f - sy * .5f);
}

#ifdef KH_AUTO
/* the whole day headless: choices by seed, walks by teleport, fights won after a moment */
static float auto_t; static int auto_seed = 3;
static void auto_log(const char *what, const char *s) { ds_log("[auto] pc=%d %.1fh %s %s", pc, hour, what, s ? s : ""); }
#endif
void story_update(float dt, const intent_t *in) {
  story_ui_t *U = &STORY_UI;
#ifdef KH_AUTO
  { static intent_t fake; fake = *in; fake.ok = false; fake.interact = false; in = &fake;
    auto_t += dt;
    if (block == B_SAY && say_t > fminf(1.5f, say_len)) { voice_stop(); block = B_NONE; U->talking = false; }
    if (block == B_CHOICE && U->choose_t > .8f) { U->sel = (auto_seed++ * 7) % U->nopt; fake.ok = true; auto_log("choice", U->opt[U->sel]); }
    if (block == B_ROAM && auto_t > 2) { player_teleport(roam_pos, 0); fake.interact = true; auto_t = 0; auto_log("roam", roam->s); }
    if (block == B_FIGHT && auto_t > KH_AUTO) { fight_end(); for (int k = 0; k < WHO_COUNT; k++) A[k].in_fight = false; if (fight_beat->flag >= 0) flags[fight_beat->flag] = true; actor_play(actor_of(W_ME), "Idle_Loop", 1, true, .3f); auto_log("fight won", FD.title); block = B_NONE; }
    if (block == B_FIGHT && U->retry) U->retry = false;
    if (block != B_ROAM && block != B_FIGHT) auto_t = 0;
    if (block == B_EPILOGUE || block == B_DONE) { static int once; if (!once++) { auto_log("END", story_end_title()); for (int i = 0; i < ST_COUNT; i++) ds_log("stat %d = %d", i, U->stats[i]); } }
  }
#endif
  /* the clock eases, so a time jump sweeps the sky */
  if (fabsf(hour - hour_target) > .001f) {
    hour = ds_damp(hour, hour_target, 1.2f, dt);
    if (fabsf(hour - hour_target) < .002f) hour = hour_target;
    world_set_time(hour);
  }
  U->hour = hour;
  step();
  U->roam = block == B_ROAM;
  U->cine = !(block == B_ROAM || block == B_FIGHT || block == B_EPILOGUE);
  U->letterbox = ds_damp(U->letterbox, U->cine ? 1 : 0, 7, dt);
  U->fade += ds_clamp(fade_to - U->fade, -fade_rate * dt, fade_rate * dt);
  U->flash = fmaxf(0, U->flash - flash_rate * dt);
  U->dim = ds_damp(U->dim, dim_to, 3, dt);
  if (U->chapter) { U->ch_t += dt; if (U->ch_t > U->ch_dur) U->chapter = false; }
  if (U->card) { U->card_t += dt; if (U->card_t > U->card_dur) U->card = false; }
  if (U->note_t > 0) { U->note_t -= dt; if (U->note_t <= 0) U->note = NULL; }
  if (U->toast_t > 0) U->toast_t -= dt;
  if (U->got_t > 0) U->got_t -= dt;
  U->marker_on = false; U->prompt_use = NULL;

  /* hold START: skip to the next choice, fight or walk */
  if (U->cine && !U->choosing && input_start_held()) {
    U->skip += dt / .8f;
    if (U->skip >= 1) { U->skip = 0; skip_scene = true; voice_stop(); if (block == B_SAY || block == B_WAIT || block == B_WALK) block = B_NONE; U->talking = false; }
  } else U->skip = 0;

  switch (block) {
    case B_WAIT: wait_t -= dt; if (wait_t <= 0) block = B_NONE; break;
    case B_SAY: {
      say_t += dt; U->line_t += dt;
      float shown = U->line_t * U->line_cps;
      if (shown >= (float)strlen(U->line)) U->line_done = true;
      if (in->ok) {
        if (!U->line_done) { U->line_t = 1e6f; U->line_done = true; }
        else { voice_stop(); block = B_NONE; U->talking = false; }
      } else if (say_t > say_len + .35f && U->line_done) { block = B_NONE; U->talking = false; }
      if (block == B_NONE && A[say_who].a) {
        const char *base = A[say_who].base;
        if (base && !strcmp(base, "Idle_Loop")) actor_play(A[say_who].a, "Idle_Loop", 1, true, .4f);
      }
      break;
    }
    case B_WALK: if (!A[say_who].walking) block = B_NONE; break;
    case B_CHOICE: {
      U->choose_t += dt;
      if (in->nav_y) { U->sel = (U->sel + in->nav_y + U->nopt) % U->nopt; sfx(SFX_UI, 0, 1); }
      if (in->ok && U->choose_t > .25f) {
        sfx(SFX_UI_OK, 0, 1);
        const sc_beat *o = &SCRIPT[choice_first + U->sel];
        U->choosing = false;
        pc = o->target;
        block = B_NONE;
        /* the choice is spoken */
        if (o->s2) {
          actor_t *a = actor_of(W_ME);
          expand(line_buf, sizeof line_buf, o->s2);
          if (voice_say(WHO_ID[me_id], line_buf)) {
            say_len = voice_seconds(WHO_ID[me_id], line_buf);
            say_t = 0; say_who = canon(W_ME); block = B_SAY;
            snprintf(name_buf, sizeof name_buf, "%s", CAST[me_id].name);
            U->talking = true; U->speaker = name_buf; U->line = line_buf; U->line_t = 0;
            U->line_cps = fmaxf(20, strlen(line_buf) / fmaxf(.5f, say_len * .94f)); U->line_done = false;
            U->speaker_col = CAST[me_id].color;
            actor_play(a, "Idle_Talking_Loop", 1, true, .3f);
            auto_cover(W_ME, -1, SH_AUTO);
          }
        }
      }
      break;
    }
    case B_ROAM: {
      player_update(dt, in);
      /* someone following along: walk after the player */
      if (roam_follow >= 0 && A[roam_follow].a) {
        slot_t *s = &A[roam_follow];
        v3 p = player_pos(), d = v3_sub(p, actor_pos(s->a)); d.y = 0;
        float l = v3_len(d);
        if (l > 2.2f) { s->walking = true; s->dest = v3_add(p, v3_scale(v3_norm(d), -1.5f)); s->speed = l > 6 ? 3.6f : l > 3.5f ? 2.2f : 1.4f; s->has_yaw = false; s->walk_t = 0; actor_play(s->a, s->speed > 2.4f ? "Sprint_Loop" : s->speed > 1.7f ? "Jog_Fwd_Loop" : "Walk_Loop", 1, true, .2f); }
        if (l > 40) actor_place(s->a, v3_add(p, v3_(-1, 0, 1)), actor_yaw(s->a));
      }
      /* side conversations */
      side_near = -1;
      for (int i = 0; i < roam->i2 && i < 8; i++) if (!side_done[i]) {
        const sc_side *sd = &SIDES[roam->i + i];
        slot_t *s = &A[canon(sd->who)];
        if (s->a && actor_is_visible(s->a) && v3_len(v3_sub(actor_pos(s->a), player_pos())) < 2.6f) { side_near = i; U->prompt_use = sd->prompt; break; }
      }
      if (side_near >= 0 && in->interact) {
        const sc_side *sd = &SIDES[roam->i + side_near];
        side_done[side_near] = true;
        call_ret = pc; pc = sd->start;
        face_now(W_ME, sd->who); face_now(sd->who, W_ME);
        block = B_NONE;
        break;
      }
      /* the objective */
      float dist = v3_len(v3_sub(roam_pos, player_pos()));
      bool beh; float mx, my; project(v3_add(roam_pos, v3_(0, 1.4f, 0)), &beh, &mx, &my);
      U->marker_on = true; U->marker_behind = beh; U->marker_x = mx; U->marker_y = my; U->marker_dist = dist;
      if (dist < roam->f) {
        if (!roam->s2) block = B_NONE;
        else { if (side_near < 0) U->prompt_use = roam->s2; if (in->interact) block = B_NONE; }
        if (block == B_NONE) {
          /* leave the unused side people where they stand no longer */
          for (int i = 0; i < roam->i2 && i < 8; i++) { const sc_side *sd = &SIDES[roam->i + i]; slot_t *s = &A[canon(sd->who)]; if (s->a) actor_visible(s->a, false); }
          memset(side_done, 0, sizeof side_done);
          actor_t *me = actor_of(W_ME); actor_play(me, "Idle_Loop", 1, true, .2f);
          last_speaker = -1;
        }
      }
      break;
    }
    case B_FIGHT: {
      if (U->retry) {
        if (in->nav_y) { U->retry_sel = !U->retry_sel; sfx(SFX_UI, 0, 1); }
        if (in->ok) {
          sfx(SFX_UI_OK, 0, 1); U->retry = false;
          if (U->retry_sel == 0) { fight_end(); begin_fight(fight_beat); }
          else {
            /* talk it down: you lose face, the day goes on */
            fight_end(); for (int k = 0; k < WHO_COUNT; k++) A[k].in_fight = false; U->stats[ST_VIBE] -= 1; U->stats[ST_ENERGY] -= 1;
            U->note = "It took longer than a sword would have. You lost some of the day."; U->note_t = 4;
            block = B_NONE;
          }
        }
        break;
      }
#ifdef KH_AUTO
      fight_update(dt, in); break;
#endif
      fight_state st = fight_update(dt, in);
      if (st == FIGHT_WON || st == FIGHT_LOST) {
        for (int k = 0; k < WHO_COUNT; k++) if (!(st == FIGHT_LOST && !(fight_flags & 6))) A[k].in_fight = false;
        bool won = st == FIGHT_WON || (fight_flags & 2);
        if (won || (fight_flags & 4)) {
          if (won && fight_beat->flag >= 0) flags[fight_beat->flag] = true;
          fight_end();
          actor_t *me = actor_of(W_ME); actor_play(me, "Idle_Loop", 1, true, .3f);
          A[canon(W_ME)].base = "Idle_Loop";
          U->stats[ST_ENERGY] -= fight_player_hp() < .3f ? 1 : 0;
          block = B_NONE;
        } else { U->retry = true; U->retry_sel = 0; U->retry_allow_skip = true; }
      }
      break;
    }
    default: break;
  }
  extras_update(dt);
  bool ran_player = block == B_ROAM;
  U->roam = ran_player;
  update_actors(dt);
  if (block != B_ROAM && block != B_FIGHT) director_update(dt);
  world_update(dt, v3_(((t3_object *)cam)->position.x, ((t3_object *)cam)->position.y, ((t3_object *)cam)->position.z));
}

const char *story_end_title(void) {
  for (const sc_ending *e = ENDINGS; e->flag >= 0; e++) if (flags[e->flag]) return e->title;
  return "KEYHOLDER";
}
int story_end_lines(const char **out, int cap) {
  int n = 0;
  for (const sc_ending *e = ENDINGS; e->flag >= 0; e++) if (flags[e->flag]) {
    for (int i = 0; i < 4 && e->lines[i] && n < cap; i++) out[n++] = e->lines[i];
    break;
  }
  for (const sc_statline *s = STAT_LINES; s->flag >= 0 && n < cap; s++)
    if (flags[s->flag] && !(s->not_flag >= 0 && flags[s->not_flag])) out[n++] = s->text;
  return n;
}
