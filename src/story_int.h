/* story_int.h: between story.c (the VM and the day) and the modules that work beside it. Nothing else
 * includes this; the public story API is story.h. */
#ifndef KH_STORY_INT_H
#define KH_STORY_INT_H
#include "story.h"

/* story.c: the day's place, held while an attic scene plays and given back after. hold keeps where the script
 * is, the roam and its tasks, the clock, the HUD and where everyone stands, puts today's people out of sight,
 * and jumps the VM to start. release puts it all back (but the player: attic.c places them). */
void story_day_hold(int start);
void story_day_release(void);
void story_extras_tick(float dt);              /* the background people (out of sight off the town stage) */

/* story_scene.c: the VM's beats for the attic's scenes */
void story_scene_end(void);                    /* OP_SCENE_END */
void story_scene_grade(bool memory, float t);  /* OP_GRADE: the flashback look in (or out) over t seconds */
void story_scene_tick(float dt);               /* every frame: the grade eases */
void story_scene_won(void);                    /* a fight in the playing scene was won */
#endif
