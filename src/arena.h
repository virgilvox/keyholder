/* arena.h: the five fight stages, inside world.c. */
#ifndef KH_ARENA_H
#define KH_ARENA_H
#include "ds.h"
#include "three.h"
#include "world.h"
void arena_init(t3_object *root);
void arena_show(world_stage s);   /* STAGE_TOWN hides them all; a stage shows in its base look */
/* phase two's look (0 or 1): it grows in over a second and a half; collision follows past halfway */
void arena_set_phase(int phase);
int arena_get_phase(void);
float arena_blend(void);          /* 0 base look .. 1 phase two */
bool arena_update(float dt);      /* per frame; true when the mood changed (the sky needs a repaint) */
void arena_floor(world_stage s, v3 *center, float *radius);
void arena_push(world_stage s, v3 *p, float r, float h, v3 *n, bool *hit);
bool arena_raycast(world_stage s, v3 o, v3 d, float max_dist, float *t, v3 *n);
/* the stage's mood: sky gradient (zenith, horizon), fog, the two lights. Linear colours */
typedef struct { t3_color zenith, horizon, fog, hemi_sky, hemi_ground, sun; float hemi, dir, fog_near, fog_far; v3 sun_dir; float stars; } arena_mood;
void arena_mood_of(world_stage s, arena_mood *m);
#endif
