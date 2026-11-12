/* enemy.h: the attic's nightmare foes (docs/ATTIC.md). Each is one of the game's own bodies made strange
 * (a palette wash, sometimes the head shrunk away, sometimes stop motion) with small vertex coloured props
 * (fx.h's fxp builder) on its sockets, so it fights with the usual clips and AI. fight.c dresses a pool
 * fighter whose fighter_def has .dress set, and undresses it at fight_end. */
#ifndef KH_ENEMY_H
#define KH_ENEMY_H
#include "actor.h"
#include "fight.h"

typedef enum {
  EK_NONE,              /* 0: an ordinary fighter */
  EK_MANNEQUIN,         /* holiday supplies: plaster white, a blank face, stop motion */
  EK_MANNEQUIN_SANTA,   /* the same in a Santa hat */
  EK_MANNEQUIN_LIGHTS,  /* the same wound in a string of glowing coloured lights */
  EK_UNCLAIMED,         /* old member storage: grey plastic, a tote for a head, a red tag, a cardboard plate */
  EK_JUNKBOT,           /* robotics: metal grey, a box head with LED eyes and an antenna, C-channel shoulders */
  EK_SYSOP,             /* the boss: dark, a CRT for a head with a green face, patch cables, a glowing sash */
  EK_COUNT
} enemy_kind;

/* the kind's props onto any actor's sockets (a pool fighter or a story actor), with its size, its gait
 * and, if the actor's palette has no wash, the kind's wash (an actor dressed already is undressed first;
 * EK_NONE only undresses). Needs fx_init (fight_init calls it). A story actor stays dressed through
 * fight_end, which only undresses pool fighters. */
void enemy_dress(actor_t *a, int kind);
/* the props off and freed; the head, size, gait and palette as they were. Safe on an actor never
 * dressed. actor_free does it on its own (without restoring), so a freed body leaves nothing behind */
void enemy_undress(actor_t *a);
int enemy_kind_of(const actor_t *a);   /* EK_NONE when not dressed */

/* a suggested fighter for the kind: body, palette with its wash, scale, name, weapon, AI, hp, power,
 * speed, and .dress set. place_x and place_z are 0: the caller places it */
fighter_def enemy_def(int kind);
#endif
