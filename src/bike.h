/* bike.h: the lab's loaner bike, a low-poly bicycle built from prop parts (fx.h), for riding through
 * downtown in a roam. One bike; player.c poses it while riding, the story parks and hides it. */
#ifndef KH_BIKE_H
#define KH_BIKE_H
#include "ds.h"
#include "three.h"

void bike_init(t3_scene *scene);     /* builds it hidden (once; it guards itself) */
void bike_show(bool on);
bool bike_shown(void);
/* where it stands (ground under the wheels' middle, yaw 0 faces +z), its roll toward the turn, the
 * front wheel's steer, and the wheels' and the cranks' turned angles in radians */
void bike_pose(v3 pos, float yaw, float lean, float steer, float wheel, float crank);
/* while someone rides it (player.c), the kickstand stays up whatever the lean; otherwise a pose with a lean
 * of .07 or more is parked, and the bike leans on its stand instead */
void bike_ridden(bool on);
v3 bike_pos(void);
v3 bike_grip(int side);              /* a grip in the world (0 right, 1 left), after bike_pose */
float bike_yaw(void);
/* the saddle's and the pedal's places in the bike's frame, for seating a rider */
#define BIKE_SEAT_Y .9f
#define BIKE_SEAT_Z -.24f
#define BIKE_WHEEL_R .34f
#endif
