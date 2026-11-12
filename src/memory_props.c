/* memory_props.c: the things the attic's flashbacks set out (see memory_props.h). Each is built once with the
 * fx prop builder, in the game's vertex coloured look, and parked hidden under the scene root (not the town
 * group: a memory may be on the attic's stage). story.c's PROP op hands over any name it does not own. */
#include "memory_props.h"
#include "fx.h"
#include "world.h"
#include "attic_env.h"   /* the desert set's balloon */
#include <string.h>
#include <math.h>

typedef enum { MP_PINK_FRONT, MP_LASER_CRATE, MP_STAGE_ROBOT, MP_MOVE_BOXES, MP_HAXMAS, MP_RACK_2018, MP_N } mp_kind;
static const char *const MP_NAME[MP_N] = { "pink_front", "laser_crate", "stage_robot", "move_boxes", "haxmas", "rack_2018" };
static t3_object *MP[MP_N];
static float mp_t[MP_N];

/* one forty's storefront the night of the work party: a coat of pink over the old facade, the new sign board
 * still leaning on the sill, drop cloths, and the window glass taped off. Built facing +z (the street). */
static void build_pink_front(void) {
  const uint32_t pink = 0xe98bb0, pink_d = 0xc8668e, glass = 0x2a3440, tape = 0x3a6ad0, cloth = 0xd8d2c4, board = 0xf2eee4;
  fxp_box(v3_(0, 2.0f, -.05f), v3_(7.6f, 4.0f, .1f), pink);                 /* the painted wall */
  fxp_box(v3_(0, 4.1f, -.02f), v3_(7.8f, .25f, .16f), pink_d);              /* the parapet's cap */
  for (int s = -1; s <= 1; s += 2) {                                         /* the two shop windows */
    fxp_box(v3_(s * 2.2f, 1.55f, .01f), v3_(2.6f, 2.1f, .02f), glass);
    fxp_box(v3_(s * 2.2f, 2.62f, .025f), v3_(2.7f, .05f, .01f), tape);      /* painter's tape at their edges */
    fxp_box(v3_(s * 2.2f, .48f, .025f), v3_(2.7f, .05f, .01f), tape);
  }
  fxp_box(v3_(0, 1.1f, .01f), v3_(1.0f, 2.2f, .02f), 0x1a1c20);             /* the door */
  fxp_box(v3_(0, .01f, .9f), v3_(7.0f, .02f, 1.6f), cloth);                  /* drop cloths on the sidewalk */
  fxp_box(v3_(2.9f, .45f, .35f), v3_(1.8f, .55f, .04f), board);              /* the new sign, leaning, not yet up */
  fxp_box(v3_(2.9f, .48f, .38f), v3_(1.5f, .14f, .01f), 0xd8402a);
  for (int k = 0; k < 4; k++) fxp_cyl(v3_(-3.2f + k * .32f, 0, .9f), .19f, .085f, .085f, 10, k == 2 ? 0xf2f0ea : pink);   /* paint cans */
  fxp_box(v3_(-2.2f, .02f, 1.2f), v3_(.32f, .04f, .22f), 0x3a3a3a);         /* a roller tray */
  fxp_bar(v3_(-1.4f, 0, .4f), v3_(-1.1f, 2.2f, .05f), .03f, 0xb08a5a);     /* an extension pole on the wall */
  fxp_box(v3_(-1.1f, 2.25f, .1f), v3_(.25f, .08f, .08f), pink);
  fxp_layer(FXP_GLOW);
  fxp_box(v3_(0, 3.5f, .08f), v3_(.18f, .1f, .1f), 0xfff0c8);              /* a work light clamped over the door */
}

/* the laser cutter's shipping crate, months late: a big plywood box on a pallet, stencilled, taped and tagged */
static void build_laser_crate(void) {
  const uint32_t ply = 0xc4a272, ply_d = 0x9c7e52, pallet = 0x8a6a44, stencil = 0x222222, tag = 0xf0d040;
  fxp_box(v3_(0, .07f, 0), v3_(1.6f, .14f, 1.1f), pallet);
  fxp_box(v3_(0, .76f, 0), v3_(1.5f, 1.24f, 1.0f), ply);
  for (int s = -1; s <= 1; s += 2) {
    fxp_box(v3_(s * .74f, .76f, 0), v3_(.04f, 1.28f, 1.04f), ply_d);         /* the battens round each face */
    fxp_box(v3_(0, .76f, s * .51f), v3_(1.54f, .08f, .03f), ply_d);
  }
  fxp_box(v3_(0, 1.4f, 0), v3_(1.54f, .04f, 1.04f), ply_d);
  fxp_box(v3_(-.2f, .9f, .505f), v3_(.7f, .1f, .01f), stencil);              /* THIS SIDE UP, as bars */
  fxp_box(v3_(-.2f, .72f, .505f), v3_(.5f, .06f, .01f), stencil);
  fxp_box(v3_(.45f, 1.0f, .506f), v3_(.22f, .3f, .01f), tag);                 /* the customs tag */
  fxp_box(v3_(.45f, 1.0f, .507f), v3_(.16f, .02f, .01f), 0xc02020);
  fxp_box(v3_(0, 1.42f, 0), v3_(.1f, .01f, 1.06f), 0xc8b070);                 /* tape over the lid */
}

/* one of the theatre robots built against the clock: six feet of plywood torso, aluminium arms on dryer duct
 * shoulders, a bucket head with two lamp eyes, on a caster base so a stagehand can roll it */
static void build_stage_robot(void) {
  const uint32_t ply = 0xb89464, alu = 0xb8c0c6, duct = 0xc8ccd0, base = 0x2a2c30, red = 0xd04030;
  fxp_box(v3_(0, .12f, 0), v3_(.9f, .16f, .7f), base);
  for (int i = 0; i < 4; i++) fxp_cyl(v3_(i & 1 ? .38f : -.38f, 0, i & 2 ? .28f : -.28f), .06f, .05f, .05f, 8, 0x111111);
  fxp_box(v3_(0, .62f, 0), v3_(.5f, .84f, .4f), ply);                         /* the hips and legs, one box */
  fxp_box(v3_(0, 1.32f, 0), v3_(.78f, .62f, .5f), ply);                        /* the chest */
  fxp_box(v3_(0, 1.36f, .255f), v3_(.4f, .26f, .01f), 0x30343a);               /* a panel with switches */
  for (int k = 0; k < 4; k++) fxp_box(v3_(-.12f + k * .08f, 1.36f, .262f), v3_(.03f, .05f, .01f), k & 1 ? red : 0xe0c020);
  for (int s = -1; s <= 1; s += 2) {
    fxp_cyl(v3_(s * .46f, 1.5f, 0), .14f, .12f, .12f, 10, duct);              /* shoulders */
    fxp_bar(v3_(s * .48f, 1.48f, 0), v3_(s * .55f, .95f, .14f), .045f, alu);   /* arms */
    fxp_box(v3_(s * .56f, .9f, .18f), v3_(.12f, .1f, .14f), alu);              /* claw hands */
  }
  fxp_cyl(v3_(0, 1.63f, 0), .3f, .2f, .23f, 12, 0x9aa0a6);                      /* the bucket head */
  fxp_layer(FXP_GLOW);
  for (int s = -1; s <= 1; s += 2) fxp_box(v3_(s * .08f, 1.8f, .2f), v3_(.07f, .07f, .03f), 0xfff2b0);
}

/* the move into 108: a stack of boxes, a rolled rug, a shop vac, and a dolly */
static void build_move_boxes(void) {
  const uint32_t box = 0xb08655, box_d = 0x9a7448, tape = 0xd0b88a;
  for (int i = 0; i < 9; i++) {
    float x = (i % 3) * .55f - .55f, y = (i / 3) * .42f, z = ((i * 7) % 3) * .08f;
    fxp_box(v3_(x, y + .2f, z), v3_(.5f, .4f, .45f), i & 1 ? box : box_d);
    fxp_box(v3_(x, y + .401f, z), v3_(.08f, .002f, .46f), tape);
  }
  fxp_cyl(v3_(1.3f, .14f, -.6f), .14f, .14f, .14f, 10, 0x8a3a2a);              /* a rolled rug lying down, cheap */
  fxp_bar(v3_(1.0f, .14f, -.6f), v3_(1.0f, .14f, .9f), .14f, 0x8a3a2a);
  fxp_cyl(v3_(-1.3f, 0, .5f), .5f, .2f, .2f, 10, 0xd0b020);                    /* a shop vac */
}

/* HAXMAS in the lab: strings of lights in swags, a small tree on a table, a paper banner's colours */
static void build_haxmas(void) {
  const uint32_t cols[4] = { 0xff4050, 0x40e060, 0x5080ff, 0xffc040 };
  fxp_box(v3_(0, .72f, 0), v3_(1.0f, .04f, .6f), 0xe0d8c8);                    /* a table */
  for (int i = 0; i < 4; i++) fxp_bar(v3_(i & 1 ? .45f : -.45f, 0, i & 2 ? .25f : -.25f), v3_(i & 1 ? .45f : -.45f, .72f, i & 2 ? .25f : -.25f), .03f, 0x8a8a90);
  fxp_cyl(v3_(0, .74f, 0), .7f, .02f, .32f, 10, 0x2a6a3a);                     /* the tree, a cone */
  fxp_cyl(v3_(0, .74f, 0), .1f, .06f, .06f, 6, 0x6a4a2a);
  for (int k = 0; k < 6; k++) fxp_box(v3_(-.4f + k * .16f, .82f, .2f), v3_(.12f, .1f, .12f), cols[k & 3]);   /* presents */
  fxp_layer(FXP_GLOW);
  fxp_box(v3_(0, 1.48f, 0), v3_(.08f, .08f, .08f), 0xfff0a0);                  /* the star */
  for (int s = 0; s < 3; s++) {                                                /* swags of lights overhead, out from the tree */
    for (int k = 0; k < 14; k++) {
      float t = k / 13.f, x = -3.5f + t * 7.0f, y = 2.7f - sinf(t * DS_PI) * .45f, z = -2.0f + s * 2.0f;
      fxp_box(v3_(x, y, z), v3_(.05f, .05f, .05f), cols[(k + s) & 3]);
    }
  }
  for (int k = 0; k < 10; k++) { float a = k * .63f; fxp_box(v3_(cosf(a) * .2f, .9f + k * .05f, sinf(a) * .2f), v3_(.03f, .03f, .03f), cols[k & 3]); }
}

/* the rack on its day up the stairs, 2018: the same 19 inch rack, half empty, strapped to a hand truck */
static void build_rack_2018(void) {
  const uint32_t frame = 0x24272c, unit = 0x3a3e45, strap = 0xe0a020, truck = 0xb02a20;
  fxp_box(v3_(0, 1.05f, 0), v3_(.6f, 1.9f, .9f), frame);
  for (int k = 0; k < 5; k++) fxp_box(v3_(0, .4f + k * .1f, .46f), v3_(.5f, .08f, .02f), unit);
  fxp_box(v3_(0, 1.0f, .5f), v3_(.5f, .08f, .04f), 0x5c5f63);                  /* the pizza box, on its shelf */
  for (int k = 0; k < 2; k++) fxp_box(v3_(0, .6f + k * .7f, 0), v3_(.64f, .05f, .94f), strap);
  fxp_bar(v3_(-.25f, 0, -.5f), v3_(-.25f, 1.5f, -.55f), .03f, truck);          /* the hand truck behind it */
  fxp_bar(v3_(.25f, 0, -.5f), v3_(.25f, 1.5f, -.55f), .03f, truck);
  fxp_box(v3_(0, .03f, -.2f), v3_(.6f, .04f, .5f), truck);
  for (int s = -1; s <= 1; s += 2) fxp_ring(v3_(s * .34f, .12f, -.5f), v3_(1, 0, 0), .12f, .05f, 10, 0x18181c);
  fxp_layer(FXP_GLOW);
  fxp_box(v3_(.2f, 1.0f, .525f), v3_(.03f, .02f, .01f), 0x40d070);
}

static t3_object *build(mp_kind k) {
  fxp_begin(); fxp_layer(FXP_BODY);
  switch (k) {
    case MP_PINK_FRONT: build_pink_front(); break;
    case MP_LASER_CRATE: build_laser_crate(); break;
    case MP_STAGE_ROBOT: build_stage_robot(); break;
    case MP_MOVE_BOXES: build_move_boxes(); break;
    case MP_HAXMAS: build_haxmas(); break;
    case MP_RACK_2018: build_rack_2018(); break;
    default: break;
  }
  t3_object *o = fxp_end(); o->visible = false; return o;
}

bool memory_prop(const char *name, int mode, v3 pos, float yaw) {
  /* the balloon is the desert set's own: on its tarp (still), filling (circle), let go (straight) */
  if (name && !strcmp(name, "balloon")) { attic_env_balloon(mode == 0 ? -1 : mode == 1 ? 0 : mode == 2 ? 1 : 2); return true; }
  bool show = mode != 0;
  int k = -1;
  for (int i = 0; i < MP_N; i++) if (name && !strcmp(name, MP_NAME[i])) k = i;
  if (k < 0) return false;
  if (!MP[k]) { if (!show) return true; MP[k] = build((mp_kind)k); }
  MP[k]->visible = show; mp_t[k] = 0;
  if (show) { t3_object_set_position(MP[k], pos.x, pos.y, pos.z); t3_object_set_rotation(MP[k], 0, yaw, 0); }
  return true;
}
void memory_props_hide(void) { for (int i = 0; i < MP_N; i++) if (MP[i]) MP[i]->visible = false; }
