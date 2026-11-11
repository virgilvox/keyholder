/* facades_town.inc: the town's own set pieces, placed by hand from photos (sources in docs/PLACES.md).
 * Chupacabra Taproom's front and its patio on Robson, the Mesa Arts Center's Shadow Walk (masts with
 * sails, coloured glass canopies, the arroyo, the sign wall) and loose street things.
 *
 * TOWN_SET is the table: one line places one piece. x, z, yaw are world (yaw 0 faces +z, south);
 * a and b are the piece's own sizes (see each kind); v is a variant. A piece with a name also answers
 * world_place: a picnic table "foo" gives foo_table (the tabletop centre, y is its height), foo_seat_a
 * and foo_seat_b (the ground under each bench's middle, facing the table) and foo_patio (standing at
 * the table's west end of its long axis, facing it). See town_set_places in city.c.
 * To look at the layout: node tools/town-map.mjs out.png x0 z0 x1 z1 4
 * Included by city.c after the other facade files: prop, mesa_sail, mesa_masonry and the rest. */

enum {
  SET_PICNIC,        /* a: table length (2.4). Long axis along local x, a bench on each side */
  SET_PATIO_RAIL,    /* a x b: steel frame and railing, open toward local -z (the building), a gap mid +z */
  SET_STOP,          /* a stop sign on a post */
  SET_CACTUS_BED,    /* a x b: low brick planter, red mulch, prickly pear and agave */
  SET_SAIL_MAST,     /* a: mast height. The Shadow Walk's white masts, three fabric sails each */
  SET_GLASS_CANOPY,  /* a x b: steel frame and coloured glass roof (v picks the colours), its colour pool below */
  SET_ARROYO,        /* a long, b wide along yaw: the dry wash and water runnel down the walk */
  SET_MAC_SIGN,      /* a: length. The low sign wall at Center and Main */
  SET_MURAL,         /* a x b on a wall facing yaw: a painted panel (v picks the palette) */
  SET_DUMPSTER,      /* a steel dumpster with lids (v: 0 green, 1 blue recycling) */
  SET_BINS,          /* a: how many wheelie bins in a row */
  SET_BIKE_RACK,     /* a: how many hoops */
  SET_UPOLE,         /* a utility pole. Consecutive poles with the same v are strung together */
  SET_SPOT,          /* nothing built: only a named place (the name, x, z, yaw) */
  SET_PALM,          /* a: trunk height. A date palm in a round planter */
  SET_KINDS
};
typedef struct { short kind; float x, z, yaw, a, b; short v; const char *name; } town_set;

static const town_set TOWN_SET[] = {
  /* Chupacabra Taproom, 14 N Robson (photos 2019, street view Oct 2025): the patio in front of the door,
   * three long tables inside a dark steel frame, one more outside by the white wall to the south */
  {SET_PATIO_RAIL, -220.4f, -41.9f, 1.5708f, 8.2f, 8.4f, 0, NULL},
  {SET_PICNIC, -219.6f, -44.9f, 0, 2.4f, 0, 0, NULL},
  {SET_PICNIC, -219.6f, -43.0f, 0, 2.4f, 0, 0, "chupacabra"},
  {SET_PICNIC, -219.6f, -39.4f, 0, 2.4f, 0, 0, NULL},
  {SET_PICNIC, -227.0f, -35.2f, 0, 2.4f, 0, 0, NULL},
  {SET_STOP, -215.6f, -46.6f, 1.5708f, 0, 0, 0, NULL},
  {SET_CACTUS_BED, -224.0f, -47.0f, 0, 2.6f, .8f, 0, NULL},
  {SET_MURAL, -233.5f, -33.86f, 3.1416f, 12.f, 3.6f, 0, NULL},
  {SET_BINS, -232.0f, -46.6f, 3.1416f, 6, 0, 0, NULL},
  /* Mesa Arts Center, 1 E Main: the Shadow Walk runs south from the plaza between the theatre wing (east)
   * and the studios and galleries (west); masts with sails, coloured glass, the arroyo */
  {SET_SAIL_MAST, 350.f, 72.f, .2f, 15.f, 0, 0, NULL},
  {SET_SAIL_MAST, 344.f, 96.f, 1.4f, 14.f, 0, 1, NULL},
  {SET_SAIL_MAST, 340.5f, 118.f, 2.6f, 15.5f, 0, 2, NULL},
  {SET_SAIL_MAST, 346.f, 140.f, .9f, 14.5f, 0, 0, NULL},
  {SET_SAIL_MAST, 351.f, 164.f, 2.1f, 15.f, 0, 1, NULL},
  {SET_GLASS_CANOPY, 336.f, 108.f, .25f, 6.5f, 3.6f, 0, NULL},
  {SET_GLASS_CANOPY, 354.f, 126.f, -.2f, 5.5f, 3.6f, 1, NULL},
  {SET_GLASS_CANOPY, 337.f, 150.f, .2f, 6.f, 3.4f, 2, NULL},
  {SET_GLASS_CANOPY, 322.f, 62.f, 0, 7.f, 4.f, 3, NULL},
  {SET_GLASS_CANOPY, 356.f, 168.f, .1f, 5.f, 3.4f, 4, NULL},
  {SET_ARROYO, 352.f, 54.f, -.2f, 30.f, 1.4f, 0, NULL},
  {SET_ARROYO, 346.f, 83.f, -.25f, 28.f, 1.4f, 0, NULL},
  {SET_ARROYO, 343.f, 110.f, .12f, 26.f, 1.4f, 0, NULL},
  {SET_ARROYO, 346.5f, 135.f, .14f, 24.f, 1.4f, 0, NULL},
  {SET_ARROYO, 349.f, 158.f, 0, 26.f, 1.4f, 0, NULL},
  {SET_MAC_SIGN, 312.f, 45.5f, 3.1416f, 9.f, 0, 0, NULL},
  /* the date palms along the plaza's Main edge and up the walk (photos 2009, 2010, 2016) */
  {SET_PALM, 298.f, 50.f, 0, 8.5f, 0, 0, NULL}, {SET_PALM, 310.f, 52.f, 0, 9.5f, 0, 1, NULL}, {SET_PALM, 336.f, 50.f, 0, 8.f, 0, 2, NULL},
  {SET_PALM, 362.f, 49.f, 0, 9.f, 0, 3, NULL}, {SET_PALM, 300.f, 64.f, 0, 10.f, 0, 4, NULL}, {SET_PALM, 330.f, 80.f, 0, 8.5f, 0, 5, NULL},
  {SET_PALM, 336.f, 128.f, 0, 9.f, 0, 6, NULL}, {SET_PALM, 344.5f, 150.f, 0, 8.f, 0, 7, NULL},
  {SET_BIKE_RACK, 300.f, 44.f, 0, 5, 0, 0, NULL},
  /* the lab's back: where the welding table sits by the back door (its footprint's centre on the ground,
   * the long side along the wall, yaw facing out into the alley), and the middle of the alley */
  {SET_SPOT, 1.4f, -42.0f, 3.1416f, 0, 0, 0, "lab_back_table"},
  {SET_SPOT, 2.5f, -47.0f, 0, 0, 0, 0, "lab_alley"},
  {SET_SPOT, -1.2f, -47.4f, -1.5708f, 0, 0, 0, "lab_truck_tailgate"},   /* behind the crew's pickup (city.c TRUCK_X), facing it */
  /* the lab's block, behind Main: bins at the backs the story walks past */
  {SET_DUMPSTER, -6.0f, -51.35f, 3.1416f, 0, 0, 0, NULL},
  {SET_DUMPSTER, 7.5f, -51.35f, 3.1416f, 0, 0, 1, NULL},
  {SET_MURAL, -40.0f, -34.42f, 3.1416f, 9.f, 4.f, 1, NULL},
  {SET_MURAL, -150.0f, 75.6f, 0, 12.f, 4.f, 2, NULL},
  /* the utility runs down the alleys behind both blocks, west to east (v: the run) */
  {SET_UPOLE, -190.f, -49.f, 0, 0, 0, 1, NULL}, {SET_UPOLE, -160.f, -49.f, 0, 0, 0, 1, NULL}, {SET_UPOLE, -130.f, -49.5f, 0, 0, 0, 1, NULL},
  {SET_UPOLE, -100.f, -49.5f, 0, 0, 0, 1, NULL}, {SET_UPOLE, -70.f, -48.5f, 0, 0, 0, 1, NULL}, {SET_UPOLE, -40.f, -48.5f, 0, 0, 0, 1, NULL},
  {SET_UPOLE, -20.f, -48.5f, 0, 0, 0, 1, NULL},
  {SET_UPOLE, -190.f, 91.f, 0, 0, 0, 2, NULL}, {SET_UPOLE, -160.f, 91.f, 0, 0, 0, 2, NULL}, {SET_UPOLE, -130.f, 91.5f, 0, 0, 0, 2, NULL},
  {SET_UPOLE, -100.f, 91.5f, 0, 0, 0, 2, NULL}, {SET_UPOLE, -70.f, 91.5f, 0, 0, 0, 2, NULL}, {SET_UPOLE, -40.f, 91.5f, 0, 0, 0, 2, NULL},
  {SET_UPOLE, -10.f, 92.f, 0, 0, 0, 2, NULL}, {SET_UPOLE, 22.f, 92.f, 0, 0, 0, 2, NULL},
  {SET_UPOLE, 52.f, -49.f, 0, 0, 0, 3, NULL}, {SET_UPOLE, 82.f, -49.f, 0, 0, 0, 3, NULL}, {SET_UPOLE, 112.f, -49.f, 0, 0, 0, 3, NULL},
  {SET_UPOLE, 142.f, -49.f, 0, 0, 0, 3, NULL}, {SET_UPOLE, 172.f, -49.f, 0, 0, 0, 3, NULL}, {SET_UPOLE, 200.f, -46.f, 0, 0, 0, 3, NULL},
  {SET_UPOLE, -335.f, -48.f, 0, 0, 0, 4, NULL}, {SET_UPOLE, -305.f, -48.f, 0, 0, 0, 4, NULL}, {SET_UPOLE, -275.f, -48.f, 0, 0, 0, 4, NULL},
  {SET_UPOLE, -250.f, -48.f, 0, 0, 0, 4, NULL},
};
#define TOWN_NSET (int)(sizeof TOWN_SET / sizeof TOWN_SET[0])

/* ── materials ── */
static t3_material *T_frame, *T_wood, *T_white, *T_sail, *T_tan, *T_mulch, *T_pad, *T_agave, *T_water, *T_stone, *T_sand,
  *T_red, *T_dump[2], *T_bin, *T_sign, *T_steelgrey, *T_glass[5], *T_pool[5], *T_paint[3][4], *T_bulb, *T_poleW, *T_wire;
static void town_materials(void) {
  T_frame = toy_paint(.23f, .16f, .11f); T_wood = toy_paint(.66f, .5f, .32f); T_white = toy_paint(.93f, .93f, .91f);
  T_sail = toy_paint(.96f, .95f, .9f); T_tan = toy_paint(.8f, .68f, .5f); T_mulch = toy_paint(.45f, .22f, .13f);
  T_pad = toy_paint(.4f, .55f, .3f); T_agave = toy_paint(.5f, .6f, .52f); T_water = toy_paint(.17f, .3f, .32f);
  T_stone = toy_paint(.6f, .54f, .46f); T_sand = toy_paint(.76f, .68f, .55f); T_red = toy_paint(.75f, .08f, .08f);
  T_dump[0] = toy_paint(.12f, .3f, .2f); T_dump[1] = toy_paint(.12f, .24f, .5f); T_bin = toy_paint(.08f, .08f, .09f);
  T_sign = toy_paint(.62f, .63f, .62f); T_steelgrey = toy_paint(.5f, .52f, .54f);
  /* the glass glows a little by day and much more after dark (city.c puts basic materials with the lamps) */
  static const float G[5][3] = {{.12f, .26f, .62f}, {.36f, .14f, .55f}, {.16f, .48f, .28f}, {.75f, .42f, .08f}, {.6f, .12f, .32f}};
  static const float PO[5][3] = {{.5f, .6f, .86f}, {.66f, .52f, .82f}, {.55f, .78f, .58f}, {.96f, .78f, .48f}, {.88f, .55f, .66f}};
  for (int i = 0; i < 5; i++) { T_glass[i] = glow(G[i][0], G[i][1], G[i][2]); T_pool[i] = toy_paint(PO[i][0], PO[i][1], PO[i][2]); }
  static const float PA[3][4][3] = {
    {{.95f, .55f, .2f}, {.2f, .55f, .62f}, {.92f, .82f, .3f}, {.7f, .2f, .35f}},
    {{.25f, .4f, .7f}, {.95f, .7f, .25f}, {.85f, .35f, .3f}, {.3f, .65f, .45f}},
    {{.55f, .3f, .65f}, {.95f, .85f, .7f}, {.2f, .5f, .55f}, {.95f, .45f, .4f}}};
  for (int i = 0; i < 3; i++) for (int k = 0; k < 4; k++) T_paint[i][k] = toy_paint(PA[i][k][0], PA[i][k][1], PA[i][k][2]);
  T_bulb = glow(1, .82f, .5f); T_poleW = toy_paint(.36f, .28f, .2f); T_wire = toy_paint(.06f, .06f, .06f);
}

/* ── Chupacabra Taproom's front (local: front on z 0, +x to the right seen from Robson, which is north) ──
 * A 1949 single storey brick box: a picture window, two doors with oval lights and transoms, a bay window
 * under a black metal hood, a steel sign board, the round blade sign at the north corner, a tan shade sail
 * over the door and brick planters of cactus. 7.6 m wide. */
static void town_white_frame(t3_object *r, float x0, float y0, float x1, float y1, float z) {
  const float t = .1f;
  prop(r, P_.box, T_white, (x0 + x1) * .5f, y1 - t * .5f, z, x1 - x0, t, .1f);
  prop(r, P_.box, T_white, (x0 + x1) * .5f, y0 + t * .5f, z, x1 - x0 + .1f, t, .14f);
  prop(r, P_.box, T_white, x0 + t * .5f, (y0 + y1) * .5f, z, t, y1 - y0, .1f);
  prop(r, P_.box, T_white, x1 - t * .5f, (y0 + y1) * .5f, z, t, y1 - y0, .1f);
}
static void town_cactus(t3_object *r, float x, float y, float z, uint32_t h) {
  /* prickly pear: a stack of flat pads; and a small agave beside it */
  for (int i = 0; i < 6; i++) {
    float a = i * 1.9f + (h % 7), rr = .12f + (i % 3) * .1f, yy = y + .25f + (i % 3) * .28f;
    t3_mesh *m = prop(r, P_.box, T_pad, x + cosf(a) * rr, yy, z + sinf(a) * rr * .6f, .2f, .26f, .05f);
    t3_object_set_rotation(&m->base, 0, a, (i % 2 ? .4f : -.3f));
  }
  for (int i = 0; i < 4; i++) {
    t3_mesh *m = prop(r, P_.cone, T_agave, x + .45f, y + .2f, z + .1f, .05f, .45f, .05f);
    t3_object_set_rotation(&m->base, cosf(i * 1.05f) * .6f, 0, sinf(i * 1.05f) * .6f);
  }
}
static void town_planter(t3_object *r, float x, float z, float w, float d, uint32_t h) {
  mesa_masonry(r, FAC_BRICK, x, .28f, z, w, .56f, d);
  prop(r, P_.box, T_mulch, x, .57f, z, w - .2f, .04f, d - .2f);
  for (float t = -w * .5f + .45f; t < w * .5f - .3f; t += 1.1f) town_cactus(r, x + t, .57f, z, h + (uint32_t)(t * 10));
}
static void town_chupacabra(t3_object *r, const whimsy_parts *p) {
  const float W = 7.6f, H = 4.6f;
  t3_material *darkbrick = toy_paint(.42f, .2f, .15f), *door = toy_paint(.33f, .37f, .35f), *hood = toy_paint(.07f, .07f, .08f);
  mesa_masonry(r, FAC_BRICK, 0, H * .5f, -.15f, W, H, .3f);
  /* coping and the frieze: a soldier course boxed into a long panel */
  prop(r, p->box, darkbrick, 0, H + .05f, -.12f, W + .1f, .14f, .4f);
  prop(r, p->box, darkbrick, 0, 4.15f, .02f, W - .5f, .08f, .05f); prop(r, p->box, darkbrick, 0, 3.05f, .02f, W - .5f, .08f, .05f);
  prop(r, p->box, darkbrick, -W * .5f + .25f, 3.6f, .02f, .08f, 1.18f, .05f); prop(r, p->box, darkbrick, W * .5f - .25f, 3.6f, .02f, .08f, 1.18f, .05f);
  /* the house number 14, top left */
  prop(r, p->box, p->black, -3.32f, 3.6f, .05f, .07f, .5f, .03f);
  prop(r, p->box, p->black, -3.0f, 3.6f, .05f, .07f, .5f, .03f); prop(r, p->box, p->black, -3.2f, 3.72f, .05f, .07f, .26f, .03f); prop(r, p->box, p->black, -3.1f, 3.56f, .05f, .26f, .07f, .03f);
  /* the picture window, left */
  prop(r, p->box, p->glass, -2.3f, 1.6f, .01f, 2.0f, 2.1f, .04f);
  prop(r, p->box, M_winglow_fwd, -2.3f, 1.6f, .04f, 1.85f, 1.95f, .01f);
  town_white_frame(r, -3.35f, .5f, -1.25f, 2.7f, .06f);
  /* two doors, oval lights, transoms */
  for (int k = 0; k < 2; k++) {
    float x = k ? .62f : -.55f;
    prop(r, p->box, door, x, 1.08f, .02f, .86f, 2.16f, .06f);
    prop(r, p->ball, p->glass, x, 1.25f, .06f, .2f, .55f, .02f);
    prop(r, p->ball, p->gold, x, 1.25f, .055f, .24f, .6f, .015f);
    prop(r, p->box, p->glass, x, 2.45f, .01f, .86f, .36f, .04f);
    prop(r, p->box, M_winglow_fwd, x, 2.45f, .035f, .8f, .3f, .01f);
    town_white_frame(r, x - .52f, .0f, x + .52f, 2.72f, .06f);
    prop(r, p->box, p->gold, x + .33f, 1.05f, .1f, .03f, .18f, .04f);
  }
  prop(r, p->box, toy_paint(.62f, .6f, .56f), .03f, .04f, .6f, 2.6f, .08f, 1.2f);   /* the stoop */
  /* the bay window under its hood */
  mesa_masonry(r, FAC_BRICK, 2.45f, .3f, .28f, 1.9f, .6f, .56f);
  prop(r, p->box, p->glass, 2.45f, 1.6f, .55f, 1.3f, 1.8f, .04f);
  prop(r, p->box, M_winglow_fwd, 2.45f, 1.6f, .58f, 1.2f, 1.7f, .01f);
  town_white_frame(r, 1.75f, .6f, 3.15f, 2.6f, .58f);
  for (int s = -1; s <= 1; s += 2) {
    t3_mesh *g = prop(r, p->box, p->glass, 2.45f + s * .83f, 1.6f, .3f, .04f, 1.8f, .6f); t3_object_set_rotation(&g->base, 0, s * .6f, 0);
    t3_mesh *f = prop(r, p->box, T_white, 2.45f + s * .83f, 1.6f, .3f, .1f, 2.0f, .64f); t3_object_set_rotation(&f->base, 0, s * .6f, 0); f->base.scale.x = .08f;
  }
  t3_mesh *hd = prop(r, p->box, hood, 2.45f, 2.82f, .38f, 2.1f, .07f, .9f); t3_object_set_rotation(&hd->base, .35f, 0, 0);
  /* the steel sign board: the goat skull and the name */
  prop(r, p->box, T_sign, .45f, 3.6f, .04f, 3.6f, 1.0f, .05f);
  mesa_logo(r, "chupacabra.png", -.75f, 3.6f, .075f, .85f, .85f);
  mesa_name(r, "CHUPACABRA TAPROOM", .95f, 3.6f, .075f, 2.4f, .32f);
  /* the round blade at the north corner, both faces */
  prop(r, p->box, hood, 3.62f, 4.1f, .45f, .05f, .05f, .9f);
  t3_mesh *disc = prop(r, p->cyl, p->black, 3.62f, 3.6f, .85f, .62f, .12f, .62f); t3_object_set_rotation(&disc->base, 0, 0, 1.5707963f);
  for (int s = -1; s <= 1; s += 2) {
    t3_mesh *lg = prop(r, FAC_PLANE, FAC_SIGNMAT, 3.62f + s * .07f, 3.6f, .85f, 1.1f, 1.1f, 1);
    lg->base.user_data = (void *)sign_find("chupacabra.png"); t3_object_set_rotation(&lg->base, 0, s * 1.5707963f, 0);
    t3_mesh *ring = prop(r, P_.ring, p->lamp, 3.62f + s * .075f, 3.6f, .85f, .58f, .58f, .58f); t3_object_set_rotation(&ring->base, 0, 1.5707963f, 0);
  }
  /* downpipe and conduit at the corner */
  prop(r, p->cyl, T_steelgrey, 3.72f, 2.3f, .06f, .05f, 4.6f, .05f);
  prop(r, p->cyl, T_steelgrey, -3.72f, 2.3f, .06f, .035f, 4.6f, .035f);
  /* the tan sail from the parapet out over the door and the walk (2025) */
  mesa_sail(r, T_tan, (t3_vec3){-1.8f, 4.3f, .1f}, (t3_vec3){2.2f, 4.3f, .1f}, (t3_vec3){.3f, 3.9f, 4.2f});
  /* brick planters: under the picture window, and around the north corner */
  town_planter(r, -2.3f, .55f, 2.1f, .55f, 11);
  mesa_masonry(r, FAC_BRICK, 4.25f, .28f, -.9f, .7f, .56f, 2.4f);
  prop(r, p->box, T_mulch, 4.25f, .57f, -.9f, .5f, .04f, 2.2f);
  town_cactus(r, 4.25f, .57f, -.4f, 5); town_cactus(r, 4.25f, .57f, -1.5f, 9);
}

/* ── the set pieces (local: built at the origin, yaw applied by the caller) ── */
static void ts_picnic(t3_object *r, float L) {
  /* the long patio tables: plank top and benches on dark steel A frames */
  prop(r, P_.box, T_wood, 0, .75f, 0, L, .05f, .78f);
  for (int s = -1; s <= 1; s += 2) prop(r, P_.box, T_wood, 0, .45f, s * .65f, L, .045f, .3f);
  for (int k = -1; k <= 1; k += 2) {
    float x = k * (L * .5f - .25f);
    prop(r, P_.box, T_frame, x, .38f, 0, .06f, .74f, .06f);
    prop(r, P_.box, T_frame, x, .4f, 0, .06f, .05f, 1.6f);
    for (int s = -1; s <= 1; s += 2) { prop(r, P_.box, T_frame, x, .22f, s * .65f, .05f, .44f, .05f); prop(r, P_.box, T_frame, x, .02f, s * .4f, .05f, .04f, .9f); }
  }
}
static void ts_patio_rail(t3_object *r, float A, float B) {
  /* posts every couple of metres, a top frame for the lights and the sail, a railing at hip height,
   * and a gap in the street side for the way in */
  const float H = 2.75f, gap = .85f;
  int nx = (int)ceilf(A / 2.1f), nz = (int)ceilf(B / 2.1f);
  for (int i = 0; i <= nx; i++) for (int s = -1; s <= 1; s += 2) prop(r, P_.box, T_frame, -A * .5f + A * i / nx, H * .5f, s * B * .5f, .1f, H, .1f);
  for (int i = 1; i < nz; i++) for (int s = -1; s <= 1; s += 2) prop(r, P_.box, T_frame, s * A * .5f, H * .5f, -B * .5f + B * i / nz, .1f, H, .1f);
  for (int s = -1; s <= 1; s += 2) {
    prop(r, P_.box, T_frame, 0, H, s * B * .5f, A, .1f, .1f);
    prop(r, P_.box, T_frame, s * A * .5f, H, 0, .1f, .1f, B);
    /* the sides: two rails */
    for (int k = 0; k < 2; k++) prop(r, P_.box, T_frame, s * A * .5f, k ? .95f : .45f, 0, .06f, .06f, B);
    /* the street side, either side of the gap */
    float len = A * .5f - gap;
    for (int k = 0; k < 2; k++) prop(r, P_.box, T_frame, s * (gap + len * .5f), k ? .95f : .45f, B * .5f, len, .06f, .06f);
  }
  for (int i = 1; i < nx; i++) prop(r, P_.box, T_frame, -A * .5f + A * i / nx, H, 0, .08f, .08f, B);   /* joists */
  /* strings of bulbs: zigzags between the long sides, a little sag */
  for (int i = 0; i < nx; i++) {
    float x0 = -A * .5f + A * i / nx, x1 = -A * .5f + A * (i + 1) / nx;
    for (int k = 0; k <= 8; k++) {
      float t = k / 8.f, x = x0 + (x1 - x0) * t, z = -B * .5f + B * t;
      if (i % 2) z = -z;
      prop(r, P_.box, T_bulb, x, H - .05f - .45f * sinf(t * 3.14159f), z, .07f, .09f, .07f);
    }
  }
}
static void ts_stop(t3_object *r) {
  prop(r, P_.cyl, T_steelgrey, 0, 1.25f, 0, .03f, 2.5f, .03f);
  t3_mesh *m = prop(r, P_.cyl, T_red, 0, 2.3f, .04f, .36f, .02f, .36f); t3_object_set_rotation(&m->base, 1.5707963f, 0, 0);
}
static void ts_cactus_bed(t3_object *r, float A, float B) {
  mesa_masonry(r, FAC_BRICK, 0, .28f, 0, A, .56f, B);
  prop(r, P_.box, T_mulch, 0, .57f, 0, A - .2f, .04f, B - .2f);
  for (float t = -A * .5f + .5f; t < A * .5f - .3f; t += 1.0f) town_cactus(r, t, .57f, 0, (uint32_t)(t * 13 + 40));
}
static void ts_sail_mast(t3_object *r, float H, int v) {
  /* a tapered white mast on a plinth, three sails hung off it on cables to low stays */
  prop(r, P_.cyl, T_white, 0, .3f, 0, .55f, .6f, .55f);
  prop(r, P_.cyl, T_white, 0, H * .5f, 0, .22f, H, .22f);
  prop(r, P_.cone, T_white, 0, H + .4f, 0, .22f, .8f, .22f);
  for (int k = 0; k < 3; k++) {
    float a = k * 2.094f + v * .5f, c = cosf(a), s = sinf(a);
    float y0 = H * (.35f + .17f * k), y1 = y0 + 3.4f;
    t3_vec3 A = {c * .3f, y1, s * .3f}, B = {c * .3f, y0, s * .3f}, C = {c * 6.5f - s * 1.5f, y0 + 1.4f, s * 6.5f + c * 1.5f};
    mesa_sail(r, T_sail, A, B, C);
    prop(r, P_.box, T_steelgrey, c * 6.6f - s * 1.5f, y0 + 1.4f, s * 6.6f + c * 1.5f, .08f, .08f, .08f);
  }
}
static void ts_glass_canopy(t3_object *r, float A, float B, int v) {
  const float H = 3.7f;
  for (int i = -1; i <= 1; i += 2) for (int k = -1; k <= 1; k += 2) prop(r, P_.box, T_steelgrey, i * (A * .5f - .2f), H * .5f, k * (B * .5f - .2f), .14f, H, .14f);
  for (int k = -1; k <= 1; k += 2) prop(r, P_.box, T_steelgrey, 0, H, k * (B * .5f - .2f), A, .14f, .14f);
  /* panes, each its own colour, with a steel rib between */
  int n = (int)(A / 1.3f); if (n < 3) n = 3;
  for (int i = 0; i < n; i++) {
    float x = -A * .5f + (i + .5f) * A / n;
    prop(r, P_.box, T_glass[(v + i) % 5], x, H + .1f, 0, A / n - .06f, .03f, B - .2f);
    prop(r, P_.box, T_steelgrey, x + A / n * .5f, H + .08f, 0, .05f, .07f, B - .2f);
    /* its coloured light on the ground, thrown a little north east by a high sun */
    prop(r, P_.box, T_pool[(v + i) % 5], x + .5f, .015f, -.6f, A / n - .1f, .01f, B - .5f);
  }
}
static void ts_arroyo(t3_object *r, float L, float Wd) {
  /* a dry wash: a sandy bed with a dark runnel of water down its middle, boulders along the banks */
  prop(r, P_.box, T_sand, 0, .01f, 0, Wd + 1.2f, .02f, L);
  prop(r, P_.box, T_water, 0, .025f, 0, Wd * .45f, .02f, L);
  for (int i = 0; i < (int)(L / 1.6f); i++) {
    float z = -L * .5f + .8f + i * 1.6f, s = (i % 2) ? 1 : -1, sz = .25f + (i * 37 % 5) * .07f;
    prop(r, P_.box, T_stone, s * (Wd * .5f + .3f), .06f, z, sz * 1.6f, sz * .9f, sz * 1.2f);
  }
}
static void ts_mac_sign(t3_object *r, float L) {
  prop(r, P_.box, toy_paint(.74f, .72f, .7f), 0, .55f, 0, L, 1.1f, .5f);
  prop(r, P_.box, toy_paint(.58f, .6f, .82f), 0, 1.15f, 0, L + .1f, .1f, .56f);
  mesa_name(r, "MESA ARTS CENTER", 0, .62f, .26f, L - 1.2f, .55f);
}
static void ts_mural(t3_object *r, float A, float B, int v) {
  /* bands and big shapes in the palette: a mural reads as colour at street distance */
  prop(r, P_.box, T_paint[v][0], 0, B * .5f + .3f, .02f, A, B, .02f);
  for (int i = 0; i < 5; i++) {
    float x = -A * .5f + (i + .5f) * A / 5, y = .3f + B * (.3f + .12f * (i % 3));
    t3_mesh *m = prop(r, P_.box, T_paint[v][1 + i % 3], x, y, .035f, A / 7, B * (.4f + .1f * (i % 2)), .01f);
    (void)m;
  }
  prop(r, P_.box, T_paint[v][3], 0, .55f, .04f, A, .5f, .01f);
}
static void ts_dumpster(t3_object *r, int v) {
  t3_material *m = T_dump[v & 1];
  prop(r, P_.box, m, 0, .62f, 0, 1.9f, 1.05f, 1.15f);
  prop(r, P_.box, m, 0, .2f, 0, 1.8f, .2f, 1.05f);
  for (int s = -1; s <= 1; s += 2) { t3_mesh *l = prop(r, P_.box, T_bin, s * .47f, 1.2f, -.02f, .92f, .05f, 1.2f); t3_object_set_rotation(&l->base, .12f, 0, 0); }
  for (int s = -1; s <= 1; s += 2) prop(r, P_.box, T_steelgrey, s * .98f, .62f, 0, .05f, .14f, .9f);
  for (int s = -1; s <= 1; s += 2) for (int k = -1; k <= 1; k += 2) prop(r, P_.cyl, T_bin, s * .8f, .07f, k * .42f, .07f, .14f, .07f);
}
static void ts_bins(t3_object *r, int n) {
  for (int i = 0; i < n; i++) {
    float x = (i - (n - 1) * .5f) * .74f;
    prop(r, P_.box, T_bin, x, .5f, 0, .58f, 1.0f, .7f);
    prop(r, P_.box, T_bin, x, 1.03f, .03f, .62f, .06f, .76f);
    prop(r, P_.cyl, T_bin, x, .1f, -.3f, .1f, .2f, .1f);
  }
}
static void ts_bike_rack(t3_object *r, int n) {
  for (int i = 0; i < n; i++) {
    float x = (i - (n - 1) * .5f) * .9f;
    t3_mesh *m = prop(r, P_.ring, T_steelgrey, x, .45f, 0, .45f, .5f, .45f); (void)m;
    prop(r, P_.box, T_steelgrey, x, .2f, 0, .05f, .4f, .9f);
  }
}
static void ts_upole(t3_object *r, float *top_y) {
  const float H = 10.5f;
  prop(r, P_.cyl, T_poleW, 0, H * .5f, 0, .15f, H, .15f);
  prop(r, P_.box, T_poleW, 0, H - .6f, 0, 2.4f, .12f, .12f);
  for (int k = -1; k <= 1; k++) prop(r, P_.cyl, T_steelgrey, k * 1.05f, H - .45f, 0, .05f, .2f, .05f);
  prop(r, P_.box, T_poleW, 0, H - 1.9f, 0, 1.6f, .1f, .1f);
  prop(r, P_.cyl, T_steelgrey, .35f, H - 2.8f, .2f, .3f, .9f, .3f);   /* the transformer can */
  if (top_y) *top_y = H - .35f;
}

/* ── the back of the lab, from the alley (street view Oct 2025) ──
 * World coordinates, no transform. The HSL-108 model's rear wall stands on z -40.41 (plan x 40.41) with
 * two exits: the corridor exit, x 3.17 to 4.20 (lab_back_door), recessed, a glass door; and the workshop
 * exit, x 4.66 to 5.63, a green steel door. Red brick, the parapet stepped up over the middle with the
 * "108 WEST MAIN" panel, two barred windows into the kitchen, a meter, green conduit boxing the doors,
 * a lamp over the recess. East, the next building's tan wall with its switchgear and a steel pole. */
#define LAB_BACK_Z0 (-40.55f)          /* the inside face of the brick skin, just behind the model's wall */
#define LAB_BACK_Z1 (-40.80f)          /* its outside face, on the alley */
static void fk_lab_back(t3_object *r, const whimsy_parts *p) {
  const float z0 = LAB_BACK_Z0, z1 = LAB_BACK_Z1, zm = (z0 + z1) * .5f, d = z0 - z1, X0 = -2.775f, X1 = 6.63f;
  t3_material *green = toy_paint(.18f, .4f, .26f), *dkgreen = toy_paint(.12f, .3f, .2f), *yellow = toy_paint(.9f, .72f, .12f), *ink = toy_paint(.06f, .07f, .07f);
  t3_material *coping = toy_paint(.5f, .26f, .18f), *tan = toy_paint(.78f, .68f, .52f), *gray = toy_paint(.6f, .62f, .62f), *teal = toy_paint(.12f, .4f, .36f);
  /* the brick skin in pieces around the two openings, up to the parapet's three heights */
  const float seg[4][2] = {{X0, 3.17f}, {4.20f, 4.66f}, {5.63f, X1}};
  for (int i = 0; i < 3; i++) mesa_masonry(r, FAC_BRICK, (seg[i][0] + seg[i][1]) * .5f, 1.15f, zm, seg[i][1] - seg[i][0], 2.3f, d);
  mesa_masonry(r, FAC_BRICK, (X0 + 4.9f) * .5f, 4.15f, zm, 4.9f - X0, 3.7f, d);
  mesa_masonry(r, FAC_BRICK, (4.9f + X1) * .5f, 3.75f, zm, X1 - 4.9f, 2.9f, d);
  mesa_masonry(r, FAC_BRICK, 2.0f, 6.2f, zm, 3.2f, .4f, d);
  prop(r, p->box, coping, (X0 + 4.9f) * .5f, 6.03f, zm, 4.9f - X0 + .05f, .1f, d + .08f);
  prop(r, p->box, coping, 2.0f, 6.43f, zm, 3.25f, .1f, d + .08f);
  prop(r, p->box, coping, (4.9f + X1) * .5f, 5.23f, zm, X1 - 4.9f + .05f, .1f, d + .08f);
  /* the roof behind the parapet, seen from above (the model has no roof deck) */
  prop(r, p->box, toy_paint(.46f, .44f, .42f), (X0 + X1) * .5f, 5.0f, -20.6f, X1 - X0, .1f, 39.8f);
  /* 108 WEST MAIN: cream panel, green border */
  prop(r, p->box, green, 2.0f, 4.75f, z1 - .03f, 1.95f, .95f, .05f);
  { t3_mesh *sg = prop(r, FAC_PLANE, FAC_SIGNMAT, 2.0f, 4.75f, z1 - .06f, 1.75f, .78f, 1);   /* faces the alley (-z) */
    sg->base.user_data = (void *)sign_find("108 WEST MAIN"); t3_object_set_rotation(&sg->base, 0, 3.14159265f, 0); }
  /* the corridor exit: reveals and the lamp. Both exit doors stand open, swung in flat against the hall
   * walls as the model has them (tools/prep-lab.mjs), and the lab draws through them from the alley
   * (world.c at_backdoor): no backdrop or second leaf here, it would stand inside the lab */
  for (int s = 0; s < 2; s++) prop(r, p->box, coping, s ? 4.21f : 3.16f, 1.15f, zm, .04f, 2.3f, d + .02f);
  prop(r, p->box, ink, 3.69f, 2.62f, z1 - .1f, .36f, .14f, .2f);
  prop(r, p->ball, p->lamp, 3.69f, 2.54f, z1 - .14f, .14f, .05f, .08f);
  /* the workshop exit: its green steel frame, a mat */
  prop(r, p->box, dkgreen, 5.15f, 2.2f, z1 - .05f, 1.15f, .1f, .08f);
  for (int s = -1; s <= 1; s += 2) prop(r, p->box, dkgreen, 5.15f + s * .53f, 1.1f, z1 - .05f, .08f, 2.2f, .08f);
  prop(r, p->box, ink, 5.15f, .01f, z1 - .55f, 1.0f, .02f, .7f);
  /* green conduit boxing the doors */
  t3_object_set_rotation(prop(r, p->cyl, green, 4.4f, 3.55f, z1 - .05f, .035f, 4.2f, .035f), 0, 0, 1.5707963f);
  prop(r, p->cyl, green, 2.3f, 2.25f, z1 - .05f, .035f, 2.6f, .035f);
  prop(r, p->cyl, green, 6.5f, 2.1f, z1 - .05f, .035f, 2.9f, .035f);
  /* two barred windows into the kitchen, a meter between them */
  for (int k = 0; k < 2; k++) {
    float x = k ? -1.45f : .3f;
    prop(r, p->box, p->glass, x, 1.62f, z1 - .01f, .95f, 1.2f, .03f);
    prop(r, p->box, M_winglow_fwd, x, 1.62f, z1 - .025f, .85f, 1.1f, .01f);
    for (int b = -2; b <= 2; b++) prop(r, p->box, green, x + b * .2f, 1.62f, z1 - .06f, .03f, 1.25f, .03f);
    prop(r, p->box, green, x, 2.27f, z1 - .05f, 1.1f, .08f, .08f); prop(r, p->box, green, x, .98f, z1 - .05f, 1.1f, .08f, .1f);
    for (int s = -1; s <= 1; s += 2) prop(r, p->box, green, x + s * .52f, 1.62f, z1 - .05f, .07f, 1.3f, .08f);
  }
  prop(r, p->box, gray, -.58f, 1.5f, z1 - .1f, .32f, .46f, .16f);
  prop(r, p->box, gray, -.58f, 2.9f, z1 - .04f, .05f, 2.4f, .05f);
  /* gas risers and their yellow guard at the east end */
  for (int k = 0; k < 2; k++) {
    prop(r, p->cyl, yellow, 6.0f + k * .35f, .45f, z1 - .5f, .06f, .9f, .06f);
    prop(r, p->box, gray, 6.0f + k * .35f, .95f, z1 - .12f, .22f, .3f, .18f);
    prop(r, p->cyl, gray, 6.0f + k * .35f, .55f, z1 - .1f, .025f, .8f, .025f);
  }
  prop(r, p->box, yellow, 6.17f, .72f, z1 - .5f, .55f, .07f, .07f);
  /* the steel pole at the corner, its arm over the alley */
  prop(r, p->cyl, ink, 6.95f, 4.6f, -41.3f, .14f, 9.2f, .14f);
  prop(r, p->cyl, ink, 6.95f, .5f, -41.3f, .24f, 1.0f, .24f);
  prop(r, p->box, ink, 6.95f, 8.6f, -42.2f, .08f, .08f, 1.8f);
  prop(r, p->box, ink, 6.95f, 8.9f, -41.3f, 1.4f, .08f, .08f);
  prop(r, p->box, p->red, 6.95f, 2.4f, -41.47f, .25f, .35f, .02f);
  /* the next building east: tan rear wall gear. Switchgear, conduit, a black door, posts */
  for (int k = 0; k < 2; k++) {
    prop(r, p->box, gray, 8.4f + k * 1.0f, .85f, -42.35f, .95f, 1.7f, .5f);
    prop(r, p->box, ink, 8.4f + k * 1.0f, .9f, -42.61f, .3f, .12f, .02f);
    prop(r, p->cyl, gray, 8.4f + k * 1.0f, 2.8f, -42.12f, .04f, 2.2f, .04f);
  }
  prop(r, p->box, ink, 11.6f, 1.08f, -42.1f, 1.0f, 2.16f, .06f);
  prop(r, p->box, tan, 11.6f, 2.35f, -42.12f, 1.2f, .5f, .06f);
  for (int k = 0; k < 3; k++) prop(r, p->cyl, ink, 7.6f + k * 1.6f, .45f, -43.1f, .07f, .9f, .07f);
  prop(r, p->box, gray, 7.6f, 1.6f, -42.1f, .5f, .7f, .2f);
  /* the alley: a teal pedestrian lamp and a no parking sign on the north side */
  prop(r, p->cyl, teal, -2.2f, 2.0f, -50.9f, .07f, 4.0f, .07f);
  prop(r, p->cyl, teal, -2.2f, .3f, -50.9f, .16f, .6f, .16f);
  prop(r, p->box, teal, -2.2f, 4.25f, -50.9f, .34f, .5f, .34f);
  prop(r, p->box, p->lamp, -2.2f, 4.2f, -50.9f, .26f, .36f, .26f);
  prop(r, p->cone, teal, -2.2f, 4.62f, -50.9f, .28f, .25f, .28f);
  prop(r, p->cyl, gray, -1.0f, 1.2f, -51.4f, .03f, 2.4f, .03f);
  prop(r, p->box, toy_paint(.95f, .95f, .95f), -1.0f, 2.1f, -51.38f, .3f, .4f, .02f);
  prop(r, p->ball, p->red, -1.0f, 2.1f, -51.36f, .11f, .11f, .01f);
}
