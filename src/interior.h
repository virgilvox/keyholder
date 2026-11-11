/* interior.h: the downtown buildings you can walk into (besides the lab). A data table: each interior
 * has a name, a door on its real facade, a room built behind it, and named spots inside (seats, a
 * counter, a stage) that the story and the street people use through world_place.
 * The town owns the rooms (interior.c); the roam glue (prompts, entering, leaving) lives in street.c. */
#ifndef KH_INTERIOR_H
#define KH_INTERIOR_H
#include "ds.h"
#include "three.h"

void interior_init(t3_scene *scene);
int interior_count(void);
const char *interior_name(int i);                 /* the sign over the door: "Proof Bread" */
/* the door: where you stand outside to go in (facing the door) and where you appear inside */
bool interior_door(int i, v3 *outside, float *yaw_out, v3 *inside, float *yaw_in);
void interior_enter(int i);                       /* builds it on first use, shows it */
void interior_exit(void);
int interior_current(void);                       /* -1 outdoors (or in the lab) */
bool interior_contains(v3 p);                     /* inside the current room: the camera keeps under its ceiling */
float interior_ceiling(void);                     /* the current room's ceiling height above its floor */
#endif
