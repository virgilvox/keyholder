/* test_world.c: the world on its own. A scripted camera shows each view for a few frames; the script
 * list below is the shot list. With the left stick it is a walk test instead (hold START). */
#ifdef KH_TEST_WORLD
#include "game.h"
#include "world.h"
#include "input.h"
#include "city.h"
#include "actor.h"
#include <stdio.h>
#include <string.h>
#include "gen/city_data.h"
static t3_scene *scene; static t3_camera *cam; static actor_t *A, *B2;
typedef struct { const char *name; float t; world_stage st; v3 eye, at; int phase; float fov; } shot_t;
#define S(n, t, st, ex, ey, ez, ax, ay, az) {n, t, st, {ex, ey, ez}, {ax, ay, az}, 0, 55}
#define S2(n, t, st, ex, ey, ez, ax, ay, az) {n, t, st, {ex, ey, ez}, {ax, ay, az}, 1, 55}
#define ST(n, t, ex, ey, ez, ax, ay, az, fov) {n, t, STAGE_TOWN, {ex, ey, ez}, {ax, ay, az}, 0, fov}
static const shot_t SHOTS[] = {
  /* 0 */  S("door_east_10", 10, STAGE_TOWN, -2, 1.6f, 5.5f, 60, 3, 9),
  /* 1 */  S("door_west_10", 10, STAGE_TOWN, 3, 1.6f, 5.5f, -60, 3, 7),
  /* 2 */  S("door_east_18", 18, STAGE_TOWN, -2, 1.6f, 5.5f, 60, 3, 9),
  /* 3 */  S("door_west_18", 18, STAGE_TOWN, 3, 1.6f, 5.5f, -60, 3, 7),
  /* 4 */  S("door_east_21", 21, STAGE_TOWN, -2, 1.6f, 5.5f, 60, 3, 9),
  /* 5 */  S("door_west_21", 21, STAGE_TOWN, 3, 1.6f, 5.5f, -60, 3, 7),
  /* 6 */  S("lab_front_10", 10, STAGE_TOWN, 0, 1.6f, 14, 0, 2.6f, 0),
  /* 7 */  S("nile_10", 10, STAGE_TOWN, -1, 1.6f, 5.0f, -1, 4, 39),
  /* 8 */  S("nile_18", 18, STAGE_TOWN, -1, 1.6f, 5.0f, -1, 4, 39),
  /* 9 */  S("station_10", 10, STAGE_TOWN, 300, 1.8f, 19.5f, 400, 3, 19.5f),
  /* 10 */ S("station_18", 18.3f, STAGE_TOWN, 300, 1.8f, 19.5f, 400, 3, 19.5f),
  /* 11 */ S("lab_inside_10", 10, STAGE_TOWN, 3, 1.5f, -12, 3, 1.3f, -28),
  /* 12 */ S("lab_inside_street_21", 21, STAGE_TOWN, 1, 1.5f, -9, 0, 1.3f, 8),
  /* 13 */ S("arena_bench", 18, STAGE_ARENA_BENCH, 0, 2.6f, 11, 0, 1.2f, -5),
  /* 14 */ S("arena_laser", 18, STAGE_ARENA_LASER, 0, 2.6f, 11, 0, 1.2f, -5),
  /* 15 */ S("arena_fridge", 18, STAGE_ARENA_FRIDGE, 0, 2.6f, 11, 0, 1.2f, -5),
  /* 16 */ S("arena_shop", 18, STAGE_ARENA_SHOP, 0, 2.6f, 11, 0, 1.2f, -5),
  /* 17 */ S("arena_unfold", 18, STAGE_ARENA_UNFOLD, 0, 8, 34, 0, 2, 0),
  /* 18 */ S("block_200_10", 10, STAGE_TOWN, -260, 1.6f, 16, -260, 3.5f, 0),
  /* 19 */ S("main_west_far_17", 17.6f, STAGE_TOWN, -140, 1.7f, 12, 40, 4, 12),
  /* 20 */ S("mid_100s_10", 10, STAGE_TOWN, -112, 1.6f, 15, -112, 3, 0),
  /* 21 */ S("south_block_10", 10, STAGE_TOWN, -60, 1.6f, 27, -60, 3, 40),
  /* 22 */ S("block_200_21", 21, STAGE_TOWN, -270, 1.6f, 16, -260, 3.5f, 0),
  /* 23 */ S("east_block_10", 10, STAGE_TOWN, 120, 1.6f, 15, 120, 3.5f, 0),
  /* 24 */ S("east_block_21", 21, STAGE_TOWN, 130, 1.6f, 15, 105, 3.5f, 0),
  /* 25 */ S("lab_lot_10", 10, STAGE_TOWN, -40, 1.6f, -72, 10, 3, -40),
  /* 26 */ S("corner_macdonald_18", 17.6f, STAGE_TOWN, 34, 1.6f, 15, 70, 6, -6),
  /* 27 */ S("robson_10", 10, STAGE_TOWN, -205, 1.6f, -8, -224, 3.5f, -44),
  /* 28 */ S("south_east_10", 10, STAGE_TOWN, 20, 1.6f, 34, -55, 3, 41),
  /* 29 */ S("center_lost_dutchman_10", 10, STAGE_TOWN, 276, 1.6f, -38, 258, 3.5f, -38),
  /* 30 */ S("macdonald_south_10", 10, STAGE_TOWN, 34, 1.6f, 60, 18, 3.5f, 66),
  /* 31 */ S("west_end_10", 10, STAGE_TOWN, -335, 2, 16, -200, 5, 16),
  /* 32 */ S("high_city_16", 16, STAGE_TOWN, -60, 42, 90, 80, 0, -30),
  /* 33 */ S("train_12", 12, STAGE_TOWN, -335, 1.8f, 12.5f, -250, 2, 21),
  /* 34 */ S2("arena_bench_2", 18, STAGE_ARENA_BENCH, 0, 2.6f, 11, 0, 1.2f, -5),
  /* 35 */ S2("arena_laser_2", 18, STAGE_ARENA_LASER, 0, 2.6f, 11, 0, 1.2f, -5),
  /* 36 */ S2("arena_fridge_2", 18, STAGE_ARENA_FRIDGE, 0, 2.6f, 11, 0, 1.2f, -5),
  /* 37 */ S2("arena_shop_2", 18, STAGE_ARENA_SHOP, 0, 2.6f, 11, 0, 1.2f, -5),
  /* 38 */ S2("arena_unfold_2", 18, STAGE_ARENA_UNFOLD, 0, 8, 34, 0, 2, 0),
  /* 39 */ ST("title_1636", 16.6f, 0, 1.2f, 13.9f, 0, 1.15f, 7.7f, 38),
  /* 40 */ ST("street_9", 9, 20, 1.7f, 12, -80, 4, 14, 55),
  /* 41 */ ST("street_13", 13, 20, 1.7f, 12, -80, 4, 14, 55),
  /* 42 */ ST("street_1830", 18.5f, 20, 1.7f, 12, -80, 4, 14, 55),
  /* 43 */ ST("street_2230", 22.5f, 20, 1.7f, 12, -80, 4, 14, 55),
  /* 44 */ ST("street_east_2230", 22.5f, -20, 1.7f, 27, 80, 4, 25, 55),
  /* 45 */ ST("lab_front_22", 22, 0, 1.6f, 14, 0, 2.6f, 0, 55),
  /* 46 */ ST("lab_inside_22", 22, 3, 1.5f, -12, 3, 1.3f, -28, 55),
  /* 47 */ ST("sky_up_13", 13, 0, 1.7f, 14, 0, 30, -20, 55),
  /* 48 */ ST("sky_up_1830", 18.5f, 0, 1.7f, 14, -40, 20, -20, 55),
  /* 49 */ ST("sky_up_2230", 22.5f, 0, 1.7f, 14, 0, 30, -20, 55),
  /* 50 */ ST("title_1636_wide", 16.6f, 0, 1.7f, 16, 0, 6, 0, 55),
  /* 51 */ ST("station_stop_13", 13, 330, 1.7f, 12.5f, 360, 2, 19.5f, 55),
  /* 52.. the back half of the lab, eye positions in world metres (world x = plan z - 2.775, world z = -plan x) */
  ST("hall_back_13", 13, 2.6f, 1.6f, -27, 2.6f, 1.3f, -40, 55),
  ST("kitchen_side_door_13", 13, 1.0f, 1.6f, -34.6f, 2.3f, 1.1f, -38.5f, 55),
  ST("kitchen_in_13", 13, 0.3f, 1.6f, -39.5f, 2.2f, 1.1f, -36.5f, 60),
  ST("corridor_13", 13, 3.4f, 1.6f, -34.5f, 3.4f, 1.2f, -40, 55),
  ST("wc_closet_13", 13, 2.2f, 1.6f, -27.5f, -1.5f, 1.1f, -33.5f, 60),
  ST("biolab_13", 13, -0.5f, 1.6f, -34.5f, 2.6f, 1.1f, -36.2f, 60),
  /* the entry: a lens crossing the front door, from the portico through the recess to just inside */
  ST("door_cross_1", 9.2f, 0.3f, 1.6f, -1.5f, 0.6f, 1.3f, -12, 50),
  ST("door_cross_2", 9.2f, 0.3f, 1.6f, -3.8f, 0.6f, 1.3f, -12, 50),
  ST("door_cross_3", 9.2f, 0.3f, 1.6f, -5.2f, 0.6f, 1.3f, -12, 50),
  ST("door_cross_4", 9.2f, 0.3f, 1.6f, -6.0f, 0.6f, 1.3f, -12, 50),
  /* the Tech Tear Apart bin, from the hall (place lab_teardown) */
  ST("teardown_bin_13", 13, 2.2f, 1.55f, -19.1f, 1.1f, 1.0f, -19.1f, 55),
};
#define NSHOT (int)(sizeof SHOTS / sizeof SHOTS[0])
static int frame; static int cur = -1;
#define FRAMES_PER 10
static v3 walker = {0, 0, 2.4f}; static v3 wvel; static bool wgnd = true;
static int walkmode;
void game_init(void) {
  scene = t3_scene_new();
  cam = t3_perspective_camera_new(55, 16.f / 9.f, .08f, 800);
  double t0 = ds_now();
  world_init(scene);
  ds_log("world_init done at %.2f s (t0 %.2f)", ds_now(), t0);
  A = actor_new(AM_D, &ACTOR_PAL_NONE, 1, scene); B2 = actor_new(AM_S, &ACTOR_PAL_NONE, 1, scene);
  actor_play(A, "Idle_Loop", 1, true, 0); actor_play(B2, "Idle_Loop", 1, true, 0);
}
void game_frame(float dt) {
  frame++;
  /* walk tests first: a straight walk from the street into the lab and out the back */
  static int stage_test = 0;
  if (stage_test == 0) {
    /* simple floor and collision checks, logged once */
    stage_test = 1;
    const char *checks[] = {"lab_door","nile","worth","level1","center_main_station","lab_lot","mid_block"};
    for (int i = 0; i < 7; i++) { v3 p; float y; if (world_place(checks[i], &p, &y)) ds_log("%s (%.1f, %.1f, %.1f) floor %.2f", checks[i], p.x, p.y, p.z, world_floor_y(v3_(p.x, p.y + 1, p.z))); else ds_log("%s missing", checks[i]); }
    /* every place stands on open ground, and every name the design asks for answers */
    for (int i = 0; i < CITY_NPLACE; i++) {
      const city_place_t *q = &CITY_PLACE[i];
      v3 a = v3_(q->x, q->y, q->z), b = a, nn = a; bool hit = false;
      city_push(&b, .3f, 1.6f, &nn, &hit);
      float fy = world_floor_y(v3_(a.x, a.y + .3f, a.z));
      if (hit || fabsf(fy - a.y) > .05f) ds_log("place %s: pushed %.2f, floor %.2f vs %.2f", q->name, v3_len(v3_sub(b, a)), fy, a.y);
    }
    const char *names[] = {"lab_door","lab_back_door","lab_lot","nile","proof","chiwas","worth","burritoholics","gus","oro","chupacabra","main_burger","az_distilling","phantom_fox","que_chevere","espiritu","haunted","against_grain","organic_bean","casa_copal","diamond_dons","level1","mad_candy","mangos","republica","margaritas","lost_dutchman","hopes","pizza_mart","guadalupe","one_macdonald","macdonald_main","center_main_station","city_plaza","arts_center","mid_block","lot_fight"};
    for (int i = 0; i < (int)(sizeof names / sizeof names[0]); i++) if (!world_place(names[i], NULL, NULL)) ds_log("no place: %s", names[i]);
    float t; v3 n;
    if (world_raycast(v3_(0, 1.5f, 10), v3_(0, 0, 1), 100, &t, &n)) ds_log("ray south from Main hits %.1f", t);
    if (world_raycast(v3_(-30, 1.5f, 5), v3_(0, 0, -1), 100, &t, &n)) ds_log("ray north at x -30 hits %.1f", t);
    ds_log("visible street to Nile %d, inside lab %d, through wall %d", world_visible(v3_(0, 1.5f, 5), v3_(0, 1.5f, 30)), world_visible(v3_(1, 1.5f, -10), v3_(1, 1.5f, -30)), world_visible(v3_(-30, 1.5f, 5), v3_(-30, 1.5f, -20)));
    /* a walk in through the door and out the back; door opens itself near */
    v3 p = v3_(.5f, 0, 4); v3 v; bool g = true; world_door(1);
    for (int i = 0; i < 300; i++) { v = v3_(p.z < -5.7f && p.x < .8f ? 1.5f : 0, 0, -3.f); world_update(1.f / 60, p); world_move(&p, &v, .3f, 1.6f, 1.f / 60, &g); }
    ds_log("walk in: from z 4 at x .5 for 5 s, reached (%.2f, %.2f, %.2f) grounded %d in_lab %d", p.x, p.y, p.z, g, world_in_lab(p));
    /* door to the back of the lab along the hall, and out through the workshop exit */
    static const float WP[][2] = {{6.2f, 2.9f}, {6.4f, 4.6f}, {28.6f, 4.75f}, {35.6f, 4.65f}, {37.3f, 4.65f}, {37.3f, 6.25f}, {40.0f, 6.3f}, {42, 6.4f}};
    p = v3_(.5f, 0, 2); g = true;
    int wi = 0, steps = 0;
    for (; steps < 2400 && wi < 8; steps++) {
      v3 w = world_lab(WP[wi][0], WP[wi][1]);
      v3 dv = v3_sub(w, p); dv.y = 0;
      if (v3_len(dv) < .35f) { wi++; continue; }
      v = v3_scale(v3_norm(dv), 3); world_update(1.f / 60, p); world_move(&p, &v, .3f, 1.6f, 1.f / 60, &g);
    }
    ds_log("hall walk: waypoint %d of 8 in %d steps, at (%.2f, %.2f, %.2f)", wi, steps, p.x, p.y, p.z);
    p = v3_(3.7f, 0, -37); g = true;
    for (int i = 0; i < 200; i++) { v = v3_(0, 0, -3.f); world_move(&p, &v, .3f, 1.6f, 1.f / 60, &g); }
    ds_log("back door: from (3.7, -37), reached (%.2f, %.2f, %.2f) in_lab %d", p.x, p.y, p.z, world_in_lab(p));
    p = v3_(3.7f, 0, -43); g = true;
    for (int i = 0; i < 200; i++) { v = v3_(0, 0, 3.f); world_move(&p, &v, .3f, 1.6f, 1.f / 60, &g); }
    ds_log("back door in: from (3.7, -43), reached (%.2f, %.2f, %.2f) in_lab %d", p.x, p.y, p.z, world_in_lab(p));
    p = v3_(-60, 0, 2); g = true;
    for (int i = 0; i < 300; i++) { v = v3_(0, 0, 4.f); world_move(&p, &v, .3f, 1.6f, 1.f / 60, &g); if (i % 30 == 0 && i < 300) { float yy = p.y; (void)yy; } }
    ds_log("across Main at x -60: reached (%.2f, %.2f, %.2f)", p.x, p.y, p.z);
    p = v3_(-60, 3, 2); g = false; v = v3_(0, -1, 0);
    for (int i = 0; i < 100; i++) { v.y -= 9.8f / 60; world_move(&p, &v, .3f, 1.6f, 1.f / 60, &g); }
    ds_log("drop: y %.2f grounded %d", p.y, g);
    world_door_auto(true);
    /* the automatic front door (the gallery window bay flanks the west side of the entry, so the west angles
     * are shallow): walkers from several angles at a walk, a jog and a run, in and out. A stall
     * (slower than half speed for a step) means the door was in the way */
    {
      static const float ANG[5] = {0, .5f, -.2f, .9f, -.32f}, SPD[3] = {1.45f, 3.5f, 6.f};
      for (int dir = 0; dir < 2; dir++) for (int a = 0; a < 5; a++) for (int k = 0; k < 3; k++) {
        for (int i = 0; i < 180; i++) world_update(1.f / 60, v3_(0, 1.6f, 30));   /* let it close, the lens far off */
        float sgn = dir ? -1 : 1;   /* 0: from the street in, 1: from inside out */
        v3 door = v3_(0, 0, -5.45f), from = v3_add(door, v3_(sinf(ANG[a]) * 4.5f * sgn, 0, cosf(ANG[a]) * 4.5f * sgn));
        v3 to = v3_add(door, v3_(0, 0, -.9f * sgn));
        if (dir) { from = v3_(sinf(ANG[a]) * .9f, 0, -6.5f); to = v3_add(door, v3_(sinf(ANG[a]) * 2.5f, 0, 3.5f)); }
        v3 p = from, v; bool g = true; int stalls = 0, steps = 0; float open_at = -1;
        for (; steps < 900; steps++) {
          /* through the middle of the doorway, then on: a person steers, a straight line would hit a jamb */
          static int leg; if (steps == 0) leg = 0;
          v3 aim = leg == 0 ? door : to, d = v3_sub(aim, p); d.y = 0; float l = v3_len(d);
          if (leg == 0 && l < .35f) { leg = 1; continue; }
          if (leg == 1 && l < .3f) break;
          v = v3_scale(d, SPD[k] / l); v.y = -1;
          v3 q = p;
          world_update(1.f / 60, v3_(0, 1.6f, 30)); world_move(&p, &v, .3f, 1.6f, 1.f / 60, &g);
          if (hypotf(p.x - q.x, p.z - q.z) < SPD[k] / 60 * .5f) stalls++;
          if (open_at < 0 && fabsf(p.z + 5.45f) < .35f) { extern float world_debug_door(void); open_at = world_debug_door(); }
        }
        ds_log("door %s angle %.1f speed %.2f: %s in %d steps, %d stalls, door %.2f open at the threshold", dir ? "out" : "in", ANG[a], SPD[k], steps < 900 ? "through" : "BLOCKED", steps, stalls, open_at);
      }
    }
  }
  int s = frame / FRAMES_PER;
  if (s >= NSHOT) s = NSHOT - 1;
  if (s != cur) {
    cur = s;
    world_set_stage(SHOTS[s].st);
    world_set_time(SHOTS[s].t);
    if (SHOTS[s].phase) { world_arena_phase(1); world_update(2.f, SHOTS[s].eye); }
    if (!strcmp(SHOTS[s].name, "station_stop_13")) { int bells = 0; v3 bp; for (int k = 0; k < 212; k++) { world_update(.25f, v3_(0, 0, -100)); while (world_train_bell(&bp)) { bells++; ds_log("  bell at %.0f %.0f after %.1f s", bp.x, bp.z, k * .25f); } } }
    if (cam->fov != SHOTS[s].fov) { cam->fov = SHOTS[s].fov; t3_camera_update_projection_matrix(cam); }
    int d, t; world_update(.016f, SHOTS[s].eye); world_debug_counts(&d, &t);
    ds_log("shot %d %s: chunks %d tris %d", s, SHOTS[s].name, d, t);
    if (SHOTS[s].st == STAGE_TOWN) { void city_debug_dump(v3, v3); city_debug_dump(SHOTS[s].eye, v3_norm(v3_sub(SHOTS[s].at, SHOTS[s].eye))); int ct; int cd = city_stats(&ct); ds_log("   lab: %d draws %d tris", d - cd, t - ct); }
  }
  const shot_t *h = &SHOTS[s];
  t3_object_set_position(cam, h->eye.x, h->eye.y, h->eye.z);
  t3_object_look_at(cam, h->at.x, h->at.y, h->at.z);
  world_update(dt, h->eye);
  v3 f = v3_norm(v3_sub(h->at, h->eye));
  actor_place(A, v3_(h->eye.x + f.x * 3.2f - f.z * .9f, world_floor_y(v3_(h->eye.x + f.x * 3.2f, h->eye.y, h->eye.z + f.z * 3.2f)), h->eye.z + f.z * 3.2f + f.x * .9f), atan2f(-f.x, -f.z));
  actor_place(B2, v3_(h->eye.x + f.x * 3.2f + f.z * .9f, world_floor_y(v3_(h->eye.x + f.x * 3.2f, h->eye.y, h->eye.z + f.z * 3.2f)), h->eye.z + f.z * 3.2f - f.x * .9f), atan2f(-f.x, -f.z));
  actor_update(A, dt); actor_update(B2, dt);
}
void game_draw(void) {
  render_scene(scene, cam);
  if (frame % FRAMES_PER == FRAMES_PER / 2) { const t3_render_info *ri = t3_renderer_info(DS_R); ds_log("  rendered: calls %d triangles %d", (int)ri->calls, (int)ri->triangles); }
}
#endif
