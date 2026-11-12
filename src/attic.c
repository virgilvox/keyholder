/* attic.c: the roped-off stairs and the attic minigame behind them. See attic.h.
 *
 * The real attic over the back of the lab (the HSL-108 "Rear attic floor", world z -40..-31 at y ~3.25)
 * is the stage: we warp the player up onto it, fence the field with four invisible solids so there is no
 * walking off and no clipping to the roof, and lay a grid of soft boards (red X tape) and a small server
 * rack in the back-left corner on top. Cross to the rack on the sound boards and reboot it; a boot on a
 * red X puts you through the ceiling and back down the stairs. Movement, animation and the follow camera
 * are player.c's, so it plays like the rest of the lab. */
#if !defined(KH_TEST_WORLD) && !defined(KH_TEST_FIGHT)
#include "attic.h"
#include "player.h"
#include "world.h"
#include "actor.h"
#include "fx.h"
#include "audio.h"
#include "hud_int.h"
#include "ds.h"
#include <string.h>
#include <math.h>

/* ---- the attic field, in world metres ---- */
#define AF_Y    3.25f            /* the attic floor you stand on (measured in play) */
#define COLS    8
#define ROWS    5
#define CX0    (-2.0f)           /* cell column 0 centre (west) */
#define CXD     1.0857f          /* column step (east is +) */
#define CZ0    (-35.6f)          /* cell row 0 centre (south, by the stair head) */
#define CZD    (-0.9f)           /* row step (north is more negative) */
#define SOFT_R  0.42f            /* how near a soft cell's centre a boot has to fall through */
/* the fence: south by the stair head, then north, west and east walls */
#define FENCE_S (-35.0f)
#define FENCE_N (-39.7f)
#define FENCE_W (-2.3f)
#define FENCE_E  5.9f
/* the entry (the stair head) and the rack (back-left, by the north-west corner) */
static const v3 ENTRY = {0.5f, AF_Y + 0.25f, -35.3f};
static const v3 RACK  = {-2.0f, AF_Y, -39.4f};

/* 1 is a soft board (a red X). A sound path snakes from the entry: c3,c2,c1 (row0) up the west side to the
 * rack. Players find their own way; this one is always open. */
static const uint8_t SOFT[ROWS][COLS] = {
  {1,0,0,0,1,1,0,1},
  {1,0,1,0,1,1,0,1},
  {0,0,1,1,0,1,1,0},
  {0,1,1,0,1,0,1,1},
  {0,1,1,0,1,0,1,1},
};

static inline float col_x(int c) { return CX0 + c * CXD; }
static inline float row_z(int r) { return CZ0 + r * CZD; }

/* ---- state ---- */
static t3_scene *SC; static t3_camera *CAM;
static t3_object *sign_obj, *haz_obj, *rack_obj;     /* sign: always up in the lab; haz/rack: shown in the game */
static enum { A_PLAY, A_FELL, A_WON } phase;
static float phase_t, run_t, creak_t, reboot;        /* reboot: 0..1 while the rack comes back up */
static int tries;                                    /* boards fallen through this visit */
static bool was_won, near_rack;
static float save_fov;
static v3 cam_eye, cam_look;                         /* the attic's own camera, eased to the player */
static v3 ret_pos; static float ret_yaw;             /* where you stood at the foot of the stairs */

/* the attic is a 1.3 m crawlspace, too low for the walking follow camera (it collapses onto the player),
 * so we drive our own: a high three-quarter view just under the rafters, looking down the field, eased
 * after the player. It ignores the walls (a contained space), so it never pulls in. */
static void aim_camera(float dt, bool snap) {
  v3 p = player_pos();
  v3 eye = v3_(ds_clamp(p.x, -1.6f, 5.2f), 4.42f, ds_clamp(p.z + 2.9f, -39.0f, -33.6f));
  v3 look = v3_(p.x, 3.35f, p.z - 1.8f);
  float k = snap ? 1 : 1 - expf(-7.f * dt);
  cam_eye = snap ? eye : v3_lerp(cam_eye, eye, k);
  cam_look = snap ? look : v3_lerp(cam_look, look, k);
  t3_object_set_position(CAM, cam_eye.x, cam_eye.y, cam_eye.z);
  t3_object_look_at(CAM, cam_look.x, cam_look.y, cam_look.z);
  if (CAM->fov != 54) { CAM->fov = 54; t3_camera_update_projection_matrix(CAM); }
}

/* ---- building the props ---- */
/* the hanging danger sign across the foot of the stairs. Hazard board on a cord; the words are read out
 * on the HUD and by the member who stops you, since the prop carries no lettering. */
static void build_sign(void) {
  const uint32_t YEL = 0xf2c21e, BLK = 0x16140e, RED = 0xc62828, ROPE = 0x2a2620, WOOD = 0x6b5436;
  float z = -32.0f, y = 1.72f;                 /* across the mouth at head height */
  float x0 = -0.35f, x1 = 1.15f;
  fxp_begin();
  fxp_layer(FXP_BODY);
  /* the cord, dipping a touch in the middle */
  fxp_bar(v3_(x0, y + 0.22f, z), v3_((x0 + x1) * .5f, y + 0.10f, z), 0.015f, ROPE);
  fxp_bar(v3_((x0 + x1) * .5f, y + 0.10f, z), v3_(x1, y + 0.22f, z), 0.015f, ROPE);
  /* two little drop cords to the board */
  fxp_bar(v3_(x0 + 0.12f, y + 0.17f, z), v3_(x0 + 0.12f, y, z), 0.012f, ROPE);
  fxp_bar(v3_(x1 - 0.12f, y + 0.17f, z), v3_(x1 - 0.12f, y, z), 0.012f, ROPE);
  /* the board: a wood backing, then a hazard face, then a red bar */
  v3 bc = v3_((x0 + x1) * .5f, y - 0.19f, z);
  fxp_box(bc, v3_(x1 - x0, 0.40f, 0.035f), WOOD);
  for (int i = 0; i < 7; i++) {
    float sx = x0 + 0.03f + i * 0.205f;
    fxp_box(v3_(sx, y - 0.19f, z + 0.02f), v3_(0.11f, 0.34f, 0.01f), (i & 1) ? BLK : YEL);
  }
  fxp_box(v3_((x0 + x1) * .5f, y - 0.07f, z + 0.03f), v3_(x1 - x0 - 0.05f, 0.085f, 0.01f), RED);
  fxp_box(v3_((x0 + x1) * .5f, y - 0.31f, z + 0.03f), v3_(x1 - x0 - 0.05f, 0.085f, 0.01f), RED);
  sign_obj = fxp_end();
  world_add_prop(sign_obj);                    /* hangs under the town group: gone in the arenas */
}

/* the red X boards, laid flat on the attic floor */
static void build_hazards(void) {
  const uint32_t TAPE = 0xe23b2e, SHADE = 0x241c16;
  fxp_begin();
  fxp_layer(FXP_BODY);
  for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) {
    if (!SOFT[r][c]) continue;
    float x = col_x(c), z = row_z(r), h = AF_Y + 0.012f;
    float s = 0.44f;
    /* a scorched patch under the tape */
    fxp_quad(v3_(x - s, h, z - s), v3_(x + s, h, z - s), v3_(x + s, h, z + s), v3_(x - s, h, z + s), SHADE);
    /* the X */
    fxp_bar(v3_(x - s * .82f, h + 0.004f, z - s * .82f), v3_(x + s * .82f, h + 0.004f, z + s * .82f), 0.05f, TAPE);
    fxp_bar(v3_(x - s * .82f, h + 0.004f, z + s * .82f), v3_(x + s * .82f, h + 0.004f, z - s * .82f), 0.05f, TAPE);
  }
  haz_obj = fxp_end();
  haz_obj->visible = false;
}

/* the little server rack in the back-left corner: the members site, the wiki and the door controller */
static void build_rack(void) {
  const uint32_t STEEL = 0x30343a, DARK = 0x1a1c20, PANEL = 0x24272d;
  fxp_begin();
  fxp_layer(FXP_BODY);
  v3 b = RACK;
  /* the cabinet */
  fxp_box(v3_(b.x, b.y + 0.70f, b.z), v3_(0.62f, 1.40f, 0.60f), STEEL);
  fxp_box(v3_(b.x, b.y + 0.70f, b.z + 0.31f), v3_(0.54f, 1.30f, 0.02f), DARK);   /* the glass front */
  /* a stack of 1U boxes */
  for (int i = 0; i < 5; i++)
    fxp_box(v3_(b.x, b.y + 0.28f + i * 0.21f, b.z + 0.30f), v3_(0.50f, 0.17f, 0.04f), PANEL);
  /* the status lights (unlit here; lit each frame in draw so they can blink) */
  rack_obj = fxp_end();
  rack_obj->visible = false;
}

void attic_init(t3_scene *scene, t3_camera *cam) {
  if (SC) return;
  SC = scene; CAM = cam;
  build_sign();
  build_hazards();
  build_rack();
}

/* ---- the run ---- */
void attic_enter(void) {
  ret_pos = player_pos(); ret_yaw = player_yaw();   /* the foot of the stairs, to drop back to */
  haz_obj->visible = true; rack_obj->visible = true;
  /* fence the field so there is no walking off the edge and no clipping to the roof */
  world_solid(0, true, v3_(1.7f, 3.0f, FENCE_S), 5.0f, 0.15f, 0, 1.8f);
  world_solid(1, true, v3_(1.7f, 3.0f, FENCE_N), 5.0f, 0.15f, 0, 1.8f);
  world_solid(2, true, v3_(FENCE_W, 3.0f, -37.3f), 0.15f, 2.6f, 0, 1.8f);
  world_solid(3, true, v3_(FENCE_E, 3.0f, -37.3f), 0.15f, 2.6f, 0, 1.8f);
  player_teleport(ENTRY, DS_PI);                     /* up the stairs, facing into the attic */
  phase = A_PLAY; phase_t = 0; run_t = 0; creak_t = 0; reboot = 0; tries = 0;
  was_won = false; near_rack = false;
  save_fov = CAM->fov;
  aim_camera(0, true);
  music_mood(MOOD_TENSION);
  sfx(SFX_STEP, 0, .4f);
}

static void close_up(void) {
  haz_obj->visible = false; rack_obj->visible = false;
  for (int i = 0; i < 4; i++) world_solid(i, false, v3_(0, 0, 0), 0, 0, 0, 0);
}

/* drop the player back down to the foot of the stairs and end the visit */
static void bail(bool won) {
  was_won = won;
  close_up();
  CAM->fov = save_fov; t3_camera_update_projection_matrix(CAM);
  player_teleport(ret_pos, ret_yaw);
  player_camera_snap();                              /* hand the follow camera back to the roam */
}

/* the cell a point stands in, or -1 off the grid */
static bool cell_at(v3 p, int *cr, int *cc) {
  int c = (int)floorf((p.x - CX0) / CXD + 0.5f);
  int r = (int)floorf((p.z - CZ0) / CZD + 0.5f);
  if (c < 0 || c >= COLS || r < 0 || r >= ROWS) return false;
  *cr = r; *cc = c; return true;
}

bool attic_update(float dt, const intent_t *in) {
  phase_t += dt;
  world_update(dt, player_pos());

  if (phase == A_PLAY) {
    run_t += dt;
    /* walk only: no jumping (that is what used to clip the roof), no weapons, no bike */
    intent_t w = *in;
    w.jump = w.jump_held = false;
    w.light = w.heavy = w.special = w.dodge = false;
    w.light_held = w.heavy_held = w.guard = w.guard_pressed = false;
    w.draw = w.bike = w.assist = w.lock = false;
    w.weapon_step = 0; w.weapon_pick = -1; w.wheel = 0;
    player_update(dt, &w);

    v3 p = player_pos();
    /* a soft board under you: a short creak, then through you go */
    int r, c; bool on_soft = false;
    if (cell_at(p, &r, &c) && SOFT[r][c]) {
      float d = hypotf(p.x - col_x(c), p.z - row_z(r));
      if (d < SOFT_R) on_soft = true;
    }
    if (on_soft) {
      if (creak_t == 0) sfx(SFX_STEP, 0, .5f);
      creak_t += dt;
      fx_glow(v3_(p.x, AF_Y + 0.02f, p.z), 0.5f + 0.2f * sinf(run_t * 40), 0xe23b2e);
      if (creak_t > 0.10f) {
        /* through the ceiling */
        phase = A_FELL; phase_t = 0; tries++;
        actor_play(player_actor(), "Hit_Knockback", 1, false, .05f);
        fx_dust(v3_(p.x, AF_Y, p.z), 1.2f, 22);
        fx_smoke(v3_(p.x, AF_Y - 0.1f, p.z), 0.8f, 0x6b5436, 10, 0.4f);
        sfx(SFX_BODY_FALL, 0, .9f); sfx(SFX_SLAM, 0, .5f);
      }
    } else creak_t = 0;

    /* the rack: step up to it and reboot */
    near_rack = hypotf(p.x - RACK.x, p.z - RACK.z) < 1.5f;
    if (near_rack && in->interact && reboot == 0) { reboot = 0.001f; sfx(SFX_UI_OK, 0, .8f); }
    if (reboot > 0) {
      if (near_rack) {
        reboot += dt / 1.1f;
        fx_glow(v3_(RACK.x, RACK.y + 1.1f, RACK.z + 0.33f), 0.4f, 0x58d06a);
        if (reboot >= 1) {
          phase = A_WON; phase_t = 0;
          fx_burst(v3_(RACK.x, RACK.y + 0.9f, RACK.z + 0.3f), 0x58d06a, 1.1f);
          fx_motes(v3_(RACK.x, RACK.y + 1.0f, RACK.z + 0.3f), 0x8fe6a0, 24, 0.6f);
          sfx(SFX_PICKUP, 0, 1); music_mood(MOOD_CLEAR);
        }
      } else reboot = 0;   /* stepped away: it stops */
    }
    if (in->back) { bail(false); return false; }
  } else if (phase == A_FELL) {
    /* a beat on the floor, then back up to try again, or back out */
    intent_t w; memset(&w, 0, sizeof w); w.weapon_pick = -1;
    player_update(dt, &w);
    if (in->back) { bail(false); return false; }
    if (phase_t > 1.2f && (in->interact || in->ok || phase_t > 3.0f)) {
      player_teleport(ENTRY, DS_PI);
      phase = A_PLAY; phase_t = 0; creak_t = 0;
    }
  } else { /* A_WON */
    intent_t w; memset(&w, 0, sizeof w); w.weapon_pick = -1;
    player_update(dt, &w);
    fx_glow(v3_(RACK.x, RACK.y + 1.1f, RACK.z + 0.33f), 0.35f, 0x58d06a);
    if (phase_t > 2.2f && (in->interact || in->ok || phase_t > 4.5f)) { bail(true); return false; }
  }
  aim_camera(dt, false);
  return true;
}

bool attic_won(void) { return was_won; }

/* ---- the HUD ---- */
#define AT_INK   0x12121au
#define AT_PAPER 0xf4eee2u
#define AT_RED   0xe23b2eu
#define AT_GREEN 0x58d06au
#define AT_YEL   0xf2c21eu

static layer_t L_top, L_obj, L_bar, L_banner;

void attic_draw(void) {
  hg_begin();
  /* the title tag, top-left */
  {
    cv *c = layer_begin(&L_top, 360, 92, 0xA77);
    if (c) {
      p_hazard(c, 0, 0, 360, 10, 26, AT_YEL, AT_INK);
      p_text(c, F_DISPLAY, 34, 1, CV_LEFT, "THE ATTIC", 14, 52, AT_PAPER, 1);
      p_text(c, F_MONO, 15, 1, CV_LEFT, "WATCH YOUR STEP", 15, 78, AT_YEL, 1);
      layer_end(&L_top, c);
    }
    hg_tex(&L_top, 44, 36, 1);
  }
  /* the objective, lower-left */
  {
    uint32_t key = (near_rack ? 2u : 1u) + (uint32_t)(tries << 2);
    cv *c = layer_begin(&L_obj, 560, 40, 0xB00 ^ key);
    if (c) {
      p_plate(c, 0, 0, 560, 40, 10, AT_INK, .82f);
      const char *s = near_rack ? "REBOOT THE RACK  {USE}" : "Reach the server rack. Keep off the red X.";
      g_markup(c, 16, 26, CV_LEFT, F_TYPE, 20, .1f, AT_PAPER, 20, true, s, true);
      if (tries > 0) {
        char t[32]; snprintf(t, sizeof t, "FELL: %d", tries);
        p_text(c, F_MONO, 15, 1, CV_LEFT, t, 460, 25, AT_RED, 1);
      }
      layer_end(&L_obj, c);
    }
    hg_tex(&L_obj, 44, 648, 1);
  }
  /* the reboot bar */
  if (reboot > 0 && phase == A_PLAY) {
    float w = 360, x = 640 - w / 2, y = 150;
    hg_rect(x - 3, y - 3, w + 6, 24, AT_INK, .78f);
    hg_rect(x, y, w * ds_clamp(reboot, 0, 1), 18, AT_GREEN, 1);
    cv *c = layer_begin(&L_bar, 360, 26, 0xC00);
    if (c) { p_text(c, F_MONO, 15, 2, CV_CENTER, "REBOOTING DOOR CONTROLLER", 180, 18, AT_PAPER, 1); layer_end(&L_bar, c); }
    hg_tex(&L_bar, x, y - 26, 1);
  }
  /* the banners */
  if (phase == A_FELL || phase == A_WON) {
    bool won = phase == A_WON;
    float a = hud_ease_out(phase_t * 3);
    hg_rect(0, 280, 1280, 160, won ? 0x0b2414 : 0x2a0e0a, .68f * a);
    cv *c = layer_begin(&L_banner, 900, 150, won ? 0xD01 : 0xD02);
    if (c) {
      p_text(c, F_DISPLAY, 60, 2, CV_CENTER, won ? "BACK ONLINE" : "THROUGH THE FLOOR",
             450, 66, won ? AT_GREEN : AT_RED, 1);
      p_text(c, F_TYPE, 22, 0, CV_CENTER,
             won ? "Members site, wiki and door controller rebooted." : "You put a boot through the ceiling.",
             450, 104, AT_PAPER, 1);
      g_markup(c, 450, 134, CV_CENTER, F_MONO, 15, 1, AT_YEL, 16, true,
               won ? "{USE} climb down" : "{USE} try again    {BACK} climb down", true);
      layer_end(&L_banner, c);
    }
    hg_tex_m(&L_banner, (float[6]){1, 0, 0, 1, 640 - 450, 300}, a, 0xffffff);
  }
  hg_end();
}
#endif
