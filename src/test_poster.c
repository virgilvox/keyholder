/* test_poster.c: the cast in battle poses, each alone over black then white so tools/bake-poster.mjs
 * can take the alpha and auto-crop a clean colour cutout for the station cover poster.
 * KH_SRC="main.c present.c audio.c voice.c input.c actor.c anim.c test_poster.c" EXTRA_CFLAGS=-DKH_TEST_POSTER */
#ifdef KH_TEST_POSTER
#include "game.h"
#include "actor.h"
#include <stdio.h>
#include <string.h>

#define STEP 16   /* frames a character: pose settles, then black at STEP-2, white at STEP-1 */

struct pose { const char *key; actor_model model; const char *clip; float yaw, hold; };
static const struct pose POSE[] = {
  { "vee",     AM_C,  "Sword_Regular_C",   0.42f, 0.44f },
  { "dex",     AM_D,  "Sword_Regular_A",  -0.40f, 0.40f },
  { "ruth",    AM_B,  "Spell_Simple_Shoot", 0.34f, 0.60f },
  { "dobbs",   AM_B2, "Melee_Hook",       -0.38f, 0.36f },
  { "vic",     AM_E2, "Sword_Regular_B",   0.44f, 0.40f },
  { "champ",   AM_Y,  "Punch_Cross",      -0.40f, 0.30f },
  { "cal",     AM_C2, "Sword_Heavy_Combo", 0.40f, 0.44f },
  { "rennick", AM_H,  "Sword_Dash",       -0.40f, 0.30f },
  { "bex",     AM_Z,  "Sword_Regular_C",   0.40f, 0.44f },
  { "root",    AM_O,  "Sword_Regular_A",  -0.42f, 0.40f },
};
#define POSE_N ((int)(sizeof POSE / sizeof POSE[0]))

static t3_scene *scene; static t3_camera *cam;
static actor_t *cur;
static int frame = -1;

static void lights(void) {
  t3_light *H = t3_hemisphere_light_new(0xffffff, 0xb6aec2, 2.2f); t3_object_add(&scene->base, H);
  t3_light *D = t3_directional_light_new(0xfff4e0, 1.8f); t3_object_set_position(D, 2.4f, 5, 3.4f); t3_object_add(&scene->base, D);
}

void game_init(void) {
  scene = t3_scene_new();
  scene->has_background = true; scene->background = t3_color_hex(0x000000);
  cam = t3_perspective_camera_new(38, 16.f / 9.f, .05f, 200); actor_lod_camera(cam);
  lights();
  actor_lod_bias(100);   /* always the near level */
}

void game_frame(float dt) {
  int f = ++frame;
  int i = f / STEP, k = f % STEP;
  if (i >= POSE_N) return;
  if (k == 0) {
    if (cur) { actor_visible(cur, false); actor_free(cur); }
    cur = actor_new(POSE[i].model, &ACTOR_PAL_NONE, 1, scene);
    actor_place(cur, v3_(0, 0, 0), POSE[i].yaw);
    actor_replay(cur, POSE[i].clip, 1, false, 0);
    ds_log("[poster] %d %s", i, POSE[i].key);
  }
  /* advance the pose a little each pre-read frame, reaching the hold time by STEP-2, then freeze */
  actor_update(cur, k < STEP - 2 ? POSE[i].hold / (STEP - 2) : 0);
  float h = actor_height(cur);
  /* a 3/4 action camera framing the whole body with headroom */
  float a = -0.4f, d = h * 1.75f + 0.6f;
  float lx = 0, ly = h * 0.52f, lz = 0;
  t3_object_set_position(cam, lx + sinf(a) * d, ly + 0.25f, lz + cosf(a) * d);
  t3_object_look_at(cam, lx, ly, lz);
  scene->background = k == STEP - 1 ? t3_color_hex(0xffffff) : t3_color_hex(0x000000);
}

void game_draw(void) { render_scene(scene, cam); }
#endif
