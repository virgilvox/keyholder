/* game.c: the title, the day, the pause menu and the ending. */
#if !defined(KH_TEST_WORLD) && !defined(KH_TEST_FIGHT)
#include "game.h"
#include "actor.h"
#include "audio.h"
#include "fight.h"
#include "fx.h"
#include "input.h"
#include "player.h"
#include "story.h"
#include "hud_vn.h"
#include "world.h"
#include "hud_int.h"
#include <string.h>

static t3_scene *scene; static t3_camera *cam;
static enum { G_TITLE, G_PLAY, G_PAUSE, G_END } mode;
static float mode_t;
static int who, pause_sel; static bool music_on = true;
static actor_t *T_A[3];
static const char *const NAMES[3] = {"VEE", "DEX", "RUTH"};
static const char *const BLURB[3] = {"First to look up. Runs the printers.", "Electronics bench. Tells you how it broke.", "Laser corner. Has actually read the manual."};
static const actor_model TITLE_M[3] = {AM_C, AM_D, AM_B};

static void title_enter(void) {
  mode = G_TITLE; mode_t = 0;
  story_stop();
  world_set_stage(STAGE_TOWN); world_set_time(16.6f);
  music_mood(MOOD_TITLE);
  v3 p; float y;
  if (!world_place("lab_door", &p, &y)) { p = v3_(0, 0, 3); y = 0; }
  for (int i = 0; i < 3; i++) {
    if (!T_A[i]) T_A[i] = actor_new(TITLE_M[i], &ACTOR_PAL_NONE, 1, scene);
    actor_visible(T_A[i], true);
    actor_place(T_A[i], v3_(p.x + (i - 1) * 1.15f, p.y, p.z + 5.6f + (i == 1 ? -.3f : 0)), 0);
    actor_play(T_A[i], "Idle_Loop", 1, true, 0);
  }
}
static void title_leave(void) { for (int i = 0; i < 3; i++) if (T_A[i]) actor_visible(T_A[i], false); }
void game_init(void) {
  scene = t3_scene_new();
  cam = t3_perspective_camera_new(38, (float)DS_W / DS_H, .06f, 1600);
  t3_object_add(&scene->base, cam);
  hg_init();
  world_init(scene);
  player_init(cam);
  fight_init(scene, cam);
  story_init(scene, cam);
  title_enter();
}
void game_frame(float dt) {
  const intent_t *in = input_get();
  mode_t += dt;
  switch (mode) {
    case G_TITLE: {
      if (in->nav_x) { int n = who + in->nav_x; if (n >= 0 && n < 3) { who = n; sfx(SFX_UI, 0, 1); } }
      for (int i = 0; i < 3; i++) {
        actor_expr_set(T_A[i], EX_JOY, i == who ? .7f : 0);
        actor_play(T_A[i], i == who ? "Sword_Idle" : "Idle_Loop", 1, true, .3f);
        v3 p = actor_pos(T_A[i]);
        actor_look(T_A[i], v3_(p.x + (who - i) * .6f, p.y + 1.4f, p.z + 3), .8f);
        actor_update(T_A[i], dt);
      }
      v3 c = actor_pos(T_A[1]);
      float sway = sinf(mode_t * .25f) * .4f;
      t3_object_set_position(cam, c.x + sway + (who - 1) * .3f, c.y + 1.2f, c.z + 6.2f);
      t3_object_look_at(cam, c.x + (who - 1) * .25f, c.y + 1.15f, c.z);
      if (cam->fov != 38) { cam->fov = 38; t3_camera_update_projection_matrix(cam); }
      world_update(dt, v3_(c.x, c.y + 1, c.z + 4));
      const bool cont = story_has_save() && (input_key_down(0x06) || in->heavy);   /* C or Y */
      if (in->ok || cont) {
        sfx(SFX_UI_OK, 0, 1);
        title_leave();
        if (cont) story_continue(); else story_new_game(who);
        mode = G_PLAY; mode_t = 0;
      }
      break;
    }
    case G_PLAY:
      if (in->pause) { mode = G_PAUSE; pause_sel = 0; mode_t = 0; sfx(SFX_UI, 0, 1); break; }
      story_update(dt, in);
      if (story_state() == SR_EPILOGUE) { mode = G_END; mode_t = 0; }
      break;
    case G_PAUSE:
      if (in->pause || in->back) { mode = G_PLAY; break; }
      if (in->nav_y) { int n = pause_sel + in->nav_y; if (n >= 0 && n < 3) { pause_sel = n; sfx(SFX_UI, 0, 1); } }
      if (in->ok) {
        sfx(SFX_UI_OK, 0, 1);
        if (pause_sel == 0) mode = G_PLAY;
        else if (pause_sel == 1) { music_on = !music_on; audio_set_music(music_on); }
        else { fight_end(); title_enter(); }
      }
      break;
    case G_END:
      world_update(dt, v3_(0, 1, 5));
      if (mode_t > 6 && in->ok) title_enter();
      break;
  }
}
void game_draw(void) {
  render_scene(scene, cam);
  render_target();
#ifdef KH_NOHUD
  return;   /* clean frames for the web page's backdrop */
#endif
  switch (mode) {
    case G_TITLE: hud_title_draw(mode_t, 0, who, story_has_save(), NAMES, BLURB); break;
    case G_PLAY:
      if (story_state() == SR_FIGHT && !STORY_UI.retry) fight_draw_hud();
      hud_vn_draw();
      break;
    case G_PAUSE: hud_vn_draw(); hud_pause_draw(pause_sel, music_on); break;
    case G_END: {
      const char *lines[16]; int n = story_end_lines(lines, 16);
      hud_end_draw(mode_t, story_end_title(), lines, n, STORY_UI.stats);
      break;
    }
  }
}
#endif
