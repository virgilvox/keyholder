/* director.h: the virtual cinematographer, after DOLLY's director.js. A shot names a type and its
 * subjects; the solver samples candidate camera positions, scores them for clear sightlines, the 180
 * degree rule and cut variety, and then a per frame rig keeps the subjects framed with drift, handheld
 * noise and a boom that never lets the camera sit inside geometry. */
#ifndef KH_DIRECTOR_H
#define KH_DIRECTOR_H
#include "ds.h"
#include "actor.h"
#include "three.h"
typedef enum { SH_AUTO, SH_FIXED, SH_WIDE, SH_ESTABLISH, SH_TWO, SH_OTS, SH_MCU, SH_CU, SH_LOW, SH_DUTCH, SH_ORBIT, SH_CRANE, SH_TRACK,
  /* v1.1 film language */
  SH_PUSH,     /* a slow dolly in on subj[0] over dur (reveals, emotional beats) */
  SH_REACT,    /* a reaction close up on subj[0] listening to subj[1] */
  SH_INSERT,   /* hands and props: part picks subj[0]'s right or left hand, or the point fpos */
  SH_HERO,     /* a low hero angle looking up at subj[0], wider than SH_LOW */
  SH_WHIP,     /* a whip pan from the current framing to subj[0] with motion blur streaks (into fights) */
  SH_TOP       /* straight down overhead, for a map like beat */
} sh_type;
typedef enum { PART_HEAD, PART_RHAND, PART_LHAND, PART_POINT } sh_part;
typedef struct {
  sh_type type;
  actor_t *subj[3]; int nsubj;
  float dist, height, fov, dur;
  v3 fpos, flook;     /* SH_FIXED; SH_INSERT with PART_POINT uses fpos */
  int part;           /* sh_part, SH_INSERT */
} dir_spec;
/* 0..1 how strongly a whip or a smash is happening now (the HUD draws speed lines with it) */
float director_whip(void);
void director_init(t3_camera *cam);
/* who may get in the way of a shot; the solver keeps them out of the frame */
void director_set_occluders(actor_t *const *list, int n);
void director_cut(const dir_spec *s);
void director_update(float dt);
bool director_active(void);
/* v1.1 moods bias coverage (the SH_AUTO pick), lens, handheld and how long a shot holds */
typedef enum { DM_CALM, DM_TENSE, DM_COMIC, DM_EMOTIONAL, DM_COUNT } dir_mood;
void director_set_mood(int mood);
int director_mood(void);
/* how many more lines a shot holds on the same speaker before auto coverage changes the angle */
int director_hold_lines(void);
/* director_cut with SH_AUTO picks the coverage by mood: subj[0] speaks, subj[1] (optional) listens */
void director_stop(void);
#endif
