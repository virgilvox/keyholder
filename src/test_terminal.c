/* test_terminal.c: the pizza-box terminal on its own, for a headless look. Boots a bare scene, opens the
 * terminal with every hack unlocked, then scripts an input run: skip the boot typing, drop into the menu,
 * launch TRACE.RUN, claim the greedy path to the target and win. game_draw renders the (empty) scene then
 * the full-screen CRT over it.
 *   build with EXTRA_CFLAGS=-DKH_TEST_TERMINAL, then tools/shot.mjs. */
#ifdef KH_TEST_TERMINAL
#include "game.h"
#include "world.h"
#include "fx.h"
#include "terminal.h"
#include "input.h"
#include "hud_int.h"
#include "ds.h"
#include <string.h>

static t3_scene *scene; static t3_camera *cam;

void game_init(void) {
  scene = t3_scene_new();
  cam = t3_perspective_camera_new(38, 16.f / 9.f, .06f, 1600);
  t3_object_add(&scene->base, cam);
  hg_init();
  world_init(scene);
  fx_init(scene);
  world_set_stage(STAGE_TOWN); world_set_time(22.f);
  term_init();
  term_open(0xFF);                 /* every program available */
}

static int fr = -1;
void game_frame(float dt) {
  fr++;
  intent_t in; memset(&in, 0, sizeof in); in.weapon_pick = -1; in.used_pad = true;
  /* 12: skip the boot typing.  20: boot -> menu.  30: launch TRACE.RUN.  40/47/54: claim toward the target. */
  in.ok = (fr == 12 || fr == 20 || fr == 30 || fr == 40 || fr == 47 || fr == 54);
  world_update(dt, v3_(0, 0, 0));
  term_update(dt, &in);
}

void game_draw(void) {
  render_scene(scene, cam);
  render_target();
  term_draw();
}
#endif
