/* test_attic.c: the attic minigame on its own, for a headless look. Boots the world, drops a player
 * at the foot of the stairs and warps straight up, then walks north (into the hazards) and, later,
 * right up to the rack to reboot it. Logs the player's height so the floor and fences can be checked.
 *   KH_SRC build with EXTRA_CFLAGS=-DKH_TEST_ATTIC, then tools/shot.mjs. */
#ifdef KH_TEST_ATTIC
#include "game.h"
#include "world.h"
#include "player.h"
#include "actor.h"
#include "fx.h"
#include "attic.h"
#include "input.h"
#include "hud_int.h"
#include "ds.h"
#include <string.h>
#include <stdio.h>

static t3_scene *scene; static t3_camera *cam; static actor_t *A;

void game_init(void) {
  scene = t3_scene_new();
  cam = t3_perspective_camera_new(38, 16.f / 9.f, .06f, 1600);
  t3_object_add(&scene->base, cam);
  hg_init();
  world_init(scene);
  fx_init(scene);
  player_init(cam);
  A = actor_new(AM_C, &ACTOR_PAL_NONE, 1, scene);
  actor_play(A, "Idle_Loop", 1, true, 0);
  world_set_stage(STAGE_TOWN); world_set_time(14.f);
  player_set_actor(A);
  player_teleport(v3_(0.3f, 0, -31.5f), DS_PI);   /* the foot of the stairs */
  attic_init(scene, cam);                         /* builds the sign across the stairs */
}

static int fr = -1;
void game_frame(float dt) {
  fr++;
  intent_t in; memset(&in, 0, sizeof in); in.weapon_pick = -1;
  if (fr == 0) { player_teleport(v3_(0.3f, 0, -29.5f), DS_PI); player_camera_snap(); }
  if (fr < 60) {
    /* walk north up to the roped-off stairs with the normal follow camera: the player's real view */
    intent_t w; memset(&w, 0, sizeof w); w.weapon_pick = -1;
    if (fr > 8) { w.my = 1.f; w.used_pad = true; }   /* step toward the stairs, then the barrier stops us */
    player_update(dt, &w);
    world_update(dt, player_pos());
    return;
  }
  if (fr == 60) player_teleport(v3_(0.3f, 0, -31.5f), DS_PI);
  if (fr == 40) attic_enter();
  if (fr < 200) { in.my = 1.f; in.used_pad = true; }   /* walk north into the field */
  if (fr == 250) player_teleport(v3_(-2.0f, 3.4f, -38.4f), DS_PI);   /* onto the safe cell by the rack */
  in.interact = (fr == 280);
  attic_update(dt, &in);
}

void game_draw(void) {
  render_scene(scene, cam);
  render_target();
  attic_draw();
}
#endif
