/* attic.c: the attic as a mode (see attic.h and docs/ATTIC.md). The first time up, the attic is the
 * nightmare: attic_deep.c walks it and asks for its fights, which are story scenes (tools/script/attic.mjs)
 * played by the story VM with the day held. Beating SYSOP at the rack turns the stage back into the
 * blueprint room, which from then on is where the stairs lead: attic_hub.c walks it, a shelf opens its
 * flashback (a story scene), the pizza box opens the terminal. attic_hud.c draws between scenes. */
#if !defined(KH_TEST_WORLD) && !defined(KH_TEST_FIGHT)
#include "attic.h"
#include "attic_int.h"
#include "attic_env.h"
#include "terminal.h"
#include "story.h"
#include "player.h"
#include "actor.h"
#include "world.h"
#include "fight.h"
#include "audio.h"
#include "hud_vn.h"
#include "script.h"
#include <string.h>

static enum { A_DEEP, A_WAKE, A_ROOM, A_TERMINAL, A_LEAVING } phase;
static float phase_t;
static int scene = -1, scene_arg;            /* the story scene attic.c is waiting on, and its clearing */
static bool was_won, deep_intro;
static v3 ret_pos; static float ret_yaw;
static unsigned found_mask, games_mask;
static const char *toast; static float toast_t;

void attic_init(t3_scene *scene_, t3_camera *cam) {
  static bool done; if (done) return; done = true;
  (void)cam;
  attic_entrance_build();
  attic_env_init(scene_);
  term_init();
}
bool attic_won(void) { return was_won; }
/* the pause menu's "climb back down": not in the middle of a scene or the terminal */
bool attic_can_leave(void) { return scene < 0 && (phase == A_DEEP || phase == A_ROOM); }
void attic_leave_now(void) { if (attic_can_leave()) { phase = A_LEAVING; phase_t = 0; } }

static void to_room(float normal) {
  attic_env_set_arena(v3_(0, 0, 0), 4.f);
  attic_env_layout(ATTIC_LAYOUT_ROOM);
  attic_env_reset(normal);
  world_set_stage(STAGE_ATTIC);
}
static void play(int sc, int arg) { scene = sc; scene_arg = arg; story_scene_play(sc); }

void attic_enter(void) {
  ret_pos = player_pos(); ret_yaw = player_yaw();
  if (player_actor()) actor_visible(player_actor(), true);   /* whoever climbs is on screen up there */
  was_won = false; scene = -1; toast_t = 0;
  { uint16_t f, g; story_attic_masks(&f, &g); found_mask = f; games_mask = g; }
  if (story_attic_beaten()) {
    to_room(1); hub_begin(); phase = A_ROOM; music_mood(MOOD_NIGHT);
  } else {
    attic_env_layout(ATTIC_LAYOUT_DEEP); world_set_stage(STAGE_ATTIC);
    deep_begin(); phase = A_DEEP; music_mood(MOOD_TENSION);
    deep_intro = true;
  }
  phase_t = 0;
  sfx(SFX_STEP, 0, .4f);
}

static void leave(void) {
  story_attic_set_masks((uint16_t)found_mask, (uint16_t)games_mask);
  attic_env_set_arena(v3_(0, 0, 0), 0);
  world_set_stage(STAGE_TOWN);
  player_teleport(ret_pos, ret_yaw);
  player_camera_snap();
  music_mood(MOOD_HOSTING);
}

/* a story scene just ended: what it means for the attic */
static void scene_done(int sc) {
  int bit = hub_scene_bit(sc);
  if (sc >= SC_ENC_HOLIDAY && sc <= SC_ENC_PARTS) {
    if (story_scene_fight_won()) deep_cleared(scene_arg); else deep_retreat();
    attic_env_set_arena(v3_(0, 0, 0), 0);
    music_mood(MOOD_TENSION);
  } else if (sc == SC_BOSS_SYSOP) {
    if (story_scene_fight_won()) { phase = A_WAKE; phase_t = 0; was_won = true; }
    else { deep_retreat(); attic_env_phase(0); }
  } else if (bit >= 0) {
    bool first = !(found_mask & (1u << bit));
    found_mask |= 1u << bit;
    story_attic_set_masks((uint16_t)found_mask, (uint16_t)games_mask);
    to_room(1); hub_return(); music_mood(MOOD_NIGHT);
    toast = hub_found_title(sc); toast_t = 3.2f;
    if (first && hub_found_count(found_mask) == HUB_MEMORIES) play(SC_ATTIC_ALL, 0);
  }
}

bool attic_update(float dt, const intent_t *in) {
  phase_t += dt;
  if (toast_t > 0) toast_t -= dt;
  /* a story scene plays: the story runs the frame, the attic waits for it to end */
  if (scene >= 0) {
    if (story_scene_busy()) { story_update(dt, in); return true; }
    int sc = scene; scene = -1; scene_done(sc);
    return true;
  }
  story_attic_tick(dt);
#ifdef KH_ATTIC_PATH
  /* test build only: number keys jump ahead inside the attic (1 the first clearing, 2 the boss, 3 the next shelf) */
  if (in->weapon_pick == 0 && phase == A_DEEP) { float r; const char *z; v3 c = attic_env_clearing(0, &r, &z); player_teleport(v3_(c.x, 0, c.z + r - 1.5f), DS_PI); player_camera_snap(); }
  if (in->weapon_pick == 1 && phase == A_DEEP) { float r; v3 b = attic_env_boss(&r); player_teleport(v3_(b.x, 0, b.z + r - 2.0f), DS_PI); player_camera_snap(); }
  if (in->weapon_pick == 3 && phase == A_DEEP) { phase = A_WAKE; phase_t = 0; was_won = true; }
  if (in->weapon_pick == 2 && phase == A_ROOM) { static int k; extern void hub_test_jump(int); hub_test_jump(k++); }
#endif
#if defined(KH_ATTIC_PATH) && defined(KH_AUTO)
  /* test build only: walk the whole attic by itself. Each clearing, the boss, then every flashback and the terminal */
  { static int step_n; static float wait;
    wait += dt;
    if (phase == A_DEEP && !deep_intro && wait > 1.f) {
      wait = 0; int nc = attic_env_clearing_count();
      if (step_n < nc) { float r; const char *z; v3 c = attic_env_clearing(step_n, &r, &z); player_teleport(v3_(c.x, 0, c.z), DS_PI); ds_log("[attic auto] clearing %d", step_n); }
      else { float r; v3 b = attic_env_boss(&r); player_teleport(v3_(b.x, 0, b.z), DS_PI); ds_log("[attic auto] boss"); }
      step_n++;
    }
    if (phase == A_ROOM && wait > 1.f) {
      wait = 0; static int m;
      static const int MEM[HUB_MEMORIES] = { SC_MEM_BALLOON, SC_MEM_LASER, SC_MEM_GLITTER, SC_MEM_MOVE, SC_MEM_HAXMAS, SC_MEM_ARDUINO, SC_MEM_ROBOTS, SC_MEM_TEARAPART };
      if (m < HUB_MEMORIES) { ds_log("[attic auto] memory %d", MEM[m]); play(MEM[m++], 0); return true; }
      if (m == HUB_MEMORIES) { m++; term_open(games_mask); phase = A_TERMINAL; ds_log("[attic auto] terminal open, memories %d", hub_found_count(found_mask)); }
    }
    if (phase == A_TERMINAL && wait > 3.f) { ds_log("[attic auto] done"); phase = A_ROOM; wait = -1e9f; }
  }
#endif
  switch (phase) {
    case A_DEEP: {
      if (deep_intro) { deep_intro = false; play(SC_DEEP_ENTER, 0); return true; }
      int arg = -1, ev = deep_update(dt, in, &arg);
      if (ev == DEEP_ENCOUNTER) play(deep_scene_for(arg), arg);
      else if (ev == DEEP_BOSS) play(SC_BOSS_SYSOP, 0);
      if (in->back && !in->pause && phase_t > .5f) { phase = A_LEAVING; phase_t = 0; }   /* Backspace or B; Esc is the pause menu */
      break;
    }
    case A_WAKE:
      /* the nightmare drops away: black, then the room as it really is, warming up */
      if (phase_t > .8f && phase_t - dt <= .8f) { attic_env_phase(0); to_room(0); attic_env_target(1); hub_begin(); story_attic_result(true); }
      if (phase_t > 1.6f) { phase = A_ROOM; phase_t = 0; music_mood(MOOD_NIGHT); play(SC_ATTIC_BACK, 0); }
      break;
    case A_ROOM: {
      int arg = -1, ev = hub_update(dt, in, &arg);
      if (ev == HUB_MEMORY) play(arg, 0);
      else if (ev == HUB_TERMINAL) { term_open(games_mask); phase = A_TERMINAL; }
      else if (ev == HUB_LEAVE) { phase = A_LEAVING; phase_t = 0; }
      break;
    }
    case A_TERMINAL:
      if (!term_update(dt, in)) { games_mask = term_unlocked(); story_attic_set_masks((uint16_t)found_mask, (uint16_t)games_mask); phase = A_ROOM; }
      break;
    case A_LEAVING:
      if (phase_t > .5f) { leave(); return false; }
      break;
  }
  return true;
}

void attic_draw(void) {
  if (scene >= 0 && story_scene_busy()) {
    if (story_state() == SR_FIGHT && !STORY_UI.retry) fight_draw_hud();
    hud_vn_draw();
    return;
  }
  if (phase == A_TERMINAL) { term_draw(); return; }
  bool deep = phase == A_DEEP;
  attic_hud_t h = { 0 };
  h.deep = deep;
  h.title = deep ? "THE ATTIC?" : "THE ATTIC";
  h.sub = deep ? deep_zone() : "AS IT REALLY IS";
  h.objective = deep ? (deep_all_cleared() ? "The rack is at the back.  {PAUSE} menu" : "Find the rack. Stay off the painted boards.  {PAUSE} menu")
                     : "Look around. The rack is still on.";
  h.prompt = deep ? NULL : hub_prompt();
  h.found = hub_found_count(found_mask); h.falls = deep_falls();
  h.toast = toast; h.toast_t = toast_t; h.toast_sub = "A MEMORY";
  h.fade = deep ? deep_fade() : 0;
  if (phase == A_WAKE) h.fade = phase_t < .8f ? phase_t / .8f : 1 - (phase_t - .8f) / .8f;
  if (phase == A_LEAVING) h.fade = phase_t / .5f;
  attic_hud_draw(&h);
}
#endif
