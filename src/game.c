/* game.c: the title, the day, the pause menu, the ending and the Battle Zone. */
#if !defined(KH_TEST_WORLD) && !defined(KH_TEST_FIGHT) && !defined(KH_TEST_ATTIC) && !defined(KH_TEST_TERMINAL)
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
#include "battle.h"
#include "attic.h"
#include <string.h>

static t3_scene *scene; static t3_camera *cam;
static enum { G_TITLE, G_PLAY, G_PAUSE, G_END, G_BATTLE, G_ATTIC } mode;
static float mode_t;
static int who, pause_sel, title_row, night_sel = 1; static bool music_on = true;
static int pause_saved;   /* the pause menu: 1 once this pause saved */
static actor_t *T_A[3];
static const char *const NAMES[3] = {"VEE", "DEX", "RUTH"};
static const char *const BLURB[3] = {"First to look up. Runs the printers.", "Electronics bench. Tells you how it broke.", "Laser corner. Has actually read the manual."};
static const actor_model TITLE_M[3] = {AM_C, AM_D, AM_B};

static void title_enter(void) {
  mode = G_TITLE; mode_t = 0; title_row = 0;
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
  actor_preload_all();   /* every body parsed now, so nobody stalls a frame when they walk in */
  battle_init(scene, cam);
  attic_init(scene, cam);
  title_enter();
}
void game_frame(float dt) {
  const intent_t *in = input_get();
  mode_t += dt;
  switch (mode) {
    case G_TITLE: {
      if (in->nav_x && title_row == 0) { int n = who + in->nav_x; if (n >= 0 && n < 3) { who = n; sfx(SFX_UI, 0, 1); } }
      /* down to the Battle Zone's entry, up to the keyholders */
      /* v1.3: and further down, the night select once a night has been reached */
      int nights = story_nights_open();
      if (in->nav_y) { int n = title_row + in->nav_y; if (n >= 0 && n < (nights ? 3 : 2)) { title_row = n; sfx(SFX_UI, 0, 1); } }
      if (title_row == 2 && nights) {
        if (!(nights & (1 << (night_sel - 1)))) for (int k = 1; k <= story_night_count(); k++) if (nights & (1 << (k - 1))) { night_sel = k; break; }
        if (in->nav_x) for (int k = night_sel + in->nav_x; k >= 1 && k <= story_night_count(); k += in->nav_x) if (nights & (1 << (k - 1))) { night_sel = k; sfx(SFX_UI, 0, 1); break; }
        if (in->ok) { sfx(SFX_UI_OK, 0, 1); title_leave(); story_start_night(night_sel); mode = G_PLAY; mode_t = 0; break; }
      }
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
      /* v2.1: with a save, ENTER (A) continues it, so a habit press never starts over; N (X) starts a new day */
      const bool has = story_has_save();
      const bool cont = title_row == 0 && has && (in->ok || input_key_down(0x06) || in->heavy);   /* or C, Y */
      const bool fresh = title_row == 0 && (has ? (input_key_down(0x11) || (in->used_pad && in->light)) : in->ok);
      if (in->ok && title_row == 1) {
        if (battle_available()) {
          sfx(SFX_UI_OK, 0, 1);
          title_leave();
          battle_set_fighters(T_A, who); battle_enter();
          mode = G_BATTLE; mode_t = 0;
        } else sfx(SFX_CLASH, 0, .5f);
        break;
      }
      if (fresh || cont) {
        sfx(SFX_UI_OK, 0, 1);
        title_leave();
        if (cont) story_continue(); else story_new_game(who);
        mode = G_PLAY; mode_t = 0;
      }
      break;
    }
    case G_PLAY:
      if (in->pause) { mode = G_PAUSE; pause_sel = 0; pause_saved = 0; mode_t = 0; sfx(SFX_UI, 0, 1); break; }
      story_update(dt, in);
      if (story_take_attic()) { attic_enter(); mode = G_ATTIC; mode_t = 0; break; }
      if (story_state() == SR_EPILOGUE) { mode = G_END; mode_t = 0; }
      break;
    case G_ATTIC:
      if (!attic_update(dt, in)) { story_attic_result(attic_won()); mode = G_PLAY; mode_t = 0; }
      break;
    case G_PAUSE:
      if (in->pause || in->back) { mode = G_PLAY; break; }
      if (in->nav_y) { int n = pause_sel + in->nav_y; if (n >= 0 && n < 4) { pause_sel = n; sfx(SFX_UI, 0, 1); } }
      if (in->ok) {
        if (pause_sel == 0) { sfx(SFX_UI_OK, 0, 1); mode = G_PLAY; }
        else if (pause_sel == 1) { if (story_save_here()) { pause_saved = 1; sfx(SFX_UI_OK, 0, 1); } else sfx(SFX_CLASH, 0, .5f); }
        else if (pause_sel == 2) { sfx(SFX_UI_OK, 0, 1); music_on = !music_on; audio_set_music(music_on); }
        else { sfx(SFX_UI_OK, 0, 1); fight_end(); title_enter(); }
      }
      break;
    case G_BATTLE:
      if (!battle_update(dt, in)) title_enter();
      break;
    case G_END:
      world_update(dt, v3_(0, 1, 5));
      {
        bool ok = mode_t > 6 && in->ok;
#ifdef KH_AUTO
        ok = mode_t > 3;   /* the autoplay reads the epilogue for three seconds, then goes on */
#endif
        /* v1.3: the day's epilogue (and each night's) leads on into the next night */
        if (ok) { if (story_resume()) { mode = G_PLAY; mode_t = 0; } else title_enter(); }
      }
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
    case G_TITLE: hud_title_draw(mode_t, title_row, who, story_has_save(), NAMES, BLURB); hud_battle_title(mode_t, title_row == 1);
      hud_nights_title(mode_t, title_row == 2, night_sel, story_nights_open(), story_night_count(), story_night_title(night_sel)); break;
    case G_BATTLE: battle_draw(); break;
    case G_ATTIC: attic_draw(); break;
    case G_PLAY:
      if (story_state() == SR_FIGHT && !STORY_UI.retry) fight_draw_hud();
      hud_vn_draw();
      break;
    case G_PAUSE: hud_vn_draw(); hud_pause_draw(pause_sel, music_on, pause_saved ? 2 : story_can_save() ? 0 : 1); break;
    case G_END: {
      const char *lines[16]; int n = story_end_lines(lines, 16);
      hud_end_draw(mode_t, story_end_title(), lines, n, STORY_UI.stats);
      break;
    }
  }
}
#endif
