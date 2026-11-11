/* player.h: the third person walk through town and lab: stick relative to the camera,
 * walk, jog and sprint with foot-speed-matched clips, jump, wall-aware follow camera, the bike, and
 * swings of a drawn weapon. */
#ifndef KH_PLAYER_H
#define KH_PLAYER_H
#include "ds.h"
#include "actor.h"
#include "input.h"
#include "three.h"
void player_init(t3_camera *cam);
void player_set_actor(actor_t *a);
actor_t *player_actor(void);
void player_teleport(v3 pos, float yaw);
void player_update(float dt, const intent_t *in);  /* moves the actor and the camera */
float player_cam_zoom(void);                      /* 1 is the default arm */
void player_zoom(int wheel, bool step);            /* wheel notches, or a step through near, mid, far */
void player_camera_snap(void);                     /* put the camera behind the player now */
void player_camera_from(v3 cam_pos);               /* continue from where the camera stands (a fight handing it back) */
v3 player_pos(void);
float player_yaw(void);
/* v1.2: the bike (bike.h). Riding is faster than a sprint with wider turns; it ends by itself at a
 * building's door (the bike stays parked outside). player_ride(false) gets off and parks it. */
void player_ride(bool on);
bool player_riding(void);
/* v1.2: a swing of whatever the actor holds, at nothing (a roam): heavy or light, chained if pressed
 * late in the last one. Does nothing while riding or with no weapon out. */
void player_swing(bool heavy);
bool player_swinging(void);
#endif
