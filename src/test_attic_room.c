/* test_attic_room.c: a showroom of the baked attic (tools/prep-attic.mjs) for headless screenshots. No
 * gameplay: the room in its normal mood from a set of fixed cameras, then once glitched, with a body at
 * the stair head for scale. Build with EXTRA_CFLAGS=-DKH_TEST_ATTIC_ROOM and game.c left out of KH_SRC.
 * Each view holds for 60 frames: shoot frames 50, 110, 170, ... */
#ifdef KH_TEST_ATTIC_ROOM
#include "game.h"
#include "world.h"
#include "player.h"
#include "actor.h"
#include "fx.h"
#include "attic_env.h"
#include "hud_int.h"
#include "ds.h"
#include <string.h>

static t3_scene *scene; static t3_camera *cam; static actor_t *A;

typedef struct { v3 pos, look; float normal; } view_t;
static const view_t VIEWS[] = {
  { { -1.5f, 1.75f,  2.6f }, { -2.6f, 1.0f, -3.6f }, 1 },   /* coming up the stair: the rack ahead */
  { {  2.6f, 2.35f,  3.7f }, { -2.2f, 0.5f, -2.4f }, 1 },    /* wide from the south-east corner */
  { { -2.2f, 1.5f, -1.6f }, { -3.8f, 1.0f, -4.2f }, 1 },    /* the rack and the parts shelf */
  { { -1.0f, 1.6f,  0.0f }, {  4.4f, 0.9f, -1.0f }, 1 },    /* the east wall: metal stock, crafts */
  { {  0.6f, 1.7f, -1.5f }, {  1.8f, 0.7f,  4.4f }, 1 },    /* the south wall: robots, holiday */
  { {  0.8f, 1.7f, -0.5f }, { -4.6f, 0.8f,  1.5f }, 1 },    /* the west wall: member storage */
  { {  1.5f, 1.6f,  1.0f }, {  0.8f, 0.0f, -2.0f }, 1 },    /* the paint on the floor */
  { {  2.6f, 2.35f,  3.7f }, { -2.2f, 0.5f, -2.4f }, 0 },    /* wide, glitched */
  { { -2.7f, 1.7f, -0.9f }, { -4.1f, 1.1f, -3.6f }, 1 },     /* standing at the terminal (the player moves there) */
};
#define NV ((int)(sizeof VIEWS / sizeof VIEWS[0]))

void game_init(void) {
  scene = t3_scene_new();
  cam = t3_perspective_camera_new(50, 16.f / 9.f, .06f, 1600);
  t3_object_add(&scene->base, cam);
  hg_init();
  world_init(scene);
  fx_init(scene);
  player_init(cam);
  attic_env_init(scene);
  A = actor_new(AM_C, &ACTOR_PAL_NONE, 1, scene);
  actor_play(A, "Idle_Loop", 1, true, 0);
  player_set_actor(A);
  attic_env_show(true);
  attic_env_reset(1);
  world_set_stage(STAGE_ATTIC);
  player_teleport(ATTIC_ENTRY, DS_PI);
}

static int fr = -1;
void game_frame(float dt) {
  fr++;
  int v = (fr / 60) % NV;
  if (fr % 60 == 0) {
    attic_env_reset(VIEWS[v].normal);
    if (v == NV - 1) player_teleport(ATTIC_TERM, DS_PI); else player_teleport(ATTIC_ENTRY, DS_PI);
  }
  world_update(dt, player_pos());
  actor_update(A, dt);
  t3_object_set_position(cam, VIEWS[v].pos.x, VIEWS[v].pos.y, VIEWS[v].pos.z);
  t3_object_look_at(cam, VIEWS[v].look.x, VIEWS[v].look.y, VIEWS[v].look.z);
  t3_object_update_world_matrix(cam, true, true);
}

void game_draw(void) { render_scene(scene, cam); render_target(); }
#endif
