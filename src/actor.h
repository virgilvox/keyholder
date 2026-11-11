/* actor.h: one anime character. A VRoid sample avatar baked by tools/prep-vrm.mjs (parsed once
 * per model and cloned, see actor_preload),
 * driven by the mocap clips (anim.c) with cel lit materials, an inverted hull
 * outline, expression morphs (a model without them keeps a still face), a procedural head look, spring bones for hair and
 * skirts, and a palette swap so one model can play several people.
 * Units are metres; an actor stands at about 1.6 m. yaw 0 faces +z. */
#ifndef KH_ACTOR_H
#define KH_ACTOR_H
#include "ds.h"
#include "three.h"

/* cast_meta order. H young man, messy black hair, hoodie; M man, slicked short hair, vest and tie;
 * O older man, grey hair, glasses, lines; P the M body heavier, stubble; U the H body big, dark stubble;
 * K black bob, school vest and skirt; E brown bob, yellow dress; V silver hair, sci-fi suit;
 * G blonde side ponytail, white dress; W cropped hair, vest and skirt; R the W body older, grey, glasses */
typedef enum { AM_B, AM_C, AM_D, AM_S, AM_F, AM_H, AM_M, AM_O, AM_P, AM_U, AM_K, AM_E, AM_V, AM_G, AM_W, AM_R, AM_COUNT } actor_model;
typedef enum { EX_BLINK, EX_A, EX_I, EX_U, EX_E, EX_O, EX_JOY, EX_ANGRY, EX_SORROW, EX_FUN, EX_COUNT } actor_expr;

/* a palette swap: a hue turn (degrees) and saturation and value scales per part */
typedef struct { float hue, sat, val; } actor_swatch;
typedef struct { actor_swatch hair, top, bottom, shoes; } actor_palette;
extern const actor_palette ACTOR_PAL_NONE;

typedef struct actor actor_t;

actor_t *actor_new(actor_model model, const actor_palette *pal, float scale, t3_scene *scene);
/* parse a model ahead of time (a loading screen, a chapter start): actor_new clones it and does
 * not stall the frame. actor_new preloads on first use otherwise. */
bool actor_preload(actor_model model);
void actor_preload_all(void);
/* level of detail, picked by the renderer by camera distance: the full body with face morphs
 * and outline within 5 m, a light body (about 5k triangles) with an outline to 10 m, the light
 * body alone beyond. bias scales those distances for every actor (2 = keep detail twice as far).
 * actor_lod_camera names the camera that renders (once; it may change its fov): the distances
 * then scale with its lens, so a long lens close up keeps the full face. */
void actor_lod_bias(float bias);
void actor_lod_camera(const t3_camera *cam);
int actor_lod_level(const actor_t *a);               /* 0 near, 1 mid, 2 far: as last rendered */
void actor_place(actor_t *a, v3 pos, float yaw);     /* a jump: the spring bones reset */
void actor_move(actor_t *a, v3 pos, float yaw);      /* a step: the spring bones follow */
v3 actor_pos(const actor_t *a);
float actor_yaw(const actor_t *a);
void actor_visible(actor_t *a, bool on);
bool actor_is_visible(const actor_t *a);
float actor_height(const actor_t *a);
float actor_hips_height(const actor_t *a);

/* crossfade to a clip by name (tools/prep-vrm.mjs CLIPS); replaying the current clip only
 * changes its speed unless restart is set */
void actor_play(actor_t *a, const char *clip, float speed, bool loop, float fade);
void actor_replay(actor_t *a, const char *clip, float speed, bool loop, float fade);
bool actor_clip_done(const actor_t *a);              /* a one shot reached its end */
float actor_clip_time(const actor_t *a);             /* seconds into the current clip */
float actor_clip_len(const actor_t *a);
float actor_clip_ground_speed(const actor_t *a);     /* metres a second the clip's feet travel at speed 1 */
void actor_clip_seek(actor_t *a, float t);           /* jump the current clip to t seconds: sections of a long mocap take */

/* look at a world point (head and neck turn, clamped); amount 0 lets go */
void actor_look(actor_t *a, v3 target, float amount);
/* expressions ease to these amounts, 0..1; blink runs on its own */
void actor_expr_set(actor_t *a, actor_expr e, float amount);
void actor_expr_clear(actor_t *a);
/* lip sync: mouth open 0..1, shape chooses the vowel; 0 closes */
void actor_mouth(actor_t *a, float open, float shape);

void actor_update(actor_t *a, float dt);             /* after actor_move / actor_play, every frame */

/* attach points: a group that follows the bone, in the bone's frame, x along the fingers
 * for the right hand; add a prop with t3_object_add. */
t3_object *actor_socket_rhand(actor_t *a);
t3_object *actor_socket_lhand(actor_t *a);
v3 actor_bone_pos(actor_t *a, const char *bone);     /* world position, after actor_update */
v3 actor_head_pos(actor_t *a);
/* a flat colour flash over the whole model (hit flash): colour multiplier 0..n, 1 = normal */
void actor_flash(actor_t *a, float r, float g, float b);
/* the blob shadow is drawn by the actor at floor_y */
void actor_set_floor(actor_t *a, float floor_y);
/* a new palette on a model already made (reuses the actor, so no glTF parse); uploads the atlas again */
void actor_repalette(actor_t *a, const actor_palette *pal);
/* both forearms up, open handed, over whatever clip plays: 0..1, eases (a beaten fighter yielding) */
void actor_hands_up(actor_t *a, float amount);

#endif
