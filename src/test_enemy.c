/* test_enemy.c: the attic's enemies on their own (KH_TEST_ENEMY), for headless screenshots. In the
 * machine shop arena: the six kinds in a line (front, then from behind), a close up of each head, each
 * one swinging a combo; the line again in the attic room (glitched, normal, glitched close); then three
 * real fights back in the arena through fight_start and fight_update with a small bot
 * (three mannequins; two Unclaimed and a Junkbot; SYSOP as a story actor the test dresses itself, kept
 * dressed through fight_end, then freed while dressed). Build with EXTRA_CFLAGS=-DKH_TEST_ENEMY and
 * game.c left out of KH_SRC. Showroom views hold 60 frames (shoot 50, 110, ...); the schedule and a
 * stop motion count are logged as [enemy] lines. */
#ifdef KH_TEST_ENEMY
#include "game.h"
#include "world.h"
#include "player.h"
#include "actor.h"
#include "fx.h"
#include "fight_int.h"
#include "weapon.h"
#include "enemy.h"
#include "hud_int.h"
#include "attic_env.h"
#include "input.h"
#include <stdio.h>
#include <string.h>

#define STAGE STAGE_ARENA_SHOP
#define VIEW 60
#define NK (EK_COUNT - 1)              /* the kinds after EK_NONE */
#define ATTIC_V (2 + NK * 2)          /* the first attic view */
#define SHOW_END ((ATTIC_V + 3) * VIEW)
#define FIGHT_LEN 960
static t3_scene *scene; static t3_camera *cam;
static actor_t *PL, *SHOW[NK];
static actor_t *STORY;   /* SYSOP as the story has it: a cast body in its own colours, dressed by hand */
static v3 C; static float R;
static int fr = -1, fight_i = -1;

static void aim(v3 eye, v3 at, float fov) {
  t3_object_set_position(cam, eye.x, eye.y, eye.z);
  t3_object_look_at(cam, at.x, at.y, at.z);
  if (cam->fov != fov) { cam->fov = fov; t3_camera_update_projection_matrix(cam); }
  t3_object_update_world_matrix(cam, true, true);
}
static v3 line_pos(int k) { return v3_(C.x + (k - (NK - 1) * .5f) * 1.3f, C.y, C.z); }
static v3 attic_pos(int k) { v3 p = v3_((k - (NK - 1) * .5f) * 1.2f, 1.5f, -1.4f); float fy = world_floor_y(p); p.y = fy > -1e8f ? fy : 0; return p; }

void game_init(void) {
  scene = t3_scene_new();
  cam = t3_perspective_camera_new(40, 16.f / 9.f, .05f, 600);
  t3_object_add(&scene->base, cam);
  hg_init();
  world_init(scene);
  player_init(cam);
  fight_init(scene, cam);
  attic_env_init(scene);
  world_set_stage(STAGE);
  world_set_time(15);
  world_stage_floor(STAGE, &C, &R);
  ds_log("[enemy] stage floor %.2f %.2f %.2f r %.1f", C.x, C.y, C.z, R);
  PL = actor_new(AM_C, &ACTOR_PAL_NONE, 1, scene);
  player_set_actor(PL);
  actor_place(PL, v3_(C.x, C.y, C.z - 6), 0);
  actor_visible(PL, false);   /* the showroom has no player */
  for (int k = 0; k < NK; k++) {
    fighter_def d = enemy_def(k + 1);
    SHOW[k] = actor_new(d.model, &d.pal, 1, scene);
    enemy_dress(SHOW[k], k + 1);
    actor_place(SHOW[k], line_pos(k), 0);
    actor_play(SHOW[k], "Idle_Loop", 1, true, 0);
    actor_update(SHOW[k], 0);
    v3 h = actor_head_pos(SHOW[k]), c = actor_bone_pos(SHOW[k], "chest"), u = actor_bone_pos(SHOW[k], "upperChest");
    ds_log("[enemy] kind %d %s model %d height %.2f head pivot %.2f chest %.2f upperChest %.2f", k + 1, d.name, (int)d.model, actor_height(SHOW[k]), h.y - C.y, c.y - C.y, u.y - C.y);
  }
  ds_log("[enemy] lineup front 0..%d, back ..%d, heads ..%d, combos ..%d, attic ..%d, fights from %d, %d frames each",
         VIEW, 2 * VIEW, (2 + NK) * VIEW, ATTIC_V * VIEW, SHOW_END, SHOW_END, FIGHT_LEN);
}

/* ---- a small bot: walk at the locked foe, swing, now and then a heavy, roll out of a telegraphed blow ---- */
static intent_t BI;
static float bot_cd, roll_cd;
static void bot(float dt) {
  memset(&BI, 0, sizeof BI); BI.weapon_pick = -1;
  if (FG.phase != PH_FIGHT) return;
  fighter_t *P = &FG.P;
  int t = -1; float best = 1e9f;
  for (int i = 0; i < FG.nfoe; i++) {
    fighter_t *o = &FG.foe[i];
    if (o->st == F_KO || o->st == F_KNEEL) continue;
    float d = v3_len(v3_sub(o->pos, P->pos));
    if (d < best) { best = d; t = i; }
  }
  if (t < 0) return;
  fighter_t *T = &FG.foe[t];
  v3 f = v3_norm(v3_(FG.cam_look.x - FG.cam_pos.x, 0, FG.cam_look.z - FG.cam_pos.z)), r = v3_(f.z, 0, -f.x);
  v3 to = v3_sub(T->pos, P->pos); to.y = 0;
  float dist = v3_len(to);
  v3 dir = dist > .01f ? v3_scale(to, 1 / dist) : f, go = dist > 1.6f ? dir : v3_(0, 0, 0);
  BI.my = v3_dot(go, f); BI.mx = -v3_dot(go, r);
  roll_cd -= dt; bot_cd -= dt;
  for (int i = 0; i < FG.nfoe; i++) {
    fighter_t *o = &FG.foe[i];
    if (o->st == F_ATTACK && !o->released && o->tele > 0 && o->tele < .12f && roll_cd <= 0 && ds_rand() < .5f && v3_len(v3_sub(o->pos, P->pos)) < 3.5f) {
      v3 s = v3_(dir.z, 0, -dir.x);
      BI.dodge = true; BI.my = v3_dot(s, f); BI.mx = -v3_dot(s, r); roll_cd = 1.2f;
      return;
    }
  }
  if (dist < 2.3f && bot_cd <= 0) { if (ds_rand() < .2f) BI.heavy = true; else BI.light = true; bot_cd = ds_rr(.18f, .3f); }
}

static void fight_go(int i) {
  static const int KINDS[3][3] = { { EK_MANNEQUIN, EK_MANNEQUIN_SANTA, EK_MANNEQUIN_LIGHTS }, { EK_UNCLAIMED, EK_UNCLAIMED, EK_JUNKBOT }, { EK_SYSOP, 0, 0 } };
  static const char *SUB[3] = { "versus the holiday bins", "versus the member storage", "versus SYSOP" };
  static const float PX[3] = { 0, -2.6f, 2.6f }, PZ[3] = { 4.5f, 3.6f, 3.6f };
  fight_def D;
  memset(&D, 0, sizeof D);
  D.title = "THE ATTIC"; D.sub = SUB[i];
  D.center = C; D.radius = R > 0 ? fminf(R, 9) : 8;
  D.player_hp = 1; D.foe_scale = 1; D.weapons = 1u << WP_ARC; D.start_weapon = WP_ARC;
  for (int k = 0; k < 3 && KINDS[i][k]; k++) {
    D.foes[k] = enemy_def(KINDS[i][k]);
    D.foes[k].place_x = PX[k]; D.foes[k].place_z = PZ[k];
    D.count++;
  }
  if (i == 2) {
    D.twist = FT_BOSS; D.phase2_at = .5f;
    if (!STORY) STORY = actor_new(AM_A, &ACTOR_PAL_NONE, 1, scene);
    enemy_dress(STORY, EK_SYSOP);
    actor_place(STORY, v3_(C.x, C.y, C.z + 4.5f), DS_PI);
    actor_play(STORY, "Idle_Loop", 1, true, 0);
    D.foes[0].actor = STORY;
  }
  for (int k = 0; k < NK; k++) actor_visible(SHOW[k], false);
  if (world_get_stage() != STAGE) world_set_stage(STAGE);
  actor_visible(PL, true);
  actor_place(PL, v3_(C.x, C.y, C.z - 2.5f), 0);
  player_teleport(v3_(C.x, C.y, C.z - 2.5f), 0);
  fight_start(&D, PL);
  bot_cd = roll_cd = 0;
  ds_log("[enemy] fight %d starts at frame %d: %d foes", i, fr, D.count);
}

/* stop motion check: how often a body's right hand moves in a view */
static v3 last_hand[NK]; static int hand_moves[NK];

void game_frame(float dt) {
  fr++;
  if (fr < SHOW_END) {
    int v = fr / VIEW, t = fr % VIEW;
    if (t == 0 && v == ATTIC_V) { world_set_stage(STAGE_ATTIC); ds_log("[enemy] attic views from frame %d", fr); }
    if (t == 0 && v >= ATTIC_V) attic_env_reset(v == ATTIC_V + 1 ? 1 : 0);
    if (t == 0) {
      for (int k = 0; k < NK; k++) {
        bool attic = v >= ATTIC_V, on = v < 2 || attic || (v - 2) % NK == k;
        actor_visible(SHOW[k], on);
        actor_place(SHOW[k], attic ? attic_pos(k) : v < 2 ? line_pos(k) : C, 0);
        bool combo = v >= 2 + NK && !attic;
        actor_replay(SHOW[k], combo ? "Sword_Regular_Combo" : "Idle_Loop", 1, true, 0);
        weapon_equip(SHOW[k], combo ? enemy_def(k + 1).weapon : -1);
        if (hand_moves[k] && v > 2 + NK) ds_log("[enemy] view %d kind %d right hand moved on %d of %d frames", v - 1, k + 1, hand_moves[k], VIEW);
        hand_moves[k] = 0;
      }
      if (v == 2 + NK) ds_log("[enemy] combos from frame %d", fr);
    }
    for (int k = 0; k < NK; k++) if (actor_is_visible(SHOW[k])) {
      actor_update(SHOW[k], dt);
      v3 h = actor_bone_pos(SHOW[k], "rightHand");
      if (v3_len(v3_sub(h, last_hand[k])) > 1e-5f) hand_moves[k]++;
      last_hand[k] = h;
    }
    weapon_update(dt);
    fx_update(dt, cam);
    if (v == 0) aim(v3_(C.x, C.y + 1.25f, C.z + 7.2f), v3_(C.x, C.y + .95f, C.z), 46);
    else if (v == 1) aim(v3_(C.x + 1.5f, C.y + 1.6f, C.z - 6.6f), v3_(C.x, C.y + 1.f, C.z), 46);
    else if (v < 2 + NK) {   /* the head, a three quarter close up, turning a little through the view */
      actor_t *a = SHOW[(v - 2) % NK];
      v3 h = actor_head_pos(a);
      float ang = .45f - t * .012f;
      aim(v3_(h.x + sinf(ang) * 1.05f, h.y + .12f, h.z + cosf(ang) * 1.05f), v3_(h.x, h.y + .1f, h.z), 36);
    } else if (v < ATTIC_V) aim(v3_(C.x + 1.9f, C.y + 1.35f, C.z + 3.1f), v3_(C.x, C.y + 1.f, C.z), 42);
    else if (v < ATTIC_V + 2) aim(v3_(.4f, 1.35f, 3.3f), v3_(0, .95f, -1.4f), 52);
    else aim(v3_(-1.2f, 1.45f, .3f), v3_(-2.4f, 1.15f, -1.4f), 40);   /* the three mannequins, close */
    world_update(dt, v >= ATTIC_V ? v3_(0, 1, 0) : C);
    return;
  }
  int i = (fr - SHOW_END) / FIGHT_LEN, t = (fr - SHOW_END) % FIGHT_LEN;
  if (t == 0) {
    if (fight_i >= 0) fight_end();
    if (i < 3) { fight_i = i; fight_go(i); } else fight_i = -1;
    if (i == 3 && STORY) ds_log("[enemy] after fight_end the story SYSOP is dressed as %d, visible %d", enemy_kind_of(STORY), actor_is_visible(STORY));
  }
  if (fight_i < 0) {   /* the story's SYSOP after its fight, still dressed; then freed while dressed */
    if (!STORY) return;
    actor_update(STORY, dt);
    v3 p = actor_pos(STORY);
    aim(v3_(p.x + 1.6f, p.y + 1.5f, p.z - 2.6f), v3_(p.x, p.y + .8f, p.z), 42);
    world_update(dt, p);
    if (t == 120) {
      actor_t *was = STORY;
      weapon_equip(STORY, -1); actor_free(STORY); STORY = NULL;
      ds_log("[enemy] freed the dressed story SYSOP: still listed %d (0 expected)", enemy_kind_of(was));
    }
    return;
  }
  bot(dt);
  fight_state s = fight_update(dt, &BI);
  if (t % 60 == 0) {
    char st[64] = {0};
    for (int k = 0; k < FG.nfoe; k++) { char b[16]; snprintf(b, sizeof b, " %d/%.0f", FG.foe[k].st, FG.foe[k].hp); strncat(st, b, sizeof st - strlen(st) - 1); }
    ds_log("[enemy] fight %d f=%d phase=%d state=%d player hp %.0f foes%s", fight_i, fr, FG.phase, s, FG.P.hp, st);
  }
  world_update(dt, FG.P.pos);
}

void game_draw(void) {
  render_scene(scene, cam);
  if (fight_i >= 0) fight_draw_hud();
  render_target();
}
#endif
