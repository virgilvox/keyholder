/* enemy.c: see enemy.h. Props are built with fx.h's fxp builder in a socket's frame (actor_socket: +y up,
 * +z the way the body faces, +x its left, origin at the bone's pivot), then moved from the fx scene onto
 * the socket. This file keeps its own reference to each prop, so undressing is remove and release. */
#include "enemy.h"
#include "fx.h"
#include <string.h>

/* ---- shapes fxp lacks: a tube between any two points, a box tapered along y ---- */
static v3 side_of(v3 d) { return v3_norm(v3_cross(d, fabsf(d.y) > .9f ? v3_(1, 0, 0) : v3_(0, 1, 0))); }
static void tube(v3 a, v3 b, float r0, float r1, int sides, uint32_t col) {
  v3 d = v3_sub(b, a);
  if (v3_len(d) < 1e-4f) return;
  d = v3_norm(d);
  v3 u = side_of(d), w = v3_cross(d, u);
  for (int k = 0; k < sides; k++) {
    float a0 = k * 6.28318f / sides, a1 = (k + 1) * 6.28318f / sides;
    v3 o0 = v3_add(v3_scale(u, cosf(a0)), v3_scale(w, sinf(a0))), o1 = v3_add(v3_scale(u, cosf(a1)), v3_scale(w, sinf(a1)));
    fxp_quad(v3_add(a, v3_scale(o0, r0)), v3_add(a, v3_scale(o1, r0)), v3_add(b, v3_scale(o1, r1)), v3_add(b, v3_scale(o0, r1)), col);
    fxp_quad(b, b, v3_add(b, v3_scale(o0, r1)), v3_add(b, v3_scale(o1, r1)), col);   /* the far cap, a fan */
  }
}
/* a box whose half sizes go from (bx, bz) at y0 to (tx, tz) at y1, centred on x 0 and z cz: a tote, a crate */
static void taper(float y0, float y1, float bx, float bz, float tx, float tz, float cz, uint32_t col, bool bottom) {
  v3 B[4] = { v3_(-bx, y0, cz + bz), v3_(bx, y0, cz + bz), v3_(bx, y0, cz - bz), v3_(-bx, y0, cz - bz) };
  v3 T[4] = { v3_(-tx, y1, cz + tz), v3_(tx, y1, cz + tz), v3_(tx, y1, cz - tz), v3_(-tx, y1, cz - tz) };
  for (int i = 0; i < 4; i++) { int j = (i + 1) & 3; fxp_quad(B[i], B[j], T[j], T[i], col); }
  fxp_quad(T[0], T[1], T[2], T[3], col);
  if (bottom) fxp_quad(B[3], B[2], B[1], B[0], col);
}
/* a box stretched along z: its front face at z0 (w0 by h0), its back at z1 (w1 by h1), centred on x 0 and y cy */
static void frustum_z(float z0, float z1, float w0, float h0, float w1, float h1, float cy, uint32_t col) {
  v3 F[4] = { v3_(-w0, cy - h0, z0), v3_(w0, cy - h0, z0), v3_(w0, cy + h0, z0), v3_(-w0, cy + h0, z0) };
  v3 K[4] = { v3_(-w1, cy - h1, z1), v3_(w1, cy - h1, z1), v3_(w1, cy + h1, z1), v3_(-w1, cy + h1, z1) };
  for (int i = 0; i < 4; i++) { int j = (i + 1) & 3; fxp_quad(F[i], F[j], K[j], K[i], col); }
  fxp_quad(K[3], K[2], K[1], K[0], col);
}
/* a flat panel facing +z at depth z: a label, a screen, a face */
static void panel(float x, float y, float z, float w, float h, uint32_t col) {
  fxp_quad(v3_(x - w / 2, y - h / 2, z), v3_(x + w / 2, y - h / 2, z), v3_(x + w / 2, y + h / 2, z), v3_(x - w / 2, y + h / 2, z), col);
}
/* a cable through a run of points: a square rod per span, a joint box at each bend so it reads as one */
static void cable(const v3 *p, int n, float w, uint32_t col) {
  for (int i = 0; i + 1 < n; i++) fxp_bar(p[i], p[i + 1], w, col);
  for (int i = 1; i + 1 < n; i++) fxp_box(p[i], v3_(w, w, w), col);
}

/* ---- the dressed table ---- */
#define MAX_DRESSED 16
#define MAX_PROPS 6
typedef struct {
  actor_t *a; int kind;
  t3_object *prop[MAX_PROPS]; int n;
  float scale;                  /* the actor's size before dressing */
  actor_palette pal; bool repal;   /* the palette before, when dressing added the kind's wash */
} dressed_t;
static dressed_t DR[MAX_DRESSED];
static unsigned dress_count;    /* tells a group's members apart: lid colours, who wears the cardboard */

static dressed_t *slot_of(const actor_t *a) { for (int i = 0; i < MAX_DRESSED; i++) if (DR[i].a == a) return &DR[i]; return NULL; }
/* the prop just built moves onto a socket; the scene's hold passes to the socket, this file keeps one of its own */
static void attach(dressed_t *d, t3_object *sock) {
  t3_object *g = fxp_end();
  if (!sock || d->n >= MAX_PROPS) { t3_object_remove(g->parent, g); return; }
  t3_object_add(sock, g);
  t3_retain(g);
  d->prop[d->n++] = g;
}

/* ---- the kinds ---- */
static const uint32_t BULB[5] = { 0xff3a2a, 0x36ff5a, 0x3c7cff, 0xffd22a, 0xff7a1e };

static void santa_hat(dressed_t *d, actor_t *a) {
  const uint32_t red = 0xb8182a, fur = 0xe6e2da;
  fxp_begin();
  /* the fur band round the hair at the hairline (the pivot is mid head: the crown about 0.14 over it, the
   * hair about 0.24 across), set back a little as a hat is worn */
  fxp_cyl(v3_(0, .055f, -.01f), .058f, .128f, .124f, 16, fur);
  /* a round crown over the spiky hair first, then the cone */
  tube(v3_(0, .1f, -.012f), v3_(0, .19f, -.026f), .126f, .108f, 14, red);
  tube(v3_(0, .19f, -.026f), v3_(0, .26f, -.055f), .108f, .062f, 12, red);
  tube(v3_(0, .26f, -.055f), v3_(.015f, .27f, -.12f), .062f, .036f, 10, red);
  tube(v3_(.015f, .27f, -.12f), v3_(.03f, .23f, -.18f), .036f, .015f, 8, red);   /* the tip flops over the back */
  fxp_box(v3_(.034f, .216f, -.19f), v3_(.056f, .056f, .056f), fur);
  attach(d, actor_socket_head(a));
}
/* wound twice round the chest, from under the arms to the waist, a bulb every few centimetres */
static void light_string(dressed_t *d, actor_t *a) {
  enum { N = 44 };
  const float turns = 2.15f, y0 = .17f, y1 = -.2f, rx = .165f, rz = .135f, cz = .02f;
  v3 p[N + 1];
  for (int i = 0; i <= N; i++) {
    float t = (float)i / N, ang = t * turns * 6.28318f + .7f;
    p[i] = v3_(sinf(ang) * rx * (1 + .1f * t), y0 + (y1 - y0) * t, cz + cosf(ang) * rz * (1 + .1f * t));
  }
  fxp_begin();
  cable(p, N + 1, .011f, 0x1d3a20);
  fxp_layer(FXP_GLOW);
  for (int i = 1; i < N; i += 2) {
    v3 out = v3_norm(v3_(p[i].x, 0, p[i].z - cz));
    fxp_box(v3_add(p[i], v3_scale(out, .016f)), v3_(.032f, .044f, .032f), BULB[(i / 2) % 5]);
  }
  attach(d, actor_socket(a, "chest"));
}

/* a milky storage tote for a head, its lid one of the four colours bins come in, a masking tape label */
static void tote_head(dressed_t *d, actor_t *a, unsigned n) {
  static const uint32_t LID[4] = { 0x2c64c8, 0xc4342c, 0x2f8f44, 0xd8a81c };
  const float y0 = -.03f, y1 = .25f, bx = .15f, bz = .13f, tx = .175f, tz = .155f, cz = .0f;
  fxp_begin();
  taper(y0, y1, bx, bz, tx, tz, cz, 0xbfc4c2, true);
  /* ribs down the sides, as moulded tubs have */
  for (int s = -1; s <= 1; s += 2) for (int k = 0; k < 2; k++) {
    float z = cz + (k ? .06f : -.06f);
    fxp_bar(v3_(s * (bx + .004f), y0 + .02f, z), v3_(s * (tx + .004f), y1 - .03f, z), .014f, 0xaab0ae);
  }
  uint32_t lid = LID[n % 4];
  fxp_box(v3_(0, y1 + .018f, cz), v3_(tx * 2 + .03f, .036f, tz * 2 + .03f), lid);
  fxp_box(v3_(0, y1 + .044f, cz), v3_(tx * 1.3f, .016f, tz * 1.2f), lid);
  for (int s = -1; s <= 1; s += 2) fxp_box(v3_(s * (tx + .028f), y1 + .006f, cz), v3_(.03f, .022f, tz * 1.1f), lid);   /* the clip handles */
  /* the label: tape across the front, a name and a date in marker. The front leans out with the taper */
  float ly = .17f, lean = (tz - bz) / (y1 - y0), fz = cz + bz + (ly - y0) * lean + .004f;
  fxp_quad(v3_(-.11f, ly - .03f, fz - .03f * lean), v3_(.11f, ly - .03f, fz - .03f * lean), v3_(.11f, ly + .03f, fz + .03f * lean), v3_(-.11f, ly + .03f, fz + .03f * lean), 0xd9c690);
  static const float SCRIB[][2] = { {-.09f, .012f}, {-.07f, .03f}, {-.035f, .02f}, {-.012f, .016f}, {.012f, .028f}, {.046f, .02f}, {.07f, .016f} };
  for (int i = 0; i < 7; i++) {
    float x = SCRIB[i][0], w = SCRIB[i][1];
    panel(x + w / 2, ly + .008f, fz + .0025f, w, .013f, 0x1e1c22);
    if (i % 2) panel(x + w / 2 - .004f, ly - .014f, fz + .0025f, w * .7f, .009f, 0x1e1c22);
  }
  attach(d, actor_socket_head(a));
}
/* the red tag the lab hangs on anything left past its date, pinned on the chest; on every other one a
 * flattened carton strapped on as a breastplate */
static void tag_and_plate(dressed_t *d, actor_t *a, bool plate) {
  const float fz = .135f;
  fxp_begin();
  if (plate) {
    const uint32_t kraft = 0x9c7446, tape = 0xc9b88c, ink = 0x2a2018;
    fxp_box(v3_(0, -.03f, fz + .01f), v3_(.32f, .3f, .05f), kraft);
    fxp_box(v3_(0, -.03f, fz + .036f), v3_(.05f, .304f, .004f), tape);   /* packing tape down the seam */
    panel(-.09f, .07f, fz + .037f, .07f, .012f, ink);                    /* THIS SIDE UP, as bars and an arrow */
    panel(-.09f, .05f, fz + .037f, .05f, .012f, ink);
    fxp_quad(v3_(.07f, .03f, fz + .037f), v3_(.11f, .03f, fz + .037f), v3_(.09f, .08f, fz + .037f), v3_(.09f, .08f, fz + .037f), ink);
    panel(.09f, .0f, fz + .037f, .014f, .05f, ink);
    for (int s = -1; s <= 1; s += 2) {   /* straps up over the shoulders */
      v3 st[3] = { v3_(s * .12f, .11f, fz + .02f), v3_(s * .12f, .2f, .02f), v3_(s * .12f, .14f, -.13f) };
      cable(st, 3, .024f, 0x2a2a2e);
    }
  }
  /* the tag: red card on a pin, a white panel to write on */
  float tz = fz + (plate ? .04f : .0f), tx = plate ? .1f : .07f;
  fxp_bar(v3_(tx, .09f, tz + .004f), v3_(tx + .03f, .1f, tz + .004f), .006f, 0x8a8e94);
  fxp_box(v3_(tx + .02f, .045f, tz + .006f), v3_(.06f, .09f, .006f), 0xd01c24);
  panel(tx + .02f, .035f, tz + .0095f, .04f, .03f, 0xf0ece0);
  attach(d, actor_socket(a, "upperChest"));
}

/* a box head off the scrap shelf: a face plate, two LED eyes, a grille, an antenna with a light on top */
static void bot_head(dressed_t *d, actor_t *a) {
  const uint32_t shell = 0x7e858e, dark = 0x2c3036, bolt = 0xb8bec6;
  fxp_begin();
  fxp_box(v3_(0, .11f, .0f), v3_(.27f, .23f, .25f), shell);
  fxp_box(v3_(0, .115f, .127f), v3_(.22f, .17f, .006f), dark);
  for (int i = 0; i < 4; i++) panel(0, .056f + i * .014f, .1312f, .12f, .006f, 0x585e66);   /* the grille's slots */
  for (int s = -1; s <= 1; s += 2) fxp_box(v3_(s * .142f, .11f, .0f), v3_(.016f, .06f, .06f), bolt);   /* ear bolts */
  fxp_bar(v3_(.07f, .225f, -.04f), v3_(.085f, .4f, -.06f), .012f, 0x9aa2ac);
  fxp_box(v3_(.07f, .232f, -.04f), v3_(.04f, .02f, .04f), dark);
  fxp_layer(FXP_GLOW);
  for (int s = -1; s <= 1; s += 2) fxp_box(v3_(s * .055f, .14f, .132f), v3_(.045f, .03f, .006f), 0xff3a1a);
  fxp_box(v3_(.085f, .405f, -.06f), v3_(.032f, .032f, .032f), 0xff4a2a);
  attach(d, actor_socket_head(a));
}
/* VEX style aluminium C-channel along the top of each upper arm: a U of bright metal, a row of square holes */
static void c_channel(dressed_t *d, actor_t *a, int side) {
  const uint32_t alu = 0xb8bdc4, hole = 0x23272c;
  const float s = (float)side, len = .22f, y = .055f, hw = .05f, fl = .035f;
  fxp_begin();
  fxp_box(v3_(s * len / 2, y, 0), v3_(len, .008f, hw * 2), alu);
  for (int k = -1; k <= 1; k += 2) fxp_box(v3_(s * len / 2, y - fl / 2, k * hw), v3_(len, fl, .008f), alu);
  for (int i = 0; i < 7; i++) for (int k = -1; k <= 1; k += 2)
    fxp_box(v3_(s * (.02f + i * .03f), y + .0045f, k * .022f), v3_(.012f, .002f, .012f), hole);
  attach(d, actor_socket(a, side > 0 ? "leftUpperArm" : "rightUpperArm"));
}
static void status_light(dressed_t *d, actor_t *a) {
  fxp_begin();
  fxp_box(v3_(-.06f, .04f, .14f), v3_(.08f, .06f, .03f), 0x2a2e34);
  fxp_layer(FXP_GLOW);
  fxp_box(v3_(-.075f, .045f, .156f), v3_(.026f, .026f, .006f), 0x3cff5c);
  fxp_box(v3_(-.042f, .045f, .156f), v3_(.014f, .014f, .006f), 0xffb21e);
  attach(d, actor_socket(a, "upperChest"));
}

/* the rack's old console for a head: a beige CRT, its screen a green face, patch cables down the back */
static const uint32_t PATCH[7] = { 0x2f64d8, 0xe0bc22, 0xc8302c, 0x2fa04a, 0x8c949c, 0xe06a1c, 0x8a3cc8 };
static void crt_head(dressed_t *d, actor_t *a) {
  const uint32_t beige = 0x8f897a, dark = 0x24262a;
  fxp_begin();
  fxp_box(v3_(0, .15f, .08f), v3_(.44f, .38f, .08f), beige);          /* the bezel */
  frustum_z(.04f, -.3f, .2f, .17f, .13f, .11f, .14f, beige);          /* the tube's back */
  fxp_box(v3_(.16f, -.005f, .124f), v3_(.06f, .016f, .006f), dark);  /* the knobs' strip */
  for (int i = 0; i < 6; i++) {                                       /* the vents on the back */
    float z = -.06f - i * .04f;
    fxp_box(v3_(0, .14f + .12f - i * .01f + .01f, z), v3_(.18f - i * .012f, .006f, .012f), dark);
  }
  /* the cables leave the back like hair, fall past the shoulders and hang behind */
  for (int i = 0; i < 7; i++) {
    float x = -.12f + i * .04f, sx = x * 1.5f;
    v3 p[5] = { v3_(x * .8f, .2f, -.26f), v3_(x, .1f, -.33f), v3_(sx, -.08f, -.3f), v3_(sx * 1.15f, -.26f, -.24f), v3_(sx * 1.2f, -.4f, -.2f - (i % 3) * .02f) };
    cable(p, 5, .02f, PATCH[i]);
    fxp_box(p[4], v3_(.028f, .05f, .028f), 0xd8dcd8);                 /* the plug */
  }
  fxp_layer(FXP_GLOW);
  const float sz = .121f;
  panel(0, .16f, sz, .34f, .28f, 0x06240e);
  for (int i = 0; i < 9; i++) panel(0, .04f + i * .03f, sz + .0008f, .34f, .006f, 0x0b3a18);   /* scan lines */
  for (int s = -1; s <= 1; s += 2) {
    panel(s * .075f, .19f, sz + .0016f, .06f, .05f, 0x56ff7c);       /* the eyes */
    fxp_quad(v3_(s * .04f, .232f, sz + .0016f), v3_(s * .115f, .25f, sz + .0016f), v3_(s * .115f, .266f, sz + .0016f), v3_(s * .04f, .248f, sz + .0016f), 0x56ff7c);   /* brows, in */
  }
  panel(0, .085f, sz + .0016f, .16f, .022f, 0x56ff7c);                /* the mouth, a flat line */
  panel(.13f, .055f, sz + .0016f, .022f, .022f, 0x56ff7c);            /* the cursor */
  attach(d, actor_socket_head(a));
}
/* more cables off the shoulders, and a glowing one worn as a sash, right shoulder to left hip */
static void sash(dressed_t *d, actor_t *a) {
  fxp_begin();
  for (int s = -1; s <= 1; s += 2) for (int i = 0; i < 3; i++) {
    float x = s * (.11f + i * .03f);
    v3 p[4] = { v3_(x, .17f, -.02f), v3_(x * 1.1f, .14f, -.12f), v3_(x * 1.2f, -.04f, -.15f), v3_(x * 1.25f, -.22f, -.13f) };
    cable(p, 4, .018f, PATCH[(i + (s > 0 ? 3 : 0)) % 7]);
  }
  fxp_layer(FXP_GLOW);
  v3 p[9] = { v3_(-.14f, .2f, .0f), v3_(-.1f, .16f, .12f), v3_(-.02f, .05f, .15f), v3_(.07f, -.07f, .15f), v3_(.15f, -.2f, .11f),
              v3_(.17f, -.27f, .0f), v3_(.1f, -.2f, -.13f), v3_(-.04f, .0f, -.14f), v3_(-.14f, .2f, .0f) };
  cable(p, 9, .028f, 0x2cffb4);
  attach(d, actor_socket(a, "chest"));
}

/* ---- dress and undress ---- */
#define MANNEQUIN_FPS 7.f   /* the stop motion of the holiday zone: a pose every seventh of a second */
static void undress(actor_t *a, bool restore) {
  dressed_t *d = a ? slot_of(a) : NULL;
  if (!d) return;
  for (int i = 0; i < d->n; i++) {
    if (d->prop[i]->parent) t3_object_remove(d->prop[i]->parent, d->prop[i]);
    t3_release(d->prop[i]);
  }
  if (restore) {
    actor_hide_head(a, false);
    actor_set_step(a, 0);
    actor_set_scale(a, d->scale);
    if (d->repal) actor_repalette(a, &d->pal);
  }
  memset(d, 0, sizeof *d);
}
/* actor_free tells us first: the props go with it, nothing is restored on a body about to go */
static void on_free(actor_t *a) { undress(a, false); }

void enemy_dress(actor_t *a, int kind) {
  if (!a) return;
  enemy_undress(a);
  if (kind <= EK_NONE || kind >= EK_COUNT) return;
  dressed_t *d = slot_of(NULL);
  if (!d) { ds_log("enemy: no slot left to dress kind %d", kind); return; }
  actor_on_free(on_free);
  memset(d, 0, sizeof *d);
  d->a = a; d->kind = kind; d->scale = actor_scale(a);
  unsigned n = dress_count++;
  fighter_def def = enemy_def(kind);
  actor_set_scale(a, def.scale);
  /* a body in a palette without a wash (a story actor in its cast colours) takes the kind's; a fighter from
   * enemy_def has it already, and a caller's own wash stands */
  actor_palette pal = actor_get_palette(a);
  if (!pal.wash) {
    d->pal = pal; d->repal = true;
    pal.wash = def.pal.wash;
    actor_repalette(a, &pal);
  }
  switch (kind) {
    case EK_MANNEQUIN: break;
    case EK_MANNEQUIN_SANTA: santa_hat(d, a); break;
    case EK_MANNEQUIN_LIGHTS: light_string(d, a); break;
    case EK_UNCLAIMED: actor_hide_head(a, true); tote_head(d, a, n); tag_and_plate(d, a, n % 2 == 0); break;
    case EK_JUNKBOT: actor_hide_head(a, true); bot_head(d, a); c_channel(d, a, 1); c_channel(d, a, -1); status_light(d, a); break;
    case EK_SYSOP: actor_hide_head(a, true); crt_head(d, a); sash(d, a); break;
  }
  if (kind == EK_MANNEQUIN || kind == EK_MANNEQUIN_SANTA || kind == EK_MANNEQUIN_LIGHTS) actor_set_step(a, MANNEQUIN_FPS);
}
void enemy_undress(actor_t *a) { undress(a, true); }
int enemy_kind_of(const actor_t *a) { dressed_t *d = a ? slot_of(a) : NULL; return d ? d->kind : EK_NONE; }

/* ---- the suggested fighters ---- */
fighter_def enemy_def(int kind) {
  fighter_def f;
  memset(&f, 0, sizeof f);
  f.pal = ACTOR_PAL_NONE; f.scale = 1; f.power = 1; f.speed = 1;
  f.dress = kind > EK_NONE && kind < EK_COUNT ? kind : EK_NONE;
  switch (kind) {
    /* plaster: a cool white (under warm light a warm one reads as bare skin), the clothes' edges kept. Three bodies, so a group of four
     * stays inside fight.c's pool of two per model */
    case EK_MANNEQUIN:        f.model = AM_M; f.pal.wash = 0x4cc9cfd6; f.name = "Mannequin"; f.weapon = WP_ARC; f.ai = AI_DUELIST; f.hp = 55; f.power = .75f; f.speed = .9f; break;
    case EK_MANNEQUIN_SANTA:  f.model = AM_W; f.pal.wash = 0x4cc9cfd6; f.name = "Santa Mannequin"; f.weapon = WP_MAUL; f.ai = AI_BRUISER; f.hp = 70; f.power = .85f; f.speed = .85f; break;
    case EK_MANNEQUIN_LIGHTS: f.model = AM_D2; f.pal.wash = 0x4cc9cfd6; f.name = "Lit Mannequin"; f.weapon = WP_SPEAR; f.ai = AI_SKIRMISH; f.hp = 50; f.power = .7f; f.speed = 1.f; break;
    /* grey tub plastic, a little of the clothes left in it */
    case EK_UNCLAIMED:        f.model = AM_J2; f.pal.wash = 0x4c8a929c; f.name = "Unclaimed"; f.weapon = WP_TWIN; f.ai = AI_SKIRMISH; f.hp = 60; f.power = .7f; f.speed = 1.05f; break;
    /* brushed metal: more of the shading kept, so the folds read as dents */
    case EK_JUNKBOT:          f.model = AM_N2; f.pal.wash = 0x8c9aa1aa; f.name = "Junkbot"; f.weapon = WP_MAUL; f.ai = AI_BRUISER; f.hp = 95; f.power = .95f; f.speed = .9f; f.scale = 1.04f; break;
    case EK_SYSOP:            f.model = AM_A; f.pal.wash = 0x80262a33; f.name = "SYSOP"; f.weapon = WP_SPEAR; f.ai = AI_BOSS; f.hp = 420; f.power = 1.1f; f.speed = 1.1f; f.scale = 1.12f; break;
    default: f.model = AM_M; f.name = "?"; break;
  }
  return f;
}
