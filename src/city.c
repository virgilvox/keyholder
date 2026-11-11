/* city.c: downtown Mesa. Everything is made at load from src/gen/city_data.h (OpenStreetMap, see
 * tools/prep-city.mjs): the ground, curbs and paint, the footprints extruded with generic fronts,
 * the storefronts of facades_*.inc placed at their addresses and the street furniture. Every part
 * is captured into vertex coloured meshes, one per cell of a grid and per material class, so a
 * block costs a few draws. Outdoor collision is 2D: footprint walls as segments, props as boxes. */
#include "city.h"
#include "gen/city_data.h"
#include "gen/sign_atlas.h"
#include "world.h"
#include "interior.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "facades_common.inc"
#include "facades_mesa.inc"
#include "facades_keyholder.inc"

/* ── the sinks ── */
/* MC_PROP and MC_FPROP: the street props, near (full) and far (a few boxes); their lamps go in MC_LAMP */
/* MC_KIT: the small things on the backs and the roofs (back_kit, roof_units), in their own fine grid of KCELL
 * cells that draw only close by */
enum { MC_PLAIN, MC_BRICK, MC_ASPHALT, MC_PAVER, MC_CONCRETE, MC_SIGN, MC_NEON, MC_LAMP, MC_WIN, MC_POOL, MC_PROP, MC_FPROP, MC_KIT, MC_COUNT };
#define CELL 90.f
#define GX0 (CITY_X0 - 60)
#define GZ0 (CITY_Z0 - 60)
#define NCX 12
#define NCZ 7
typedef struct { float *pos, *nor, *uv, *col; uint32_t *idx; int nv, ni, cv, ci; } sink_t;
static sink_t *sinks[NCX * NCZ * MC_COUNT];
static inline int cell_of(float x, float z) {
  int cx = (int)floorf((x - GX0) / CELL), cz = (int)floorf((z - GZ0) / CELL);
  cx = cx < 0 ? 0 : cx >= NCX ? NCX - 1 : cx; cz = cz < 0 ? 0 : cz >= NCZ ? NCZ - 1 : cz;
  return cz * NCX + cx;
}
#define KCELL 30.f
#define KNX (NCX * 3)
#define KNZ (NCZ * 3)
static sink_t *ksinks[KNX * KNZ];
static sink_t *kit_sink(float x, float z) {
  int cx = (int)floorf((x - GX0) / KCELL), cz = (int)floorf((z - GZ0) / KCELL);
  cx = cx < 0 ? 0 : cx >= KNX ? KNX - 1 : cx; cz = cz < 0 ? 0 : cz >= KNZ ? KNZ - 1 : cz;
  sink_t **s = &ksinks[cz * KNX + cx];
  if (!*s) *s = calloc(1, sizeof(sink_t));
  return *s;
}
static sink_t *sink_at(int cell, int mc) {
  sink_t **s = &sinks[cell * MC_COUNT + mc];
  if (!*s) *s = calloc(1, sizeof(sink_t));
  return *s;
}
static int s_v(sink_t *b, float x, float y, float z, float nx, float ny, float nz, float u, float v, float r, float g, float bl) {
  if (b->nv == b->cv) {
    b->cv = b->cv ? b->cv * 2 : 512;
    b->pos = realloc(b->pos, sizeof(float) * 3 * (size_t)b->cv);
    b->nor = realloc(b->nor, sizeof(float) * 3 * (size_t)b->cv);
    b->uv = realloc(b->uv, sizeof(float) * 2 * (size_t)b->cv);
    b->col = realloc(b->col, sizeof(float) * 3 * (size_t)b->cv);
  }
  int i = b->nv++;
  float *P = b->pos + i * 3, *N = b->nor + i * 3, *U = b->uv + i * 2, *C = b->col + i * 3;
  P[0] = x; P[1] = y; P[2] = z; N[0] = nx; N[1] = ny; N[2] = nz; U[0] = u; U[1] = v; C[0] = r; C[1] = g; C[2] = bl;
  return i;
}
static void s_t(sink_t *b, int a, int c, int d) {
  if (b->ni + 3 > b->ci) { b->ci = b->ci ? b->ci * 2 : 1536; b->idx = realloc(b->idx, sizeof(uint32_t) * (size_t)b->ci); }
  b->idx[b->ni++] = (uint32_t)a; b->idx[b->ni++] = (uint32_t)c; b->idx[b->ni++] = (uint32_t)d;
}
/* a quad a b c d, counter clockwise seen from the normal's side */
static void s_quad(sink_t *s, v3 a, v3 b, v3 c, v3 d, float r, float g, float bl, float us, float vs) {
  v3 n = v3_norm(v3_cross(v3_sub(b, a), v3_sub(d, a)));
  float lu = v3_len(v3_sub(b, a)) * us, lv = v3_len(v3_sub(d, a)) * vs;
  int k = s_v(s, a.x, a.y, a.z, n.x, n.y, n.z, 0, 0, r, g, bl);
  s_v(s, b.x, b.y, b.z, n.x, n.y, n.z, lu, 0, r, g, bl);
  s_v(s, c.x, c.y, c.z, n.x, n.y, n.z, lu, lv, r, g, bl);
  s_v(s, d.x, d.y, d.z, n.x, n.y, n.z, 0, lv, r, g, bl);
  s_t(s, k, k + 1, k + 2); s_t(s, k, k + 2, k + 3);
}
static void s_box(sink_t *s, float x0, float y0, float z0, float x1, float y1, float z1, t3_color c) {
  s_quad(s, v3_(x0, y0, z1), v3_(x1, y0, z1), v3_(x1, y1, z1), v3_(x0, y1, z1), c.r, c.g, c.b, 1, 1);
  s_quad(s, v3_(x1, y0, z0), v3_(x0, y0, z0), v3_(x0, y1, z0), v3_(x1, y1, z0), c.r, c.g, c.b, 1, 1);
  s_quad(s, v3_(x1, y0, z1), v3_(x1, y0, z0), v3_(x1, y1, z0), v3_(x1, y1, z1), c.r, c.g, c.b, 1, 1);
  s_quad(s, v3_(x0, y0, z0), v3_(x0, y0, z1), v3_(x0, y1, z1), v3_(x0, y1, z0), c.r, c.g, c.b, 1, 1);
  s_quad(s, v3_(x0, y1, z1), v3_(x1, y1, z1), v3_(x1, y1, z0), v3_(x0, y1, z0), c.r, c.g, c.b, 1, 1);
}

/* ── colliders ── */
typedef struct { float x0, z0, x1, z1, y0, y1; } col_box;
typedef struct { float ax, az, bx, bz, h; } col_seg;
#define MAXBOX 9000
static col_box *boxes; static int nboxes;
static col_seg *segs; static int nsegs;
static void box_add(float x0, float z0, float x1, float z1, float y0, float y1) {
  if (nboxes >= MAXBOX) return;
  boxes[nboxes++] = (col_box){x0, z0, x1, z1, y0, y1};
}
/* grid over the map, for both */
#define CG 20.f
#define CGX 46
#define CGZ 26
typedef struct { int *items; int n, cap; } bin_t;
static bin_t bin_box[CGX * CGZ], bin_seg[CGX * CGZ];
static inline int cg_x(float x) { int i = (int)floorf((x - GX0) / CG); return i < 0 ? 0 : i >= CGX ? CGX - 1 : i; }
static inline int cg_z(float z) { int i = (int)floorf((z - GZ0) / CG); return i < 0 ? 0 : i >= CGZ ? CGZ - 1 : i; }
static void bin_add(bin_t *b, int v) {
  if (b->n == b->cap) { b->cap = b->cap ? b->cap * 2 : 8; b->items = realloc(b->items, sizeof(int) * (size_t)b->cap); }
  b->items[b->n++] = v;
}
static uint32_t *st_box, *st_seg, st_now;
static void bins_build(void) {
  for (int i = 0; i < nboxes; i++) {
    col_box *c = &boxes[i];
    for (int z = cg_z(c->z0); z <= cg_z(c->z1); z++) for (int x = cg_x(c->x0); x <= cg_x(c->x1); x++) bin_add(&bin_box[z * CGX + x], i);
  }
  for (int i = 0; i < nsegs; i++) {
    col_seg *c = &segs[i];
    float x0 = fminf(c->ax, c->bx), x1 = fmaxf(c->ax, c->bx), z0 = fminf(c->az, c->bz), z1 = fmaxf(c->az, c->bz);
    for (int z = cg_z(z0); z <= cg_z(z1); z++) for (int x = cg_x(x0); x <= cg_x(x1); x++) bin_add(&bin_seg[z * CGX + x], i);
  }
  st_box = calloc((size_t)nboxes + 1, sizeof *st_box); st_seg = calloc((size_t)nsegs + 1, sizeof *st_seg);
}

/* ── capture: a tree of props into the sinks ── */
static t3_material *M_winglow_fwd;
typedef struct { int cell; bool collide; int lod; } cap_ctx;  /* lod: 0 none, 1 near props, 2 far props */
static void capture(t3_object *o, void *vctx) {
  cap_ctx *ctx = vctx;
  if (o->type != T3_MESH) return;
  t3_mesh *m = (t3_mesh *)o;
  t3_geometry *g = m->geometry;
  t3_material *mt = m->materials[0];
  const sign_rect *sign = o->user_data;
  int mc = MC_PLAIN;
  t3_color col = mt->color;
  bool uvs = false;
  if (sign) { mc = sign->neon ? MC_NEON : MC_SIGN; uvs = true; col = (t3_color){1, 1, 1}; }
  else if (mt == FAC_BRICK) { mc = MC_BRICK; uvs = true; col = (t3_color){1, 1, 1}; }
  else if (mt == M_winglow_fwd) mc = MC_WIN;
  else if (mt->type == T3_MESH_BASIC_MATERIAL) mc = MC_LAMP;
  if (ctx->lod == 2 && mc == MC_LAMP) return;      /* the near pass already put the lamps in */
  if (ctx->lod && mc != MC_LAMP) mc = ctx->lod == 1 ? MC_PROP : MC_FPROP;
  sink_t *s = sink_at(ctx->cell, mc);
  const float *e = o->matrix_world.e;
  float a = e[0], b = e[4], c = e[8], d = e[1], f = e[5], h = e[9], i = e[2], j = e[6], l = e[10];
  float A = f * l - h * j, B = -(d * l - h * i), C = d * j - f * i;
  float det = a * A + b * B + c * C;
  float inv = det != 0 ? 1.f / det : 0;
  float nm[9] = {A * inv, B * inv, C * inv, -(b * l - c * j) * inv, (a * l - c * i) * inv, -(a * j - b * i) * inv, (b * h - c * f) * inv, -(a * h - c * d) * inv, (a * f - b * d) * inv};
  const float *P = t3_attribute_f32(g->attributes[T3_ATTR_POSITION]), *N = t3_attribute_f32(g->attributes[T3_ATTR_NORMAL]);
  const float *U = g->attributes[T3_ATTR_UV] ? t3_attribute_f32(g->attributes[T3_ATTR_UV]) : NULL;
  const float *CC = mt->vertex_colors && g->attributes[T3_ATTR_COLOR] ? t3_attribute_f32(g->attributes[T3_ATTR_COLOR]) : NULL;
  int nv = g->attributes[T3_ATTR_POSITION]->count, base = s->nv;
  float bb[6] = {1e9f, 1e9f, 1e9f, -1e9f, -1e9f, -1e9f};
  for (int v = 0; v < nv; v++) {
    float x = P[v * 3], y = P[v * 3 + 1], z = P[v * 3 + 2];
    float wx = e[0] * x + e[4] * y + e[8] * z + e[12], wy = e[1] * x + e[5] * y + e[9] * z + e[13], wz = e[2] * x + e[6] * y + e[10] * z + e[14];
    bb[0] = fminf(bb[0], wx); bb[1] = fminf(bb[1], wy); bb[2] = fminf(bb[2], wz); bb[3] = fmaxf(bb[3], wx); bb[4] = fmaxf(bb[4], wy); bb[5] = fmaxf(bb[5], wz);
    float nx = N[v * 3], ny = N[v * 3 + 1], nz = N[v * 3 + 2];
    v3 n = v3_norm(v3_(nm[0] * nx + nm[1] * ny + nm[2] * nz, nm[3] * nx + nm[4] * ny + nm[5] * nz, nm[6] * nx + nm[7] * ny + nm[8] * nz));
    float u = U ? U[v * 2] : 0, w = U ? U[v * 2 + 1] : 0;
    if (sign) { u = sign->u0 + (sign->u1 - sign->u0) * (u); w = 1 - (sign->v0 + (sign->v1 - sign->v0) * (1 - w)); }
    /* vertex colours tint per channel (the ported facades write grey shades, the cars their own colours) */
    float sr = CC ? CC[v * 3] : 1, sg = CC ? CC[v * 3 + 1] : 1, sb = CC ? CC[v * 3 + 2] : 1;
    s_v(s, wx, wy, wz, n.x, n.y, n.z, uvs ? u : 0, uvs ? w : 0, col.r * sr, col.g * sg, col.b * sb);
  }
  (void)uvs;
  int ni = g->index ? g->index->count : nv;
  bool flip = det < 0;
  for (int t = 0; t + 2 < ni; t += 3) {
    uint32_t i0, i1, i2;
    if (!g->index) { i0 = t; i1 = t + 1; i2 = t + 2; }
    else if (g->index->type == T3_UINT32) { uint32_t *q = g->index->array; i0 = q[t]; i1 = q[t + 1]; i2 = q[t + 2]; }
    else { uint16_t *q = g->index->array; i0 = q[t]; i1 = q[t + 1]; i2 = q[t + 2]; }
    if (flip) s_t(s, base + i0, base + i2, base + i1); else s_t(s, base + i0, base + i1, base + i2);
  }
  if (!ctx->collide || sign) return;
  /* what a body should bump: posts and furniture below head height, long rails */
  float dx = bb[3] - bb[0], dz = bb[5] - bb[2], dy = bb[4] - bb[1];
  if (bb[1] > 1.7f || bb[4] < 0.25f) return;
  bool solid = dx > 0.22f && dz > 0.22f && dy > 0.1f;
  bool rail = (dx > 2.0f || dz > 2.0f) && fminf(dx, dz) < .3f && bb[4] > .4f;
  if (!solid && !rail) return;
  if (rail && !solid) { if (dx < dz) { bb[0] -= .05f; bb[3] += .05f; } else { bb[2] -= .05f; bb[5] += .05f; } }
  box_add(bb[0], bb[2], bb[3], bb[5], bb[1], bb[4]);
}
static float city_ground(float x, float z);
static whimsy_parts P_;
static t3_material *M_white, *M_timber, *M_teal, *M_ink, *M_wire, *M_bark, *M_leaf, *M_leaf2, *M_frond, *M_conc, *M_dirt, *M_steel, *M_gspl, *M_bronze, *M_palebark;
static car_parts CP;
static void capture_group(t3_object *g, float cx, float cz, bool collide) {
  t3_object_update_matrix_world(g, true);
  cap_ctx ctx = {cell_of(cx, cz), collide, 0};
  t3_object_traverse(g, capture, &ctx);
}

#include "facades_town.inc"

/* ── facades ── */
typedef void (*facade_fn)(t3_object *, const whimsy_parts *);
static t3_material *FAC_STUCCO;
#define W(n, call) static void w_##n(t3_object *r, const whimsy_parts *p) { call; }
W(nile, mesa_nile(r, p, FAC_BRICK)) W(level1, mesa_level_one(r, p))
W(worth, mesa_neighbor_restaurants(r, p, 0)) W(burrito, mesa_neighbor_restaurants(r, p, 1))
W(chiwas, mesa_food_shops(r, p, FAC_BRICK, 0)) W(proof, mesa_food_shops(r, p, FAC_BRICK, 1)) W(candy, mesa_food_shops(r, p, FAC_BRICK, 2))
W(nct, mesa_nct(r, p, FAC_BRICK)) W(gus, mesa_gus(r, p, FAC_BRICK)) W(dons, mesa_diamond_dons(r, p))
W(marg, mesa_margaritas(r, p)) W(dutch, mesa_lost_dutchman(r, p)) W(mangos, mesa_mangos(r, p, FAC_BRICK)) W(espiritu, mesa_espiritu(r, p))
W(copal, mesa_casa_copal(r, p)) W(haunted, mesa_east_valley_paranormal(r, p)) W(grain, mesa_against_the_grain(r, p))
W(oro, mesa_simple_venue(r, p, FAC_STUCCO, FAC_BRICK, 1)) W(chupa, town_chupacabra(r, p)) W(que, mesa_simple_venue(r, p, FAC_STUCCO, FAC_BRICK, 3))
W(fox, mesa_simple_venue(r, p, FAC_STUCCO, FAC_BRICK, 4)) W(az, mesa_simple_venue(r, p, FAC_STUCCO, FAC_BRICK, 5)) W(burger, mesa_simple_venue(r, p, FAC_STUCCO, FAC_BRICK, 6))
W(bean, mesa_simple_venue(r, p, FAC_STUCCO, FAC_BRICK, 7)) W(bri, mesa_simple_venue(r, p, FAC_STUCCO, FAC_BRICK, 8))
W(pomeroy, fk_pomeroy(r, p)) W(oldbrick, fk_old_brick(r, p)) W(butterfly, fk_butterfly(r, p)) W(cream, fk_cream_block(r, p, FAC_BRICK)) W(trellis, fk_trellis(r, p)) W(onemac, fk_one_macdonald(r, p, FAC_BRICK))
/* front: the local z of the plane that sits on the footprint's wall; w: the facade's own width */
static const struct { const char *name; facade_fn fn; float front, w; } FACADES[] = {
  {"nile", w_nile, -12, 16}, {"level1", w_level1, -20, 10.4f}, {"worth", w_worth, -12, 10}, {"burritoholics", w_burrito, -12, 10},
  {"chiwas", w_chiwas, -11.9f, 13}, {"proof", w_proof, -11.9f, 11}, {"mad_candy", w_candy, -11.0f, 13}, {"nct", w_nct, -11.93f, 13}, {"gus", w_gus, -11.93f, 15},
  {"diamond_dons", w_dons, -11.57f, 17.4f}, {"margaritas", w_marg, -11.8f, 22}, {"lost_dutchman", w_dutch, -14, 23}, {"mangos", w_mangos, -11.94f, 14}, {"espiritu", w_espiritu, -12.07f, 22},
  {"casa_copal", w_copal, -12.1f, 10.8f}, {"haunted", w_haunted, -11.38f, 26}, {"against_grain", w_grain, -11.38f, 24},
  {"oro", w_oro, -11.9f, 13.4f}, {"chupacabra", w_chupa, 0, 7.6f}, {"que_chevere", w_que, -11.9f, 13.4f}, {"phantom_fox", w_fox, -11.9f, 18.3f}, {"az_distilling", w_az, -11.9f, 13.4f},
  {"main_burger", w_burger, -11.9f, 13.4f}, {"organic_bean", w_bean, -11.9f, 13.4f}, {"bri", w_bri, -11.9f, 13.4f},
  {"pomeroy", w_pomeroy, 0, 10}, {"old_brick", w_oldbrick, 0, 10}, {"butterfly", w_butterfly, 0, 20}, {"cream_block", w_cream, 0, 21}, {"trellis", w_trellis, 0, 11.3f}, {"one_macdonald", w_onemac, 0, 40},
};
static const struct { const char *place, *label; } LABELS[] = {
  {"ribaudo", "CHIROPRACTIC"}, {"boutique", "The Boutique"}, {"naysayer", "Naysayer Tattoo"}, {"blossom", "Blossom"}, {"crismons", "CRISMON'S FLOWERS"}, {"atomic", "Atomic Vintage"},
  {"history", "History, by George!"}, {"tre_bella", "Tre Bella"}, {"oneohone", "OneOhOne Gallery"}, {"le_studio", "Le Studio"}, {"linton", "Linton Milano Music"}, {"nunthaporn", "THAI CUISINE"},
  {"drew_lofts", "THE DREW"}, {"twelve_west", "12 WEST"}, {"west_of_nile", "SHOP"}, {"booms", "SHOP"},
};
static uint32_t hash2(float a, float b) { uint32_t h = (uint32_t)((int)(a * 7) * 73856093) ^ (uint32_t)((int)(b * 11) * 19349663); h ^= h >> 13; h *= 0x5bd1e995; return h ^ (h >> 15); }
static void place_group(t3_object *inner, float cx, float cz, float yaw, float sx, float front) {
  t3_object *outer = t3_group_new();
  t3_object_set_position(outer, cx, 0, cz);
  t3_object_set_rotation(outer, 0, yaw, 0);
  outer->scale = t3_v3(sx, 1, 1);
  inner->position.z = -front;
  t3_object_add(outer, inner);
  capture_group(outer, cx, cz, true);
  t3_object_remove(outer, inner);
  t3_release(outer);
}
static void build_lots(void) {
  for (int i = 0; i < CITY_NLOT; i++) {
    const city_lot *L = &CITY_LOT[i];
    t3_object *g = t3_group_new();
    float sx = 1, front = 0;
    if (L->facade) {
      int f = -1;
      for (int k = 0; k < (int)(sizeof FACADES / sizeof FACADES[0]); k++) if (!strcmp(FACADES[k].name, L->facade)) f = k;
      if (f < 0) { ds_log("city: no facade %s", L->facade); t3_release(g); continue; }
      FACADES[f].fn(g, &P_);
      sx = L->w / FACADES[f].w; front = FACADES[f].front;
    } else {
      const char *label = NULL;
      for (int k = 0; k < (int)(sizeof LABELS / sizeof LABELS[0]); k++) if (!strcmp(LABELS[k].place, L->place)) label = LABELS[k].label;
      fk_generic(g, &P_, FAC_BRICK, L->style, L->w, label, hash2(L->cx, L->cz));
    }
    place_group(g, L->cx, L->cz, L->yaw, sx, front);
    t3_release(g);
  }
  /* the lab's own front */
  t3_object *g = t3_group_new();
  fk_lab_front(g, &P_, M_white, M_timber);
  capture_group(g, 0, 0, true);
  t3_release(g);
  g = t3_group_new();
  fk_lab_back(g, &P_);
  capture_group(g, 2, -41, true);
  t3_release(g);
}

/* ── ground ── */
static void ground_rect(const city_rect *r) {
  int mc = r->type == GT_PAVER ? MC_PAVER : (r->type == GT_CONCRETE || r->type == GT_BED) ? MC_CONCRETE : MC_ASPHALT;
  float x0 = r->x0, x1 = r->x1, z0 = r->z0, z1 = r->z1;
  const float STEP = 15;
  float base[3] = {1, 1, 1};
  if (r->type == GT_LOT) { base[0] = .86f; base[1] = .85f; base[2] = .84f; }
  if (r->type == GT_BED) { base[0] = .92f; base[1] = .9f; base[2] = .86f; }
  for (float z = floorf(z0 / STEP) * STEP; z < z1; z += STEP) for (float x = floorf(x0 / STEP) * STEP; x < x1; x += STEP) {
    float qx0 = fmaxf(x, x0), qx1 = fminf(x + STEP, x1), qz0 = fmaxf(z, z0), qz1 = fminf(z + STEP, z1);
    if (qx1 <= qx0 || qz1 <= qz0) continue;
    float cx = (qx0 + qx1) * .5f, cz = (qz0 + qz1) * .5f;
    sink_t *s = sink_at(cell_of(cx, cz), mc);
    float t = .9f + (hash2(x, z) % 100) * .002f;
    float r0 = base[0] * t, g0 = base[1] * t, b0 = base[2] * t;
    const float us = mc == MC_PAVER ? 1.f / 2.2f : 1.f / 4;
    int k = s_v(s, qx0, r->y, qz1, 0, 1, 0, qx0 * us, qz1 * us, r0, g0, b0);
    s_v(s, qx1, r->y, qz1, 0, 1, 0, qx1 * us, qz1 * us, r0, g0, b0);
    s_v(s, qx1, r->y, qz0, 0, 1, 0, qx1 * us, qz0 * us, r0, g0, b0);
    s_v(s, qx0, r->y, qz0, 0, 1, 0, qx0 * us, qz0 * us, r0, g0, b0);
    s_t(s, k, k + 1, k + 2); s_t(s, k, k + 2, k + 3);
  }
}
static void build_ground(void) {
  for (int i = 0; i < CITY_NGROUND; i++) ground_rect(&CITY_GROUND[i]);
  /* curbs: a face down to the street and a pale strip on top; red along the bus stop by the lab */
  for (int i = 0; i < CITY_NCURB; i++) {
    const city_curb *c = &CITY_CURB[i];
    const float y0 = -0.15f, wd = .28f;
    float len = c->axis ? c->z1 - c->z0 : c->x1 - c->x0;
    for (float a = 0; a < len; a += 30) {
      float b = fminf(a + 30, len);
      float cx = c->axis ? c->x0 : c->x0 + (a + b) * .5f, cz = c->axis ? c->z0 + (a + b) * .5f : c->z0;
      sink_t *s = sink_at(cell_of(cx, cz), MC_PLAIN);
      bool red = !c->axis && c->z0 < 10 && c->z0 > 5 && cx > -32 && cx < -2;
      t3_color top = red ? lin(.72f, .2f, .15f) : lin(.74f, .72f, .67f);
      if (c->axis) {
        float x = c->x0, in = -c->dir * wd;   /* dir: the side of the street away from the raised ground */
        if (c->dir > 0) s_quad(s, v3_(x, y0, c->z0 + a), v3_(x, y0, c->z0 + b), v3_(x, .005f, c->z0 + b), v3_(x, .005f, c->z0 + a), top.r * .8f, top.g * .8f, top.b * .8f, 1, 1);
        else s_quad(s, v3_(x, y0, c->z0 + b), v3_(x, y0, c->z0 + a), v3_(x, .005f, c->z0 + a), v3_(x, .005f, c->z0 + b), top.r * .8f, top.g * .8f, top.b * .8f, 1, 1);
        float xa = fminf(x, x - in), xb = fmaxf(x, x - in);
        s_quad(s, v3_(xa, .012f, c->z0 + b), v3_(xb, .012f, c->z0 + b), v3_(xb, .012f, c->z0 + a), v3_(xa, .012f, c->z0 + a), top.r, top.g, top.b, 1, 1);
      } else {
        float z = c->z0, in = -c->dir * wd;
        if (c->dir < 0) s_quad(s, v3_(c->x0 + a, y0, z), v3_(c->x0 + b, y0, z), v3_(c->x0 + b, .005f, z), v3_(c->x0 + a, .005f, z), top.r * .8f, top.g * .8f, top.b * .8f, 1, 1);
        else s_quad(s, v3_(c->x0 + b, y0, z), v3_(c->x0 + a, y0, z), v3_(c->x0 + a, .005f, z), v3_(c->x0 + b, .005f, z), top.r * .8f, top.g * .8f, top.b * .8f, 1, 1);
        float za = fminf(z, z - in), zb = fmaxf(z, z - in);
        s_quad(s, v3_(c->x0 + a, .012f, zb), v3_(c->x0 + b, .012f, zb), v3_(c->x0 + b, .012f, za), v3_(c->x0 + a, .012f, za), top.r, top.g, top.b, 1, 1);
      }
    }
  }
  /* paint */
  t3_color white = lin(.8f, .8f, .76f), yellow = lin(.84f, .7f, .2f);
  for (int i = 0; i < CITY_NSTRIPE; i++) {
    const city_stripe *q = &CITY_STRIPE[i];
    float cx = (q->x0 + q->x1) * .5f, cz = (q->z0 + q->z1) * .5f;
    float gy = city_floor_y(cx, cz, 0.05f) + .025f;
    sink_t *s = sink_at(cell_of(cx, cz), MC_PLAIN);
    t3_color c = q->c ? yellow : white;
    s_quad(s, v3_(q->x0, gy, q->z1), v3_(q->x1, gy, q->z1), v3_(q->x1, gy, q->z0), v3_(q->x0, gy, q->z0), c.r, c.g, c.b, 1, 1);
  }
  /* the rails: four steel strips down the bed, bending out around the station's platform */
  t3_color steel = lin(.52f, .5f, .47f), slot = lin(.28f, .27f, .26f);
  for (float x = CITY_X0 - 40; x < CITY_X1 + 40; x += 15) {
    #define RAILS(xx) (fabsf((xx) - 352) < 54 ? 2.0f : fabsf((xx) - 352) < 74 ? 2.0f * (74 - fabsf((xx) - 352)) / 20 : 0)
    float o0 = RAILS(x), o1 = RAILS(x + 15);
    sink_t *s = sink_at(cell_of(x + 7, MAIN_CENTER), MC_PLAIN);
    for (int t = -1; t <= 1; t += 2) for (int r = -1; r <= 1; r += 2) {
      float z0 = MAIN_CENTER + t * (2.2f + o0) + r * .72f, z1 = MAIN_CENTER + t * (2.2f + o1) + r * .72f;
      s_quad(s, v3_(x, -.075f, z0 + .05f), v3_(x + 15, -.075f, z1 + .05f), v3_(x + 15, -.075f, z1 - .05f), v3_(x, -.075f, z0 - .05f), steel.r, steel.g, steel.b, 1, 1);
      s_quad(s, v3_(x, -.08f, z0 + .09f), v3_(x + 15, -.08f, z1 + .09f), v3_(x + 15, -.08f, z1 + .05f), v3_(x, -.08f, z0 + .05f), slot.r, slot.g, slot.b, 1, 1);
    }
    /* overhead wires: a sagging pair per track */
    t3_color w = lin(.1f, .1f, .1f);
    for (int t = -1; t <= 1; t += 2) { float z = MAIN_CENTER + t * 2.2f; s_box(s, x, 7.0f, z - .012f, x + 15, 7.03f, z + .012f, w); s_box(s, x, 7.85f, z - .012f, x + 15, 7.875f, z + .012f, w); }
  }
  /* the brick strips along the bed's edges */
  for (int t = -1; t <= 1; t += 2) for (float x = CITY_X0 - 40; x < CITY_X1 + 40; x += 15) {
    bool cross = false;
    float mx = x + 7.5f;
    if (fabsf(mx + 205) < 9 || fabsf(mx - 37) < 9 || fabsf(mx - 279) < 11) cross = true;
    if (cross) continue;
    float z0 = MAIN_CENTER + t * MAIN_BED_HALF, z1 = z0 + t * MAIN_STRIP;
    float a = fminf(z0, z1), b = fmaxf(z0, z1);
    sink_t *s = sink_at(cell_of(mx, a), MC_PAVER);
    float y = -.09f;
    int k = s_v(s, x, y, b, 0, 1, 0, x * .45f, b * .45f, .82f, .74f, .72f);
    s_v(s, x + 15, y, b, 0, 1, 0, (x + 15) * .45f, b * .45f, .82f, .74f, .72f);
    s_v(s, x + 15, y, a, 0, 1, 0, (x + 15) * .45f, a * .45f, .82f, .74f, .72f);
    s_v(s, x, y, a, 0, 1, 0, x * .45f, a * .45f, .82f, .74f, .72f);
    s_t(s, k, k + 1, k + 2); s_t(s, k, k + 2, k + 3);
  }
  /* the station's platform in the median */
  sink_t *s = sink_at(cell_of(352, MAIN_CENTER), MC_CONCRETE);
  float x0 = 305, x1 = 399, z0 = MAIN_CENTER - 2.7f, z1 = MAIN_CENTER + 2.7f, y = .2f;
  for (float x = x0; x < x1; x += 15) {
    float q1 = fminf(x + 15, x1);
    int k = s_v(s, x, y, z1, 0, 1, 0, x * .25f, z1 * .25f, .9f, .88f, .82f);
    s_v(s, q1, y, z1, 0, 1, 0, q1 * .25f, z1 * .25f, .9f, .88f, .82f);
    s_v(s, q1, y, z0, 0, 1, 0, q1 * .25f, z0 * .25f, .9f, .88f, .82f);
    s_v(s, x, y, z0, 0, 1, 0, x * .25f, z0 * .25f, .9f, .88f, .82f);
    s_t(s, k, k + 1, k + 2); s_t(s, k, k + 2, k + 3);
  }
  sink_t *ps = sink_at(cell_of(352, MAIN_CENTER), MC_PLAIN);
  t3_color pc = lin(.78f, .76f, .7f), yl = lin(.85f, .72f, .22f);
  s_quad(ps, v3_(x0, -.1f, z1), v3_(x1, -.1f, z1), v3_(x1, y, z1), v3_(x0, y, z1), pc.r, pc.g, pc.b, 1, 1);
  s_quad(ps, v3_(x1, -.1f, z0), v3_(x0, -.1f, z0), v3_(x0, y, z0), v3_(x1, y, z0), pc.r, pc.g, pc.b, 1, 1);
  s_quad(ps, v3_(x0, -.1f, z0), v3_(x0, -.1f, z1), v3_(x0, y, z1), v3_(x0, y, z0), pc.r, pc.g, pc.b, 1, 1);
  s_quad(ps, v3_(x1, -.1f, z1), v3_(x1, -.1f, z0), v3_(x1, y, z0), v3_(x1, y, z1), pc.r, pc.g, pc.b, 1, 1);
  s_quad(ps, v3_(x0, y + .012f, z1 - .1f), v3_(x1, y + .012f, z1 - .1f), v3_(x1, y + .012f, z1 - .5f), v3_(x0, y + .012f, z1 - .5f), yl.r, yl.g, yl.b, 1, 1);
  s_quad(ps, v3_(x0, y + .012f, z0 + .5f), v3_(x1, y + .012f, z0 + .5f), v3_(x1, y + .012f, z0 + .1f), v3_(x0, y + .012f, z0 + .1f), yl.r, yl.g, yl.b, 1, 1);
}

/* ── generic extrusions ── */
#define C6_R 1.45f
#define C6_G 2.6f
#define C6_B 2.6f
static t3_color wall_colour(int style, uint32_t h) {
  /* 0 stucco warm, 1 white, 2 red brick, 3 beige office, 4 the Arts Center's lavender, 5 civic concrete, 6 tan brick (a tint on the brick map) */
  static const float C[7][3] = {{.84f, .78f, .66f}, {.93f, .93f, .9f}, {.62f, .36f, .28f}, {.78f, .73f, .62f}, {.6f, .62f, .84f}, {.72f, .7f, .65f}, {1.35f, 1.6f, 1.45f}};
  float t = .94f + (h % 16) * .008f;
  return lin(C[style][0] * t, C[style][1] * t, C[style][2] * t);
}
static bool in_lot(float x, float z) {
  for (int i = 0; i < CITY_NLOT; i++) {
    const city_lot *L = &CITY_LOT[i];
    float dx = x - L->cx, dz = z - L->cz, hw = L->w * .5f;
    bool along = fabsf(sinf(L->yaw)) < .5f;
    if (along ? (fabsf(dx) < hw + .8f && fabsf(dz) < 3.2f) : (fabsf(dz) < hw + .8f && fabsf(dx) < 3.2f)) return true;
  }
  if (x > -3.2f && x < 7 && z > -2 && z < 2) return true;
  return false;
}
/* the stretches of a wall that lots stand on, in order, as distances along it */
static int lot_cuts(float ax, float az, float ux, float uz, float nx, float nz, float L, float (*out)[2]) {
  int n = 0;
  for (int i = 0; i < CITY_NLOT && n < 16; i++) {
    const city_lot *T = &CITY_LOT[i];
    if (!T->facade) continue;
    float fx = sinf(T->yaw), fz = cosf(T->yaw);                /* the lot's outward normal */
    if (fx * nx + fz * nz < .9f) continue;
    float dx = T->cx - ax, dz = T->cz - az;
    if (fabsf(dx * nx + dz * nz) > 3.2f) continue;
    float t = dx * ux + dz * uz, hw = T->w * .5f;
    float t0 = fmaxf(0, t - hw), t1 = fminf(L, t + hw);
    if (t1 <= t0) continue;
    int k = n++;
    while (k > 0 && out[k - 1][0] > t0) { out[k][0] = out[k - 1][0]; out[k][1] = out[k - 1][1]; k--; }
    out[k][0] = t0; out[k][1] = t1;
  }
  return n;
}
/* ── the backs of the blocks ── */
static int ground_type(float x, float z) {
  for (int i = CITY_NGROUND - 1; i >= 0; i--) { const city_rect *r = &CITY_GROUND[i]; if (r->over) continue; if (x >= r->x0 && x <= r->x1 && z >= r->z0 && z <= r->z1) return r->type; }
  return GT_LOT;
}
static bool inside_building(float x, float z, float m) {
  /* the lab is its own model, not a footprint: nothing of the town goes in it */
  for (int k = 0; k < 5; k++) { if (world_in_lab(v3_(x + (k == 1 ? m : k == 2 ? -m : 0), 0, z + (k == 3 ? m : k == 4 ? -m : 0)))) return true; if (m <= 0) break; }
  for (int b = 0; b < CITY_NBUILDING; b++) {
    const city_building *B = &CITY_BUILDING[b];
    const float *P = CITY_PTS + B->first * 2;
    for (int k = 0; k < 5; k++) {
      float qx = x + (k == 1 ? m : k == 2 ? -m : 0), qz = z + (k == 3 ? m : k == 4 ? -m : 0);
      bool in = false;
      for (int i = 0, j = B->n - 1; i < B->n; j = i++) {
        float xi = P[i * 2], zi = P[i * 2 + 1], xj = P[j * 2], zj = P[j * 2 + 1];
        if ((zi > qz) != (zj > qz) && qx < (xj - xi) * (qz - zi) / (zj - zi) + xi) in = !in;
      }
      if (in) return true;
      if (m <= 0) break;
    }
  }
  return false;
}
bool city_open_ground(float x, float z) { int t = ground_type(x, z); return t != GT_ASPHALT && t != GT_BED && !inside_building(x, z, 0); }
/* places the story fights or stages in, and the lab's back (built by hand): the kit keeps out */
static bool kit_clear(float x, float z, float m) {
  static const float KEEP[][3] = {{-62, -66, 13}, {-22, -62, 7}, {3, -45, 9}, {-28, 12.5f, 7}, {-231, -42, 9}, {-219, -42, 7}};
  for (int i = 0; i < (int)(sizeof KEEP / sizeof KEEP[0]); i++) if (hypotf(x - KEEP[i][0], z - KEEP[i][1]) < KEEP[i][2] + m) return false;
  for (int i = 0; i < CITY_NPROP; i++) if (CITY_PROP[i].t == CP_CAR && hypotf(x - CITY_PROP[i].x, z - CITY_PROP[i].z) < 3.2f + m) return false;
  return true;
}
/* a box on an axis pair: along u (the wall), out along n, y up. Faces wind out (see s_quad) */
static void s_obox(sink_t *s, float cx, float cz, float y0, float y1, float ux, float uz, float w, float d, t3_color c) {
  float nx = uz, nz = -ux, hw = w * .5f, hd = d * .5f;
#define K(a, b, y) v3_(cx + ux * (a) * hw + nx * (b) * hd, (y), cz + uz * (a) * hw + nz * (b) * hd)
  s_quad(s, K(-1, 1, y0), K(-1, 1, y1), K(1, 1, y1), K(1, 1, y0), c.r, c.g, c.b, 1, 1);
  s_quad(s, K(-1, -1, y0), K(1, -1, y0), K(1, -1, y1), K(-1, -1, y1), c.r, c.g, c.b, 1, 1);
  s_quad(s, K(1, -1, y0), K(1, 1, y0), K(1, 1, y1), K(1, -1, y1), c.r, c.g, c.b, 1, 1);
  s_quad(s, K(-1, -1, y0), K(-1, -1, y1), K(-1, 1, y1), K(-1, 1, y0), c.r, c.g, c.b, 1, 1);
  s_quad(s, K(-1, -1, y1), K(1, -1, y1), K(1, 1, y1), K(-1, 1, y1), c.r, c.g, c.b, 1, 1);
#undef K
}
static void obox_collide(float cx, float cz, float y0, float y1, float ux, float uz, float w, float d) {
  float ex = fabsf(ux) * w * .5f + fabsf(uz) * d * .5f, ez = fabsf(uz) * w * .5f + fabsf(ux) * d * .5f;
  box_add(cx - ex, cz - ez, cx + ex, cz + ez, y0, y1);
}
/* along a back wall: steel doors with a step and a lamp, meters and conduit, coolers, and out in the lot
 * dumpsters and bins. Near detail only (MC_PROP); the dumpsters and condensers leave a box for far off */
static void back_kit(float ax, float az, float ux, float uz, float nx, float nz, float L, float H, uint32_t hh) {
  /* only walls that look onto a lot, a yard or an alley */
  float mx = ax + ux * L * .5f + nx * 2.5f, mz = az + uz * L * .5f + nz * 2.5f;
  if (ground_type(mx, mz) != GT_LOT || inside_building(mx, mz, 0)) return;
  static const float DOORC[4][3] = {{.42f, .44f, .46f}, {.5f, .36f, .25f}, {.2f, .32f, .26f}, {.45f, .2f, .18f}};
  t3_color frame = lin(.3f, .3f, .31f), conc = lin(.66f, .64f, .6f), gray = lin(.6f, .62f, .63f), cond = lin(.8f, .8f, .77f), fan = lin(.15f, .15f, .16f);
  t3_color dump[2] = {lin(.12f, .3f, .2f), lin(.12f, .24f, .5f)}, lid = lin(.06f, .06f, .07f), lampc = lin(1, .85f, .6f);
  int k = 0;
  for (float d = 2.5f + (hh % 4); d < L - 2; d += 12 + (hh % 8), k++) {
    uint32_t q = hh ^ (uint32_t)(k * 2654435761u);
    float px = ax + ux * d, pz = az + uz * d;
    if (!kit_clear(px + nx, pz + nz, 0)) continue;
    /* the whole group, from the cooler's spot to the bins' end, has to look onto open lot: one sample at the
     * wall's middle does not see a neighbour built against part of it (a party wall with a notch) */
    bool open = true;
    for (float t = d - 2.6f; t <= d + 4.2f && open; t += 1.f) {
      float sx = ax + ux * t + nx * .6f, sz = az + uz * t + nz * .6f;
      if (t > 0 && t < L && (inside_building(sx, sz, 0) || ground_type(sx, sz) != GT_LOT)) open = false;
    }
    if (!open) continue;
    int cell = cell_of(px, pz);
    sink_t *nr = kit_sink(px, pz), *fr = sink_at(cell, MC_FPROP), *lp = sink_at(cell, MC_LAMP);
    /* the door, its frame, step and lamp */
    const float *dc = DOORC[q % 4];
    t3_color dcol = lin(dc[0], dc[1], dc[2]);
    s_obox(nr, px + nx * .03f, pz + nz * .03f, 0, 2.2f, ux, uz, 1.0f, .06f, dcol);
    s_obox(nr, px + nx * .05f, pz + nz * .05f, 2.2f, 2.3f, ux, uz, 1.2f, .1f, frame);
    s_obox(nr, px + nx * .35f, pz + nz * .35f, 0, .15f, ux, uz, 1.5f, .7f, conc);
    s_obox(nr, px + nx * .12f, pz + nz * .12f, 2.55f, 2.75f, ux, uz, .3f, .22f, lid);
    s_obox(lp, px + nx * .2f, pz + nz * .2f, 2.48f, 2.56f, ux, uz, .22f, .14f, lampc);
    if (q % 5 < 2) s_obox(nr, px + nx * .5f, pz + nz * .5f, 2.6f, 2.68f, ux, uz, 1.6f, 1.0f, lin(.25f, .25f, .26f));   /* a small hood */
    /* meters and conduit beside it */
    if (q % 3 != 0 && d + 2 < L) {
      float ex = px + ux * 1.4f, ez = pz + uz * 1.4f;
      s_obox(nr, ex + nx * .1f, ez + nz * .1f, 1.1f, 1.9f, ux, uz, .55f, .2f, gray);
      s_obox(nr, ex + nx * .05f, ez + nz * .05f, 1.9f, fminf(H - .4f, 5.f), ux, uz, .06f, .06f, gray);
    }
    /* a cooler hung on the wall, or a condenser on a pad */
    if (q % 7 == 3 && H > 3.5f) {
      float ex = px - ux * 2.0f, ez = pz - uz * 2.0f;
      s_obox(nr, ex + nx * .35f, ez + nz * .35f, 2.4f, 3.3f, ux, uz, 1.0f, .7f, cond);
      s_obox(nr, ex + nx * .71f, ez + nz * .71f, 2.6f, 3.1f, ux, uz, .7f, .02f, fan);
    } else if (q % 7 == 5) {
      float ex = px - ux * 2.2f + nx * .7f, ez = pz - uz * 2.2f + nz * .7f;
      if (kit_clear(ex, ez, .5f) && !inside_building(ex, ez, .5f)) {
        s_obox(nr, ex, ez, 0, .1f, ux, uz, 1.2f, 1.2f, conc);
        s_obox(nr, ex, ez, .1f, 1.0f, ux, uz, .9f, .9f, cond);
        s_obox(nr, ex, ez, 1.0f, 1.04f, ux, uz, .6f, .6f, fan);
        s_obox(fr, ex, ez, 0, 1.0f, ux, uz, .9f, .9f, cond);
        obox_collide(ex, ez, 0, 1.0f, ux, uz, .9f, .9f);
      }
    }
    /* out in the lot: a dumpster, or a row of bins against the wall */
    if (q % 5 == 1 || q % 5 == 3) {
      float ex = px + ux * 2.8f + nx * 1.2f, ez = pz + uz * 2.8f + nz * 1.2f;
      if (d + 4 < L && kit_clear(ex, ez, 1) && !inside_building(ex, ez, 1.2f) && ground_type(ex, ez) == GT_LOT) {
        t3_color dcl = dump[(q >> 4) & 1];
        s_obox(nr, ex, ez, .1f, 1.15f, ux, uz, 1.9f, 1.15f, dcl);
        s_obox(nr, ex, ez, 0, .1f, ux, uz, 1.7f, 1.0f, lid);
        s_obox(nr, ex, ez, 1.15f, 1.22f, ux, uz, 1.95f, 1.2f, lid);
        s_obox(fr, ex, ez, 0, 1.2f, ux, uz, 1.9f, 1.15f, dcl);
        obox_collide(ex, ez, 0, 1.2f, ux, uz, 1.9f, 1.15f);
      }
    } else if (q % 5 == 4) {
      for (int b = 0; b < 3 + (int)(q % 3); b++) {
        float ex = px + ux * (1.6f + b * .72f) + nx * .45f, ez = pz + uz * (1.6f + b * .72f) + nz * .45f;
        if (d + 1.6f + b * .72f > L - .4f || !kit_clear(ex, ez, 0)) break;
        s_obox(nr, ex, ez, 0, 1.0f, ux, uz, .6f, .7f, (b % 3 == 2) ? lin(.12f, .26f, .5f) : lid);
        s_obox(nr, ex, ez, 1.0f, 1.06f, ux, uz, .64f, .76f, lid);
      }
      obox_collide(px + ux * 2.6f + nx * .45f, pz + uz * 2.6f + nz * .45f, 0, 1.0f, ux, uz, 2.4f, .7f);
    }
  }
}
/* rooftops: condensers and swamp coolers on curbs, a hatch, sometimes a vent stack */
static void roof_units(const city_building *B, float cx, float cz, float roofy, uint32_t h0) {
  const float *P = CITY_PTS + B->first * 2;
  float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f;
  for (int i = 0; i < B->n; i++) { x0 = fminf(x0, P[i * 2]); x1 = fmaxf(x1, P[i * 2]); z0 = fminf(z0, P[i * 2 + 1]); z1 = fmaxf(z1, P[i * 2 + 1]); }
  float area = (x1 - x0) * (z1 - z0);
  int n = area < 400 ? 1 : area < 1500 ? 2 : 3;
  /* near detail: from the street the parapets hide them; from above they show within a block */
  sink_t *pl = kit_sink(cx, cz);
  t3_color unit = lin(.78f, .78f, .75f), curb = lin(.5f, .49f, .47f), dark = lin(.2f, .2f, .21f);
  for (int k = 0; k < n; k++) {
    uint32_t q = h0 ^ (uint32_t)((k + 1) * 2246822519u);
    float x = x0 + 2 + (q % 1000) / 1000.f * fmaxf(0, x1 - x0 - 4), z = z0 + 2 + ((q >> 10) % 1000) / 1000.f * fmaxf(0, z1 - z0 - 4);
    bool in = false;
    for (int i = 0, j = B->n - 1; i < B->n; j = i++) { float xi = P[i * 2], zi = P[i * 2 + 1], xj = P[j * 2], zj = P[j * 2 + 1]; if ((zi > z) != (zj > z) && x < (xj - xi) * (z - zi) / (zj - zi) + xi) in = !in; }
    if (!in) continue;
    float w = 1.1f + (q >> 20) % 3 * .4f;
    s_box(pl, x - w * .5f - .1f, roofy, z - .7f, x + w * .5f + .1f, roofy + .25f, z + .7f, curb);
    s_box(pl, x - w * .5f, roofy + .25f, z - .55f, x + w * .5f, roofy + 1.15f, z + .55f, unit);
    s_box(pl, x - .3f, roofy + 1.15f, z - .3f, x + .3f, roofy + 1.2f, z + .3f, dark);
    if (q % 3 == 0) s_box(pl, x + w, roofy, z + 1.2f, x + w + .15f, roofy + 1.6f, z + 1.35f, dark);   /* a vent stack */
  }
}
static void build_buildings(void) {
  t3_color glassc = lin(.17f, .24f, .3f), roofc = lin(.46f, .44f, .42f), frame = lin(.14f, .14f, .15f), coping = lin(.88f, .86f, .8f);
  segs = malloc(sizeof(col_seg) * 4096);
  for (int b = 0; b < CITY_NBUILDING; b++) {
    const city_building *B = &CITY_BUILDING[b];
    const float *pts = CITY_PTS + B->first * 2;
    uint32_t h0 = hash2(pts[0], pts[1]);
    t3_color wc = wall_colour(B->style, h0);
    float cx = 0, cz = 0;
    for (int i = 0; i < B->n; i++) { cx += pts[i * 2]; cz += pts[i * 2 + 1]; }
    cx /= B->n; cz /= B->n;
    int cell = cell_of(cx, cz);
    bool brick = B->style == 2 || B->style == 6;
    sink_t *s = sink_at(cell, brick ? MC_BRICK : MC_PLAIN), *pl = sink_at(cell, MC_PLAIN), *win = sink_at(cell, MC_WIN);
    t3_color wb = B->style == 2 ? (t3_color){1, 1, 1} : B->style == 6 ? (t3_color){C6_R, C6_G, C6_B} : wc;
    float H = B->h, roofy = H - .7f;
    for (int i = 0; i < B->n; i++) {
      float ax = pts[i * 2], az = pts[i * 2 + 1], bx = pts[((i + 1) % B->n) * 2], bz = pts[((i + 1) % B->n) * 2 + 1];
      float L = hypotf(bx - ax, bz - az);
      if (L < .01f) continue;
      /* the polygon runs counter clockwise in (x, z), so outward is (dz, -dx) */
      float ux = (bx - ax) / L, uz = (bz - az) / L, nx = uz, nz = -ux;
      /* wall, seen from outside. The quad must wind counter clockwise from outside */
      if (nsegs < 4096) segs[nsegs++] = (col_seg){ax, az, bx, bz, H};
      /* where a facade stands on this wall the facade is the wall, with its recesses: leave a gap */
      float cuts[16][2]; int nc = lot_cuts(ax, az, ux, uz, nx, nz, L, cuts);
      float from = 0;
      for (int c = 0; c <= nc; c++) {
        float to = c < nc ? cuts[c][0] : L;
        if (to > from + .02f) {
          float qax = ax + ux * from, qaz = az + uz * from, qbx = ax + ux * to, qbz = az + uz * to;
          s_quad(s, v3_(qbx, 0, qbz), v3_(qax, 0, qaz), v3_(qax, H, qaz), v3_(qbx, H, qbz), wb.r, wb.g, wb.b, 1.f / .88f, 1.f / .64f);
          s_quad(pl, v3_(qbx + nx * .08f, H, qbz + nz * .08f), v3_(qax + nx * .08f, H, qaz + nz * .08f), v3_(qax + nx * .08f, H + .14f, qaz + nz * .08f), v3_(qbx + nx * .08f, H + .14f, qbz + nz * .08f), coping.r, coping.g, coping.b, 1, 1);
          s_quad(pl, v3_(qax + nx * .08f, H + .14f, qaz + nz * .08f), v3_(qax - nx * .25f, H + .14f, qaz - nz * .25f), v3_(qbx - nx * .25f, H + .14f, qbz - nz * .25f), v3_(qbx + nx * .08f, H + .14f, qbz + nz * .08f), coping.r, coping.g, coping.b, 1, 1);
        }
        if (c < nc) from = cuts[c][1];
      }
      /* the backs: doors, meters, coolers and bins where a wall looks onto a lot (see back_kit) */
      if (!CITY_FRONT[B->first + i] && L > 6 && B->style != 4) back_kit(ax, az, ux, uz, nx, nz, L, H, hash2(ax, bz));
      if (L < 3) continue;
      /* the street level: glazing where the wall faces a street and no facade stands */
      bool front = CITY_FRONT[B->first + i] && B->style != 4;
      float bay = 3.4f; int nb = (int)(L / bay);
      float off = (L - nb * bay) * .5f;
      if (front) for (int k = 0; k < nb; k++) {
        float t0 = off + k * bay + .25f, t1 = t0 + bay - .5f;
        float p0x = ax + ux * t0, p0z = az + uz * t0, p1x = ax + ux * t1, p1z = az + uz * t1;
        if (in_lot((p0x + p1x) * .5f, (p0z + p1z) * .5f)) continue;
        const float o = .03f;
        s_quad(pl, v3_(p1x + nx * o, .5f, p1z + nz * o), v3_(p0x + nx * o, .5f, p0z + nz * o), v3_(p0x + nx * o, 3.0f, p0z + nz * o), v3_(p1x + nx * o, 3.0f, p1z + nz * o), glassc.r, glassc.g, glassc.b, 1, 1);
        /* a frame across the top and a plinth */
        s_quad(pl, v3_(p1x + nx * .05f, 2.95f, p1z + nz * .05f), v3_(p0x + nx * .05f, 2.95f, p0z + nz * .05f), v3_(p0x + nx * .05f, 3.1f, p0z + nz * .05f), v3_(p1x + nx * .05f, 3.1f, p1z + nz * .05f), frame.r, frame.g, frame.b, 1, 1);
        if (hash2(p0x, p0z) % 3 == 0) s_quad(win, v3_(p1x + nx * .06f, .6f, p1z + nz * .06f), v3_(p0x + nx * .06f, .6f, p0z + nz * .06f), v3_(p0x + nx * .06f, 2.85f, p0z + nz * .06f), v3_(p1x + nx * .06f, 2.85f, p1z + nz * .06f), .85f, .55f, .25f, 1, 1);
      }
      /* upper floors */
      if (B->style == 4) {
        /* the Arts Center: lavender stucco masses with a band at each floor, and on some walls a glass box,
         * dark blue green, white mullions and deep white louvres (photos 2009 to 2026) */
        float drx, drz; bool room = interior_front_on(ax, az, bx, bz, &drx, &drz);   /* the lobby (interior.c) */
        if (L > 9 && (room || hash2(ax, az) % 5 < 3)) {
          t3_color gc = lin(.14f, .3f, .36f), wf = lin(.93f, .93f, .92f);
          float gy1 = fminf(H - 1.2f, 13.f);
          if (room) {
            /* the way in: a pair of dark glass doors in a white frame with a canopy, at the room's door */
            float td = (drx - ax) * ux + (drz - az) * uz, cx0 = ax + ux * td, cz0 = az + uz * td;
            t3_color dk = lin(.05f, .08f, .1f);
            s_quad(pl, v3_(cx0 + ux * 1.1f + nx * .09f, .02f, cz0 + uz * 1.1f + nz * .09f), v3_(cx0 - ux * 1.1f + nx * .09f, .02f, cz0 - uz * 1.1f + nz * .09f), v3_(cx0 - ux * 1.1f + nx * .09f, 2.7f, cz0 - uz * 1.1f + nz * .09f), v3_(cx0 + ux * 1.1f + nx * .09f, 2.7f, cz0 + uz * 1.1f + nz * .09f), dk.r, dk.g, dk.b, 1, 1);
            for (int k = -1; k <= 1; k++) s_obox(pl, cx0 + ux * k * 1.15f + nx * .14f, cz0 + uz * k * 1.15f + nz * .14f, 0, 2.8f, ux, uz, k ? .14f : .06f, .14f, wf);
            s_obox(pl, cx0 + nx * .14f, cz0 + nz * .14f, 2.7f, 2.9f, ux, uz, 2.44f, .16f, wf);
            s_obox(pl, cx0 + nx * 1.0f, cz0 + nz * 1.0f, 3.3f, 3.45f, ux, uz, 3.6f, 2.0f, wf);
            for (int k = -1; k <= 1; k += 2) s_obox(pl, cx0 + ux * k * .3f + nx * .2f, cz0 + uz * k * .3f + nz * .2f, 1.0f, 1.6f, ux, uz, .03f, .06f, lin(.6f, .62f, .64f));
          }
          s_quad(pl, v3_(bx + nx * .05f, .2f, bz + nz * .05f), v3_(ax + nx * .05f, .2f, az + nz * .05f), v3_(ax + nx * .05f, gy1, az + nz * .05f), v3_(bx + nx * .05f, gy1, bz + nz * .05f), gc.r, gc.g, gc.b, 1, 1);
          if (hash2(bz, ax) % 2) s_quad(win, v3_(bx + nx * .07f, .3f, bz + nz * .07f), v3_(ax + nx * .07f, .3f, az + nz * .07f), v3_(ax + nx * .07f, 3.6f, az + nz * .07f), v3_(bx + nx * .07f, 3.6f, bz + nz * .07f), .9f, .75f, .5f, 1, 1);
          for (float t = .75f; t < L - .3f; t += 1.5f) s_obox(pl, ax + ux * t + nx * .12f, az + uz * t + nz * .12f, .2f, gy1, ux, uz, .08f, .16f, wf);
          for (float yy = 3.8f; yy < gy1; yy += 1.6f) s_obox(pl, ax + ux * L * .5f + nx * .35f, az + uz * L * .5f + nz * .35f, yy, yy + .07f, ux, uz, L, .6f, wf);
          continue;
        }
        t3_color band = lin(.8f, .82f, .9f);
        for (float yy = 3.2f; yy < H - 1; yy += 4.6f) s_quad(pl, v3_(bx + nx * .04f, yy, bz + nz * .04f), v3_(ax + nx * .04f, yy, az + nz * .04f), v3_(ax + nx * .04f, yy + .5f, az + nz * .04f), v3_(bx + nx * .04f, yy + .5f, bz + nz * .04f), band.r, band.g, band.b, 1, 1);
        continue;
      }
      if (H < 7.5f) continue;
      float floorh = 3.7f;
      for (int f = front ? 1 : 0; ; f++) {
        float y0 = 1.2f + f * floorh, y1 = y0 + (B->style == 3 ? 2.6f : 1.9f);
        if (y1 > roofy) break;
        float wb2 = B->style == 3 ? 2.9f : 2.8f;
        int nw = (int)(L / wb2); float off2 = (L - nw * wb2) * .5f;
        for (int k = 0; k < nw; k++) {
          float t0 = off2 + k * wb2 + (B->style == 3 ? .05f : .55f), t1 = off2 + (k + 1) * wb2 - (B->style == 3 ? .05f : .55f);
          float p0x = ax + ux * t0, p0z = az + uz * t0, p1x = ax + ux * t1, p1z = az + uz * t1;
          t3_color gc = B->style == 3 ? lin(.16f, .35f, .4f) : glassc;
          s_quad(pl, v3_(p1x + nx * .03f, y0, p1z + nz * .03f), v3_(p0x + nx * .03f, y0, p0z + nz * .03f), v3_(p0x + nx * .03f, y1, p0z + nz * .03f), v3_(p1x + nx * .03f, y1, p1z + nz * .03f), gc.r, gc.g, gc.b, 1, 1);
          if (hash2(p0x + f, p0z - f) % 100 < 38) s_quad(win, v3_(p1x + nx * .05f, y0, p1z + nz * .05f), v3_(p0x + nx * .05f, y0, p0z + nz * .05f), v3_(p0x + nx * .05f, y1, p0z + nz * .05f), v3_(p1x + nx * .05f, y1, p1z + nz * .05f), .8f, .52f + (hash2(p1x, f) % 20) * .01f, .24f + (hash2(f, p1z) % 20) * .012f, 1, 1);
        }
      }
    }
    /* the roof */
    const unsigned short *T = CITY_TRI + B->tfirst;
    for (int t = 0; t + 2 < B->tn; t += 3) {
      const float *a = pts + T[t] * 2, *bb = pts + T[t + 1] * 2, *c = pts + T[t + 2] * 2;
      /* counter clockwise in (x, z) looks clockwise from above: reverse */
      int k = s_v(pl, a[0], roofy, a[1], 0, 1, 0, 0, 0, roofc.r, roofc.g, roofc.b);
      s_v(pl, c[0], roofy, c[1], 0, 1, 0, 0, 0, roofc.r, roofc.g, roofc.b);
      s_v(pl, bb[0], roofy, bb[1], 0, 1, 0, 0, 0, roofc.r, roofc.g, roofc.b);
      s_t(pl, k, k + 1, k + 2);
    }
    /* rooftop plant: condensers and coolers where they fit inside the outline */
    if (B->tn) roof_units(B, cx, cz, roofy, h0);
  }
}

/* the light a lamp throws on the ground: a fan, added over the street after dark */
static void pool_fan(float x, float z, float y, float R, float r, float g, float b) {
  sink_t *s = sink_at(cell_of(x, z), MC_POOL);
  int c = s_v(s, x, y, z, 0, 1, 0, 0, 0, r, g, b);
  const int N = 12;
  int first = s->nv;
  for (int i = 0; i < N; i++) { float a = i * 6.28318f / N; s_v(s, x + cosf(a) * R, y, z + sinf(a) * R, 0, 1, 0, 0, 0, 0, 0, 0); }
  for (int i = 0; i < N; i++) s_t(s, c, first + (i + 1) % N, first + i);
}
static void pool_rect(float x0, float z0, float x1, float z1, float y, float r, float g, float b, bool toward_z1) {
  /* bright at the wall side, fading across */
  float cx = (x0 + x1) * .5f, cz = (z0 + z1) * .5f;
  sink_t *s = sink_at(cell_of(cx, cz), MC_POOL);
  float a = toward_z1 ? 1 : 0, d = toward_z1 ? 0 : 1;
  int k = s_v(s, x0, y, z1, 0, 1, 0, 0, 0, r * d, g * d, b * d);
  s_v(s, x1, y, z1, 0, 1, 0, 0, 0, r * d, g * d, b * d);
  s_v(s, x1, y, z0, 0, 1, 0, 0, 0, r * a, g * a, b * a);
  s_v(s, x0, y, z0, 0, 1, 0, 0, 0, r * a, g * a, b * a);
  s_t(s, k, k + 1, k + 2); s_t(s, k, k + 2, k + 3);
}
static void build_pools(void) {
  for (int i = 0; i < CITY_NPROP; i++) {
    const city_prop *q = &CITY_PROP[i];
    if (q->t == CP_LAMP) { float lx = q->x + cosf(q->yaw) * .55f, lz = q->z - sinf(q->yaw) * .55f; pool_fan(lx, lz, city_ground(lx, lz) + .03f, 6.5f, .13f, .09f, .045f); }
    if (q->t == CP_POLE) for (int s = -1; s <= 1; s += 2) pool_fan(q->x, q->z + s * 3.2f, -.05f, 9, .06f, .05f, .04f);
  }
  /* shop light on the sidewalk in front of each storefront */
  for (int i = 0; i < CITY_NLOT; i++) {
    const city_lot *L = &CITY_LOT[i];
    if (fabsf(sinf(L->yaw)) > .5f) continue;
    float hw = L->w * .5f;
    bool north = L->cz < 19;
    if (north) pool_rect(L->cx - hw, L->cz + .1f, L->cx + hw, L->cz + 5.5f, .035f, .1f, .07f, .035f, false);
    else pool_rect(L->cx - hw, L->cz - 5.5f, L->cx + hw, L->cz - .1f, .035f, .1f, .07f, .035f, true);
  }
}

/* ── props ── */
/* far versions of the props: the same outline from a few boxes and low balls */
static t3_geometry *G_lowball;
static void far_tree(t3_object *r, uint32_t h, float s) {
  prop(r, P_.box, M_bark, 0, 1.2f * s, 0, .14f * s, 2.4f * s, .14f * s);
  for (int i = 0; i < 3; i++) {
    float a = i * 2.1f + h % 5, rr = .35f * s;
    prop(r, G_lowball, i % 2 ? M_leaf : M_leaf2, cosf(a) * rr, (2.5f + i * .45f) * s, sinf(a) * rr, .85f * s, .7f * s, .85f * s);
  }
}
/* the same car in a dozen boxes: each stretch of the body's profile a box to its mean top, the cabin's glass
 * and roof, the wheels, the lamps. Every kind keeps its length, height and outline across the switch */
static void far_car(t3_object *r, int colour, int kind) {
  t3_material *m = CP.paint[colour % 10];
  car_geom g = car_measure(kind);
  const car_profile *B = g.B;
  for (int i = 0; i + 1 < B->n; i++) {
    float x0 = B->p[i][0], x1 = B->p[i + 1][0];
    if (x1 - x0 < .1f) continue;
    float lo = fmaxf(.42f, (B->p[i][1] + B->p[i + 1][1]) * .5f), hi = (B->p[i][2] + B->p[i + 1][2]) * .5f, w = fminf(B->p[i][3], B->p[i + 1][3]);
    prop(r, P_.box, m, (x0 + x1) * .5f * g.L, g.base + (lo + hi) * .5f * g.Hb, 0, (x1 - x0) * g.L, (hi - lo) * g.Hb, 2 * w);
  }
  const float (*C)[4] = g.C->p;
  float gx0 = g.cx + (C[0][0] + .1f) * g.sx, gx1 = g.cx + (C[3][0] - .3f) * g.sx, rx0 = g.cx + C[1][0] * g.sx, rx1 = g.cx + C[2][0] * g.sx;
  prop(r, P_.box, CP.glass, (gx0 + gx1) * .5f, g.cy + 1.2f * g.ch, 0, gx1 - gx0, .52f * g.ch, 1.52f);
  prop(r, P_.box, m, (rx0 + rx1) * .5f, g.cy + 1.48f * g.ch + .005f, 0, rx1 - rx0, .05f, 1.45f);
  if (kind == 3) {   /* the bed's walls up to the beltline */
    float bx0 = -2.3f * g.L, bx1 = -.87f * g.L;
    prop(r, P_.box, m, (bx0 + bx1) * .5f, g.base + .99f * g.Hb, 0, bx1 - bx0, .38f * g.Hb, 1.8f);
  }
  for (int ax = -1; ax <= 1; ax += 2) prop(r, P_.box, CP.rubber, ax * 1.43f * g.L, g.wr, 0, 1.6f * g.wr, 2 * g.wr, 2 * (g.tr + .11f));
  const float *F = B->p[B->n - 1], *R = B->p[0];
  for (int side = -1; side <= 1; side += 2) {
    prop(r, P_.box, CP.head, 2.3f * g.L, g.base + (F[1] + .7f * (F[2] - F[1])) * g.Hb, side * (F[3] - .2f), .06f, .12f, .3f);
    prop(r, P_.box, CP.tail, -2.3f * g.L, g.base + (R[1] + .68f * (R[2] - R[1])) * g.Hb, side * (R[3] - .2f), .06f, .12f, .28f);
  }
}
static void far_lamp(t3_object *r) {
  prop(r, P_.box, M_ink, 0, 2.3f, 0, .14f, 4.6f, .14f);
  prop(r, P_.box, M_ink, .3f, 4.45f, 0, .6f, .06f, .06f);
  prop(r, P_.box, M_ink, .55f, 4.55f, 0, .4f, .16f, .4f);
  prop(r, P_.box, P_.lamp, .55f, 4.4f, 0, .24f, .12f, .24f);
}
static void far_pole(t3_object *r) {
  prop(r, P_.box, M_teal, 0, 4.5f, 0, .26f, 9, .26f);
  for (int side = -1; side <= 1; side += 2) {
    t3_mesh *a = prop(r, P_.box, M_teal, 0, 7.6f, side * 1.6f, .08f, .08f, 3.4f); t3_object_set_rotation(a, side * -.35f, 0, 0);
    prop(r, P_.box, P_.lamp, 0, 8.05f, side * 3.2f, .26f, .3f, .26f);
    prop(r, P_.box, M_teal, 0, 8.25f, side * 3.2f, .34f, .1f, .34f);
  }
}
/* parked and passing: mostly sedans and SUVs, some pickups and vans, now and then a taxi or a police car */
static void build_props(void) {
  G_lowball = t3_sphere_geometry_new(1, 6, 4);
  for (int lod = 1; lod <= 2; lod++) for (int i = 0; i < CITY_NPROP; i++) {
    const city_prop *q = &CITY_PROP[i];
    t3_object *g = t3_group_new();
    uint32_t h = hash2(q->x, q->z);
    float gy = city_floor_y(q->x, q->z, 0.4f);
    if (gy < -1e8f) gy = 0;
    float r = 0, rx = 0, rz = 0, top = 2.4f;
    bool far = lod == 2;
    switch (q->t) {
      case CP_POLE: if (far) far_pole(g); else fk_pole(g, &P_, M_teal, M_wire); r = .3f; break;
      case CP_LAMP: if (far) far_lamp(g); else { whimsy_parts pl = P_; pl.ball = G_lowball; fk_lamp(g, &pl, M_ink); } r = .18f; break;
      case CP_TREE: if (far) far_tree(g, h, .9f + q->v * .22f); else fk_tree(g, &P_, M_bark, M_leaf, M_leaf2, h, .9f + q->v * .22f); r = .2f; break;
      case CP_PALM: fk_palm(g, &P_, M_palebark, M_frond, 6.5f + (h % 5) * 1.4f, h); r = .32f; break;
      case CP_PLANTER:
        if (far) { prop(g, P_.box, M_conc, 0, .1f, 0, 3.2f, .22f, 1.6f); far_tree(g, h, .85f); }
        else fk_planter(g, &P_, M_conc, M_dirt, M_bark, M_leaf, M_leaf2, h);
        rx = 1.6f; rz = .8f; top = .24f; break;
      case CP_BENCH: if (far) prop(g, P_.box, M_gspl, 0, .5f, -.1f, 1.8f, .45f, .5f); else fk_bench(g, &P_, M_gspl); rx = .92f; rz = .3f; top = .5f; break;
      case CP_CAR: {
        int kind = (int)((h >> 9) % 5), colour = (int)((h >> 5) % 10);
        if (far) far_car(g, colour, kind); else fk_car(g, &P_, &CP, colour, kind);
        car_extent(kind, &rx, &rz, &top); break;
      }
      case CP_SIGNAL: fk_signal(g, &P_, M_teal); r = .16f; break;
      case CP_TOWER: fk_tower(g, &P_, M_steel, q->v ? 26 : 15, q->v ? 3.2f : 2.1f); r = q->v ? 3.3f : 2.2f; top = .5f; break;
      case CP_SHELTER: fk_shelter(g, &P_, M_teal, M_gspl); rx = 2.1f; rz = .3f; top = .5f; break;
      case CP_TRASH: prop(g, far ? P_.box : P_.cyl, M_gspl, 0, .5f, 0, .32f * (far ? 1.7f : 1), 1, .32f * (far ? 1.7f : 1)); if (!far) prop(g, P_.cyl, M_ink, 0, 1.03f, 0, .34f, .06f, .34f); r = .36f; top = 1.05f; break;
      case CP_STATUE: fk_statue(g, &P_, M_bronze, M_conc); rx = .62f; rz = .62f; top = 2; break;
      case CP_TRELLIS: fk_trellis_shelter(g, &P_, M_timber, M_leaf, M_teal); rx = 0; break;
      default: break;
    }
    t3_object_set_position(g, q->x, gy, q->z);
    t3_object_set_rotation(g, 0, q->yaw, 0);
    t3_object_update_matrix_world(g, true);
    cap_ctx ctx = {cell_of(q->x, q->z), false, lod};
    t3_object_traverse(g, capture, &ctx);
    t3_release(g);
    if (far) continue;
    if (q->t == CP_TRELLIS) { for (int sx = -1; sx <= 1; sx += 2) for (int sz = -1; sz <= 1; sz += 2) box_add(q->x + sx * 4.2f - .15f, q->z + sz * 1.5f - .15f, q->x + sx * 4.2f + .15f, q->z + sz * 1.5f + .15f, gy, gy + 3.7f); continue; }
    if (r > 0) box_add(q->x - r, q->z - r, q->x + r, q->z + r, gy, gy + top);
    else if (rx > 0) {
      float c = fabsf(cosf(q->yaw)), s = fabsf(sinf(q->yaw));
      float ex = rx * c + rz * s, ez = rx * s + rz * c;
      box_add(q->x - ex, q->z - ez, q->x + ex, q->z + ez, gy, gy + top);
    }
  }
}


/* ── the hand-placed set pieces (TOWN_SET in facades_town.inc) and the places they name ── */
typedef struct { char name[40]; float x, y, z, yaw; } set_place_t;
#define MAX_SET_PLACES 96
static set_place_t set_places[MAX_SET_PLACES]; static int nset_places;
static void set_place_add(const char *base, const char *suffix, float x, float y, float z, float yaw) {
  if (nset_places >= MAX_SET_PLACES) return;
  set_place_t *q = &set_places[nset_places++];
  snprintf(q->name, sizeof q->name, "%s%s", base, suffix);
  q->x = x; q->y = y; q->z = z; q->yaw = yaw;
}
/* local (along the piece, out from it) to world, as three.js turns a group by yaw */
static void set_local(const town_set *S, float lx, float lz, float *x, float *z) {
  float c = cosf(S->yaw), s = sinf(S->yaw);
  *x = S->x + lx * c + lz * s; *z = S->z - lx * s + lz * c;
}
static void set_collide(const town_set *S, float lx0, float lz0, float lx1, float lz1, float y0, float y1) {
  float xa, za, xb, zb, xc, zc, xd, zd;
  set_local(S, lx0, lz0, &xa, &za); set_local(S, lx1, lz0, &xb, &zb); set_local(S, lx1, lz1, &xc, &zc); set_local(S, lx0, lz1, &xd, &zd);
  box_add(fminf(fminf(xa, xb), fminf(xc, xd)), fminf(fminf(za, zb), fminf(zc, zd)), fmaxf(fmaxf(xa, xb), fmaxf(xc, xd)), fmaxf(fmaxf(za, zb), fmaxf(zc, zd)), y0, y1);
}
/* a wire from a to b with a sag: a ribbon seen from both sides, near only */
static void s_wire(float ax, float ay, float az, float bx, float by, float bz, float sag) {
  const int N = 5;
  sink_t *s = sink_at(cell_of((ax + bx) * .5f, (az + bz) * .5f), MC_PROP);
  for (int i = 0; i < N; i++) {
    float t0 = (float)i / N, t1 = (float)(i + 1) / N;
    v3 p0 = v3_(ax + (bx - ax) * t0, ay + (by - ay) * t0 - sag * 4 * t0 * (1 - t0), az + (bz - az) * t0);
    v3 p1 = v3_(ax + (bx - ax) * t1, ay + (by - ay) * t1 - sag * 4 * t1 * (1 - t1), az + (bz - az) * t1);
    const float w = .025f;
    s_quad(s, p0, p1, v3_(p1.x, p1.y + w, p1.z), v3_(p0.x, p0.y + w, p0.z), .05f, .05f, .05f, 1, 1);
    s_quad(s, p1, p0, v3_(p0.x, p0.y + w, p0.z), v3_(p1.x, p1.y + w, p1.z), .05f, .05f, .05f, 1, 1);
  }
}
static void build_sets(void) {
  float last_x = 0, last_z = 0, last_y = 0, last_yaw = 0; int last_run = -1;
  for (int i = 0; i < TOWN_NSET; i++) {
    const town_set *S = &TOWN_SET[i];
    if (S->kind == SET_SPOT) { if (S->name) set_place_add(S->name, "", S->x, 0, S->z, S->yaw); continue; }
    float gy = city_ground(S->x, S->z); if (gy < -1e8f) gy = 0;
    /* lod 0: drawn with the blocks; 1: near detail, with an optional far stand in */
    int lod = 0; bool collide = true;
    t3_object *g = t3_group_new(), *far = NULL;
    float yaw = S->yaw, top = 0;
    switch (S->kind) {
      case SET_PICNIC:
        ts_picnic(g, S->a); lod = 1; collide = false;
        set_collide(S, -S->a * .5f, -.36f, S->a * .5f, .36f, 0, .78f);
        if (S->name) {
          float x, z;
          set_place_add(S->name, "_table", S->x, gy + .775f, S->z, S->yaw);
          set_local(S, 0, -.65f, &x, &z); set_place_add(S->name, "_seat_a", x, gy, z, S->yaw);
          set_local(S, 0, .65f, &x, &z); set_place_add(S->name, "_seat_b", x, gy, z, S->yaw + 3.14159265f);
          set_local(S, -(S->a * .5f + .9f), 0, &x, &z); set_place_add(S->name, "_patio", x, gy, z, S->yaw + 1.5707963f);
        }
        break;
      case SET_PATIO_RAIL: ts_patio_rail(g, S->a, S->b); break;
      case SET_STOP: ts_stop(g); lod = 1; break;
      case SET_CACTUS_BED: ts_cactus_bed(g, S->a, S->b); lod = 1; far = t3_group_new(); prop(far, P_.box, M_conc, 0, .28f, 0, S->a, .56f, S->b); break;
      case SET_SAIL_MAST: ts_sail_mast(g, S->a, S->v); break;
      case SET_GLASS_CANOPY: ts_glass_canopy(g, S->a, S->b, S->v); break;
      case SET_ARROYO: ts_arroyo(g, S->a, S->b); collide = false; lod = 1; far = t3_group_new(); prop(far, P_.box, T_sand, 0, .01f, 0, S->b + 1.2f, .02f, S->a); break;
      case SET_MAC_SIGN: ts_mac_sign(g, S->a); break;
      case SET_PALM:
        prop(g, P_.cyl, M_conc, 0, .25f, 0, .9f, .5f, .9f); prop(g, P_.cyl, M_dirt, 0, .5f, 0, .8f, .02f, .8f);
        fk_palm(g, &P_, M_palebark, M_frond, S->a, (uint32_t)S->v * 977u + 13u);
        collide = false; box_add(S->x - .9f, S->z - .9f, S->x + .9f, S->z + .9f, gy, gy + .5f);
        break;
      case SET_MURAL: ts_mural(g, S->a, S->b, S->v % 3); collide = false; break;
      case SET_DUMPSTER: ts_dumpster(g, S->v); lod = 1; far = t3_group_new(); prop(far, P_.box, T_dump[S->v & 1], 0, .6f, 0, 1.9f, 1.2f, 1.15f); break;
      case SET_BINS: ts_bins(g, (int)S->a); lod = 1; break;
      case SET_BIKE_RACK: ts_bike_rack(g, (int)S->a); lod = 1; break;
      case SET_UPOLE:
        yaw = 1.5707963f; ts_upole(g, &top); lod = 1;
        far = t3_group_new(); prop(far, P_.box, T_poleW, 0, 5.25f, 0, .3f, 10.5f, .3f);
        box_add(S->x - .2f, S->z - .2f, S->x + .2f, S->z + .2f, gy, gy + 3);
        collide = false;
        if (last_run == S->v) for (int k = -1; k <= 1; k++) s_wire(last_x, last_y + top, last_z + k * 1.05f, S->x, gy + top, S->z + k * 1.05f, .55f);
        last_run = S->v; last_x = S->x; last_z = S->z; last_y = gy; last_yaw = yaw;
        break;
      default: break;
    }
    (void)last_yaw;
    for (int pass = 0; pass < (far ? 2 : 1); pass++) {
      t3_object *o = pass ? far : g;
      t3_object_set_position(o, S->x, gy, S->z);
      t3_object_set_rotation(o, 0, yaw, 0);
      t3_object_update_matrix_world(o, true);
      cap_ctx ctx = {cell_of(S->x, S->z), !pass && collide, pass ? 2 : lod};
      t3_object_traverse(o, capture, &ctx);
    }
    t3_release(g); if (far) t3_release(far);
  }
}
/* ── procedural tiles ── */
static t3_texture *tile_tex(const uint8_t *px, int n) {
  t3_texture *tx = t3_texture_new(n, n, px);
  tx->color_space = T3_SRGB_COLOR_SPACE;
  tx->wrap_s = tx->wrap_t = T3_REPEAT;
  tx->generate_mipmaps = true; tx->min_filter = T3_LINEAR_MIPMAP_LINEAR;
  return tx;
}
static uint32_t tsh(uint32_t x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; return x ^ (x >> 16); }
static t3_texture *tex_brick(void) {
  enum { N = 128 }; static uint8_t px[N * N * 4];
  for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
    bool mortar = y % 16 < 2 || (x + (y / 16 % 2) * 16) % 32 < 2;
    uint32_t h = tsh((uint32_t)(x / 32 + y / 16 * 29));
    int at = (y * N + x) * 4, v = (int)(h % 24);
    px[at] = mortar ? 166 : 138 + v; px[at + 1] = mortar ? 145 : 62 + v; px[at + 2] = mortar ? 120 : 44 + v; px[at + 3] = 255;
  }
  return tile_tex(px, N);
}
/* red and tan pavers, herringbone-ish rows: two metres a tile */
static t3_texture *tex_paver(void) {
  enum { N = 128 }; static uint8_t px[N * N * 4];
  for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
    int row = y / 8, sx = (x + (row % 2) * 8) / 16;
    bool joint = y % 8 < 1 || (x + (row % 2) * 8) % 16 < 1;
    uint32_t h = tsh((uint32_t)(sx * 31 + row * 977));
    int t = (int)(h % 30), kind = (int)(h >> 8) % 5;
    int at = (y * N + x) * 4;
    int r = kind < 2 ? 178 + t / 2 : kind < 4 ? 188 + t / 2 : 200 + t / 2, g = kind < 2 ? 122 + t / 3 : kind < 4 ? 134 + t / 3 : 152 + t / 3, b = kind < 2 ? 100 + t / 3 : kind < 4 ? 110 + t / 3 : 126 + t / 3;
    if (joint) { r = r * 7 / 10; g = g * 7 / 10; b = b * 7 / 10; }
    px[at] = (uint8_t)r; px[at + 1] = (uint8_t)g; px[at + 2] = (uint8_t)b; px[at + 3] = 255;
  }
  return tile_tex(px, N);
}
static t3_texture *tex_noise(int r, int g, int b, int amp, int joints) {
  enum { N = 128 }; uint8_t *px = malloc(N * N * 4);
  for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
    uint32_t h = tsh((uint32_t)(x + y * N)), h2 = tsh((uint32_t)((x / 8) * 37 + (y / 8) * 1009));
    int v = (int)(h % (uint32_t)amp) - amp / 2 + (int)(h2 % (uint32_t)(amp)) - amp / 2;
    if (joints && (x % 64 < 1 || y % 64 < 1)) v -= 40;
    int at = (y * N + x) * 4;
    px[at] = (uint8_t)ds_clamp((float)(r + v), 0, 255); px[at + 1] = (uint8_t)ds_clamp((float)(g + v), 0, 255); px[at + 2] = (uint8_t)ds_clamp((float)(b + v), 0, 255); px[at + 3] = 255;
  }
  t3_texture *t = tile_tex(px, N); free(px); return t;
}

/* ── the meshes ── */
typedef struct { t3_mesh *mesh; float x, z, r, reach; int mc; int tris; int cell; } chunk_t;
#define PROP_NEAR 30.f   /* a cell draws its full props while the focus is this close to its edge (90 m cells) */
static chunk_t *chunks; static int nchunks;
static t3_material *cmat[MC_COUNT];
static t3_object *croot;
static float city_night;
static t3_color city_tint = {1, 1, 1};
static const float REACH[MC_COUNT] = {390, 230, 460, 240, 460, 170, 280, 340, 400, 170, PROP_NEAR, 330, 28};
static void finish_meshes(void) {
  chunks = malloc(sizeof(chunk_t) * (NCX * NCZ * MC_COUNT + KNX * KNZ));
  long total = 0;
  for (int i = 0; i < NCX * NCZ * MC_COUNT; i++) {
    sink_t *s = sinks[i];
    if (!s || !s->ni) continue;
    int mc = i % MC_COUNT;
    t3_geometry *g = t3_geometry_new();
    set_attr(g, T3_ATTR_POSITION, t3_attribute_new(T3_FLOAT32, s->pos, s->nv, 3));
    set_attr(g, T3_ATTR_NORMAL, t3_attribute_new(T3_FLOAT32, s->nor, s->nv, 3));
    set_attr(g, T3_ATTR_COLOR, t3_attribute_new(T3_FLOAT32, s->col, s->nv, 3));
    if (mc == MC_BRICK || mc == MC_ASPHALT || mc == MC_PAVER || mc == MC_CONCRETE || mc == MC_SIGN || mc == MC_NEON) set_attr(g, T3_ATTR_UV, t3_attribute_new(T3_FLOAT32, s->uv, s->nv, 2));
    t3_attribute *ix = t3_attribute_new(T3_UINT32, s->idx, s->ni, 1);
    t3_geometry_set_index(g, ix); t3_release(ix);
    t3_geometry_compute_bounding_sphere(g);
    t3_mesh *m = t3_mesh_new(g, cmat[mc]);
    t3_object_add(croot, m);
    chunks[nchunks++] = (chunk_t){m, g->bounding_sphere.center.x, g->bounding_sphere.center.z, g->bounding_sphere.radius, REACH[mc], mc, s->ni / 3, i / MC_COUNT};
    total += s->ni / 3;
    t3_release(g); t3_release(m);
    free(s->pos); free(s->nor); free(s->uv); free(s->col); free(s->idx); free(s); sinks[i] = NULL;
  }
  for (int i = 0; i < KNX * KNZ; i++) {
    sink_t *s = ksinks[i];
    if (!s || !s->ni) { if (s) { free(s->pos); free(s->nor); free(s->uv); free(s->col); free(s->idx); free(s); ksinks[i] = NULL; } continue; }
    t3_geometry *g = t3_geometry_new();
    set_attr(g, T3_ATTR_POSITION, t3_attribute_new(T3_FLOAT32, s->pos, s->nv, 3));
    set_attr(g, T3_ATTR_NORMAL, t3_attribute_new(T3_FLOAT32, s->nor, s->nv, 3));
    set_attr(g, T3_ATTR_COLOR, t3_attribute_new(T3_FLOAT32, s->col, s->nv, 3));
    t3_attribute *ix = t3_attribute_new(T3_UINT32, s->idx, s->ni, 1);
    t3_geometry_set_index(g, ix); t3_release(ix);
    t3_geometry_compute_bounding_sphere(g);
    t3_mesh *m = t3_mesh_new(g, cmat[MC_KIT]);
    t3_object_add(croot, m);
    chunks[nchunks++] = (chunk_t){m, g->bounding_sphere.center.x, g->bounding_sphere.center.z, g->bounding_sphere.radius, REACH[MC_KIT], MC_KIT, s->ni / 3, -1};
    total += s->ni / 3;
    t3_release(g); t3_release(m);
    free(s->pos); free(s->nor); free(s->uv); free(s->col); free(s->idx); free(s); ksinks[i] = NULL;
  }
  /* a cell's far props switch on exactly where its near ones switch off: one sphere for both */
  for (int i = 0; i < nchunks; i++) if (chunks[i].mc == MC_FPROP) for (int k = 0; k < nchunks; k++) if (chunks[k].mc == MC_PROP && chunks[k].cell == chunks[i].cell) { chunks[i].x = chunks[k].x; chunks[i].z = chunks[k].z; chunks[i].r = chunks[k].r; }
  ds_log("city: %d chunks, %ld tris, %d boxes, %d segments", nchunks, total, nboxes, nsegs);
}
static t3_texture *TEX_BRICK;
static t3_material *lit(bool mapped, t3_texture *tx) {
  t3_material *m = t3_mesh_lambert_material_new(0xffffff);
  m->vertex_colors = true;
  if (mapped) t3_material_set_map(m, tx);
  return m;
}
/* a tree of props into meshes of its own under parent: lit, lamps (follow the clock) and lit windows */
static t3_material *M_winglow;
static void private_meshes(t3_object *src, t3_object *parent) {
  t3_object_update_matrix_world(src, true);
  sink_t *keep[MC_COUNT];
  for (int mc = 0; mc < MC_COUNT; mc++) { keep[mc] = sinks[mc]; sinks[mc] = NULL; }
  cap_ctx ctx = {0, false, 0};
  t3_object_traverse(src, capture, &ctx);
  for (int mc = 0; mc < MC_COUNT; mc++) {
    sink_t *k = sinks[mc];
    sinks[mc] = keep[mc];
    if (!k) continue;
    if (k->ni) {
      t3_geometry *G = t3_geometry_new();
      set_attr(G, T3_ATTR_POSITION, t3_attribute_new(T3_FLOAT32, k->pos, k->nv, 3));
      set_attr(G, T3_ATTR_NORMAL, t3_attribute_new(T3_FLOAT32, k->nor, k->nv, 3));
      set_attr(G, T3_ATTR_COLOR, t3_attribute_new(T3_FLOAT32, k->col, k->nv, 3));
      t3_attribute *ix = t3_attribute_new(T3_UINT32, k->idx, k->ni, 1); t3_geometry_set_index(G, ix); t3_release(ix);
      t3_geometry_compute_bounding_sphere(G);
      t3_mesh *m = t3_mesh_new(G, cmat[mc == MC_LAMP || mc == MC_WIN ? mc : MC_PLAIN]);
      t3_object_add(parent, m); t3_release(m); t3_release(G);
    }
    free(k->pos); free(k->nor); free(k->uv); free(k->col); free(k->idx); free(k);
  }
}

/* ── things that move: the light rail and cars in the through lanes ── */
#define STOP_X 352.f            /* Center and Main station, the middle of the platform */
typedef enum { TR_WAIT, TR_RUN, TR_DWELL } tr_state;
static struct { t3_object *g; float x, dir, v, wait; tr_state st; bool stopped; float bell_t; } TR;
static int bell_q; static v3 bell_at;
static void bell(float x, float z) { bell_q = 1; bell_at = v3_(x, 2.5f, z); }
static void build_train(void) {
  /* a two section car; east along the south track, back west along the north one */
  TR.g = t3_group_new();
  t3_object *g = t3_group_new();
  t3_material *body = toy_paint(.86f, .88f, .88f), *band = toy_paint(.1f, .42f, .46f), *gl = toy_paint(.1f, .14f, .17f), *dark = toy_paint(.1f, .1f, .11f);
  for (int s = -1; s <= 1; s += 2) {
    float x = s * 7.5f;
    prop(g, P_.box, body, x, 2.0f, 0, 13.5f, 2.6f, 2.65f);
    prop(g, P_.box, dark, x, .55f, 0, 12.5f, .7f, 2.4f);
    prop(g, P_.box, band, x, 1.1f, 0, 13.55f, .5f, 2.7f);
    prop(g, P_.box, gl, x, 2.35f, 0, 13.0f, 1.1f, 2.7f);
    for (int k = -2; k <= 2; k++) prop(g, P_.box, body, x + k * 2.6f, 2.35f, 0, .12f, 1.1f, 2.72f);
    /* the cabin lights in the windows: lit after dark */
    for (int k = -2; k < 2; k++) for (int z = -1; z <= 1; z += 2) prop(g, P_.box, M_winglow, x + k * 2.6f + 1.3f, 2.35f, z * 1.365f, 2.3f, .9f, .02f);
    prop(g, P_.box, gl, x + s * 6.72f, 2.3f, 0, .06f, 1.2f, 2.2f);
    prop(g, P_.box, dark, x, 3.42f, 0, 2.5f, .25f, 1.2f);
  }
  prop(g, P_.box, dark, 0, 2, 0, 1.2f, 2.6f, 2.4f);
  t3_mesh *arm = prop(g, P_.box, dark, 0, 4.2f, 0, 2.2f, .06f, .06f); t3_object_set_rotation(arm, 0, 0, .55f);
  prop(g, P_.box, dark, 0, 4.82f, 0, .08f, .06f, 1.5f);
  /* head lamps white at one end, tail lamps red at the other, and a destination sign over each cab */
  t3_material *head = glow(1, .95f, .8f), *tail = glow(1, .08f, .05f), *dest = glow(1, .7f, .2f);
  for (int s = -1; s <= 1; s += 2) {
    for (int z = -1; z <= 1; z += 2) prop(g, P_.box, s > 0 ? head : tail, s * 14.22f, 1.3f, z * .9f, .06f, .2f, .3f);
    prop(g, P_.box, dest, s * 14.24f, 3.0f, 0, .04f, .25f, 1.4f);
  }
  private_meshes(g, TR.g);
  t3_object_add(croot, TR.g);
  t3_release(g); t3_release(body); t3_release(band); t3_release(gl); t3_release(dark); t3_release(head); t3_release(tail); t3_release(dest);
  TR.x = CITY_X0 - 60; TR.dir = 1; TR.wait = 8; TR.st = TR_WAIT; TR.v = 0;
  TR.g->visible = false;
}
#define NCAR 6
#define LANE_W 11.6f            /* westbound, the north half of Main */
#define LANE_E 27.4f            /* eastbound, the south half */
static struct { t3_object *g; float x, dir, v, vmax, wait, hx, hz; } CAR[NCAR];
static bool traffic = true;
static bool train_held;   /* a fight in town brings its own light rail (fight.c): this one waits, out of sight */
static void build_cars(void) {
  for (int i = 0; i < NCAR; i++) {
    t3_object *g = t3_group_new();
    fk_car(g, &P_, &CP, (i * 7 + 2) % 10, i % 5);
    float top; car_extent(i % 5, &CAR[i].hx, &CAR[i].hz, &top);
    CAR[i].g = t3_group_new();
    private_meshes(g, CAR[i].g);
    t3_object_add(croot, CAR[i].g);
    t3_release(g);
    CAR[i].dir = i % 2 ? -1 : 1;
    CAR[i].x = CITY_X0 + (i * 137) % (int)(CITY_X1 - CITY_X0);
    CAR[i].vmax = 9 + (i * 7 % 4); CAR[i].v = CAR[i].vmax;
    CAR[i].wait = 0;
  }
}
/* the crew's pickup, parked down the alley behind the lab, tailgate toward the back door (city_crew_truck) */
#define TRUCK_X (-4.6f)
#define TRUCK_Z (-47.4f)
static t3_object *TRUCK;
#define NSOLID 4
static int solids(col_box *out);
/* parked: a solid box like any other, while it is there (it comes and goes with the scene) */
static col_box truck_box(void) {
  float hx, hz, top; car_extent(3, &hx, &hz, &top);
  return (col_box){TRUCK_X - hx, TRUCK_Z - hz, TRUCK_X + hx, TRUCK_Z + hz, 0, top};
}
static void build_truck(void) {
  t3_object *g = t3_group_new();
  fk_car(g, &P_, &CP, 6, 3);   /* the crew's pickup */
  TRUCK = t3_group_new();
  private_meshes(g, TRUCK);
  t3_release(g);
  t3_object_set_position(TRUCK, TRUCK_X, 0, TRUCK_Z);
  t3_object_set_rotation(TRUCK, 0, 3.14159265f, 0);    /* the front west, the tailgate east */
  t3_object_add(croot, TRUCK);
}
void city_crew_truck(bool on) { if (TRUCK) TRUCK->visible = on; }
/* solids that come and go with a scene (the story's van and tables): a box around a footprint turned by yaw */
static struct { bool on; col_box c; } SOLID[NSOLID];
void city_solid(int slot, bool on, v3 at, float hx, float hz, float yaw, float top) {
  if (slot < 0 || slot >= NSOLID) return;
  float cs = fabsf(cosf(yaw)), sn = fabsf(sinf(yaw)), ex = hx * cs + hz * sn, ez = hx * sn + hz * cs;
  SOLID[slot].on = on;
  SOLID[slot].c = (col_box){at.x - ex, at.z - ez, at.x + ex, at.z + ez, at.y, at.y + top};
}
static int solids(col_box *out) {
  int n = 0;
  if (TRUCK && TRUCK->visible) out[n++] = truck_box();
  for (int i = 0; i < NSOLID; i++) if (SOLID[i].on) out[n++] = SOLID[i].c;
  return n;
}
#if defined(KH_TEST_WORLD) && defined(KH_CAR_SHOWROOM)
/* a test lineup in the clear fight lot: the five kinds near (x -62) and their far stand ins (x -74), 6 m apart */
static void car_showroom(void) {
  for (int k = 0; k < CAR_KINDS; k++) for (int f = 0; f < 2; f++) {
    t3_object *g = t3_group_new(), *o = t3_group_new();
    if (f) far_car(g, 4 + k, k); else fk_car(g, &P_, &CP, 4 + k, k);
    private_meshes(g, o); t3_release(g);
    t3_object_set_position(o, f ? -74.f : -62.f, 0, -76.f + k * 6.f);
    t3_object_add(croot, o);
  }
}
#endif
static float train_off(float x) { return fabsf(x - STOP_X) < 54 ? 2 : fabsf(x - STOP_X) < 74 ? 2 * (74 - fabsf(x - STOP_X)) / 20 : 0; }
static void movers_update(float dt, v3 focus) {
  /* the train: runs, slows into the station, dwells with the bell, runs on, and comes back the other way */
  const float VMAX = 12, ACC = 1.3f;
  if (!train_held) switch (TR.st) {
    case TR_WAIT:
      TR.wait -= dt;
      if (TR.wait <= 0) { TR.st = TR_RUN; TR.v = VMAX; TR.stopped = false; TR.bell_t = 0; }
      break;
    case TR_RUN: {
      float to_stop = (STOP_X - TR.x) * TR.dir;
      float v = VMAX;
      if (!TR.stopped && to_stop > 0) v = fminf(VMAX, sqrtf(2 * ACC * to_stop) + .3f);
      else v = fminf(VMAX, TR.v + ACC * dt);
      TR.v = v;
      TR.x += TR.dir * v * dt;
      if (!TR.stopped && (STOP_X - TR.x) * TR.dir <= 0) { TR.x = STOP_X; TR.st = TR_DWELL; TR.wait = 14; TR.v = 0; TR.stopped = true; bell(TR.x, MAIN_CENTER); }
      /* the bell coming into the station, and near whoever is watching */
      TR.bell_t -= dt;
      float near = fabsf(TR.x - focus.x);
      if (TR.bell_t <= 0 && ((!TR.stopped && to_stop > 0 && to_stop < 70) || (near < 45 && fabsf(focus.z - MAIN_CENTER) < 30))) { bell(TR.x, MAIN_CENTER); TR.bell_t = 5; }
      if ((TR.dir > 0 && TR.x > CITY_X1 + 70) || (TR.dir < 0 && TR.x < CITY_X0 - 70)) { TR.dir = -TR.dir; TR.st = TR_WAIT; TR.wait = 22; }
      break;
    }
    case TR_DWELL:
      TR.wait -= dt;
      if (TR.wait <= 0) { TR.st = TR_RUN; TR.v = 0; bell(TR.x, MAIN_CENTER); TR.bell_t = 6; }
      break;
  }
  TR.g->visible = !train_held && TR.st != TR_WAIT && hypotf(TR.x - focus.x, MAIN_CENTER - focus.z) < 420;
  t3_object_set_position(TR.g, TR.x, -.08f, MAIN_CENTER + TR.dir * (2.2f + train_off(TR.x)));
  t3_object_set_rotation(TR.g, 0, TR.dir > 0 ? 0 : DS_PI, 0);
  /* cars: steady along the through lanes, braking for someone in the road ahead, round again at the ends */
  for (int i = 0; i < NCAR; i++) {
    float lane = CAR[i].dir > 0 ? LANE_E : LANE_W, ahead = (focus.x - CAR[i].x) * CAR[i].dir;
    /* someone (the camera, the player) on this half of the road, or at its curb: wait well short of them */
    bool half = fabsf(focus.z - lane) < 5.f, onto = half && fabsf(ahead) < 6;   /* a cut put the camera on it */
    bool block = half && ahead >= 6 && ahead < 24;
    for (int k = 0; k < NCAR; k++) if (k != i && CAR[k].dir == CAR[i].dir) { float d = (CAR[k].x - CAR[i].x) * CAR[i].dir; if (d > 0 && d < 9) block = true; }
    float target = block ? 0 : CAR[i].vmax;
    CAR[i].v = target < CAR[i].v ? fmaxf(target, CAR[i].v - 7 * dt) : fminf(target, CAR[i].v + 2.5f * dt);
    CAR[i].x += CAR[i].dir * CAR[i].v * dt;
    if (CAR[i].dir > 0 && CAR[i].x > CITY_X1 + 50) CAR[i].x = CITY_X0 - 50;
    if (CAR[i].dir < 0 && CAR[i].x < CITY_X0 - 50) CAR[i].x = CITY_X1 + 50;
    CAR[i].g->visible = traffic && !onto && hypotf(CAR[i].x - focus.x, lane - focus.z) < 260;
    t3_object_set_position(CAR[i].g, CAR[i].x, -.15f, lane);
    t3_object_set_rotation(CAR[i].g, 0, CAR[i].dir > 0 ? 0 : DS_PI, 0);
  }
}
void city_traffic(bool on) { traffic = on; }
void city_train_hold(bool on) { train_held = on; }
bool city_train_bell(v3 *at) { if (!bell_q) return false; bell_q--; if (at) *at = bell_at; return true; }

void city_init(t3_object *root) {
  croot = root;
  boxes = malloc(sizeof(col_box) * MAXBOX);
  /* parts */
  P_ = (whimsy_parts){
    t3_box_geometry_new(1, 1, 1, 1, 1, 1), t3_sphere_geometry_new(1, 8, 5),
    t3_cone_geometry_new(1, 1, 8, 1, false), t3_cylinder_geometry_new(1, 1, 1, 8, 1, false),
    t3_torus_geometry_new(1, .12f, 5, 14, 6.2831853f),
    toy_paint(.92f, .12f, .2f), toy_paint(1, .96f, .85f), toy_paint(1, .72f, .12f), toy_paint(.12f, .55f, .95f), toy_paint(.12f, .55f, .25f), toy_paint(1, .36f, .65f),
    toy_paint(.42f, .23f, .12f), toy_paint(.06f, .08f, .12f), toy_paint(.2f, .3f, .36f), glow(1, .75f, .25f)};
  /* ported colours ran through a bright PBR pass; keep them off pure white */
  P_.cream->color = lin(.93f, .9f, .8f);
  FAC_PLANE = t3_plane_geometry_new(1, 1, 1, 1);
  FAC_SIGNMAT = t3_mesh_basic_material_new(0xffffff);
  FAC_BRICK = toy_paint(1, 1, 1);
  FAC_STUCCO = toy_paint(.95f, .88f, .72f);
  M_white = toy_paint(.91f, .9f, .86f); M_timber = toy_paint(.74f, .6f, .44f); M_teal = toy_paint(.1f, .42f, .4f); M_ink = toy_paint(.1f, .13f, .13f); M_wire = toy_paint(.12f, .12f, .12f);
  M_bark = toy_paint(.36f, .29f, .22f); M_leaf = toy_paint(.36f, .47f, .2f); M_leaf2 = toy_paint(.46f, .54f, .24f); M_frond = toy_paint(.3f, .47f, .2f); M_palebark = toy_paint(.5f, .42f, .34f);
  M_conc = toy_paint(.74f, .71f, .65f); M_dirt = toy_paint(.46f, .36f, .26f); M_steel = toy_paint(.8f, .82f, .84f); M_gspl = toy_paint(.13f, .33f, .22f); M_bronze = toy_paint(.36f, .25f, .15f);
  for (int i = 0; i < CAR_BODY_N; i++) CP.body[i] = car_shell(&CAR_BODY[i], false);
  for (int i = 0; i < CAR_CAB_N; i++) CP.cabin[i] = car_shell(&CAR_CAB[i], true);
  CP.disc = t3_cylinder_geometry_new(1, 1, 1, 10, 1, false);
  /* the paints you see in a Mesa lot: silver, white, black, grey, navy, red, sky blue, sage, tan, maroon */
  const float pc[10][3] = {{.66f, .68f, .7f}, {.88f, .87f, .83f}, {.08f, .08f, .1f}, {.38f, .39f, .41f}, {.14f, .2f, .33f},
                           {.55f, .08f, .08f}, {.24f, .48f, .72f}, {.42f, .5f, .4f}, {.64f, .56f, .42f}, {.36f, .1f, .14f}};
  for (int i = 0; i < 10; i++) CP.paint[i] = toy_paint(pc[i][0], pc[i][1], pc[i][2]);
  CP.glass = toy_paint(.1f, .14f, .18f); CP.rubber = toy_paint(.03f, .03f, .035f); CP.alloy = toy_paint(.55f, .57f, .6f);
  CP.head = glow(.9f, .95f, 1); CP.tail = glow(1, .05f, .03f);

  /* materials */
  size_t n = 0; uint8_t *png = ds_asset("city_signs.png", &n);
  t3_texture *signs = png ? t3_texture_load_memory(png, n) : NULL;
  free(png);
  if (signs) { signs->color_space = T3_SRGB_COLOR_SPACE; signs->generate_mipmaps = true; signs->min_filter = T3_LINEAR_MIPMAP_LINEAR; }
  cmat[MC_PLAIN] = lit(false, NULL);
  t3_texture *tb = tex_brick(), *ta = tex_noise(74, 73, 73, 16, 0), *tp = tex_paver(), *tc = tex_noise(176, 171, 160, 14, 1);
  cmat[MC_BRICK] = lit(true, tb); TEX_BRICK = tb; cmat[MC_ASPHALT] = lit(true, ta); cmat[MC_PAVER] = lit(true, tp); cmat[MC_CONCRETE] = lit(true, tc);
  for (int mc = MC_SIGN; mc <= MC_NEON; mc++) {
    t3_material *m = t3_mesh_basic_material_new(0xffffff);
    m->vertex_colors = true; m->alpha_test = .5f;
    if (signs) t3_material_set_map(m, signs);
    cmat[mc] = m;
  }
  cmat[MC_LAMP] = t3_mesh_basic_material_new(0xffffff); cmat[MC_LAMP]->vertex_colors = true;
  cmat[MC_WIN] = t3_mesh_basic_material_new(0xffffff); cmat[MC_WIN]->vertex_colors = true;
  cmat[MC_PROP] = cmat[MC_FPROP] = cmat[MC_KIT] = cmat[MC_PLAIN];
  { t3_material *m = t3_mesh_basic_material_new(0xffffff); m->vertex_colors = true; m->transparent = true; m->blending = 2; m->depth_write = false; m->opacity = 1; cmat[MC_POOL] = m; }

  M_winglow = M_winglow_fwd = glow(1, .82f, .55f);
  town_materials();
  build_ground();
  build_buildings();
  build_lots();
  build_props();
  build_sets();
  build_pools();
  build_train();
  build_cars();
  build_truck();
#if defined(KH_TEST_WORLD) && defined(KH_CAR_SHOWROOM)
  car_showroom();
#endif
  finish_meshes();
  bins_build();
  city_set_time(0, (t3_color){1, 1, 1});
}

/* the clock: lamps and neon */
void city_set_time(float night, t3_color tint) {
  city_night = night;
  city_tint = tint;
  cmat[MC_PLAIN]->color = tint; cmat[MC_BRICK]->color = tint; cmat[MC_ASPHALT]->color = tint; cmat[MC_PAVER]->color = tint; cmat[MC_CONCRETE]->color = tint;
  float sb = ds_lerp(.78f, 1.05f, night), nb = ds_lerp(.75f, 3.2f, night), lb = ds_lerp(.55f, 3.6f, night), wb = night * .9f;
  cmat[MC_SIGN]->color = (t3_color){sb, sb, sb};
  cmat[MC_NEON]->color = (t3_color){nb, nb, nb};
  cmat[MC_LAMP]->color = (t3_color){lb, lb, lb};
  cmat[MC_WIN]->color = (t3_color){wb, wb, wb};
  cmat[MC_POOL]->color = (t3_color){night, night, night};
  for (int i = 0; i < MC_COUNT; i++) if (cmat[i]) cmat[i]->version++;
}

static int draws_now, tris_now;
void city_update(float dt, v3 focus, float cap) {
  draws_now = tris_now = 0;
  for (int i = 0; i < nchunks; i++) {
    chunk_t *c = &chunks[i];
    float d = hypotf(c->x - focus.x, c->z - focus.z) - c->r;
    float reach = fminf(c->reach, cap);
    bool on = d < reach;
    if (c->mc == MC_FPROP) on = on && d >= PROP_NEAR;
    if ((c->mc == MC_WIN || c->mc == MC_POOL) && city_night < .02f) on = false;
    ((t3_object *)c->mesh)->visible = on;
    if (on) { draws_now++; tris_now += c->tris; }
  }
  movers_update(dt, focus);
  /* neon after dark hums: a slow breath, and now and then one tube stutters */
  static float nt; nt += dt;
  if (city_night > .05f) {
    float nb = ds_lerp(.75f, 3.2f, city_night) * (1 + .05f * sinf(nt * 2.1f) + (fmodf(nt, 7.3f) < .12f ? -.25f : 0));
    cmat[MC_NEON]->color = (t3_color){nb, nb, nb}; cmat[MC_NEON]->version++;
  }
}
int city_stats(int *tris) { if (tris) *tris = tris_now; return draws_now; }
#ifdef KH_TEST_WORLD
void city_debug_dump(v3 eye, v3 fwd) {
  static const char *NM[MC_COUNT] = {"plain", "brick", "asphalt", "paver", "concrete", "sign", "neon", "lamp", "win", "pool", "prop", "fprop", "kit"};
  int d[MC_COUNT] = {0}, t[MC_COUNT] = {0};
  for (int i = 0; i < nchunks; i++) if (((t3_object *)chunks[i].mesh)->visible) {
    chunk_t *c = &chunks[i]; t3_geometry *g = c->mesh->geometry; v3 cc = v3_(g->bounding_sphere.center.x, g->bounding_sphere.center.y, g->bounding_sphere.center.z), dv = v3_sub(cc, eye);
    float l = v3_len(dv); if (l > c->r && v3_dot(dv, fwd) / l < cosf(.9f) - c->r / l) continue;
    d[c->mc]++; t[c->mc] += c->tris; }
  char b[512]; int o = 0;
  for (int i = 0; i < MC_COUNT; i++) o += snprintf(b + o, sizeof b - o, "%s %d/%dk ", NM[i], d[i], t[i] / 1000);
  ds_log("   city: %s", b);
}
#endif

/* ── queries ── */
static float city_ground(float x, float z) {
  /* the platform, the bed, then the ordinary rectangles */
  float best = -1e9f; 
  for (int i = CITY_NGROUND - 1; i >= 0; i--) {
    const city_rect *r = &CITY_GROUND[i];
    if (x < r->x0 || x > r->x1 || z < r->z0 || z > r->z1) continue;
    best = r->y;
    if (r->over) break;
    break;
  }
  if (x > 305 && x < 399 && fabsf(z - MAIN_CENTER) < 2.7f) best = .2f;
  return best;
}
float city_floor_y(float x, float z, float y) {
  float best = city_ground(x, z);
  if (!boxes || !st_box) return best;
  bin_t *b = &bin_box[cg_z(z) * CGX + cg_x(x)];
  for (int k = 0; k < b->n; k++) {
    const col_box *c = &boxes[b->items[k]];
    if (x < c->x0 || x > c->x1 || z < c->z0 || z > c->z1) continue;
    if (c->y1 > best && c->y1 <= y + WORLD_STEP + .01f && (c->x1 - c->x0) * (c->z1 - c->z0) > .3f) best = c->y1;
  }
  return best;
}
/* a standing cylinder against walls and boxes: pushes p out in x and z */
void city_push(v3 *p, float r, float h, v3 *n, bool *hit) {
  if (++st_now == 0) { memset(st_box, 0, sizeof(uint32_t) * (size_t)nboxes); memset(st_seg, 0, sizeof(uint32_t) * (size_t)nsegs); st_now = 1; }
  int x0 = cg_x(p->x - r), x1 = cg_x(p->x + r), z0 = cg_z(p->z - r), z1 = cg_z(p->z + r);
  float yb = p->y + WORLD_STEP, yt = p->y + h;
  for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++) {
    bin_t *bs = &bin_seg[z * CGX + x];
    for (int k = 0; k < bs->n; k++) {
      int id = bs->items[k];
      if (st_seg[id] == st_now) continue;
      st_seg[id] = st_now;
      const col_seg *s = &segs[id];
      if (yb > s->h) continue;
      float ex = s->bx - s->ax, ez = s->bz - s->az, l2 = ex * ex + ez * ez;
      float t = ds_clamp(((p->x - s->ax) * ex + (p->z - s->az) * ez) / l2, 0, 1);
      float qx = s->ax + ex * t, qz = s->az + ez * t, dx = p->x - qx, dz = p->z - qz, d2 = dx * dx + dz * dz;
      if (d2 >= r * r) continue;
      float d = sqrtf(d2);
      float nx = d > 1e-4f ? dx / d : ez / sqrtf(l2), nz = d > 1e-4f ? dz / d : -ex / sqrtf(l2);
      p->x += nx * (r - d); p->z += nz * (r - d);
      *n = v3_(nx, 0, nz); *hit = true;
    }
    bin_t *bb = &bin_box[z * CGX + x];
    for (int k = 0; k < bb->n; k++) {
      int id = bb->items[k];
      if (st_box[id] == st_now) continue;
      st_box[id] = st_now;
      const col_box *c = &boxes[id];
      if (yb > c->y1 || yt < c->y0) continue;
      float qx = ds_clamp(p->x, c->x0, c->x1), qz = ds_clamp(p->z, c->z0, c->z1), dx = p->x - qx, dz = p->z - qz, d2 = dx * dx + dz * dz;
      if (d2 >= r * r) continue;
      if (d2 > 1e-8f) {
        float d = sqrtf(d2), nx = dx / d, nz = dz / d;
        p->x += nx * (r - d); p->z += nz * (r - d); *n = v3_(nx, 0, nz);
      } else {
        /* inside: out through the nearest side */
        float l = p->x - c->x0, rr = c->x1 - p->x, b = p->z - c->z0, f = c->z1 - p->z, m = fminf(fminf(l, rr), fminf(b, f));
        if (m == l) { p->x = c->x0 - r; *n = v3_(-1, 0, 0); }
        else if (m == rr) { p->x = c->x1 + r; *n = v3_(1, 0, 0); }
        else if (m == b) { p->z = c->z0 - r; *n = v3_(0, 0, -1); }
        else { p->z = c->z1 + r; *n = v3_(0, 0, 1); }
      }
      *hit = true;
    }
  }
  /* the parked truck and the story's big props (city_solid): out along the shortest way, as from any box */
  col_box sb[NSOLID + 1]; int ns = solids(sb);
  for (int i = 0; i < ns; i++) {
    const col_box *c = &sb[i];
    float qx = ds_clamp(p->x, c->x0, c->x1), qz = ds_clamp(p->z, c->z0, c->z1), dx = p->x - qx, dz = p->z - qz, d2 = dx * dx + dz * dz;
    if (d2 < r * r && yb <= c->y1 && yt >= c->y0) {
      if (d2 > 1e-8f) { float d = sqrtf(d2); p->x += dx / d * (r - d); p->z += dz / d * (r - d); *n = v3_(dx / d, 0, dz / d); }
      else {
        float l = p->x - c->x0, rr = c->x1 - p->x, b = p->z - c->z0, f = c->z1 - p->z, m = fminf(fminf(l, rr), fminf(b, f));
        if (m == l) { p->x = c->x0 - r; *n = v3_(-1, 0, 0); } else if (m == rr) { p->x = c->x1 + r; *n = v3_(1, 0, 0); }
        else if (m == b) { p->z = c->z0 - r; *n = v3_(0, 0, -1); } else { p->z = c->z1 + r; *n = v3_(0, 0, 1); }
      }
      *hit = true;
    }
  }
  /* the train and the moving cars push to the side, out of their lane */
  for (int i = -1; i < NCAR; i++) {
    t3_object *t = i < 0 ? TR.g : CAR[i].g;
    if (!t || !t->visible) continue;
    float cx = t->position.x, cz = t->position.z, hx = i < 0 ? 14.5f : CAR[i].hx, hz = i < 0 ? 1.4f : CAR[i].hz;
    if (fabsf(p->x - cx) < hx + r && fabsf(p->z - cz) < hz + r) { float s = p->z > cz ? 1 : -1; p->z = cz + s * (hz + r); *n = v3_(0, 0, s); *hit = true; }
  }
}
static bool ray_box(v3 o, v3 d, const col_box *c, float *t, v3 *n) {
  float tmin = 0, tmax = 1e9f; v3 nn = v3_(0, 1, 0);
  const float lo[3] = {c->x0, c->y0, c->z0}, hi[3] = {c->x1, c->y1, c->z1}, oo[3] = {o.x, o.y, o.z}, dd[3] = {d.x, d.y, d.z};
  for (int a = 0; a < 3; a++) {
    if (fabsf(dd[a]) < 1e-7f) { if (oo[a] < lo[a] || oo[a] > hi[a]) return false; continue; }
    float t0 = (lo[a] - oo[a]) / dd[a], t1 = (hi[a] - oo[a]) / dd[a], s = -1;
    if (t0 > t1) { float q = t0; t0 = t1; t1 = q; s = 1; }
    if (t0 > tmin) { tmin = t0; nn = a == 0 ? v3_(s, 0, 0) : a == 1 ? v3_(0, s, 0) : v3_(0, 0, s); }
    if (t1 < tmax) tmax = t1;
    if (tmin > tmax) return false;
  }
  *t = tmin; *n = nn;
  return true;
}
bool city_raycast(v3 o, v3 d, float max_dist, float *t_out, v3 *n_out) {
  if (++st_now == 0) { memset(st_box, 0, sizeof(uint32_t) * (size_t)nboxes); memset(st_seg, 0, sizeof(uint32_t) * (size_t)nsegs); st_now = 1; }
  float best = max_dist; v3 bn = v3_(0, 1, 0); bool any = false;
  int cx = cg_x(o.x), cz = cg_z(o.z);
  int sx = d.x > 0 ? 1 : -1, sz = d.z > 0 ? 1 : -1;
  float tdx = fabsf(d.x) > 1e-7f ? CG / fabsf(d.x) : 1e30f, tdz = fabsf(d.z) > 1e-7f ? CG / fabsf(d.z) : 1e30f;
  float bx = GX0 + (cx + (sx > 0)) * CG, bz = GZ0 + (cz + (sz > 0)) * CG;
  float tmx = fabsf(d.x) > 1e-7f ? (bx - o.x) / d.x : 1e30f, tmz = fabsf(d.z) > 1e-7f ? (bz - o.z) / d.z : 1e30f;
  for (int guard = 0; guard < 256; guard++) {
    bin_t *bs = &bin_seg[cz * CGX + cx];
    for (int k = 0; k < bs->n; k++) {
      int id = bs->items[k];
      if (st_seg[id] == st_now) continue;
      st_seg[id] = st_now;
      const col_seg *s = &segs[id];
      float ex = s->bx - s->ax, ez = s->bz - s->az;
      float den = d.x * ez - d.z * ex;
      if (fabsf(den) < 1e-7f) continue;
      float tt = ((s->ax - o.x) * ez - (s->az - o.z) * ex) / den;
      float u = ((s->ax - o.x) * d.z - (s->az - o.z) * d.x) / den;
      if (tt < 0 || tt >= best || u < 0 || u > 1) continue;
      float y = o.y + d.y * tt;
      if (y > s->h || y < -.2f) continue;
      best = tt; any = true;
      float l = sqrtf(ex * ex + ez * ez);
      bn = v3_(ez / l, 0, -ex / l);
      if (v3_dot(bn, d) > 0) bn = v3_scale(bn, -1);
    }
    bin_t *bb = &bin_box[cz * CGX + cx];
    for (int k = 0; k < bb->n; k++) {
      int id = bb->items[k];
      if (st_box[id] == st_now) continue;
      st_box[id] = st_now;
      float tt; v3 nn;
      if (ray_box(o, d, &boxes[id], &tt, &nn) && tt < best) { best = tt; bn = nn; any = true; }
    }
    float tn = fminf(tmx, tmz);
    if (any && best <= tn) break;
    if (tn > max_dist) break;
    if (tmx < tmz) { cx += sx; tmx += tdx; if (cx < 0 || cx >= CGX) break; }
    else { cz += sz; tmz += tdz; if (cz < 0 || cz >= CGZ) break; }
  }
  /* the parked truck and the story's big props, while they are there: lenses and lines of sight stop at them */
  { col_box sb[NSOLID + 1]; int ns = solids(sb);
    for (int i = 0; i < ns; i++) { float tt; v3 nn; if (ray_box(o, d, &sb[i], &tt, &nn) && tt < best) { best = tt; bn = nn; any = true; } } }
  /* and the light rail going by: a lens across the track comes in to this side of it, not into its flank */
  if (TR.g && TR.g->visible) {
    float cx = TR.g->position.x, cz = TR.g->position.z, tt; v3 nn;
    col_box c = {cx - 14.5f, cz - 1.4f, cx + 14.5f, cz + 1.4f, -.1f, 3.6f};
    if (ray_box(o, d, &c, &tt, &nn) && tt < best) { best = tt; bn = nn; any = true; }
  }
  /* the ground: only below head height of a low curb is it worth sampling */
  if (d.y < -1e-5f) {
    float t0 = (o.y - .3f) / -d.y, t1 = (o.y + .16f) / -d.y;
    if (t0 < 0) t0 = 0;
    if (t1 > best) t1 = best;
    if (t0 < t1) {
      float step = fminf(.5f, fmaxf((t1 - t0) / 32, .05f));
      for (float t = t0; t <= t1; t += step) {
        float x = o.x + d.x * t, z = o.z + d.z * t, y = o.y + d.y * t;
        if (y <= city_ground(x, z)) { best = t; bn = v3_(0, 1, 0); any = true; break; }
      }
    }
  }
  if (!any) return false;
  *t_out = best; if (n_out) *n_out = bn;
  return true;
}
bool city_place(const char *name, v3 *pos, float *yaw) {
  for (int i = 0; i < CITY_NPLACE; i++) if (!strcmp(CITY_PLACE[i].name, name)) {
    if (pos) *pos = v3_(CITY_PLACE[i].x, CITY_PLACE[i].y, CITY_PLACE[i].z);
    if (yaw) *yaw = CITY_PLACE[i].yaw;
    return true;
  }
  for (int i = 0; i < nset_places; i++) if (!strcmp(set_places[i].name, name)) {
    if (pos) *pos = v3_(set_places[i].x, set_places[i].y, set_places[i].z);
    if (yaw) *yaw = set_places[i].yaw;
    return true;
  }
  return false;
}
int city_set_place_count(void) { return nset_places; }
const char *city_set_place_name(int i) { return i >= 0 && i < nset_places ? set_places[i].name : NULL; }
bool city_lot_front(const char *place, v3 *front, float *yaw, float *width) {
  for (int i = 0; i < CITY_NLOT; i++) if (!strcmp(CITY_LOT[i].place, place)) {
    if (front) *front = v3_(CITY_LOT[i].cx, 0, CITY_LOT[i].cz);
    if (yaw) *yaw = CITY_LOT[i].yaw;
    if (width) *width = CITY_LOT[i].w;
    return true;
  }
  return false;
}
bool city_sign_uv(const char *key, float uv[4]) {
  for (int i = 0; i < SIGN_N; i++) if (!strcmp(SIGNS[i].key, key)) { uv[0] = SIGNS[i].u0; uv[1] = SIGNS[i].v0; uv[2] = SIGNS[i].u1; uv[3] = SIGNS[i].v1; return true; }
  return false;
}
t3_material *city_sign_material(void) { return cmat[MC_SIGN]; }
t3_texture *city_brick_texture(void) { return TEX_BRICK; }
