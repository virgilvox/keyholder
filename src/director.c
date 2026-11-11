/* director.c: see director.h. A shot is solved at the cut, then re-scored every few frames while it
 * runs: when people walk into the lens, a wall comes between, the face leaves the frame or the lens
 * ends up too close, it re-solves and eases there (or cuts, when the move would be long or blocked). */
#include "director.h"
#include "world.h"
#include <string.h>

static t3_camera *cam;
typedef struct { v3 pos, look; float fov, roll; } frame_t;
typedef struct { float ang, dk, hk, base; int side; } cand_t;   /* base: the subject's yaw, kept from the solve */
typedef struct {
  dir_spec s;
  cand_t c;
  float t, handheld, drift;
  v3 pos, look; float fov;  /* damped */
  bool fresh;
  float check_t, since, score0; int bad;   /* the live re-score */
  v3 e_pos, e_look; float e_fov, e_t, e_len; bool whip;   /* an ease or a whip from the old framing */
  dir_spec asked; bool retry;   /* a solve that fell back tries the asked shot once more as poses settle */
} shot_t;
static shot_t S;
static bool active;
static float T;
static int line_side; static actor_t *line_a, *line_b;
static v3 last_pos, last_look; static float last_fov = 40;
static bool have_last;
static sh_type last_type = SH_AUTO;
static actor_t *const *OCC; static int NOCC;
static int mood;
/* bones are posed by actor_update: someone placed this frame still has last frame's (or no) pose,
 * so a bone far from the body falls back to where it would stand */
static bool stale_bone(actor_t *a, v3 b) {
  v3 p = actor_pos(a); float dx = b.x - p.x, dz = b.z - p.z;
  return dx * dx + dz * dz > 1.5f * 1.5f || b.y < p.y - .2f || b.y > p.y + 3;
}
static v3 head(actor_t *a) {
  v3 h = actor_head_pos(a);
  if (stale_bone(a, h)) { h = actor_pos(a); h.y += actor_height(a) * .9f; }
  return h;
}
static v3 bone(actor_t *a, const char *name, float up) {
  v3 b = actor_bone_pos(a, name);
  if (stale_bone(a, b)) { b = actor_pos(a); b.y += up; }
  return b;
}
/* how fast people move, from their eyes frame to frame: a walking close up leads its subject */
#define NVEL 24
static struct { actor_t *a; v3 p, v; } VEL[NVEL];
static v3 vel_of(actor_t *a) { for (int i = 0; i < NVEL; i++) if (VEL[i].a == a) return VEL[i].v; return v3_(0, 0, 0); }
static void track(float dt) {
  if (dt <= 0) return;
  for (int i = 0; i < NOCC && i < NVEL; i++) {
    actor_t *a = OCC[i]; v3 p = head(a);
    if (VEL[i].a != a) { VEL[i].a = a; VEL[i].p = p; VEL[i].v = v3_(0, 0, 0); continue; }
    v3 v = v3_scale(v3_sub(p, VEL[i].p), 1 / dt); v.y = 0;
    if (v3_len(v) > 8) v = v3_(0, 0, 0);   /* a teleport */
    VEL[i].v = v3_lerp(VEL[i].v, v, 1 - expf(-6 * dt)); VEL[i].p = p;
  }
}
/* handheld, lens (fov scale), lines a shot holds, dutch roll on close ups */
static const struct { float hh, lens; int hold; float roll; } MOOD[DM_COUNT] = {
  { 1, 1, 1, 0 }, { 1.9f, .9f, 0, .06f }, { 1.1f, 1.08f, 0, 0 }, { .55f, .9f, 2, 0 } };
void director_set_mood(int m) { mood = m >= 0 && m < DM_COUNT ? m : DM_CALM; }
int director_mood(void) { return mood; }
int director_hold_lines(void) { return MOOD[mood].hold; }
void director_set_occluders(actor_t *const *list, int n) { OCC = list; NOCC = n; }
/* the shortest distance from point p to segment a-b */
static float seg_dist(v3 p, v3 a, v3 b) { v3 ab = v3_sub(b, a); float t = ds_clamp(v3_dot(v3_sub(p, a), ab) / fmaxf(1e-5f, v3_dot(ab, ab)), 0, 1); return v3_len(v3_sub(p, v3_add(a, v3_scale(ab, t)))); }
static float smooth(float k) { k = ds_clamp(k, 0, 1); return k * k * (3 - 2 * k); }

static v3 eye(actor_t *a) { v3 h = head(a); h.y += .05f; return h; }
static v3 chest(actor_t *a) { v3 h = head(a); h.y -= .42f; return h; }
/* where a face points, from the eyes (a kneeling or turned head is not the body's yaw) */
static v3 face_dir(actor_t *a) {
  v3 re = actor_bone_pos(a, "rightEye"), le = actor_bone_pos(a, "leftEye");
  v3 r = stale_bone(a, re) ? v3_(0, 0, 0) : v3_sub(re, le); r.y = 0;
  if (v3_len(r) < .01f) { float y = actor_yaw(a); return v3_(sinf(y), 0, cosf(y)); }
  r = v3_norm(r); return v3_(r.z, 0, -r.x);
}
static v3 center(const dir_spec *s) {
  v3 c = v3_(0, 0, 0);
  for (int i = 0; i < s->nsubj; i++) c = v3_add(c, actor_pos(s->subj[i]));
  return s->nsubj ? v3_scale(c, 1.f / s->nsubj) : c;
}
static bool close_type(sh_type t) { return t == SH_MCU || t == SH_CU || t == SH_LOW || t == SH_DUTCH || t == SH_PUSH || t == SH_REACT || t == SH_HERO; }
/* the point an insert looks at */
static v3 insert_pt(const dir_spec *s) {
  if (s->part == PART_POINT || !s->nsubj) return s->fpos;
  if (s->part == PART_HEAD) return eye(s->subj[0]);
  return bone(s->subj[0], s->part == PART_LHAND ? "leftHand" : "rightHand", 1);
}
/* what the shot is about: the face or point that must stay in frame and in clear view */
static v3 key_of(const dir_spec *s) {
  if (s->type == SH_INSERT) return insert_pt(s);
  if (s->type == SH_OTS && s->nsubj > 1) return eye(s->subj[1]);
  if (s->type == SH_TOP) { v3 c = center(s); c.y += .5f; return c; }
  if (s->type == SH_WIDE || s->type == SH_ESTABLISH || s->type == SH_CRANE) { v3 c = center(s); c.y += 1.2f; return c; }
  return s->nsubj ? eye(s->subj[0]) : s->fpos;
}
/* the yaw a close up hangs from: toward who they face, or where they face */
static float face_yaw(const dir_spec *s) {
  actor_t *A = s->subj[0], *B = s->nsubj > 1 ? s->subj[1] : NULL;
  v3 fd = face_dir(A);
  /* toward who they talk to, unless they face away (walking off, turned): then where the face points */
  if (B) { v3 d = v3_sub(actor_pos(B), actor_pos(A)); d.y = 0;
    if (d.x * d.x + d.z * d.z > .01f && v3_dot(v3_norm(d), fd) > .2f) return atan2f(d.x, d.z); }
  return atan2f(fd.x, fd.z);
}
static frame_t frame_of(const dir_spec *s, const cand_t *c, float t) {
  frame_t f = { v3_(0, 0, 0), v3_(0, 0, 0), 40, 0 };
  float ang = c->ang, dk = c->dk, hk = c->hk; int side = c->side;
  actor_t *A = s->subj[0], *B = s->nsubj > 1 ? s->subj[1] : NULL;
  switch (s->type) {
    case SH_FIXED: f.pos = s->fpos; f.look = s->flook; f.fov = s->fov > 0 ? s->fov : 40; break;
    case SH_WIDE: case SH_ESTABLISH: {
      v3 ce = center(s);
      bool e = s->type == SH_ESTABLISH;
      float d = (s->dist > 0 ? s->dist : (e ? 9 : 4.6f)) * dk, h = (s->height > 0 ? s->height : (e ? 3.2f : 1.75f)) + hk;
      f.pos = v3_(ce.x + sinf(ang) * d, ce.y + h, ce.z + cosf(ang) * d);
      f.look = v3_(ce.x, ce.y + (e ? 1.1f : 1.15f), ce.z);
      f.fov = s->fov > 0 ? s->fov : (e ? 50 : 40);
      break;
    }
    case SH_TWO: {
      v3 a = actor_pos(A), b = B ? actor_pos(B) : a;
      float ly = atan2f(b.x - a.x, b.z - a.z), yaw = ly + side * DS_PI / 2 + ang;
      v3 ce = v3_scale(v3_add(a, b), .5f);
      float dd = ((s->dist > 0 ? s->dist : 2.5f) + v3_len(v3_sub(b, a)) * .8f) * dk;
      f.pos = v3_(ce.x + sinf(yaw) * dd, ce.y + 1.38f + hk, ce.z + cosf(yaw) * dd);
      f.look = v3_(ce.x, ce.y + 1.2f, ce.z);
      f.fov = s->fov > 0 ? s->fov : 34;
      break;
    }
    case SH_OTS: {
      /* hk widens the shoulder offset: tall hair needs more room */
      v3 a = actor_pos(A), b = B ? actor_pos(B) : v3_add(a, v3_(0, 0, 1));
      v3 dir = v3_sub(b, a); dir.y = 0; dir = v3_norm(dir);
      v3 right = v3_(dir.z, 0, -dir.x), ea = eye(A), eb = B ? eye(B) : b;
      f.pos = v3_add(v3_add(ea, v3_scale(dir, -dk * .95f)), v3_scale(right, side * (.62f + hk)));
      f.pos.y = ea.y + .02f - hk * .3f;
      f.look = v3_add(eb, v3_scale(right, side * .2f)); f.look.y -= .08f;
      f.fov = s->fov > 0 ? s->fov : 27;
      break;
    }
    case SH_MCU: case SH_CU: case SH_LOW: case SH_DUTCH: case SH_REACT: case SH_PUSH: {
      float near = s->dist > 0 ? s->dist : s->type == SH_CU ? 1.25f : s->type == SH_REACT ? 1.35f : s->type == SH_LOW ? 2.0f : 1.5f;
      if (s->type == SH_PUSH) {
        float d0 = s->dist > 0 ? s->dist : 2.7f, k = smooth(t / (s->dur > 0 ? s->dur : 5));
        near = ds_lerp(d0, 1.2f, k * k * .3f + k * .7f);
      }
      float fy = c->base, y = fy + ang;
      v3 p = actor_pos(A), e = eye(A);
      float h = (s->type == SH_LOW ? p.y + .62f : e.y - .05f) + hk;
      p.x = e.x; p.z = e.z;   /* hang from the face: a kneeling or leaning head is not over the feet */
      f.pos = v3_(p.x + sinf(y) * near * dk, h, p.z + cosf(y) * near * dk);
      v3 lead = v3_(sinf(fy), 0, cosf(fy));
      bool tight = s->type == SH_CU || s->type == SH_REACT;
      f.look = v3_add(e, v3_scale(lead, tight ? .04f : .1f));
      f.look.y = e.y - (tight ? .03f : s->type == SH_LOW ? .12f : .14f);
      f.fov = s->fov > 0 ? s->fov : (s->type == SH_CU ? 24 : s->type == SH_REACT ? 25 : s->type == SH_LOW ? 36 : s->type == SH_PUSH ? 31 : 30);
      f.roll = s->type == SH_DUTCH ? .16f : (s->type == SH_CU || s->type == SH_MCU) ? MOOD[mood].roll * (ang < 0 ? -1 : 1) : 0;
      break;
    }
    case SH_HERO: {
      /* low and wide, looking up past the chest */
      v3 p = actor_pos(A), e = eye(A); float y = c->base + ang, d = (s->dist > 0 ? s->dist : 2.3f) * dk;
      p.x = e.x; p.z = e.z;
      f.pos = v3_(p.x + sinf(y) * d, p.y + (s->height > 0 ? s->height : .32f) + hk, p.z + cosf(y) * d);
      f.look = v3_(p.x, e.y - .3f, p.z);
      f.fov = s->fov > 0 ? s->fov : 50;
      break;
    }
    case SH_INSERT: {
      /* from the front side, a little above, close on the hands or the prop */
      v3 q = insert_pt(s); float y = c->base + ang, d = (s->dist > 0 ? s->dist : .85f) * dk;
      f.pos = v3_(q.x + sinf(y) * d, q.y + .45f + hk, q.z + cosf(y) * d);
      f.look = q;
      f.fov = s->fov > 0 ? s->fov : 32;
      break;
    }
    case SH_TOP: {
      /* nearly straight down; a small offset keeps look_at away from the pole */
      v3 ce = center(s); float spread = 0;
      for (int i = 0; i < s->nsubj; i++) spread = fmaxf(spread, v3_len(v3_sub(actor_pos(s->subj[i]), ce)));
      float h = fmaxf(s->height > 0 ? s->height : 5.5f, spread * 1.2f + 2.4f) * dk, off = .35f + hk;
      f.pos = v3_(ce.x + sinf(ang) * off, ce.y + h, ce.z + cosf(ang) * off);
      f.look = v3_(ce.x, ce.y + .5f, ce.z);
      f.fov = s->fov > 0 ? s->fov : 54 + fmaxf(0, 1 - dk) * 40;   /* under a ceiling the lens goes wide */
      break;
    }
    case SH_ORBIT: {
      v3 ce = center(s); float d = (s->dist > 0 ? s->dist : 2.6f) * dk, y = ang + t * .22f;
      f.pos = v3_(ce.x + sinf(y) * d, ce.y + 1.5f, ce.z + cosf(y) * d);
      f.look = v3_(ce.x, ce.y + 1.2f, ce.z);
      f.fov = s->fov > 0 ? s->fov : 36;
      break;
    }
    case SH_CRANE: {
      v3 ce = center(s);
      float k = ds_clamp(t / (s->dur > 0 ? s->dur : 5), 0, 1), e = k * k * (3 - 2 * k), d0 = (s->dist > 0 ? s->dist : 4) * dk;
      float h = ds_lerp(s->height > 0 ? s->height : .7f, s->height > 0 ? s->height + 3 : 4.2f, e), d = ds_lerp(d0 * .7f, d0 * 1.4f, e);
      f.pos = v3_(ce.x + sinf(ang) * d, ce.y + h, ce.z + cosf(ang) * d);
      f.look = v3_(ce.x, ce.y + ds_lerp(1.3f, .9f, e), ce.z);
      f.fov = s->fov > 0 ? s->fov : 42;
      break;
    }
    case SH_TRACK: {
      v3 ce = center(s); float d = (s->dist > 0 ? s->dist : 3.2f) * dk, hy = actor_yaw(A), y = hy + ang;
      f.pos = v3_(ce.x + sinf(y) * d, ce.y + 1.45f, ce.z + cosf(y) * d);
      f.look = v3_(ce.x + sinf(hy) * .6f, ce.y + 1.25f, ce.z + cosf(hy) * .6f);
      f.fov = s->fov > 0 ? s->fov : 38;
      break;
    }
    default: break;
  }
  if (s->type != SH_FIXED && !(s->fov > 0)) f.fov *= MOOD[mood].lens;
  return f;
}
static bool near_wall(v3 p, float r) {
  static const v3 D[6] = {{1,0,0},{-1,0,0},{0,0,1},{0,0,-1},{0,1,0},{0,-1,0}};
  for (int i = 0; i < 6; i++) { float t; v3 n; if (world_raycast(p, D[i], r, &t, &n)) return true; }
  return false;
}
/* the lens stands where its people are: in the lab with someone in the lab, outside with someone
 * outside. A lens on the far side of the storefront or a wall sees the building, not the scene
 * (a group split across the door may be framed from either side) */
static bool same_space(const dir_spec *s, v3 pos) {
  if (world_get_stage() != STAGE_TOWN) return true;
  int nin = 0, nout = 0;
  if (s->nsubj) for (int i = 0; i < s->nsubj; i++) { if (world_indoors(actor_pos(s->subj[i]))) nin++; else nout++; }
  else { if (world_indoors(s->type == SH_INSERT ? s->fpos : s->flook)) nin++; else nout++; }
  return world_indoors(pos) ? nin > 0 : nout > 0;
}
static float vis_score(const dir_spec *s, v3 pos) {
  float got = 0, n = 0;
  if (s->type == SH_INSERT) {
    v3 q = insert_pt(s);
    n = 2; got = world_visible(pos, q) + world_visible(pos, v3_(q.x, q.y + .08f, q.z));
    return got / n;
  }
  /* an over the shoulder only needs the face it looks at; a close up only its subject */
  int i0 = s->type == SH_OTS && s->nsubj > 1 ? 1 : 0, i1 = close_type(s->type) || s->type == SH_OTS ? i0 + 1 : s->nsubj;
  for (int i = i0; i < i1 && i < s->nsubj; i++) {
    v3 pts[2] = { eye(s->subj[i]), chest(s->subj[i]) };
    for (int k = 0; k < 2; k++) { n += 1; if (world_visible(pos, pts[k])) got += 1; }
  }
  return n > 0 ? got / n : 1;
}
/* the nearest a lens may sit to the face (or point) it frames */
static float min_dist(sh_type t) {
  switch (t) {
    case SH_CU: case SH_REACT: return .85f;
    case SH_MCU: case SH_DUTCH: case SH_PUSH: return 1.f;
    case SH_OTS: return 1.25f;
    case SH_INSERT: return .4f;
    case SH_LOW: case SH_HERO: return 1.3f;
    default: return 1.4f;
  }
}
/* how good a framing is. fail is set when it must not be used (or must change now, when live) */
static float rate(const dir_spec *s, frame_t f, bool *fail) {
  *fail = false;
  if (near_wall(f.pos, .2f)) { *fail = true; return -100; }
  if (!same_space(s, f.pos)) { *fail = true; return -100; }
  float v = vis_score(s, f.pos);
  float sc = v * 3 - (v < .6f ? 2 : 0);
  if (v < .5f) *fail = true;
  v3 key = key_of(s);
  /* other people: nobody's hair fills the frame. The shoulder in an over the shoulder may sit in
   * the corner of the shot, never across the face we are looking at */
  bool closeup = close_type(s->type);
  v3 fw = v3_norm(v3_sub(f.look, f.pos)), rt = v3_norm(v3_cross(fw, fabsf(fw.y) > .98f ? v3_(0, 0, 1) : v3_(0, 1, 0))), up = v3_cross(rt, fw);
  float th = tanf(f.fov * DS_PI / 360) * .75f, kz = v3_dot(v3_sub(key, f.pos), fw);
  for (int i = 0; i < NOCC; i++) {
    actor_t *o = OCC[i];
    if (!actor_is_visible(o)) continue;
    bool is_subject = false, is_shoulder = s->type == SH_OTS && o == s->subj[0];
    /* a close up has one subject: its second is only who they are facing, and can be in the way */
    for (int k = 0; k < (closeup || s->type == SH_INSERT ? 1 : s->nsubj); k++) if (s->subj[k] == o) is_subject = true;
    if (s->type == SH_INSERT && s->part == PART_POINT) is_subject = false;   /* a prop: everyone is in the way */
    v3 h = head(o), hair = v3_(h.x, h.y + .12f, h.z), c2 = v3_(h.x, h.y - .5f, h.z), c3 = actor_pos(o);
    c3.y += .15f;   /* down to the feet: a low lens sees legs */
    float near_cam = fminf(seg_dist(f.pos, hair, c3), v3_len(v3_sub(f.pos, h)));
    if (s->type == SH_INSERT && is_subject) {  /* the hands' owner may be close, not in the lens */
      if (near_cam < .3f) { *fail = true; return -100; }
      continue;
    }
    if (near_cam < (is_shoulder ? .4f : .65f)) { if (!is_subject || is_shoulder) { *fail = true; return -100; } }
    if (is_subject && !is_shoulder) {
      /* a group shot: nobody in it fills the lens, or shows it the back of their head */
      if (!closeup && s->type != SH_INSERT && s->type != SH_TOP) {
        if (near_cam < 1.5f) sc -= (1.5f - near_cam) * 3;
        v3 fd = face_dir(o), tc = v3_sub(f.pos, h); tc.y = 0; tc = v3_norm(tc);
        if (v3_dot(fd, tc) < -.3f) sc -= near_cam < 2.5f ? 1.5f : .5f;
      }
      continue;
    }
    float blk = fminf(fminf(seg_dist(h, f.pos, key), seg_dist(hair, f.pos, key)), seg_dist(c2, f.pos, key));
    /* tall hair and shoulders: keep the face we look at well clear */
    if (blk < (is_shoulder ? .3f : .34f)) { sc -= is_shoulder ? 2.5f : 2.2f; if (!is_shoulder && blk < .22f) *fail = true; }
    else if (blk < .5f && !is_shoulder) sc -= .4f;
    if (is_shoulder) continue;
    /* who a close up faces may sit at the edge of it (a dirty single); only the face line above is theirs to keep clear */
    if ((closeup || s->type == SH_OTS) && s->nsubj > 1 && o == s->subj[1]) continue;
    /* a body between the lens and the subject, anywhere in the frame: legs past a low lens, a passer by */
    int fg = 0; float zn = 1e9f;
    v3 pts[4] = { hair, c2, v3_(h.x, h.y - .9f, h.z), v3_(c3.x, c3.y + .3f, c3.z) };
    for (int k = 0; k < 4; k++) {
      v3 d = v3_sub(pts[k], f.pos); float z = v3_dot(d, fw);
      if (z < .15f || z > kz - .3f) continue;
      float sx = v3_dot(d, rt) / z / (th / .75f * DS_W / DS_H), sy = v3_dot(d, up) / z / (th / .75f);
      if (fabsf(sx) < 1.05f && fabsf(sy) < 1.05f) { fg++; zn = fminf(zn, z); }
    }
    if (fg) {
      sc -= fg * .5f + (zn < 1.6f ? 1.5f : 0) + (zn < 1.2f ? 2 : 0);
      if (fg >= 2 && ((zn < 1.2f && (closeup || s->type == SH_INSERT || s->type == SH_OTS)) || zn < .9f)) *fail = true;
    }
  }
  /* a close up: the subject's own hands (and what they hold) stay off the face */
  if ((closeup || s->type == SH_OTS) && s->nsubj) {
    actor_t *A = s->type == SH_OTS && s->nsubj > 1 ? s->subj[1] : s->subj[0];
    /* the line to the face, stopping short of it: a hand at the chest under the chin is fine */
    v3 kd = v3_sub(key, f.pos); float kl = v3_len(kd);
    v3 k2 = v3_add(f.pos, v3_scale(kd, fmaxf(0, kl - .3f) / fmaxf(.01f, kl)));
    for (int k = 0; k < 2; k++) {
      v3 hp = bone(A, k ? "leftHand" : "rightHand", 1);
      /* the hand, and a blade held up from it */
      float dd = fminf(seg_dist(hp, f.pos, k2), fminf(seg_dist(v3_(hp.x, hp.y + .45f, hp.z), f.pos, k2), seg_dist(v3_(hp.x, hp.y + .85f, hp.z), f.pos, k2)));
      if (dd < .22f) sc -= 3.5f; else if (dd < .34f) sc -= 1.2f;
    }
  }
  /* a group shot keeps everyone in it */
  if ((s->type == SH_TWO || s->type == SH_WIDE || s->type == SH_TOP || s->type == SH_ESTABLISH) && s->nsubj > 1) {
    float tn = tanf(f.fov * DS_PI / 360);
    for (int i = 0; i < s->nsubj; i++) {
      v3 d = v3_sub(chest(s->subj[i]), f.pos); float z = v3_dot(d, fw);
      if (z < .3f) { *fail = true; break; }
      float sx = v3_dot(d, rt) / z / (tn * DS_W / DS_H), sy = v3_dot(d, up) / z / tn;
      if (fabsf(sx) > .95f || fabsf(sy) > .95f) { sc -= 2; if (fabsf(sx) > 1.1f || fabsf(sy) > 1.1f) *fail = true; }
    }
  }
  /* a close up sees a face, not the back of a head */
  if (closeup && s->nsubj) {
    v3 fd = face_dir(s->subj[0]), e = eye(s->subj[0]), tc = v3_sub(f.pos, e); tc.y = 0; tc = v3_norm(tc);
    float k = v3_dot(fd, tc);
    float kmin = s->type == SH_HERO || s->type == SH_LOW ? -.05f : .3f;   /* a close up needs the face, not a profile into hair */
    if (k < kmin) { *fail = true; sc -= 3; } else if (k < .5f) sc -= (.5f - k) * 3;
  }
  /* the line from what it looks at back to the lens: a wall there means the boom would pull in */
  { v3 d = v3_sub(f.pos, f.look); float len = v3_len(d), t; v3 n;
    /* a hit right at the look point means the point sits in a pillar or a prop: the eye checks cover that */
    if (len > .1f && world_raycast(f.look, d, len, &t, &n) && t < len - .05f && t > .45f) { sc -= 3; if (t < len * .8f) *fail = true; } }
  /* furniture in front of the lens: rays across the frame, as far as the subject */
  {
    float reach = fminf(5, fmaxf(.5f, v3_dot(v3_sub(key, f.pos), fw) - .35f));
    int hits = 0;
    for (int i = 0; i < 9; i++) {
      float kx = (i % 3 - 1) * 2.1f, ky = i < 3 ? -1.3f : i < 6 ? 0 : 1.f;   /* low corners reach the bottom edge, where chairs sit */
      v3 d = v3_add(fw, v3_add(v3_scale(rt, kx * th), v3_scale(up, ky * th)));
      float t; v3 n;
      if (world_raycast(f.pos, d, reach * v3_len(d), &t, &n) && n.y < .7f) { hits++;   /* the floor is not in the way */ sc -= i == 4 ? 1.5f : t < 1 ? .7f : .4f; }
    }
    if (hits >= 4 && (closeup || s->type == SH_OTS || s->type == SH_INSERT)) *fail = true;
  }
  {
    /* the face is in the frame, not too close, not cut off at the eyes */
    v3 d = v3_sub(key, f.pos); float z = v3_dot(d, fw);
    if (z < min_dist(s->type) * .85f) { *fail = true; sc -= 3; }
    else if (z < min_dist(s->type)) sc -= 1;
    if (z > .1f) {
      float sx = v3_dot(d, rt) / z / (th / .75f * DS_W / DS_H), sy = v3_dot(d, up) / z / (th / .75f);
      if (fabsf(sx) > .85f || sy > .8f || sy < -.85f) { *fail = true; sc -= 3; }
      else if (fabsf(sx) > .6f || sy > .6f) sc -= .5f;
    } else *fail = true;
  }
  return sc;
}
/* moving shots must be clean along the move, not only at its start */
static float rate_shot(const dir_spec *s, const cand_t *c, float t, bool *fail) {
  float sc = rate(s, frame_of(s, c, t), fail);
  if (*fail) return sc;
  float te = s->type == SH_PUSH || s->type == SH_CRANE ? (s->dur > 0 ? s->dur : 5) : s->type == SH_ORBIT ? t + 4 : -1;
  if (te > t) { bool f2; float s2 = rate(s, frame_of(s, c, te), &f2); if (f2) { *fail = true; return s2; } sc = fminf(sc, s2); }
  return sc;
}
static float fb_best; static cand_t fb_c;   /* the least bad of the failures, for when nothing passes */
static void try_cand(const dir_spec *s, cand_t c, float prefer, float t, const v3 *near_pos, float *best, cand_t *bc) {
  bool fail;
  float sc = rate_shot(s, &c, t, &fail);
  if (fail) { if (sc > -100 && sc + prefer > fb_best) { fb_best = sc + prefer; fb_c = c; } return; }
  sc += prefer;
  frame_t f = frame_of(s, &c, t);
  if (s->nsubj >= 2 && s->type != SH_INSERT) {
    v3 a = actor_pos(s->subj[0]), b = actor_pos(s->subj[1]);
    int sd = (b.x - a.x) * (f.pos.z - a.z) - (b.z - a.z) * (f.pos.x - a.x) > 0 ? 1 : -1;
    bool same = (line_a == s->subj[0] && line_b == s->subj[1]);
    bool flip = (line_a == s->subj[1] && line_b == s->subj[0]);
    if ((same && sd != line_side) || (flip && sd == line_side)) sc -= .9f;
  }
  if (near_pos) sc -= fminf(3, v3_len(v3_sub(*near_pos, f.pos))) * .35f;   /* a re-solve: stay near */
  else if (have_last) {
    v3 d1 = v3_norm(v3_sub(last_look, last_pos)), d2 = v3_norm(v3_sub(f.look, f.pos));
    float a = acosf(ds_clamp(v3_dot(d1, d2), -1, 1));
    if (a < .35f && v3_len(v3_sub(last_pos, f.pos)) < 1.5f) sc -= .6f;
  }
  sc += ds_rand() * .05f;
  if (sc > *best) { *best = sc; *bc = c; }
}
static cand_t C(float ang, float dk, float hk, int side, float base) { cand_t c = { ang, dk, hk, base, side }; return c; }
/* search the candidates of a shot type; near_pos is the live camera on a re-solve */
static float solve(dir_spec *s, cand_t *bc, float t, const v3 *near_pos) {
  float best = -1e9f;
  *bc = C(0, 1, 0, 1, 0);
  float fy = s->nsubj ? face_yaw(s) : 0;
  switch (s->type) {
    case SH_FIXED: return 0;
    case SH_WIDE: case SH_ESTABLISH:
      for (int r = 0; r < 24; r++) { float y = r * DS_PI / 12;
        try_cand(s, C(y, 1, 0, 1, 0), 0, t, near_pos, &best, bc);
        try_cand(s, C(y, .75f, 0, 1, 0), -.1f, t, near_pos, &best, bc);
        try_cand(s, C(y, .55f, .2f, 1, 0), -.25f, t, near_pos, &best, bc);
        try_cand(s, C(y, 1, .9f, 1, 0), -.15f, t, near_pos, &best, bc); }
      break;
    case SH_TWO: {
      static const float off[7] = {-.9f, -.6f, -.3f, 0, .3f, .6f, .9f}, dks[3] = {1, .75f, 1.3f};
      for (int d = 0; d < 3; d++) for (int o = 0; o < 7; o++) for (int sd = -1; sd <= 1; sd += 2)
        try_cand(s, C(off[o], dks[d], 0, sd, 0), -fabsf(off[o]) * .15f - fabsf(dks[d] - 1) * .3f, t, near_pos, &best, bc);
      break;
    }
    case SH_OTS: {
      static const float hks[3] = {0, .18f, .34f};
      for (int sd = -1; sd <= 1; sd += 2) for (int h = 0; h < 3; h++) {
        try_cand(s, C(0, 1, hks[h], sd, 0), -h * .2f, t, near_pos, &best, bc);
        try_cand(s, C(0, 1.35f, hks[h], sd, 0), -.05f - h * .2f, t, near_pos, &best, bc);
      }
      break;
    }
    case SH_MCU: case SH_CU: case SH_LOW: case SH_DUTCH: case SH_REACT: case SH_PUSH: case SH_HERO: {
      static const float angs[11] = {-.75f, -.5f, -.25f, 0, .25f, .5f, .75f, -1.05f, 1.05f, -1.4f, 1.4f}, dks[3] = {1, .8f, 1.3f};
      /* a reaction is seen from the speaker's side; a hero shot from the front */
      /* a lens over the furniture: a little higher, looking down a touch */
      int na = s->type == SH_REACT ? 7 : 11;
      for (int h = 0; h < 2; h++) for (int d = 0; d < 3; d++) for (int a = 0; a < na; a++)
        try_cand(s, C(angs[a], dks[d], h * .32f, 1, fy), -fabsf(angs[a]) * .25f + (fabsf(angs[a]) > .1f ? .05f : 0) - fabsf(dks[d] - 1) * .4f - h * .5f, t, near_pos, &best, bc);
      break;
    }
    case SH_INSERT: {
      float by = s->nsubj ? actor_yaw(s->subj[0]) : 0;
      if (s->part == PART_POINT && s->nsubj) { v3 d = v3_sub(actor_pos(s->subj[0]), s->fpos); by = atan2f(d.x, d.z); }
      static const float angs[8] = {.45f, -.45f, .8f, -.8f, 0, 1.2f, -1.2f, DS_PI};
      for (int a = 0; a < 8; a++) for (int d = 0; d < 2; d++)
        try_cand(s, C(angs[a], d ? 1.3f : 1, d ? .15f : 0, 1, by), -fabsf(angs[a]) * .12f - d * .15f, t, near_pos, &best, bc);
      break;
    }
    case SH_TOP:
      /* under a ceiling it comes down and leans out into a steep high angle */
      for (int r = 0; r < 8; r++) for (int d = 0; d < 6; d++) {
        static const float hs[6] = {1, .75f, .58f, .47f, .47f, .47f}, os[6] = {0, 0, .5f, .9f, 1.7f, 2.6f};
        try_cand(s, C(r * DS_PI / 4, hs[d], os[d], 1, 0), -d * .2f, t, near_pos, &best, bc);
      }
      break;
    case SH_ORBIT: case SH_CRANE:
      for (int r = 0; r < 12; r++) try_cand(s, C(r * DS_PI / 6, 1, 0, 1, 0), 0, t, near_pos, &best, bc);
      break;
    case SH_TRACK:
      /* beside, or a little ahead, so the face leads into the frame */
      for (int k = 0; k < 6; k++) { static const float a[6] = {.75f, -.75f, 1.1f, -1.1f, DS_PI / 2, -DS_PI / 2};
        try_cand(s, C(a[k], 1, 0, 1, 0), k < 2 ? .15f : k < 4 ? .1f : 0, t, near_pos, &best, bc); }
      break;
    default: break;
  }
  return best;
}
/* a solve that always lands somewhere: a tight corner steps back to a wide, then to anything */
static float solve_any(dir_spec *s, cand_t *bc, float t, const v3 *near_pos) {
  fb_best = -1e9f;
  dir_spec s0 = *s;
  float best = solve(s, bc, t, near_pos);
  float fb0 = fb_best; cand_t fc0 = fb_c;
  /* a weak solve: a plainer shot of the same people wins if it is clearly cleaner */
  if (best > -50 && best < .3f && s->type != SH_FIXED && s->type != SH_WIDE && s->type != SH_ESTABLISH) {
    sh_type alt[3]; int na = 0;
    if (close_type(s->type) && s->type != SH_MCU) alt[na++] = SH_MCU;
    if (s->nsubj >= 2 && s->type != SH_TWO && s->type != SH_INSERT) alt[na++] = SH_TWO;
    alt[na++] = SH_WIDE;
    for (int i = 0; i < na; i++) {
      dir_spec m = *s; m.type = alt[i]; m.fov = 0;
      if (alt[i] == SH_WIDE) { m.dist = 3.6f; m.height = 1.6f; } else if (alt[i] == SH_MCU) m.dist = 0;
      cand_t c2; float b2 = solve(&m, &c2, t, near_pos);
      if (b2 > best + 1.5f) { best = b2; *bc = c2; *s = m; }
    }
    return best;
  }
  if (best < -50 && s->type != SH_FIXED) {
    /* a close up that found nothing tries a wider lens on the same face first */
    if (close_type(s->type) && s->type != SH_MCU) { dir_spec m = *s; m.type = SH_MCU; m.dist = 0; best = solve(&m, bc, t, near_pos); if (best > -50) { *s = m; return best; } }
    dir_spec w = *s; w.type = SH_WIDE; w.dist = 3.6f; w.height = 1.6f; w.fov = 0;
    best = solve(&w, bc, t, near_pos);
    if (best < -50) { w.dist = 2.4f; best = solve(&w, bc, t, near_pos); }
    *s = w;
    if (best < -50) {
      /* nothing passes: the least bad framing of what was asked, else of the wide */
      if (fb0 > -50) { *s = s0; *bc = fc0; best = fb0 - 5; }
      else if (fb_best > -50) { *bc = fb_c; best = fb_best - 5; }
      else *bc = C(0, 1, 0, 1, 0);
    }
  }
  return best;
}
static void note_line(void) {
  if (S.s.nsubj >= 2 && S.s.type != SH_INSERT) {
    frame_t f = frame_of(&S.s, &S.c, S.t);
    v3 a = actor_pos(S.s.subj[0]), b = actor_pos(S.s.subj[1]);
    line_a = S.s.subj[0]; line_b = S.s.subj[1];
    line_side = (b.x - a.x) * (f.pos.z - a.z) - (b.z - a.z) * (f.pos.x - a.x) > 0 ? 1 : -1;
  }
}
void director_init(t3_camera *c) { cam = c; }
bool director_active(void) { return active; }
void director_stop(void) { active = false; S.whip = false; }
/* coverage for a line when the script does not ask: by mood, never the same type three times running */
static sh_type pick(bool listener) {
  static const struct { sh_type t; int w[DM_COUNT]; } P[] = {
    { SH_OTS,   { 40, 34, 15, 25 } }, { SH_MCU, { 25, 18, 25, 22 } }, { SH_TWO, { 25, 6, 35, 6 } },
    { SH_REACT, { 10, 12, 25, 12 } }, { SH_CU,  { 0, 22, 0, 22 } },   { SH_LOW, { 0, 8, 0, 0 } },
    { SH_PUSH,  { 0, 0, 0, 13 } } };
  if (!listener) return mood == DM_TENSE || mood == DM_EMOTIONAL ? (ds_rand() < .5f ? SH_CU : SH_MCU) : SH_MCU;
  for (int tries = 0; tries < 3; tries++) {
    int tot = 0; for (unsigned i = 0; i < sizeof P / sizeof *P; i++) tot += P[i].w[mood];
    int r = (int)(ds_rand() * tot);
    for (unsigned i = 0; i < sizeof P / sizeof *P; i++) { r -= P[i].w[mood]; if (r < 0) { if (P[i].t != last_type || tries == 2) return P[i].t; break; } }
  }
  return SH_MCU;
}
void director_cut(const dir_spec *in) {
  dir_spec s = *in;
  bool whip = s.type == SH_WHIP && have_last && active;
  if (s.type == SH_AUTO) {
    /* subj[0] speaks, subj[1] listens */
    sh_type t = pick(s.nsubj > 1);
    if (t == SH_OTS) { actor_t *a = s.subj[0]; s.subj[0] = s.subj[1]; s.subj[1] = a; }   /* over the listener, on the speaker */
    if (t == SH_REACT) { actor_t *a = s.subj[0]; s.subj[0] = s.subj[1]; s.subj[1] = a; }  /* on the listener */
    s.type = t;
  }
  /* a whip lands on a medium (one face) or a two shot, then plays as that */
  if (s.type == SH_WHIP) { s.type = s.nsubj == 2 ? SH_TWO : s.nsubj > 2 ? SH_WIDE : SH_MCU; if (s.type == SH_MCU && !(s.dist > 0)) s.dist = 0; }
  /* an insert on a hand hanging at the hip would frame the hips: it needs a hand raised to work height */
  if (s.type == SH_INSERT && s.nsubj && s.part != PART_POINT && s.part != PART_HEAD) {
    v3 q = insert_pt(&s);
    if (q.y - actor_pos(s.subj[0]).y < .8f * (eye(s.subj[0]).y - actor_pos(s.subj[0]).y)) { s.type = SH_MCU; s.nsubj = 1; s.dist = 0; s.fov = 0; }
  }
  /* a close up on someone walking hangs back: hair streams and the lens has to keep up */
  if (close_type(s.type) && s.nsubj && v3_len(vel_of(s.subj[0])) > .6f && !(s.dist > 0)) {
    if (s.type == SH_PUSH || s.type == SH_HERO) s.dist = 2.8f;
    else { s.type = SH_TRACK; s.nsubj = 1; }   /* a walking close up becomes a tracking medium beside them */
  }
  cand_t bc;
  dir_spec asked = s;
  float sc = solve_any(&s, &bc, 0, NULL);
  S.asked = asked; S.retry = s.type != asked.type;
  if (s.type == SH_MCU && in->type == SH_WHIP) bc.dk = fmaxf(bc.dk, 1.15f);
  S.s = s; S.c = bc; S.t = 0; S.fresh = true; S.check_t = 0; S.since = 0; S.bad = 0; S.score0 = sc;
  S.handheld = s.type == SH_TRACK ? .6f : (s.type == SH_MCU || s.type == SH_OTS || s.type == SH_REACT) ? .35f : s.type == SH_PUSH ? .15f : .2f;
  S.drift = (s.type == SH_FIXED || s.type == SH_ORBIT || s.type == SH_CRANE || s.type == SH_PUSH || s.type == SH_TOP || s.type == SH_INSERT) ? 0 : .06f;
  S.whip = whip; S.e_t = 0; S.e_len = 0;
  if (whip) { S.e_pos = last_pos; S.e_look = last_look; S.e_fov = last_fov; S.e_len = .42f; }
  active = true;
#ifdef KH_DIRLOG
  { v3 fd = s.nsubj ? face_dir(s.subj[0]) : v3_(0, 0, 0); float ay = s.nsubj ? actor_yaw(s.subj[0]) : 0;
    ds_log("[dir] cut %d->%d sc=%.2f dk=%.2f ang=%.2f face=%.2f yaw=%.2f", in->type, s.type, sc, bc.dk, bc.ang, atan2f(fd.x, fd.z), ay);
    frame_t f = frame_of(&s, &bc, 0); ds_log("[dir]   cam %.2f %.2f %.2f look %.2f %.2f %.2f", f.pos.x, f.pos.y, f.pos.z, f.look.x, f.look.y, f.look.z);
    for (int i = 0; i < NOCC; i++) if (actor_is_visible(OCC[i])) { v3 h = head(OCC[i]); ds_log("[dir]   occ %d head %.2f %.2f %.2f d=%.2f", i, h.x, h.y, h.z, v3_len(v3_sub(h, f.pos))); } }
#endif
  last_type = in->type == SH_AUTO ? s.type : in->type;
  note_line();
}
float director_whip(void) {
  if (!active || !S.whip || S.e_len <= 0 || S.e_t >= S.e_len) return 0;
  return sinf(DS_PI * ds_clamp(S.e_t / S.e_len, 0, 1));
}
/* the live check: re-score what the lens sees now; re-solve near here when it fails */
static void recheck(float dt) {
  if (S.s.type == SH_FIXED || S.whip) return;
  S.since += dt; S.check_t += dt;
  if (S.retry && S.t > .12f) {
    S.retry = false;
    dir_spec s = S.asked; cand_t bc;
    float nsc = solve(&s, &bc, S.t, NULL);
    if (nsc > fmaxf(.5f, S.score0 - 1)) {
#ifdef KH_DIRLOG
      ds_log("[dir] retry %d sc=%.2f", s.type, nsc);
#endif
      S.s = s; S.c = bc; S.fresh = true; S.score0 = nsc; S.since = 0; S.bad = 0; note_line(); return;
    }
  }
  if (S.check_t < .1f) return;
  S.check_t = 0;
  frame_t live = { S.pos, S.look, S.fov, 0 };
  bool fail; float sc = rate(&S.s, live, &fail);
  bool soft = sc < S.score0 - 2.5f && sc < 1.5f;
  S.bad = (fail || soft) ? S.bad + (fail ? 2 : 1) : 0;
  /* hold a fresh solve a moment, unless the lens is in someone */
  if (S.bad < 4 || (S.since < .9f && !(fail && sc <= -100))) return;
  dir_spec s = S.s; cand_t bc; v3 here = S.pos;
  float nsc = solve_any(&s, &bc, S.t, &here);
  frame_t f = frame_of(&s, &bc, S.t);
  /* a short move with a clear path eases; a long or blocked one cuts */
  bool path = v3_len(v3_sub(f.pos, here)) < 1.6f && world_visible(here, f.pos) && world_indoors(here) == world_indoors(f.pos);   /* never sweep through the door: cut */
  for (int i = 0; path && i < NOCC; i++) {
    if (!actor_is_visible(OCC[i])) continue;
    v3 h = head(OCC[i]);
    if (seg_dist(h, here, f.pos) < .45f || seg_dist(v3_(h.x, h.y - .6f, h.z), here, f.pos) < .45f) path = false;
  }
#ifdef KH_DIRLOG
  ds_log("[dir] resolve type %d sc=%.2f fail=%d -> %d sc=%.2f %s", S.s.type, sc, fail, s.type, nsc, path && sc > -100 ? "ease" : "cut");
#endif
  S.s = s; S.c = bc; S.since = 0; S.bad = 0; S.score0 = nsc;
  if (path && sc > -100) { S.e_pos = S.pos; S.e_look = S.look; S.e_fov = S.fov; S.e_t = 0; S.e_len = .65f; }
  else { S.fresh = true; S.e_len = 0; }
  note_line();
}
static float nz(float a, float b, float c) { return sinf(T * a + c) * .5f + sinf(T * b + c * 1.7f) * .5f; }
/* the yaw and pitch of a direction, for a whip that pans rather than slides */
static void ypr(v3 d, float *y, float *p) { d = v3_norm(d); *y = atan2f(d.x, d.z); *p = asinf(ds_clamp(d.y, -1, 1)); }
void director_update(float dt) {
  if (!active || !cam) return;
  T += dt; S.t += dt;
  track(dt);
  recheck(dt);
  frame_t f = frame_of(&S.s, &S.c, S.t);
  /* the lab's interior and lights follow who the shot is about, not the lens */
  world_subject(S.s.nsubj ? center(&S.s) : S.s.type == SH_INSERT ? S.s.fpos : f.look);
  /* lead a moving subject by the rig's lag, so the lens keeps its distance */
  if (S.s.type != SH_FIXED && S.s.type != SH_TOP && S.s.nsubj) {
    v3 v = vel_of(S.s.subj[0]);
    if (S.s.nsubj > 1 && !close_type(S.s.type)) v = v3_scale(v3_add(v, vel_of(S.s.subj[1])), .5f);
    f.pos = v3_add(f.pos, v3_scale(v, .2f)); f.look = v3_add(f.look, v3_scale(v, .14f));
  }
  if (S.drift > 0) {
    v3 dir = v3_sub(f.look, f.pos);
    f.pos = v3_add(f.pos, v3_scale(dir, fminf(.12f, S.t * S.drift * .03f)));
  }
  if (S.fresh) { S.pos = f.pos; S.look = f.look; S.fov = f.fov; S.fresh = false; }
  if (S.e_len > 0 && S.e_t < S.e_len) {
    S.e_t += dt;
    float k = ds_clamp(S.e_t / S.e_len, 0, 1);
    if (S.whip) {
      /* a whip: the head pans fast through the old and new directions, the dolly follows */
      float ks = k < .5f ? 4 * k * k * k : 1 - powf(-2 * k + 2, 3) / 2;
      S.pos = v3_lerp(S.e_pos, f.pos, ks);
      float y0, p0, y1, p1; ypr(v3_sub(S.e_look, S.e_pos), &y0, &p0); ypr(v3_sub(f.look, f.pos), &y1, &p1);
      float y = y0 + ds_wrap_pi(y1 - y0) * ks, p = ds_lerp(p0, p1, ks);
      float dl = ds_lerp(v3_len(v3_sub(S.e_look, S.e_pos)), v3_len(v3_sub(f.look, f.pos)), ks);
      S.look = v3_add(S.pos, v3_scale(v3_(sinf(y) * cosf(p), sinf(p), cosf(y) * cosf(p)), dl));
      S.fov = ds_lerp(S.e_fov, f.fov, ks) + sinf(DS_PI * k) * 9;   /* the lens breathes out with the speed */
    } else {
      float ks = smooth(k);
      S.pos = v3_lerp(S.e_pos, f.pos, ks); S.look = v3_lerp(S.e_look, f.look, ks); S.fov = ds_lerp(S.e_fov, f.fov, ks);
    }
    if (S.e_t >= S.e_len) S.whip = false;
  } else {
    float rate = S.s.type == SH_FIXED ? 50 : S.s.type == SH_INSERT ? 8 : 5;
    S.pos = v3_(ds_damp(S.pos.x, f.pos.x, rate, dt), ds_damp(S.pos.y, f.pos.y, rate, dt), ds_damp(S.pos.z, f.pos.z, rate, dt));
    S.look = v3_(ds_damp(S.look.x, f.look.x, rate * 1.4f, dt), ds_damp(S.look.y, f.look.y, rate * 1.4f, dt), ds_damp(S.look.z, f.look.z, rate * 1.4f, dt));
    S.fov = ds_damp(S.fov, f.fov, 6, dt);
  }
  float hh = S.handheld * MOOD[mood].hh;
  v3 pos = v3_add(S.pos, v3_scale(v3_(nz(.7f, 1.9f, 1), nz(.9f, 2.3f, 2) * .6f, nz(.6f, 1.7f, 3)), .012f * hh));
  v3 look = v3_add(S.look, v3_scale(v3_(nz(.5f, 1.3f, 4), nz(.8f, 1.1f, 5), nz(.4f, 1.5f, 6)), .02f * hh));
  /* the boom: the last guard against a wall between the lens and what it looks at. The solver keeps
   * this rare; a pull of more than a little asks the live check for a new angle */
  if (S.s.type != SH_FIXED) {
    /* a sphere over the near plane's corners, not a ray: a ray lets the frame's edges into the wall */
    v3 d = v3_sub(pos, look); float len = v3_len(d); d = v3_norm(d);
    float t = world_cam_clear(look, pos, .14f);
    if (t < len - 1e-3f && t > .45f) {
      pos = v3_add(look, v3_scale(d, t));
      if (t < len * .85f) S.bad += 2;
    }
  }
  t3_object_set_position(cam, pos.x, pos.y, pos.z);
  t3_object_look_at(cam, look.x, look.y, look.z);
  if (f.roll != 0) { /* a dutch tilt: roll about the view axis */
    t3_quat q = ((t3_object *)cam)->quaternion; float s = sinf(f.roll * .5f), c = cosf(f.roll * .5f);
    t3_quat o = { q.x * c + q.y * s, q.y * c - q.x * s, q.z * c + q.w * s, q.w * c - q.z * s };
    t3_object_set_quaternion(cam, o);
  }
  if (cam->fov != S.fov) { cam->fov = S.fov; t3_camera_update_projection_matrix(cam); }
  last_pos = pos; last_look = look; last_fov = S.fov; have_last = true;
}
