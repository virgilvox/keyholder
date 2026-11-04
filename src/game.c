#include "game.h"
#include "actor.h"
static t3_scene *scene; static t3_camera *cam; static actor_t *A[5]; static float T;
void game_init(void) {
  scene = t3_scene_new();
  scene->has_background = true; scene->background = t3_color_hex(0x6a9ad0);
  scene->fog = t3_fog_linear(0x6a9ad0, 30, 200);
  cam = t3_perspective_camera_new(38, 16.f/9.f, .05f, 500);
  t3_light *h = t3_hemisphere_light_new(0xffffff, 0xb0a8b8, 2.4f); t3_object_add(&scene->base, h);
  t3_light *d = t3_directional_light_new(0xfff4e0, 1.6f); t3_object_set_position(d, 2, 5, 4); t3_object_add(&scene->base, d);
  t3_material *gm = t3_mesh_lambert_material_new(0x90887c);
  t3_mesh *g = t3_mesh_new(t3_plane_geometry_new(100, 100, 1, 1), gm); t3_object_set_rotation(&g->base, -DS_PI/2, 0, 0); t3_object_add(&scene->base, &g->base);
  const char *clips[5] = {"Idle_Talking_Loop","Idle_Loop","Sword_Idle","Idle_Loop","Idle_Loop"};
  for (int i = 0; i < 5; i++) {
    A[i] = actor_new((actor_model)i, &ACTOR_PAL_NONE, 1, scene);
    actor_place(A[i], v3_((i - 2) * .75f, 0, 0), 0);
    actor_play(A[i], clips[i], 1, true, 0);
  }
}
void game_frame(float dt) {
  T += dt;
  for (int i = 0; i < 5; i++) { actor_mouth(A[i], .5f + .5f * sinf(T * 8), fmodf(T, 1)); actor_expr_set(A[i], EX_JOY, i == 4); actor_update(A[i], dt); }
  t3_object_set_position(cam, 0, 1.42f, 1.6f); t3_object_look_at(cam, -1.1f, 1.38f, 0);
}
void game_draw(void) { render_scene(scene, cam); }
