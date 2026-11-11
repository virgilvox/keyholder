/* test_fight.c: runs the fight module alone (KH_TEST_FIGHT): a stub flat world, a two foe
 * fight, scripted input through tools/shot.mjs, and a bot that plays whole fights headless
 * to tune difficulty. Keys held in the first frames choose: Digit1..6 a tier, KeyN a poor
 * bot, KeyG a good bot, KeyX all tiers in a batch with no drawing between them, KeyM start
 * with a full meter, KeyT the tutorial flags, KeyH foes nearly beaten, KeyL the player nearly
 * beaten, KeyP the third person walk instead of a fight. v1.1: KeyO with Digit1..9, 0 a twist tier (tutorial, timer, retrieve,
 * score, hold, defend, protect, boss Root, boss Burnout with the finisher, a plain fight), KeyY with KeyX
 * batches only those, KeyW with Digit1..5 start with that weapon (all owned), KeyC start in phase two (the checkpoint),
 * KeyZ the Battle Zone flag with every modifier off but turbo. */
#ifdef KH_TEST_FIGHT
#include "game.h"
#include "actor.h"
#include "fight_int.h"
#include "weapon.h"
#include "player.h"
#include "fx.h"
#include "input.h"
#include "audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- a stub world: a floor at 0 and a block to test the spring arm against ---- */
static world_stage STAGE;
static const float BX0 = 6, BX1 = 8, BZ0 = -2, BZ1 = 2;
void world_init(t3_scene *s) {} void world_set_stage(world_stage s) { STAGE = s; } world_stage world_get_stage(void) { return STAGE; }
void world_update(float dt, v3 f) {} void world_set_time(float h) {}
void world_stage_floor(world_stage s, v3 *c, float *r) { *c = v3_(0,0,0); *r = 12; }
void world_move(v3 *p, v3 *v, float r, float h, float dt, bool *g) {
  p->x += v->x * dt; p->y += v->y * dt; p->z += v->z * dt;
  if (p->x > BX0 - r && p->x < BX1 + r && p->z > BZ0 - r && p->z < BZ1 + r) {
    float a = p->x - (BX0 - r), b = (BX1 + r) - p->x, c = p->z - (BZ0 - r), d = (BZ1 + r) - p->z, m = fminf(a, fminf(b, fminf(c, d)));
    if (m == a) { p->x = BX0 - r; v->x = 0; } else if (m == b) { p->x = BX1 + r; v->x = 0; } else if (m == c) { p->z = BZ0 - r; v->z = 0; } else { p->z = BZ1 + r; v->z = 0; }
  }
  if (p->y <= 0) { p->y = 0; if (v->y < 0) v->y = 0; if (g) *g = true; } else if (g) *g = false;
}
bool world_raycast(v3 o, v3 d, float m, float *t, v3 *n) {
  float best = 1e9f; bool hit = false;
  if (d.y < -1e-3f) { float k = -o.y / d.y; if (k >= 0 && k <= m) { best = k; hit = true; if (n) *n = v3_(0,1,0); } }
  float t0 = 0, t1 = m; bool ok = true;
  float mn[3] = {BX0, 0, BZ0}, mx[3] = {BX1, 4, BZ1}, O[3] = {o.x, o.y, o.z}, D[3] = {d.x, d.y, d.z};
  for (int i = 0; i < 3 && ok; i++) {
    if (fabsf(D[i]) < 1e-6f) { if (O[i] < mn[i] || O[i] > mx[i]) ok = false; continue; }
    float a = (mn[i] - O[i]) / D[i], b = (mx[i] - O[i]) / D[i];
    if (a > b) { float s = a; a = b; b = s; }
    t0 = fmaxf(t0, a); t1 = fminf(t1, b); if (t0 > t1) ok = false;
  }
  if (ok && t0 < best) { best = t0; hit = true; if (n) *n = v3_(0,0,1); }
  if (hit) *t = best;
  return hit;
}
bool world_visible(v3 a, v3 b) { return true; }
float world_floor_y(v3 p) { return p.y >= 0 ? 0 : -1e9f; }
bool world_in_lab(v3 p) { return false; }
v3 world_lab(float x, float z) { return v3_(z, 0, -x); }
float world_lab_yaw(float d) { return d * DS_PI / 180; }
bool world_place(const char *n, v3 *p, float *y) { *p = v3_(0,0,0); *y = 0; return true; }
void world_door(float o) {}

static t3_scene *scene; static t3_camera *cam;
static actor_t *PL;
static const actor_palette RED = {{-30, 1, 1}, {140, 1, .9f}, {0, 1, 1}, {0, 1, 1}};
static const actor_palette BLU = {{180, 1, 1}, {200, 1, .9f}, {200, .8f, 1}, {0, 1, 1}};

/* the tuning table */
#define FOE(M, P, NAME, W, AI, HP, POW, SP, X, Z) { M, P, 1, NAME, W, AI, HP, POW, SP, X, Z, NULL }
#define NT 17
static fight_def TIER[NT];
static actor_t *SAM;
static const fight_bark BARKS[] = {
  { FB_START, "vee", "Vee", 0xff7a1a, "Okay. Let us do this properly." },
  { FB_PHASE2, "root", "Root", 0xe8433a, "Back at 140, we did not have a wiki. We had me." },
  { FB_OBJ_WARN, "dex", "Dex", 0x58c8ff, "The objective! Watch the objective!" },
  { FB_HAZARD, "dex", "Dex", 0x58c8ff, "Heads up, that is live!" },
  { FB_PARRY, "root", "Root", 0xe8433a, "Huh. Somebody taught you right." },
  { FB_PLAYER_LOW, "dex", "Dex", 0x58c8ff, "Breathe. Reset. Go again." },
  { FB_FOE_LOW, "root", "Root", 0xe8433a, "Six years. That part. Six years." },
  { FB_ASSIST, "bex", "Bex", 0x60e090, "Labeled. Dated. Go!" },
  { FB_FINISH, "vee", "Vee", 0xff7a1a, "Lights off. Door locked." },
};
static void tiers(void) {
  static const actor_palette *r = &RED, *b = &BLU;
  fight_def base = { "CONFLICT RESOLUTION", "versus the whole mailing list", 0, {{0}}, {0, 0, 0}, 13, 1, 1, 31, WP_ARC, false, false };
  for (int i = 0; i < 7; i++) TIER[i] = base;
  fighter_def t0[] = { FOE(AM_F, *r, "Vinyl Vic", WP_ARC, AI_DUELIST, 100, 1.f, .9f, 0, 5), FOE(AM_C, *b, "Dee Burr", WP_TWIN, AI_SKIRMISH, 70, .9f, .95f, 3, 4) };
  TIER[0].count = 2; memcpy(TIER[0].foes, t0, sizeof t0);
  fighter_def t1[] = { FOE(AM_F, *r, "Vinyl Vic", WP_ARC, AI_DUELIST, 130, 1.f, .9f, 0, 5) };
  TIER[1].count = 1; memcpy(TIER[1].foes, t1, sizeof t1); TIER[1].title = "FIRST CONTACT"; TIER[1].sub = "versus Vinyl Vic";
  fighter_def t2[] = { FOE(AM_F, *r, "Big Rack", WP_MAUL, AI_BRUISER, 150, 1.f, .9f, 0, 5), FOE(AM_C, *b, "Dee Burr", WP_TWIN, AI_SKIRMISH, 80, 1.f, 1.f, 3, 4) };
  TIER[2].count = 2; memcpy(TIER[2].foes, t2, sizeof t2); TIER[2].sub = "versus the shop rats";
  fighter_def t3[] = { FOE(AM_F, *r, "Solder Sue", WP_SPEAR, AI_DUELIST, 100, .95f, 1.05f, 0, 5), FOE(AM_C, *b, "Dee Burr", WP_TWIN, AI_SKIRMISH, 70, .9f, 1.1f, 3, 4), FOE(AM_B, *r, "Lens Cap", WP_STAFF, AI_CASTER, 60, .85f, 1.f, -3, 7) };
  TIER[3].count = 3; memcpy(TIER[3].foes, t3, sizeof t3); TIER[3].sub = "versus the bylaws committee";
  fighter_def t4[] = { FOE(AM_F, *r, "Big Rack", WP_MAUL, AI_BRUISER, 110, 1.f, 1.f, 0, 6), FOE(AM_C, *b, "Dee Burr", WP_TWIN, AI_SKIRMISH, 65, 1.f, 1.2f, 3, 4), FOE(AM_B, *r, "Solder Sue", WP_ARC, AI_DUELIST, 90, 1.f, 1.15f, -3, 4) };
  TIER[4].count = 3; memcpy(TIER[4].foes, t4, sizeof t4); TIER[4].sub = "versus the night shift";
  fighter_def t5[] = { FOE(AM_D, *r, "The Landlord", WP_ARC, AI_BOSS, 400, 1.1f, 1.2f, 0, 6) };
  TIER[5].count = 1; memcpy(TIER[5].foes, t5, sizeof t5); TIER[5].title = "LEASE TERMINATION"; TIER[5].sub = "versus The Landlord";
  fighter_def t6[] = { FOE(AM_F, *r, "Lazer Lou", WP_STAFF, AI_CASTER, 90, .85f, 1.f, 0, 8), FOE(AM_C, *b, "Dee Burr", WP_TWIN, AI_SKIRMISH, 60, .7f, .95f, 3, 4) };
  TIER[6].count = 2; memcpy(TIER[6].foes, t6, sizeof t6); TIER[6].sub = "versus the laser queue";
  /* v1.1: the day's fights with their twists, numbers as in day.mjs */
  for (int i = 7; i < NT; i++) { TIER[i] = base; TIER[i].barks = BARKS; TIER[i].nbarks = sizeof BARKS / sizeof *BARKS; TIER[i].radius = 11; }
  fighter_def f7[] = { FOE(AM_F, *r, "Ruth", WP_ARC, AI_DUELIST, 55, .45f, .8f, 0, 3) };
  TIER[7].count = 1; memcpy(TIER[7].foes, f7, sizeof f7); TIER[7].title = "WALK THROUGH ORIENTATION"; TIER[7].sub = "As revised"; TIER[7].twist = FT_TUTORIAL; TIER[7].tutorial = true; TIER[7].radius = 5.5f;
  fighter_def f8[] = { FOE(AM_F, *r, "Vinyl Vic", WP_STAFF, AI_CASTER, 110, .7f, .95f, 0, 6) };
  TIER[8].count = 1; memcpy(TIER[8].foes, f8, sizeof f8); TIER[8].title = "CONFLICT RESOLUTION"; TIER[8].sub = "Polyvinyl chloride is not on the list"; TIER[8].twist = FT_TIMER; TIER[8].twist_a = 60; TIER[8].obj_pos = v3_(-6, 0, 3); TIER[8].obj_label = "STOP THE CUT";
  fighter_def f9[] = { FOE(AM_C, *b, "Rennick", WP_TWIN, AI_SKIRMISH, 70, .75f, 1.05f, -2.5f, 5), FOE(AM_F, *r, "Dobbs", WP_MAUL, AI_BRUISER, 110, .85f, .85f, 2.5f, 5) };
  TIER[9].count = 2; memcpy(TIER[9].foes, f9, sizeof f9); TIER[9].title = "THEFT OF PROPERTY"; TIER[9].sub = "HSL or member owned"; TIER[9].twist = FT_RETRIEVE;
  fighter_def f10[] = { FOE(AM_C, *b, "The Champ", WP_TWIN, AI_DUELIST, 120, .9f, 1.2f, 0, 5) };
  TIER[10].count = 1; memcpy(TIER[10].foes, f10, sizeof f10); TIER[10].title = "FREE PLAY"; TIER[10].sub = "Insert coin"; TIER[10].twist = FT_SCORE; TIER[10].radius = 8;
  fighter_def f11[] = { FOE(AM_B, *r, "Bex", WP_SPEAR, AI_DUELIST, 160, 1.f, 1.f, 0, 6) };
  TIER[11].count = 1; memcpy(TIER[11].foes, f11, sizeof f11); TIER[11].title = "THE FRIDGE"; TIER[11].sub = "Everything in it has a home"; TIER[11].twist = FT_HOLD; TIER[11].twist_a = 1; TIER[11].obj_pos = v3_(0, 0, 10);
  fighter_def f12[] = { FOE(AM_F, *r, "Cal", WP_MAUL, AI_BRUISER, 180, 1.f, .9f, 0, 6), FOE(AM_C, *b, "Crew", WP_SPEAR, AI_SKIRMISH, 70, .75f, 1.f, -4, 5), FOE(AM_C, *r, "Crew", WP_ARC, AI_SKIRMISH, 70, .75f, 1.f, 4, 5) };
  TIER[12].count = 3; memcpy(TIER[12].foes, f12, sizeof f12); TIER[12].title = "THE BACK LOT"; TIER[12].sub = "Three of them, one of you"; TIER[12].twist = FT_DEFEND; TIER[12].twist_a = 70; TIER[12].radius = 12; TIER[12].obj_pos = v3_(0, 0, 8);
  fighter_def f13[] = { FOE(AM_D, *r, "Brant Holloway", WP_ARC, AI_BOSS, 300, 1.f, 1.05f, 0, 7), FOE(AM_B, *b, "Intern", WP_STAFF, AI_CASTER, 40, .45f, .95f, -6, 3), FOE(AM_C, *r, "Intern", WP_TWIN, AI_SKIRMISH, 40, .45f, .95f, 6, 3) };
  TIER[13].count = 3; memcpy(TIER[13].foes, f13, sizeof f13); TIER[13].title = "A BRIEF WORD FROM OUR SPONSOR"; TIER[13].sub = "Brant Holloway, growth"; TIER[13].twist = FT_PROTECT;
  fighter_def f14[] = { FOE(AM_F, *r, "Root", WP_MAUL, AI_BOSS, 320, 1.1f, 1.f, 0, 7) };
  TIER[14].count = 1; memcpy(TIER[14].foes, f14, sizeof f14); TIER[14].title = "ACCESS DENIED"; TIER[14].sub = "Root, member since the old building"; TIER[14].twist = FT_BOSS; TIER[14].phase2_at = .5f;
  fighter_def f15[] = { FOE(AM_D, ACTOR_PAL_NONE, "Burnout", WP_SPEAR, AI_BOSS, 380, 1.1f, 1.1f, 0, 8) };
  TIER[15].count = 1; memcpy(TIER[15].foes, f15, sizeof f15); TIER[15].title = "LOCK UP"; TIER[15].sub = "Everyone else went home"; TIER[15].twist = FT_BOSS; TIER[15].phase2_at = .5f; TIER[15].finisher_qte = true; TIER[15].radius = 12;
  fighter_def f16[] = { FOE(AM_C, *b, "Haunter", WP_SPEAR, AI_SKIRMISH, 80, .5f, 1.05f, 0, 3) };
  TIER[16].count = 1; memcpy(TIER[16].foes, f16, sizeof f16); TIER[16].title = "PROP CHECK"; TIER[16].sub = "Foam, mostly"; TIER[16].radius = 5.5f;
  fighter_def ally = FOE(AM_B, *b, "Bex", WP_SPEAR, AI_DUELIST, 100, 1.f, 1.f, 0, 0);
  for (int i = 12; i <= 15; i++) { TIER[i].assist[0] = ally; TIER[i].nassist = 1; }
}

/* ---- the bot ---- */
static int bot_kind;   /* 0 none, 1 poor, 2 good */
static intent_t BI;
static float bot_mash, bot_jog, bot_think; static int bot_seen[4]; static int bot_plan_kind; static float bot_plan_t; static bool bot_guard_prev;
static bool bot_cares;   /* a poor bot may or may not mind the objective, decided per fight */
static void bot_reset(void) { bot_mash = bot_jog = bot_think = 0; memset(bot_seen, 0, sizeof bot_seen); bot_plan_kind = 0; bot_plan_t = 0; bot_guard_prev = false; bot_cares = ds_rand() < .5f; }
static bool dead(const fighter_t *o) { return o->st == F_KO || o->st == F_KNEEL; }
static void bot_step(float dt) {
  memset(&BI, 0, sizeof BI); BI.weapon_pick = -1;
  fighter_t *P = &FG.P;
  const twist_t *TW = &FG.tw;
  bool good = bot_kind == 2, minds = good || bot_cares;
  /* the finisher's prompt */
  if (FG.fin_t > .45f && (good || ds_rand() < .03f)) { BI.heavy = true; return; }
  if (FG.phase != PH_FIGHT) return;
  int t = -1;
  { float best = 1e9f; for (int i = 0; i < FG.nfoe; i++) { fighter_t *o = &FG.foe[i]; if (dead(o)) continue; float d = v3_len(v3_sub(o->pos, P->pos)) - (minds && o->goal && o->aggro <= 0 ? 4 : 0); if (d < best) { best = d; t = i; } } }
  if (FG.lock >= 0 && !(minds && t >= 0 && FG.foe[t].goal)) t = FG.lock;
  if (t < 0) return;
  fighter_t *T = &FG.foe[t];
  /* where to stand: the foe, unless the twist says otherwise */
  v3 goal = T->pos; float reach = 1.7f; bool swing_obj = false;
  if (minds) {
    if (TW->kind == FT_TIMER && !TW->obj_done) { goal = TW->obj; reach = 1.4f; swing_obj = true; }
    if (TW->kind == FT_HOLD && TW->meter > (good ? .45f : .7f)) { goal = TW->obj; reach = 2.4f; swing_obj = true; }
    if (TW->kind == FT_RETRIEVE && TW->carrier == -1) { goal = TW->obj; reach = .3f; }
    if (TW->kind == FT_RETRIEVE && TW->carrier >= 0) { t = TW->carrier; T = &FG.foe[t]; goal = T->pos; }
  }
  float react = bot_kind == 1 ? .42f : .2f;
  float act_p = bot_kind == 1 ? .1f : .72f, parry_share = bot_kind == 1 ? .1f : .4f;
  if (TW->kind == FT_TUTORIAL) { act_p = good ? .9f : .5f; parry_share = TW->step == TUT_PARRY ? .9f : TW->step == TUT_GUARD ? 1 : .3f; }
  fighter_t *threat = NULL;
  for (int i = 0; i < FG.nfoe; i++) {
    fighter_t *o = &FG.foe[i];
    if (o->st == F_ATTACK && v3_len(v3_sub(o->pos, P->pos)) < 5.5f && !(o->goal && o->aggro <= 0)) {
      if (!o->released && o->tele > 0) {
        if (bot_seen[i] != o->mv + 1) {
          bot_seen[i] = o->mv + 1;
          float r = ds_rand();
          bot_plan_kind = r < act_p ? (ds_rand() < parry_share ? 2 : 1) : 0;
          bot_plan_t = o->tele + .06f;
          (void)react;
        }
        threat = o;
      }
    } else if (o->st != F_ATTACK) bot_seen[i] = 0;
  }
  bot_plan_t -= dt;
  v3 f = v3_norm(v3_(FG.cam_look.x - FG.cam_pos.x, 0, FG.cam_look.z - FG.cam_pos.z));
  v3 r = v3_(f.z, 0, -f.x);
  v3 to = v3_sub(goal, P->pos); to.y = 0;
  float dist = v3_len(to);
  v3 dir = dist > .01f ? v3_scale(to, 1 / dist) : f;
  v3 go = v3_(0, 0, 0);
  if (dist > reach) go = dir;
  /* the tutorial's first lesson: walk */
  if (TW->kind == FT_TUTORIAL && TW->step == TUT_MOVE) go = v3_(sinf(FG.clock), 0, cosf(FG.clock));
  /* the good bot steps off the light rail when the bell goes */
  if (good && TW->kind == FT_RETRIEVE && TW->train_st >= 1) {
    float tz = FG.def.center.z + 3.5f;
    if (fabsf(P->pos.z - tz) < 2.4f) go = v3_(0, 0, P->pos.z < tz ? -1 : 1);
  }
  BI.my = v3_dot(go, f); BI.mx = -v3_dot(go, r);
  if (bot_plan_kind && threat) {
    if (bot_plan_kind == 1 && threat->tele < .1f) { BI.dodge = true; v3 s2 = v3_(dir.z, 0, -dir.x); BI.my = v3_dot(s2, f); BI.mx = -v3_dot(s2, r); bot_plan_kind = 0; }
    if (bot_plan_kind == 2 && threat->tele < .08f) { BI.guard = true; BI.guard_pressed = !bot_guard_prev; }
  }
  if (bot_plan_kind == 2 && bot_plan_t > -.25f && !threat && bot_guard_prev) BI.guard = true;
  if (bot_plan_kind == 2 && bot_plan_t <= -.25f) bot_plan_kind = 0;
  bot_guard_prev = BI.guard;
  if (good) for (int i = 0; i < FG.nfoe; i++) {
    fighter_t *o = &FG.foe[i];
    if (o->st == F_CHARGE && o->charge >= o->stun - .12f && ds_rand() < .35f) { BI.dodge = true; v3 s2 = v3_(dir.z, 0, -dir.x); BI.my = v3_dot(s2, f); BI.mx = -v3_dot(s2, r); }
    if (o->st == F_LEAP && o->t > .95f && P->st != F_ROLL) { BI.dodge = true; v3 a = v3_norm(v3_sub(P->pos, o->leap_to)); a.y = 0; BI.my = v3_dot(a, f); BI.mx = -v3_dot(a, r); }
  }
  /* hazards: the good bot jumps the sweep and rolls out of the marks */
  if (good) {
    if (TW->sweep_on > .5f) {
      v3 d = v3_(sinf(TW->sweep), 0, cosf(TW->sweep)), rel = v3_sub(P->pos, FG.def.center); rel.y = 0;
      float ang = atan2f(rel.x, rel.z), diff = ds_wrap_pi(ang - TW->sweep); (void)d;
      if (diff > 0 && diff < .2f + .12f / fmaxf(v3_len(rel), 1) && P->st == F_FREE) BI.jump = true;
    }
    if (TW->nstrike > 0 && TW->strike_t < .5f && P->st != F_ROLL)
      for (int k = 0; k < TW->nstrike; k++) if (v3_len(v3_sub(P->pos, TW->strike[k])) < 1.7f) { v3 a = v3_norm(v3_sub(P->pos, TW->strike[k])); a.y = 0; if (v3_len(a) < .1f) a = r; BI.dodge = true; BI.my = v3_dot(a, f); BI.mx = -v3_dot(a, r); }
  }
  /* call the ally */
  if (FG.def.nassist > 0 && !FG.ally_on && FG.ally_cd <= 0 && (good ? ds_rand() < .02f : ds_rand() < .002f)) BI.assist = true;
  if (BI.guard) return;
  if (good && bot_plan_kind && threat) return;
  bot_mash -= dt;
  bool in_range = dist < reach + .6f || (!swing_obj && v3_len(v3_sub(T->pos, P->pos)) < 2.3f);
  if (good) for (int i = 0; i < FG.nfoe; i++) { fighter_t *o = &FG.foe[i]; if (o->st == F_ATTACK && !o->released && o->tele < .55f && v3_len(v3_sub(o->pos, P->pos)) < 4.5f) in_range = false; }
  if (TW->kind == FT_RETRIEVE && TW->carrier == -1 && minds) in_range = false;
  if (TW->kind == FT_TUTORIAL && TW->step >= TUT_DODGE && TW->step <= TUT_PARRY) in_range = false;   /* a pupil waits for the swing */
  if (bot_mash <= 0 && in_range) {
    float hv = TW->kind == FT_TUTORIAL && TW->step == TUT_HEAVY ? .9f : bot_kind == 1 ? .12f : TW->kind == FT_SCORE ? .3f : .16f;
    if (bot_kind == 1) { bot_mash = ds_rr(.25f, .6f); if (ds_rand() < hv) BI.heavy = true; else BI.light = true; }
    else { bot_mash = ds_rr(.14f, .26f); if (ds_rand() < hv) BI.heavy = true; else BI.light = true; }
    if (good && TW->kind == FT_SCORE && ds_rand() < .05f) BI.weapon_step = 1;
  }
  if (FG.meter >= 100 && ds_rand() < (bot_kind == 1 ? .01f : .05f)) BI.special = true;
  if (TW->kind == FT_TUTORIAL && TW->step == TUT_DODGE && ds_rand() < .02f) BI.dodge = true;
}

/* ---- the frame ---- */
static int frame, tier = 0, batch_tier, batch_bot, batch_rep, batch_end = 7, start_wp = -1; static bool batch, started, meter_full, tutorial, foes_weak, bot_weak, checkpoint, zone; static float fight_time;
static actor_t *story; static int rounds;
static t3_mesh *mk_box(float w, float h, float d, uint32_t c, float x, float y, float z) {
  t3_material *m = t3_mesh_lambert_material_new(c); m->fog = true; m->version++;
  t3_mesh *b = t3_mesh_new(t3_box_geometry_new(w, h, d, 1, 1, 1), m);
  t3_object_set_position(&b->base, x, y, z); t3_object_add(&scene->base, &b->base);
  return b;
}
void game_init(void) {
  scene = t3_scene_new();
  scene->has_background = true; scene->background = t3_color_hex(0x7fb2dc);
  scene->fog = t3_fog_linear(0x7fb2dc, 40, 120);
  cam = t3_perspective_camera_new(46, 16.f / 9.f, .05f, 400);
  t3_light *h = t3_hemisphere_light_new(0xffffff, 0xb0a8b8, 2.2f); t3_object_add(&scene->base, h);
  t3_light *d = t3_directional_light_new(0xfff4e0, 1.6f); t3_object_set_position(d, 2, 5, 4); t3_object_add(&scene->base, d);
  mk_box(120, .2f, 120, 0x77706a, 0, -.1f, 0);
  for (int i = -12; i <= 12; i++) { mk_box(.03f, .01f, 40, 0x5d5853, i * 2.f, .002f, 0); mk_box(40, .01f, .03f, 0x5d5853, 0, .002f, i * 2.f); }
  mk_box(BX1 - BX0, 4, BZ1 - BZ0, 0x8a7a68, (BX0 + BX1) / 2, 2, (BZ0 + BZ1) / 2);
  tiers();
  fight_init(scene, cam);
  player_init(cam);
  PL = actor_new(AM_D, &ACTOR_PAL_NONE, 1, scene);
  actor_place(PL, v3_(0, 0, -3), 0);
  player_set_actor(PL);
}
static void begin(int t) {
  actor_place(PL, v3_(0, 0, -3), 0);
  fight_def D = TIER[t];
  if (story) {   /* a story actor, weapon in hand, standing where the scene left it; a second fight in a new palette */
    D.foes[0].actor = story; D.foes[1 % D.count].actor = D.count > 1 ? NULL : story;
    if (rounds == 1) { D.foes[1].pal = RED; D.count = 2; D.foes[1].model = AM_C; D.foes[1].name = "Dee Burr"; D.foes[1].hp = 40; D.foes[1].power = 1; D.foes[1].speed = 1; D.foes[1].ai = AI_SKIRMISH; D.foes[1].weapon = WP_TWIN; D.foes[1].place_x = 3; D.foes[1].place_z = 4; D.foes[1].actor = NULL; }
  }
  if (tutorial) D.tutorial = true;
  if (start_wp >= 0) { D.weapons = 31; D.start_weapon = start_wp; }
  if (checkpoint) D.start_phase = 1;
  if (zone) { D.battle_zone = true; D.mods = MOD_TURBO; }
  if (D.twist == FT_PROTECT) {
    if (!SAM) SAM = actor_new(AM_C, &BLU, 1, scene);
    D.protect_actor = SAM;
  } else if (SAM) actor_visible(SAM, false);
  fight_start(&D, PL);
  if (meter_full) FG.meter = 100;
  if (foes_weak) for (int i = 0; i < FG.nfoe; i++) FG.foe[i].hp = FG.foe[i].ghost = 8;
  if (bot_weak) FG.P.hp = FG.P.ghost = 6;
  bot_reset(); fight_time = 0; started = true;
}
static void sim_step(float dt) {
  if (bot_kind) bot_step(dt);
  fight_state s = fight_update(dt, bot_kind ? &BI : input_get());
  fight_time += dt;
  if (story && s != FIGHT_RUNNING && !batch) {
    bool vis = actor_is_visible(story);
    ds_log("story round %d ended %d visible %d kneel %d weapon %d", rounds, s, vis, FG.foe[0].st == F_KNEEL, weapon_of(story));
    fight_end();
    ds_log("after fight_end visible %d weapon %d", actor_is_visible(story), weapon_of(story));
    if (++rounds < 2) { foes_weak = true; begin(1); }
  }
  if (batch) {
    bool over = s != FIGHT_RUNNING || fight_time > 400;
    if (over) {
      fight_result R; fight_result_get(&R);
      ds_log("[bot] tier=%d bot=%s result=%s time=%.1f dmg=%.1f hp=%.0f meter=%.0f score=%d rank=%c line=%s fin=%d", batch_tier, batch_bot == 1 ? "poor" : "good", s == FIGHT_WON ? "WON" : s == FIGHT_LOST ? "LOST" : "TIMEOUT", FG.phase == PH_FIGHT ? fight_time : fight_time - FG.phase_t, FG.damage_taken, FG.P.hp, FG.meter, R.score, R.rank, FG.tw.fail ? FG.tw.fail : "-", R.finisher);
      fight_end();
      batch_rep++;
      if (batch_rep >= 10) { batch_rep = 0; batch_bot++; if (batch_bot > 2) { batch_bot = 1; batch_tier++; } }
      if (batch_tier >= batch_end) { batch = false; started = false; bot_kind = 0; ds_log("[bot] done"); __builtin_trap(); }
      bot_kind = batch_bot;
      begin(batch_tier);
    }
  }
}
void game_frame(float dt) {
  frame++;
  if (frame <= 3) {
    bool ko = input_key_down(0x04 + 'O' - 'A'), kw = input_key_down(0x04 + 'W' - 'A');
    for (int i = 0; i < 10; i++) if (input_key_down(0x1e + i)) { if (kw) { if (i < 5) start_wp = i; } else if (ko) tier = 7 + i; else if (i < 7) tier = i; }
    if (input_key_down(0x04 + 'N' - 'A')) bot_kind = 1;
    if (input_key_down(0x04 + 'G' - 'A')) bot_kind = 2;
    if (input_key_down(0x04 + 'X' - 'A')) batch = true;
    if (input_key_down(0x04 + 'Y' - 'A')) { batch_end = NT; if (tier < 7) tier = 7; }
    if (input_key_down(0x04 + 'C' - 'A')) checkpoint = true;
    if (input_key_down(0x04 + 'U' - 'A')) batch_end = -1;   /* only the chosen tier */
    if (input_key_down(0x04 + 'Z' - 'A')) zone = true;
    if (input_key_down(0x04 + 'M' - 'A')) meter_full = true;
    if (input_key_down(0x04 + 'T' - 'A')) tutorial = true;
    if (input_key_down(0x04 + 'P' - 'A')) bot_kind = 3;
    if (input_key_down(0x04 + 'H' - 'A')) foes_weak = true;
    if (input_key_down(0x04 + 'L' - 'A')) bot_weak = true;
    if (input_key_down(0x04 + 'S' - 'A') && !story) { story = actor_new(AM_B, &BLU, 1, scene); actor_place(story, v3_(-2, 0, 2.5f), 3); actor_play(story, "Idle_Loop", 1, true, 0); weapon_equip(story, WP_SPEAR); }
  }
  if (frame == 4) {
    if (bot_kind == 3) {   /* the walk; with KeyB, back to a wall with the camera wanting to be inside it */
      bot_kind = 0; started = false;
      if (input_key_down(0x04 + 'B' - 'A')) player_teleport(v3_(5.5f, 0, 0), -DS_PI / 2); else player_camera_snap();
      return;
    }
    if (batch) { if (batch_end < 0) batch_end = tier + 1; batch_tier = tier; batch_bot = 1; bot_kind = 1; begin(batch_tier); }
    else begin(tier);
  }
  if (frame < 4) return;
  if (!started && !batch) {
    player_update(dt, input_get());
    fx_update(dt, cam);
    { /* foot slip: while a foot is the lower one and near the ground it should stand still */
      static v3 pl, pr; static float slip, n, spd;
      v3 l = actor_bone_pos(PL, "leftToes"), r = actor_bone_pos(PL, "rightToes");
      v3 f = l.y < r.y ? l : r, pf = l.y < r.y ? pl : pr;
      float s = sqrtf((f.x-pf.x)*(f.x-pf.x)+(f.z-pf.z)*(f.z-pf.z)) / fmaxf(dt, 1e-3f);
      if (f.y < .12f) { slip = n == 0 ? s : fminf(slip, s); n++; }
      pl = l; pr = r;
      v3 pp = player_pos(); static v3 pq; spd = v3_len(v3_sub(pp, pq)) / fmaxf(dt, 1e-3f); pq = pp;
      if (frame % 40 == 0) { ds_log("walk f=%d speed=%.2f stance_foot_min_speed=%.2f pos=%.1f,%.1f", frame, spd, n ? slip : -1, pp.x, pp.z); n = 0; }
    }
    return;
  }
  if (batch) { for (int i = 0; i < 90 && batch; i++) sim_step(1 / 60.f); }
  else if (rounds < 2) sim_step(dt);
  else actor_update(story, dt);
  if (!batch && (frame % 60) == 0) ds_log("f=%d phase=%d P.st=%d clip=%s lock=%d wp=%d foe0=%d", frame, FG.phase, FG.P.st, FG.P.clip ? FG.P.clip : "", FG.lock, FG.P.wp, FG.foe[0].st);
  if (!batch && FG.tw.kind && (frame % 120) == 0) ds_log("twist=%d meter=%.1f/%.0f obj_hp=%.0f carrier=%d score=%.0f step=%d/%.1f/%.1f style=%.0f", FG.tw.kind, FG.tw.meter, FG.tw.meter_max, FG.tw.obj_hp, FG.tw.carrier, FG.tw.score, FG.tw.step, FG.tw.step_n, FG.tw.step_t, FG.tw.style);
  if (!batch && bot_kind == 0 && FG.phase == PH_FIGHT && (frame % 240) == 0) ds_log("t=%.1f hp=%.0f meter=%.0f combo=%d foe0=%.0f st=%d", fight_time, FG.P.hp, FG.meter, FG.combo, FG.foe[0].hp, FG.foe[0].st);
}
void game_draw(void) {
  render_scene(scene, cam);
  if (started) fight_draw_hud();
}
#endif
