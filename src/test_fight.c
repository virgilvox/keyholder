/* test_fight.c: runs the fight module alone (KH_TEST_FIGHT): a stub flat world, a two foe
 * fight, scripted input through tools/shot.mjs, and a bot that plays whole fights headless
 * to tune difficulty. Keys held in the first frames choose: Digit1..6 a tier, KeyN a poor
 * bot, KeyG a good bot, KeyX all tiers in a batch with no drawing between them, KeyM start
 * with a full meter, KeyT the tutorial flags, KeyH foes nearly beaten, KeyL the player nearly
 * beaten, KeyP the third person walk instead of a fight. */
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
static fight_def TIER[7];
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
}

/* ---- the bot ---- */
static int bot_kind;   /* 0 none, 1 poor, 2 good */
static intent_t BI;
static float bot_mash, bot_jog, bot_think; static int bot_seen[4]; static int bot_plan_kind; static float bot_plan_t; static bool bot_guard_prev;
static void bot_reset(void) { bot_mash = bot_jog = bot_think = 0; memset(bot_seen, 0, sizeof bot_seen); bot_plan_kind = 0; bot_plan_t = 0; bot_guard_prev = false; }
static void bot_step(float dt) {
  memset(&BI, 0, sizeof BI); BI.weapon_pick = -1;
  fighter_t *P = &FG.P;
  if (FG.phase != PH_FIGHT) return;
  int t = FG.lock;
  if (t < 0) { float best = 1e9f; for (int i = 0; i < FG.nfoe; i++) { fighter_t *o = &FG.foe[i]; if (o->st == F_KO || o->st == F_KNEEL) continue; float d = v3_len(v3_sub(o->pos, P->pos)); if (d < best) { best = d; t = i; } } }
  if (t < 0) return;
  fighter_t *T = &FG.foe[t];
  /* the threat: a foe swinging or about to, close enough */
  float react = bot_kind == 1 ? .42f : .2f;
  float act_p = bot_kind == 1 ? .1f : .72f, parry_share = bot_kind == 1 ? .1f : .4f;
  fighter_t *threat = NULL;
  for (int i = 0; i < FG.nfoe; i++) {
    fighter_t *o = &FG.foe[i];
    if (o->st == F_ATTACK && v3_len(v3_sub(o->pos, P->pos)) < 5.5f) {
      if (!o->released && o->tele > 0) {
        if (bot_seen[i] != o->mv + 1) {
          bot_seen[i] = o->mv + 1;
          /* decide once per telegraph, after a human reaction */
          float r = ds_rand();
          bot_plan_kind = r < act_p ? (ds_rand() < parry_share ? 2 : 1) : 0;
          bot_plan_t = o->tele + .06f;
          (void)react;
        }
        threat = o;
      }
    } else if (o->st != F_ATTACK) bot_seen[i] = 0;
  }
  if (bot_plan_t > 0) bot_plan_t -= dt;
  /* aim: the stick is camera relative, so move in camera space */
  v3 f = v3_norm(v3_(FG.cam_look.x - FG.cam_pos.x, 0, FG.cam_look.z - FG.cam_pos.z));
  v3 r = v3_(f.z, 0, -f.x);
  v3 to = v3_sub(T->pos, P->pos); to.y = 0;
  float dist = v3_len(to);
  v3 dir = dist > .01f ? v3_scale(to, 1 / dist) : f;
  float reach = 1.7f;
  v3 go = v3_(0, 0, 0);
  if (dist > reach) go = dir;
  /* the good bot does not stand between two foes */
  BI.my = v3_dot(go, f); BI.mx = -v3_dot(go, r);
  /* the planned defence, timed to the release */
  if (bot_plan_kind && threat) {
    if (bot_plan_kind == 1 && threat->tele < .1f) { BI.dodge = true; v3 s = v3_(dir.z, 0, -dir.x); BI.my = v3_dot(s, f); BI.mx = -v3_dot(s, r); bot_plan_kind = 0; }
    if (bot_plan_kind == 2 && threat->tele < .08f) { BI.guard = true; BI.guard_pressed = !bot_guard_prev; }
  }
  if (bot_plan_kind == 2 && bot_plan_t > -.25f && !threat && bot_guard_prev) BI.guard = true;
  if (bot_plan_kind == 2 && bot_plan_t <= -.25f) bot_plan_kind = 0;
  bot_guard_prev = BI.guard;
  /* a caster's windup or a boss's leap: roll when it comes due */
  if (bot_kind == 2) for (int i = 0; i < FG.nfoe; i++) {
    fighter_t *o = &FG.foe[i];
    if (o->st == F_CHARGE && o->charge >= o->stun - .12f && ds_rand() < .35f) { BI.dodge = true; v3 s = v3_(dir.z, 0, -dir.x); BI.my = v3_dot(s, f); BI.mx = -v3_dot(s, r); }
    if (o->st == F_LEAP && o->t > .95f && P->st != F_ROLL) { BI.dodge = true; v3 a = v3_norm(v3_sub(P->pos, o->leap_to)); a.y = 0; BI.my = v3_dot(a, f); BI.mx = -v3_dot(a, r); }
  }
  if (BI.guard) return;
  if (bot_kind == 2 && bot_plan_kind && threat) return;   /* the good bot does not swing into a blow it means to meet */
  /* attack: a mash for the poor bot, combos and heavies for the good one */
  bot_mash -= dt;
  bool in_range = dist < reach + .6f;
  /* the good bot keeps its swings short of a blow it can see coming */
  if (bot_kind == 2) for (int i = 0; i < FG.nfoe; i++) { fighter_t *o = &FG.foe[i]; if (o->st == F_ATTACK && !o->released && o->tele < .55f && v3_len(v3_sub(o->pos, P->pos)) < 4.5f) in_range = false; }
  if (bot_mash <= 0 && in_range) {
    if (bot_kind == 1) { bot_mash = ds_rr(.25f, .6f); if (ds_rand() < .12f) BI.heavy = true; else BI.light = true; }
    else { bot_mash = ds_rr(.14f, .26f); if (ds_rand() < .16f) BI.heavy = true; else BI.light = true; }
  }
  if (FG.meter >= 100 && ds_rand() < (bot_kind == 1 ? .01f : .05f)) BI.special = true;
}

/* ---- the frame ---- */
static int frame, tier = 0, batch_tier, batch_bot, batch_rep; static bool batch, started, meter_full, tutorial, foes_weak, bot_weak; static float fight_time;
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
      ds_log("[bot] tier=%d bot=%s result=%s time=%.1f dmg=%.1f hp=%.0f meter=%.0f", batch_tier, batch_bot == 1 ? "poor" : "good", s == FIGHT_WON ? "WON" : s == FIGHT_LOST ? "LOST" : "TIMEOUT", FG.phase == PH_FIGHT ? fight_time : fight_time - FG.phase_t, FG.damage_taken, FG.P.hp, FG.meter);
      fight_end();
      batch_rep++;
      if (batch_rep >= 10) { batch_rep = 0; batch_bot++; if (batch_bot > 2) { batch_bot = 1; batch_tier++; } }
      if (batch_tier >= 7) { batch = false; started = false; bot_kind = 0; ds_log("[bot] done"); return; }
      bot_kind = batch_bot;
      begin(batch_tier);
    }
  }
}
void game_frame(float dt) {
  frame++;
  if (frame <= 3) {
    for (int i = 0; i < 7; i++) if (input_key_down(0x1e + i)) tier = i;
    if (input_key_down(0x04 + 'N' - 'A')) bot_kind = 1;
    if (input_key_down(0x04 + 'G' - 'A')) bot_kind = 2;
    if (input_key_down(0x04 + 'X' - 'A')) batch = true;
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
    if (batch) { batch_tier = tier; batch_bot = 1; bot_kind = 1; begin(batch_tier); }
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
  if (!batch && bot_kind == 0 && FG.phase == PH_FIGHT && (frame % 240) == 0) ds_log("t=%.1f hp=%.0f meter=%.0f combo=%d foe0=%.0f st=%d", fight_time, FG.P.hp, FG.meter, FG.combo, FG.foe[0].hp, FG.foe[0].st);
}
void game_draw(void) {
  render_scene(scene, cam);
  if (started) fight_draw_hud();
}
#endif
