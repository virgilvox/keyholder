/* story.h: runs the day. A little virtual machine over gen/script.c: dialogue with auto coverage,
 * choices, walks, fights, free roam with hosting tasks. game.c calls story_update each frame; hud_vn.c
 * draws from STORY_UI. */
#ifndef KH_STORY_H
#define KH_STORY_H
#include "ds.h"
#include "input.h"
#include "three.h"
#ifdef KH_GEN_ENUM
#include KH_GEN_ENUM
#else
#include "gen/script_enum.h"
#endif

typedef enum { SR_RUNNING, SR_ROAM, SR_FIGHT, SR_EPILOGUE, SR_DONE } story_run;
/* the hosting minigame on screen */
enum { MG_NONE, MG_GREET, MG_POINT, MG_CARRY, MG_TIMING, MG_MASH, MG_HOLD, MG_STICK, MG_SIGNIN };
enum { UT_OPEN = 1, UT_ACTIVE, UT_DONE };
#define UI_TASKS 8
#define UI_PINS 6
typedef struct { const char *label; int state; bool optional, target; float age; } ui_task;
typedef struct {
  /* dialogue */
  bool talking; const char *speaker; uint32_t speaker_col; const char *line; float line_t, line_cps; bool line_done;
  /* choice */
  bool choosing; const char *prompt; int nopt, sel; const char *opt[5]; float choose_t;
  /* cards */
  bool chapter; const char *ch_title, *ch_sub; float ch_t, ch_dur, ch_hour;
  bool card; const char *card_title, *card_sub; float card_t, card_dur;
  const char *note; float note_t;
  const char *objective;
  /* toast: a stat moved (two at once for a task's reward) */
  const char *toast; int toast_n; float toast_t;
  const char *toast2; int toast2_n;
  const char *got; float got_t;
  /* play */
  bool roam, cine;
  float hour;
  int stats[ST_COUNT];
  const char *prompt_use;
  bool marker_on, marker_behind, marker_task; float marker_x, marker_y, marker_dist;
  const char *marker_label; bool marker_corner;   /* v1.3: what it points at; aiming at a corner of the route */
  float letterbox, fade, flash, dim;
  float skip;
  bool skip_hint;     /* v1.4: a skippable stretch (the orientation tutorial): show "hold START to skip" */
  /* retry */
  bool retry; int retry_sel; bool retry_allow_skip;
  /* hosting: the task list, grey pins on the other open tasks */
  int ntask; ui_task task[UI_TASKS];
  int npin; float pin_x[UI_PINS], pin_y[UI_PINS]; bool pin_behind[UI_PINS];
  /* the minigame: mg is MG_*; v is progress 0..1 */
  int mg; const char *mg_title; float mg_t, mg_v, mg_needle, mg_z0, mg_z1, mg_shake, mg_flash, mg_angle, mg_window;
  int mg_hits, mg_need, mg_step, mg_nseq; const char *mg_seq[4];
  bool mg_anchor, mg_anchor_behind, mg_aim; float mg_ax, mg_ay;   /* a world point on screen: the visitor, the station */
  int mg_trip, mg_trips; bool mg_carrying;
  /* banter and barks: a small subtitle, no letterbox */
  const char *sub_name, *sub_text; uint32_t sub_col; float sub_t, sub_dur;
  /* v1.2 roam gear: the always-on hints (gear_wp NULL when no weapon is owned; gear_bike when it can be ridden here) */
  const char *gear_wp; bool gear_drawn, gear_bike, gear_riding;
} story_ui_t;
extern story_ui_t STORY_UI;

void story_init(t3_scene *scene, t3_camera *cam);
void story_new_game(int playable);       /* 0 Vee, 1 Dex, 2 Ruth */
bool story_has_save(void);
/* v2.1: the pause menu's save: only during a free walk (a roam), where Continue can put you back */
bool story_can_save(void);
bool story_save_here(void);
void story_continue(void);
void story_stop(void);                   /* off the stage, back to the title */
void story_update(float dt, const intent_t *in);
story_run story_state(void);
/* v1.4: the attic. During a roam, stepping up to the roped-off stairs offers the climb; game.c polls
 * story_take_attic (clears the request), runs the minigame, then reports the outcome back so the day
 * gets the toast and the stat. */
bool story_take_attic(void);
void story_attic_result(bool rebooted);
/* v1.5: the attic hub's persisted progress (saved in save_t's slack, global across the day) */
bool story_attic_beaten(void);
void story_attic_masks(uint16_t *found, uint16_t *games);
void story_attic_set_masks(uint16_t found, uint16_t games);
/* v2.0: the attic's scenes (tools/script/attic.mjs, by SC_*), played through the story VM from attic.c. The
 * day's place (where the script is, the roam and its tasks, the clock, the HUD, where everyone stands) is kept
 * and put back when the scene ends; flags it sets stay set. While story_scene_busy, game.c runs story_update and
 * draws the story HUD as in the day. attic.c puts the player and the attic's stage back after. */
bool story_scene_play(int scene);
bool story_scene_busy(void);
bool story_scene_fight_won(void);
void story_scene_abort(void);       /* the pause menu quits to the title mid scene */   /* the last scene had a fight and it was won */
/* the attic between scenes: keeps the town's people out of sight up there and eases the flashback grade */
void story_attic_tick(float dt);
const char *story_player_name(void);
/* the epilogue's text */
/* v1.3: the nights. The epilogue's screen done, story_resume goes on into the next night (false: the story is
 * over); story_more says whether there is one. The title's night select: story_nights_open is a mask (bit n-1
 * for night n) of the nights the save can start, story_start_night(n) starts one. */
bool story_resume(void);
bool story_more(void);
int story_nights_open(void);
int story_night_count(void);
const char *story_night_title(int n);
void story_start_night(int n);
const char *story_end_title(void);
int story_end_lines(const char **out, int cap);
#endif
