/* attic_env.h: the STAGE_ATTIC room itself. Its geometry, its collision and raycast, the soft-floor
 * hazard grid, and the glitched-to-normal lighting mood. world.c routes the stage's floor/collision/
 * raycast/mood to these; attic.c (the gameplay) builds its play on top and never touches the room's
 * internals. Compiled in every build (unlike attic.c), so world.c always links. */
#ifndef KH_ATTIC_ENV_H
#define KH_ATTIC_ENV_H
#include "ds.h"
#include "three.h"
#include "arena.h"   /* arena_mood */

/* the room's anchor points, for the gameplay to place the player and things */
extern const v3 ATTIC_ENTRY;   /* the stair head you arrive at */
extern const v3 ATTIC_RACK;    /* the server rack (and its terminal) */

void attic_env_init(t3_scene *scene);              /* build the room once */
bool attic_env_soft(v3 p);                         /* is this point on a soft board (a red X)? */
void attic_env_reset(float normal);                /* on entering: 0 glitched, 1 normal */
void attic_env_target(float normal);               /* ease the room toward this (1 on the reboot) */
float attic_env_level(void);                       /* the current 0..1 glitched..normal */

/* called by world.c for STAGE_ATTIC: a flat floor at y=0, its own collision/raycast/mood */
void attic_env_show(bool on);
void attic_env_floor(v3 *center, float *radius);
void attic_env_push(v3 *p, float r, float h, v3 *n, bool *hit);
bool attic_env_raycast(v3 o, v3 d, float max_dist, float *t, v3 *n);
void attic_env_mood(arena_mood *m);
bool attic_env_update(float dt);                   /* true when the mood changed (world repaints) */
#endif
