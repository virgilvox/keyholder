/* player.c: see player.h. Speed follows the stick; the clip is chosen by speed and its rate
 * set so the feet travel as fast as the ground (actor_clip_ground_speed), so nothing slides.
 * The camera orbits on a spring arm that world_raycast shortens at once and lets back out
 * slowly, tight in the lab and wide outdoors. */
#include "player.h"
#include "world.h"
#include "audio.h"
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

void player_init(t3_camera *c) { cam = c; }
void player_set_actor(actor_t *a) {
  A = a;
  if (a) { pos = actor_pos(a); yaw = actor_yaw(a); vel = v3_(0, 0, 0); speed = 0; grounded = true; }
}
actor_t *player_actor(void) { return A; }
v3 player_pos(void) { return pos; }
float player_yaw(void) { return yaw; }
void player_teleport(v3 p, float y) {
  pos = p; yaw = y; vel = v3_(0, 0, 0); speed = 0;
  if (A) actor_place(A, p, y);
  player_camera_snap();
}

/* shortest arm that does not cross a wall, from the pivot, along dir */
static float arm_length(v3 pivot, v3 dir, float want) {
  float t; v3 n;
  if (world_raycast(pivot, dir, want + .25f, &t, &n)) return fmaxf(.45f, t - .25f);
  return want;
}
static float want_dist(void) { return world_in_lab(pos) ? 2.3f : 3.5f; }
static v3 cam_dir(void) {   /* from the pivot out to the camera */
  float cp = cosf(cam_pitch);
  return v3_(-sinf(cam_yaw) * cp, sinf(cam_pitch), -cosf(cam_yaw) * cp);
}
static v3 pivot_at(void) { return v3_(pos.x, pos.y + (world_in_lab(pos) ? 1.3f : 1.36f), pos.z); }
static void cam_apply(float dt, bool snap) {
  if (!cam) return;
  v3 pivot = pivot_at(), d = cam_dir();
  float want = want_dist(), arm = arm_length(pivot, d, want);
  /* in at once, out slowly */
  if (snap) cam_dist = arm;
  else cam_dist = arm < cam_dist ? arm : ds_damp(cam_dist, arm, 2.6f, dt);
  v3 p = v3_add(pivot, v3_scale(d, cam_dist));
  /* pressed against a wall: rise and look down past the head, not at the back of the collar */
  if (cam_dist < 1.3f) p.y += (1.3f - cam_dist) * .55f;
  float fy = world_floor_y(v3_(p.x, p.y + .4f, p.z));
  if (fy > -1e8f && p.y < fy + .22f) p.y = fy + .22f;
  v3 look = v3_add(pivot, v3_(sinf(cam_yaw) * .6f, -.06f, cosf(cam_yaw) * .6f));
  if (snap || !cam_ok) { cam_pos = p; cam_look = look; cam_ok = true; }
  else { cam_pos = v3_lerp(cam_pos, p, 1 - expf(-18 * dt)); cam_look = v3_lerp(cam_look, look, 1 - expf(-14 * dt)); }
  /* the damping must not drag the camera back through a wall */
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

  /* a full stick held for a moment, or the sprint button, runs; keys jog until sprint is held */
  full_t = (in->used_pad && mag > .95f) ? full_t + dt : 0;
  bool run = in->sprint || full_t > .55f;
  float want = 0;
  if (mag > .02f) {
    if (run) want = SPRINT_V;
    else if (!in->used_pad && mag > .98f) want = JOG_V;   /* keys are all or nothing */
    else want = mag < .55f ? ds_lerp(WALK_V * .6f, WALK_V, mag / .55f) : ds_lerp(WALK_V, JOG_V, (mag - .55f) / .4f);
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
  float real = sqrtf(vel.x * vel.x + vel.z * vel.z);
  if (land_t > 0) land_t -= dt;
  if (!grounded) {
    if (air_t > .3f && !landing) actor_play(A, "Jump_Loop", 1, true, .2f);
    if (actor_clip_done(A)) actor_play(A, "Jump_Loop", 1, true, .2f);
  } else if (!(land_t > 0 && real < 1.f)) {
    landing = false;
    const char *clip = real < .15f ? "Idle_Loop" : real < 2.2f ? "Walk_Loop" : real < 4.7f ? "Jog_Fwd_Loop" : "Sprint_Loop";
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

  /* a gentle drift back behind her once the stick is left alone and she is moving */
  if (idle_look > 1.4f && real > 1.f) cam_yaw += ds_wrap_pi(yaw - cam_yaw) * (1 - expf(-.6f * dt));
  cam_apply(dt, false);
  audio_listener(cam_pos.x, cam_pos.y, cam_pos.z, cam_yaw);
}
