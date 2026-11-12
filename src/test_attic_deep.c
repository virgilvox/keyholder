/* test_attic_deep.c: a showroom of the attic's layouts for headless screenshots, and a walk along the
 * nightmare's route. No gameplay. First a list of views (fixed cameras, or the real follow camera behind a
 * body standing somewhere), each held for its frames; the frame to shoot for each is logged at boot
 * ("deep: shoot ..."). Then a body walks the whole route on scripted stick input through player_update,
 * logging where it gets stuck, how often the lens ends up inside a solid, and a line every 300 frames.
 * Build with EXTRA_CFLAGS=-DKH_TEST_ATTIC_DEEP and game.c left out of KH_SRC. */
#ifdef KH_TEST_ATTIC_DEEP
#include "game.h"
#include "world.h"
#include "player.h"
#include "actor.h"
#include "fx.h"
#include "input.h"
#include "attic_env.h"
#include "gen/attic_deep_data.h"
#include "hud_int.h"
#include "ds.h"
#include <string.h>
#include <math.h>

static t3_scene *scene; static t3_camera *cam; static actor_t *A;

typedef struct {
  const char *what;
  int layout, follow, phase, balloon, hold;
  v3 at; float yaw;      /* where the body stands (the follow camera starts behind it) */
  v3 pos, look;          /* a fixed camera */
  bool plan;             /* hide the roof: a look down on the plan */
} view_t;
#define MAXV 40
static view_t V[MAXV]; static int NV;
static int start_of[MAXV];

static v3 land_at(int i) { return v3_(ATTIC_DEEP_LANDINGS[i].x, 0, ATTIC_DEEP_LANDINGS[i].z); }
static v3 ring_at(int i) { return v3_(ATTIC_DEEP_CLEARINGS[i].x, 0, ATTIC_DEEP_CLEARINGS[i].z); }
static void follow(const char *w, int layout, v3 at, float yaw) { V[NV++] = (view_t){ .what = w, .layout = layout, .follow = 1, .hold = 70, .at = at, .yaw = yaw, .balloon = -1 }; }
static void fixed(const char *w, int layout, v3 at, v3 pos, v3 look) { V[NV++] = (view_t){ .what = w, .layout = layout, .hold = 50, .at = at, .yaw = DS_PI, .pos = pos, .look = look, .balloon = -1 }; }
static v3 anchor(const char *n, float *yaw) { v3 p = v3_(0, 0, 0); attic_env_layout(ATTIC_LAYOUT_DESERT); attic_env_anchor(n, &p, yaw); return p; }

static void make_views(void) {
  const int D = ATTIC_LAYOUT_DEEP, S = ATTIC_LAYOUT_DESERT;
  follow("deep: the stair head, third person", D, land_at(0), DS_PI);
  follow("deep: landing 1, the second soft run ahead", D, land_at(1), DS_PI);
  fixed("deep: down the holiday aisle from above the stair", D, land_at(1), v3_add(land_at(0), v3_(0, 4.2f, 2.4f)), v3_add(land_at(0), v3_(0, 0.8f, -18)));
  fixed("deep: looking up the canyon", D, land_at(1), v3_add(land_at(1), v3_(0.6f, 1.2f, 3.5f)), v3_add(land_at(1), v3_(-0.3f, 7.5f, -8)));
  { v3 c = ring_at(0); fixed("deep: holiday clearing", D, c, v3_add(c, v3_(-6.0f, 3.2f, 0.4f)), v3_add(c, v3_(5, 1.6f, -3))); }
  { v3 c = ring_at(0); fixed("deep: holiday clearing, back", D, c, v3_add(c, v3_(3.6f, 3.0f, -1.0f)), v3_add(c, v3_(-5, 1.2f, 4))); }
  follow("deep: members aisle", D, land_at(3), DS_PI);
  { v3 c = ring_at(1); fixed("deep: members clearing", D, c, v3_add(c, v3_(0.3f, 3.2f, 6.0f)), v3_add(c, v3_(-3, 1.4f, -5))); }
  follow("deep: robotics aisle", D, land_at(6), -DS_PI / 2);
  { v3 c = ring_at(2); fixed("deep: robotics clearing", D, c, v3_add(c, v3_(0.2f, 3.2f, 6.0f)), v3_add(c, v3_(2, 1.2f, -5))); }
  follow("deep: metal aisle", D, land_at(9), DS_PI);
  follow("deep: metal aisle, east leg", D, land_at(10), DS_PI / 2);
  { v3 c = ring_at(3); fixed("deep: metal clearing", D, c, v3_add(c, v3_(-6.0f, 3.2f, 0.3f)), v3_add(c, v3_(5, 1.3f, 2))); }
  follow("deep: the last landing before the boss", D, land_at(13), DS_PI);
  { v3 b = v3_(ATTIC_DEEP_BOSS_X, 0, ATTIC_DEEP_BOSS_Z);
    fixed("deep: boss chamber", D, b, v3_add(b, v3_(1.5f, 3.6f, 8.0f)), v3_add(b, v3_(-2.5f, 1.8f, -6)));
    V[NV] = V[NV - 1]; V[NV].what = "deep: boss chamber, phase two"; V[NV].phase = 1; V[NV].hold = 160; NV++;
    fixed("deep: boss chamber from the rack", D, b, v3_add(b, v3_(-6.5f, 2.6f, -6.0f)), v3_add(b, v3_(3, 1.5f, 3)));
    V[NV] = V[NV - 1]; V[NV].what = "deep: from the rack, phase two"; V[NV].phase = 1; V[NV].hold = 160; NV++; }
  follow("deep: at the terminal", D, v3_(ATTIC_DEEP_TERM_X, 0, ATTIC_DEEP_TERM_Z), ATTIC_DEEP_TERM_YAW);
  { v3 c = v3_(ATTIC_DEEP_X0 * 0.5f + ATTIC_DEEP_X1 * 0.5f, 0, -20); fixed("deep: plan, south half", D, land_at(0), v3_add(c, v3_(0, 70, 1)), c); V[NV - 1].plan = true; }
  { v3 c = v3_(ATTIC_DEEP_X0 * 0.5f + ATTIC_DEEP_X1 * 0.5f, 0, -80); fixed("deep: plan, north half", D, land_at(0), v3_add(c, v3_(0, 70, 1)), c); V[NV - 1].plan = true; }
  /* the desert */
  float y;
  v3 w = anchor("cam_wide", &y), tb = anchor("table", NULL), fill = anchor("fill", NULL), low = anchor("cam_low", NULL), sa = anchor("stand_a", &y);
  fixed("desert: wide", S, sa, w, v3_add(tb, v3_(-2, 0.5f, -3)));
  follow("desert: at the table", S, sa, DS_PI);
  fixed("desert: the balloon laid out", S, sa, v3_add(fill, v3_(2.5f, 1.6f, 3.0f)), v3_add(fill, v3_(0, 0.2f, -0.5f)));
  fixed("desert: filling", S, sa, low, v3_add(fill, v3_(0, 1.4f, 0))); V[NV - 1].balloon = 1; V[NV - 1].hold = 240;
  fixed("desert: let go", S, sa, low, v3_add(fill, v3_(-1, 6, -2))); V[NV - 1].balloon = 2; V[NV - 1].hold = 150;
  fixed("desert: rising", S, sa, low, v3_add(fill, v3_(-4, 22, -6))); V[NV - 1].balloon = -2; V[NV - 1].hold = 200;
  fixed("desert: east, into the sunrise", S, sa, v3_add(fill, v3_(-6, 1.7f, 2)), v3_add(fill, v3_(30, 3, 8)));
  fixed("desert: west, the lit range", S, sa, v3_add(tb, v3_(4, 1.7f, 2)), v3_add(tb, v3_(-40, 2, -6)));
  fixed("desert: the truck", S, sa, v3_add(tb, v3_(-1.5f, 1.6f, 5.5f)), v3_add(tb, v3_(4, 0.8f, -3)));
}

void game_init(void) {
  scene = t3_scene_new();
  cam = t3_perspective_camera_new(50, 16.f / 9.f, .06f, 1600);
  t3_object_add(&scene->base, cam);
  hg_init();
  world_init(scene);
  fx_init(scene);
  player_init(cam);
  attic_env_init(scene);
  A = actor_new(AM_C, &ACTOR_PAL_NONE, 1, scene);
  actor_play(A, "Idle_Loop", 1, true, 0);
  player_set_actor(A);
  make_views();
  attic_env_layout(ATTIC_LAYOUT_DEEP);
  world_set_stage(STAGE_ATTIC);
  attic_env_show(true);
  char buf[1024]; int n = 0, f = 0;
  for (int i = 0; i < NV; i++) { start_of[i] = f; f += V[i].hold; n += snprintf(buf + n, sizeof buf - n, "%s%d", i ? "," : "", f - 8); }
  ds_log("deep: shoot %s", buf);
  for (int i = 0; i < NV; i++) ds_log("deep: view %d at frame %d: %s", i, start_of[i] + V[i].hold - 8, V[i].what);
  ds_log("deep: the walk starts at frame %d", f);
}

/* ---------------- the walk ---------------- */
static bool nohide;   /* A held at boot: never call attic_env_focus (an A/B of the far chunk hiding) */
static int walk_i = 1, walk_frames, stuck_frames, lens_in, lens_n, soft_frames;
static float best_s, walk_s;
static v3 last_p;
static float route_s(v3 p) {   /* how far along the route the nearest point on it is */
  float best = 1e9f, acc = 0, out = 0;
  for (int i = 0; i + 1 < (int)(sizeof ATTIC_DEEP_ROUTE / sizeof ATTIC_DEEP_ROUTE[0]); i++) {
    v3 a = v3_(ATTIC_DEEP_ROUTE[i].x, 0, ATTIC_DEEP_ROUTE[i].z), b = v3_(ATTIC_DEEP_ROUTE[i + 1].x, 0, ATTIC_DEEP_ROUTE[i + 1].z);
    v3 ab = v3_sub(b, a); float L = v3_len(ab), t = ds_clamp(v3_dot(v3_sub(p, a), ab) / (L * L), 0, 1);
    v3 q = v3_add(a, v3_scale(ab, t)); float d = hypotf(p.x - q.x, p.z - q.z);
    if (d < best) { best = d; out = acc + t * L; }
    acc += L;
  }
  return out;
}
static bool lens_inside(v3 c) {
  for (int i = 0; i < (int)(sizeof ATTIC_DEEP_SOLIDS / sizeof ATTIC_DEEP_SOLIDS[0]); i++) {
    const attic_box_t *b = &ATTIC_DEEP_SOLIDS[i];
    if (c.x > b->x0 && c.x < b->x1 && c.z > b->z0 && c.z < b->z1 && c.y < b->top) return true;
  }
  return c.y > ATTIC_DEEP_ROOF;
}
static void walk_frame(float dt) {
  const int NR = (int)(sizeof ATTIC_DEEP_ROUTE / sizeof ATTIC_DEEP_ROUTE[0]);
  if (walk_frames == 0) {
    attic_env_layout(ATTIC_LAYOUT_DEEP); attic_env_phase(0);
    float yaw; v3 e = attic_env_entry(&yaw);
    player_teleport(e, yaw); player_camera_snap();
    last_p = e;
    ds_log("deep: walk from %.1f %.1f", e.x, e.z);
  }
  walk_frames++;
  v3 p = player_pos();
  intent_t in; memset(&in, 0, sizeof in); in.used_pad = true;
  if (walk_i < NR) {
    v3 goal = v3_(ATTIC_DEEP_ROUTE[walk_i].x, 0, ATTIC_DEEP_ROUTE[walk_i].z);
    float dx = goal.x - p.x, dz = goal.z - p.z, d = hypotf(dx, dz);
    if (d < 0.7f) walk_i++;
    else {
      /* the stick, relative to the camera: the lens is behind the head, so its yaw is from it to the body */
      t3_vec3 cp = ((t3_object *)cam)->position;
      float cy = atan2f(p.x - cp.x, p.z - cp.z), fx = sinf(cy), fz = cosf(cy);
      float ux = dx / d, uz = dz / d;
      in.my = (ux * fx + uz * fz) * 0.93f;
      in.mx = (ux * -fz + uz * fx) * 0.93f;
    }
  }
  player_update(dt, &in);
  p = player_pos();
  if (!nohide) attic_env_focus(p);
  world_update(dt, p);
  walk_s = route_s(p);
  if (walk_s > best_s + 0.05f) { best_s = walk_s; stuck_frames = 0; } else stuck_frames++;
  if (stuck_frames == 120) ds_log("deep: STUCK at %.2f %.2f (route %.1f m)", p.x, p.z, walk_s);
  if (attic_env_soft(p)) soft_frames++;
  t3_vec3 cp = ((t3_object *)cam)->position;
  lens_n++; if (lens_inside(v3_(cp.x, cp.y, cp.z))) { lens_in++; if (lens_in % 30 == 1) ds_log("deep: lens inside a solid at %.2f %.2f %.2f (body %.2f %.2f)", cp.x, cp.y, cp.z, p.x, p.z); }
  if (walk_frames % 300 == 0) ds_log("deep: walk frame %d at %.1f %.1f, route %.1f of %.1f m, lens %.1f %.1f %.1f, cam inside %d of %d, on soft boards %d", walk_frames, p.x, p.z, walk_s, ATTIC_DEEP_ROUTE_LEN, cp.x, cp.y, cp.z, lens_in, lens_n, soft_frames);
  if (walk_i >= NR && walk_frames % 60 == 0) ds_log("deep: walk done at frame %d (%.1f s), route %.1f m", walk_frames, walk_frames / 60.f, walk_s);
  (void)last_p;
}

static int fr = -1;
static void plan_roof(bool hide) {
  char nm[24];
  for (int k = 0; k < 8; k++) { snprintf(nm, sizeof nm, "deep_roof_%d", k); t3_object *o = t3_object_get_by_name(&scene->base, nm); if (o) o->visible = !hide; }
}
void game_frame(float dt) {
  fr++;
  if (fr == 1) { nohide = input_get()->jump_held; if (nohide) ds_log("deep: hiding off"); }
  int v = 0;
  while (v < NV && fr >= start_of[v] + V[v].hold) v++;
  if (v >= NV) { walk_frame(dt); return; }
  const view_t *w = &V[v];
  if (fr == start_of[v]) {
    if (w->balloon != -2) attic_env_layout(w->layout);   /* a clean start: phase, balloon, hidden chunks */
    attic_env_phase(w->phase);
    if (w->balloon >= 0) attic_env_balloon(w->balloon);
    player_teleport(w->at, w->yaw);
    if (w->follow) player_camera_snap();
  }
  if (!nohide) attic_env_focus(w->plan ? w->look : player_pos());
  if (w->follow) { intent_t in; memset(&in, 0, sizeof in); player_update(dt, &in); }
  else {
    actor_update(A, dt);
    t3_object_set_position(cam, w->pos.x, w->pos.y, w->pos.z);
    t3_object_look_at(cam, w->look.x, w->look.y, w->look.z);
    t3_object_update_world_matrix(cam, true, true);
  }
  world_update(dt, w->follow ? player_pos() : w->pos);
  if (w->plan) { scene->fog.far = 900; scene->fog.near = 800; }   /* the plan from far above: no fog, no roof */
  plan_roof(w->plan);
}

void game_draw(void) { render_scene(scene, cam); render_target(); }
#endif
