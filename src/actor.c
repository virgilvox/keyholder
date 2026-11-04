/* actor.c: see actor.h. The spring bones follow the VRM 0 secondary animation
 * rule: each joint's tail is a verlet point pulled back toward its rest direction
 * by stiffness, pushed by gravity, kept at its length, pushed out of collider
 * spheres, and the joint turned to point at it. */
#include "actor.h"
#include "anim.h"
#include "gen/cast_meta.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const actor_palette ACTOR_PAL_NONE = {{0, 1, 1}, {0, 1, 1}, {0, 1, 1}, {0, 1, 1}};

#define MAX_JOINTS 96
#define MAX_COLLIDERS 24
typedef struct {
  t3_object *bone, *parent;
  float axis[3], len;
  float tail[3], prev[3];
  float stiff, grav, drag, radius;
} sp_joint;
typedef struct { t3_object *bone; float r, off[3]; } sp_collider;

struct actor {
  actor_model model;
  float scale, floor_y;
  t3_object *root, *gltf_scene;
  t3_gltf *gltf;
  t3_mesh *face, *body, *outline, *shadow;
  ck_mixer mixer;
  ck_action *cur;
  int cur_clip;
  v3 pos; float yaw;
  t3_object *b_neck, *b_head, *b_chest, *b_rhand, *b_lhand;
  t3_object *s_rhand, *s_lhand;
  /* look */
  v3 look_at; float look_tgt, look_w, look_yaw, look_pitch;
  /* face */
  float ex[EX_COUNT], ex_t[EX_COUNT];
  float blink_t, blink_v;
  float mouth, mouth_shape;
  /* springs */
  sp_joint j[MAX_JOINTS]; int nj;
  sp_collider c[MAX_COLLIDERS]; int nc;
  bool spring_reset;
};

static ck_rig *RIG;

/* ---- small quaternion helpers on {x, y, z, w} ---- */
static void qmul(float *o, const float *a, const float *b) {
  float x = a[3]*b[0] + a[0]*b[3] + a[1]*b[2] - a[2]*b[1];
  float y = a[3]*b[1] - a[0]*b[2] + a[1]*b[3] + a[2]*b[0];
  float z = a[3]*b[2] + a[0]*b[1] - a[1]*b[0] + a[2]*b[3];
  float w = a[3]*b[3] - a[0]*b[0] - a[1]*b[1] - a[2]*b[2];
  o[0] = x; o[1] = y; o[2] = z; o[3] = w;
}
static void qrot(float *o, const float *q, const float *v) {
  float t[3] = { 2*(q[1]*v[2] - q[2]*v[1]), 2*(q[2]*v[0] - q[0]*v[2]), 2*(q[0]*v[1] - q[1]*v[0]) };
  float x = v[0] + q[3]*t[0] + q[1]*t[2] - q[2]*t[1];
  float y = v[1] + q[3]*t[1] + q[2]*t[0] - q[0]*t[2];
  float z = v[2] + q[3]*t[2] + q[0]*t[1] - q[1]*t[0];
  o[0] = x; o[1] = y; o[2] = z;
}
static void qinv(float *o, const float *q) { o[0] = -q[0]; o[1] = -q[1]; o[2] = -q[2]; o[3] = q[3]; }
static void qaxis(float *o, float x, float y, float z, float angle) {
  float s = sinf(angle * .5f); o[0] = x * s; o[1] = y * s; o[2] = z * s; o[3] = cosf(angle * .5f);
}
/* the rotation of an object's world matrix (scale taken out) */
static void wquat(const t3_object *ob, float *q) {
  const float *m = ob->matrix_world.e;
  float sx = sqrtf(m[0]*m[0] + m[1]*m[1] + m[2]*m[2]), sy = sqrtf(m[4]*m[4] + m[5]*m[5] + m[6]*m[6]), sz = sqrtf(m[8]*m[8] + m[9]*m[9] + m[10]*m[10]);
  float a = m[0]/sx, b = m[4]/sy, c = m[8]/sz, d = m[1]/sx, e = m[5]/sy, f = m[9]/sz, g = m[2]/sx, h = m[6]/sy, i = m[10]/sz;
  float t = a + e + i, s;
  if (t > 0) { s = .5f / sqrtf(t + 1); q[3] = .25f / s; q[0] = (h - f) * s; q[1] = (c - g) * s; q[2] = (d - b) * s; }
  else if (a > e && a > i) { s = 2 * sqrtf(1 + a - e - i); q[3] = (h - f) / s; q[0] = .25f * s; q[1] = (b + d) / s; q[2] = (c + g) / s; }
  else if (e > i) { s = 2 * sqrtf(1 + e - a - i); q[3] = (c - g) / s; q[0] = (b + d) / s; q[1] = .25f * s; q[2] = (f + h) / s; }
  else { s = 2 * sqrtf(1 + i - a - e); q[3] = (d - b) / s; q[0] = (c + g) / s; q[1] = (f + h) / s; q[2] = .25f * s; }
}
static void wpos(const t3_object *ob, float *p) { const float *m = ob->matrix_world.e; p[0] = m[12]; p[1] = m[13]; p[2] = m[14]; }

/* ---- palette swap on the body atlas ---- */
static void hsv_turn(uint8_t *px, const actor_swatch *s) {
  float r = px[0] / 255.f, g = px[1] / 255.f, b = px[2] / 255.f;
  float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b)), d = mx - mn, h = 0;
  if (d > 1e-4f) { if (mx == r) h = fmodf((g - b) / d + 6, 6); else if (mx == g) h = (b - r) / d + 2; else h = (r - g) / d + 4; }
  float sat = mx > 0 ? d / mx : 0, v = mx;
  h = fmodf(h + s->hue / 60.f + 12, 6); sat = ds_clamp(sat * s->sat, 0, 1); v = ds_clamp(v * s->val, 0, 1);
  float c = v * sat, x = c * (1 - fabsf(fmodf(h, 2) - 1)), m = v - c, o[3];
  int k = (int)h;
  o[0] = k == 0 || k == 5 ? c : k == 1 || k == 4 ? x : 0;
  o[1] = k == 1 || k == 2 ? c : k == 0 || k == 3 ? x : 0;
  o[2] = k == 3 || k == 4 ? c : k == 2 || k == 5 ? x : 0;
  for (int i = 0; i < 3; i++) px[i] = (uint8_t)((o[i] + m) * 255.f + .5f);
}
static void apply_palette(t3_texture *T, actor_model model, const actor_palette *pal) {
  if (!T || !T->pixels || !pal) return;
  const cast_meta *M = &CAST_META[model];
  const actor_swatch *sw[4] = { &pal->hair, &pal->top, &pal->bottom, &pal->shoes };
  for (int i = 0; i < M->nrect; i++) {
    const cast_rect *r = &M->rect[i];
    const actor_swatch *s = sw[r->kind];
    if (s->hue == 0 && s->sat == 1 && s->val == 1) continue;
    for (int y = r->y; y < r->y + r->h && y < T->height; y++)
      for (int x = r->x; x < r->x + r->w && x < T->width; x++) hsv_turn(T->pixels + (size_t)(y * T->width + x) * 4, s);
  }
  T->version++;
}

/* ---- materials: cel lambert off the atlas, dark flat hull ---- */
static void setup_mesh(t3_object *o, void *ctx) {
  actor_t *a = ctx;
  if (o->type != T3_MESH && o->type != T3_SKINNED_MESH) return;
  o->frustum_culled = false; o->cast_shadow = false; o->receive_shadow = false;
  t3_mesh *m = (t3_mesh *)o;
  t3_material *src = m->material_count ? m->materials[0] : NULL;
  if (!strcmp(o->name, "outline")) {
    t3_material *M = t3_mesh_basic_material_new(0x14101a);
    M->fog = true;
    if (m->materials[0]) t3_release(m->materials[0]);
    m->materials[0] = M;
    a->outline = m;
    return;
  }
  t3_material *M = t3_mesh_lambert_material_new(0xffffff);
  t3_texture *T = src ? src->map : NULL;
  if (T) {
    T->color_space = T3_SRGB_COLOR_SPACE;
    T->generate_mipmaps = true; T->min_filter = T3_LINEAR_MIPMAP_LINEAR; T->mag_filter = T3_LINEAR;
    T->version++;
    t3_material_set_map(M, T);
  }
  M->alpha_test = 0.45f;
  M->side = T3_DOUBLE_SIDE;
  M->fog = true;
  if (src) t3_release(src);
  m->materials[0] = M;
  if (!strcmp(o->name, "face")) { a->face = m; t3_mesh_update_morph_targets(m); }
  else a->body = m;
}

/* ---- springs ---- */
static void add_chain(actor_t *a, t3_object *bone, t3_object *parent, float stiff, float grav, float drag, float radius) {
  if (a->nj >= MAX_JOINTS) return;
  sp_joint *j = &a->j[a->nj++];
  j->bone = bone; j->parent = parent; j->stiff = stiff; j->grav = grav; j->drag = drag; j->radius = radius;
  /* the tail: the first child, or 7 cm on along the bone */
  float ax[3];
  if (bone->child_count) { t3_object *c = bone->children[0]; ax[0] = c->position.x; ax[1] = c->position.y; ax[2] = c->position.z; }
  else { ax[0] = bone->position.x; ax[1] = bone->position.y; ax[2] = bone->position.z; float l = sqrtf(ax[0]*ax[0]+ax[1]*ax[1]+ax[2]*ax[2]); if (l > 1e-5f) { ax[0] *= .07f/l; ax[1] *= .07f/l; ax[2] *= .07f/l; } }
  float l = sqrtf(ax[0]*ax[0] + ax[1]*ax[1] + ax[2]*ax[2]);
  j->len = l;
  if (l > 1e-5f) { j->axis[0] = ax[0]/l; j->axis[1] = ax[1]/l; j->axis[2] = ax[2]/l; } else { j->axis[1] = -1; j->len = .05f; }
  for (int i = 0; i < bone->child_count; i++) add_chain(a, bone->children[i], bone, stiff, grav, drag, radius);
}
static void load_springs(actor_t *a) {
  size_t n; char *t = (char *)ds_asset(CAST_META[a->model].spr, &n);
  if (!t) return;
  char *save = NULL;
  for (char *line = strtok_r(t, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
    char name[32]; float r, x, y, z, st, gr, dr, hr; int cnt, off = 0;
    if (line[0] == 'c' && sscanf(line, "c %31s %f %f %f %f", name, &r, &x, &y, &z) == 5) {
      if (a->nc >= MAX_COLLIDERS) continue;
      t3_object *b = t3_object_get_by_name(a->gltf_scene, name);
      if (b) { sp_collider *c = &a->c[a->nc++]; c->bone = b; c->r = r; c->off[0] = x; c->off[1] = y; c->off[2] = z; }
    } else if (line[0] == 'g' && sscanf(line, "g %f %f %f %f %d%n", &st, &gr, &dr, &hr, &cnt, &off) == 5) {
      char *p = line + off;
      for (int i = 0; i < cnt; i++) {
        int adv = 0;
        if (sscanf(p, " %31s%n", name, &adv) != 1) break;
        p += adv;
        t3_object *b = t3_object_get_by_name(a->gltf_scene, name);
        if (b && b->parent) add_chain(a, b, b->parent, st, gr, dr, hr);
      }
    }
  }
  free(t);
}
static void spring_seed(actor_t *a) {
  for (int i = 0; i < a->nj; i++) {
    sp_joint *j = &a->j[i];
    float q[4], p[3], d[3];
    wquat(j->bone, q); wpos(j->bone, p);
    float s = a->scale;
    qrot(d, q, j->axis);
    for (int k = 0; k < 3; k++) { j->tail[k] = p[k] + d[k] * j->len * s; j->prev[k] = j->tail[k]; }
  }
}
static void spring_step(actor_t *a, float dt) {
  float s = a->scale;
  float cp[MAX_COLLIDERS][3];
  for (int c = 0; c < a->nc; c++) {
    float q[4], p[3], o[3]; sp_collider *C = &a->c[c];
    wquat(C->bone, q); wpos(C->bone, p);
    qrot(o, q, C->off);
    for (int k = 0; k < 3; k++) cp[c][k] = p[k] + o[k] * s;
  }
  for (int i = 0; i < a->nj; i++) {
    sp_joint *j = &a->j[i];
    t3_object_update_world_matrix(j->parent, false, false);
    float pq[4], p[3], d[3];
    wquat(j->parent, pq);
    /* the bone's own head: the parent's frame plus its local offset */
    float lo[3] = { j->bone->position.x, j->bone->position.y, j->bone->position.z }, wo[3], pp[3];
    qrot(wo, pq, lo); wpos(j->parent, pp);
    for (int k = 0; k < 3; k++) p[k] = pp[k] + wo[k] * s;
    qrot(d, pq, j->axis);
    float next[3], len = j->len * s, keep = 1 - j->drag;
    float scale_dt = dt * 60.f;   /* keep the source's feel at any frame rate */
    for (int k = 0; k < 3; k++) next[k] = j->tail[k] + (j->tail[k] - j->prev[k]) * keep + d[k] * j->stiff * dt * len * 6.f;
    next[1] -= j->grav * dt * .9f;
    (void)scale_dt;
    /* keep the length */
    float v[3] = { next[0]-p[0], next[1]-p[1], next[2]-p[2] };
    float l = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]); if (l < 1e-5f) l = 1e-5f;
    for (int k = 0; k < 3; k++) next[k] = p[k] + v[k] / l * len;
    /* out of the colliders */
    for (int c = 0; c < a->nc; c++) {
      float dv[3] = { next[0]-cp[c][0], next[1]-cp[c][1], next[2]-cp[c][2] };
      float rr = (a->c[c].r + j->radius) * s, dl = sqrtf(dv[0]*dv[0] + dv[1]*dv[1] + dv[2]*dv[2]);
      if (dl < rr && dl > 1e-5f) {
        for (int k = 0; k < 3; k++) next[k] = cp[c][k] + dv[k] / dl * rr;
        float w[3] = { next[0]-p[0], next[1]-p[1], next[2]-p[2] }; float wl = sqrtf(w[0]*w[0] + w[1]*w[1] + w[2]*w[2]); if (wl < 1e-5f) wl = 1e-5f;
        for (int k = 0; k < 3; k++) next[k] = p[k] + w[k] / wl * len;
      }
    }
    memcpy(j->prev, j->tail, sizeof j->tail); memcpy(j->tail, next, sizeof next);
    /* turn the bone to the tail, in the parent's frame */
    float pi[4], dir[3], w[3] = { next[0]-p[0], next[1]-p[1], next[2]-p[2] };
    qinv(pi, pq); qrot(dir, pi, w);
    float dl = sqrtf(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]); if (dl < 1e-5f) dl = 1e-5f;
    for (int k = 0; k < 3; k++) dir[k] /= dl;
    float dt2 = j->axis[0]*dir[0] + j->axis[1]*dir[1] + j->axis[2]*dir[2];
    float q[4] = { j->axis[1]*dir[2] - j->axis[2]*dir[1], j->axis[2]*dir[0] - j->axis[0]*dir[2], j->axis[0]*dir[1] - j->axis[1]*dir[0], 1 + dt2 };
    float ql = sqrtf(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    if (ql < 1e-5f) { q[0] = 0; q[1] = 0; q[2] = 0; q[3] = 1; ql = 1; }
    t3_quat Q = { q[0]/ql, q[1]/ql, q[2]/ql, q[3]/ql };
    t3_object_set_quaternion(j->bone, Q);
    t3_object_update_world_matrix(j->bone, false, false);
  }
}

actor_t *actor_new(actor_model model, const actor_palette *pal, float scale, t3_scene *scene) {
  actor_t *a = calloc(1, sizeof *a);
  a->model = model; a->scale = scale > 0 ? scale : 1; a->cur_clip = -1; a->blink_t = ds_rr(1, 4);
  if (!RIG) RIG = rig_load("rig.bin");
  a->root = t3_object_new();
  size_t n; uint8_t *b = ds_asset(CAST_META[model].glb, &n);
  if (b) { a->gltf = t3_gltf_parse(b, n, NULL, NULL); free(b); }
  if (!a->gltf) { ds_log("actor: cannot parse %s", CAST_META[model].glb); t3_object_add(&scene->base, a->root); return a; }
  a->gltf_scene = a->gltf->scene;
  t3_object_add(a->root, a->gltf_scene);
  t3_object_traverse(a->gltf_scene, setup_mesh, a);
  if (a->body && a->body->materials[0]) apply_palette(a->body->materials[0]->map, model, pal);
  if (RIG) { mixer_init(&a->mixer, RIG, a->gltf_scene); a->mixer.hips_scale = CAST_META[model].hips_h; }
  a->b_neck = t3_object_get_by_name(a->gltf_scene, "neck");
  a->b_head = t3_object_get_by_name(a->gltf_scene, "head");
  a->b_chest = t3_object_get_by_name(a->gltf_scene, "upperChest");
  a->b_rhand = t3_object_get_by_name(a->gltf_scene, "rightHand");
  a->b_lhand = t3_object_get_by_name(a->gltf_scene, "leftHand");
  a->s_rhand = t3_object_new(); a->s_lhand = t3_object_new();
  if (a->b_rhand) t3_object_add(a->b_rhand, a->s_rhand);
  if (a->b_lhand) t3_object_add(a->b_lhand, a->s_lhand);
  load_springs(a);
  /* the blob shadow */
  {
    enum { N = 32 };
    uint8_t *px = malloc(N * N * 4);
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
      float dx = (x + .5f) / N * 2 - 1, dy = (y + .5f) / N * 2 - 1, d = sqrtf(dx*dx + dy*dy);
      uint8_t *p = px + (y * N + x) * 4; p[0] = 8; p[1] = 6; p[2] = 14; p[3] = (uint8_t)(ds_clamp(1 - d, 0, 1) * 150);
    }
    t3_texture *T = t3_texture_new(N, N, px); free(px);
    t3_material *M = t3_mesh_basic_material_new(0xffffff);
    t3_material_set_map(M, T); M->transparent = true; M->depth_write = false; M->fog = false;
    t3_geometry *G = t3_plane_geometry_new(1.1f, 1.1f, 1, 1);
    a->shadow = t3_mesh_new(G, M);
    t3_object_set_rotation(&a->shadow->base, -DS_PI / 2, 0, 0);
    t3_object_set_position(&a->shadow->base, 0, .02f, 0);
    a->shadow->base.render_order = 1;
    t3_object_add(&scene->base, &a->shadow->base);
    t3_release(T); t3_release(M); t3_release(G);
  }
  t3_object_add(&scene->base, a->root);
  actor_play(a, "Idle_Loop", 1, true, 0);
  actor_place(a, v3_(0, 0, 0), 0);
  return a;
}

void actor_move(actor_t *a, v3 pos, float yaw) {
  a->pos = pos; a->yaw = yaw;
  t3_object_set_position(a->root, pos.x, pos.y, pos.z);
  t3_object_set_rotation(a->root, 0, yaw, 0);
  t3_object_set_scale(a->root, a->scale, a->scale, a->scale);
  if (a->shadow) t3_object_set_position(&a->shadow->base, pos.x, a->floor_y + .02f, pos.z);
}
void actor_place(actor_t *a, v3 pos, float yaw) { a->floor_y = pos.y; actor_move(a, pos, yaw); a->spring_reset = true; }
void actor_set_floor(actor_t *a, float f) { a->floor_y = f; }
v3 actor_pos(const actor_t *a) { return a->pos; }
float actor_yaw(const actor_t *a) { return a->yaw; }
void actor_visible(actor_t *a, bool on) { a->root->visible = on; if (a->shadow) a->shadow->base.visible = on; }
bool actor_is_visible(const actor_t *a) { return a->root->visible; }
float actor_height(const actor_t *a) { return CAST_META[a->model].height * a->scale; }
float actor_hips_height(const actor_t *a) { return CAST_META[a->model].hips_h * a->scale; }

static void play(actor_t *a, const char *clip, float speed, bool loop, float fade, bool restart) {
  if (!RIG) return;
  int c = rig_clip_index(RIG, clip);
  if (c < 0) { ds_log("actor: no clip %s", clip); return; }
  ck_action *ac = mixer_action(&a->mixer, c);
  if (c == a->cur_clip && a->cur && !restart) { ac->time_scale = speed; return; }
  if (c == a->cur_clip && a->cur) { action_reset(ac); ac->time_scale = speed; return; }
  action_reset(ac);
  ac->loop = loop ? LOOP_REPEAT : LOOP_ONCE;
  ac->clamp_when_finished = !loop;
  ac->time_scale = speed;
  action_play(&a->mixer, ac);
  if (a->cur && fade > 0) action_cross_fade_from(&a->mixer, ac, a->cur, fade);
  else {
    if (a->cur) action_stop(&a->mixer, a->cur);
    action_set_effective_weight(ac, 1);
  }
  a->cur = ac; a->cur_clip = c;
}
void actor_play(actor_t *a, const char *clip, float speed, bool loop, float fade) { play(a, clip, speed, loop, fade, false); }
void actor_replay(actor_t *a, const char *clip, float speed, bool loop, float fade) { play(a, clip, speed, loop, fade, true); }
bool actor_clip_done(const actor_t *a) { return a->cur && a->cur->loop == LOOP_ONCE && a->cur->paused; }
float actor_clip_time(const actor_t *a) { return a->cur ? a->cur->time : 0; }
float actor_clip_len(const actor_t *a) { return a->cur ? action_duration(&a->mixer, a->cur) : 0; }
float actor_clip_ground_speed(const actor_t *a) { return a->cur_clip >= 0 && RIG ? RIG->clips[a->cur_clip].speed * actor_hips_height(a) : 0; }

void actor_look(actor_t *a, v3 t, float amount) { a->look_at = t; a->look_tgt = amount; }
void actor_expr_set(actor_t *a, actor_expr e, float v) { a->ex_t[e] = v; }
void actor_expr_clear(actor_t *a) { for (int i = EX_JOY; i <= EX_FUN; i++) a->ex_t[i] = 0; }
void actor_mouth(actor_t *a, float open, float shape) { a->mouth = open; a->mouth_shape = shape; }

static void apply_look(actor_t *a, float dt) {
  a->look_w = ds_damp(a->look_w, a->look_tgt, 6, dt);
  if (a->look_w < 0.005f || !a->b_head) return;
  v3 h = actor_head_pos(a);
  float dx = a->look_at.x - h.x, dy = a->look_at.y - h.y, dz = a->look_at.z - h.z;
  float want = ds_wrap_pi(atan2f(dx, dz) - a->yaw);
  float pitch = atan2f(dy, sqrtf(dx*dx + dz*dz));
  want = ds_clamp(want, -1.15f, 1.15f); pitch = ds_clamp(pitch, -.5f, .45f);
  a->look_yaw = ds_damp(a->look_yaw, want, 8, dt); a->look_pitch = ds_damp(a->look_pitch, pitch, 8, dt);
  float qy[4], qp[4], d[4], cur[4], o[4];
  qaxis(qy, 0, 1, 0, a->look_yaw * a->look_w * .5f);
  qaxis(qp, 1, 0, 0, -a->look_pitch * a->look_w * .5f);
  qmul(d, qy, qp);
  t3_object *bones[2] = { a->b_neck, a->b_head };
  for (int i = 0; i < 2; i++) {
    t3_object *b = bones[i]; if (!b) continue;
    cur[0] = b->quaternion.x; cur[1] = b->quaternion.y; cur[2] = b->quaternion.z; cur[3] = b->quaternion.w;
    qmul(o, d, cur);
    t3_quat Q = { o[0], o[1], o[2], o[3] };
    t3_object_set_quaternion(b, Q);
  }
}

void actor_update(actor_t *a, float dt) {
  if (!a->gltf || !a->root->visible) return;
  mixer_update(&a->mixer, dt);
  if (a->mixer.bound) {}
  apply_look(a, dt);
  /* the face */
  a->blink_t -= dt;
  if (a->blink_t < 0) { a->blink_v = 1; a->blink_t = ds_rr(2, 5.5f); }
  a->blink_v = fmaxf(0, a->blink_v - dt * 7);
  float blink = a->blink_v > 0 ? 1 - fabsf(a->blink_v * 2 - 1) : 0;
  for (int i = 0; i < EX_COUNT; i++) a->ex[i] = ds_damp(a->ex[i], a->ex_t[i], 9, dt);
  if (a->face && a->face->morph_influences) {
    float *m = a->face->morph_influences;
    int n = a->face->morph_influence_count;
    float shape = a->mouth_shape, op = ds_clamp(a->mouth, 0, 1);
    /* a(0) e(.25) i(.5) o(.75) u(1) */
    float vow[5] = { fmaxf(0, 1 - fabsf(shape) * 4), fmaxf(0, 1 - fabsf(shape - .25f) * 4), fmaxf(0, 1 - fabsf(shape - .5f) * 4), fmaxf(0, 1 - fabsf(shape - .75f) * 4), fmaxf(0, 1 - fabsf(shape - 1) * 4) };
    float joy = a->ex[EX_JOY];
    if (n > EX_FUN) {
      m[EX_BLINK] = fmaxf(blink * (1 - joy), a->ex[EX_BLINK]);
      m[EX_A] = vow[0] * op; m[EX_E] = vow[1] * op; m[EX_I] = vow[2] * op; m[EX_O] = vow[3] * op; m[EX_U] = vow[4] * op;
      m[EX_JOY] = joy; m[EX_ANGRY] = a->ex[EX_ANGRY]; m[EX_SORROW] = a->ex[EX_SORROW]; m[EX_FUN] = a->ex[EX_FUN];
    }
  }
  t3_object_update_world_matrix(a->root, true, true);
  if (a->spring_reset) { spring_seed(a); a->spring_reset = false; }
  else if (dt > 0) spring_step(a, dt);
  t3_object_update_world_matrix(a->root, true, true);
}

t3_object *actor_socket_rhand(actor_t *a) { return a->s_rhand; }
t3_object *actor_socket_lhand(actor_t *a) { return a->s_lhand; }
v3 actor_bone_pos(actor_t *a, const char *bone) {
  t3_object *b = a->gltf_scene ? t3_object_get_by_name(a->gltf_scene, bone) : NULL;
  if (!b) return a->pos;
  float p[3]; wpos(b, p);
  return v3_(p[0], p[1], p[2]);
}
v3 actor_head_pos(actor_t *a) { float p[3] = {a->pos.x, a->pos.y + 1.4f, a->pos.z}; if (a->b_head) wpos(a->b_head, p); return v3_(p[0], p[1], p[2]); }
void actor_flash(actor_t *a, float r, float g, float b) {
  t3_mesh *m[2] = { a->face, a->body };
  for (int i = 0; i < 2; i++) if (m[i] && m[i]->materials[0]) m[i]->materials[0]->color = (t3_color){ r, g, b };
}
