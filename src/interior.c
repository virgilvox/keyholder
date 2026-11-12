/* interior.c: the rooms behind the facades (see interior.h). ROOMS is the table: one line per interior
 * (the lot whose facade it stands behind, the door, the room's size, its look, a furnishing function and
 * its named spots). A room is built the first time someone goes in, at its real place behind the facade,
 * as a few vertex coloured meshes: walls, floor and ceiling facing in, furniture as boxes and drums,
 * lamps and screens that glow, windows that show the time of day. Its walls and furniture are its own
 * colliders; while the camera is inside, world.c draws nothing of the street.
 *
 * Room space: x across the facade (+x to the right seen from the street), z out of the building toward
 * the street (the room runs from z = -back to z = -back - d), y up from the sidewalk. To add an interior:
 * a ROOMS line, a furnish function that calls rb_* in room space, and a spots list. */
#include "interior.h"
#include "city.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

typedef struct { const char *name; float x, y, z, yaw; } spot_t;          /* room space, yaw 0 faces +z (the street) */
typedef struct room_s room_t;
typedef void (*furnish_fn)(room_t *);
typedef struct {
  const char *id, *name;          /* the lot place id (or a free id), the sign */
  float cx, cz, yaw;              /* when there is no lot: the front's centre on the wall line and the yaw it faces */
  float door_x;                   /* the door, room space x */
  float w, d, h, back;            /* inside size, and the front wall's distance behind the wall line */
  int look;                       /* LOOK_*: walls, floor, ceiling, light */
  float glaze;                    /* how much of the front is window (0 none .. 1 all but the door) */
  furnish_fn furnish;
  const spot_t *spots;
  bool memory;                    /* v2.0: only the attic's flashbacks go in (no street door): one forty, years ago */
} room_def;

/* ── the builder: sinks per material, boxes in room space, colliders ── */
enum { RM_LIT, RM_BRICK, RM_GLOW, RM_SKY, RM_SIGN, RM_N };
typedef struct { float *pos, *nor, *col, *uv; uint32_t *idx; int nv, ni, cv, ci; } sink;
typedef struct { float x0, z0, x1, z1, y0, y1; } ibox;
struct room_s {
  const room_def *def;
  float cx, cz, yaw, c, s;        /* room space to world: rotate by yaw about the front centre */
  bool built;
  bool door_ok; v3 door_out;      /* the outside door point, found once (interior_door) */
  t3_object *root;
  sink sk[RM_N];
  ibox *col; int ncol, ccol;
  int tris;
  t3_material *mat[RM_N];
};

static void sk_v(sink *b, v3 p, v3 n, float u, float v, float r, float g, float bl) {
  if (b->nv == b->cv) {
    b->cv = b->cv ? b->cv * 2 : 256;
    b->pos = realloc(b->pos, sizeof(float) * 3 * (size_t)b->cv); b->nor = realloc(b->nor, sizeof(float) * 3 * (size_t)b->cv);
    b->col = realloc(b->col, sizeof(float) * 3 * (size_t)b->cv); b->uv = realloc(b->uv, sizeof(float) * 2 * (size_t)b->cv);
  }
  int i = b->nv++;
  float *P = b->pos + i * 3, *N = b->nor + i * 3, *C = b->col + i * 3, *U = b->uv + i * 2;
  P[0] = p.x; P[1] = p.y; P[2] = p.z; N[0] = n.x; N[1] = n.y; N[2] = n.z; C[0] = r; C[1] = g; C[2] = bl; U[0] = u; U[1] = v;
}
static void sk_t(sink *b, int a, int c, int d) {
  if (b->ni + 3 > b->ci) { b->ci = b->ci ? b->ci * 2 : 768; b->idx = realloc(b->idx, sizeof(uint32_t) * (size_t)b->ci); }
  b->idx[b->ni++] = (uint32_t)a; b->idx[b->ni++] = (uint32_t)c; b->idx[b->ni++] = (uint32_t)d;
}
static v3 to_world(const room_t *R, float x, float y, float z) { return v3_(R->cx + x * R->c + z * R->s, y, R->cz - x * R->s + z * R->c); }
static v3 dir_world(const room_t *R, v3 d) { return v3_(d.x * R->c + d.z * R->s, d.y, -d.x * R->s + d.z * R->c); }
static v3 to_room(const room_t *R, v3 p) { float dx = p.x - R->cx, dz = p.z - R->cz; return v3_(dx * R->c - dz * R->s, p.y, dx * R->s + dz * R->c); }
static v3 dir_room(const room_t *R, v3 d) { return v3_(d.x * R->c - d.z * R->s, d.y, d.x * R->s + d.z * R->c); }
/* a quad a b c d in room space, counter clockwise seen from the side it faces; uv in metres */
static void rb_quad(room_t *R, int m, v3 a, v3 b, v3 c, v3 d, t3_color col, const float *uv4) {
  sink *s = &R->sk[m];
  v3 n = v3_norm(v3_cross(v3_sub(b, a), v3_sub(d, a)));
  v3 wn = dir_world(R, n);
  float lu = v3_len(v3_sub(b, a)), lv = v3_len(v3_sub(d, a));
  float u0 = uv4 ? uv4[0] : 0, v0 = uv4 ? uv4[1] : 0, u1 = uv4 ? uv4[2] : lu / .88f, v1 = uv4 ? uv4[3] : lv / .64f;
  int k = s->nv;
  sk_v(s, to_world(R, a.x, a.y, a.z), wn, u0, v0, col.r, col.g, col.b);
  sk_v(s, to_world(R, b.x, b.y, b.z), wn, u1, v0, col.r, col.g, col.b);
  sk_v(s, to_world(R, c.x, c.y, c.z), wn, u1, v1, col.r, col.g, col.b);
  sk_v(s, to_world(R, d.x, d.y, d.z), wn, u0, v1, col.r, col.g, col.b);
  sk_t(s, k, k + 1, k + 2); sk_t(s, k, k + 2, k + 3);
}
static void rb_collide(room_t *R, float x0, float y0, float z0, float x1, float y1, float z1) {
  if (R->ncol == R->ccol) { R->ccol = R->ccol ? R->ccol * 2 : 64; R->col = realloc(R->col, sizeof(ibox) * (size_t)R->ccol); }
  R->col[R->ncol++] = (ibox){fminf(x0, x1), fminf(z0, z1), fmaxf(x0, x1), fmaxf(z0, z1), fminf(y0, y1), fmaxf(y0, y1)};
}
enum { RB_SOLID = 1, RB_NOBOTTOM = 2 };
/* a box in room space; solid ones below head height become colliders */
static void rb_box(room_t *R, int m, float x0, float y0, float z0, float x1, float y1, float z1, t3_color c, int flags) {
  if (x0 > x1) { float t = x0; x0 = x1; x1 = t; }
  if (z0 > z1) { float t = z0; z0 = z1; z1 = t; }
  rb_quad(R, m, v3_(x0, y0, z1), v3_(x1, y0, z1), v3_(x1, y1, z1), v3_(x0, y1, z1), c, NULL);
  rb_quad(R, m, v3_(x1, y0, z0), v3_(x0, y0, z0), v3_(x0, y1, z0), v3_(x1, y1, z0), c, NULL);
  rb_quad(R, m, v3_(x1, y0, z1), v3_(x1, y0, z0), v3_(x1, y1, z0), v3_(x1, y1, z1), c, NULL);
  rb_quad(R, m, v3_(x0, y0, z0), v3_(x0, y0, z1), v3_(x0, y1, z1), v3_(x0, y1, z0), c, NULL);
  rb_quad(R, m, v3_(x0, y1, z1), v3_(x1, y1, z1), v3_(x1, y1, z0), v3_(x0, y1, z0), c, NULL);
  if (!(flags & RB_NOBOTTOM) && y0 > .05f) rb_quad(R, m, v3_(x0, y0, z0), v3_(x1, y0, z0), v3_(x1, y0, z1), v3_(x0, y0, z1), c, NULL);
  if (flags & RB_SOLID) rb_collide(R, x0, y0, z0, x1, y1, z1);
}
/* by centre and size, the common case */
static void rb_cbox(room_t *R, int m, float x, float y, float z, float w, float h, float d, t3_color c, int flags) {
  rb_box(R, m, x - w * .5f, y, z - d * .5f, x + w * .5f, y + h, z + d * .5f, c, flags);
}
/* an eight sided drum standing on y0 (stools, round tables, pots, columns) */
static void rb_drum(room_t *R, int m, float x, float z, float r, float y0, float y1, t3_color c, int flags) {
  const int N = 8;
  for (int i = 0; i < N; i++) {
    float a0 = i * 6.2831853f / N, a1 = (i + 1) * 6.2831853f / N;
    v3 p0 = v3_(x + cosf(a0) * r, y0, z + sinf(a0) * r), p1 = v3_(x + cosf(a1) * r, y0, z + sinf(a1) * r);
    rb_quad(R, m, p1, p0, v3_(p0.x, y1, p0.z), v3_(p1.x, y1, p1.z), c, NULL);
    rb_quad(R, m, v3_(x, y1, z), v3_(p1.x, y1, p1.z), v3_(p0.x, y1, p0.z), v3_(x, y1, z), c, NULL);
  }
  if (flags & RB_SOLID) rb_collide(R, x - r, y0, z - r, x + r, y1, z + r);
}
/* a sign from the city's atlas, facing room +z (dir 0), -z (1), +x (2) or -x (3) */
static void rb_sign(room_t *R, const char *key, float x, float y, float z, float w, float h, int dir) {
  float uv[4];
  if (!city_sign_uv(key, uv)) return;
  float u0 = uv[0], u1 = uv[2], vt = 1 - uv[1], vb = 1 - uv[3], hw = w * .5f, hh = h * .5f;
  v3 a, b, c, d;   /* bottom left, bottom right, top right, top left as seen from the front */
  if (dir == 0) { a = v3_(x - hw, y - hh, z); b = v3_(x + hw, y - hh, z); }
  else if (dir == 1) { a = v3_(x + hw, y - hh, z); b = v3_(x - hw, y - hh, z); }
  else if (dir == 2) { a = v3_(x, y - hh, z + hw); b = v3_(x, y - hh, z - hw); }
  else { a = v3_(x, y - hh, z - hw); b = v3_(x, y - hh, z + hw); }
  c = v3_(b.x, y + hh, b.z); d = v3_(a.x, y + hh, a.z);
  const float q[4] = {u0, vb, u1, vt};
  rb_quad(R, RM_SIGN, a, b, c, d, (t3_color){1, 1, 1}, q);
}
static t3_color rgb(float r, float g, float b) { return (t3_color){r * r, g * g, b * b}; }   /* about linear */
static uint32_t rhash(uint32_t x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; return x ^ (x >> 16); }

/* ── the looks: walls, floor, ceiling, and how bright the room's light is ── */
enum { LOOK_BAKERY, LOOK_TAPROOM, LOOK_ARCADE, LOOK_THEATER, LOOK_GALLERY, LOOK_GLITTER };
static const struct { float wall[3], floor[3], ceil[3], trim[3]; bool brick; float light; } LOOKS[] = {
  [LOOK_BAKERY] = {{.93f, .9f, .82f}, {.72f, .55f, .38f}, {.9f, .88f, .84f}, {.3f, .24f, .18f}, false, 1.0f},
  [LOOK_TAPROOM] = {{1, 1, 1}, {.52f, .52f, .5f}, {.42f, .3f, .2f}, {.2f, .16f, .12f}, true, .85f},
  [LOOK_ARCADE] = {{.16f, .15f, .2f}, {.2f, .14f, .3f}, {.08f, .08f, .1f}, {.05f, .05f, .06f}, false, .55f},
  [LOOK_THEATER] = {{.14f, .13f, .13f}, {.25f, .22f, .21f}, {.07f, .07f, .07f}, {.45f, .12f, .1f}, false, .68f},
  [LOOK_GALLERY] = {{.95f, .95f, .94f}, {.78f, .78f, .76f}, {.96f, .96f, .95f}, {.55f, .55f, .54f}, false, 1.05f},
  [LOOK_GLITTER] = {{.93f, .6f, .74f}, {.56f, .55f, .52f}, {.9f, .88f, .86f}, {.3f, .22f, .26f}, false, .95f},
};
/* walls, floor and ceiling facing in; the front wall with its windows (the daylight) and the door */
static void rb_shell(room_t *R) {
  const room_def *D = R->def;
  const float W = D->w * .5f, Z0 = -D->back - D->d, Z1 = -D->back, H = D->h;
  t3_color wall = rgb(LOOKS[D->look].wall[0], LOOKS[D->look].wall[1], LOOKS[D->look].wall[2]);
  t3_color fl = rgb(LOOKS[D->look].floor[0], LOOKS[D->look].floor[1], LOOKS[D->look].floor[2]);
  t3_color ce = rgb(LOOKS[D->look].ceil[0], LOOKS[D->look].ceil[1], LOOKS[D->look].ceil[2]);
  t3_color trim = rgb(LOOKS[D->look].trim[0], LOOKS[D->look].trim[1], LOOKS[D->look].trim[2]);
  int wm = LOOKS[D->look].brick ? RM_BRICK : RM_LIT;
  /* floor in tiles (a little variation), ceiling, back and side walls */
  for (float x = -W; x < W - .01f; x += 2) for (float z = Z0; z < Z1 - .01f; z += 2) {
    float x1 = fminf(x + 2, W), z1 = fminf(z + 2, Z1), k = .93f + (rhash((uint32_t)(x * 7 + 100) * 31 + (uint32_t)(z * 13 + 400)) % 14) * .01f;
    rb_quad(R, RM_LIT, v3_(x, 0, z1), v3_(x1, 0, z1), v3_(x1, 0, z), v3_(x, 0, z), (t3_color){fl.r * k, fl.g * k, fl.b * k}, NULL);
  }
  rb_quad(R, RM_LIT, v3_(-W, H, Z0), v3_(W, H, Z0), v3_(W, H, Z1), v3_(-W, H, Z1), ce, NULL);
  rb_quad(R, wm, v3_(-W, 0, Z0), v3_(W, 0, Z0), v3_(W, H, Z0), v3_(-W, H, Z0), wall, NULL);
  rb_quad(R, wm, v3_(W, 0, Z0), v3_(W, 0, Z1), v3_(W, H, Z1), v3_(W, H, Z0), wall, NULL);
  rb_quad(R, wm, v3_(-W, 0, Z1), v3_(-W, 0, Z0), v3_(-W, H, Z0), v3_(-W, H, Z1), wall, NULL);
  /* the front, seen from inside (facing -z): wall, windows of daylight, the door and its EXIT sign */
  rb_quad(R, wm, v3_(W, 0, Z1), v3_(-W, 0, Z1), v3_(-W, H, Z1), v3_(W, H, Z1), wall, NULL);
  float dx = D->door_x, gy0 = .6f, gy1 = fminf(H - .5f, 3.0f);
  if (D->glaze > 0) for (float x = -W + .4f; x < W - .4f; x += 1.6f) {
    float x1 = fminf(x + 1.4f, W - .4f);
    if (x1 > dx - .8f && x < dx + .8f) continue;
    if (fabsf((x + x1) * .5f) > W * D->glaze) continue;
    rb_quad(R, RM_SKY, v3_(x1, gy0, Z1 - .01f), v3_(x, gy0, Z1 - .01f), v3_(x, gy1, Z1 - .01f), v3_(x1, gy1, Z1 - .01f), (t3_color){1, 1, 1}, NULL);
    rb_box(R, RM_LIT, x - .05f, gy0 - .08f, Z1 - .06f, x1 + .05f, gy0, Z1, trim, 0);
  }
  rb_quad(R, RM_SKY, v3_(dx + .45f, .05f, Z1 - .012f), v3_(dx - .45f, .05f, Z1 - .012f), v3_(dx - .45f, 2.1f, Z1 - .012f), v3_(dx + .45f, 2.1f, Z1 - .012f), (t3_color){.8f, .8f, .8f}, NULL);
  rb_box(R, RM_LIT, dx - .55f, 0, Z1 - .08f, dx - .45f, 2.2f, Z1, trim, 0);
  rb_box(R, RM_LIT, dx + .45f, 0, Z1 - .08f, dx + .55f, 2.2f, Z1, trim, 0);
  rb_box(R, RM_LIT, dx - .55f, 2.1f, Z1 - .08f, dx + .55f, 2.2f, Z1, trim, 0);
  rb_box(R, RM_LIT, dx - .02f, .2f, Z1 - .05f, dx + .02f, 1.9f, Z1 - .01f, trim, 0);   /* the door's middle rail */
  rb_sign(R, "EXIT", dx, 2.45f, Z1 - .02f, .55f, .2f, 1);
  /* skirting */
  rb_box(R, RM_LIT, -W, 0, Z0, W, .1f, Z0 + .03f, trim, 0);
  rb_box(R, RM_LIT, -W, 0, Z0, -W + .03f, .1f, Z1, trim, 0);
  rb_box(R, RM_LIT, W - .03f, 0, Z0, W, .1f, Z1, trim, 0);
  /* the walls as colliders, thick, just outside; floor and ceiling slabs for the camera's rays */
  rb_collide(R, -W - .5f, -.5f, Z0 - .5f, W + .5f, H + .5f, Z0);
  rb_collide(R, -W - .5f, -.5f, Z1, W + .5f, H + .5f, Z1 + .5f);
  rb_collide(R, -W - .5f, -.5f, Z0, -W, H + .5f, Z1);
  rb_collide(R, W, -.5f, Z0, W + .5f, H + .5f, Z1);
  rb_collide(R, -W, H, Z0, W, H + .5f, Z1);
  rb_collide(R, -W, -.5f, Z0, W, -.001f, Z1);
}
/* a lamp hanging on a cord, its bulb glowing */
static void rb_pendant(room_t *R, float x, float z, float drop, t3_color shade) {
  const float H = R->def->h;
  rb_cbox(R, RM_LIT, x, H - drop, z, .02f, drop, .02f, rgb(.1f, .1f, .1f), 0);
  rb_drum(R, RM_LIT, x, z, .2f, H - drop - .2f, H - drop, shade, 0);
  rb_cbox(R, RM_GLOW, x, H - drop - .26f, z, .14f, .1f, .14f, rgb(1, .86f, .6f), 0);
}
static void rb_stool(room_t *R, float x, float z, float h, t3_color seat) {
  rb_drum(R, RM_LIT, x, z, .045f, 0, h - .05f, rgb(.15f, .15f, .16f), 0);
  rb_drum(R, RM_LIT, x, z, .2f, h - .06f, h, seat, 0);
  rb_drum(R, RM_LIT, x, z, .18f, 0, .03f, rgb(.15f, .15f, .16f), 0);
}

/* ── the rooms ── */
/* Proof Bread: a bakery counter with a pastry case, the bread wall behind it, the oven at the back,
 * cafe tables at the windows */
static void furnish_proof(room_t *R) {
  const room_def *D = R->def; const float W = D->w * .5f, Z0 = -D->back - D->d;
  t3_color wood = rgb(.62f, .45f, .28f), dark = rgb(.25f, .19f, .13f), steel = rgb(.72f, .74f, .76f), loaf = rgb(.66f, .42f, .2f), loaf2 = rgb(.5f, .3f, .14f);
  /* the counter across the room, its glass case and register */
  float cz = -6.0f;
  rb_box(R, RM_LIT, -W + .3f, 0, cz - .35f, 1.4f, .95f, cz + .35f, wood, RB_SOLID);
  rb_box(R, RM_LIT, -W + .3f, .95f, cz - .4f, 1.4f, 1.0f, cz + .4f, dark, 0);
  rb_box(R, RM_GLOW, -2.6f, 1.0f, cz - .3f, -.4f, 1.32f, cz + .3f, rgb(.72f, .66f, .55f), 0);
  for (int i = 0; i < 9; i++) rb_cbox(R, RM_LIT, -2.45f + i * .25f, 1.04f, cz + ((i % 2) ? .1f : -.1f), .16f, .08f, .14f, (i % 3) ? loaf : rgb(.9f, .78f, .5f), 0);
  rb_cbox(R, RM_LIT, .6f, 1.0f, cz, .35f, .22f, .3f, rgb(.2f, .2f, .22f), 0);
  rb_cbox(R, RM_GLOW, .6f, 1.18f, cz + .1f, .25f, .14f, .02f, rgb(.5f, .75f, .9f), 0);
  /* the bread wall: shelves of loaves, and the board over it */
  float bz = Z0 + 3.2f;
  rb_box(R, RM_LIT, -W + .3f, 0, bz - .45f, W - 1.2f, 2.6f, bz - .05f, dark, RB_SOLID);
  for (int sh = 0; sh < 5; sh++) {
    float y = .5f + sh * .45f;
    rb_box(R, RM_LIT, -W + .35f, y, bz - .05f, W - 1.25f, y + .04f, bz + .3f, wood, 0);
    for (float x = -W + .5f; x < W - 1.5f; x += .32f) rb_cbox(R, RM_LIT, x, y + .04f, bz + .12f, .26f, .14f + (rhash((uint32_t)(x * 50 + sh)) % 3) * .02f, .18f, ((int)(x * 10) + sh) % 3 ? loaf : loaf2, 0);
  }
  rb_cbox(R, RM_LIT, -.3f, 2.85f, bz - .05f, 3.6f, .9f, .05f, rgb(.12f, .12f, .12f), 0);
  rb_sign(R, "BREAD", -.3f, 2.95f, bz + .01f, 1.4f, .5f, 0);
  rb_sign(R, "proof.png", -1.7f, 2.9f, bz + .01f, .6f, .6f, 0);
  /* the deck oven at the back, flour sacks, a rack */
  rb_box(R, RM_LIT, -1.6f, 0, Z0 + .1f, 1.6f, 2.1f, Z0 + 1.4f, steel, RB_SOLID);
  for (int k = 0; k < 3; k++) { float y = .45f + k * .52f; rb_box(R, RM_LIT, -1.4f, y, Z0 + 1.4f, 1.4f, y + .36f, Z0 + 1.43f, rgb(.15f, .15f, .15f), 0); rb_box(R, RM_GLOW, -1.2f, y + .14f, Z0 + 1.43f, 1.2f, y + .2f, Z0 + 1.44f, rgb(1, .55f, .2f), 0); }
  for (int k = 0; k < 4; k++) rb_cbox(R, RM_LIT, W - .6f, k * .22f, Z0 + .6f + (k % 2) * .1f, .7f, .2f, .45f, rgb(.92f, .9f, .85f), k == 0 ? RB_SOLID : 0);
  rb_box(R, RM_LIT, W - 1.0f, 0, Z0 + 2.0f, W - .3f, 1.8f, Z0 + 2.7f, steel, RB_SOLID);
  /* two cafe tables at the windows, either side of the door */
  for (int k = 0; k < 2; k++) {
    float x = k ? W - 1.4f : -W + 1.2f, z = -1.9f;
    rb_drum(R, RM_LIT, x, z, .35f, .72f, .76f, wood, 0); rb_drum(R, RM_LIT, x, z, .05f, 0, .72f, dark, RB_SOLID);
    for (int s = -1; s <= 1; s += 2) { rb_cbox(R, RM_LIT, x + s * .6f, 0, z, .4f, .45f, .4f, dark, 0); rb_cbox(R, RM_LIT, x + s * .78f, .45f, z, .05f, .45f, .4f, dark, 0); }
  }
  for (int k = 0; k < 4; k++) rb_pendant(R, -2.2f + k * 1.4f, cz - .1f, .9f, rgb(.2f, .2f, .2f));
  for (int k = 0; k < 2; k++) rb_pendant(R, k ? W - 1.4f : -W + 1.2f, -1.9f, 1.0f, rgb(.2f, .2f, .2f));
}
static const spot_t SPOTS_PROOF[] = {
  {"proof_counter", -.9f, 0, -5.1f, 3.14159f},      /* in front of the register, facing it */
  {"proof_baker", -.9f, 0, -7.0f, 0},                /* behind the counter, facing the shop */
  {"proof_table", -2.4f, 0, -2.8f, 0},                /* by the window table, facing it */
  {"proof_inside", .3f, 0, -1.6f, 3.14159f},
  {NULL, 0, 0, 0, 0}};

/* Chupacabra Taproom: red brick, the old rafters and a spiral duct, the steel bar along the south wall
 * with the tap wall behind it, high tops, Edison bulbs */
static void furnish_chupacabra(room_t *R) {
  const room_def *D = R->def; const float W = D->w * .5f, Z0 = -D->back - D->d, Z1 = -D->back, H = D->h;
  t3_color steel = rgb(.3f, .31f, .33f), top = rgb(.5f, .34f, .2f), beam = rgb(.42f, .28f, .16f), tapc = rgb(.8f, .8f, .82f);
  /* rafters, ties and the duct */
  for (float z = Z1 - .8f; z > Z0; z -= 1.2f) rb_box(R, RM_LIT, -W, H - .3f, z - .08f, W, H - .05f, z + .08f, beam, 0);
  rb_box(R, RM_LIT, -W, H - .55f, (Z0 + Z1) * .5f - .1f, W, H - .4f, (Z0 + Z1) * .5f + .1f, beam, 0);
  rb_box(R, RM_LIT, .75f, H - 1.15f, Z0 + .3f, 1.25f, H - .65f, Z1 - .3f, tapc, 0);
  /* the bar along the south (left from the door) wall: steel front with the logo cut in, wood top */
  float bx = -W + 1.5f, z0 = Z1 - 2.6f, z1 = Z0 + 1.6f;
  rb_box(R, RM_LIT, bx - .3f, 0, z1, bx + .3f, 1.05f, z0, steel, RB_SOLID);
  rb_box(R, RM_LIT, bx - .38f, 1.05f, z1 - .05f, bx + .38f, 1.11f, z0 + .05f, top, 0);
  rb_sign(R, "chupacabra.png", bx + .31f, .6f, (z0 + z1) * .5f, .8f, .8f, 2);
  for (float z = z0 - .5f; z > z1 + .3f; z -= .8f) rb_stool(R, bx + .85f, z, .78f, rgb(.2f, .14f, .1f));
  /* the tap wall behind it: a dark board, a row of taps and handles, the ON TAP board and two screens */
  rb_box(R, RM_LIT, -W, .9f, z1, -W + .06f, 2.0f, z0, rgb(.14f, .12f, .11f), 0);
  int n = 0;
  for (float z = z0 - .2f; z > z1 + .2f; z -= .2f, n++) {
    static const float HC[5][3] = {{.85f, .2f, .15f}, {.95f, .75f, .2f}, {.2f, .5f, .8f}, {.15f, .15f, .15f}, {.3f, .7f, .35f}};
    rb_cbox(R, RM_LIT, -W + .1f, 1.3f, z, .08f, .04f, .04f, tapc, 0);
    rb_cbox(R, RM_LIT, -W + .12f, 1.34f, z, .03f, .18f, .03f, rgb(HC[n % 5][0], HC[n % 5][1], HC[n % 5][2]), 0);
  }
  rb_box(R, RM_LIT, -W + .02f, 0, z1, -W + .55f, .9f, z0, rgb(.45f, .46f, .48f), RB_SOLID);
  rb_sign(R, "ON TAP", -W + .08f, 2.45f, (z0 + z1) * .5f, 1.6f, .45f, 2);
  for (int k = 0; k < 2; k++) rb_box(R, RM_GLOW, -W + .04f, 2.1f, z0 - .4f - k * 2.2f - 1.5f, -W + .08f, 2.95f, z0 - .4f - k * 2.2f, rgb(.95f, .88f, .7f), 0);
  /* a can fridge by the back */
  rb_box(R, RM_LIT, -W, 0, Z0, -W + .8f, 2.0f, Z0 + .7f, rgb(.2f, .2f, .22f), RB_SOLID);
  rb_box(R, RM_GLOW, -W + .8f, .2f, Z0 + .05f, -W + .82f, 1.85f, Z0 + .65f, rgb(.8f, .9f, 1), 0);
  /* high tops along the north side */
  for (int k = 0; k < 3; k++) {
    float x = W - 1.0f, z = Z1 - 2.0f - k * 2.4f;
    rb_drum(R, RM_LIT, x, z, .4f, 1.0f, 1.05f, top, 0); rb_drum(R, RM_LIT, x, z, .05f, 0, 1.0f, steel, RB_SOLID);
    rb_stool(R, x - .6f, z, .78f, rgb(.2f, .14f, .1f)); rb_stool(R, x, z - .6f, .78f, rgb(.2f, .14f, .1f)); rb_stool(R, x, z + .6f, .78f, rgb(.2f, .14f, .1f));
  }
  /* a long table down the middle with benches */
  rb_box(R, RM_LIT, -.4f, .74f, Z0 + 1.6f, .4f, .79f, Z0 + 4.4f, top, 0);
  rb_box(R, RM_LIT, -.06f, 0, Z0 + 1.8f, .06f, .74f, Z0 + 4.2f, steel, RB_SOLID);
  for (int s = -1; s <= 1; s += 2) rb_box(R, RM_LIT, s * .65f - .15f, .42f, Z0 + 1.6f, s * .65f + .15f, .46f, Z0 + 4.4f, top, 0);
  for (int k = 0; k < 6; k++) rb_pendant(R, (k % 2 ? .3f : -.6f), Z1 - 1.2f - k * 1.6f, 1.1f + (k % 3) * .2f, rgb(.1f, .1f, .1f));
}
static const spot_t SPOTS_CHUPA[] = {
  {"chupacabra_bar", -1.2f, 0, -4.65f, -1.5708f},    /* at the bar between two stools, facing it */
  {"chupacabra_bartender", -2.62f, 0, -4.65f, 1.5708f},
  {"chupacabra_hightop", 1.3f, 0, -2.35f, 1.5708f},
  {"chupacabra_inside", .6f, 0, -1.4f, 3.14159f},
  {NULL, 0, 0, 0, 0}};

/* Level 1 Arcade Bar: dark room, rows of cabinets down both walls, pinball in the middle, the bar at the back */
static void furnish_level1(room_t *R) {
  const room_def *D = R->def; const float W = D->w * .5f, Z0 = -D->back - D->d, Z1 = -D->back, H = D->h;
  static const float SC[6][3] = {{.3f, .9f, 1}, {1, .35f, .8f}, {.5f, 1, .4f}, {1, .85f, .3f}, {.6f, .5f, 1}, {1, .45f, .3f}};
  t3_color cab = rgb(.12f, .12f, .14f), panel = rgb(.3f, .3f, .34f);
  /* the cabinets: body, screen, marquee, control deck. Both side walls, facing in */
  for (int side = -1; side <= 1; side += 2) {
    int k = 0;
    for (float z = Z1 - 2.6f; z > Z0 + 4.5f; z -= .85f, k++) {
      float x = side * (W - .45f), f = -side;       /* the front faces the middle */
      const float *c = SC[(k + (side > 0 ? 3 : 0)) % 6];
      t3_color body = (k % 4 == 1) ? rgb(.55f, .1f, .12f) : (k % 4 == 3) ? rgb(.1f, .2f, .5f) : cab;
      rb_box(R, RM_LIT, x - .38f, 0, z - .36f, x + .38f, 1.85f, z + .36f, body, RB_SOLID);
      rb_box(R, RM_LIT, x + f * .38f, .85f, z - .33f, x + f * .62f, .98f, z + .33f, panel, 0);
      rb_box(R, RM_GLOW, x + f * .39f, 1.15f, z - .27f, x + f * .4f, 1.6f, z + .27f, rgb(c[0], c[1], c[2]), 0);
      rb_box(R, RM_GLOW, x + f * .39f, 1.68f, z - .3f, x + f * .4f, 1.82f, z + .3f, rgb(c[1], c[2], c[0]), 0);
      for (int b = 0; b < 3; b++) rb_cbox(R, RM_GLOW, x + f * .5f, .98f, z - .15f + b * .12f, .04f, .02f, .04f, rgb(c[(b + 1) % 3], c[b % 3], .4f), 0);
    }
  }
  /* pinball down the middle */
  for (int k = 0; k < 4; k++) {
    float z = Z1 - 4.5f - k * 2.1f, x = (k % 2) ? .9f : -.9f;
    rb_box(R, RM_LIT, x - .37f, .75f, z - .7f, x + .37f, .95f, z + .65f, rgb(.15f, .1f, .2f), RB_SOLID);
    rb_box(R, RM_GLOW, x - .32f, .955f, z - .6f, x + .32f, .96f, z + .6f, rgb(SC[k][0] * .7f, SC[k][1] * .7f, SC[k][2] * .7f), 0);
    for (int s = -1; s <= 1; s += 2) for (int e = -1; e <= 1; e += 2) rb_cbox(R, RM_LIT, x + s * .3f, 0, z + e * .55f, .05f, .8f, .05f, panel, 0);
    rb_box(R, RM_LIT, x - .37f, .95f, z - .82f, x + .37f, 1.85f, z - .68f, rgb(.15f, .1f, .2f), 0);
    rb_box(R, RM_GLOW, x - .32f, 1.15f, z - .67f, x + .32f, 1.75f, z - .66f, rgb(SC[(k + 2) % 6][0], SC[(k + 2) % 6][1], SC[(k + 2) % 6][2]), 0);
  }
  /* the bar across the back, its back bar of bottles, stools */
  float bz = Z0 + 2.8f;
  rb_box(R, RM_LIT, -W + 1.0f, 0, bz - .35f, W - 1.0f, 1.05f, bz + .35f, rgb(.1f, .1f, .12f), RB_SOLID);
  rb_box(R, RM_GLOW, -W + 1.0f, .1f, bz + .35f, W - 1.0f, .14f, bz + .36f, rgb(.7f, .3f, 1), 0);
  rb_box(R, RM_LIT, -W + 1.0f, 1.05f, bz - .42f, W - 1.0f, 1.1f, bz + .42f, rgb(.3f, .22f, .15f), 0);
  for (float x = -W + 1.5f; x < W - 1.2f; x += .9f) rb_stool(R, x, bz + .85f, .78f, rgb(.6f, .12f, .15f));
  rb_box(R, RM_LIT, -W + .5f, 0, Z0, W - .5f, 2.4f, Z0 + .5f, rgb(.08f, .08f, .09f), RB_SOLID);
  for (int sh = 0; sh < 3; sh++) for (float x = -W + .8f; x < W - .8f; x += .22f) {
    float y = 1.05f + sh * .4f; uint32_t q = rhash((uint32_t)(x * 40) + sh * 77);
    rb_cbox(R, RM_GLOW, x, y, Z0 + .55f, .07f, .24f + (q % 3) * .04f, .07f, rgb(.3f + (q % 5) * .12f, .5f + (q % 3) * .15f, .3f + (q % 7) * .08f), 0);
  }
  /* the neon */
  rb_sign(R, "HIGH SCORE", 0, 3.3f, Z0 + .52f, 3.2f, .6f, 0);
  rb_sign(R, "INSERT COIN", -W + .03f, 2.6f, (Z0 + Z1) * .5f, 2.6f, .45f, 2);
  rb_sign(R, "PLAY", W - .03f, 2.6f, (Z0 + Z1) * .5f, 1.2f, .5f, 3);
  rb_sign(R, "levelone.jpg", 0, 3.0f, Z1 - .05f, 1.6f, 1.1f, 1);
  for (float z = Z1 - 1; z > Z0 + 1; z -= 3) rb_box(R, RM_GLOW, -W + .5f, H - .06f, z - 1.2f, W - .5f, H - .03f, z - 1.1f, rgb(.4f, .25f, 1), 0);
}
static const spot_t SPOTS_LEVEL1[] = {
  {"level1_bar", .45f, 0, -17.4f, 3.14159f},         /* at the bar between two stools, facing it */
  {"level1_cabinet", -3.9f, 0, -3.0f, -1.5708f},     /* in front of the first cabinet on the west wall, facing it */
  {"level1_pinball", -.9f, 0, -3.7f, 3.14159f},
  {"level1_floor", 0, 0, -14.5f, 3.14159f},           /* the open floor between the pinball and the bar */
  {"level1_inside", -3.0f, 0, -1.6f, 3.14159f},
  {NULL, 0, 0, 0, 0}};

/* The Nile Theater: the lobby and box office, then the hall: black walls, a bar on the side, the stage at
 * the back with its truss of lights and the stacks either side */
static void furnish_nile(room_t *R) {
  const room_def *D = R->def; const float W = D->w * .5f, Z0 = -D->back - D->d, Z1 = -D->back, H = D->h;
  t3_color black = rgb(.1f, .1f, .1f), red = rgb(.5f, .1f, .08f), wood = rgb(.4f, .28f, .17f), steel = rgb(.5f, .5f, .52f);
  /* the lobby wall with two openings into the hall */
  float lz = Z1 - 6.0f;
  rb_box(R, RM_LIT, -W, 0, lz - .2f, -3.0f, H, lz, red, RB_SOLID);
  rb_box(R, RM_LIT, -1.2f, 0, lz - .2f, 1.2f, H, lz, red, RB_SOLID);
  rb_box(R, RM_LIT, 3.0f, 0, lz - .2f, W, H, lz, red, RB_SOLID);
  rb_box(R, RM_LIT, -3.0f, 2.6f, lz - .2f, -1.2f, H, lz, red, 0); rb_box(R, RM_LIT, 1.2f, 2.6f, lz - .2f, 3.0f, H, lz, red, 0);
  /* box office */
  rb_box(R, RM_LIT, -W + .3f, 0, Z1 - 4.5f, -W + 2.6f, 1.1f, Z1 - 3.6f, wood, RB_SOLID);
  rb_sign(R, "BOX OFFICE", -W + 1.45f, 2.2f, Z1 - 3.58f, 1.8f, .4f, 0);
  rb_sign(R, "NILE", 0, 3.6f, lz + .01f, 2.2f, .8f, 0);
  /* the stage: deck, steps, apron, the black back wall, the proscenium, a truss of lights */
  float sz = Z0 + 6.0f, SH = .9f;
  rb_box(R, RM_LIT, -W, 0, Z0, W, SH, sz, rgb(.15f, .13f, .12f), RB_SOLID);
  rb_box(R, RM_LIT, -W, SH - .02f, sz - .02f, W, SH, sz + .02f, steel, 0);
  for (int k = 0; k < 2; k++) rb_box(R, RM_LIT, 2.2f, 0, sz + .3f * (1 - k), 3.2f, .3f * (k + 1), sz + .3f * (2 - k), wood, RB_SOLID);   /* two steps up */
  for (int s = -1; s <= 1; s += 2) { rb_box(R, RM_LIT, s * (W - .9f) - .45f, 0, sz - .3f, s * (W - .9f) + .45f, H - .5f, sz, black, RB_SOLID); }
  rb_box(R, RM_LIT, -W, H - 1.6f, sz - .3f, W, H - .5f, sz, black, 0);
  rb_box(R, RM_LIT, -W + 1.0f, H - 2.0f, sz - 2.0f, W - 1.0f, H - 1.85f, sz - 1.8f, steel, 0);
  static const float LC[4][3] = {{1, .3f, .3f}, {.3f, .5f, 1}, {1, .8f, .3f}, {.7f, .3f, 1}};
  for (int k = 0; k < 8; k++) rb_cbox(R, RM_GLOW, -W + 1.6f + k * (D->w - 3.2f) / 7, H - 2.25f, sz - 1.9f, .2f, .22f, .2f, rgb(LC[k % 4][0], LC[k % 4][1], LC[k % 4][2]), 0);
  /* the stacks, a drum kit's riser and a mic */
  for (int s = -1; s <= 1; s += 2) rb_box(R, RM_LIT, s * (W - 2.0f) - .5f, SH, sz - 1.2f, s * (W - 2.0f) + .5f, SH + 2.2f, sz - .4f, black, RB_SOLID);
  rb_box(R, RM_LIT, -1.5f, SH, Z0 + .8f, 1.5f, SH + .4f, Z0 + 2.6f, rgb(.2f, .2f, .2f), RB_SOLID);
  rb_drum(R, RM_LIT, 0, Z0 + 1.7f, .3f, SH + .4f, SH + .9f, rgb(.6f, .1f, .1f), 0);
  rb_drum(R, RM_LIT, 0, sz - 1.0f, .02f, SH, SH + 1.5f, steel, 0);
  /* the bar along the east wall of the hall, and a sound desk at the back of the floor */
  rb_box(R, RM_LIT, W - 1.0f, 0, lz - 9.0f, W - .4f, 1.1f, lz - 1.5f, wood, RB_SOLID);
  rb_box(R, RM_GLOW, W - .1f, 1.4f, lz - 8.5f, W - .06f, 2.2f, lz - 2.0f, rgb(.9f, .6f, .3f), 0);
  rb_box(R, RM_LIT, -1.5f, 0, lz - 3.0f, 1.5f, 1.0f, lz - 2.0f, rgb(.15f, .15f, .15f), RB_SOLID);
  rb_box(R, RM_GLOW, -1.3f, 1.0f, lz - 2.9f, 1.3f, 1.02f, lz - 2.1f, rgb(.3f, .8f, .5f), 0);
  /* lobby lights, the hall's dim lamps */
  for (int k = 0; k < 3; k++) rb_pendant(R, -3.0f + k * 3.0f, Z1 - 3.0f, 1.0f, rgb(.6f, .45f, .2f));
  for (float z = lz - 3; z > sz + 1; z -= 4) for (int s = -1; s <= 1; s += 2) rb_cbox(R, RM_GLOW, s * (W - .05f), 2.6f, z, .04f, .3f, .3f, rgb(1, .6f, .3f), 0);
}
static const spot_t SPOTS_NILE[] = {
  {"nile_stage", 0, .9f, -32.0f, 0},                 /* centre stage, facing the house (y: the deck) */
  {"nile_floor", 0, 0, -21.0f, 3.14159f},            /* the middle of the floor, facing the stage */
  {"nile_bar", 4.9f, 0, -12.0f, 1.5708f},
  {"nile_box_office", -5.1f, 0, -3.3f, 3.14159f},
  {"nile_inside", 0, 0, -1.6f, 3.14159f},
  {NULL, 0, 0, 0, 0}};

/* Mesa Arts Center, the lobby behind the glass on the plaza: terrazzo, board formed concrete columns, white
 * walls with the work on them, the box office, benches (the museum's 2026 photos) */
static void furnish_mac(room_t *R) {
  const room_def *D = R->def; const float W = D->w * .5f, Z0 = -D->back - D->d, Z1 = -D->back, H = D->h;
  t3_color conc = rgb(.62f, .6f, .57f), wood = rgb(.82f, .7f, .52f), lav = rgb(.6f, .62f, .84f), ink = rgb(.12f, .12f, .14f),
           door = rgb(.36f, .24f, .16f), steel = rgb(.72f, .74f, .76f), stone = rgb(.55f, .52f, .48f), white = rgb(.94f, .93f, .9f);
  static const float AC[6][3] = {{.15f, .3f, .7f}, {.95f, .7f, .15f}, {.85f, .25f, .2f}, {.2f, .6f, .45f}, {.6f, .25f, .6f}, {.95f, .93f, .88f}};
  /* the columns stand just inside the glass, clear of the door and of every line from the way in to the
   * signs (across the middle they cut BOX OFFICE and the gallery's name in half) */
  static const float CX[4] = {-7.0f, -3.4f, 3.4f, 7.0f};
  for (int k = 0; k < 4; k++) rb_cbox(R, RM_LIT, CX[k], 0, Z1 - .9f, .6f, H, .6f, conc, RB_SOLID);
  /* the box office on the east side */
  rb_box(R, RM_LIT, W - 4.5f, 0, Z0 + 2.0f, W - .5f, 1.1f, Z0 + 2.8f, wood, RB_SOLID);
  rb_box(R, RM_LIT, W - 4.6f, 1.1f, Z0 + 1.95f, W - .4f, 1.16f, Z0 + 2.85f, ink, 0);
  rb_box(R, RM_LIT, W - 4.5f, 2.6f, Z0 + .1f, W - .5f, 3.2f, Z0 + .3f, lav, 0);
  rb_sign(R, "BOX OFFICE", W - 2.5f, 2.9f, Z0 + .32f, 2.2f, .45f, 0);
  /* the theater's doors under its name: two pairs of tall timber doors in dark frames, push bars, slot windows */
  rb_sign(R, "IKEDA THEATER", -2.0f, 4.6f, Z0 + .02f, 4.2f, .6f, 0);
  for (int k = 0; k < 2; k++) {
    float cx = k ? -.4f : -3.6f;
    rb_box(R, RM_LIT, cx - 1.15f, 0, Z0, cx + 1.15f, 2.95f, Z0 + .06f, ink, 0);
    for (int s = -1; s <= 1; s += 2) {
      float x0 = cx + (s < 0 ? -1.05f : .03f), x1 = cx + (s < 0 ? -.03f : 1.05f);
      rb_box(R, RM_LIT, x0, 0, Z0 + .06f, x1, 2.8f, Z0 + .1f, door, 0);
      rb_box(R, RM_LIT, cx + s * .62f - .08f, 1.45f, Z0 + .1f, cx + s * .62f + .08f, 2.35f, Z0 + .11f, ink, 0);
      rb_box(R, RM_LIT, cx + s * .15f, 1.0f, Z0 + .1f, cx + s * .9f, 1.06f, Z0 + .17f, steel, 0);
    }
  }
  /* show posters either side: a dark frame, a ground and a bold block of colour */
  static const float PX[2] = {-6.4f, 2.2f};
  for (int k = 0; k < 2; k++) {
    float x = PX[k];
    rb_box(R, RM_LIT, x - .65f, .9f, Z0, x + .65f, 2.85f, Z0 + .04f, ink, 0);
    rb_box(R, RM_LIT, x - .58f, .97f, Z0 + .04f, x + .58f, 2.78f, Z0 + .05f, rgb(AC[k + 1][0], AC[k + 1][1], AC[k + 1][2]), 0);
    rb_box(R, RM_LIT, x - .4f, 1.5f + k * .3f, Z0 + .05f, x + .25f, 2.4f + k * .1f, Z0 + .06f, rgb(AC[k * 3][0], AC[k * 3][1], AC[k * 3][2]), 0);
    rb_box(R, RM_LIT, x - .45f, 1.1f, Z0 + .05f, x + .45f, 1.22f, Z0 + .06f, white, 0);
  }
  rb_sign(R, "MESA CONTEMPORARY ARTS", -W + .02f, 3.6f, Z0 + 2.8f, 5.0f, .5f, 2);   /* the gallery's wall, seen from the lobby */
  /* works on the gallery wall: framed canvases, each with a second colour laid over its ground */
  for (int k = 0; k < 5; k++) {
    float z1 = Z1 - 2.0f - k * 2.2f, z0 = z1 - 1.4f, y0 = 1.3f + (k % 2) * .3f, y1 = 2.4f + (k % 2) * .3f;
    const float *c = AC[k], *o = AC[(k + 2) % 6];
    rb_box(R, RM_LIT, -W + .01f, y0 - .06f, z0 - .06f, -W + .04f, y1 + .06f, z1 + .06f, k % 2 ? white : ink, 0);
    rb_box(R, RM_LIT, -W + .04f, y0, z0, -W + .07f, y1, z1, rgb(c[0], c[1], c[2]), 0);
    if (k % 2) rb_box(R, RM_LIT, -W + .07f, y0 + .2f, z0 + .25f, -W + .08f, y1 - .35f, z1 - .55f, rgb(o[0], o[1], o[2]), 0);
    else rb_box(R, RM_LIT, -W + .07f, y0 + (y1 - y0) * .55f, z0 + .1f, -W + .08f, y0 + (y1 - y0) * .7f, z1 - .1f, rgb(o[0], o[1], o[2]), 0);
  }
  /* a carved stone head on a plinth, after the 2026 show, looking at the door: a narrow chin, broad cheeks,
   * a high forehead rounded off at the crown, a brow ridge over the eyes, the nose and the ears */
  {
    const float hx = -2.0f, hz = -4.0f, y = 1.05f;
    t3_color dk = rgb(.47f, .44f, .4f);
    rb_cbox(R, RM_LIT, hx, 0, hz, .9f, y, .9f, conc, RB_SOLID);
    rb_cbox(R, RM_LIT, hx, y, hz - .02f, .22f, .14f, .24f, stone, 0);              /* neck */
    rb_cbox(R, RM_LIT, hx, y + .14f, hz + .02f, .3f, .12f, .36f, stone, 0);        /* chin and jaw */
    rb_cbox(R, RM_LIT, hx, y + .26f, hz, .4f, .28f, .46f, stone, 0);               /* cheeks */
    rb_cbox(R, RM_LIT, hx, y + .54f, hz - .01f, .38f, .18f, .44f, stone, 0);       /* forehead */
    rb_cbox(R, RM_LIT, hx, y + .72f, hz - .02f, .28f, .07f, .34f, stone, 0);       /* crown */
    rb_cbox(R, RM_LIT, hx, y + .5f, hz + .24f, .34f, .05f, .05f, dk, 0);           /* brow ridge */
    rb_cbox(R, RM_LIT, hx, y + .3f, hz + .25f, .08f, .2f, .08f, stone, 0);         /* nose */
    rb_cbox(R, RM_LIT, hx, y + .2f, hz + .235f, .14f, .03f, .03f, dk, 0);          /* the line of the mouth */
    for (int s2 = -1; s2 <= 1; s2 += 2) rb_cbox(R, RM_LIT, hx + s2 * .215f, y + .32f, hz - .02f, .04f, .16f, .1f, stone, 0);   /* ears */
  }
  /* a tall figure, its head a pale block */
  rb_drum(R, RM_LIT, 3.0f, -5.0f, .3f, 0, 2.6f, rgb(.2f, .3f, .7f), RB_SOLID);
  rb_cbox(R, RM_LIT, 3.0f, 2.6f, -5.0f, .45f, .5f, .4f, rgb(.92f, .88f, .8f), 0);
  /* and up in the height of the room, a hanging work: coloured plates on wires at different heights */
  static const float HP[6][5] = {{-1.6f, -6.2f, 5.6f, 1.1f, .7f}, {-.2f, -7.4f, 4.6f, .8f, .8f}, {1.3f, -6.6f, 5.2f, .6f, 1.2f},
                                 {.4f, -8.8f, 5.9f, 1.3f, .6f}, {-1.0f, -9.4f, 4.9f, .7f, .7f}, {2.0f, -8.4f, 4.4f, .9f, .5f}};
  for (int k = 0; k < 6; k++) {
    const float *h = HP[k], *c = AC[(k + 1) % 6];
    rb_cbox(R, RM_LIT, h[0], h[2], h[1], h[3], .05f, h[4], rgb(c[0], c[1], c[2]), 0);
    rb_cbox(R, RM_LIT, h[0], h[2] + .05f, h[1], .015f, H - h[2] - .05f, .015f, ink, 0);
  }
  /* benches */
  for (int k = -1; k <= 1; k += 2) rb_cbox(R, RM_LIT, k * 5.5f, 0, Z1 - 3.0f, 2.0f, .45f, .5f, wood, RB_SOLID);   /* either side of the way in */
  /* linear lights */
  for (float z = Z1 - 1.5f; z > Z0 + 1; z -= 3) rb_box(R, RM_GLOW, -W + 1, H - .04f, z - .1f, W - 1, H - .01f, z + .1f, rgb(1, .97f, .9f), 0);
}
static const spot_t SPOTS_MAC[] = {
  {"arts_center_lobby", 0, 0, -6.0f, 3.14159f},
  {"arts_center_box_office", 6.5f, 0, -9.6f, 3.14159f},
  {"arts_center_gallery", -7.3f, 0, -9.4f, -1.5708f},
  {"arts_center_inside", 0, 0, -1.6f, 3.14159f},
  {NULL, 0, 0, 0, 0}};

/* id, sign, (free placement), door x, w, d, h, back, look, glaze, furnish, spots */

/* v2.0: the lab's old storefront at one forty West Main, as the attic's flashbacks remember it (2011 to 2018):
 * the walls the work party painted pink, workbenches down both sides, a couch and folding tables up front,
 * shelving and the kitchenette at the back, and a square of tape on the floor where the laser was to go */
static void furnish_glitter(room_t *R) {
  const room_def *D = R->def; const float W = D->w * .5f, Z0 = -D->back - D->d;
  t3_color bench = rgb(.62f, .5f, .34f), top = rgb(.74f, .66f, .5f), steel = rgb(.6f, .62f, .64f), dark = rgb(.2f, .19f, .2f);
  t3_color tote[4] = { rgb(.25f, .45f, .8f), rgb(.8f, .25f, .22f), rgb(.3f, .62f, .35f), rgb(.85f, .75f, .25f) };
  /* workbenches down both walls, with pegboard over them */
  for (int sd = -1; sd <= 1; sd += 2) {
    float x0 = sd < 0 ? -W + .1f : W - .85f, x1 = sd < 0 ? -W + .85f : W - .1f;
    for (float z = -4.0f; z > Z0 + 5.5f; z -= 2.6f) {
      rb_box(R, RM_LIT, x0, .88f, z - 2.3f, x1, .94f, z, top, 0);
      rb_box(R, RM_LIT, x0 + .05f, 0, z - 2.25f, x0 + .12f, .88f, z - 2.15f, bench, RB_SOLID);
      rb_box(R, RM_LIT, x0 + .05f, 0, z - .15f, x0 + .12f, .88f, z - .05f, bench, RB_SOLID);
      rb_box(R, RM_LIT, x1 - .12f, 0, z - 2.25f, x1 - .05f, .88f, z - 2.15f, bench, RB_SOLID);
      rb_box(R, RM_LIT, x1 - .12f, 0, z - .15f, x1 - .05f, .88f, z - .05f, bench, RB_SOLID);
      rb_box(R, RM_LIT, x0, 0, z - 2.3f, x1, .9f, z, bench, RB_SOLID | RB_NOBOTTOM);
      float wx = sd < 0 ? -W + .02f : W - .04f;
      rb_box(R, RM_LIT, wx, 1.1f, z - 2.2f, wx + .02f, 2.1f, z - .1f, rgb(.75f, .62f, .45f), 0);   /* pegboard */
      for (int k = 0; k < 5; k++) rb_cbox(R, RM_LIT, wx + sd * -.04f, 1.3f + (k % 2) * .4f, z - .4f - k * .38f, .03f, .22f, .05f, k % 2 ? steel : dark, 0);
      /* what is on the bench: a vice, a box of parts, a soldering station's glow */
      rb_cbox(R, RM_LIT, (x0 + x1) * .5f, .94f, z - .5f, .2f, .14f, .14f, steel, 0);
      rb_cbox(R, RM_LIT, (x0 + x1) * .5f, .94f, z - 1.4f, .35f, .12f, .25f, tote[((int)-z) & 3], 0);
      rb_cbox(R, RM_GLOW, (x0 + x1) * .5f, .94f, z - 1.9f, .12f, .05f, .1f, rgb(1, .35f, .25f), 0);
    }
  }
  /* up front: a thrift store couch and a coffee table, folding tables with laptops */
  rb_box(R, RM_LIT, -W + .5f, 0, -3.4f, -W + 1.4f, .45f, -1.2f, rgb(.42f, .32f, .5f), RB_SOLID);
  rb_box(R, RM_LIT, -W + .5f, .45f, -3.4f, -W + .75f, .95f, -1.2f, rgb(.38f, .28f, .46f), 0);
  rb_cbox(R, RM_LIT, -W + 2.0f, 0, -2.3f, .6f, .4f, 1.0f, bench, RB_SOLID);
  for (int k = 0; k < 2; k++) {
    float z = -5.2f - k * 2.2f;
    rb_box(R, RM_LIT, -1.3f, .72f, z - .45f, 1.3f, .76f, z + .45f, rgb(.85f, .85f, .82f), 0);
    for (int l = 0; l < 4; l++) rb_cbox(R, RM_LIT, l & 1 ? 1.2f : -1.2f, 0, z + (l & 2 ? .38f : -.38f), .04f, .72f, .04f, steel, l == 0 ? RB_SOLID : 0);
    rb_box(R, RM_LIT, -1.3f, 0, z - .45f, 1.3f, .72f, z + .45f, dark, RB_SOLID | RB_NOBOTTOM);
    for (int l = 0; l < 2; l++) { float x = l ? .55f : -.6f; rb_cbox(R, RM_LIT, x, .76f, z, .34f, .02f, .24f, dark, 0); rb_box(R, RM_GLOW, x - .16f, .78f, z - .13f, x + .16f, .98f, z - .11f, rgb(.55f, .7f, .9f), 0); }
  }
  /* the laser's square, taped on the floor in the middle of the room */
  float lz = -11.0f;
  rb_box(R, RM_LIT, -1.0f, 0, lz - .7f, 1.0f, .006f, lz - .64f, rgb(.95f, .8f, .2f), 0);
  rb_box(R, RM_LIT, -1.0f, 0, lz + .64f, 1.0f, .006f, lz + .7f, rgb(.95f, .8f, .2f), 0);
  rb_box(R, RM_LIT, -1.0f, 0, lz - .7f, -.94f, .006f, lz + .7f, rgb(.95f, .8f, .2f), 0);
  rb_box(R, RM_LIT, .94f, 0, lz - .7f, 1.0f, .006f, lz + .7f, rgb(.95f, .8f, .2f), 0);
  /* the back: steel shelving full of totes, and the kitchenette */
  for (float x = -W + .3f; x < W - 2.5f; x += 1.3f) {
    rb_box(R, RM_LIT, x, 0, Z0 + .1f, x + 1.2f, 2.2f, Z0 + .6f, steel, RB_SOLID | RB_NOBOTTOM);
    for (int sh = 0; sh < 4; sh++) for (int k = 0; k < 2; k++) rb_cbox(R, RM_LIT, x + .3f + k * .6f, .1f + sh * .55f, Z0 + .37f, .5f, .32f, .42f, tote[(sh + k + (int)(x * 3)) & 3], 0);
  }
  rb_box(R, RM_LIT, W - 2.2f, 0, Z0 + .1f, W - .1f, .9f, Z0 + .75f, rgb(.88f, .86f, .82f), RB_SOLID);
  rb_box(R, RM_LIT, W - 1.0f, 0, Z0 + .1f, W - .1f, 1.75f, Z0 + .8f, rgb(.9f, .9f, .9f), RB_SOLID);
  /* bare fluorescent tubes, and a string of party lights along the left wall that nobody took down */
  for (float z = -3.0f; z > Z0 + 1.0f; z -= 3.0f) for (int sd = -1; sd <= 1; sd += 2) rb_cbox(R, RM_GLOW, sd * 1.6f, D->h - .08f, z, .12f, .05f, 1.4f, rgb(.95f, .97f, 1), 0);
  for (float z = -1.5f; z > Z0 + 1.0f; z -= .5f) rb_cbox(R, RM_GLOW, -W + .06f, 2.45f + .06f * sinf(z * 2.f), z, .05f, .05f, .05f, ((int)(-z * 2)) % 3 == 0 ? rgb(1, .3f, .4f) : ((int)(-z * 2)) % 3 == 1 ? rgb(.3f, .9f, .4f) : rgb(.4f, .6f, 1), 0);
}
static const spot_t SPOTS_GLITTER[] = {
  {"glitter_inside", 0, 0, -1.6f, 3.14159f},        /* just inside the door, facing in */
  {"glitter_couch", -2.4f, 0, -2.3f, 1.5708f},       /* by the couch, facing the room's middle */
  {"glitter_table", 0, 0, -6.3f, 3.14159f},          /* at the folding tables */
  {"glitter_laser", 0, 0, -11.0f, 0},                /* the laser's taped square */
  {"glitter_laser_n", 0, 0, -13.0f, 0},              /* beyond it, facing the front */
  {"glitter_bench_l", -2.7f, 0, -8.0f, -1.5708f},    /* at the left benches, facing them */
  {"glitter_bench_r", 2.7f, 0, -8.0f, 1.5708f},      /* at the right benches, facing them */
  {"glitter_back", 0, 0, -16.0f, 0},                 /* in front of the back shelving, facing the front */
  {NULL}
};

static const room_def ROOMS[] = {
  {"proof", "Proof Bread", 0, 0, 0, 0, 7.2f, 13.f, 4.2f, .3f, LOOK_BAKERY, .9f, furnish_proof, SPOTS_PROOF},
  {"chupacabra", "Chupacabra Taproom", 0, 0, 0, .6f, 7.0f, 11.f, 4.3f, .35f, LOOK_TAPROOM, .8f, furnish_chupacabra, SPOTS_CHUPA},
  {"level1", "Level 1 Arcade Bar", 0, 0, 0, -3.0f, 11.4f, 21.f, 4.6f, .4f, LOOK_ARCADE, .6f, furnish_level1, SPOTS_LEVEL1},
  {"nile", "The Nile Theater", 0, 0, 0, 0, 13.2f, 36.f, 9.f, .4f, LOOK_THEATER, .5f, furnish_nile, SPOTS_NILE},
  /* the Ikeda Theater's glass lobby on the plaza: its north east face, (370.7, 58.8) to (391.4, 66.5) */
  {"arts_center", "Mesa Arts Center", 381.07f, 62.63f, 2.786f, 0, 17.f, 13.f, 7.f, .3f, LOOK_GALLERY, 1.f, furnish_mac, SPOTS_MAC},
  /* v2.0: one forty West Main behind today's shopfront there, for the flashbacks only */
  {"atomic", "One Forty", 0, 0, 0, 0, 7.6f, 18.f, 4.0f, .3f, LOOK_GLITTER, .85f, furnish_glitter, SPOTS_GLITTER, true},
};
#define NROOM (int)(sizeof ROOMS / sizeof ROOMS[0])
/* the lot wall a placed room's front stands on (the Arts Center lobby): city.c glazes that wall whatever its
 * hash says and frames the door there, so the way in is a door from outside too. The door's middle, or false */
bool interior_front_on(float ax, float az, float bx, float bz, float *dx, float *dz) {
  for (int i = 0; i < NROOM; i++) {
    const room_def *D = &ROOMS[i];
    if (D->cx == 0 && D->cz == 0) continue;   /* a room behind a facade: the facade has its door */
    float ex = bx - ax, ez = bz - az, l2 = ex * ex + ez * ez;
    if (l2 < 1e-6f) continue;
    float t = ((D->cx - ax) * ex + (D->cz - az) * ez) / l2, px = ax + ex * t - D->cx, pz = az + ez * t - D->cz;
    if (t < 0 || t > 1 || px * px + pz * pz > .36f) continue;
    *dx = D->cx + cosf(D->yaw) * D->door_x; *dz = D->cz - sinf(D->yaw) * D->door_x;
    return true;
  }
  return false;
}
static room_t RM[NROOM];
static int current = -1;
static t3_scene *SC;
static t3_object *rooms_root;
static t3_material *M_sky;

/* the rooms hang under the town (world.c): an arena hides them with it */
void interior_attach(t3_object *parent) { if (rooms_root && parent) { t3_retain(rooms_root); t3_object_remove(&SC->base, rooms_root); t3_object_add(parent, rooms_root); t3_release(rooms_root); } }
void interior_init(t3_scene *scene) {
  SC = scene;
  rooms_root = t3_group_new(); t3_object_add(&scene->base, rooms_root);
  M_sky = t3_mesh_basic_material_new(0xffffff); M_sky->vertex_colors = true;
  for (int i = 0; i < NROOM; i++) {
    room_t *R = &RM[i]; const room_def *D = &ROOMS[i];
    R->def = D;
    v3 f; float yaw, w;
    if (D->cx != 0 || D->cz != 0) { R->cx = D->cx; R->cz = D->cz; R->yaw = D->yaw; }
    else if (city_lot_front(D->id, &f, &yaw, &w)) { R->cx = f.x; R->cz = f.z; R->yaw = yaw; }
    else { ds_log("interior: no lot %s", D->id); }
    R->c = cosf(R->yaw); R->s = sinf(R->yaw);
  }
  current = -1;
}
int interior_count(void) { return NROOM; }
const char *interior_name(int i) { return i >= 0 && i < NROOM ? ROOMS[i].name : ""; }
const char *interior_id(int i) { return i >= 0 && i < NROOM ? ROOMS[i].id : ""; }
int interior_find(const char *id) { for (int i = 0; i < NROOM; i++) if (!strcmp(ROOMS[i].id, id)) return i; return -1; }
bool interior_door(int i, v3 *outside, float *yaw_out, v3 *inside, float *yaw_in) {
  if (i < 0 || i >= NROOM || ROOMS[i].memory) return false;   /* a memory has no door from today's street */
  room_t *R = &RM[i]; const room_def *D = R->def;
  /* outside: on the street in front of the door, a step clear of whatever stands at the facade, and far
   * enough out that the follow camera behind someone coming out has room before the shopfront. The town
   * does not move there, so it is found once (this is asked for every room many times a frame) */
  if (!R->door_ok) {
    v3 o = to_world(R, D->door_x, 0, 1.8f);
    for (float out = 1.8f; out < 4.5f; out += .3f) {
      v3 q = to_world(R, D->door_x, 0, out), p = q, n; bool hit = false;
      city_push(&p, .3f, 1.6f, &n, &hit);
      if (!hit) { o = q; break; }
    }
    R->door_out = o; R->door_ok = true;
  }
  if (outside) *outside = R->door_out;
  if (yaw_out) *yaw_out = R->yaw + 3.14159265f;
  if (inside) *inside = to_world(R, D->door_x, 0, -D->back - 1.0f);
  if (yaw_in) *yaw_in = R->yaw + 3.14159265f;
  return true;
}

/* ── building on first entry ── */
static void build_room(room_t *R) {
  const room_def *D = R->def;
  float light = LOOKS[D->look].light;
  for (int m = 0; m < RM_N; m++) memset(&R->sk[m], 0, sizeof R->sk[m]);
  rb_shell(R);
  D->furnish(R);
  R->root = t3_group_new();
  t3_material *lit = t3_mesh_lambert_material_new(0xffffff); lit->vertex_colors = true; lit->color = (t3_color){light, light, light};
  t3_material *brick = t3_mesh_lambert_material_new(0xffffff); brick->vertex_colors = true; brick->color = (t3_color){light, light, light};
  if (city_brick_texture()) t3_material_set_map(brick, city_brick_texture());
  t3_material *glow = t3_mesh_basic_material_new(0xffffff); glow->vertex_colors = true;
  R->mat[RM_LIT] = lit; R->mat[RM_BRICK] = brick; R->mat[RM_GLOW] = glow; R->mat[RM_SKY] = M_sky; R->mat[RM_SIGN] = city_sign_material();
  R->tris = 0;
  for (int m = 0; m < RM_N; m++) {
    sink *s = &R->sk[m];
    if (s->ni && R->mat[m]) {
      t3_geometry *g = t3_geometry_new();
      t3_attribute *a;
      a = t3_attribute_new(T3_FLOAT32, s->pos, s->nv, 3); t3_geometry_set_attribute(g, T3_ATTR_POSITION, a); t3_release(a);
      a = t3_attribute_new(T3_FLOAT32, s->nor, s->nv, 3); t3_geometry_set_attribute(g, T3_ATTR_NORMAL, a); t3_release(a);
      a = t3_attribute_new(T3_FLOAT32, s->col, s->nv, 3); t3_geometry_set_attribute(g, T3_ATTR_COLOR, a); t3_release(a);
      if (m == RM_BRICK || m == RM_SIGN) { a = t3_attribute_new(T3_FLOAT32, s->uv, s->nv, 2); t3_geometry_set_attribute(g, T3_ATTR_UV, a); t3_release(a); }
      a = t3_attribute_new(T3_UINT32, s->idx, s->ni, 1); t3_geometry_set_index(g, a); t3_release(a);
      t3_geometry_compute_bounding_sphere(g);
      t3_mesh *me = t3_mesh_new(g, R->mat[m]);
      t3_object_add(R->root, me);
      t3_release(me); t3_release(g);
      R->tris += s->ni / 3;
    }
    free(s->pos); free(s->nor); free(s->col); free(s->uv); free(s->idx);
    memset(s, 0, sizeof *s);
  }
  t3_release(lit); t3_release(brick); t3_release(glow);
  t3_object_add(rooms_root, R->root);
  R->built = true;
  ds_log("interior: built %s, %d tris, %d colliders", D->id, R->tris, R->ncol);
}
void interior_enter(int i) {
  if (i < 0 || i >= NROOM) return;
  if (!RM[i].built) build_room(&RM[i]);
  for (int k = 0; k < NROOM; k++) if (RM[k].root) RM[k].root->visible = k == i;
  current = i;
}
void interior_exit(void) {
  for (int k = 0; k < NROOM; k++) if (RM[k].root) RM[k].root->visible = false;
  current = -1;
}
int interior_current(void) { return current; }
static bool in_room(const room_t *R, v3 p, float m) {
  const room_def *D = R->def;
  v3 q = to_room(R, p);
  return fabsf(q.x) < D->w * .5f + m && q.z < -D->back + m && q.z > -D->back - D->d - m && q.y > -1.f && q.y < D->h + .5f;
}
bool interior_contains(v3 p) { return current >= 0 && in_room(&RM[current], p, .05f); }
float interior_ceiling(void) { return current >= 0 ? RM[current].def->h : 3.f; }
int interior_tris(void) { return current >= 0 ? RM[current].tris : 0; }

/* ── spots ── */
static bool spot_of(int i, const spot_t *s, v3 *pos, float *yaw) {
  const room_t *R = &RM[i];
  if (pos) *pos = to_world(R, s->x, s->y, s->z);
  if (yaw) *yaw = R->yaw + s->yaw;
  return true;
}
bool interior_place(const char *name, v3 *pos, float *yaw) {
  for (int i = 0; i < NROOM; i++) for (const spot_t *s = ROOMS[i].spots; s && s->name; s++) if (!strcmp(s->name, name)) return spot_of(i, s, pos, yaw);
  return false;
}
int interior_spot_count(int i) { int n = 0; if (i >= 0 && i < NROOM) for (const spot_t *s = ROOMS[i].spots; s && s->name; s++) n++; return n; }
const char *interior_spot_name(int i, int k) { return k >= 0 && k < interior_spot_count(i) ? ROOMS[i].spots[k].name : NULL; }

/* ── collision, floors and rays in room space ── */
bool interior_push(v3 *p, float r, float h, v3 *normal, bool *hit) {
  if (current < 0) return false;
  room_t *R = &RM[current];
  if (!in_room(R, *p, .4f)) return false;
  v3 q = to_room(R, *p);
  float yb = q.y + .32f, yt = q.y + h;
  for (int i = 0; i < R->ncol; i++) {
    const ibox *c = &R->col[i];
    if (yb > c->y1 || yt < c->y0) continue;
    float qx = fmaxf(c->x0, fminf(q.x, c->x1)), qz = fmaxf(c->z0, fminf(q.z, c->z1)), dx = q.x - qx, dz = q.z - qz, d2 = dx * dx + dz * dz;
    if (d2 >= r * r) continue;
    v3 n;
    if (d2 > 1e-8f) { float d = sqrtf(d2); n = v3_(dx / d, 0, dz / d); q.x += n.x * (r - d); q.z += n.z * (r - d); }
    else {
      float l = q.x - c->x0, rr = c->x1 - q.x, b = q.z - c->z0, f = c->z1 - q.z, m = fminf(fminf(l, rr), fminf(b, f));
      if (m == l) { q.x = c->x0 - r; n = v3_(-1, 0, 0); } else if (m == rr) { q.x = c->x1 + r; n = v3_(1, 0, 0); }
      else if (m == b) { q.z = c->z0 - r; n = v3_(0, 0, -1); } else { q.z = c->z1 + r; n = v3_(0, 0, 1); }
    }
    if (normal) *normal = dir_world(R, n);
    if (hit) *hit = true;
  }
  v3 w = to_world(R, q.x, q.y, q.z);
  p->x = w.x; p->z = w.z;
  return true;
}
bool interior_floor(v3 p, float up, float *y) {
  if (current < 0) return false;
  room_t *R = &RM[current];
  if (!in_room(R, p, .05f)) return false;
  v3 q = to_room(R, p);
  float best = 0, lim = q.y + up;
  for (int i = 0; i < R->ncol; i++) {
    const ibox *c = &R->col[i];
    if (q.x < c->x0 || q.x > c->x1 || q.z < c->z0 || q.z > c->z1) continue;
    if (c->y1 > best && c->y1 <= lim && c->y1 < R->def->h - .1f && (c->x1 - c->x0) * (c->z1 - c->z0) > .3f) best = c->y1;
  }
  if (y) *y = best;
  return true;
}
static bool ray_ibox(v3 o, v3 d, const ibox *c, float *t, v3 *n) {
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
bool interior_raycast(v3 o, v3 d, float max_dist, float *t, v3 *normal) {
  if (current < 0) return false;
  room_t *R = &RM[current];
  if (!in_room(R, o, .05f)) return false;
  v3 q = to_room(R, o), dq = dir_room(R, d);
  float best = max_dist; v3 bn = v3_(0, 1, 0); bool any = false;
  for (int i = 0; i < R->ncol; i++) {
    float tt; v3 nn;
    if (ray_ibox(q, dq, &R->col[i], &tt, &nn) && tt < best && tt > 0) { best = tt; bn = nn; any = true; }
  }
  if (any) { if (t) *t = best; if (normal) *normal = dir_world(R, bn); }
  return any;
}
void interior_set_time(float night) {
  if (!M_sky) return;
  /* day: a bright sky through the glass; dusk: amber; night: the street's blue dark and a sodium glow */
  float d = 1 - night;
  M_sky->color = (t3_color){.16f + .56f * d, .18f + .64f * d, .3f + .66f * d};
  M_sky->version++;
}
