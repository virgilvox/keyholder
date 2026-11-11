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

/* added by the town (world.c uses these; nothing else needs them) */
const char *interior_id(int i);                   /* the place id of its facade: "proof", "chupacabra", "level1" */
int interior_find(const char *id);                /* by that id, or -1 */
/* the named spots inside, for world_place: "proof_counter", "level1_bar", ... Answered whether or not the
 * room has been built. pos.y is the floor (or the stage, or a seat's top where the name says seat) */
bool interior_place(const char *name, v3 *pos, float *yaw);
int interior_spot_count(int i);
const char *interior_spot_name(int i, int k);
/* the current room's walls and furniture, for world_move, world_floor_y and world_raycast. Each answers
 * only for a point (a ray's origin) inside the current room, and then the city is not asked */
bool interior_push(v3 *p, float r, float h, v3 *normal, bool *hit);
bool interior_floor(v3 p, float up, float *y);
bool interior_raycast(v3 o, v3 d, float max_dist, float *t, v3 *normal);
void interior_set_time(float night);              /* the daylight in the windows */
int interior_tris(void);                          /* the current room's triangles (the debug counts) */
void interior_attach(t3_object *parent);          /* hang the rooms under the town group (world_init) */
#endif
