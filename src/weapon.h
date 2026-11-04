/* weapon.h: the five weapon props, their swoosh ribbons and summon flash. weapon_equip and
 * weapon_draw_flash are in fight.h for the story's use. */
#ifndef KH_WEAPON_H
#define KH_WEAPON_H
#include "fight.h"

void weapon_init(t3_scene *scene);              /* fight_init calls it */
void weapon_update(float dt);                   /* after the actors moved: extends and ages the ribbons */
void weapon_trail(actor_t *a, bool on);         /* the swoosh of whatever it holds */
int weapon_of(actor_t *a);                      /* what it holds, or -1 */
/* the blade as a segment in the world: hand 0 right, 1 left. False when nothing is there. */
bool weapon_points(actor_t *a, int hand, v3 *base, v3 *tip);
uint32_t weapon_color(int w);                   /* the weapon's glow colour */
#endif
