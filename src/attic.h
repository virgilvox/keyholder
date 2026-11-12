/* attic.h: the attic minigame. The stairs at the back of the lab are roped off with a danger sign
 * (a member warns you not to go up alone). Going up anyway drops you into the real attic over the back
 * of the lab, where the wooden floor is soft in places, marked with red X tape. You pick a path across
 * the joists to the little server rack in the back-left corner (the members site, the wiki and the door
 * controller live on it) and reboot it. Step on a soft board and you put a boot through the ceiling.
 *
 * Self contained like bike.c and battle.c: it reuses the player's actor, the lab's own attic geometry
 * and the follow camera, and lays its hazards and the rack on top. game.c drives it as its own mode. */
#ifndef KH_ATTIC_H
#define KH_ATTIC_H
#include "ds.h"
#include "input.h"
#include "three.h"

void attic_init(t3_scene *scene, t3_camera *cam);  /* builds the stair sign and the (hidden) attic props once */
void attic_enter(void);                            /* from the stairs: remember where you stood, warp up */
bool attic_update(float dt, const intent_t *in);   /* false when you come back down (won or not) */
void attic_draw(void);                             /* the minigame HUD, after the scene */
bool attic_won(void);                              /* the last run rebooted the rack */
#endif
