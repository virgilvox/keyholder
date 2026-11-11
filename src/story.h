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
  float letterbox, fade, flash, dim;
  float skip;
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
} story_ui_t;
extern story_ui_t STORY_UI;

void story_init(t3_scene *scene, t3_camera *cam);
void story_new_game(int playable);       /* 0 Vee, 1 Dex, 2 Ruth */
bool story_has_save(void);
void story_continue(void);
void story_stop(void);                   /* off the stage, back to the title */
void story_update(float dt, const intent_t *in);
story_run story_state(void);
const char *story_player_name(void);
/* the epilogue's text */
const char *story_end_title(void);
int story_end_lines(const char **out, int cap);
#endif
