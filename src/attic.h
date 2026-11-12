/* attic.h: the attic. The back stairs are roped off with a PVC danger sign; going up warps the player
 * into a purpose-built attic STAGE (STAGE_ATTIC, its own roomy geometry, lighting and collision, not the
 * real crawlspace mesh). First visit is a minigame: cross the soft floor (red X marks the weak boards)
 * through the junk to the server rack and reboot it, and the glitched room snaps back to normal. After
 * that it is a hub you can re-enter any time: replay the run, examine old objects (each plays a memory),
 * and use the rack's pizza-box terminal (hacking minigames, terminal.c). Self contained like bike.c.
 *
 * The environment half (attic_env_*) is the STAGE_ATTIC stage, routed to here by world.c (every non-town
 * stage is a flat floor at y=0 with collision/raycast/mood through these). */
#ifndef KH_ATTIC_H
#define KH_ATTIC_H
#include "ds.h"
#include "input.h"
#include "three.h"
#include "arena.h"   /* arena_mood */

/* the mode (game.c G_ATTIC) */
void attic_init(t3_scene *scene, t3_camera *cam);  /* builds the sign, the hidden attic env and props, once */
void attic_enter(void);                            /* from the stairs: remember where you stood, warp into the stage */
bool attic_update(float dt, const intent_t *in);   /* false when you come back down */
void attic_draw(void);                             /* the minigame / hub / memory HUD, after the scene */
bool attic_won(void);                              /* the last visit rebooted the rack (first time only) */

/* the STAGE_ATTIC environment, called by world.c (never by arena.c: idx() would run off its 5 tables) */
void attic_env_show(bool on);                      /* build on first use, then show/hide the room */
void attic_env_floor(v3 *center, float *radius);
void attic_env_push(v3 *p, float r, float h, v3 *n, bool *hit);
bool attic_env_raycast(v3 o, v3 d, float max_dist, float *t, v3 *n);
void attic_env_mood(arena_mood *m);                /* glitched (0) to normal (1), by the reboot */
bool attic_env_update(float dt);                   /* true when the mood changed (world repaints the lights) */
#endif
