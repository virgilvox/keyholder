/* test_cast.c: every body on its own (KH_TEST_CAST). Each model gets six shots of PHASE frames:
 * standing (front), walking (three quarter), talking with expressions (face close up), a fight
 * stance (side), the back (hair springs) and the far level (8 m and 14 m). Then a line up of the
 * whole cast, then a crowd of twelve at 4 to 15 m for the triangle count, then three spawns
 * mid scene for the hitch check. The schedule is logged ([cast] lines) so frames can be picked.
 * KH_SRC="main.c present.c audio.c voice.c input.c actor.c anim.c test_cast.c" EXTRA_CFLAGS=-DKH_TEST_CAST */
#ifdef KH_TEST_CAST
#include "game.h"
#include "actor.h"
#include <stdio.h>
#include <string.h>

#define PHASE 48
#define NPH 6
static t3_scene *scene; static t3_camera *cam;
static actor_t *A[AM_COUNT], *crowd[12], *late[3];
static int frame;
static const char *PH[NPH] = {"stand", "walk", "talk", "fight", "back", "far"};

static void lights(void) {
  scene->has_background = true; scene->background = t3_color_hex(0x8fb4d8); scene->fog = t3_fog_linear(0x8fb4d8, 60, 300);
  t3_light *H = t3_hemisphere_light_new(0xffffff, 0xb0a8b8, 2.4f); t3_object_add(&scene->base, H);
  t3_light *D = t3_directional_light_new(0xfff4e0, 1.6f); t3_object_set_position(D, 2, 5, 4); t3_object_add(&scene->base, D);
  t3_mesh *g = t3_mesh_new(t3_plane_geometry_new(200, 200, 1, 1), t3_mesh_lambert_material_new(0x9a9286));
  t3_object_set_rotation(&g->base, -DS_PI / 2, 0, 0); t3_object_add(&scene->base, &g->base);
}
static void look(float ex, float ey, float ez, float ax, float ay, float az, float fov) {
  t3_object_set_position(cam, ex, ey, ez); t3_object_look_at(cam, ax, ay, az);
  if (cam->fov != fov) { cam->fov = fov; t3_camera_update_projection_matrix(cam); }
}
static void hide_all(void) {
  for (int i = 0; i < AM_COUNT; i++) actor_visible(A[i], false);
  for (int i = 0; i < 12; i++) if (crowd[i]) actor_visible(crowd[i], false);
}

void game_init(void) {
  scene = t3_scene_new();
  cam = t3_perspective_camera_new(40, 16.f / 9.f, .05f, 400); actor_lod_camera(cam);
  lights();
  actor_preload_all();
  for (int i = 0; i < AM_COUNT; i++) { A[i] = actor_new((actor_model)i, &ACTOR_PAL_NONE, 1, scene); actor_visible(A[i], false); }
  ds_log("[cast] %d models, %d frames a phase, phases stand walk talk fight back far; line up at frame %d, crowd at %d, spawns at %d",
         AM_COUNT, PHASE, AM_COUNT * NPH * PHASE, AM_COUNT * NPH * PHASE + 60, AM_COUNT * NPH * PHASE + 120);
}

#ifndef KH_CAST_FROM
#define KH_CAST_FROM 0   /* -DKH_CAST_FROM=n starts at model n (frame numbers stay the same) */
#endif
void game_frame(float dt) {
  if (frame < KH_CAST_FROM * NPH * PHASE) frame = KH_CAST_FROM * NPH * PHASE;
  int f = frame++;
  int per = NPH * PHASE, end = AM_COUNT * per;
  if (f < end) {
    int m = f / per, ph = (f % per) / PHASE, t = f % PHASE;
    actor_t *a = A[m];
    if (t == 0) {
      hide_all(); actor_visible(a, true); actor_expr_clear(a); actor_mouth(a, 0, 0);
      actor_place(a, v3_(0, 0, 0), 0);
      static const char *CLIP[NPH] = {"Idle_Loop", "Walk_Loop", "Idle_Talking_Loop", "Sword_Idle", "Jog_Fwd_Loop", "Idle_Loop"};
      actor_replay(a, CLIP[ph], 1, true, 0);
      if (t == 0 && ph == 0) ds_log("[cast] model %d from frame %d", m, f);
    }
    float h = actor_height(a);
    switch (ph) {
    case 0: look(0, h * .55f, 3.4f, 0, h * .5f, 0, 40); break;
    case 1: look(2.4f, h * .6f, 2.4f, 0, h * .5f, 0, 40); break;
    case 2: {   /* joy, then angry, then sorrow, talking through it */
      look(0.05f, h - .08f, .75f, 0, h - .1f, 0, 38);
      actor_expr_clear(a);
      actor_expr_set(a, t < 16 ? EX_JOY : t < 32 ? EX_ANGRY : EX_SORROW, 1);
      actor_mouth(a, .5f + .5f * sinf(f * .7f), (f / 6 % 5) * .25f);
      actor_look(a, v3_(.6f, h, 2), 1);
      break; }
    case 3: look(3.2f, h * .55f, .4f, 0, h * .5f, 0, 40); break;
    case 4: look(0, h * .75f, -2.6f, 0, h * .6f, 0, 40); break;
    case 5: look(0, 1.5f, t < PHASE / 2 ? 8 : 14, 0, h * .5f, 0, 40); break;
    }
    actor_update(a, dt);
    return;
  }
  f -= end;
  if (f % 12 == 0 && f < 60) {   /* the line up, in model order left to right, nine at a time (five groups, 12 frames each) */
    hide_all();
    int lo = f / 12 * 9, hi = lo + 9 < AM_COUNT ? lo + 9 : AM_COUNT;
    for (int i = lo; i < hi; i++) {
      actor_visible(A[i], true); actor_expr_clear(A[i]); actor_mouth(A[i], 0, 0); actor_look(A[i], v3_(0, 0, 0), 0);
      actor_place(A[i], v3_((i - lo - (hi - lo - 1) * .5f) * .7f, 0, 0), 0);
      actor_replay(A[i], "Idle_Loop", 1, true, 0);
    }
    actor_lod_bias(100);
  }
  if (f < 60) {
    look(0, .95f, 5.6f, 0, .85f, 0, 40);
    for (int i = 0; i < AM_COUNT; i++) actor_update(A[i], dt);
    return;
  }
  f -= 60;
  if (f == 0) {   /* a crowd of twelve, 4 to 15 m out */
    actor_lod_bias(1);
    hide_all();
    for (int i = 0; i < 12; i++) {
      crowd[i] = actor_new((actor_model)(i % AM_COUNT), &ACTOR_PAL_NONE, 1, scene);
      float d = 4 + i, ang = (i % 2 ? 1 : -1) * (.08f + .03f * i);
      actor_place(crowd[i], v3_(sinf(ang) * d, 0, -cosf(ang) * d), ds_rr(-3, 3));
      actor_play(crowd[i], i % 3 ? "Idle_Loop" : "Walk_Loop", 1, true, 0);
    }
  }
  if (f < 60) {
    look(0, 1.6f, 0, 0, 1.2f, -10, 55);
    for (int i = 0; i < 12; i++) actor_update(crowd[i], dt);
    return;
  }
  f -= 60;
  /* spawns mid scene: three new actors of preloaded models, one a frame */
  if (f < 3) { late[f] = actor_new((actor_model)((f * 5) % AM_COUNT), &ACTOR_PAL_NONE, 1, scene); actor_place(late[f], v3_(f - 1.f, 0, -3), 0); ds_log("[cast] spawn %d at frame %d", f, frame - 1); }
  look(0, 1.6f, 0, 0, 1.2f, -10, 55);
  for (int i = 0; i < 12; i++) actor_update(crowd[i], dt);
  for (int i = 0; i < 3 && i <= f; i++) actor_update(late[i], dt);
}

void game_draw(void) {
  render_scene(scene, cam);
  const t3_render_info *ri = t3_renderer_info(DS_R);
  int per = NPH * PHASE, end = AM_COUNT * per, f = frame - 1;
  if (f < end && f % PHASE == PHASE - 1) ds_log("[cast] model %d %s: calls %d tris %d lod %d", f / per, PH[(f % per) / PHASE], (int)ri->calls, (int)ri->triangles, actor_lod_level(A[f / per]));
  if (f == end + 29 || f == end + 59 || f == end + 119) ds_log("[cast] %s: calls %d tris %d", f < end + 60 ? "line up" : "crowd of 12", (int)ri->calls, (int)ri->triangles);
}
#endif
