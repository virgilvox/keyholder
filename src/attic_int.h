/* attic_int.h: between the attic's modules. attic.c is the mode (what phase you are in, scenes, coming and
 * going); attic_deep.c walks the nightmare (soft boards, landings, where the fights start); attic_hub.c is the
 * room come back to itself (things to examine, the terminal, the stairs down); attic_hud.c draws; and
 * attic_entrance.c is the roped off stair in the lab. The stage itself is attic_env.c. */
#ifndef KH_ATTIC_INT_H
#define KH_ATTIC_INT_H
#include "ds.h"
#include "input.h"

void attic_entrance_build(void);

/* the nightmare run */
enum { DEEP_NONE, DEEP_ENCOUNTER, DEEP_BOSS };
void deep_begin(void);                                   /* the stair head, the run from the start */
int deep_update(float dt, const intent_t *in, int *arg); /* an event; for DEEP_ENCOUNTER, arg is the clearing */
void deep_cleared(int clearing);
void deep_retreat(void);                                 /* back to the last landing (a fight given up) */
bool deep_all_cleared(void);
float deep_fade(void);                                   /* 0..1 black while a board gives way */
int deep_falls(void);
const char *deep_zone(void);                             /* the zone you are in, for the HUD */
int deep_scene_for(int clearing);                        /* the SC_* encounter scene of a clearing */

/* the room */
enum { HUB_NONE, HUB_MEMORY, HUB_TERMINAL, HUB_LEAVE };
void hub_begin(void);                                    /* at the stair head */
int hub_update(float dt, const intent_t *in, int *arg);  /* for HUB_MEMORY, arg is the SC_* scene */
void hub_return(void);                                   /* back where you stood when the memory started */
const char *hub_prompt(void);                            /* "EXAMINE THE PARTS SHELF  {USE}" or NULL */
const char *hub_found_title(int scene);                  /* the memory's name, for the toast */
int hub_found_count(unsigned mask);
int hub_scene_bit(int scene);                            /* the found mask bit of a memory scene, or -1 */
#define HUB_MEMORIES 8

/* the HUD */
typedef struct { const char *title, *sub, *objective, *prompt, *toast, *toast_sub; float fade, toast_t; int found, falls; bool deep; } attic_hud_t;
void attic_hud_draw(const attic_hud_t *h);
#endif
