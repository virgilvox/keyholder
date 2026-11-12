/* attic_env.h: the STAGE_ATTIC stage itself, in three layouts: the blueprint room, the nightmare attic of the
 * first visit, and the desert set of the balloon launch flashback. Their geometry, collision and raycast, the
 * soft boards and anchors, and the mood. world.c routes the stage's floor/collision/raycast/mood to these;
 * attic.c (the gameplay) and story.c build their play on top and never touch the layouts' internals.
 * Compiled in every build (unlike attic.c), so world.c always links. */
#ifndef KH_ATTIC_ENV_H
#define KH_ATTIC_ENV_H
#include "ds.h"
#include "three.h"
#include "arena.h"   /* arena_mood */

/* the layouts the stage can show. Collision, raycast, soft boards, anchors and mood follow the current one */
enum { ATTIC_LAYOUT_ROOM, ATTIC_LAYOUT_DEEP, ATTIC_LAYOUT_DESERT, ATTIC_LAYOUT_COUNT };

/* the blueprint room's anchors (attic_env_entry and the others below follow the current layout) */
extern const v3 ATTIC_ENTRY;   /* the stair head you arrive at */
extern const v3 ATTIC_RACK;    /* the server rack (and its terminal) */
extern const v3 ATTIC_TERM;    /* where you stand to use the pizza-box terminal */

void attic_env_init(t3_scene *scene);              /* build the room once */
/* show this layout from now on (the room by default). Loads its model the first time. Resets the phase, the
 * arena and the balloon. If the stage is already up, the sky and lights repaint at once */
void attic_env_layout(int layout);
int attic_env_get_layout(void);

bool attic_env_soft(v3 p);                         /* is this point on a soft board (a red X)? */
int attic_env_soft_at(v3 p);                       /* the soft board under p: the landing a fall drops to, else -1 */
void attic_env_reset(float normal);                /* the room, on entering: 0 glitched, 1 normal */
void attic_env_target(float normal);               /* ease the room toward this (1 on the reboot) */
float attic_env_level(void);                       /* the room's current 0..1 glitched..normal */

/* anchors of the current layout (yaw 0 faces +z; pass NULL for what you do not need) */
v3 attic_env_entry(float *yaw);                    /* where you arrive */
v3 attic_env_rack(void);                           /* the rack (the room and the nightmare) */
v3 attic_env_term(float *yaw);                     /* the stand point in front of the rack's terminal */
int attic_env_landing_count(void);                 /* the nightmare's landings, the checkpoints, in route order */
v3 attic_env_landing(int i, float *yaw);           /* (0 is the stair head; the room has only that one) */
int attic_env_clearing_count(void);                /* the nightmare's fight clearings, in route order */
v3 attic_env_clearing(int i, float *radius, const char **zone);   /* zone: "holiday" "members" "robotics" "metal" */
v3 attic_env_boss(float *radius);                  /* the boss chamber's ring (its phase two radius: attic_env_boss2) */
float attic_env_boss2(void);
/* named spots of the current layout: the desert's "entry" "truck" "tailgate" "table" "laptop" "radio" "tanks"
 * "payload" "fill" "launch" "stand_a".."stand_e" "cam_wide" "cam_low" "road"; the nightmare's "entry" "rack"
 * "term" "boss"; the room's "entry" "rack" "term". pos.y is the height of the spot (a table top) */
bool attic_env_anchor(const char *name, v3 *pos, float *yaw);

/* fights: the floor the fight system rings itself with (attic_env_floor answers this while set; radius <= 0
 * clears it, and the floor falls back to the clearing nearest the focus) */
void attic_env_set_arena(v3 center, float radius);
/* the boss chamber's phase two (0 or 1): over a second and a half the towers tip in over the ring, what was on
 * them falls into the margin (solid past halfway; the ring is then attic_env_boss2), the lights turn red */
void attic_env_phase(int phase);
int attic_env_get_phase(void);
float attic_env_phase_blend(void);
/* where the player is, each frame: far chunks of the nightmare hide, and the floor falls back to the nearest
 * clearing. Optional (nothing hides until it is called) */
void attic_env_focus(v3 p);

/* the desert's balloon: 0 laid out on its tarp, 1 filling at the tanks (about 3 s), 2 let go (it rises and
 * drifts off, the payload on its line below, until it is out of sight), -1 not there at all (the payload stays
 * on the table). Hidden in the other layouts */
void attic_env_balloon(int mode);
int attic_env_balloon_mode(void);
float attic_env_balloon_height(void);              /* the neck's height above the fill spot, metres */

/* called by world.c for STAGE_ATTIC: its floor, collision, raycast and mood */
void attic_env_show(bool on);
void attic_env_floor(v3 *center, float *radius);
void attic_env_push(v3 *p, float r, float h, v3 *n, bool *hit);
bool attic_env_raycast(v3 o, v3 d, float max_dist, float *t, v3 *n);
void attic_env_mood(arena_mood *m);
bool attic_env_update(float dt);                   /* true when the mood changed (world repaints) */
#endif
