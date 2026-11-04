/* arena.c: the five fight stages (assets/models/arena_*.glb, made by tools/prep-arena.mjs from the
 * lab's own parts). A flat floor, a circle that keeps fighters in and a few round pillars. Mood is
 * colours and the sky and fog, not extra lights. */
#include "arena.h"
#include "gen/arena_data.h"
#include <stdlib.h>
#include <string.h>

static t3_object *group[5];
static t3_material *mlit, *mglow;
void arena_init(t3_object *root) {
  mlit = t3_mesh_lambert_material_new(0xffffff); mlit->vertex_colors = true; mlit->side = T3_DOUBLE_SIDE;
  mglow = t3_mesh_basic_material_new(0xffffff); mglow->vertex_colors = true; mglow->side = T3_DOUBLE_SIDE;
  mglow->color = (t3_color){1.6f, 1.6f, 1.6f};
  for (int i = 0; i < 5; i++) {
    group[i] = t3_group_new();
    t3_object_add(root, group[i]);
    size_t n; uint8_t *b = ds_asset(ARENA_INFO[i].glb, &n);
    if (!b) continue;
    t3_gltf *g = t3_gltf_parse(b, n, NULL, NULL);
    free(b);
    if (!g) continue;
    t3_object *r = g->scene;
    for (int k = 0; k < r->child_count; k++) {
      t3_object *o = r->children[k];
      if (o->type != T3_MESH) continue;
      t3_mesh *m = (t3_mesh *)o;
      t3_material *nm = o->name && !strncmp(o->name, "glow_", 5) ? mglow : mlit;
      t3_retain(nm); t3_release(m->materials[0]); m->materials[0] = nm;
    }
    t3_object_add(group[i], r);
    group[i]->visible = false;
  }
}
static inline int idx(world_stage s) { return (int)s - (int)STAGE_ARENA_BENCH; }
void arena_show(world_stage s) { for (int i = 0; i < 5; i++) if (group[i]) group[i]->visible = (i == idx(s)); }
void arena_floor(world_stage s, v3 *c, float *r) { *c = v3_(0, 0, 0); *r = ARENA_INFO[idx(s)].clear; }
void arena_push(world_stage s, v3 *p, float r, float h, v3 *n, bool *hit) {
  const arena_info *A = &ARENA_INFO[idx(s)];
  (void)h;
  float d = hypotf(p->x, p->z), lim = A->keep - r;
  if (d > lim && d > 1e-4f) { p->x *= lim / d; p->z *= lim / d; *n = v3_(-p->x / lim, 0, -p->z / lim); *hit = true; }
  for (int i = 0; i < A->np; i++) {
    float dx = p->x - A->p[i].x, dz = p->z - A->p[i].z, q = hypotf(dx, dz), m = A->p[i].r + r;
    if (q < m && q > 1e-4f) { p->x = A->p[i].x + dx / q * m; p->z = A->p[i].z + dz / q * m; *n = v3_(dx / q, 0, dz / q); *hit = true; }
  }
}
bool arena_raycast(world_stage s, v3 o, v3 d, float max_dist, float *t_out, v3 *n_out) {
  const arena_info *A = &ARENA_INFO[idx(s)];
  float best = max_dist; v3 bn = v3_(0, 1, 0); bool any = false;
  if (d.y < -1e-6f) { float t = -o.y / d.y; if (t >= 0 && t < best) { best = t; bn = v3_(0, 1, 0); any = true; } }
  /* the cylinder of the keep-in ring and past it, seen from inside: a wall 4 m beyond it */
  float R = A->keep + 4, a = d.x * d.x + d.z * d.z, b = o.x * d.x + o.z * d.z, c = o.x * o.x + o.z * o.z - R * R;
  if (a > 1e-7f && c < 0) { float t = (-b + sqrtf(b * b - a * c)) / a; if (t >= 0 && t < best) { best = t; v3 q = v3_add(o, v3_scale(d, t)); bn = v3_norm(v3_(-q.x, 0, -q.z)); any = true; } }
  for (int i = 0; i < A->np; i++) {
    float ox = o.x - A->p[i].x, oz = o.z - A->p[i].z, r = A->p[i].r, bb = ox * d.x + oz * d.z, cc = ox * ox + oz * oz - r * r;
    if (a < 1e-7f) continue;
    float disc = bb * bb - a * cc;
    if (disc < 0) continue;
    float t = (-bb - sqrtf(disc)) / a;
    if (t >= 0 && t < best && o.y + d.y * t < 7) { best = t; v3 q = v3_add(o, v3_scale(d, t)); bn = v3_norm(v3_(q.x - A->p[i].x, 0, q.z - A->p[i].z)); any = true; }
  }
  if (!any) return false;
  *t_out = best; if (n_out) *n_out = bn;
  return true;
}
#define H t3_color_hex
void arena_mood_of(world_stage s, arena_mood *m) {
  switch (s) {
    default:
    case STAGE_ARENA_BENCH:  /* late sun through a sawdust haze */
      *m = (arena_mood){H(0x3a2c52), H(0xf0a060), H(0xc08860), H(0xffe6c4), H(0x7c5a40), H(0xffb868), 1.7f, 1.7f, 26, 90, v3_norm(v3_(-.7f, .45f, .4f)), 0}; break;
    case STAGE_ARENA_LASER:  /* magenta and cyan in the dark */
      *m = (arena_mood){H(0x0c0a24), H(0x5a1050), H(0x2c0c34), H(0xc0a0ff), H(0xa02060), H(0xff60c0), 1.5f, 1.3f, 22, 80, v3_norm(v3_(.3f, .9f, -.3f)), .6f}; break;
    case STAGE_ARENA_FRIDGE: /* cold and bright */
      *m = (arena_mood){H(0x8cc4e8), H(0xe8f6ff), H(0xd8ecf8), H(0xdceaf8), H(0x90b0cc), H(0xc8e4ff), 1.15f, .6f, 24, 85, v3_norm(v3_(.2f, 1, .3f)), 0}; break;
    case STAGE_ARENA_SHOP:   /* furnace orange on steel */
      *m = (arena_mood){H(0x14100e), H(0x70300c), H(0x3a1c0c), H(0xd8c0a8), H(0x904820), H(0xff8a30), 1.5f, 2.0f, 22, 80, v3_norm(v3_(.5f, .6f, -.6f)), 0}; break;
    case STAGE_ARENA_UNFOLD: /* the night sky */
      *m = (arena_mood){H(0x05061a), H(0x241c50), H(0x181438), H(0x98a0e8), H(0x484068), H(0xc8c0f0), 1.2f, .8f, 70, 190, v3_norm(v3_(-.4f, .8f, .4f)), 1}; break;
  }
}
