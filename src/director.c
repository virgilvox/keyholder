/* director.c: see director.h. */
#include "director.h"
#include "world.h"
#include <string.h>

static t3_camera *cam;
typedef struct { v3 pos, look; float fov, roll; } frame_t;
typedef struct {
  dir_spec s;
  float ang, dk, hk;  /* the chosen candidate */
  int side;
  float t, handheld, drift;
  v3 pos, look;       /* damped */
  bool fresh;
} shot_t;
static shot_t S;
static bool active;
static float T;
static int line_side; static actor_t *line_a, *line_b;
static v3 last_pos, last_look;
static bool have_last;
static actor_t *const *OCC; static int NOCC;
void director_set_occluders(actor_t *const *list, int n) { OCC = list; NOCC = n; }
/* the shortest distance from point p to segment a-b */
static float seg_dist(v3 p, v3 a, v3 b) { v3 ab = v3_sub(b, a); float t = ds_clamp(v3_dot(v3_sub(p, a), ab) / fmaxf(1e-5f, v3_dot(ab, ab)), 0, 1); return v3_len(v3_sub(p, v3_add(a, v3_scale(ab, t)))); }

static v3 eye(actor_t *a) { v3 h = actor_head_pos(a); h.y += .05f; return h; }
static v3 chest(actor_t *a) { v3 h = actor_head_pos(a); h.y -= .42f; return h; }
static v3 center(const dir_spec *s) {
  v3 c = v3_(0, 0, 0);
  for (int i = 0; i < s->nsubj; i++) c = v3_add(c, actor_pos(s->subj[i]));
  return s->nsubj ? v3_scale(c, 1.f / s->nsubj) : c;
}
static frame_t frame_of(const dir_spec *s, float ang, float dk, float hk, int side, float t) {
  frame_t f = { v3_(0, 0, 0), v3_(0, 0, 0), 40, 0 };
  actor_t *A = s->subj[0], *B = s->nsubj > 1 ? s->subj[1] : NULL;
  switch (s->type) {
    case SH_FIXED: f.pos = s->fpos; f.look = s->flook; f.fov = s->fov > 0 ? s->fov : 40; break;
    case SH_WIDE: case SH_ESTABLISH: {
      v3 c = center(s);
      bool e = s->type == SH_ESTABLISH;
      float d = (s->dist > 0 ? s->dist : (e ? 9 : 4.6f)) * dk, h = (s->height > 0 ? s->height : (e ? 3.2f : 1.75f)) + hk;
      f.pos = v3_(c.x + sinf(ang) * d, c.y + h, c.z + cosf(ang) * d);
      f.look = v3_(c.x, c.y + (e ? 1.1f : 1.15f), c.z);
      f.fov = s->fov > 0 ? s->fov : (e ? 50 : 40);
      break;
    }
    case SH_TWO: {
      v3 a = actor_pos(A), b = B ? actor_pos(B) : a;
      float ly = atan2f(b.x - a.x, b.z - a.z), yaw = ly + side * DS_PI / 2 + ang;
      v3 c = v3_scale(v3_add(a, b), .5f);
      float dd = ((s->dist > 0 ? s->dist : 2.5f) + v3_len(v3_sub(b, a)) * .8f) * dk;
      f.pos = v3_(c.x + sinf(yaw) * dd, c.y + 1.38f, c.z + cosf(yaw) * dd);
      f.look = v3_(c.x, c.y + 1.2f, c.z);
      f.fov = s->fov > 0 ? s->fov : 34;
      break;
    }
    case SH_OTS: {
      v3 a = actor_pos(A), b = B ? actor_pos(B) : v3_add(a, v3_(0, 0, 1));
      v3 dir = v3_sub(b, a); dir.y = 0; dir = v3_norm(dir);
      v3 right = v3_(dir.z, 0, -dir.x), ea = eye(A), eb = B ? eye(B) : b;
      f.pos = v3_add(v3_add(ea, v3_scale(dir, -dk * .95f)), v3_scale(right, side * .62f));
      f.pos.y = ea.y + .08f;
      f.look = v3_add(eb, v3_scale(right, side * .2f)); f.look.y -= .08f;
      f.fov = s->fov > 0 ? s->fov : 27;
      break;
    }
    case SH_MCU: case SH_CU: case SH_LOW: case SH_DUTCH: {
      float near = s->type == SH_CU ? 1.25f : s->type == SH_LOW ? 2.0f : 1.5f;
      float fy = (B ? atan2f(actor_pos(B).x - actor_pos(A).x, actor_pos(B).z - actor_pos(A).z) : actor_yaw(A));
      float y = fy + ang;
      v3 p = actor_pos(A), e = eye(A);
      float h = s->type == SH_LOW ? p.y + .62f : e.y - .05f;
      f.pos = v3_(p.x + sinf(y) * near * dk, h, p.z + cosf(y) * near * dk);
      v3 lead = v3_(sinf(fy), 0, cosf(fy));
      f.look = v3_add(e, v3_scale(lead, s->type == SH_CU ? .04f : .1f));
      f.look.y = e.y - (s->type == SH_CU ? .03f : s->type == SH_LOW ? .12f : .14f);
      f.fov = s->fov > 0 ? s->fov : (s->type == SH_CU ? 24 : s->type == SH_LOW ? 36 : 30);
      f.roll = s->type == SH_DUTCH ? .16f : 0;
      break;
    }
    case SH_ORBIT: {
      v3 c = center(s); float d = (s->dist > 0 ? s->dist : 2.6f) * dk, y = ang + t * .22f;
      f.pos = v3_(c.x + sinf(y) * d, c.y + 1.5f, c.z + cosf(y) * d);
      f.look = v3_(c.x, c.y + 1.2f, c.z);
      f.fov = s->fov > 0 ? s->fov : 36;
      break;
    }
    case SH_CRANE: {
      v3 c = center(s);
      float k = ds_clamp(t / (s->dur > 0 ? s->dur : 5), 0, 1), e = k * k * (3 - 2 * k), d0 = s->dist > 0 ? s->dist : 4;
      float h = ds_lerp(s->height > 0 ? s->height : .7f, s->height > 0 ? s->height + 3 : 4.2f, e), d = ds_lerp(d0 * .7f, d0 * 1.4f, e);
      f.pos = v3_(c.x + sinf(ang) * d, c.y + h, c.z + cosf(ang) * d);
      f.look = v3_(c.x, c.y + ds_lerp(1.3f, .9f, e), c.z);
      f.fov = s->fov > 0 ? s->fov : 42;
      break;
    }
    case SH_TRACK: {
      v3 c = center(s); float d = (s->dist > 0 ? s->dist : 3.2f) * dk, hy = actor_yaw(A), y = hy + ang;
      f.pos = v3_(c.x + sinf(y) * d, c.y + 1.45f, c.z + cosf(y) * d);
      f.look = v3_(c.x + sinf(hy) * .6f, c.y + 1.25f, c.z + cosf(hy) * .6f);
      f.fov = s->fov > 0 ? s->fov : 38;
      break;
    }
    default: break;
  }
  return f;
}
static bool near_wall(v3 p, float r) {
  static const v3 D[6] = {{1,0,0},{-1,0,0},{0,0,1},{0,0,-1},{0,1,0},{0,-1,0}};
  for (int i = 0; i < 6; i++) { float t; v3 n; if (world_raycast(p, D[i], r, &t, &n)) return true; }
  return false;
}
static float vis_score(const dir_spec *s, v3 pos) {
  float got = 0, n = 0;
  for (int i = 0; i < s->nsubj; i++) {
    v3 pts[2] = { eye(s->subj[i]), chest(s->subj[i]) };
    for (int k = 0; k < 2; k++) { n += 1; if (world_visible(pos, pts[k])) got += 1; }
  }
  return n > 0 ? got / n : 1;
}
static void try_cand(const dir_spec *s, float ang, float dk, float hk, int side, float prefer, float *best, float *ba, float *bd, float *bh, int *bs) {
  frame_t f = frame_of(s, ang, dk, hk, side, 0);
  if (near_wall(f.pos, .22f)) return;
  float v = vis_score(s, f.pos);
  float sc = v * 3 + prefer - (v < .6f ? 2 : 0);
  /* other people: nobody's hair fills the frame. The shoulder in an over the shoulder may sit in
   * the corner of the shot, never across the face we are looking at */
  v3 target = s->type == SH_OTS && s->nsubj > 1 ? eye(s->subj[1]) : eye(s->subj[0]);
  for (int i = 0; i < NOCC; i++) {
    actor_t *o = OCC[i];
    if (!actor_is_visible(o)) continue;
    bool is_subject = false, is_shoulder = s->type == SH_OTS && o == s->subj[0];
    bool close_up = s->type == SH_MCU || s->type == SH_CU || s->type == SH_LOW || s->type == SH_DUTCH;
    /* a close up has one subject: its second is only who they are facing, and can be in the way */
    for (int k = 0; k < (close_up ? 1 : s->nsubj); k++) if (s->subj[k] == o) is_subject = true;
    v3 h = actor_head_pos(o), c2 = v3_(h.x, h.y - .5f, h.z), c3 = v3_(h.x, h.y - 1.f, h.z);
    float near_cam = fminf(seg_dist(f.pos, h, c3), v3_len(v3_sub(f.pos, h)));
    if (near_cam < (is_shoulder ? .42f : .6f)) { if (!is_subject || is_shoulder) return; }
    if (is_subject && !is_shoulder) continue;
    float blk = fminf(seg_dist(h, f.pos, target), seg_dist(c2, f.pos, target));
    if (blk < (is_shoulder ? .27f : .3f)) sc -= is_shoulder ? 2.5f : 2.0f;
  }
  if (!world_visible(f.pos, v3_add(f.pos, v3_scale(v3_norm(v3_sub(f.look, f.pos)), .9f)))) sc -= 3;
  if (s->nsubj >= 2) {
    v3 a = actor_pos(s->subj[0]), b = actor_pos(s->subj[1]);
    int sd = (b.x - a.x) * (f.pos.z - a.z) - (b.z - a.z) * (f.pos.x - a.x) > 0 ? 1 : -1;
    bool same = (line_a == s->subj[0] && line_b == s->subj[1]);
    bool flip = (line_a == s->subj[1] && line_b == s->subj[0]);
    if ((same && sd != line_side) || (flip && sd == line_side)) sc -= .9f;
  }
  if (have_last) {
    v3 d1 = v3_norm(v3_sub(last_look, last_pos)), d2 = v3_norm(v3_sub(f.look, f.pos));
    float a = acosf(ds_clamp(v3_dot(d1, d2), -1, 1));
    if (a < .35f && v3_len(v3_sub(last_pos, f.pos)) < 1.5f) sc -= .6f;
  }
  sc += ds_rand() * .05f;
  if (sc > *best) { *best = sc; *ba = ang; *bd = dk; *bh = hk; *bs = side; }
}
void director_init(t3_camera *c) { cam = c; }
bool director_active(void) { return active; }
void director_stop(void) { active = false; }
void director_cut(const dir_spec *in) {
  dir_spec s = *in;
  float best = -1e9f, ba = 0, bd = 1, bh = 0; int bs = 1;
  switch (s.type) {
    case SH_FIXED: break;
    case SH_WIDE: case SH_ESTABLISH:
      for (int r = 0; r < 24; r++) { float y = r * DS_PI / 12;
        try_cand(&s, y, 1, 0, 1, 0, &best, &ba, &bd, &bh, &bs);
        try_cand(&s, y, .75f, 0, 1, -.1f, &best, &ba, &bd, &bh, &bs);
        try_cand(&s, y, .55f, .2f, 1, -.25f, &best, &ba, &bd, &bh, &bs);
        try_cand(&s, y, 1, .9f, 1, -.15f, &best, &ba, &bd, &bh, &bs); }
      break;
    case SH_TWO: {
      static const float off[7] = {-.9f, -.6f, -.3f, 0, .3f, .6f, .9f}, dks[3] = {1, .75f, 1.3f};
      for (int d = 0; d < 3; d++) for (int o = 0; o < 7; o++) for (int sd = -1; sd <= 1; sd += 2)
        try_cand(&s, off[o], dks[d], 0, sd, -fabsf(off[o]) * .15f - fabsf(dks[d] - 1) * .3f, &best, &ba, &bd, &bh, &bs);
      break;
    }
    case SH_OTS:
      for (int sd = -1; sd <= 1; sd += 2) { try_cand(&s, 0, 1, 0, sd, 0, &best, &ba, &bd, &bh, &bs); try_cand(&s, 0, 1.35f, 0, sd, -.05f, &best, &ba, &bd, &bh, &bs); }
      break;
    case SH_MCU: case SH_CU: case SH_LOW: case SH_DUTCH: {
      static const float angs[11] = {-.75f, -.5f, -.25f, 0, .25f, .5f, .75f, -1.05f, 1.05f, -1.4f, 1.4f}, dks[3] = {1, .8f, 1.3f};
      for (int d = 0; d < 3; d++) for (int a = 0; a < 11; a++)
        try_cand(&s, angs[a], dks[d], 0, 1, -fabsf(angs[a]) * .25f + (fabsf(angs[a]) > .1f ? .05f : 0) - fabsf(dks[d] - 1) * .4f, &best, &ba, &bd, &bh, &bs);
      break;
    }
    case SH_ORBIT: case SH_CRANE:
      for (int r = 0; r < 12; r++) try_cand(&s, r * DS_PI / 6, 1, 0, 1, 0, &best, &ba, &bd, &bh, &bs);
      break;
    case SH_TRACK:
      try_cand(&s, DS_PI / 2, 1, 0, 1, .1f, &best, &ba, &bd, &bh, &bs);
      try_cand(&s, -DS_PI / 2, 1, 0, 1, 0, &best, &ba, &bd, &bh, &bs);
      break;
    default: break;
  }
  /* nothing clean from here (a tight corner): step back to a wide, which always finds something */
  if (best < -50 && s.type != SH_FIXED && s.type != SH_WIDE) {
    s.type = SH_WIDE; s.dist = 3.6f; s.height = 1.6f;
    for (int r = 0; r < 24; r++) try_cand(&s, r * DS_PI / 12, 1, 0, 1, 0, &best, &ba, &bd, &bh, &bs);
  }
  S.s = s; S.ang = ba; S.dk = bd; S.hk = bh; S.side = bs; S.t = 0; S.fresh = true;
  S.handheld = s.type == SH_TRACK ? .6f : (s.type == SH_MCU || s.type == SH_OTS) ? .35f : .2f;
  S.drift = (s.type == SH_FIXED || s.type == SH_ORBIT || s.type == SH_CRANE) ? 0 : .06f;
  active = true;
  if (s.nsubj >= 2) {
    frame_t f = frame_of(&s, ba, bd, bh, bs, 0);
    v3 a = actor_pos(s.subj[0]), b = actor_pos(s.subj[1]);
    line_a = s.subj[0]; line_b = s.subj[1];
    line_side = (b.x - a.x) * (f.pos.z - a.z) - (b.z - a.z) * (f.pos.x - a.x) > 0 ? 1 : -1;
  }
}
static float nz(float a, float b, float c) { return sinf(T * a + c) * .5f + sinf(T * b + c * 1.7f) * .5f; }
void director_update(float dt) {
  if (!active || !cam) return;
  T += dt; S.t += dt;
  frame_t f = frame_of(&S.s, S.ang, S.dk, S.hk, S.side, S.t);
  if (S.drift > 0) {
    v3 dir = v3_sub(f.look, f.pos);
    f.pos = v3_add(f.pos, v3_scale(dir, fminf(.12f, S.t * S.drift * .03f)));
  }
  if (S.fresh) { S.pos = f.pos; S.look = f.look; S.fresh = false; }
  float rate = S.s.type == SH_FIXED ? 50 : 6;
  S.pos = v3_(ds_damp(S.pos.x, f.pos.x, rate, dt), ds_damp(S.pos.y, f.pos.y, rate, dt), ds_damp(S.pos.z, f.pos.z, rate, dt));
  S.look = v3_(ds_damp(S.look.x, f.look.x, rate * 1.2f, dt), ds_damp(S.look.y, f.look.y, rate * 1.2f, dt), ds_damp(S.look.z, f.look.z, rate * 1.2f, dt));
  float hh = S.handheld;
  v3 pos = v3_add(S.pos, v3_scale(v3_(nz(.7f, 1.9f, 1), nz(.9f, 2.3f, 2) * .6f, nz(.6f, 1.7f, 3)), .012f * hh));
  v3 look = v3_add(S.look, v3_scale(v3_(nz(.5f, 1.3f, 4), nz(.8f, 1.1f, 5), nz(.4f, 1.5f, 6)), .02f * hh));
  /* the boom: pull in toward what it looks at rather than sit in a wall */
  if (S.s.type != SH_FIXED) {
    v3 d = v3_sub(pos, look); float len = v3_len(d); d = v3_norm(d);
    float t; v3 n;
    if (world_raycast(look, d, len + .15f, &t, &n)) pos = v3_add(look, v3_scale(d, fmaxf(.45f, t - .22f)));
  }
  t3_object_set_position(cam, pos.x, pos.y, pos.z);
  t3_object_look_at(cam, look.x, look.y, look.z);
  if (f.roll != 0) { /* a dutch tilt: roll about the view axis */
    t3_quat q = ((t3_object *)cam)->quaternion; float s = sinf(f.roll * .5f), c = cosf(f.roll * .5f);
    t3_quat o = { q.x * c + q.y * 0 - q.z * 0 + q.w * 0 + q.y * s, q.y * c - q.x * s, q.z * c + q.w * s, q.w * c - q.z * s };
    t3_object_set_quaternion(cam, o);
  }
  if (cam->fov != f.fov) { cam->fov = f.fov; t3_camera_update_projection_matrix(cam); }
  last_pos = pos; last_look = look; have_last = true;
}
