/* attic.c: the roped-off attic, its own stage (STAGE_ATTIC) and the hub built on it. See attic.h.
 *
 * A purpose-built room, not the real crawlspace mesh: flat floor at y=0, ~14x12 m, 3.6 m ceiling, its own
 * lighting (world.c routes STAGE_ATTIC collision/raycast/mood here). You arrive with the room glitched and
 * wrong; cross the soft floor (red X = a board that gives) through the junk to the server rack, reboot it,
 * and the room snaps back to normal. After that it is a hub: walk it freely, replay the run, examine old
 * things (each plays a memory), and use the rack's pizza-box terminal (terminal.c). */
#if !defined(KH_TEST_WORLD) && !defined(KH_TEST_FIGHT)
#include "attic.h"
#include "attic_env.h"
#include "terminal.h"
#include "story.h"
#include "player.h"
#include "world.h"
#include "actor.h"
#include "fx.h"
#include "audio.h"
#include "hud_int.h"
#include "ds.h"
#include <string.h>
#include <math.h>

static t3_scene *SC; static t3_camera *CAM;

/* ---------------- the mode ---------------- */
static enum { A_PLAY, A_FELL, A_WON, A_HUB, A_MEMORY, A_TERMINAL } phase;
static float phase_t, run_t, creak_t;
static int tries;
static bool was_won, near_use;
static int use_kind, use_id;                       /* the examinable in reach */
static v3 ret_pos; static float ret_yaw;
static unsigned games_mask;                        /* hacking minigames unlocked (persisted) */
static uint16_t found_mask;                        /* examinables whose memory has played (persisted) */
static int mem_id;                                 /* the memory now playing */
static actor_t *ghost;                             /* the figure from the past in a memory */
static v3 mem_eye, mem_look;                        /* the memory's camera */

/* examinables in the hub: old things that hold a memory, the replay spot, the terminal */
enum { EX_MEMORY, EX_REPLAY, EX_TERMINAL };
typedef struct { v3 pos; float r; int kind; int id; const char *label; } exam_t;
static const exam_t EXAM[] = {
  { { -3.3f, 0, -3.9f }, 1.6f, EX_TERMINAL, 0, "USE TERMINAL" },   /* in front of the rack */
  { {  5.2f, 0, -3.3f }, 1.4f, EX_MEMORY, 0, "EXAMINE" },   /* the dead CRT */
  { { -6.0f, 0,  3.0f }, 1.4f, EX_MEMORY, 1, "EXAMINE" },   /* a tote on the west shelf */
  { {  4.2f, 0,  4.4f }, 1.6f, EX_REPLAY, 0, "RUN THE DRILL" },
};
#define NEXAM ((int)(sizeof EXAM / sizeof EXAM[0]))

/* a memory sequence: examining the object dims the room to a warm past, a figure acts the moment out
 * while the camera eases in, and the narration plays line by line. */
typedef struct {
  const char *title; const char *line[3]; int nline;
  actor_model model; v3 at; float yaw; const char *clip;   /* the figure from the past */
  v3 eye0, look0, eye1, look1; float dur;                  /* the camera's slow push */
} mem_t;
static const mem_t MEM[] = {
  { "THE OLD RENDER NODE",
    { "This box ran the first wiki, back when the whole space was one room.",
      "Somebody soldered the power switch on. It never got turned off.",
      "Put a hand on the case. It is still warm." }, 3,
    AM_O, { 5.0f, 0, -3.0f }, -2.2f, "Fixing_Kneeling",
    { 3.2f, 1.55f, -0.8f }, { 5.0f, 0.65f, -3.0f }, { 4.1f, 1.1f, -1.8f }, { 5.0f, 0.6f, -3.0f }, 9.0f },
  { "A BOX OF BADGES",
    { "Laminated member badges. Names you still see on the sign-in sheet.",
      "Most of them learned to solder at the bench downstairs.",
      "" }, 2,
    AM_L, { -5.6f, 0, 2.9f }, 1.1f, "Idle_Loop",
    { -3.3f, 1.6f, 3.7f }, { -5.6f, 1.05f, 2.9f }, { -4.4f, 1.3f, 3.3f }, { -5.6f, 1.0f, 2.9f }, 8.5f },
};
#define NMEM ((int)(sizeof MEM / sizeof MEM[0]))

/* ---------------- the entrance: a roped-off stair with a little PVC sign ---------------- */
static void build_sign(void) {
  const uint32_t PVC = 0xe9e9e4, POST = 0x8a8f96, ROPE = 0x6b1f1f, SIGN = 0xf2efe6, RED = 0xc62828;
  float z = -32.0f, x0 = -0.35f, x1 = 1.15f;
  fxp_begin(); fxp_layer(FXP_BODY);
  /* two stanchion posts with a sagging rope across the stair mouth */
  fxp_cyl(v3_(x0, 0, z), 1.02f, 0.03f, 0.03f, 8, POST); fxp_cyl(v3_(x0, 1.0f, z), 0.06f, 0.05f, 0.05f, 8, POST);
  fxp_cyl(v3_(x1, 0, z), 1.02f, 0.03f, 0.03f, 8, POST); fxp_cyl(v3_(x1, 1.0f, z), 0.06f, 0.05f, 0.05f, 8, POST);
  fxp_bar(v3_(x0, 0.92f, z), v3_((x0+x1)/2, 0.78f, z), 0.02f, ROPE);
  fxp_bar(v3_((x0+x1)/2, 0.78f, z), v3_(x1, 0.92f, z), 0.02f, ROPE);
  /* a small PVC-framed sign hung on the rope */
  float sx = (x0+x1)/2, sy = 0.5f;
  fxp_bar(v3_(sx-0.03f, 0.78f, z), v3_(sx-0.03f, sy, z), 0.012f, PVC);   /* the hanger */
  fxp_box(v3_(sx, sy-0.14f, z), v3_(0.5f, 0.32f, 0.02f), SIGN);          /* the panel */
  fxp_box(v3_(sx, sy-0.02f, z+0.015f), v3_(0.52f, 0.06f, 0.012f), RED);  /* a red bar top */
  fxp_box(v3_(sx, sy-0.26f, z+0.015f), v3_(0.52f, 0.06f, 0.012f), RED);  /* a red bar bottom */
  /* the PVC frame around the panel: four thin white pipes */
  fxp_cyl(v3_(sx-0.27f, sy-0.30f, z+0.01f), 0.32f, 0.018f, 0.018f, 6, PVC);
  fxp_cyl(v3_(sx+0.27f, sy-0.30f, z+0.01f), 0.32f, 0.018f, 0.018f, 6, PVC);
  fxp_bar(v3_(sx-0.28f, sy-0.30f, z+0.01f), v3_(sx+0.28f, sy-0.30f, z+0.01f), 0.018f, PVC);
  fxp_bar(v3_(sx-0.28f, sy+0.02f, z+0.01f), v3_(sx+0.28f, sy+0.02f, z+0.01f), 0.018f, PVC);
  t3_object *s = fxp_end(); world_add_prop(s);
}

void attic_init(t3_scene *scene, t3_camera *cam) {
  if (SC) return;
  SC = scene; CAM = cam;
  build_sign();
  attic_env_init(SC);
  term_init();
}

/* ---------------- enter / leave ---------------- */
static void dodge_arm(void) { phase = A_PLAY; phase_t = 0; run_t = 0; creak_t = 0; tries = 0; }

void attic_enter(void) {
  ret_pos = player_pos(); ret_yaw = player_yaw();
  was_won = false; mem_id = -1;
  { uint16_t f, g; story_attic_masks(&f, &g); found_mask = f; games_mask = g ? g : 1; }
  bool beaten = story_attic_beaten();
  attic_env_show(true);
  world_set_stage(STAGE_ATTIC);
  music_mood(beaten ? MOOD_NIGHT : MOOD_TENSION);
  if (beaten) { attic_env_reset(1); phase = A_HUB; phase_t = 0; player_teleport(ATTIC_ENTRY, DS_PI); }
  else { attic_env_reset(0); dodge_arm(); player_teleport(ATTIC_ENTRY, DS_PI); }
  sfx(SFX_STEP, 0, .4f);
}

static void leave(void) {
  if (ghost) { actor_visible(ghost, false); actor_free(ghost); ghost = NULL; }
  story_attic_set_masks(found_mask, games_mask);
  attic_env_show(false);
  world_set_stage(STAGE_TOWN);          /* apply_lights repaints the town from the (unchanged) story clock */
  player_teleport(ret_pos, ret_yaw);
  player_camera_snap();
  music_mood(MOOD_HOSTING);             /* back in the lab; the next scene resets it if need be */
}

/* walk-only intent: no jumping (kept so nothing clips), no weapons, no bike */
static intent_t walk_only(const intent_t *in) {
  intent_t w = *in;
  w.jump = w.jump_held = w.light = w.heavy = w.special = w.dodge = false;
  w.light_held = w.heavy_held = w.guard = w.guard_pressed = w.draw = w.bike = w.assist = w.lock = false;
  w.weapon_step = 0; w.weapon_pick = -1; w.wheel = 0;
  return w;
}

/* ---------------- update ---------------- */
bool attic_update(float dt, const intent_t *in) {
  phase_t += dt;

  if (phase == A_TERMINAL) {
    if (!term_update(dt, in)) { games_mask = term_unlocked(); phase = A_HUB; phase_t = 0; player_camera_snap(); }
    world_update(dt, player_pos());
    return true;
  }
  if (phase == A_MEMORY) {
    const mem_t *M = &MEM[mem_id];
    world_update(dt, M->at);
    if (ghost) actor_update(ghost, dt);
    float t = ds_clamp((phase_t - 0.6f) / (M->dur - 1.8f), 0, 1), e = t*t*(3-2*t);   /* the slow push */
    mem_eye = v3_lerp(M->eye0, M->eye1, e); mem_look = v3_lerp(M->look0, M->look1, e);
    t3_object_set_position(CAM, mem_eye.x, mem_eye.y, mem_eye.z);
    t3_object_look_at(CAM, mem_look.x, mem_look.y, mem_look.z);
    if (phase_t > M->dur || (phase_t > 1.4f && (in->interact || in->ok || in->back))) {
      if (ghost) { actor_visible(ghost, false); actor_free(ghost); ghost = NULL; }
      phase = A_HUB; phase_t = 0; player_camera_snap();
    }
    return true;
  }

  intent_t w = walk_only(in);
  player_update(dt, &w);
  world_update(dt, player_pos());
  v3 p = player_pos();

  if (phase == A_PLAY) {
    run_t += dt;
    if (attic_env_soft(p)) {
      if (creak_t == 0) sfx(SFX_STEP, 0, .5f);
      creak_t += dt;
      fx_glow(v3_(p.x, 0.03f, p.z), 0.5f + 0.2f*sinf(run_t*40), 0xe23b2e);
      if (creak_t > 0.10f) {
        phase = A_FELL; phase_t = 0; tries++;
        actor_play(player_actor(), "Hit_Knockback", 1, false, .05f);
        fx_dust(v3_(p.x, 0, p.z), 1.2f, 22); fx_smoke(v3_(p.x, -0.1f, p.z), 0.8f, 0x6b5436, 10, 0.4f);
        sfx(SFX_BODY_FALL, 0, .9f); sfx(SFX_SLAM, 0, .5f);
      }
    } else creak_t = 0;
    /* reach the rack: reboot, and the room comes back */
    if (hypotf(p.x-ATTIC_RACK.x, p.z-ATTIC_RACK.z) < 1.4f) {
      phase = A_WON; phase_t = 0; was_won = true; attic_env_target(1);
      fx_burst(v3_(ATTIC_RACK.x, 0.9f, ATTIC_RACK.z+0.3f), 0x58d06a, 1.1f);
      fx_motes(v3_(ATTIC_RACK.x, 1.0f, ATTIC_RACK.z+0.3f), 0x8fe6a0, 26, 0.6f);
      sfx(SFX_PICKUP, 0, 1); music_mood(MOOD_CLEAR);
    }
    if (in->back) { leave(); return false; }
  } else if (phase == A_FELL) {
    if (in->back) { leave(); return false; }
    if (phase_t > 1.2f && (in->interact || in->ok || phase_t > 3.0f)) { player_teleport(ATTIC_ENTRY, DS_PI); phase = A_PLAY; phase_t = 0; creak_t = 0; }
  } else if (phase == A_WON) {
    fx_glow(v3_(ATTIC_RACK.x, 1.1f, ATTIC_RACK.z+0.3f), 0.35f, 0x58d06a);
    if (phase_t > 2.6f) { phase = A_HUB; phase_t = 0; music_mood(MOOD_NIGHT); }
  } else {   /* A_HUB */
    near_use = false; use_kind = -1;
    float best = 1e9f;
    for (int i = 0; i < NEXAM; i++) {
      float d = hypotf(p.x-EXAM[i].pos.x, p.z-EXAM[i].pos.z);
      if (d < EXAM[i].r && d < best) { best = d; near_use = true; use_kind = EXAM[i].kind; use_id = EXAM[i].id; }
    }
    if (near_use && in->interact) {
      if (use_kind == EX_TERMINAL) { phase = A_TERMINAL; phase_t = 0; term_open(games_mask); sfx(SFX_UI_OK,0,1); }
      else if (use_kind == EX_REPLAY) { attic_env_target(1); dodge_arm(); player_teleport(ATTIC_ENTRY, DS_PI); sfx(SFX_UI_OK,0,1); }
      else if (use_kind == EX_MEMORY) {
        mem_id = use_id % NMEM; found_mask |= (uint16_t)(1u << use_id);
        const mem_t *M = &MEM[mem_id];
        ghost = actor_new(M->model, &ACTOR_PAL_NONE, 1, SC);
        actor_place(ghost, M->at, M->yaw); actor_play(ghost, M->clip, 1, true, 0); actor_visible(ghost, true);
        mem_eye = M->eye0; mem_look = M->look0;
        phase = A_MEMORY; phase_t = 0; sfx(SFX_UI, 0, 1);
      }
    }
    if (in->back) { leave(); return false; }
  }
  return true;
}

bool attic_won(void) { return was_won; }

/* ---------------- HUD ---------------- */
#define AT_INK 0x0c0f14u
#define AT_PAPER 0xf4eee2u
#define AT_RED 0xe23b2eu
#define AT_GREEN 0x58d06au
#define AT_YEL 0xf2c21eu
static layer_t L_top, L_obj, L_banner, L_mem;

void attic_draw(void) {
  if (phase == A_TERMINAL) { term_draw(); return; }
  bool pad = input_get()->used_pad;   /* device-aware button glyphs, like the rest of the HUD */
  hg_begin();
  /* the tag and objective (not in the terminal or a memory) */
  if (phase != A_MEMORY) {
    cv *c = layer_begin(&L_top, 360, 92, 0xA77);
    if (c) { p_hazard(c, 0, 0, 360, 10, 26, AT_YEL, AT_INK);
      p_text(c, F_DISPLAY, 34, 1, CV_LEFT, "THE ATTIC", 14, 52, AT_PAPER, 1);
      p_text(c, F_MONO, 15, 1, CV_LEFT, phase==A_HUB||phase==A_WON ? "AUTHORIZED, APPARENTLY" : "WATCH YOUR STEP", 15, 78, AT_YEL, 1);
      layer_end(&L_top, c); }
    hg_tex(&L_top, 44, 36, 1);

    uint32_t key = (uint32_t)phase*16 + (near_use ? (uint32_t)use_kind+1 : 0)*4 + (uint32_t)(tries<<8);
    cv *cc = layer_begin(&L_obj, 600, 40, 0xB00 ^ key ^ (pad?0x40000:0));
    if (cc) { p_plate(cc, 0, 0, 600, 40, 10, AT_INK, .82f);
      const char *s = "Reach the server rack. Keep off the red X.";
      if (phase == A_HUB) s = near_use
        ? (use_kind==EX_TERMINAL ? "USE THE TERMINAL  {USE}" : use_kind==EX_REPLAY ? "RUN THE DRILL  {USE}" : "EXAMINE  {USE}")
        : "The attic is yours. Look around.";
      g_markup(cc, 16, 26, CV_LEFT, F_TYPE, 20, .1f, AT_PAPER, 20, pad, s, true);
      if (tries > 0 && phase == A_PLAY) { char t[24]; snprintf(t,sizeof t,"FELL: %d", tries); p_text(cc, F_MONO, 15, 1, CV_LEFT, t, 500, 25, AT_RED, 1); }
      layer_end(&L_obj, cc); }
    hg_tex(&L_obj, 44, 648, 1);
  }
  /* win / fail banners */
  if (phase == A_FELL || phase == A_WON) {
    bool won = phase == A_WON; float a = hud_ease_out(phase_t*3);
    hg_rect(0, 280, 1280, 160, won ? 0x0b2414 : 0x2a0e0a, .68f*a);
    cv *c = layer_begin(&L_banner, 920, 150, (won ? 0xD01u : 0xD02u) ^ (pad?0x40000u:0u));
    if (c) { p_text(c, F_DISPLAY, 58, 2, CV_CENTER, won ? "BACK ONLINE" : "THROUGH THE FLOOR", 460, 64, won ? AT_GREEN : AT_RED, 1);
      p_text(c, F_TYPE, 22, 0, CV_CENTER, won ? "Server up. The room settles. Members site, wiki, door controller." : "A boot through the ceiling.", 460, 102, AT_PAPER, 1);
      if (!won) g_markup(c, 460, 132, CV_CENTER, F_MONO, 15, 1, AT_YEL, 16, pad, "{USE} try again    {BACK} climb down", true);
      layer_end(&L_banner, c); }
    hg_tex_m(&L_banner, (float[6]){1,0,0,1,640-460,300}, a, 0xffffff);
  }
  /* a memory sequence: the room shows through a warm sepia wash and a cinematic frame while the figure
   * acts it out; the title fades in and out up top, the narration plays one line at a time */
  if (phase == A_MEMORY && mem_id >= 0) {
    const mem_t *M = &MEM[mem_id];
    float fin = hud_ease_out(phase_t*1.5f), fout = ds_clamp(M->dur - phase_t, 0, 1), a = fin * fout;
    hg_rect(0, 0, 1280, 720, 0x3a2614, 0.26f*a);
    hg_vignette(0x1a0f04, 0.5f*a, 0.4f);
    float lb = 72*a; hg_rect(0, 0, 1280, lb, 0x000000, a); hg_rect(0, 720-lb, 1280, lb, 0x000000, a);
    float ta = hud_ease_out(phase_t*1.4f) * ds_clamp((3.6f - phase_t)*0.9f, 0, 1);
    if (ta > 0.01f) {
      cv *c = layer_begin(&L_mem, 760, 92, 0xE10u + (uint32_t)mem_id);
      if (c) { p_text(c, F_MONO, 14, 4, CV_CENTER, "A MEMORY", 380, 26, AT_YEL, 1);
        p_text(c, F_DISPLAY, 36, 1, CV_CENTER, M->title, 380, 72, 0xf0e2c4, 1); layer_end(&L_mem, c); }
      hg_tex_m(&L_mem, (float[6]){1,0,0,1,640-380,92}, ta, 0xffffff);
    }
    int beat = (int)((phase_t - 1.2f) / ((M->dur - 1.6f) / (float)M->nline));
    if (beat >= 0 && beat < M->nline && M->line[beat][0]) {
      cv *c = layer_begin(&L_banner, 1040, 44, 0xF00u + (uint32_t)beat + (uint32_t)mem_id*8u);
      if (c) { p_text(c, F_TYPE, 23, 0, CV_CENTER, M->line[beat], 520, 30, 0xf4ecda, 1); layer_end(&L_banner, c); }
      hg_tex(&L_banner, 120, 632, a);
    }
    cv *sc = layer_begin(&L_top, 240, 26, 0xE99u ^ (pad?0x40000u:0u));
    if (sc) { g_markup(sc, 236, 18, CV_RIGHT, F_MONO, 13, 1, 0x9a8f7a, 14, pad, "{USE} let it go", true); layer_end(&L_top, sc); }
    hg_tex(&L_top, 1010, 690, a*0.9f);
  }
  hg_end();
}
#endif
