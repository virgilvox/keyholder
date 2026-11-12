/* test_attic.c: the rebuilt attic stage on its own, for a headless look. Boots the world, drops a player
 * at the foot of the stairs and warps into STAGE_ATTIC, then scripts a path that shows the glitched room,
 * a fall, the reboot (room goes normal), the hub, a memory and the terminal.
 *   KH_SRC-free build with EXTRA_CFLAGS=-DKH_TEST_ATTIC, then tools/shot.mjs. */
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
  cam = t3_perspective_camera_new(40, 16.f / 9.f, .06f, 1600);
  t3_object_add(&scene->base, cam);
  hg_init();
  world_init(scene);
  fx_init(scene);
  player_init(cam);
  A = actor_new(AM_C, &ACTOR_PAL_NONE, 1, scene);
  actor_play(A, "Idle_Loop", 1, true, 0);
  world_set_stage(STAGE_TOWN); world_set_time(14.f);
  player_set_actor(A);
  player_teleport(v3_(0.3f, 0, -31.3f), 3.14159f);   /* the lab stair bottom (where we return to) */
  attic_init(scene, cam);
  attic_enter();                                      /* warp into STAGE_ATTIC, glitched, A_PLAY */
}

static int fr = -1;
static bool left = false;
void game_frame(float dt) {
  fr++;
  intent_t in; memset(&in, 0, sizeof in); in.weapon_pick = -1;
  if (fr < 130) { in.my = 1.f; in.used_pad = true; }        /* walk north into the hazards (should fall) */
  if (fr == 150) player_teleport(v3_(-3.3f, 0, -4.0f), 3.14159f);  /* near the rack to trigger the win */
  if (fr >= 152 && fr < 180) { in.my = 1.f; in.used_pad = true; }  /* step to the rack -> win, then hub */
  /* hub: examine the dead CRT -> a memory */
  if (fr == 430) player_teleport(v3_(5.2f, 0, -2.3f), 3.14159f);
  if (fr >= 432 && fr < 470) { in.my = 1.f; in.used_pad = true; }
  if (fr == 478) in.interact = true;
  /* hub: open the terminal */
  if (fr == 560) player_teleport(v3_(-1.3f, 0, -3.9f), 3.14159f);
  if (fr >= 562 && fr < 600) { in.my = 1.f; in.used_pad = true; }
  if (fr == 606) in.interact = true;
  if (fr == 760) in.back = true;          /* log off the terminal, back to the hub */
  if (fr == 820) in.back = true;          /* leave the attic, back down to the lab */
  if (!left) { if (!attic_update(dt, &in)) left = true; }
  else world_update(dt, player_pos());    /* back in STAGE_TOWN: confirm the lab renders */
}

void game_draw(void) {
  render_scene(scene, cam);
  render_target();
  if (!left) attic_draw();
}
#endif
