/* player.h: the third person walk through town and lab: stick relative to the camera,
 * walk, jog and sprint with foot-speed-matched clips, jump, wall-aware follow camera. */
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
void player_camera_snap(void);                     /* put the camera behind the player now */
void player_camera_from(v3 cam_pos);               /* continue from where the camera stands (a fight handing it back) */
v3 player_pos(void);
float player_yaw(void);
#endif
