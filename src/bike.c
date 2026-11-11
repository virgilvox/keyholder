/* bike.c: see bike.h. A city bike in the HeatSync orange: diamond frame, black tyres on silver rims
 * with three crossed spokes (so the spin reads), a fork and bars that steer, cranks that turn. Each
 * moving part is its own prop group under a lean group under the root, so a pose is a few rotations. */
#include "bike.h"
#include "fx.h"

static t3_object *root, *lean_g, *front_g, *wheel_f, *wheel_r, *crank_g, *stand_g;
static v3 at; static float at_yaw;
static bool ridden;
/* parked, it leans on its kickstand, on the side away from the chain (-x): the stand's foot is on the ground
 * at this lean */
#define PARK_LEAN .2f

#define FRAME 0xe8642au
#define TYRE 0x17171bu
#define RIM 0xb4bac2u
#define DARK 0x26262cu

/* fxp_end puts the part in the scene: move it under a parent instead */
static t3_object *part_under(t3_object *parent, v3 p) {
  t3_object *o = fxp_end();
  t3_retain(o);
  t3_object_remove(o->parent, o);
  t3_object_add(parent, o);
  t3_release(o);
  t3_object_set_position(o, p.x, p.y, p.z);
  return o;
}
static t3_object *group_under(t3_object *parent, v3 p) {
  t3_object *g = t3_group_new();
  t3_object_add(parent, g); t3_release(g);
  t3_object_set_position(g, p.x, p.y, p.z);
  return g;
}
static void wheel_parts(void) {   /* about the axle at the origin, turning about x */
  fxp_ring(v3_(0, 0, 0), v3_(1, 0, 0), BIKE_WHEEL_R - .025f, .028f, 16, TYRE);
  fxp_ring(v3_(0, 0, 0), v3_(1, 0, 0), BIKE_WHEEL_R - .065f, .012f, 16, RIM);
  for (int i = 0; i < 3; i++) {
    float a = i * 1.0472f, c = cosf(a) * (BIKE_WHEEL_R - .07f), s = sinf(a) * (BIKE_WHEEL_R - .07f);
    fxp_bar(v3_(0, -s, -c), v3_(0, s, c), .014f, RIM);
  }
  fxp_box(v3_(0, 0, 0), v3_(.1f, .05f, .05f), DARK);
}

void bike_init(t3_scene *scene) {
  if (root) return;
  root = t3_group_new(); t3_object_add(&scene->base, root); t3_release(root);
  lean_g = group_under(root, v3_(0, 0, 0));
  /* the frame, in the bike's frame: +z forward, y up, the origin on the ground between the wheels */
  v3 ra = v3_(0, BIKE_WHEEL_R, -.52f), bb = v3_(0, .3f, -.02f), st = v3_(0, .84f, -.2f), ht = v3_(0, .9f, .38f), hb = v3_(0, .7f, .43f);
  fxp_begin();
  fxp_bar(bb, st, .045f, FRAME);                            /* seat tube */
  fxp_bar(bb, hb, .05f, FRAME);                             /* down tube */
  fxp_bar(v3_(0, .8f, -.19f), ht, .042f, FRAME);            /* top tube */
  fxp_bar(hb, ht, .05f, FRAME);                             /* head tube */
  for (int s = -1; s <= 1; s += 2) {
    fxp_bar(v3_(s * .055f, bb.y, bb.z), v3_(s * .06f, ra.y, ra.z), .03f, FRAME);          /* chain stays */
    fxp_bar(v3_(s * .03f, .79f, -.19f), v3_(s * .06f, ra.y, ra.z), .028f, FRAME);        /* seat stays */
  }
  fxp_bar(st, v3_(0, BIKE_SEAT_Y - .02f, BIKE_SEAT_Z), .03f, RIM);                      /* seat post */
  fxp_box(v3_(0, BIKE_SEAT_Y, BIKE_SEAT_Z - .02f), v3_(.15f, .05f, .2f), DARK);          /* saddle */
  fxp_box(v3_(0, BIKE_SEAT_Y - .005f, BIKE_SEAT_Z + .1f), v3_(.06f, .04f, .12f), DARK);  /* its nose */
  fxp_bar(v3_(.075f, bb.y, bb.z + .02f), v3_(.075f, ra.y, ra.z), .02f, 0x55585eu);       /* chain */
  fxp_box(v3_(0, .62f, -.62f), v3_(.16f, .025f, .3f), RIM);                             /* a rear rack */
  fxp_bar(v3_(-.07f, .62f, -.5f), v3_(-.06f, ra.y, ra.z), .018f, RIM);
  fxp_bar(v3_(.07f, .62f, -.5f), v3_(.06f, ra.y, ra.z), .018f, RIM);
  fxp_layer(FXP_GLOW); fxp_box(v3_(0, .6f, -.78f), v3_(.08f, .04f, .02f), 0xff2a20u);   /* tail light */
  part_under(lean_g, v3_(0, 0, 0));
  /* the steering: fork, stem, bars and the front wheel turn about the head tube */
  front_g = group_under(lean_g, v3_(0, 0, .42f));
  fxp_begin();
  for (int s = -1; s <= 1; s += 2) fxp_bar(v3_(s * .045f, .72f, .01f), v3_(s * .055f, BIKE_WHEEL_R, .1f), .026f, FRAME);
  fxp_bar(v3_(0, .9f, -.04f), v3_(0, 1.02f, -.1f), .035f, DARK);          /* stem */
  /* swept-back cruiser bars: out from the stem, then back toward the rider, so the grips sit within reach */
  fxp_bar(v3_(-.2f, 1.02f, -.1f), v3_(.2f, 1.02f, -.1f), .03f, DARK);
  for (int s = -1; s <= 1; s += 2) {
    fxp_bar(v3_(s * .2f, 1.02f, -.1f), v3_(s * .25f, 1.03f, -.24f), .03f, DARK);
    fxp_bar(v3_(s * .25f, 1.03f, -.24f), v3_(s * .25f, 1.03f, -.34f), .045f, TYRE);   /* grips */
  }
  fxp_cyl(v3_(.16f, 1.04f, -.1f), .03f, .028f, .022f, 8, 0xd8c040u);      /* the bell */
  fxp_layer(FXP_GLOW); fxp_box(v3_(0, .86f, .02f), v3_(.07f, .06f, .05f), 0xfff2c0u);   /* head lamp */
  part_under(front_g, v3_(0, 0, 0));
  fxp_begin(); wheel_parts(); wheel_f = part_under(front_g, v3_(0, BIKE_WHEEL_R, .1f));
  fxp_begin(); wheel_parts(); wheel_r = part_under(lean_g, ra);
  /* the cranks, about the bottom bracket */
  crank_g = group_under(lean_g, bb);
  fxp_begin();
  fxp_ring(v3_(.075f, 0, 0), v3_(1, 0, 0), .1f, .012f, 12, 0x8a9098u);
  fxp_bar(v3_(.09f, 0, 0), v3_(.09f, -.17f, 0), .025f, RIM);
  fxp_bar(v3_(-.09f, 0, 0), v3_(-.09f, .17f, 0), .025f, RIM);
  fxp_box(v3_(.15f, -.17f, 0), v3_(.1f, .025f, .07f), DARK);
  fxp_box(v3_(-.15f, .17f, 0), v3_(.1f, .025f, .07f), DARK);
  part_under(crank_g, v3_(0, 0, 0));
  /* the kickstand: from the chain stay behind the bottom bracket, down and back to a foot on the ground */
  stand_g = group_under(lean_g, v3_(0, 0, 0));
  { v3 foot = v3_(-.21f, .21f * tanf(PARK_LEAN) + .012f, -.26f);
    fxp_begin();
    fxp_bar(v3_(-.06f, .27f, -.1f), foot, .018f, DARK);
    fxp_box(v3_(foot.x, foot.y - .006f, foot.z), v3_(.05f, .014f, .08f), DARK);
    part_under(stand_g, v3_(0, 0, 0)); }
  /* the parts use YXZ: turn about y, then roll and spin in that frame */
  root->rotation.order = T3_YXZ; lean_g->rotation.order = T3_YXZ;
  root->visible = false;
}
void bike_show(bool on) { if (root) root->visible = on; }
bool bike_shown(void) { return root && root->visible; }
v3 bike_pos(void) { return at; }
float bike_yaw(void) { return at_yaw; }
void bike_ridden(bool on) { ridden = on; }
void bike_pose(v3 p, float yaw, float lean, float steer, float wheel, float crank) {
  if (!root) return;
  at = p; at_yaw = yaw;
  /* a parked pose (a little lean, nobody on it) stands on the kickstand: walked or ridden, the stand is up */
  bool parked = !ridden && lean >= .07f;
  stand_g->visible = parked;
  if (parked) lean = -PARK_LEAN;
  t3_object_set_position(root, p.x, p.y, p.z);
  t3_object_set_rotation(root, 0, yaw, 0);
  t3_object_set_rotation(lean_g, 0, 0, -lean);
  t3_object_set_rotation(front_g, 0, steer, 0);
  t3_object_set_rotation(wheel_f, wheel, 0, 0);
  t3_object_set_rotation(wheel_r, wheel, 0, 0);
  t3_object_set_rotation(crank_g, crank, 0, 0);
}

/* a grip's place in the world (side 0 right, 1 left), after bike_pose: where a rider's hand goes */
v3 bike_grip(int side) {
  if (!front_g) return v3_(0, 0, 0);
  t3_object_update_world_matrix(front_g, true, false);
  const float *m = front_g->matrix_world.e;
  float x = side ? .25f : -.25f, y = 1.03f, z = -.29f;
  return v3_(m[0] * x + m[4] * y + m[8] * z + m[12], m[1] * x + m[5] * y + m[9] * z + m[13], m[2] * x + m[6] * y + m[10] * z + m[14]);
}
