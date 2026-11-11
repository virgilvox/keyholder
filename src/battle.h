/* battle.h: the Battle Zone, a replayable ladder of the day's fights with ranks, scores and modifiers.
 * Progress lives in DS_SAVE.data bytes 520..1019 (the day's save stays below 520). */
#ifndef KH_BATTLE_H
#define KH_BATTLE_H
#include "ds.h"
#include "input.h"
#include "three.h"
void battle_unlock_fight(int fi);     /* FI_ id: won in the story */
void battle_unlock_weapon(int w);     /* weapon_id: given in the story */
bool battle_available(void);          /* anything unlocked: the title shows the entry */
void battle_init(t3_scene *scene, t3_camera *cam);
void battle_enter(void);              /* from the title */
bool battle_update(float dt, const intent_t *in);   /* false when the player backs out to the title */
void battle_draw(void);               /* after the scene: menus, results */
#endif
