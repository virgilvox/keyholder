/* director.h: the virtual cinematographer, after DOLLY's director.js. A shot names a type and its
 * subjects; the solver samples candidate camera positions, scores them for clear sightlines, the 180
 * degree rule and cut variety, and then a per frame rig keeps the subjects framed with drift, handheld
 * noise and a boom that never lets the camera sit inside geometry. */
#ifndef KH_DIRECTOR_H
#define KH_DIRECTOR_H
#include "ds.h"
#include "actor.h"
#include "three.h"
typedef enum { SH_AUTO, SH_FIXED, SH_WIDE, SH_ESTABLISH, SH_TWO, SH_OTS, SH_MCU, SH_CU, SH_LOW, SH_DUTCH, SH_ORBIT, SH_CRANE, SH_TRACK } sh_type;
typedef struct {
  sh_type type;
  actor_t *subj[3]; int nsubj;
  float dist, height, fov, dur;
  v3 fpos, flook;     /* SH_FIXED */
} dir_spec;
void director_init(t3_camera *cam);
/* who may get in the way of a shot; the solver keeps them out of the frame */
void director_set_occluders(actor_t *const *list, int n);
void director_cut(const dir_spec *s);
void director_update(float dt);
bool director_active(void);
void director_stop(void);
#endif
