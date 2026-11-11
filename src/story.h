/* story.h: runs the day. A little virtual machine over gen/script.c: dialogue with auto coverage,
 * choices, walks, fights, free roam. game.c calls story_update each frame; hud_vn.c draws from STORY_UI. */
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
  /* toast: a stat moved */
  const char *toast; int toast_n; float toast_t;
  const char *got; float got_t;
  /* play */
  bool roam, cine;
  float hour;
  int stats[ST_COUNT];
  const char *prompt_use;
  bool marker_on, marker_behind; float marker_x, marker_y, marker_dist;
  float letterbox, fade, flash, dim;
  float skip;
  /* retry */
  bool retry; int retry_sel; bool retry_allow_skip;
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
