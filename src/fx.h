/* fx.h: particles, sparks, shockwave rings and additive flashes in the 3D scene. Three
 * instanced pools, so three draw calls at most, no allocation after init. */
#ifndef KH_FX_H
#define KH_FX_H
#include "ds.h"
#include "three.h"

void fx_init(t3_scene *scene);
void fx_clear(void);
void fx_update(float dt, const t3_camera *cam);        /* after the camera moved */
void fx_sparks(v3 at, v3 normal, uint32_t color, int count);   /* a hit: a hot spray that bounces */
void fx_clash(v3 at, uint32_t color);                  /* steel on steel: a white star and a fan of sparks */
void fx_flash(v3 at, float size, uint32_t color);      /* a round additive flash */
void fx_shock(v3 at, float r0, float r1, uint32_t color, float life);   /* a ring on the ground growing out */
void fx_ring_up(v3 at, float r0, float r1, uint32_t color, float life); /* a ring facing the camera growing out */
void fx_dust(v3 at, float size, int count);            /* feet, falls, landings */
void fx_motes(v3 at, uint32_t color, int count, float speed);  /* slow glowing motes */
void fx_streak(v3 a, v3 b, uint32_t color, float width, float life);   /* a bright line that fades */
void fx_burst(v3 at, uint32_t color, float size);      /* the summon and the big hits: flash, rings, sparks */
/* one frame draws, call every frame while the thing exists */
void fx_glow(v3 at, float size, uint32_t color);
void fx_glint(v3 at, float size, uint32_t color);      /* a four point star: a blade about to swing */
void fx_beam(v3 from, v3 to, uint32_t color, float width);
void fx_ring_flat(v3 at, float radius, uint32_t color);/* flat on the ground */
void fx_smoke(v3 at, float size, uint32_t color, int count, float rise);   /* coloured puffs: gas, frost, steam */

/* props: small vertex coloured meshes, PS2 style. Build between fxp_begin and fxp_end in the prop's own
 * frame (y up, origin on the floor); fxp_end adds a group to the scene, at most three draw calls
 * (lit body, unlit glow, additive halo). Remove it with t3_object_remove(&scene->base, obj). */
enum { FXP_BODY, FXP_GLOW, FXP_HALO };
void fxp_begin(void);
void fxp_layer(int layer);
void fxp_box(v3 c, v3 size, uint32_t color);
void fxp_cyl(v3 base, float h, float r0, float r1, int sides, uint32_t color);
void fxp_quad(v3 a, v3 b, v3 c, v3 d, uint32_t color);
void fxp_disc(v3 c, float r, int sides, uint32_t inner, uint32_t outer);     /* flat, facing up */
void fxp_bar(v3 a, v3 b, float w, uint32_t color);                            /* a square rod from a to b */
void fxp_ring(v3 c, v3 axis, float R, float r, int segs, uint32_t color);    /* a torus about axis: a wheel */
t3_object *fxp_end(void);
#endif
