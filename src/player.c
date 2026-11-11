/* player.c: see player.h. Speed follows the stick; the clip is chosen by speed and its rate
 * set so the feet travel as fast as the ground (actor_clip_ground_speed), so nothing slides.
 * The camera orbits on a spring arm that a sphere probe (world_cam_clear) shortens at once and
 * lets back out slowly once the way stays clear, tight in the lab and wide outdoors. */
#include "player.h"
#include "world.h"
#include "audio.h"
#include "weapon.h"
#include "bike.h"
#include "fx.h"
#include <math.h>

#define WALK_V 1.45f
#define JOG_V 3.5f
#define SPRINT_V 6.0f
#define GRAVITY 16.f
#define JUMP_V 5.6f
#define RADIUS .3f
#define HEIGHT 1.6f

static t3_camera *cam;
static actor_t *A;
static v3 pos, vel;
static float yaw, speed;
static float cam_yaw, cam_pitch = .16f, cam_dist = 3.4f, idle_look, full_t;
static bool grounded = true, landing;
static float air_t, land_t, step_t;
static v3 cam_pos, cam_look;
static bool cam_ok;

/* v1.2: the bike and the roam swing */
#define RIDE_V 8.5f        /* cruising: well past a sprint */
#define RIDE_FAST_V 11.f   /* sprint held: standing on the pedals */
#define RIDE_TURN_R 3.6f   /* the tightest turn at speed, metres: wider than on foot */
#define RIDE_R .4f
#define RIDER_HIP .47f     /* the sitting clip's hips over its feet: the rider's origin sits this far under the saddle */
#define RIDER_BACK .06f    /* and this far forward of it */
#define RIDER_PITCH .16f   /* leaning into the bars */
static bool riding;
static float lean, steer, wheel_a, crank_a, tick_t, pedal_k;
static v3 last_out; static float last_out_yaw;
static float swing_t, swing_len; static int swing_n, swing_queue;

void player_init(t3_camera *c) { cam = c; }
static void ride_drop(void);
void player_set_actor(actor_t *a) {
  ride_drop();
  A = a;
  if (a) { pos = actor_pos(a); yaw = actor_yaw(a); vel = v3_(0, 0, 0); speed = 0; grounded = true; }
}
actor_t *player_actor(void) { return A; }
v3 player_pos(void) { return pos; }
float player_yaw(void) { return yaw; }
void player_teleport(v3 p, float y) {
  ride_drop();
  pos = p; yaw = y; vel = v3_(0, 0, 0); speed = 0;
  if (A) actor_place(A, p, y);
  player_camera_snap();
}

/* The probe: a sphere a little wider than the near plane's corners (near .06 m with a 38 degree lens at
 * 16:9 reaches about .075 m from the eye), so a wall the lens rests against never slices the frame. */
#define CAM_R .16f
#define CAM_MIN .5f
static float cam_hold;   /* seconds the way out has stayed clear since the arm last pulled in */
static float cam_lift;   /* extra pitch while boxed in: the boom rises over the head */
/* the longest arm whose probe stays clear of walls and props, from the pivot along dir */
static float arm_length(v3 pivot, v3 dir, float want) {
  return fmaxf(CAM_MIN, world_cam_clear(pivot, v3_add(pivot, v3_scale(dir, want)), CAM_R));
}
static float want_dist(void) { return world_in_lab(pos) ? 2.3f : 3.5f; }
static v3 cam_dir_lift(float lift) {   /* from the pivot out to the camera, raised by lift */
  float pt = fminf(1.25f, cam_pitch + lift), cp = cosf(pt);
  return v3_(-sinf(cam_yaw) * cp, sinf(pt), -cosf(cam_yaw) * cp);
}
static v3 pivot_at(void) { return v3_(pos.x, pos.y + (world_in_lab(pos) ? 1.3f : 1.36f), pos.z); }
static void cam_apply(float dt, bool snap) {
  if (!cam) return;
  v3 pivot = pivot_at();
  float want = want_dist();
  /* boxed in (a doorway, a corner, a narrow room): rather than press into the back of the head, the boom
   * rises and looks down past it, to the steepest of a few higher angles that buys real room. Up quickly,
   * back down slowly. */
  float lift_to = 0, arm0 = arm_length(pivot, cam_dir_lift(0), want);
  if (arm0 < want * .6f) {
    float best = arm0;
    for (int k = 1; k <= 3; k++) { float a = arm_length(pivot, cam_dir_lift(k * .3f), want); if (a > best + .35f) { best = a; lift_to = k * .3f; } }
  }
  cam_lift = snap ? lift_to : ds_damp(cam_lift, lift_to, lift_to > cam_lift ? 8.f : 2.f, dt);
  /* a corridor or a corner: when the player is not turning the camera, it swings gently toward the
   * nearest open direction (whiskers either side), so it does not sit against a wall behind the head */
  if (!snap && arm0 < want * .5f && idle_look > .4f) {
    static const float OFF[6] = {.35f, -.35f, .7f, -.7f, 1.05f, -1.05f};
    float keep = cam_yaw;
    for (int k = 0; k < 6; k++) {
      cam_yaw = keep + OFF[k];
      float a = arm_length(pivot, cam_dir_lift(0), want);
      if (a > fmaxf(arm0 + .6f, want * .7f)) { keep += OFF[k] * (1 - expf(-1.6f * dt)); break; }
    }
    cam_yaw = keep;
  }
  v3 d = cam_dir_lift(cam_lift);
  float arm = arm_length(pivot, d, want);
  /* in at once; out only once the way has stayed clear a moment, then slowly, so passing a doorway or
   * a chair back does not pump the lens in and out */
  if (snap || arm < cam_dist) { cam_dist = arm; cam_hold = 0; }
  else { cam_hold += dt; if (cam_hold > .3f) cam_dist = ds_damp(cam_dist, arm, 2.6f, dt); }
  v3 p = v3_add(pivot, v3_scale(d, cam_dist));
  /* pressed against a wall: rise well over the head and look down past it, not into the hair (the lab's
   * ceiling is 3 m up: there is room above even where there is none behind) */
  if (cam_dist < 1.3f) p.y += (1.3f - cam_dist) * 1.2f;
  float fy = world_floor_y(v3_(p.x, p.y + .4f, p.z));
  if (fy > -1e8f && p.y < fy + .22f) p.y = fy + .22f;
  /* risen over the head, it looks a little further ahead (not higher: with the lens half a metre over her
   * a higher aim drops her out of the bottom of a 38 degree frame) */
  float ahead = (.6f + ds_clamp((1.3f - cam_dist) / .8f, 0, 1) * .25f) * cosf(fminf(1.25f, cam_pitch + cam_lift));   /* steep: lead less, or she drops to the bottom edge */
  v3 look = v3_add(pivot, v3_(sinf(cam_yaw) * ahead, -.06f, cosf(cam_yaw) * ahead));
  if (snap || !cam_ok) { cam_pos = p; cam_look = look; cam_ok = true; }
  else { cam_pos = v3_lerp(cam_pos, p, 1 - expf(-18 * dt)); cam_look = v3_lerp(cam_look, look, 1 - expf(-14 * dt)); }
  /* the damping (and the rise) must not carry the lens into a wall: probe from the pivot to where it is */
  { v3 dd = v3_sub(cam_pos, pivot); float l = v3_len(dd);
    if (l > 1e-3f) { float c = world_cam_clear(pivot, cam_pos, CAM_R); if (c < l) cam_pos = v3_add(pivot, v3_scale(dd, c / l)); } }
  /* the lab's interior and lights follow the player, not the lens */
  world_subject(pos);
  t3_object_set_position(cam, cam_pos.x, cam_pos.y, cam_pos.z);
  t3_object_look_at(cam, cam_look.x, cam_look.y, cam_look.z);
  t3_object_update_world_matrix(cam, true, true);
}
void player_camera_snap(void) {
  if (!cam) return;
  cam_yaw = yaw; cam_pitch = .16f;
  cam_apply(0, true);
}
/* the camera's yaw and height angle, after a fight hands the camera back */
void player_camera_from(v3 cpos) {
  v3 d = v3_sub(cpos, pivot_at());
  float h = sqrtf(d.x * d.x + d.z * d.z);
  cam_yaw = atan2f(-d.x, -d.z); cam_pitch = ds_clamp(atan2f(d.y, h), -.5f, 1.1f);
  cam_dist = fmaxf(.45f, v3_len(d));
  cam_pos = cpos;
}

/* the actor's root group: the top of its bone tree under the scene (actor.h keeps it private) */
static t3_object *actor_root(void) {
  t3_object *o = A ? actor_socket_rhand(A) : NULL;
  while (o && o->parent && o->parent->parent) o = o->parent;
  return o;
}
static void park(v3 p, float y) {   /* on its stand: a little lean, the bars turned */
  float fy = world_floor_y(v3_(p.x, p.y + .5f, p.z));
  if (fy > -1e8f) p.y = fy;
  bike_pose(p, y, .1f, .35f, wheel_a, crank_a);
}
bool player_riding(void) { return riding; }
/* off at once with no step aside (a teleport, a new actor): the bike stays where it was */
static void ride_drop(void) {
  if (!riding) { swing_t = 0; swing_queue = 0; if (A) weapon_trail(A, false); return; }
  riding = false;
  t3_object *r = actor_root(); if (r) t3_object_set_rotation(r, 0, yaw, 0);
  if (bike_shown()) park(bike_pos(), bike_yaw());
}
void player_ride(bool on) {
  if (!A || on == riding) return;
  riding = on;
  t3_object *r = actor_root();
  if (on) {
    swing_t = 0; swing_queue = 0;
    speed = fminf(speed, 3.f); lean = steer = 0; pedal_k = 0;
    if (r) r->rotation.order = T3_YXZ;
    last_out = pos; last_out_yaw = yaw;
    bike_pose(pos, yaw, 0, 0, wheel_a, crank_a); bike_show(true);
    fx_dust(pos, .6f, 6);
    sfx(SFX_BIKE_BELL, 0, .55f);
    actor_play(A, "Sitting_Idle_Loop", 1, true, .15f);
  } else {
    /* off to the left of it, and leave it parked where it stands */
    v3 at = bike_shown() ? bike_pos() : pos;
    park(at, yaw);
    if (r) t3_object_set_rotation(r, 0, yaw, 0);
    v3 v = v3_(cosf(yaw) * 7.f, 0, -sinf(yaw) * 7.f);   /* .7 m in a tenth of a second, sliding round anything */
    bool g = grounded; world_move(&pos, &v, RADIUS, HEIGHT, .1f, &g);
    speed = 0; vel = v3_(0, 0, 0);
    actor_place(A, pos, yaw);
    actor_play(A, "Idle_Loop", 1, true, .2f);
    sfx(SFX_PICKUP, 0, .4f);
  }
}
bool player_swinging(void) { return swing_t > 0; }
static void swing_start(bool heavy) {
  int w = weapon_of(A);
  const char *clip; float rate, seek = 0; sfx_t snd;
  if (w == WP_STAFF) { clip = heavy ? "Sword_Attack" : "Spell_Simple_Shoot"; rate = heavy ? 1.1f : 1.5f; snd = heavy ? SFX_SWING_ARC : SFX_BOLT; seek = heavy ? .1f : 0; }
  else if (heavy) { clip = "Sword_Attack"; rate = w == WP_MAUL ? .85f : 1.1f; seek = .1f; snd = w == WP_MAUL ? SFX_SWING_MAUL : w == WP_SPEAR ? SFX_SWING_SPEAR : SFX_SWING_ARC; }
  else {
    clip = swing_n & 1 ? "Sword_Regular_B" : "Sword_Regular_A";
    rate = w == WP_TWIN ? 1.7f : w == WP_MAUL ? .95f : 1.3f;
    snd = w == WP_TWIN ? SFX_SWING_TWIN : w == WP_MAUL ? SFX_SWING_MAUL : w == WP_SPEAR ? SFX_SWING_SPEAR : SFX_SWING_ARC;
    swing_n++;
  }
  if (heavy) swing_n = 0;
  actor_replay(A, clip, rate, false, .06f);
  if (seek > 0) actor_clip_seek(A, seek);
  swing_len = fmaxf(.25f, (actor_clip_len(A) - seek) / rate * .82f);
  swing_t = 1e-4f;
  weapon_trail(A, true);
  sfx(snd, 0, heavy ? .9f : .7f);
}
void player_swing(bool heavy) {
  if (!A || riding || weapon_of(A) < 0) return;
  if (swing_t > 0) { if (swing_t > swing_len * .45f) swing_queue = heavy ? 2 : 1; return; }
  swing_start(heavy);
}

/* the bike: the stick steers toward where it points, at a rate the turning circle allows; pushing it
 * back brakes; letting go coasts on the freewheel */
static void ride_step(float dt, const intent_t *in, v3 wish, float mag) {
  float want = 0, d = 0; bool brake = false;
  full_t = (in->used_pad && mag > .95f) ? full_t + dt : 0;
  if (mag > .02f) {
    d = ds_wrap_pi(atan2f(wish.x, wish.z) - yaw);
    if (fabsf(d) > 2.3f && speed > 1.2f) brake = true;
    else want = (in->sprint || full_t > .8f ? RIDE_FAST_V : RIDE_V) * (in->used_pad ? fmaxf(.3f, mag) : 1) * (1 - .45f * fminf(1, fabsf(d) / 1.6f));
  }
  if (brake) speed = fmaxf(0, speed - 11 * dt);
  else if (speed < want) speed = fminf(want, speed + 4.2f * dt);
  else speed = fmaxf(want, speed - (want > 0 ? 3.f : 1.4f) * dt);
  float max_rate = speed > .6f ? fminf(2.4f, speed / RIDE_TURN_R) : 1.4f;   /* at a crawl it still turns round, slowly */
  float rate = mag > .02f && !brake ? ds_clamp(d * 3.f, -max_rate, max_rate) : 0;
  yaw = ds_wrap_pi(yaw + rate * dt);
  steer = ds_damp(steer, ds_clamp(rate / fmaxf(speed, 1.2f) * 2.f, -.55f, .55f), 10, dt);
  lean = ds_damp(lean, ds_clamp(rate * speed * .05f, -.45f, .45f), 6, dt);
  v3 fwd = v3_(sinf(yaw), 0, cosf(yaw));
  vel.x = fwd.x * speed; vel.z = fwd.z * speed;
  if (in->jump && grounded && speed > .5f) { vel.y = 3.4f; grounded = false; air_t = 0; sfx(SFX_JUMP, 0, .45f); }   /* a hop */
  bool was = grounded;
  vel.y -= GRAVITY * dt;
  world_move(&pos, &vel, RIDE_R, HEIGHT, dt, &grounded);
  if (!grounded) air_t += dt;
  if (!was && grounded && air_t > .15f) { sfx(SFX_LAND, 0, .5f); fx_dust(pos, .5f, 5); }
  if (grounded) air_t = 0;
  float real = sqrtf(vel.x * vel.x + vel.z * vel.z);
  if (real < speed - .3f) speed = real;   /* into a wall or a bench: the speed goes with it */
  /* the door: off at the threshold, the bike stays outside */
  if (world_in_lab(pos)) {
    if (A) actor_play(A, "Idle_Loop", 1, true, .2f);
    riding = false;
    park(last_out, last_out_yaw);
    t3_object *r = actor_root(); if (r) t3_object_set_rotation(r, 0, yaw, 0);
    speed = fminf(speed, 1.5f);
    sfx(SFX_PICKUP, 0, .4f);
    return;
  }
  if (grounded) { last_out = pos; last_out_yaw = yaw; }
  /* wheels roll with the ground; the cranks turn while pedalling and stop on the freewheel, which ticks */
  bool pedalling = want > .5f && !brake && speed < want + .3f;
  pedal_k = ds_damp(pedal_k, pedalling ? 1 : 0, 6, dt);
  wheel_a = fmodf(wheel_a + speed / BIKE_WHEEL_R * dt, 6.2832f);
  crank_a = fmodf(crank_a + pedal_k * speed / (BIKE_WHEEL_R * 2.6f) * dt, 6.2832f);
  if (speed > 1.f && pedal_k < .3f) { tick_t -= dt * speed; if (tick_t <= 0) { tick_t = .32f; sfx(SFX_BIKE_TICK, 0, ds_clamp(speed / 14.f, .08f, .4f)); } }
  bike_pose(pos, yaw, lean, steer, wheel_a, crank_a);
  /* the rider: the sitting clip raised onto the saddle, leaning with the bike and into the bars, a bob
   * with the pedals */
  float oy = BIKE_SEAT_Y - RIDER_HIP + .012f * pedal_k * sinf(crank_a * 2), oz = BIKE_SEAT_Z + RIDER_BACK;
  float sx = sinf(lean) * oy, sy = cosf(lean) * oy;
  v3 rp = v3_(pos.x + sx * cosf(yaw) + oz * sinf(yaw), pos.y + sy, pos.z - sx * sinf(yaw) + oz * cosf(yaw));
  actor_play(A, "Sitting_Idle_Loop", 1, true, .15f);
  actor_move(A, rp, yaw);
  actor_set_floor(A, pos.y);
  t3_object *r = actor_root();
  if (r) { r->rotation.order = T3_YXZ; t3_object_set_rotation(r, RIDER_PITCH + .04f * pedal_k, yaw, -lean); }
}

void player_update(float dt, const intent_t *in) {
  if (!A) return;
  /* look */
  float look = fabsf(in->ax) + fabsf(in->ay);
  cam_yaw -= in->ax * 2.5f * dt;
  cam_pitch = ds_clamp(cam_pitch - in->ay * 1.5f * dt, -.38f, 1.0f);
  idle_look = look > .05f ? 0 : idle_look + dt;

  /* stick relative to the camera */
  float mag = fminf(1, sqrtf(in->mx * in->mx + in->my * in->my));
  float fx = sinf(cam_yaw), fz = cosf(cam_yaw);
  v3 wish = v3_(fx * in->my - fz * in->mx, 0, fz * in->my + fx * in->mx);
  float wl = v3_len(wish);
  if (wl > 1e-3f) wish = v3_scale(wish, 1 / wl);

  float real;
  if (riding) {
    ride_step(dt, in, wish, mag);
    real = speed;
    if (riding) actor_update(A, dt);
    else { actor_move(A, pos, yaw); actor_update(A, dt); }
  } else {
  /* a full stick held for a moment, or the sprint button, runs; keys jog until sprint is held */
  full_t = (in->used_pad && mag > .95f) ? full_t + dt : 0;
  bool run = in->sprint || full_t > .55f;
  float want = 0;
  if (mag > .02f) {
    if (run) want = SPRINT_V;
    else if (!in->used_pad && mag > .98f) want = JOG_V;   /* keys are all or nothing */
    else want = mag < .55f ? ds_lerp(WALK_V * .6f, WALK_V, mag / .55f) : ds_lerp(WALK_V, JOG_V, (mag - .55f) / .4f);
  }
  /* a swing plants the feet: a slow step at most, and it chains into the next one pressed late in it */
  if (swing_t > 0) {
    want = fminf(want, WALK_V * .35f);
    swing_t += dt;
    if (swing_t >= swing_len || in->jump) {
      swing_t = 0; weapon_trail(A, false);
      if (swing_queue && !in->jump && weapon_of(A) >= 0) { bool h = swing_queue == 2; swing_queue = 0; swing_start(h); }
      swing_queue = 0;
    }
  }
  float accel = want > speed ? 14.f : 18.f;
  speed = speed < want ? fminf(want, speed + accel * dt) : fmaxf(want, speed - accel * dt);
  if (!grounded) speed = fminf(speed, fmaxf(want, speed));

  /* turn smoothing: quick at a walk, wider at a run so she leans round */
  if (mag > .02f) {
    float tgt = atan2f(wish.x, wish.z), d = ds_wrap_pi(tgt - yaw);
    float rate = ds_lerp(13.f, 8.f, ds_clamp(speed / SPRINT_V, 0, 1));
    float turn = d * (1 - expf(-rate * dt));
    yaw = ds_wrap_pi(yaw + turn);
    /* a hard reversal at a run scrubs speed */
    if (fabsf(d) > 2.2f && speed > JOG_V) speed *= 1 - 5 * dt;
  }
  v3 fwd = v3_(sinf(yaw), 0, cosf(yaw));
  vel.x = fwd.x * speed; vel.z = fwd.z * speed;

  /* jump */
  if (in->jump && grounded) {
    vel.y = JUMP_V; grounded = false; air_t = 0; landing = false;
    actor_play(A, "Jump_Start", 1.9f, false, .08f); actor_clip_seek(A, .38f);
    sfx(SFX_JUMP, 0, .7f);
  }
  bool was = grounded;
  vel.y -= GRAVITY * dt;
  world_move(&pos, &vel, RADIUS, HEIGHT, dt, &grounded);
  if (!grounded) air_t += dt;
  if (!was && grounded) {
    if (air_t > .22f) { actor_play(A, "Jump_Land", 2.1f, false, .06f); actor_clip_seek(A, .3f); land_t = .3f; landing = true; sfx(SFX_LAND, 0, .6f); }
    air_t = 0;
  }
  if (grounded) actor_set_floor(A, pos.y);

  /* the clip follows the real ground speed, so the feet do not slide */
  real = sqrtf(vel.x * vel.x + vel.z * vel.z);
  if (land_t > 0) land_t -= dt;
  if (!grounded) {
    if (air_t > .3f && !landing) actor_play(A, "Jump_Loop", 1, true, .2f);
    if (actor_clip_done(A)) actor_play(A, "Jump_Loop", 1, true, .2f);
  } else if (swing_t > 0) {
    landing = false;   /* the swing's clip plays on */
  } else if (!(land_t > 0 && real < 1.f)) {
    landing = false;
    int wp = weapon_of(A);
    const char *idle = wp < 0 ? "Idle_Loop" : wp == WP_STAFF ? "Spell_Simple_Idle_Loop" : "Sword_Idle";   /* drawn: a guard stance */
    const char *clip = real < .15f ? idle : real < 2.2f ? "Walk_Loop" : real < 4.7f ? "Jog_Fwd_Loop" : "Sprint_Loop";
    actor_play(A, clip, 1, true, .18f);
    float g = actor_clip_ground_speed(A);
    if (real >= .15f && g > .05f) actor_play(A, clip, ds_clamp(real / g, .35f, 2.6f), true, .18f);
    /* steps */
    if (real > .5f) {
      step_t -= dt * real;
      if (step_t <= 0) { step_t = real > 4.7f ? 2.3f : real > 2.2f ? 1.75f : 1.0f; sfx(SFX_STEP, ds_rr(-.1f, .1f), ds_clamp(real / 8.f, .1f, .5f)); }
    }
  }
  actor_move(A, pos, yaw);
  actor_update(A, dt);
  }

  /* a gentle drift back behind her once the stick is left alone and she is moving; on the bike sooner
   * and firmer, since it cannot turn on the spot to face the lens */
  if (riding) { if (idle_look > .5f && real > 1.5f) cam_yaw += ds_wrap_pi(yaw - cam_yaw) * (1 - expf(-2.f * dt)); }
  else if (idle_look > 1.4f && real > 1.f) cam_yaw += ds_wrap_pi(yaw - cam_yaw) * (1 - expf(-.6f * dt));
  cam_apply(dt, false);
  audio_listener(cam_pos.x, cam_pos.y, cam_pos.z, cam_yaw);
}
