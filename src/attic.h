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

/* the gameplay mode (game.c G_ATTIC). The STAGE_ATTIC room itself lives in attic_env.h/.c. */
void attic_init(t3_scene *scene, t3_camera *cam);  /* builds the sign, the attic room and props, once */
void attic_enter(void);                            /* from the stairs: remember where you stood, warp into the stage */
bool attic_update(float dt, const intent_t *in);   /* false when you come back down */
void attic_draw(void);                             /* the minigame / hub / memory HUD, after the scene */
bool attic_won(void);                              /* the last visit rebooted the rack (first time only) */
bool attic_can_leave(void);                        /* the pause menu may offer "climb back down" now */
void attic_leave_now(void);                        /* walk back down to the lab (attic_update then returns false) */
#endif
