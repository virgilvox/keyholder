/* story.c: see story.h. */
#include "story.h"
#include "script.h"
#include "player.h"
#include "voice.h"
#include "game.h"
#include "battle.h"
#include "weapon.h"
#include "bike.h"
#include "fx.h"
#include "street.h"   /* street */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

story_ui_t STORY_UI;
static t3_scene *scene; static t3_camera *cam;
static int me_id = W_VEE, m1_id = W_DEX, m2_id = W_RUTH;

/* a walk along a path: waypoints, then the destination */
#define WPN 24
typedef struct {
  bool on; v3 dest; float dest_yaw; bool has_yaw; float speed, t, tmax;
  v3 wp[WPN]; int nwp, iwp;
  float stuck_t; v3 stuck_p; int stuck_n;
} walk_t;
typedef struct {
  actor_t *a;
  walk_t w;
  float turn_to; bool turning;
  const char *base;       /* the clip it rests in */
  float kneel;            /* kneel_hold's phase */
  actor_expr ex; float ex_t;
  int weapon;
  bool in_fight;
} slot_t;
static slot_t A[WHO_COUNT];

static int pc, call_ret = -1;
static enum { B_NONE, B_WAIT, B_SAY, B_CHOICE, B_WALK, B_ROAM, B_FIGHT, B_CARD, B_EPILOGUE, B_DONE } block;
static float wait_t, say_t, say_len;
static int say_who;
static bool flags[FLAG_COUNT];
static unsigned weapons = 1;
static float hour = 9, hour_target = 9;
static char line_buf[512], name_buf[64], obj_buf[160];
static int choice_first;
/* roam */
static const sc_beat *roam;
static v3 roam_pos; static bool roam_has_place; static bool side_done[8]; static int side_near = -1;
static int roam_follow = -1; static float roam_t, roam_h0, follow_path_t;
static bool attic_want, attic_warned, attic_done;   /* v1.4: the attic minigame off the back stairs */
static uint16_t attic_found_m, attic_games_m;        /* v1.5: hub progress (persisted in save_t) */
/* fight */
static int fight_flags, fight_id;
static const sc_beat *fight_beat;
/* coverage */
static int last_speaker = -1, lines_in_shot; static bool shot_held;
static bool skip_scene;
static float fade_to, fade_rate = 2, flash_rate = 2, dim_to;
/* banter and barks */
static char sub_buf[320], sub_name[64]; static int sub_who = -1; static float bark_t;
static bool banter_done[48];
static bool door_forced;
/* v1.3: a subtitle said during a roam (banter, a task's bark) is cleared when a chapter card comes up */
static bool sub_roam;
/* v1.3: the roam's goal once its work is done, shown as the objective; how long the work has been done */
static char goal_buf[160]; static float gate_t;
/* v1.3: flags the runtime keeps (the loaner bike) and the part being played (0 the day, n night n) */
static int FLG_BIKE = -1, part_now, epi_part;
static void props_hide(void);
static bool bike_scene; static int bike_with = -1; static float bike_wheel; static v3 bike_last;
static void bike_sync(void);
static struct { int task; v3 dest, via; v3 wp[24]; int n, k; float t, len; } WF = { -1 };   /* the wayfinder (wf_frame) */
static int flag_named(const char *n) { for (int i = 0; i < FLAG_COUNT; i++) if (!strcmp(FLAG_NAMES[i], n)) return i; return -1; }

static void walk_start(walk_t *w, actor_t *a, v3 dest, float yaw, bool has_yaw, float speed);
static bool walk_step(walk_t *w, actor_t *a, float dt);
static void extras_update(float dt);
static void extras_reset(void);
static void extras_make(void);
static void tasks_end(void);
static void sub_clear(void);

/* ---- who is who ---- */
static int canon(int w) {
  if (w == W_ME) return me_id;
  if (w == W_M1) return m1_id;
  if (w == W_M2) return m2_id;
  return w;
}
static const char *speaker_id(int w) { w = canon(w); return w == W_SHADOW ? WHO_ID[me_id] : WHO_ID[w]; }
static const char *display_name(int w) { w = canon(w); return w == W_SHADOW ? "You" : CAST[w].name; }
static uint32_t who_color(int w) { w = canon(w); return w == W_SHADOW ? 0x8878c8 : CAST[w].color; }
static actor_t *actor_of(int w) {
  w = canon(w);
  slot_t *s = &A[w];
  if (!s->a) {
    const sc_cast *c = &CAST[w];
    if (w == W_SHADOW) {
      const sc_cast *m = &CAST[me_id];
      actor_palette p = { {250, .35f, .28f}, {250, .4f, .26f}, {250, .4f, .22f}, {250, .4f, .2f} };
      s->a = actor_new(m->model, &p, m->scale, scene);
    } else s->a = actor_new(c->model < AM_COUNT ? c->model : AM_B, &c->pal, c->scale, scene);
    s->base = "Idle_Loop"; s->weapon = -1;
    actor_visible(s->a, false);
  }
  return s->a;
}
static void expand(char *o, size_t cap, const char *s) {
  size_t n = 0;
  for (const char *p = s; *p && n + 16 < cap;) {
    const char *r = NULL; int skip = 0;
    if (!strncmp(p, "{me}", 4)) { r = CAST[me_id].name; skip = 4; }
    else if (!strncmp(p, "{m1}", 4)) { r = CAST[m1_id].name; skip = 4; }
    else if (!strncmp(p, "{m2}", 4)) { r = CAST[m2_id].name; skip = 4; }
    if (r) { size_t l = strlen(r); memcpy(o + n, r, l); n += l; p += skip; }
    else o[n++] = *p++;
  }
  o[n] = 0;
}

/* ---- marks ---- */
/* the lab's named places, HSL-108 plan metres (x from the street, z from the west wall) and plan yaw
 * (0 faces plan +z, the east wall; 90 faces the back; 180 the west wall; -90 the street). Each stands
 * on clear floor in front of the thing and faces it. */
static const struct { const char *n; float x, z, yaw; } LAB_PLACES[] = {
  {"lab_welcome", 7.4f, 4.4f, 60}, {"lab_laser", 25.1f, 2.6f, 110}, {"lab_kitchen", 36.8f, 3.6f, -20},
  {"lab_cowork", 11.6f, 5.4f, 40}, {"lab_shop", 31.4f, 7.2f, -90},
  {"lab_entry", 6.25f, 3.45f, -90},       /* just inside the front door, facing it */
  {"lab_signin", 6.1f, 2.25f, 90},        /* the sign in kiosk's tablet, beside the welcome board */
  {"lab_printer", 9.3f, 1.3f, 180},       /* the 3D printer rack on the west wall */
  {"lab_laser_hood", 26.5f, 1.75f, 180},  /* the 80 W CO2 laser's lid */
  {"lab_soda", 13.9f, 1.45f, 180},        /* the soda fridge on the art and LEGO wall */
  {"lab_pegboard", 12.9f, 1.4f, 180},     /* the west wall pegboard over the LEGO drawers */
  {"lab_toolwall", 24.8f, 8.75f, 0},      /* the woodshop pegboard on the east wall, hand tools */
  {"lab_elec", 19.45f, 7.95f, 0},         /* the electronics bench: scope, supply, ESD mat */
  {"lab_lounge", 15.6f, 5.3f, 0},         /* the lounge floor in front of the sofas */
  {"lab_couch", 14.9f, 6.6f, 90},         /* a seat on the lounge sofa, facing the TV */
  {"lab_fab", 36.8f, 7.05f, 0},           /* the welding table in the metal fab bay */
  {"lab_scrap", 24.8f, 1.3f, 180},        /* the laser scrap shelves */
  {"lab_classroom", 10.0f, 8.3f, 0},      /* the wall whiteboard behind the coworking tables */
  {"lab_fridge", 38.95f, 1.3f, 75},       /* the kitchen refrigerator */
  {"lab_bike", 17.5f, 5.2f, 0},           /* the bike station bench */
  {"lab_teardown", 19.1f, 4.55f, 180},    /* the Tech Tear Apart bin: the short blue bin shelf at the end of member storage, facing the hall, across from electronics */
  {"lab_clamps", 24.2f, 8.7f, 0},         /* the woodshop pegboard where the clamps hang */
};
static bool lab_place(const char *n, v3 *p, float *y) {
  if (!n) return false;
  for (unsigned i = 0; i < sizeof LAB_PLACES / sizeof LAB_PLACES[0]; i++) if (!strcmp(LAB_PLACES[i].n, n)) {
    *p = world_lab(LAB_PLACES[i].x, LAB_PLACES[i].z); *y = world_lab_yaw(LAB_PLACES[i].yaw); return true;
  }
  return false;
}
static bool place_of(const char *n, v3 *p, float *y) {
  if (!n) return false;
  return lab_place(n, p, y) || world_place(n, p, y);
}
static bool nav_free(int ix, int iz);
static int nav_ix(float v);
static bool overhead_clear(v3 p);
/* the ground at a mark, never a table, bench or sofa top (the old probe from 1.2 m up took the highest
 * surface, and people stood on tables). A mark inside furniture moves to the nearest clear floor; a
 * sitting clip puts its sitter back on a real seat (sit_snap). */
static v3 on_floor(v3 p) {
  float base = p.y, g = world_floor_y(v3_(p.x, base + .35f, p.z));
  if (g < -1e8f) g = world_floor_y(v3_(p.x, base + 1.2f, p.z));
  if (g > -1e8f) p.y = g;
  /* a body at the mark: does the world push it out? (the arenas too: their counters and machines push) */
  v3 q = v3_(p.x, p.y, p.z), v = v3_(0, 0, 0); bool gr = true;
  world_move(&q, &v, .22f, 1.5f, 1 / 60.f, &gr);
  if (hypotf(q.x - p.x, q.z - p.z) < .04f && overhead_clear(p)) return p;
  int cx = nav_ix(p.x), cz = nav_ix(p.z); float bd = 1e9f; v3 best = p;
  for (int r = 1; r <= 5 && bd > 1e8f; r++) for (int dx = -r; dx <= r; dx++) for (int dz = -r; dz <= r; dz++) {
    if (abs(dx) != r && abs(dz) != r) continue;
    v3 c = v3_((cx + dx) * .3f, p.y, (cz + dz) * .3f);   /* NAV_R, the grid below */ float d = hypotf(c.x - p.x, c.z - p.z);
    if (d < bd && nav_free(cx + dx, cz + dz)) { bd = d; best = c; }
  }
  if (bd > 1e8f) return p;
  float f = world_floor_y(v3_(best.x, base + .35f, best.z)); if (f > -1e8f) best.y = f;
  return best;
}
static v3 resolve(const sc_mark *m, float *yaw, bool *has_yaw) {
  v3 p = v3_(0, 0, 0); float y = 0; bool hy = m->yaw > -9000;
  switch (m->kind) {
    case MK_LAB: p = world_lab(m->x, m->z); y = hy ? world_lab_yaw(m->yaw) : 0; break;
    case MK_PLACE: {
      v3 b; float by = 0;
      if (!place_of(m->name, &b, &by)) ds_log("story: no place %s", m->name ? m->name : "?");
      p = v3_add(b, v3_(m->x, 0, m->z));
      y = hy ? m->yaw * DS_PI / 180 : by; hy = true;
      break;
    }
    case MK_STAGE: { v3 c; float r; world_stage_floor(world_get_stage(), &c, &r); p = v3_add(c, v3_(m->x, 0, m->z)); y = hy ? m->yaw * DS_PI / 180 : 0; break; }
    default: break;
  }
  p = on_floor(p);
  if (yaw) *yaw = y;
  if (has_yaw) *has_yaw = hy;
  return p;
}
/* ---- the sit snap: a sitting clip sits on a real seat ---- */
/* The script asks for Sitting_* clips at marks that may have no chair. The nearest free seat of the lab
 * model within 1.5 m takes the sitter (the sofa and the lounge chair reach .9 m further: people flop
 * there), turned the way the seat faces. Nobody near a seat stands instead. Returns the clip to play. */
static const char *sit_snap(actor_t *a, const char *clip) {
  if (!a || !clip || strncmp(clip, "Sitting", 7) || world_get_stage() != STAGE_TOWN) return clip;
  const float SIT_H = .45f;   /* the clip's seat: its hips ride .55 m over the root, straight above it */
  v3 p = actor_pos(a), s, bp = p; float y, by = 0, best = 1.5f; bool soft, found = false;
  for (int i = 0; world_seat(i, &s, &y, &soft); i++) {
    float d = hypotf(s.x - p.x, s.z - p.z);
    if (d < .12f && fabsf(p.y - (s.y - SIT_H)) < .1f) return clip;   /* already on it */
    float sc = d - (soft ? .9f : 0);
    if (sc >= best) continue;
    bool taken = false;
    for (int j = 0; j < WHO_COUNT && !taken; j++) if (A[j].a && A[j].a != a && actor_is_visible(A[j].a) && hypotf(actor_pos(A[j].a).x - s.x, actor_pos(A[j].a).z - s.z) < .35f) taken = true;
    if (taken) continue;
    best = sc; bp = s; by = y; found = true;
  }
  if (!found) return strstr(clip, "Talking") ? "Idle_Talking_Loop" : "Idle_Loop";
  float g = world_floor_y(v3_(bp.x, .3f, bp.z));
  v3 at = v3_(bp.x - sinf(by) * .04f, fmaxf(bp.y - SIT_H, (g > -1e8f ? g : 0) - .05f), bp.z - cosf(by) * .04f);
  actor_place(a, at, by);
  actor_set_floor(a, g > -1e8f ? g : 0);
  return clip;
}
static void face_now(int who, int target) {
  actor_t *a = actor_of(who), *b = actor_of(target);
  v3 d = v3_sub(actor_pos(b), actor_pos(a));
  actor_move(a, actor_pos(a), atan2f(d.x, d.z));
  if (actor_is_visible(b)) actor_look(a, actor_head_pos(b), .9f);   /* turned to someone, you look at them, not at what you held before */
  if (canon(who) == me_id && !roam) player_teleport(actor_pos(a), atan2f(d.x, d.z));   /* and the player's own heading, in a scene */
}
static float flat_dist(v3 a, v3 b) { return hypotf(a.x - b.x, a.z - b.z); }
static v3 fwd_of(float yaw) { return v3_(sinf(yaw), 0, cosf(yaw)); }

/* ---- paths: a 0.3 m grid probed lazily with world_move, A* when the straight line is blocked,
 * then pulled tight so walkers cut corners the way people do ---- */
#define NAV_R .3f
#define NAV_HN 32768
static struct { int16_t x, z; uint8_t st; } NAV[NAV_HN];
static int nav_stage = -1;
static float nav_y;
/* v1.3: something low overhead (a table top, a bench, a shelf) means this is under furniture, not floor. The
 * world's push only moves a body sideways, and a thin top straight above it pushes it down, not out, so a mark
 * under a kitchen table passed as clear floor and people stood inside tables. */
static bool overhead_clear(v3 p) {
  if (world_get_stage() != STAGE_TOWN || !world_in_lab(p)) return true;
  float t; v3 n;
  return !world_raycast(v3_(p.x, p.y + .12f, p.z), v3_(0, 1, 0), 1.45f, &t, &n);
}
static bool nav_probe(int ix, int iz) {
  float x = ix * NAV_R, z = iz * NAV_R;
  if (world_get_stage() == STAGE_TOWN && fabsf(x) < 1.0f && fabsf(z + 5.45f) < .5f) return true;   /* the front door opens for walkers */
  v3 p = v3_(x, nav_y, z), v = v3_(0, 0, 0); bool g = true;
  world_move(&p, &v, .27f, 1.5f, 1 / 60.f, &g);
  if (hypotf(p.x - x, p.z - z) >= .05f) return false;
  return overhead_clear(v3_(x, nav_y, z));
}
static bool nav_free(int ix, int iz) {
  if ((int)world_get_stage() != nav_stage) {
    memset(NAV, 0, sizeof NAV); nav_stage = (int)world_get_stage();
    nav_y = 0;
    if (nav_stage != STAGE_TOWN) { v3 c; float r; world_stage_floor(world_get_stage(), &c, &r); nav_y = c.y; }
  }
  uint32_t h = ((uint32_t)ix * 73856093u ^ (uint32_t)iz * 19349663u) & (NAV_HN - 1);
  for (int k = 0; k < 64; k++) {
    uint32_t j = (h + (uint32_t)k) & (NAV_HN - 1);
    if (!NAV[j].st) { NAV[j].x = (int16_t)ix; NAV[j].z = (int16_t)iz; NAV[j].st = nav_probe(ix, iz) ? 1 : 2; return NAV[j].st == 1; }
    if (NAV[j].x == ix && NAV[j].z == iz) return NAV[j].st == 1;
  }
  return nav_probe(ix, iz);
}
static int nav_ix(float v) { return (int)lroundf(v / NAV_R); }
/* clear floor along a segment (its ends excepted: people stand at tables and on sofas) */
static bool nav_line(v3 a, v3 b) {
  float d = flat_dist(a, b);
  int n = (int)(d / (NAV_R * .5f)) + 1;
  for (int i = 0; i <= n; i++) {
    float t = (float)i / n;
    if (t * d < .4f || (1 - t) * d < .4f) continue;
    if (!nav_free(nav_ix(a.x + (b.x - a.x) * t), nav_ix(a.z + (b.z - a.z) * t))) return false;
  }
  return true;
}
#define AS_N 5000
typedef struct { int16_t x, z, par; bool closed; float g, f; } as_node;
static as_node AN[AS_N]; static int an_n;
static int16_t AH[16384];
static int HP[AS_N], hn;
static int as_get(int x, int z, bool make) {
  uint32_t h = ((uint32_t)x * 2654435761u ^ (uint32_t)z * 40503u) & 16383u;
  for (int k = 0; k < 16384; k++) {
    uint32_t j = (h + (uint32_t)k) & 16383u;
    if (!AH[j]) {
      if (!make || an_n >= AS_N) return -1;
      as_node *n = &AN[an_n]; n->x = (int16_t)x; n->z = (int16_t)z; n->par = -1; n->closed = false; n->g = 1e9f; n->f = 1e9f;
      AH[j] = (int16_t)(++an_n); return an_n - 1;
    }
    as_node *n = &AN[AH[j] - 1];
    if (n->x == x && n->z == z) return AH[j] - 1;
  }
  return -1;
}
static void hp_push(int i) {
  int k = hn++; HP[k] = i;
  while (k > 0) { int p = (k - 1) / 2; if (AN[HP[p]].f <= AN[HP[k]].f) break; int t = HP[p]; HP[p] = HP[k]; HP[k] = t; k = p; }
}
static int hp_pop(void) {
  int r = HP[0]; HP[0] = HP[--hn]; int k = 0;
  for (;;) {
    int l = k * 2 + 1, m = k;
    if (l < hn && AN[HP[l]].f < AN[HP[m]].f) m = l;
    if (l + 1 < hn && AN[HP[l + 1]].f < AN[HP[m]].f) m = l + 1;
    if (m == k) break;
    int t = HP[m]; HP[m] = HP[k]; HP[k] = t; k = m;
  }
  return r;
}
static float octile(int ax, int az, int bx, int bz) { float dx = fabsf((float)(ax - bx)), dz = fabsf((float)(az - bz)); return (dx + dz) + (1.4142f - 2) * fminf(dx, dz); }
/* waypoints from a to b around furniture; returns how many (0 when the straight line is clear) */
static int nav_path(v3 a, v3 b, v3 *out, int cap) {
  if (flat_dist(a, b) < .8f || nav_line(a, b)) return 0;
  memset(AH, 0, sizeof AH); an_n = 0; hn = 0;
  int sx = nav_ix(a.x), sz = nav_ix(a.z), gx = nav_ix(b.x), gz = nav_ix(b.z);
  int s = as_get(sx, sz, true); AN[s].g = 0; AN[s].f = octile(sx, sz, gx, gz); hp_push(s);
  int found = -1;
  while (hn) {
    int c = hp_pop(); as_node *n = &AN[c];
    if (n->closed) continue;
    n->closed = true;
    if (abs(n->x - gx) <= 2 && abs(n->z - gz) <= 2 && (abs(n->x - gx) + abs(n->z - gz) == 0 || nav_line(v3_(n->x * NAV_R, 0, n->z * NAV_R), b))) { found = c; break; }
    for (int d = 0; d < 8; d++) {
      static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1}, DZ[8] = {0, 0, 1, -1, 1, -1, 1, -1};
      int nx = n->x + DX[d], nz = n->z + DZ[d];
      bool goal = abs(nx - gx) + abs(nz - gz) == 0;
      if (!goal && !nav_free(nx, nz)) continue;
      if (d >= 4 && (!nav_free(n->x + DX[d], n->z) || !nav_free(n->x, n->z + DZ[d]))) continue;   /* no corner cutting */
      int k = as_get(nx, nz, true); if (k < 0) continue;
      n = &AN[c];
      float g = n->g + (d >= 4 ? 1.4142f : 1);
      if (g < AN[k].g && !AN[k].closed) { AN[k].g = g; AN[k].f = g + octile(nx, nz, gx, gz); AN[k].par = (int16_t)c; if (hn < AS_N) hp_push(k); }
    }
  }
  if (found < 0) return 0;
  /* the cells back from the goal, then pulled tight */
  static v3 cells[AS_N]; int nc = 0;
  for (int c = found; c >= 0 && nc < AS_N; c = AN[c].par) cells[nc++] = v3_(AN[c].x * NAV_R, a.y, AN[c].z * NAV_R);
  int n = 0; v3 anchor = a; int i = nc - 1;
  while (i > 0 && n < cap) {
    int j = i - 1;
    for (int k = 0; k < i; k++) if (nav_line(anchor, cells[k])) { j = k; break; }
    if (j == 0) break;
    out[n++] = cells[j]; anchor = cells[j]; i = j;
  }
  return n;
}

/* ---- walking ---- */
static void walk_start(walk_t *w, actor_t *a, v3 dest, float yaw, bool has_yaw, float speed) {
  w->on = true; w->dest = dest; w->dest_yaw = yaw; w->has_yaw = has_yaw; w->speed = speed; w->t = 0;
  w->nwp = nav_path(actor_pos(a), dest, w->wp, WPN); w->iwp = 0;
  float len = 0; v3 p = actor_pos(a);
  for (int i = 0; i < w->nwp; i++) { len += flat_dist(p, w->wp[i]); p = w->wp[i]; }
  len += flat_dist(p, dest);
  w->tmax = len / fmaxf(.3f, speed) + 8;
  w->stuck_t = 0; w->stuck_p = actor_pos(a); w->stuck_n = 0;
}
/* true when it has arrived (and stands at the destination) */
static bool walk_step(walk_t *w, actor_t *a, float dt) {
  if (!w->on) return true;
  v3 p = actor_pos(a);
  while (w->iwp < w->nwp && flat_dist(p, w->wp[w->iwp]) < .45f) w->iwp++;
  bool last = w->iwp >= w->nwp;
  v3 tgt = last ? w->dest : w->wp[w->iwp], d = v3_sub(tgt, p); d.y = 0;
  float l = v3_len(d);
  w->t += dt;
  if ((last && l < .12f) || w->t > w->tmax) {
    w->on = false;
    actor_place(a, w->dest, w->has_yaw ? w->dest_yaw : actor_yaw(a));
    return true;
  }
  /* stuck on something the grid did not know: go round again from here */
  w->stuck_t += dt;
  if (w->stuck_t > 1.1f) {
    if (flat_dist(p, w->stuck_p) < .2f * w->speed) {
      if (++w->stuck_n >= 4) { w->on = false; actor_place(a, w->dest, w->has_yaw ? w->dest_yaw : actor_yaw(a)); return true; }
      w->nwp = nav_path(p, w->dest, w->wp, WPN); w->iwp = 0;
    } else w->stuck_n = 0;
    w->stuck_t = 0; w->stuck_p = p;
  }
  float sp = w->speed * (last ? fminf(1, .35f + l) : 1);
  v3 dir = v3_scale(d, 1 / fmaxf(l, 1e-4f)), vel = v3_scale(dir, sp);
  bool g; vel.y = -2;
  world_move(&p, &vel, .25f, 1.5f, dt, &g);
  float y = actor_yaw(a), want = atan2f(dir.x, dir.z);
  y += ds_wrap_pi(want - y) * fminf(1, dt * 9);
  actor_move(a, p, y); actor_set_floor(a, p.y);
  return false;
}
static const char *walk_clip(float speed) { return speed > 2.4f ? "Sprint_Loop" : speed > 1.7f ? "Jog_Fwd_Loop" : "Walk_Loop"; }
/* someone keeping up with a moving point (the player, or a kid for the parent beside them) */
static void follow_to(actor_t *a, walk_t *w, v3 p, float keep, float *path_t, float dt) {
  v3 d = v3_sub(p, actor_pos(a)); d.y = 0;
  float l = v3_len(d);
  *path_t -= dt;
  if (l > keep + .7f && (!w->on || *path_t <= 0 || flat_dist(w->dest, p) > 1.5f)) {
    float sp = l > 6 ? 3.6f : l > 3.5f ? 2.2f : 1.4f;
    walk_start(w, a, v3_add(p, v3_scale(v3_norm(d), -keep)), 0, false, sp);
    actor_play(a, walk_clip(sp), 1, true, .2f);
    *path_t = .6f;
  }
  if (l > 40) { actor_place(a, v3_add(p, v3_(-1, 0, 1)), actor_yaw(a)); w->on = false; }
}
static void follow_player(actor_t *a, walk_t *w, float dt, float keep) { follow_to(a, w, player_pos(), keep, &follow_path_t, dt); }

/* ---- camera helpers ---- */
static int resolve_subj(const int *subj, actor_t **out) {
  int n = 0;
  for (int i = 0; i < 3; i++) if (subj[i] >= 0) out[n++] = actor_of(subj[i]);
  return n;
}
static bool task_talk(int *partner);
/* who a line is said to: in a hosting task's talk, the host and the task's person to each other; else the
 * nearest other visible actor */
static int listener_of(int who) {
  int w = canon(who), best = -1; float bd = 7;
  v3 p = actor_pos(actor_of(w));
  int me = canon(W_ME), tw;
  if (task_talk(&tw)) {
    int l = w == me ? tw : me;
    if (l >= 0 && l != w && A[l].a && actor_is_visible(A[l].a) && v3_len(v3_sub(actor_pos(A[l].a), p)) < bd) return l;
  }
  for (int i = 0; i < WHO_COUNT; i++) {
    if (i == w || !A[i].a || !actor_is_visible(A[i].a)) continue;
    float d = v3_len(v3_sub(actor_pos(A[i].a), p));
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}
static int extras_list(actor_t **L, int n);
static void occluders(void) {
  static actor_t *L[WHO_COUNT + 16]; int n = 0;
  for (int i = 0; i < WHO_COUNT; i++) if (A[i].a && actor_is_visible(A[i].a)) L[n++] = A[i].a;
  n = extras_list(L, n);   /* the people in the background walk into lenses too */
  director_set_occluders(L, n);
}
static void cut_to(sh_type t, int who, int other) {
  occluders();
  dir_spec s; memset(&s, 0, sizeof s);
  s.type = t; s.subj[0] = actor_of(who); s.nsubj = 1;
  if (other >= 0) { s.subj[1] = actor_of(other); s.nsubj = 2; }
  director_cut(&s);
}
static void auto_cover(int who, int to, sh_type asked) {
  int w = canon(who), l = to >= 0 ? canon(to) : listener_of(w);
  if (asked != SH_AUTO) {
    shot_held = false;
    /* over the listener's shoulder onto the speaker; a reaction is on the listener */
    if ((asked == SH_OTS || asked == SH_REACT) && l >= 0) cut_to(asked, l, w); else cut_to(asked, w, l);
    lines_in_shot = 0; last_speaker = w; return;
  }
  /* a SHOT the script asked for holds through the next line */
  if (shot_held) { shot_held = false; last_speaker = w; lines_in_shot = 0; return; }
  /* same speaker going on: hold a line or two (the mood decides), then change the angle */
  if (w == last_speaker && lines_in_shot < director_hold_lines()) { lines_in_shot++; return; }
  cut_to(SH_AUTO, w, l);   /* the director picks the coverage by mood */
  last_speaker = w; lines_in_shot = 0;
}

/* v1.2 roam gear (see the gear section by story_update): the bike, once found, rides with the save */
#define GEAR_BIKE 0x42
static bool gear_bike_have;
static void gear_use_first(const intent_t *in);
/* ---- saves: the day's save stays under byte 520 of DS_SAVE.data (the Battle Zone keeps the rest) ----
 * v1.3 (ver SAVE_V3) adds, after v1.2's bike byte: the chapter or night the save resumes at (a hash of its
 * title, so a later script still finds it), every flag as a bit, and the state each night began with (the
 * title's night select starts a night from it). A v1.2 save has none of that: it resumes at the chapter
 * whose hour it saved, with its first FLAG_V12 flags, which the compiler keeps in their v1.2 order. */
#define SAVE_V3 0x33
#define NIGHTS_MAX 3
typedef struct { uint8_t ok, me, weapons, bike; int8_t stats[ST_COUNT]; uint8_t fbits[32]; } carry_t;
typedef struct {
  uint32_t magic; int pc, me; unsigned weapons; float hour, x, y, z, yaw; int stats[ST_COUNT]; uint8_t flags[64]; char objective[160]; uint8_t bike;   /* bike: GEAR_BIKE once the bike is unlocked */
  uint8_t ver, ended, done;    /* done: bit 0 the day finished, bit n night n */
  uint32_t chap;
  uint8_t fbits[32];
  carry_t start[NIGHTS_MAX];
  /* v1.5: the attic hub, global across the day (NOT in carry_t). Appended after start[] so offsetof(bike)
   * stays 300 and save_v3's memset-from-ver zeroes them on migration. */
  uint16_t attic_found, attic_games;
  uint8_t attic_state;      /* bit0 the minigame has been beaten */
} save_t;
_Static_assert(sizeof(save_t) <= 520, "the day's save must stay under DS_SAVE.data[520]");
_Static_assert(FLAG_COUNT <= 256, "flags are saved as 256 bits");
_Static_assert(offsetof(save_t, bike) == 300, "v1.2 saves keep their layout");
static uint32_t title_hash(const char *t) { uint32_t h = 2166136261u; for (; t && *t; t++) { h ^= (uint8_t)*t; h *= 16777619u; } return h ? h : 1; }
static save_t *save_v3(void) {
  save_t *s = (save_t *)DS_SAVE.data;
  if (s->ver != SAVE_V3 || s->magic != DS_SAVE_MAGIC) {   /* over a v1.2 save, or nothing: the new fields start clean */
    memset(&s->ver, 0, sizeof *s - offsetof(save_t, ver)); s->ver = SAVE_V3;
  }
  return s;
}
static void bits_put(uint8_t *b) { memset(b, 0, 32); for (int i = 0; i < FLAG_COUNT; i++) if (flags[i]) b[i >> 3] |= (uint8_t)(1u << (i & 7)); }
static void bits_get(const uint8_t *b) { for (int i = 0; i < FLAG_COUNT; i++) flags[i] = (b[i >> 3] >> (i & 7)) & 1; }
static uint32_t save_chap;   /* the title hash of the chapter or night the save resumes at */
/* a night replayed from the title's night select: it saves only once it is further along than the save it
 * started over, so going back to night one never costs the night three you were in the middle of */
static bool replay;
static void autosave(void) {
  if (replay) {
    const save_t *o = (const save_t *)DS_SAVE.data;
    if (story_has_save() && o->ver == SAVE_V3 && o->pc > pc) return;
    replay = false;   /* past it now: an ordinary run that saves */
  }
  save_t *s = save_v3();
  s->magic = DS_SAVE_MAGIC; s->pc = pc; s->me = me_id; s->weapons = weapons; s->hour = hour_target;
  { actor_t *m = actor_of(W_ME); v3 p = actor_pos(m); s->x = p.x; s->y = p.y; s->z = p.z; s->yaw = actor_yaw(m); }
  memcpy(s->stats, STORY_UI.stats, sizeof s->stats);
  for (int i = 0; i < 64; i++) s->flags[i] = i < FLAG_COUNT && flags[i];
  bits_put(s->fbits);
  snprintf(s->objective, sizeof s->objective, "%s", obj_buf);
  s->bike = gear_bike_have ? GEAR_BIKE : 0;
  s->chap = save_chap;
  s->attic_state = attic_done ? 1 : 0; s->attic_found = attic_found_m; s->attic_games = attic_games_m;
  s->ended = pc >= SCRIPT_COUNT || SCRIPT[pc].op == OP_END;
  DS_SAVE.magic = DS_SAVE_MAGIC;
}
bool story_has_save(void) {
  const save_t *s = (const save_t *)DS_SAVE.data;
  return DS_SAVE.magic == DS_SAVE_MAGIC && s->magic == DS_SAVE_MAGIC && !(s->ver == SAVE_V3 && s->ended);
}
/* the nights the title can start: bit n-1 for night n, once the save holds the state it began with */
int story_nights_open(void) {
  const save_t *s = (const save_t *)DS_SAVE.data;
  if (DS_SAVE.magic != DS_SAVE_MAGIC || s->magic != DS_SAVE_MAGIC || s->ver != SAVE_V3) return 0;
  int m = 0; for (int n = 0; n < NIGHTS_MAX && n < NIGHT_COUNT; n++) if (s->start[n].ok) m |= 1 << n;
  return m;
}
int story_night_count(void) { return NIGHT_COUNT < NIGHTS_MAX ? NIGHT_COUNT : NIGHTS_MAX; }
const char *story_night_title(int n) {
  for (int i = 0; i < SCRIPT_COUNT; i++) if (SCRIPT[i].op == OP_NIGHT && SCRIPT[i].i == n) return SCRIPT[i].s;
  return NULL;
}
static void set_cast(int m) {
  me_id = m;
  m1_id = m == W_VEE ? W_DEX : W_VEE;
  m2_id = m == W_RUTH ? W_DEX : W_RUTH;
}
static void hide_all(void) { for (int i = 0; i < WHO_COUNT; i++) if (A[i].a) { actor_visible(A[i].a, false); A[i].w.on = false; } }
void story_init(t3_scene *s, t3_camera *c) { scene = s; cam = c; director_init(c); }
static void boot(void) {
  FLG_BIKE = flag_named("has_bike");
  bike_sync(); bike_with = -1; bike_scene = false; WF.task = -1; epi_part = part_now;
  props_hide();
  block = B_NONE; call_ret = -1; last_speaker = -1; skip_scene = false; roam = NULL;
  memset(side_done, 0, sizeof side_done);
  STORY_UI.fade = 1; fade_to = 1; dim_to = 0;
  world_set_stage(STAGE_TOWN);
  hide_all(); tasks_end(); sub_clear();
  player_set_actor(actor_of(W_ME));
  extras_make();
  street_init(scene, cam, occluders);   /* street */
}
#ifdef KH_PROBE
/* a debug map of the lab floor in plan metres: '#' a body is pushed, '^' something low overhead, '.' clear floor */
static void probe_map(float x0, float x1, float z0, float z1) {
  char row[200];
  for (float px = x0; px <= x1; px += .3f) {
    int n = 0;
    for (float pz = z0; pz <= z1 && n < 190; pz += .3f) {
      v3 w = world_lab(px, pz), q = w, v = v3_(0, 0, 0); bool g = true;
      world_move(&q, &v, .22f, 1.5f, 1 / 60.f, &g);
      row[n++] = hypotf(q.x - w.x, q.z - w.z) >= .04f ? '#' : !overhead_clear(w) ? '^' : '.';
    }
    row[n] = 0; ds_log("[probe] x %5.1f %s", px, row);
  }
}
#endif
void story_new_game(int p) {
#ifdef KH_PROBE
  ds_log("[probe] plan z from %.1f by .3 m", 0.f); probe_map(4.0f, 40.4f, 0.f, 9.4f);
#endif
  static const int PL[3] = { W_VEE, W_DEX, W_RUTH };
  memset(&STORY_UI, 0, sizeof STORY_UI); memset(flags, 0, sizeof flags);
  attic_done = attic_warned = false; attic_found_m = attic_games_m = 0;   /* v1.5: a new day, the attic is wrong again */
  street_reset(false);   /* street: a new day, nobody downtown has met you */
  set_cast(PL[p]);
  for (int i = 0; i < ST_COUNT; i++) STORY_UI.stats[i] = 0;
  STORY_UI.stats[ST_TRUST] = 5; STORY_UI.stats[ST_SAFETY] = 5; STORY_UI.stats[ST_VIBE] = 5; STORY_UI.stats[ST_DUES] = 5; STORY_UI.stats[ST_ENERGY] = 5;
  obj_buf[0] = 0; STORY_UI.objective = NULL;
  weapons = 0; hour = hour_target = 8.9f; world_set_time(hour);
  gear_bike_have = false;
  pc = 0; part_now = 0; save_chap = 0; replay = false;
  boot();
}
/* the beat a save resumes at: its chapter or night by title, else (v1.2) the day's chapter at its hour */
static bool first_chapter(int at) { for (int i = 0; i < at; i++) if (SCRIPT[i].op == OP_CHAPTER) return false; return true; }
static int resume_pc(const save_t *s) {
  if (s->ver == SAVE_V3 && s->chap) for (int i = 0; i < SCRIPT_COUNT; i++) {
    const sc_beat *b = &SCRIPT[i];
    /* a chapter card is its scene's first beat (day.mjs), except the day's first: the opening plays from the top */
    if ((b->op == OP_CHAPTER || b->op == OP_NIGHT) && title_hash(b->s) == s->chap) return b->op == OP_CHAPTER && first_chapter(i) ? 0 : i;
  }
  for (int i = 0; i < SCRIPT_COUNT && SCRIPT[i].op != OP_NIGHT; i++) if (SCRIPT[i].op == OP_CHAPTER && fabsf(SCRIPT[i].f - s->hour) < .02f) return i;
  return s->pc >= 0 && s->pc < SCRIPT_COUNT ? s->pc : 0;
}
static int part_at(int at) { int n = 0; for (int i = 0; i <= at && i < SCRIPT_COUNT; i++) if (SCRIPT[i].op == OP_NIGHT) n = SCRIPT[i].i; return n; }
void story_continue(void) {
  save_t *s = (save_t *)DS_SAVE.data;
  memset(&STORY_UI, 0, sizeof STORY_UI);
  set_cast(s->me); pc = resume_pc(s); weapons = s->weapons; hour = hour_target = s->hour;
  gear_bike_have = s->bike == GEAR_BIKE;   /* a save from before v1.2 holds no such byte */
  if (s->ver == SAVE_V3) { attic_done = (s->attic_state & 1) != 0; attic_found_m = s->attic_found; attic_games_m = s->attic_games; }
  memcpy(STORY_UI.stats, s->stats, sizeof s->stats);
  memset(flags, 0, sizeof flags);
  if (s->ver == SAVE_V3) bits_get(s->fbits);
  else for (int i = 0; i < FLAG_V12 && i < FLAG_COUNT; i++) flags[i] = s->flags[i] != 0;
  part_now = part_at(pc); save_chap = s->ver == SAVE_V3 ? s->chap : 0; replay = false;
  snprintf(obj_buf, sizeof obj_buf, "%s", s->objective);
  STORY_UI.objective = obj_buf[0] ? obj_buf : NULL;
  world_set_time(hour);
  boot();
  { actor_t *m = actor_of(W_ME); actor_visible(m, true); actor_place(m, v3_(s->x, s->y, s->z), s->yaw); }
  fade_to = 0; fade_rate = 1;
}
/* the title's night select: night n from the state it began with */
void story_start_night(int n) {
  save_t *s = (save_t *)DS_SAVE.data;
  if (n < 1 || n > NIGHTS_MAX || !(story_nights_open() & (1 << (n - 1)))) return;
  const carry_t *c = &s->start[n - 1];
  street_reset(false);   /* street: a night picked from the title starts with nobody met */
  int at = -1; for (int i = 0; i < SCRIPT_COUNT; i++) if (SCRIPT[i].op == OP_NIGHT && SCRIPT[i].i == n) { at = i; break; }
  if (at < 0) return;
  memset(&STORY_UI, 0, sizeof STORY_UI);
  set_cast(c->me); weapons = c->weapons; gear_bike_have = c->bike == GEAR_BIKE;
  for (int i = 0; i < ST_COUNT; i++) STORY_UI.stats[i] = c->stats[i];
  memset(flags, 0, sizeof flags); bits_get(c->fbits);
  obj_buf[0] = 0; STORY_UI.objective = NULL;
  hour = hour_target = SCRIPT[at].f; world_set_time(hour);
  pc = at; part_now = n; save_chap = 0; replay = true;
  boot();
}
/* the epilogue's screen is done: on into the next night, or false when the story is over */
bool story_resume(void) {
  if (block != B_EPILOGUE) return false;
  if (pc >= SCRIPT_COUNT || SCRIPT[pc].op == OP_END) { block = B_DONE; return false; }
  block = B_NONE; STORY_UI.fade = 1; fade_to = 1;
  hide_all(); director_stop(); sub_clear();
  return true;
}
void story_stop(void) { voice_stop(); props_hide(); bike_show(false); hide_all(); director_stop(); extras_reset(); tasks_end(); sub_clear(); }
story_run story_state(void) { return block == B_ROAM ? SR_ROAM : block == B_FIGHT ? SR_FIGHT : block == B_EPILOGUE ? SR_EPILOGUE : block == B_DONE ? SR_DONE : SR_RUNNING; }
bool story_take_attic(void) { bool w = attic_want; attic_want = false; return w; }
void story_attic_result(bool rebooted) {
  if (rebooted && !attic_done) {
    attic_done = true;
    STORY_UI.stats[ST_SAFETY]++;
    STORY_UI.toast = story_stat_name(ST_SAFETY); STORY_UI.toast_n = 1; STORY_UI.toast2 = NULL; STORY_UI.toast_t = 2.6f;
  }
}
bool story_attic_beaten(void) { return attic_done; }
void story_attic_masks(uint16_t *found, uint16_t *games) { if (found) *found = attic_found_m; if (games) *games = attic_games_m ? attic_games_m : 1; }
void story_attic_set_masks(uint16_t found, uint16_t games) { attic_found_m = found; attic_games_m = games; }
const char *story_player_name(void) { return CAST[me_id].name; }

/* the people a chapter needs, made while its card is up rather than at their first line */
static void preload_chapter(int from) {
  for (int i = from; i < SCRIPT_COUNT && i < from + 600; i++) {
    const sc_beat *b = &SCRIPT[i];
    if (i > from && b->op == OP_CHAPTER) break;
    switch (b->op) {
      case OP_PLACE: case OP_WALK: case OP_SAY: case OP_CLIP: if (b->who >= 0) actor_of(b->who); break;
      case OP_ROAM:
        for (int k = 0; k < b->i2; k++) actor_of(SIDES[b->i + k].who);
        for (int k = 0; k < b->nt; k++) if (TASKS[b->t0 + k].who >= 0) actor_of(TASKS[b->t0 + k].who);
        break;
      case OP_FIGHT: {
        const sc_fight *F = &FIGHT_TABLE[b->i];
        for (int k = 0; k < F->count; k++) if (!(F->foes[k].palette && !strcmp(F->foes[k].palette, "alt"))) actor_of(F->foes[k].palette && !strcmp(F->foes[k].palette, "shadow") ? W_SHADOW : F->foes[k].who);
        break;
      }
      default: break;
    }
  }
}

/* ---- subtitles: banter and barks ---- */
static void sub_clear(void) { STORY_UI.sub_t = 0; STORY_UI.sub_text = NULL; sub_who = -1; bark_t = 0; }
static float sub_say(int who, const char *text) {
  expand(sub_buf, sizeof sub_buf, text);
  snprintf(sub_name, sizeof sub_name, "%s", display_name(who));
  float len = 0;
  if (voice_say(speaker_id(who), sub_buf)) len = voice_seconds(speaker_id(who), sub_buf);
  if (len <= 0) len = fmaxf(1.6f, strlen(sub_buf) / 15.f);
  story_ui_t *U = &STORY_UI;
  U->sub_name = sub_name; U->sub_text = sub_buf; U->sub_col = who_color(who); U->sub_t = len + .5f; U->sub_dur = len + .5f;
  sub_who = canon(who);
  sub_roam = block == B_ROAM || call_ret >= 0;
  return len;
}

/* ---- fights ---- */
static fight_def FD;
static fight_bark FBK[10]; static char fbk_txt[10][256];
static weapon_id assist_weapon(int w) {
  if (w == W_VEE) return WP_SPEAR;     /* filament: she runs the printers */
  if (w == W_RUTH) return WP_STAFF;    /* the laser corner */
  if (w == W_DEX) return WP_MAUL;
  return WP_ARC;
}
static void begin_fight(const sc_beat *b, int start_phase) {
  const sc_fight *F = &FIGHT_TABLE[b->i];
  bike_init(scene); bike_show(false); bike_with = -1;   /* a twist with a bike brings fight.c's own */
  memset(&FD, 0, sizeof FD);
  FD.title = F->title; FD.sub = F->sub; FD.count = F->count; FD.radius = F->radius;
  v3 c; float yaw, r;
  if (!strcmp(F->where, "stage")) world_stage_floor(world_get_stage(), &c, &r);
  else if (!strcmp(F->where, "lounge")) c = world_lab(15.6f, 4.8f);
  else if (!place_of(F->where, &c, &yaw)) c = actor_pos(actor_of(W_ME));
  FD.center = c;
  for (int i = 0; i < F->count; i++) {
    const sc_foe *e = &F->foes[i];
    fighter_def *d = &FD.foes[i];
    bool shadow = e->palette && !strcmp(e->palette, "shadow");
    bool alt = e->palette && !strcmp(e->palette, "alt");
    int w = shadow ? W_SHADOW : canon(e->who);
    const sc_cast *cc = w == W_SHADOW ? &CAST[me_id] : &CAST[w];
    d->model = cc->model < AM_COUNT ? cc->model : AM_B; d->pal = cc->pal; d->scale = cc->scale;
    d->name = e->name ? e->name : cc->name;
    d->weapon = (weapon_id)e->weapon; d->ai = (ai_kind)e->ai;
    d->hp = e->hp; d->power = e->power; d->speed = e->speed; d->place_x = e->x; d->place_z = e->z;
    if (alt) { d->pal.top.hue += 40; d->pal.hair.hue += 40; d->actor = NULL; }
    else { d->actor = actor_of(w); actor_visible(d->actor, true); A[w].in_fight = true; }
  }
  /* the story's numbers lean on the fight */
  int rep = STORY_UI.stats[ST_TRUST] + STORY_UI.stats[ST_SAFETY] + STORY_UI.stats[ST_VIBE] - 15;
  FD.foe_scale = ds_clamp(1.0f - rep * .015f, .8f, 1.2f);
  FD.player_hp = ds_clamp(.7f + STORY_UI.stats[ST_ENERGY] * .06f, .6f, 1.0f);
  FD.weapons = weapons | 1u; FD.start_weapon = 0;
  for (int k = WP_COUNT - 1; k >= 0; k--) if (A[canon(W_ME)].weapon == k) FD.start_weapon = k;
  FD.tutorial = (b->i2 & 1) != 0;
  FD.easy = b->target >= 0 && flags[b->target];
  /* v1.1: the twist, its object, who to protect, barks and the allies the day has earned */
  FD.twist = F->twist; FD.twist_a = F->twist_a; FD.twist_b = F->twist_b;
  if (F->obj_x != 0 || F->obj_z != 0) FD.obj_pos = v3_add(c, v3_(F->obj_x, 0, F->obj_z));
  FD.obj_label = F->obj_label; FD.obj_kind = F->obj_kind;
  if (F->protect >= 0) { actor_t *pa = actor_of(F->protect); actor_visible(pa, true); A[canon(F->protect)].in_fight = true; FD.protect_actor = pa; }
  FD.nbarks = 0;
  for (int i = 0; i < F->nbark && i < 10; i++) {
    const sc_bark *k = &F->barks[i];
    fight_bark *o = &FBK[FD.nbarks++];
    expand(fbk_txt[i], sizeof fbk_txt[i], k->text);
    o->on = k->on; o->speaker = speaker_id(k->who); o->name = display_name(k->who); o->color = who_color(k->who); o->text = fbk_txt[i];
  }
  FD.barks = FBK;
  FD.nassist = 0;
  for (int i = 0; i < F->nassist && i < 3; i++) {
    const sc_assist *as = &F->assists[i];
    bool ok = (as->flag >= 0 && flags[as->flag]) || (as->trust > 0 && STORY_UI.stats[ST_TRUST] >= as->trust);
    if (!ok) continue;
    int w = canon(as->who);
    const sc_cast *cc = &CAST[w];
    fighter_def *d = &FD.assist[FD.nassist++];
    d->model = cc->model < AM_COUNT ? cc->model : AM_B; d->pal = cc->pal; d->scale = cc->scale; d->name = cc->name;
    d->weapon = assist_weapon(w); d->ai = AI_DUELIST; d->hp = 100; d->power = .8f; d->speed = 1;
    d->place_x = FD.nassist == 1 ? -2.5f : 2.5f; d->place_z = 3;
    d->actor = actor_of(w); A[w].in_fight = true;   /* fight.c drives it while it is on the field */
  }
  FD.phase2_at = F->phase2_at; FD.finisher_qte = F->finisher_qte; FD.start_phase = start_phase;
  fight_beat = b; fight_flags = b->i2; fight_id = b->i;
  STORY_UI.cine = false; STORY_UI.talking = false;
  actor_t *me = actor_of(W_ME); actor_visible(me, true); A[canon(W_ME)].in_fight = true;
  fight_start(&FD, me);
  block = B_FIGHT;
}
static void fight_won(void) {
  if (fight_beat->flag >= 0) flags[fight_beat->flag] = true;
  battle_unlock_fight(fight_id);
}

/* ---- hosting: the roam's tasks ---- */
#define TMAX 16
enum { TS_HIDDEN, TS_OPEN, TS_DONE };
typedef struct {
  int st; bool talked;
  v3 at, to; float at_yaw, to_yaw;
  int who, vis;           /* a cast actor, or one of the walk in visitors */
  int trips; bool holding;
  float win; bool arrived; int quality;
  int box; float look_t;
  float with_t;           /* the companion's path timer */
} task_t;
static task_t T[TMAX]; static int tn, tcur = -1;
static float me_kneel;   /* the player's kneel_hold phase, in a kneeling minigame */
/* someone resting kneeling at work or sitting (their base clip): a task keeps them down, they only look up */
static bool busy_pose(int who) {
  const char *b = who >= 0 ? A[canon(who)].base : NULL;   /* an extra (a walk in) rests standing */
  return b && (!strncmp(b, "Sitting", 7) || strstr(b, "Kneeling"));
}
static bool task_talk(int *partner) { *partner = tcur >= 0 && T[tcur].who >= 0 ? canon(T[tcur].who) : -1; return tcur >= 0; }
static enum { TP_NONE, TP_PRE, TP_PLAY, TP_POST } tphase;
/* walk in visitors for tasks with nobody named */
#define NVIS 3
static struct { actor_t *a; walk_t w; int task; } VIS[NVIS];
static const actor_palette VIS_PAL[NVIS] = {
  {{200, .8f, 1.1f}, {20, 1.1f, 1.1f}, {220, .6f, .8f}, {0, 1, 1}},
  {{-30, .9f, .8f}, {160, .8f, 1.0f}, {30, .7f, .7f}, {0, 1, 1}},
  {{90, .5f, 1.3f}, {280, .7f, 1.1f}, {200, .5f, .6f}, {0, 1, 1}},
};
/* bodies no named character wears, so a walk in is never somebody's twin */
static const actor_model VIS_MODEL[NVIS] = {AM_W, AM_M, AM_S};
/* the props people carry */
#define NBOX 4
static t3_mesh *BOX[NBOX]; static t3_material *box_mat;
/* the player's clip while a task holds them (carrying, kneeling): played over player.c's own */
static const char *ovr_clip; static float ovr_t, ovr_rate; static bool ovr_loop;
static v3 last_ppos;
static void gesture(const char *clip, float rate);

static const sc_task *task_def(int i) { return &TASKS[roam->t0 + i]; }
static actor_t *task_actor(int i) {
  if (T[i].who >= 0) return actor_of(T[i].who);
  if (T[i].vis >= 0) return VIS[T[i].vis].a;
  return NULL;
}
static walk_t *task_walk(int i) {
  if (T[i].who >= 0) return &A[T[i].who].w;
  if (T[i].vis >= 0) return &VIS[T[i].vis].w;
  return NULL;
}
/* kinds where the person stands at the task's mark (a greet walks in to it; a point's person follows you) */
static bool task_has_person(int k) { return k == TK_GREET || k == TK_ANSWER || k == TK_SIGNIN; }
static int vis_take(int task) {
  for (int v = 0; v < NVIS; v++) if (VIS[v].task < 0) {
    if (!VIS[v].a) { VIS[v].a = actor_new(VIS_MODEL[v] < AM_COUNT ? VIS_MODEL[v] : AM_B, &VIS_PAL[v], 1, scene); }
    VIS[v].task = task; VIS[v].w.on = false;
    return v;
  }
  return -1;
}
static void box_geo(void) {
  if (box_mat) return;
  box_mat = t3_mesh_lambert_material_new(0xffffff);
  box_mat->vertex_colors = true; box_mat->fog = true; box_mat->side = T3_DOUBLE_SIDE; box_mat->version++;
  static const uint32_t COL[NBOX] = { 0xa8804c, 0xb03a2e, 0x9c2a22, 0x8c9098 };   /* cardboard, a soda case, a toolbox, a parts bin */
  static const float S[NBOX][3] = { {.42f, .3f, .32f}, {.4f, .22f, .27f}, {.46f, .2f, .2f}, {.36f, .16f, .26f} };
  for (int b = 0; b < NBOX; b++) {
    float pos[36 * 3], nor[36 * 3], col[36 * 3]; int n = 0;
    float hx = S[b][0] / 2, hy = S[b][1], hz = S[b][2] / 2;
    static const int F[6][4][3] = {
      {{1,0,0},{1,1,0},{1,1,1},{1,0,1}}, {{0,0,1},{0,1,1},{0,1,0},{0,0,0}}, {{0,1,0},{0,1,1},{1,1,1},{1,1,0}},
      {{0,0,0},{1,0,0},{1,0,1},{0,0,1}}, {{0,0,1},{1,0,1},{1,1,1},{0,1,1}}, {{1,0,0},{0,0,0},{0,1,0},{1,1,0}} };
    static const float N[6][3] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
    float r = ((COL[b] >> 16) & 255) / 255.f, g = ((COL[b] >> 8) & 255) / 255.f, bl = (COL[b] & 255) / 255.f;
    for (int f = 0; f < 6; f++) {
      static const int TRI[6] = {0, 1, 2, 0, 2, 3};
      float k = f == 2 ? 1.15f : f == 3 ? .6f : 1;
      for (int t = 0; t < 6; t++) {
        const int *v = F[f][TRI[t]];
        pos[n * 3] = v[0] ? hx : -hx; pos[n * 3 + 1] = v[1] ? hy : 0; pos[n * 3 + 2] = v[2] ? hz : -hz;
        nor[n * 3] = N[f][0]; nor[n * 3 + 1] = N[f][1]; nor[n * 3 + 2] = N[f][2];
        col[n * 3] = fminf(1, r * k); col[n * 3 + 1] = fminf(1, g * k); col[n * 3 + 2] = fminf(1, bl * k);
        n++;
      }
    }
    t3_geometry *G = t3_geometry_new();
    t3_attribute *ap = t3_attribute_new(T3_FLOAT32, pos, n, 3), *an = t3_attribute_new(T3_FLOAT32, nor, n, 3), *ac = t3_attribute_new(T3_FLOAT32, col, n, 3);
    t3_geometry_set_attribute(G, T3_ATTR_POSITION, ap); t3_release(ap);
    t3_geometry_set_attribute(G, T3_ATTR_NORMAL, an); t3_release(an);
    t3_geometry_set_attribute(G, T3_ATTR_COLOR, ac); t3_release(ac);
    BOX[b] = t3_mesh_new(G, box_mat); t3_release(G);
    ((t3_object *)BOX[b])->visible = false; ((t3_object *)BOX[b])->frustum_culled = false;
    t3_object_add(&scene->base, BOX[b]);
  }
}
static int box_kind(const sc_task *k) {
  const char *s = k->label ? k->label : k->name;
  if (!s) return 0;
  if (strstr(s, "soda") || strstr(s, "Soda") || strstr(s, "drink") || strstr(s, "can")) return 1;
  if (strstr(s, "tool") || strstr(s, "Tool") || strstr(s, "drill") || strstr(s, "clamp")) return 2;
  if (strstr(s, "part") || strstr(s, "bin") || strstr(s, "filament")) return 3;
  return 0;
}
static void box_place(int b, v3 p, float yaw) {
  if (b < 0) return;
  t3_object *o = (t3_object *)BOX[b];
  o->visible = true; t3_object_set_position(o, p.x, p.y, p.z); t3_object_set_rotation(o, 0, yaw, 0);
}
static void box_hide(int b) { if (b >= 0 && BOX[b]) ((t3_object *)BOX[b])->visible = false; }
static v3 box_spot(int i) {
  v3 p = v3_add(T[i].at, v3_scale(fwd_of(T[i].at_yaw), .55f));
  float f = world_floor_y(v3_(p.x, p.y + .5f, p.z)); if (f > -1e8f) p.y = f;
  return p;
}

static int done_n(void) { int n = 0; for (int i = 0; i < tn; i++) if (T[i].st == TS_DONE) n++; return n; }
static int gate_n(void) {
  int req = 0; for (int i = 0; i < tn; i++) if (!task_def(i)->optional) req++;
  int n = roam->need > 0 ? roam->need : req;
  return n < tn ? n : tn;
}
static bool gate_open(void) { return done_n() >= gate_n(); }
/* where the player goes to do a task now */
static v3 task_point(int i) {
  const sc_task *k = task_def(i);
  if (tcur == i && tphase == TP_PLAY) {
    if (k->kind == TK_CARRY) return T[i].holding ? T[i].to : T[i].at;
  }
  if (k->kind == TK_POINT) return T[i].to;
  if (task_has_person(k->kind)) { actor_t *a = task_actor(i); if (a) return actor_pos(a); }
  if (k->kind == TK_CARRY) return box_spot(i);
  return street_task_point(T[i].at);   /* street */
}
static const char *task_prompt(int i) {
  const sc_task *k = task_def(i);
  if (k->kind == TK_CARRY && tcur == i && tphase == TP_PLAY) return T[i].holding ? "Put it down" : "Pick up";
  if (k->prompt) return k->prompt;
  switch (k->kind) {
    case TK_GREET: return "Say hi"; case TK_POINT: return "Show them around"; case TK_ANSWER: return "Talk";
    case TK_CARRY: return "Pick up"; case TK_FIX: return "Fix it"; case TK_SIGNIN: return "Sign them in";
    default: return "Do it";
  }
}
static const char *task_label(int i) { const sc_task *k = task_def(i); return k->label ? k->label : k->name; }

/* a mark on top of furniture: the nearest clear floor instead, so nobody stands on a table */
static v3 clear_floor(v3 p) {
  if (!world_in_lab(p) || p.y < .3f) return p;
  int cx = nav_ix(p.x), cz = nav_ix(p.z);
  for (int r = 1; r <= 6; r++) for (int dx = -r; dx <= r; dx++) for (int dz = -r; dz <= r; dz++) {
    if (abs(dx) != r && abs(dz) != r) continue;
    if (nav_free(cx + dx, cz + dz)) { v3 q = v3_((cx + dx) * NAV_R, 0, (cz + dz) * NAV_R); float f = world_floor_y(v3_(q.x, 1, q.z)); q.y = f > -1e8f && f < .3f ? f : 0; return q; }
  }
  return p;
}
/* someone walks in to stand at p: from the street when p is the lab, else along the sidewalk */
static void walk_in(actor_t *a, walk_t *w, v3 p, float yaw, float side) {
  v3 s;
  if (world_in_lab(p)) { float dy; if (!place_of("lab_door", &s, &dy)) s = v3_(0, 0, 2.4f); s = v3_add(s, v3_(side * 1.2f, 0, 2.2f)); }
  else s = v3_add(p, v3_(side * 4.5f, 0, 1.2f));
  float f = world_floor_y(v3_(s.x, s.y + 1, s.z)); if (f > -1e8f) s.y = f;
  actor_place(a, s, atan2f(p.x - s.x, p.z - s.z));
  walk_start(w, a, p, yaw, true, 1.15f);
  actor_play(a, "Walk_Loop", 1, true, .2f);
}
static void task_show_person(int i) {
  const sc_task *k = task_def(i);
  if (k->kind == TK_GREET && T[i].who < 0) T[i].vis = vis_take(i);
  actor_t *a = task_actor(i); if (!a) return;
  if (k->kind == TK_GREET) {
    float side = ds_rand() < .5f ? -1 : 1;
    actor_visible(a, true); actor_hands_up(a, 0); actor_expr_clear(a);
    walk_in(a, task_walk(i), T[i].at, T[i].at_yaw, side);
    T[i].arrived = false; T[i].win = 1;
    /* whoever comes with them (a task that waits on this greeting, its person not about yet) */
    int gi = roam->t0 + i, n = 0;
    for (int j = 0; j < tn; j++) {
      const sc_task *q = task_def(j);
      if (q->requires != gi || T[j].who < 0 || T[j].who == T[i].who || actor_is_visible(actor_of(T[j].who))) continue;
      actor_t *c = actor_of(T[j].who); actor_visible(c, true); actor_expr_clear(c);
      v3 off = v3_scale(v3_(cosf(T[i].at_yaw), 0, -sinf(T[i].at_yaw)), .8f * (n++ ? -1 : 1));
      walk_in(c, &A[T[j].who].w, v3_add(T[i].at, off), T[i].at_yaw, side);
    }
    return;
  }
  if (actor_is_visible(a)) return;          /* already about: the one following you, someone from the scene */
  actor_visible(a, true); actor_hands_up(a, 0); actor_expr_clear(a);
  v3 p = T[i].at; float y = T[i].at_yaw;
  if (k->kind == TK_POINT) { p = v3_add(player_pos(), v3_scale(fwd_of(player_yaw()), -1.4f)); y = player_yaw(); }
  else if (!task_has_person(k->kind)) { p = v3_add(p, v3_scale(v3_(cosf(y), 0, -sinf(y)), .95f)); y += DS_PI * .5f; }
  p = clear_floor(p);
  actor_place(a, p, y);
  actor_play(a, k->kind == TK_ANSWER ? "Idle_TalkingPhone_Loop" : "Idle_Loop", 1, true, .2f);
  A[T[i].who].base = "Idle_Loop";
}
static void tasks_begin(const sc_beat *b) {
  tn = b->nt < TMAX ? b->nt : TMAX; tcur = -1; tphase = TP_NONE;
  memset(T, 0, sizeof T);
  box_geo();
  int nb = 0;
  for (int i = 0; i < tn; i++) {
    const sc_task *k = &TASKS[b->t0 + i];
    task_t *t = &T[i]; bool hy;
    t->who = k->who >= 0 ? canon(k->who) : -1; t->vis = -1; t->box = -1;
    if (k->mark.kind != MK_NONE) t->at = resolve(&k->mark, &t->at_yaw, &hy);
    else if (k->kind == TK_GREET || k->kind == TK_SIGNIN) { place_of(k->kind == TK_GREET ? "lab_entry" : "lab_signin", &t->at, &t->at_yaw); }
    else { t->at = roam_pos; t->at_yaw = 0; }
    if (k->kind == TK_SIGNIN && k->mark.kind == MK_NONE) t->at = v3_add(t->at, v3_(.7f, 0, .3f));
    if (k->mark2.kind != MK_NONE) t->to = resolve(&k->mark2, &t->to_yaw, &hy);
    else { t->to = t->at; t->to_yaw = t->at_yaw; }
    if (k->kind == TK_CARRY) {   /* the prop by what is carried, another if that one is taken */
      int want = box_kind(k);
      for (int j = 0; j < NBOX && (nb >> want & 1); j++) want = (want + 1) % NBOX;
      if (!(nb >> want & 1)) { t->box = want; nb |= 1 << want; }
    }
  }
}
static void tasks_end(void) {
  for (int v = 0; v < NVIS; v++) { if (VIS[v].a) actor_visible(VIS[v].a, false); VIS[v].task = -1; VIS[v].w.on = false; }
  for (int b = 0; b < NBOX; b++) box_hide(b);
  if (roam) for (int i = 0; i < tn; i++) {
    if (T[i].who >= 0 && A[T[i].who].a && T[i].who != canon(W_ME) && T[i].who != roam_follow) { actor_visible(A[T[i].who].a, false); A[T[i].who].w.on = false; }
    int cw = TASKS[roam->t0 + i].with; if (cw >= 0) { cw = canon(cw); if (A[cw].a && cw != canon(W_ME) && cw != roam_follow) { actor_visible(A[cw].a, false); A[cw].w.on = false; } }
  }
  tn = 0; tcur = -1; tphase = TP_NONE; ovr_clip = NULL;
  STORY_UI.ntask = 0; STORY_UI.mg = MG_NONE; STORY_UI.npin = 0;
}
static void call_beats(int start) {
  call_ret = pc; pc = start; block = B_NONE;
  STORY_UI.prompt_use = NULL;
  if (sub_who >= 0) { voice_stop(); sub_clear(); }
}
static void reward(const sc_task *k, int quality) {
  story_ui_t *U = &STORY_UI; int shown = 0;
  for (int s = 0; s < 2; s++) {
    if (k->stat[s] < 0 || !k->stat_n[s]) continue;
    if (quality < 2 && s == 1) continue;      /* a slow greeting earns the first stat only */
    if (quality < 1) continue;
    U->stats[k->stat[s]] += k->stat_n[s];
    const char *nm = story_stat_name(k->stat[s]);
    if (!nm) continue;
    if (!shown) { U->toast = nm; U->toast_n = k->stat_n[s]; U->toast2 = NULL; U->toast_t = 2.6f; }
    else { U->toast2 = nm; U->toast2_n = k->stat_n[s]; }
    shown++;
  }
}
static void task_finish(int i, int quality) {
  const sc_task *k = task_def(i);
  T[i].st = TS_DONE; T[i].holding = false;
  reward(k, quality);
  sfx(SFX_UI_OK, 0, 1); sfx(SFX_PICKUP, 0, .6f);
  STORY_UI.mg = MG_NONE; ovr_clip = NULL;
  actor_t *a = task_actor(i);
  if (a && actor_is_visible(a)) {
    actor_expr_set(a, quality > 0 ? EX_JOY : EX_SORROW, quality > 0 ? .9f : .5f);
    /* a nod, or folded arms; somebody kneeling at work or sitting stays down and only looks up */
    if (!busy_pose(T[i].who)) actor_play(a, quality > 0 ? "Yes" : "Idle_FoldArms_Loop", 1, quality <= 0, .2f);
    actor_look(a, actor_head_pos(actor_of(W_ME)), .8f);
    T[i].look_t = 2.2f;
  }
  actor_play(actor_of(W_ME), "Idle_Loop", 1, true, .25f);
  /* the clock moves as the work gets done */
  if (roam->f3 > 0) { int g = gate_n(); float fr = g > 0 ? fminf(1, (float)done_n() / g) : 1; hour_target = roam_h0 + (roam->f3 - roam_h0) * fr; }
#ifdef KH_AUTO
  ds_log("[auto] pc=%d %.1fh task done %s q%d", pc, hour, k->name ? k->name : "?", quality);
#endif
  if (k->done >= 0) { tcur = i; tphase = TP_POST; call_beats(k->done); }
  else { tcur = -1; tphase = TP_NONE; }
}
/* the minigame begins, the start beats done */
static void mg_setup(int i) {
  const sc_task *k = task_def(i);
  story_ui_t *U = &STORY_UI;
  U->mg = MG_NONE; U->mg_t = 0; U->mg_v = 0; U->mg_hits = 0; U->mg_step = 0; U->mg_shake = 0; U->mg_flash = 0; U->mg_angle = 0;
  U->mg_title = task_label(i);
  actor_t *me = actor_of(W_ME);
  switch (k->kind) {
    case TK_GREET: task_finish(i, T[i].quality); return;
    case TK_ANSWER: task_finish(i, 2); return;
    case TK_ERRAND: task_finish(i, 2); if (block == B_ROAM) gesture("Interact", 1.2f); return;
    case TK_CARRY: U->mg = MG_CARRY; U->mg_trips = k->n > 0 ? k->n : 1; U->mg_trip = T[i].trips; T[i].holding = false; break;
    case TK_POINT: task_finish(i, 2); return;
    case TK_SIGNIN: {
      static const char *ACT[4] = {"USE", "JUMP", "DODGE", "HEAVY"};
      U->mg = MG_SIGNIN; U->mg_nseq = k->n >= 3 && k->n <= 4 ? k->n : 4;
      int prev = -1;
      for (int s = 0; s < U->mg_nseq; s++) { int r; do r = (int)(ds_rand() * 4); while (r == prev); U->mg_seq[s] = ACT[r]; prev = r; }
      break;
    }
    case TK_FIX:
      U->mg = k->mode == FIX_TIMING ? MG_TIMING : k->mode == FIX_MASH ? MG_MASH : k->mode == FIX_HOLD ? MG_HOLD : MG_STICK;
      U->mg_need = U->mg == MG_TIMING ? 3 : 1;
      U->mg_needle = 0; U->mg_z0 = .38f; U->mg_z1 = .62f; U->mg_window = 1;
      break;
    default: task_finish(i, 2); return;
  }
  if (U->mg == MG_SIGNIN || (U->mg >= MG_TIMING && U->mg <= MG_STICK)) {
    /* the player steps up to it and gets to work; the camera settles behind */
    player_teleport(T[i].at, T[i].at_yaw);
    bool low = U->mg == MG_MASH || U->mg == MG_STICK;
    me_kneel = 0;
    actor_play(me, low ? "Fixing_Kneeling" : "Interact", U->mg == MG_MASH ? 1.4f : 1, true, .25f);
    if (U->mg == MG_SIGNIN) { actor_t *a = task_actor(i); if (a) actor_look(a, actor_head_pos(me), .8f); }
  }
}
static bool locked_mg(int mg) { return mg == MG_SIGNIN || (mg >= MG_TIMING && mg <= MG_STICK); }
/* a locked minigame's frame: 1 done, -1 walked away, 0 going */
static int mg_run(float dt, const intent_t *in) {
  story_ui_t *U = &STORY_UI;
  U->mg_t += dt;
  U->mg_shake = fmaxf(0, U->mg_shake - dt * 3); U->mg_flash = fmaxf(0, U->mg_flash - dt * 3);
  if (in->back) return -1;
  bool live = U->mg_t > .25f;
  bool press = live && (in->interact || in->light);
  bool held = in->light_held || input_key_down(WC_KEY_E);
  switch (U->mg) {
    case MG_TIMING: {
      /* the needle sweeps; each hit narrows the zone and speeds it up */
      float sp = .75f + U->mg_hits * .22f;
      U->mg_needle += dt * sp * (U->mg_window > 0 ? 1 : -1);
      if (U->mg_needle > 1) { U->mg_needle = 2 - U->mg_needle; U->mg_window = -1; }
      if (U->mg_needle < 0) { U->mg_needle = -U->mg_needle; U->mg_window = 1; }
      if (press) {
        if (U->mg_needle >= U->mg_z0 && U->mg_needle <= U->mg_z1) {
          U->mg_hits++; U->mg_flash = 1; sfx(SFX_UI_OK, 0, .9f);
          float w = .24f - U->mg_hits * .05f, c = ds_rr(.18f + w / 2, .82f - w / 2);
          U->mg_z0 = c - w / 2; U->mg_z1 = c + w / 2;
        } else { U->mg_shake = 1; sfx(SFX_CLASH, 0, .5f); }
      }
      U->mg_v = (float)U->mg_hits / U->mg_need;
      return U->mg_hits >= U->mg_need ? 1 : 0;
    }
    case MG_MASH:
      if (press) { U->mg_v += .075f; U->mg_flash = .6f; sfx(SFX_STEP, 0, .5f); }
      U->mg_v = fmaxf(0, U->mg_v - dt * .3f);
      return U->mg_v >= 1 ? 1 : 0;
    case MG_HOLD:
      if (live && held) U->mg_v += dt / 2.6f;
      else if (U->mg_v > 0) { U->mg_v = fmaxf(0, U->mg_v - dt * 1.1f); if (U->mg_v > .05f && live) U->mg_shake = fmaxf(U->mg_shake, .3f); }
      return U->mg_v >= 1 ? 1 : 0;
    case MG_STICK: {
      float m = sqrtf(in->mx * in->mx + in->my * in->my);
      if (m > .6f) {
        float a = atan2f(in->mx, in->my);
        if (U->mg_window > 0) { float d = ds_wrap_pi(a - U->mg_needle); if (fabsf(d) < 2.0f) U->mg_angle += d; }
        U->mg_needle = a; U->mg_window = 1;
      } else U->mg_window = 0;
      U->mg_v = ds_clamp(fabsf(U->mg_angle) / (DS_PI * 5), 0, 1);
      return U->mg_v >= 1 ? 1 : 0;
    }
    case MG_SIGNIN: {
      const char *got = NULL;
      if (live) { if (in->interact || in->light) got = "USE"; else if (in->jump) got = "JUMP"; else if (in->dodge) got = "DODGE"; else if (in->heavy) got = "HEAVY"; }
      if (got) {
        if (!strcmp(got, U->mg_seq[U->mg_step])) { U->mg_step++; U->mg_flash = 1; sfx(SFX_UI, 0, 1); }
        else { U->mg_shake = 1; sfx(SFX_CLASH, 0, .5f); }
      }
      U->mg_v = (float)U->mg_step / U->mg_nseq;
      return U->mg_step >= U->mg_nseq ? 1 : 0;
    }
    default: return 1;
  }
}
static void project(v3 w, bool *behind, float *x, float *y);
/* start a task when the player presses at it */
static void task_use(int i) {
  const sc_task *k = task_def(i);
  if (tcur == i && tphase == TP_PLAY) {
    story_ui_t *U = &STORY_UI;
    if (k->kind == TK_CARRY) {
      if (!T[i].holding) { T[i].holding = true; U->mg_carrying = true; gesture("PickUp_Table", 1.4f); sfx(SFX_PICKUP, 0, .7f); }
      else {
        T[i].holding = false; U->mg_carrying = false; T[i].trips++; U->mg_trip = T[i].trips;
        box_place(T[i].box, v3_add(T[i].to, v3_scale(fwd_of(T[i].to_yaw), .5f)), T[i].to_yaw);
        sfx(SFX_LAND, 0, .6f);
        if (T[i].trips >= (k->n > 0 ? k->n : 1)) task_finish(i, 2);
      }
    }
    return;
  }
  if (tcur >= 0) return;
  tcur = i;
  if (k->kind == TK_GREET) {
    /* how quickly they were looked up at */
    T[i].quality = !T[i].arrived || T[i].win > .5f ? 2 : T[i].win > 0 ? 1 : 0;
    actor_t *a = task_actor(i); walk_t *w = task_walk(i);
    if (w) w->on = false;
    if (a) { actor_play(a, "Idle_Loop", 1, true, .2f); actor_look(a, actor_head_pos(actor_of(W_ME)), .9f); }
    if (T[i].quality == 2) { STORY_UI.note = "Looked up in time."; STORY_UI.note_t = 2.2f; }
  }
  actor_t *a = task_actor(i);
  if (k->kind == TK_POINT) {
    v3 tg = v3_add(T[i].to, v3_add(v3_scale(fwd_of(T[i].to_yaw), .9f), v3_(0, 1.05f, 0)));
    if (a) { walk_t *w = task_walk(i); if (w) w->on = false; actor_look(a, tg, 1); actor_play(a, "Idle_Loop", 1, true, .2f); }
  } else if (a && actor_is_visible(a)) {
    /* face each other at talking distance: step back out of someone you walked right up to. Somebody kneeling
     * at work or sitting stays as they are, hands on the job, and looks up */
    v3 ap = actor_pos(a), pp = player_pos(), d = v3_sub(ap, pp); d.y = 0;
    float l = v3_len(d);
    /* someone kneeling or sitting is shot from low and to the side: standing 1.1 m off, the player's own body
     * is between every lens and their head, so they get more room */
    const float GAP = busy_pose(T[i].who) ? 1.6f : 1.1f;
    if (l < GAP) {
      v3 dir = l > .05f ? v3_scale(d, 1 / l) : fwd_of(player_yaw());
      for (int n = 0; n < 5; n++) {   /* straight back, then turned a little either way, onto clear floor */
        float t = (n + 1) / 2 * .6f * (n & 1 ? 1 : -1), c = cosf(t), sn = sinf(t);
        v3 r = v3_(dir.x * c - dir.z * sn, 0, dir.x * sn + dir.z * c), q = v3_sub(ap, v3_scale(r, GAP)); q.y = pp.y;
        if (!world_in_lab(q) || nav_free(nav_ix(q.x), nav_ix(q.z))) { pp = q; d = v3_scale(r, GAP); break; }
      }
    }
    player_teleport(pp, atan2f(d.x, d.z));
    if (busy_pose(T[i].who)) actor_look(a, actor_head_pos(actor_of(W_ME)), .8f);
    else actor_move(a, ap, atan2f(-d.x, -d.z));
  }
  if (k->start >= 0 && !T[i].talked) { T[i].talked = true; tphase = TP_PRE; call_beats(k->start); }
  else { tphase = TP_PLAY; mg_setup(i); }
}
static void ovr_apply(actor_t *a, float dt);
static void gesture(const char *clip, float rate) { ovr_clip = clip; ovr_t = 0; ovr_rate = rate; ovr_loop = false; }
static void ovr_apply(actor_t *a, float dt) {
  if (!ovr_clip) return;
  actor_play(a, ovr_clip, 1, ovr_loop, 0);
  ovr_t += dt * ovr_rate;
  float len = actor_clip_len(a);
  if (len > 0) ovr_t = ovr_loop ? fmodf(ovr_t, len) : fminf(ovr_t, len - .01f);
  actor_clip_seek(a, ovr_t);
  actor_update(a, 0);
}
/* per frame for the open tasks: appearance, visitors walking in and looking around, the carried box */
static void tasks_tick(float dt) {
  story_ui_t *U = &STORY_UI;
  for (int i = 0; i < tn; i++) {
    const sc_task *k = task_def(i); task_t *t = &T[i];
    if (t->st == TS_HIDDEN && roam_t >= k->delay && (k->requires < 0 || (k->requires - roam->t0 >= 0 && k->requires - roam->t0 < tn && T[k->requires - roam->t0].st == TS_DONE))) {
      t->st = TS_OPEN;
      task_show_person(i);
      if (k->kind == TK_CARRY) box_place(t->box, box_spot(i), t->at_yaw);
      if (roam_t > .5f) { sfx(SFX_UI, 0, .7f); }
    }
    if (t->st == TS_HIDDEN) continue;
    actor_t *a = task_actor(i); walk_t *w = task_walk(i);
    if (t->look_t > 0) { t->look_t -= dt; if (t->look_t <= 0 && a) actor_expr_clear(a); }
    if (t->vis >= 0 && a && actor_is_visible(a)) {
      if (w->on) { if (walk_step(w, a, dt)) actor_play(a, "Idle_Loop", 1, true, .2f); }
      actor_update(a, dt);
    }
    if (t->st != TS_OPEN || !a) continue;
    if (k->kind == TK_GREET && tcur != i) {
      if (!t->arrived && !w->on) { t->arrived = true; actor_play(a, "Idle_Loop", 1, true, .3f); }
      if (t->arrived) {
        /* looking round for someone; the window closes */
        t->win = fmaxf(0, t->win - dt / 9.f);
        float s = sinf(roam_t * 1.3f + i);
        v3 h = actor_head_pos(a); float y = actor_yaw(a) + s * .9f;
        actor_look(a, v3_(h.x + sinf(y) * 3, h.y - .1f, h.z + cosf(y) * 3), .9f);
        if (t->win <= 0 && a) { actor_expr_set(a, EX_SORROW, .5f); actor_play(a, "Idle_FoldArms_Loop", 1, true, .4f); }
      }
    }
    if (k->kind == TK_POINT && T[i].who >= 0 && T[i].who != roam_follow && tcur < 0) follow_player(a, w, dt, 1.6f);
  }
  /* a task's companion (a parent with a kid) stays close by its person, open or done */
  for (int i = 0; i < tn; i++) {
    const sc_task *k = task_def(i);
    if (k->with < 0 || T[i].st == TS_HIDDEN) continue;
    actor_t *a = task_actor(i); int cw = canon(k->with);
    if (!a || !actor_is_visible(a) || cw == T[i].who) continue;
    actor_t *c = actor_of(cw);
    if (!actor_is_visible(c)) { actor_visible(c, true); actor_place(c, clear_floor(v3_add(actor_pos(a), v3_(.9f, 0, .6f))), actor_yaw(a)); }
    if (!A[cw].w.on || flat_dist(A[cw].w.dest, actor_pos(a)) > 2.2f) follow_to(c, &A[cw].w, actor_pos(a), 1.1f, &T[i].with_t, dt);
    if (!A[cw].w.on && flat_dist(actor_pos(c), actor_pos(a)) < 2.4f) { v3 d = v3_sub(actor_pos(a), actor_pos(c)); A[cw].turn_to = atan2f(d.x, d.z); A[cw].turning = true; }
  }
  /* the carried box rides between the hands */
  if (tcur >= 0 && tphase == TP_PLAY && task_def(tcur)->kind == TK_CARRY) {
    t3_object *bx = T[tcur].box >= 0 ? (t3_object *)BOX[T[tcur].box] : NULL;
    if (bx && T[tcur].holding) {
      actor_t *me = actor_of(W_ME);
      v3 l = actor_bone_pos(me, "leftHand"), r = actor_bone_pos(me, "rightHand"), m = v3_scale(v3_add(l, r), .5f);
      v3 f = fwd_of(actor_yaw(me));
      m = v3_add(m, v3_add(v3_scale(f, .06f), v3_(0, -.14f, 0)));
      box_place(T[tcur].box, m, actor_yaw(me));
    } else if (bx && !T[tcur].holding && T[tcur].trips < (task_def(tcur)->n > 0 ? task_def(tcur)->n : 1) && flat_dist(player_pos(), T[tcur].to) > 2.5f)
      box_place(T[tcur].box, box_spot(tcur), T[tcur].at_yaw);   /* the next one waits where they came from */
  }
  /* the list */
  U->ntask = 0;
  for (int i = 0; i < tn && U->ntask < UI_TASKS; i++) {
    if (T[i].st == TS_HIDDEN) continue;
    ui_task *r = &U->task[U->ntask++];
    if (r->label != task_label(i)) r->age = 0;
    r->label = task_label(i); r->optional = task_def(i)->optional;
    r->state = T[i].st == TS_DONE ? UT_DONE : tcur == i ? UT_ACTIVE : UT_OPEN; r->target = false; r->age += dt;
  }
}

/* ---- v1.3: the loaner bike in scenes, and small props the story moves (Gerald) ----
 * BIKE parks the bike.c bicycle at a mark or has someone push it along beside them (a thief making off with it);
 * the roam gear leaves it alone while a scene holds it. The flag has_bike follows gear_bike_have. */
static void bike_sync(void) { if (FLG_BIKE >= 0) flags[FLG_BIKE] = gear_bike_have; }
static void bike_scene_op(const sc_beat *b) {
  bike_init(scene);
  switch (b->i) {
    case 0: bike_show(false); bike_with = -1; bike_scene = false; break;
    case 1: {
      sc_mark m = b->mark; float y; bool hy; v3 p = resolve(&m, &y, &hy);
      bike_pose(p, y, .1f, .35f, bike_wheel, 0); bike_show(true); bike_with = -1; bike_scene = true;
      break; }
    case 2: bike_with = canon(b->who); bike_scene = true; bike_last = actor_pos(actor_of(bike_with)); bike_show(true); break;
    case 3: gear_bike_have = true; bike_sync(); break;
  }
}
static void bike_scene_tick(void) {
  if (bike_with < 0 || !A[bike_with].a || !actor_is_visible(A[bike_with].a)) return;
  actor_t *a = A[bike_with].a; float y = actor_yaw(a);
  v3 p = actor_pos(a), side = v3_(cosf(y), 0, -sinf(y));
  v3 at = v3_add(p, v3_add(v3_scale(side, -.42f), v3_scale(fwd_of(y), .25f)));
  bike_wheel = fmodf(bike_wheel + flat_dist(p, bike_last) / BIKE_WHEEL_R, 6.2832f); bike_last = p;
  float f = world_floor_y(v3_(at.x, at.y + .6f, at.z)); if (f > -1e8f) at.y = f;
  bike_pose(at, y, .04f, .05f, bike_wheel, 0);
}
/* small props the story moves. gerald: Ines's line follower, a shoebox of a robot on two wheels: 1 sits still,
 * 2 drives a slow circle to its left (one motor wired backwards), 3 drives straight out and back. table: the fix-it
 * table, a folding table with a cloth, a lamp and a sign (1 stands). weld_table: the lab's welding table on its
 * casters, the same model the back lot fight guards. van: Copper Cal's work van, the mark at its open back doors */
enum { PROP_GERALD, PROP_TABLE, PROP_WELD, PROP_VAN, PROP_N };
static struct { t3_object *o; int mode; v3 home; float yaw0, t; } PR[PROP_N];
static t3_object *prop_build(int k) {
  if (k == PROP_WELD) { t3_object *o = fight_weld_table_model(); o->visible = false; return o; }
  fxp_begin();
  if (k == PROP_VAN) {
    /* a high roof cargo van along -z from its back doors at the origin: white, a copper stripe, dark glass */
    const uint32_t body = 0xe8e6e0, trim = 0x2a2c30, glass = 0x26303a, copper = 0xb87333;
    fxp_box(v3_(0, 1.27f, -1.95f), v3_(1.96f, 1.74f, 3.9f), body);           /* the cargo box */
    fxp_box(v3_(0, 1.17f, -4.3f), v3_(1.96f, 1.54f, .8f), body);             /* the cab */
    fxp_box(v3_(0, .77f, -5.0f), v3_(1.9f, .74f, .62f), body);               /* the hood */
    fxp_box(v3_(0, 1.55f, -4.71f), v3_(1.7f, .62f, .04f), glass);            /* the windshield */
    for (int s = -1; s <= 1; s += 2) {
      fxp_box(v3_(s * .99f, 1.5f, -4.3f), v3_(.02f, .5f, .62f), glass);       /* the cab's side windows */
      fxp_box(v3_(s * .99f, 1.3f, -1.95f), v3_(.02f, .16f, 3.7f), copper);    /* the stripe */
      fxp_box(v3_(s * 1.0f, 1.12f, .46f), v3_(.05f, 1.5f, .88f), body);      /* a back door, swung open on its hinge */
      fxp_box(v3_(s * 1.0f, 1.55f, .5f), v3_(.06f, .4f, .62f), glass);
      fxp_box(v3_(s * .9f, .95f, -.02f), v3_(.14f, .3f, .05f), 0xc02a20);     /* a tail light */
      fxp_box(v3_(s * 1.05f, 1.55f, -3.95f), v3_(.12f, .16f, .06f), trim);   /* a mirror */
      for (int a = 0; a < 2; a++) {                                          /* the wheels, in their arches */
        v3 w = v3_(s * .86f, .34f, a ? -4.15f : -.85f);
        fxp_ring(w, v3_(1, 0, 0), .24f, .11f, 12, 0x18181c);
        fxp_ring(w, v3_(1, 0, 0), .1f, .05f, 8, 0x9a9aa0);
      }
    }
    fxp_box(v3_(0, 1.2f, -.03f), v3_(1.8f, 1.5f, .02f), 0x1a1a1e);          /* the dark inside, through the doors */
    fxp_box(v3_(0, .44f, -.08f), v3_(1.42f, .06f, .1f), 0x6a6a70);           /* the floor's lip */
    fxp_box(v3_(0, .32f, .04f), v3_(2.0f, .16f, .14f), trim);                /* the bumper */
    fxp_box(v3_(0, .5f, -5.33f), v3_(2.0f, .2f, .12f), trim);                /* the front bumper */
    fxp_box(v3_(0, .82f, -5.32f), v3_(1.2f, .22f, .04f), 0x4a4c52);          /* the grille */
    fxp_layer(FXP_HALO); fxp_disc(v3_(0, .03f, -1.4f), 2.6f, 12, 0x30200c, 0); fxp_disc(v3_(0, .03f, -3.9f), 2.6f, 12, 0x30200c, 0);
  } else if (k == PROP_GERALD) {
    fxp_box(v3_(0, .05f, 0), v3_(.16f, .06f, .2f), 0x3a6ea8u);              /* the chassis */
    fxp_box(v3_(0, .1f, -.02f), v3_(.12f, .04f, .1f), 0x2a8a4au);            /* the board on top */
    fxp_box(v3_(0, .13f, .07f), v3_(.07f, .035f, .03f), 0xd8d8d0u);          /* the eyes: an ultrasonic sensor */
    for (int s = -1; s <= 1; s += 2) fxp_ring(v3_(s * .095f, .035f, -.03f), v3_(1, 0, 0), .032f, .012f, 10, 0x1a1a1eu);
    fxp_cyl(v3_(0, 0, .07f), .02f, .012f, .012f, 6, 0x9a9aa0u);              /* the caster */
    fxp_layer(FXP_GLOW); fxp_box(v3_(.045f, .125f, -.06f), v3_(.015f, .015f, .015f), 0x40ff60u);
  } else {
    fxp_box(v3_(0, .74f, 0), v3_(1.8f, .04f, .76f), 0xe0d8c8u);              /* the top, under a cloth */
    fxp_box(v3_(0, .62f, .37f), v3_(1.8f, .24f, .01f), 0xe0642au);           /* the cloth's front, lab orange */
    for (int i = 0; i < 4; i++) { float x = i & 1 ? .82f : -.82f, z = i & 2 ? .3f : -.3f; fxp_bar(v3_(x, 0, z), v3_(x, .72f, z), .03f, 0x8a8a90u); }
    fxp_box(v3_(-.5f, .82f, -.05f), v3_(.32f, .12f, .24f), 0x6a5040u);      /* a toolbox */
    fxp_box(v3_(.35f, .8f, .05f), v3_(.4f, .08f, .3f), 0x3a3c42u);           /* a mat, a board on it */
    fxp_bar(v3_(.75f, .76f, -.2f), v3_(.75f, 1.15f, -.2f), .02f, 0x2a2a2eu); /* the lamp */
    fxp_box(v3_(.75f, 1.17f, -.12f), v3_(.1f, .06f, .14f), 0x2a2a2eu);
    fxp_box(v3_(-.62f, .96f, .2f), v3_(.46f, .3f, .02f), 0xf2ece0u);        /* a tent sign at the left end, low: FIX IT, free */
    fxp_box(v3_(-.62f, 1.02f, .21f), v3_(.36f, .07f, .02f), 0xe0642au);
    fxp_layer(FXP_GLOW); fxp_box(v3_(.75f, 1.12f, -.06f), v3_(.06f, .02f, .06f), 0xfff0c0u);
  }
  t3_object *o = fxp_end(); o->visible = false; return o;
}
/* the van and the welding table are things you walk into, not through: a solid box while they stand */
static void prop_solid(int k) {
  bool on = PR[k].o && PR[k].o->visible; float yw = PR[k].yaw0; v3 h = PR[k].home;
  if (k == PROP_VAN)        /* 2 m wide, from its back doors 5.45 m forward along its local -z; 2.15 m tall, but the box
                             * reaches 2.4 so the lens treats it as a wall to come in front of, not a prop to climb over */
    world_solid(0, on, v3_add(h, v3_(-2.62f * sinf(yw), 0, -2.62f * cosf(yw))), 1.0f, 2.75f, yw, 2.4f);
  else if (k == PROP_WELD)
    world_solid(1, on, h, .92f, .42f, yw, .95f);
}
static void props_hide(void) { for (int k = 0; k < PROP_N; k++) if (PR[k].o) { PR[k].o->visible = false; PR[k].mode = 0; prop_solid(k); } }
static void prop_op(const sc_beat *b) {
  int k = !b->s ? PROP_GERALD : !strcmp(b->s, "table") ? PROP_TABLE : !strcmp(b->s, "weld_table") ? PROP_WELD : !strcmp(b->s, "van") ? PROP_VAN : PROP_GERALD;
  if (!PR[k].o) PR[k].o = prop_build(k);
  if (b->i == 0) { PR[k].o->visible = false; PR[k].mode = 0; prop_solid(k); return; }
  sc_mark m = b->mark; float y; bool hy;
  if (m.kind != MK_NONE) { PR[k].home = resolve(&m, &y, &hy); PR[k].yaw0 = hy ? y : 0; }
  PR[k].mode = b->i; PR[k].t = 0; PR[k].o->visible = true;
  t3_object_set_position(PR[k].o, PR[k].home.x, PR[k].home.y, PR[k].home.z); t3_object_set_rotation(PR[k].o, 0, PR[k].yaw0, 0);
  prop_solid(k);
}
static void prop_tick(float dt) {
  for (int k = 0; k < PROP_N; k++) {
    if (!PR[k].o || !PR[k].o->visible || PR[k].mode < 2) continue;
    PR[k].t += dt;
    float t0 = PR[k].t, yaw0 = PR[k].yaw0; v3 home = PR[k].home, p = home; float y = yaw0;
    if (PR[k].mode == 2) {   /* a circle of about 35 cm to its left, slowly */
      float r = .35f, ang = yaw0 + t0 * .7f;
      v3 c = v3_add(home, v3_scale(v3_(cosf(yaw0), 0, -sinf(yaw0)), -r));
      p = v3_add(c, v3_scale(v3_(cosf(ang), 0, -sinf(ang)), r)); y = ang;
    } else {                 /* straight out a metre and a half and back, turning at the ends */
      float L = 1.5f, v = .35f, per = 2 * L / v + 2, t = fmodf(t0, per), d;
      if (t < L / v) d = t * v;
      else if (t < L / v + 1) { d = L; y += DS_PI * (t - L / v); }
      else if (t < 2 * L / v + 1) { d = L - (t - L / v - 1) * v; y += DS_PI; }
      else { d = 0; y += DS_PI + DS_PI * (t - 2 * L / v - 1); }
      p = v3_add(home, v3_scale(fwd_of(yaw0), d));
    }
    float f = world_floor_y(v3_(p.x, p.y + .4f, p.z)); if (f > -1e8f) p.y = f;
    t3_object_set_position(PR[k].o, p.x, p.y, p.z); t3_object_set_rotation(PR[k].o, 0, y, 0);
  }
}

/* ---- the machine ---- */
const char *story_stat_name(int i) {
  switch (i) { case ST_TRUST: return "TRUST"; case ST_SAFETY: return "SAFETY"; case ST_VIBE: return "VIBE"; case ST_DUES: return "DUES"; case ST_ENERGY: return "ENERGY"; default: return NULL; }
}
static void step(void) {
  while (block == B_NONE && pc < SCRIPT_COUNT) {
    const sc_beat *b = &SCRIPT[pc];
    switch (b->op) {
      case OP_CHAPTER:
        if (sub_roam && STORY_UI.sub_t > 0) { voice_stop(); sub_clear(); }   /* the walk's last line does not run over the card */
        hour = hour_target = b->f; world_set_time(hour); STORY_UI.hour = hour;
        save_chap = title_hash(b->s);
        pc++; autosave(); pc--;
        STORY_UI.chapter = true; STORY_UI.ch_title = b->s; STORY_UI.ch_sub = b->s2; STORY_UI.ch_t = 0; STORY_UI.ch_dur = 2.8f; STORY_UI.ch_hour = b->f;
        preload_chapter(pc);
        break;
      case OP_CLOCK: hour = hour_target = b->f; world_set_time(hour); break;
      case OP_PLACE: {
        actor_t *a = actor_of(b->who); slot_t *s = &A[canon(b->who)];
        float y; bool hy; v3 p = resolve(&b->mark, &y, &hy);
        actor_visible(a, true); actor_hands_up(a, 0); actor_place(a, p, y); s->w.on = false; s->base = "Idle_Loop";
        if (canon(b->who) == me_id) player_teleport(p, y);   /* the player is where the scene puts them: a later hand-off must not snap them back */
        actor_play(a, "Idle_Loop", 1, true, .1f);
        if (b->who2 >= 0) { actor_t *t = actor_of(b->who2); if (actor_is_visible(t)) face_now(b->who, b->who2); actor_look(a, actor_head_pos(t), .8f); }
        break;
      }
      case OP_HIDE: { int w = canon(b->who); if (A[w].a) { actor_visible(A[w].a, false); A[w].w.on = false; } break; }
      case OP_WALK: {
        actor_t *a = actor_of(b->who); slot_t *s = &A[canon(b->who)];
        float y; bool hy; v3 p = resolve(&b->mark, &y, &hy);
        actor_visible(a, true);
        if (skip_scene) { actor_place(a, p, y); s->w.on = false; break; }
        walk_start(&s->w, a, p, y, hy, b->f);
        actor_play(a, walk_clip(b->f), 1, true, .18f);
        if (!b->i) { say_who = canon(b->who); block = B_WALK; }
        break;
      }
      case OP_WAITWALK: if (A[canon(b->who)].w.on) { say_who = canon(b->who); block = B_WALK; } break;
      case OP_SAY: {
        if (bark_t > 0 && !skip_scene) { wait_t = bark_t; block = B_WAIT; pc--; break; }   /* a bark finishes first */
        int w = canon(b->who);
        actor_t *a = actor_of(w);
        if (b->i >= 0) { actor_expr_clear(a); actor_expr_set(a, (actor_expr)b->i, b->f); A[w].ex = (actor_expr)b->i; A[w].ex_t = 4; }
        if (b->s2) { const char *clip = sit_snap(a, b->s2); actor_play(a, clip, 1, !strstr(clip, "Interact"), .25f); A[w].base = clip; }
        else if (A[w].base && !strcmp(A[w].base, "Idle_Loop")) actor_play(a, "Idle_Talking_Loop", 1, true, .3f);
        expand(line_buf, sizeof line_buf, b->s);
        snprintf(name_buf, sizeof name_buf, "%s", display_name(w));
        int l = b->who2 >= 0 ? canon(b->who2) : listener_of(w);
        if (l >= 0 && A[l].a) {
          actor_look(a, actor_head_pos(A[l].a), .9f);
          actor_look(A[l].a, actor_head_pos(a), .9f);
          /* a speaker turns to whoever they are talking to, a little */
          v3 d = v3_sub(actor_pos(A[l].a), actor_pos(a));
          if (!A[w].w.on && d.x * d.x + d.z * d.z > .01f) { A[w].turn_to = atan2f(d.x, d.z); A[w].turning = true; }
        }
        auto_cover(w, b->who2, (sh_type)b->i2);
        if (skip_scene) break;
        sub_clear();
        voice_say(speaker_id(w), line_buf);
        say_len = voice_seconds(speaker_id(w), line_buf);
        if (say_len <= 0) say_len = fmaxf(1.4f, strlen(line_buf) / 16.f);
        say_t = 0; say_who = w; block = B_SAY;
        STORY_UI.talking = true; STORY_UI.speaker = name_buf; STORY_UI.line = line_buf; STORY_UI.line_t = 0;
        STORY_UI.line_cps = fmaxf(20, strlen(line_buf) / fmaxf(.5f, say_len * .94f)); STORY_UI.line_done = false;
        STORY_UI.speaker_col = who_color(w);
        break;
      }
      case OP_BARK:
        if (skip_scene) break;
        bark_t = sub_say(b->who, b->s);
        break;
      case OP_SHOT: {
        dir_spec s; memset(&s, 0, sizeof s);
        s.type = (sh_type)b->i;
        if (s.type == SH_FIXED) {
          sc_mark m1 = b->mark, m2 = b->mark2;
          s.fpos = resolve(&m1, NULL, NULL); s.flook = resolve(&m2, NULL, NULL); s.fpos.y += 1.45f; s.flook.y += 1.2f; s.fov = b->f;
        } else {
          s.nsubj = resolve_subj(b->subj, s.subj); s.dist = b->f; s.height = b->f2; s.fov = b->i2 / 10.f; s.dur = b->target / 100.f;
          if (b->flag >= 0) s.part = b->flag;
          if (b->mark.kind != MK_NONE) {
            sc_mark m1 = b->mark; s.fpos = resolve(&m1, NULL, NULL);
            /* a point insert's height is how far above the floor the thing is (a robot on the floor: .15) */
            if (s.part == PART_POINT && b->f2 > 0) { s.fpos.y += b->f2; s.height = 0; } else s.fpos.y += 1.0f;
          }
        }
        occluders(); director_cut(&s); last_speaker = -1; shot_held = true;
        break;
      }
      case OP_WAIT: if (!skip_scene) { wait_t = b->f; block = B_WAIT; } break;
      case OP_FACE: face_now(b->who, b->who2); break;
      case OP_LOOK: actor_look(actor_of(b->who), actor_head_pos(actor_of(b->who2)), .9f); break;
      case OP_CLIP: {
        actor_t *a = actor_of(b->who);
        const char *clip = sit_snap(a, b->s);   /* sitting: onto a real seat, or standing */
        actor_play(a, clip, b->f, b->i != 0, .25f);
        if (b->i) A[canon(b->who)].base = clip;
        break;
      }
      case OP_EMOTE: actor_expr_set(actor_of(b->who), (actor_expr)b->i, b->f); break;
      case OP_OBJECTIVE:
        snprintf(obj_buf, sizeof obj_buf, "%s", b->s); STORY_UI.objective = obj_buf;
        break;
      case OP_ROAM: {
        roam = b; roam_follow = b->who >= 0 ? canon(b->who) : -1;
        world_door_auto(true); door_forced = false;   /* a scene's DOOR(0) turned the automatic door off; free play needs it back */
        if (b->f2 > 0) { hour_target = fmaxf(b->f2, hour_target); }   /* never back past the chapter card's hour */
        roam_h0 = hour_target; roam_t = 0;
        v3 p; float y;
        roam_has_place = place_of(b->s, &p, &y);
        if (!roam_has_place) { if (b->s) ds_log("story: no roam place %s", b->s); p = actor_pos(actor_of(W_ME)); }
        roam_pos = p;
        /* put the side conversations' people out */
        for (int i = 0; i < b->i2 && i < 8; i++) if (!side_done[i]) {
          const sc_side *sd = &SIDES[b->i + i];
          actor_t *ac = actor_of(sd->who);
          float yy; bool hy; v3 q = resolve(&sd->mark, &yy, &hy);
          actor_visible(ac, true); actor_place(ac, q, yy);
          /* a sitter keeps sitting only on a real seat: the mark itself may be on the floor beside it */
          slot_t *ss = &A[canon(sd->who)];
          if (ss->base && !strncmp(ss->base, "Sitting", 7)) { const char *c = sit_snap(ac, ss->base); if (c != ss->base) { actor_play(ac, c, 1, true, .2f); ss->base = c; } }
        }
        tasks_begin(b);
        memset(banter_done, 0, sizeof banter_done);
        attic_warned = false;   /* re-arm the stairs warning for this roam */
        WF.task = -1; WF.n = 0; WF.t = 0; gate_t = 0;
        bike_with = -1; bike_scene = false;   /* a bike a scene parked stays parked; the gear takes it from here */
        director_stop();
        world_door_auto(true);
        player_set_actor(actor_of(W_ME));
        player_teleport(actor_pos(actor_of(W_ME)), actor_yaw(actor_of(W_ME)));
        player_camera_snap();
        last_ppos = player_pos();
        STORY_UI.talking = false;
        skip_scene = false;
        block = B_ROAM;
        break;
      }
      case OP_FIGHT:
        if (skip_scene) { skip_scene = false; }
        sub_clear();
        begin_fight(b, 0);
        break;
      case OP_CHOICE: {
        if (!b->s) {   /* no prompt: a branch on who is playing (options in order Vee, Dex, Ruth), no menu */
          int k = me_id == W_DEX ? 1 : me_id == W_RUTH ? 2 : 0;
          pc = SCRIPT[pc + 1 + (k < b->i ? k : 0)].target - 1;
          break;
        }
        skip_scene = false;
        STORY_UI.choosing = true; STORY_UI.prompt = b->s; STORY_UI.nopt = b->i; STORY_UI.sel = 0; STORY_UI.choose_t = 0;
        choice_first = pc + 1;
        for (int i = 0; i < b->i && i < 5; i++) STORY_UI.opt[i] = SCRIPT[pc + 1 + i].s;
        STORY_UI.talking = false;
        block = B_CHOICE;
        break;
      }
      case OP_OPTION: break;
      case OP_JUMP: pc = b->target - 1; break;
      case OP_JUMP_NOT: {
        bool ok = b->flag >= 0 ? flags[b->flag] : (b->i >= 0 ? STORY_UI.stats[b->i] >= b->f : true);
        if (!ok) pc = b->target - 1;
        break;
      }
      case OP_FLAG: flags[b->flag] = true; break;
      case OP_ADD:
        STORY_UI.stats[b->i] += b->i2;
        if (story_stat_name(b->i)) { STORY_UI.toast = story_stat_name(b->i); STORY_UI.toast_n = b->i2; STORY_UI.toast2 = NULL; STORY_UI.toast_t = 2.4f; }
        break;
      case OP_CARD:
        STORY_UI.card = true; STORY_UI.card_title = b->s; STORY_UI.card_sub = b->s2; STORY_UI.card_t = 0; STORY_UI.card_dur = b->f;
        if (!skip_scene) { wait_t = b->f; block = B_WAIT; } else STORY_UI.card = false;
        break;
      case OP_MUSIC:
        music_mood((mood_t)b->i);
        director_set_mood(b->i == MOOD_FIGHT || b->i == MOOD_BOSS || b->i == MOOD_FINAL || b->i == MOOD_TENSION ? DM_TENSE
                        : b->i == MOOD_CINE || b->i == MOOD_NIGHT ? DM_EMOTIONAL : DM_CALM);
        break;
      case OP_STAGE: world_set_stage((world_stage)b->i); last_speaker = -1; break;
      case OP_GIVE: weapons |= 1u << b->i; STORY_UI.got = b->s; STORY_UI.got_t = 3; battle_unlock_weapon(b->i); break;
      case OP_DRAW: {
        actor_t *a = actor_of(b->who);
        A[canon(b->who)].weapon = b->i;
        weapon_equip(a, b->i);
        if (b->i >= 0 && !skip_scene) weapon_draw_flash(a);
        break;
      }
      case OP_DOOR: world_door(b->f); break;
      case OP_FX:
        switch (b->i) {
          case FX_FADE_IN: STORY_UI.fade = 1; fade_to = 0; fade_rate = 1 / fmaxf(.05f, b->f); break;
          case FX_FADE_OUT: fade_to = 1; fade_rate = 1 / fmaxf(.05f, b->f); if (!skip_scene) { wait_t = b->f; block = B_WAIT; } else STORY_UI.fade = 1; break;
          case FX_FLASH: STORY_UI.flash = 1; flash_rate = 1 / fmaxf(.05f, b->f); dim_to = 0; break;
          case FX_LIGHTS_OUT: dim_to = .5f; break;
          default: break;
        }
        break;
      case OP_NOTE: STORY_UI.note = b->s; STORY_UI.note_t = 4; break;
      case OP_NIGHT: {
        /* a night begins: a new evening at the lab. The state it starts from is kept for the title's night select */
        if (STORY_UI.sub_t > 0) { voice_stop(); sub_clear(); }
        STORY_UI.note = NULL; STORY_UI.note_t = 0; STORY_UI.toast_t = 0; STORY_UI.got_t = 0;   /* nothing of the last part over the card */
        part_now = b->i;
        STORY_UI.stats[ST_ENERGY] = 5;   /* slept on it */
        hide_all(); tasks_end(); director_stop(); world_set_stage(STAGE_TOWN); world_door_auto(true);
        /* memory: the last part's cast goes, weapons first; the night makes again whom it needs (preload_chapter) */
        for (int w = 0; w < WHO_COUNT; w++) if (A[w].a && w != me_id) { weapon_equip(A[w].a, -1); actor_free(A[w].a); memset(&A[w], 0, sizeof A[w]); } /* memory */
        occluders();   /* the director's list held the freed cast until the next cut: a street cameo cuts without refreshing it */ /* memory */
        extras_reset(); street_reset(true);   /* street: a different evening. The extras are cast again, the strangers do not remember today (Moheeb and Luis do), and the talks are new */
        obj_buf[0] = 0; STORY_UI.objective = NULL;
        hour = hour_target = b->f; world_set_time(hour); STORY_UI.hour = hour;
        if (b->i >= 1 && b->i <= NIGHTS_MAX) {
          save_t *sv = save_v3(); carry_t *c = &sv->start[b->i - 1];
          c->ok = 1; c->me = (uint8_t)me_id; c->weapons = (uint8_t)weapons; c->bike = gear_bike_have ? GEAR_BIKE : 0;
          for (int i = 0; i < ST_COUNT; i++) c->stats[i] = (int8_t)ds_clamp(STORY_UI.stats[i], -120, 120);
          bits_put(c->fbits);
        }
        save_chap = title_hash(b->s);
        pc++; autosave(); pc--;
        STORY_UI.chapter = true; STORY_UI.ch_title = b->s; STORY_UI.ch_sub = b->s2; STORY_UI.ch_t = 0; STORY_UI.ch_dur = 3.6f; STORY_UI.ch_hour = b->f;
        preload_chapter(pc);
#ifdef KH_AUTO
        ds_log("[auto] pc=%d NIGHT %d %s", pc, b->i, b->s);
#endif
        break;
      }
      case OP_EPILOGUE: {
        epi_part = b->i;
#ifdef KH_AUTO
        ds_log("[auto] END %s (part %d)", story_end_title(), b->i); for (int i = 0; i < ST_COUNT; i++) ds_log("[auto] stat %d = %d", i, STORY_UI.stats[i]);
        if (pc + 1 >= SCRIPT_COUNT || SCRIPT[pc + 1].op == OP_END) ds_log("[auto] END FINAL %s", story_end_title());
#endif
        /* the save moves on to the next night, so the title's continue starts it */
        save_t *sv = save_v3(); sv->done |= (uint8_t)(1u << (b->i & 7));
        save_chap = 0;
        for (int i = pc + 1; i < SCRIPT_COUNT; i++) if (SCRIPT[i].op == OP_NIGHT) { save_chap = title_hash(SCRIPT[i].s); break; }
        pc++; autosave(); pc--;
        block = B_EPILOGUE; music_mood(MOOD_CLEAR); STORY_UI.cine = false; break;
      }
      case OP_BIKE: bike_scene_op(b); break;
      case OP_PROP: prop_op(b); break;
      case OP_END: block = B_DONE; break;
      case OP_RET: pc = call_ret - 1; call_ret = -1; block = B_ROAM; director_stop(); player_camera_snap(); skip_scene = false; STORY_UI.talking = false; break;
      default: break;
    }
    pc++;
  }
}

/* Fixing_Kneeling is a whole cycle: down onto a knee (to about 1 s), the work, and back up (from 4.2 s) in 5.2 s.
 * Looped as it is, someone kneeling at a job stands up every five seconds. Whoever rests in it kneels down once,
 * then the clip runs its working middle forward and back, which has no seam. *ph is the hold's phase (0 not yet) */
#define KNEEL_IN 1.2f
#define KNEEL_OUT 3.9f
static void kneel_hold(actor_t *a, const char *clip, float *ph, float dt) {
  if (!a || !clip || strcmp(clip, "Fixing_Kneeling")) { *ph = 0; return; }
  float t = actor_clip_time(a);
  if (*ph <= 0) { if (t < KNEEL_IN || t > KNEEL_OUT) return; *ph = 1e-4f + (t - KNEEL_IN); }
  *ph += dt;
  float span = KNEEL_OUT - KNEEL_IN, u = fmodf(*ph, 2 * span);
  actor_clip_seek(a, KNEEL_IN + (u < span ? u : 2 * span - u));
}
static void update_actors(float dt) {
  for (int i = 0; i < WHO_COUNT; i++) {
    slot_t *s = &A[i];
    if (!s->a || !actor_is_visible(s->a)) continue;
    if (block == B_FIGHT && s->in_fight) continue;   /* fight.c drives its fighters */
    bool is_player = canon(W_ME) == i;
    if (s->w.on) {
      if (walk_step(&s->w, s->a, dt)) { actor_play(s->a, "Idle_Loop", 1, true, .2f); s->base = "Idle_Loop"; }
    } else if (s->turning) {
      float y = actor_yaw(s->a), e = ds_wrap_pi(s->turn_to - y);
      if (fabsf(e) < .03f) s->turning = false;
      else actor_move(s->a, actor_pos(s->a), y + e * fminf(1, dt * 6));
    }
    if (s->ex_t > 0) { s->ex_t -= dt; if (s->ex_t <= 0 && block != B_SAY) actor_expr_clear(s->a); }
    /* lip sync: the line being said, or a bark or banter */
    if ((block == B_SAY && say_who == i) || (sub_who == i && STORY_UI.sub_t > 0 && voice_busy())) { float o, sh; voice_mouth(&o, &sh); actor_mouth(s->a, o, sh); }
    else actor_mouth(s->a, 0, 0);
    if (!(STORY_UI.roam && is_player)) { kneel_hold(s->a, s->base, &s->kneel, dt); actor_update(s->a, dt); }
  }
}

/* ================================================================ extras (street region)
 * Owned with street.c: EX_MODEL, EX_PAL, EX_ALT, the X[] pool, extras_mode, extras_zone, spot_put,
 * extras_list and the helpers down to extras_look. Members at the benches by day, people on Main, the
 * Arduino Night crowd, the people inside a venue you walk into. Every one of them can be a stranger you
 * talk to: street.c casts a stranger (tools/script/street.mjs) onto the body a slot wears, this region
 * tells it who is near each frame, and holds a body still while it talks. extras_list feeds the
 * director's occluders, so nobody in the background walks into a lens. */
#define EXTRAS 10
typedef struct {
  actor_t *a; float x, z, dir, speed; int role; walk_t w; float t;
  int type; bool alt, held; const char *clip;   /* the stranger they play (-1 a passer by), which look, in a talk, the clip they rest in */
  float kneel;    /* kneel_hold's phase */
} extra_t;
static extra_t X[EXTRAS];
static int extras_mode = -1, extras_zone = -1;
static int extras_list(actor_t **L, int n) {
  for (int i = 0; i < EXTRAS; i++) if (X[i].a && actor_is_visible(X[i].a)) L[n++] = X[i].a;
  return n;
}     /* mode: 0 day, 1 Arduino Night, 2 night, 3 none; zone: 0 street, 1 lab, 2 inside a venue */
/* each extra its own body, none a named character's or a walk in's (actor.h: the background bodies),
 * adults first: the street uses the first nine, the lab by day eight, Arduino Night all ten. The bodies
 * carry their own colours, so the palettes only nudge a top or a pair of trousers. */
static const actor_model EX_MODEL[EXTRAS] = {AM_H2, AM_I2, AM_L2, AM_O2, AM_N2, AM_K2, AM_J2, AM_M2, AM_P2, AM_E};
static const actor_palette EX_PAL[EXTRAS] = {
  {{0, 1, 1}, {0, 1, 1}, {0, 1, 1}, {0, 1, 1}},
  {{0, 1, 1}, {30, 1, 1}, {0, 1, 1.1f}, {0, 1, 1}},
  {{0, 1, 1}, {0, 1, 1}, {0, 1, 1}, {0, 1, 1}},
  {{0, 1, 1}, {0, .9f, 1}, {-20, 1, 1}, {0, 1, 1}},
  {{0, 1, 1}, {0, 1, 1}, {0, 1, 1}, {0, 1, 1}},
  {{0, 1, 1}, {-40, 1, 1}, {0, 1, 1}, {0, 1, 1}},
  {{0, 1, 1}, {100, .8f, 1}, {0, 1, 1}, {0, 1, 1}},
  {{0, 1, 1}, {0, 1, 1}, {0, 1, 1}, {0, 1, 1}},
  {{0, 1, 1}, {150, 1, 1}, {0, 1, 1}, {0, 1, 1}},
  {{0, 1, 1}, {0, 1, 1}, {0, 1, 1}, {0, 1, 1}},
};
/* each body's second look (a stranger with look: 1): another top, darker or lighter trousers. One more
 * atlas per body at most, so the palette cache stays small. */
static const actor_palette EX_ALT[EXTRAS] = {
  {{0, 1, 1}, {190, .9f, .85f}, {0, 1, .7f}, {0, 1, 1}},
  {{0, 1, .8f}, {140, 1, 1}, {0, .6f, 1.25f}, {0, 1, 1}},
  {{0, 1, 1}, {200, .7f, 1}, {30, 1, .8f}, {0, 1, 1}},
  {{0, 1, 1}, {-70, 1, 1}, {0, 1, .75f}, {0, 1, 1}},
  {{0, 1, .9f}, {170, .8f, .9f}, {0, 1, 1.2f}, {0, 1, 1}},
  {{0, 1, 1}, {120, 1, .9f}, {0, .5f, 1}, {0, 1, 1}},
  {{0, 1, 1}, {-60, .9f, .9f}, {0, 1, .8f}, {0, 1, 1}},
  {{0, 1, 1}, {90, .7f, 1}, {0, 1, 1}, {0, 1, 1}},
  {{0, 1, 1}, {60, 1, 1}, {0, 1, .7f}, {0, 1, 1}},
  {{0, 1, 1}, {170, 1, 1}, {0, 1, 1}, {0, 1, 1}},
};
/* a spot in the lab: plan x, z, yaw and what they do there */
typedef struct { float x, z, yaw; const char *clip; } spot_t;
static const spot_t LAB_DAY[] = {
  {6.9f, 5.85f, 0, "Sitting_Idle_Loop"},          /* a coworking chair */
  {22.9f, 1.25f, 180, "Sitting_Idle_Loop"},        /* at the jewelry bench */
  {26.6f, 5.45f, 0, "Fixing_Kneeling"},            /* under the eight foot bench */
  {7.6f, 1.35f, 180, "Interact"},                  /* the printer rack */
  {21.2f, 4.9f, 45, "Idle_Talking_Loop"},          /* two talking by the electronics */
  {21.9f, 5.6f, -135, "Idle_Loop"},
  {18.9f, 8.0f, 0, "Interact"},                    /* the electronics bench */
  {30.2f, 7.9f, 0, "Interact"},                    /* the lathe */
};
static const spot_t LAB_NIGHT[] = {   /* Arduino Night: around the tables and the lounge */
  {11.2f, 3.6f, 90, "Idle_Talking_Loop"}, {12.6f, 4.9f, -60, "Idle_Loop"}, {14.4f, 2.8f, 45, "Fixing_Kneeling"},
  {9.6f, 5.0f, 180, "Idle_Talking_Loop"}, {16.8f, 3.4f, -60, "Idle_TalkingPhone_Loop"}, {9.2f, 4.2f, 0, "Idle_Loop"},
  {6.9f, 5.85f, 0, "Sitting_Talking_Loop"}, {18.9f, 8.0f, 0, "Interact"}, {13.4f, 4.3f, 150, "Idle_Talking_Loop"},
  {19.6f, 5.3f, -90, "Idle_Loop"},
};
static const spot_t LAB_LATE[] = { {6.9f, 5.85f, 0, "Sitting_Idle_Loop"}, {18.9f, 8.0f, 0, "Interact"} };
static void extras_make(void) {
  for (int i = 0; i < EXTRAS; i++) if (!X[i].a) {
    X[i].a = actor_new(EX_MODEL[i], &EX_PAL[i], ds_rr(.96f, 1.04f), scene);
    actor_visible(X[i].a, false);
    X[i].type = -1; X[i].alt = false;
  }
}
static void extras_reset(void) {
  for (int i = 0; i < EXTRAS; i++) { if (X[i].a) actor_visible(X[i].a, false); X[i].type = -1; X[i].held = false; street_uncast(i); }
  extras_mode = -1; extras_zone = -1;
  street_stop();
}
/* which stranger slot i plays here, and the look they wear for it */
static void extra_cast(int i, int where, unsigned seed) {
  extra_t *e = &X[i]; bool alt = false;
  e->type = street_cast(i, EX_MODEL[i], where, seed, &alt);
  if (e->type < 0) alt = false;
  if (alt != e->alt) { actor_repalette(e->a, alt ? &EX_ALT[i] : &EX_PAL[i]); e->alt = alt; }
}
static void spot_put(extra_t *e, const spot_t *s, int i) {
  actor_visible(e->a, true); e->w.on = false; e->role = 1;
  actor_place(e->a, world_lab(s->x, s->z), world_lab_yaw(s->yaw));
  actor_set_floor(e->a, 0);
  e->clip = sit_snap(e->a, s->clip);
  actor_play(e->a, e->clip, .9f + (i % 4) * .05f, true, 0);
}
static void extras_update(float dt) {
  if (!X[0].a) return;
  v3 cp = v3_(((t3_object *)cam)->position.x, 0, ((t3_object *)cam)->position.z);
  int mode = world_get_stage() != STAGE_TOWN || block == B_FIGHT ? 3 : hour < 18.9f ? 0 : hour < 21.8f ? 1 : 2;
  /* which side of the storefront the camera is on, with some give at the door; a venue's room is its own */
  int zone = extras_zone;
  if (interior_current() >= 0) zone = 2;
  else {
    if (zone == 2) zone = 0;
    if (cp.z < -6.5f && world_in_lab(cp)) zone = 1; else if (cp.z > -4.5f || !world_in_lab(cp)) zone = 0;
    if (zone < 0) zone = 0;
  }
  if (street_busy() && extras_mode >= 0) { mode = extras_mode; zone = extras_zone; }   /* a talk or a door keeps the scene as it is */
  if (mode != extras_mode || zone != extras_zone) {
    static unsigned layout; layout++;
    extras_mode = mode; extras_zone = zone;
    for (int i = 0; i < EXTRAS; i++) { actor_visible(X[i].a, false); X[i].role = 0; X[i].type = -1; X[i].held = false; street_uncast(i); }
    if (mode == 3) return;
    if (zone == 1) {
      const spot_t *S = mode == 0 ? LAB_DAY : mode == 1 ? LAB_NIGHT : LAB_LATE;
      int n = mode == 0 ? (int)(sizeof LAB_DAY / sizeof *LAB_DAY) : mode == 1 ? (int)(sizeof LAB_NIGHT / sizeof *LAB_NIGHT) : (hour < 23 ? 2 : 0);
      for (int i = 0; i < n && i < EXTRAS; i++) { extra_cast(i, SW_LAB, layout * 7 + i); spot_put(&X[i], &S[i], i); }
    } else if (zone == 2) {
      /* inside a venue: at its counter, its seats and tables (or spread out from the door), adults only */
      v3 P[EXTRAS]; float Y[EXTRAS]; bool behind[EXTRAS];
      int n = street_inside_spots(P, Y, behind, 5);
      static const int WHO[5] = {1, 2, 7, 6, 3};   /* I2 L2 M2 J2 O2: the counter first */
      for (int k = 0; k < n; k++) {
        int i = WHO[k]; extra_t *e = &X[i];
        extra_cast(i, SW_INSIDE, layout * 5 + k);
        e->role = 3; e->w.on = false; actor_visible(e->a, true); actor_place(e->a, P[k], Y[k]); actor_set_floor(e->a, P[k].y);
        e->clip = behind[k] ? "Idle_Loop" : k % 2 ? "Idle_Talking_Loop" : "Idle_TalkingPhone_Loop";
        actor_play(e->a, e->clip, .95f, true, 0);
      }
    } else {
      /* Main: walkers either way along both sidewalks, a few standing out front of the patios */
      int nw = mode == 0 ? 6 : mode == 1 ? 4 : 2, ns = mode == 0 ? 3 : 2;
      v3 d, n; float y;
      if (!world_place("lab_door", &d, &y)) d = v3_(0, 0, 2.4f);
      if (!world_place("nile", &n, &y)) n = v3_(0, 0, 35.6f);
      for (int i = 0; i < nw && i < EXTRAS; i++) {
        extra_t *e = &X[i];
        extra_cast(i, SW_STREET, layout * 3 + i);
        e->role = 2; e->dir = i & 1 ? 1 : -1; e->speed = ds_rr(1.0f, 1.45f); e->x = cp.x - 90 + i * 31 + ds_rr(-6, 6);
        e->z = i % 3 == 0 ? n.z - .4f : d.z + .9f + (i % 2) * .6f;
        e->clip = "Walk_Loop";
        actor_play(e->a, "Walk_Loop", e->speed / 1.3f, true, 0);
      }
      static const struct { const char *place; float dx, dz, yaw; const char *clip; } STAND[3] = {
        {"nile", -3.2f, -.6f, 90, "Idle_Talking_Loop"}, {"nile", -2.2f, -.4f, -90, "Idle_Loop"}, {"proof", 1.0f, .8f, 200, "Idle_TalkingPhone_Loop"} };
      for (int s = 0; s < ns && 6 + s < EXTRAS; s++) {
        int i = 6 + s; extra_t *e = &X[i]; v3 p; float yy;
        if (!world_place(STAND[s].place, &p, &yy)) continue;
        p = v3_add(p, v3_(STAND[s].dx, 0, STAND[s].dz));
        float f = world_floor_y(v3_(p.x, p.y + 1, p.z)); if (f > -1e8f) p.y = f;
        extra_cast(i, SW_STREET, layout * 3 + i);
        e->role = 3; e->clip = STAND[s].clip; actor_visible(e->a, true); actor_place(e->a, p, STAND[s].yaw * DS_PI / 180); actor_set_floor(e->a, p.y);
        actor_play(e->a, STAND[s].clip, .95f, true, 0);
      }
    }
  }
  street_people_begin();
  v3 pp = player_pos();
  for (int i = 0; i < EXTRAS; i++) {
    extra_t *e = &X[i];
    if (!e->role) continue;
    if (e->t > 0) { e->t -= dt; if (e->t <= 0) actor_look(e->a, actor_head_pos(e->a), 0); }
    /* in a talk: stop, turn to you (street.c plays the clips and the look); after it, back to what they were doing */
    if (street_holds(i)) {
      e->held = true;
      bool stands = e->role == 2 || (e->clip && !strncmp(e->clip, "Idle", 4));   /* a sitter or a worker only looks round */
      if (stands) { v3 p = actor_pos(e->a); float want = atan2f(pp.x - p.x, pp.z - p.z), y = actor_yaw(e->a); actor_move(e->a, p, y + ds_wrap_pi(want - y) * fminf(1, dt * 6)); }
      street_person(i, e->a, e->clip);
      actor_update(e->a, dt);
      continue;
    }
    if (e->held) {
      e->held = false;
      actor_look(e->a, actor_head_pos(e->a), 0);
      if (e->role == 2) { e->x = actor_pos(e->a).x; e->z = actor_pos(e->a).z; actor_play(e->a, "Walk_Loop", e->speed / 1.3f, true, .3f); }
      else if (e->clip) actor_play(e->a, e->clip, .95f, true, .3f);
    }
    if (e->role == 2) {
      e->x += e->dir * e->speed * dt;
      /* they stay around you: past 110 m they come back round the other way, as someone else */
      bool wrap = false;
      if (e->x - cp.x > 110) { e->x -= 220; wrap = true; } else if (e->x - cp.x < -110) { e->x += 220; wrap = true; }
      if (wrap) extra_cast(i, SW_STREET, (unsigned)(e->x * 13) + i);
      v3 p = v3_(e->x, 0, e->z);
      float f = world_floor_y(v3_(p.x, 1.2f, p.z));
      if (f > -1e8f) p.y = f;
      bool near = fabsf(e->x - cp.x) < 70;
      actor_visible(e->a, near);
      actor_move(e->a, p, e->dir > 0 ? DS_PI / 2 : -DS_PI / 2); actor_set_floor(e->a, p.y);
      if (!near) continue;
    } else if (flat_dist(actor_pos(e->a), cp) > 60) continue;
    if (e->type >= 0 && flat_dist(actor_pos(e->a), pp) < STREET_NEAR) street_person(i, e->a, e->clip);
    kneel_hold(e->a, e->clip, &e->kneel, dt);
    actor_update(e->a, dt);
  }
}

/* something happened on the street: the people out there look round at it for a moment */
static void extras_look(v3 p) {
  for (int i = 0; i < EXTRAS; i++) {
    extra_t *e = &X[i];
    if (e->role < 2 || e->held || flat_dist(actor_pos(e->a), p) > 35) continue;
    actor_look(e->a, v3_(p.x, p.y + 1.6f, p.z), .9f); e->t = 3.5f;
  }
}
/* ================================================================ end of the extras (street region) */
static void project(v3 w, bool *behind, float *x, float *y) {
  t3_object_update_world_matrix(cam, true, false);
  const float *m = ((t3_object *)cam)->matrix_world.e;
  v3 r = v3_(m[0], m[1], m[2]), u = v3_(m[4], m[5], m[6]), f = v3_(-m[8], -m[9], -m[10]), c = v3_(m[12], m[13], m[14]);
  v3 d = v3_sub(w, c);
  float z = v3_dot(d, f);
  *behind = z <= .05f;
  float t = tanf(cam->fov * DS_PI / 360);
  float sx = v3_dot(d, r) / fmaxf(.05f, fabsf(z)) / (t * (float)DS_W / DS_H), sy = v3_dot(d, u) / fmaxf(.05f, fabsf(z)) / t;
  *x = DS_W * (.5f + sx * .5f); *y = DS_H * (.5f - sy * .5f);
}

#ifdef KH_AUTO
/* the whole day headless: choices by seed, walks by teleport, tasks done on the spot, fights won after a moment */
static float auto_t; static int auto_seed = 3;
#ifdef KH_AUTO_SLOW
#define AUTO_STEP ((float)KH_AUTO_SLOW)
#else
#define AUTO_STEP 1.2f
#endif
static void auto_log(const char *what, const char *s) { ds_log("[auto] pc=%d %.1fh %s %s", pc, hour, what, s ? s : ""); }
#endif

/* ---- v1.3 the wayfinder: the marker keeps one target until it is done (or another task is started), says what
 * it is ("Soda fridge 12 m", "Pick up: Kitchen"), and indoors follows the walking route round shelves and walls
 * (A* on the nav grid, refreshed twice a second), aiming at the next corner. Between the lab and the street it
 * aims at the door to go through first. ---- */
static char wf_label[96];
static const struct { const char *id, *name; } WHERE[] = {
  {"lab_welcome", "Welcome board"}, {"lab_laser", "Laser"}, {"lab_laser_hood", "Laser hood"}, {"lab_kitchen", "Kitchen"},
  {"lab_cowork", "Coworking tables"}, {"lab_shop", "Woodshop"}, {"lab_entry", "Front door"}, {"lab_signin", "Sign-in tablet"},
  {"lab_printer", "Printers"}, {"lab_soda", "Soda fridge"}, {"lab_pegboard", "Pegboard"}, {"lab_toolwall", "Tool wall"},
  {"lab_elec", "Electronics bench"}, {"lab_lounge", "Lounge"}, {"lab_couch", "Couch"}, {"lab_fab", "Fab bay"},
  {"lab_scrap", "Scrap shelves"}, {"lab_classroom", "Classroom"}, {"lab_fridge", "Kitchen fridge"}, {"lab_bike", "Bike station"},
  {"lab_teardown", "Tear Apart bin"}, {"lab_clamps", "Clamp rack"}, {"lab_door", "Front door"}, {"lab_back_door", "Back door"},
  {"lab_lot", "Back lot"}, {"lot_fight", "Back lot"}, {"mid_block", "Main Street"}, {"macdonald_main", "Macdonald and Main"},
  {"center_main_station", "Center and Main station"}, {"proof", "Proof Bread"}, {"nile", "The Nile"}, {"level1", "Level 1 Arcade"},
  {"against_grain", "Against the Grain"}, {"city_plaza", "City Plaza"}, {"arts_center", "Arts Center"},
};
static const char *where_of(const sc_mark *m) {
  if (m->kind != MK_PLACE || !m->name) return NULL;
  for (unsigned i = 0; i < sizeof WHERE / sizeof WHERE[0]; i++) if (!strcmp(WHERE[i].id, m->name)) return WHERE[i].name;
  return NULL;
}
static const char *wf_task_label(int i) {
  const sc_task *k = task_def(i);
  const char *a = k->where ? k->where : where_of(&k->mark), *b = k->where2 ? k->where2 : where_of(&k->mark2);
  if (k->kind == TK_CARRY) {
    bool hold = tcur == i && tphase == TP_PLAY && T[i].holding;
    snprintf(wf_label, sizeof wf_label, hold ? "Drop off: %s" : "Pick up: %s", hold ? (b ? b : "there") : (a ? a : task_label(i)));
    return wf_label;
  }
  if (k->kind == TK_GREET && k->who >= 0 && !a) a = "Front door";
  return a ? a : task_label(i);
}
/* the two doors of the lab: where to stand inside and outside each */
static void wf_doors(v3 in[2], v3 out[2]) {
  float y;
  if (!place_of("lab_entry", &in[0], &y)) in[0] = world_lab(6.25f, 3.45f);
  if (!place_of("lab_door", &out[0], &y)) out[0] = v3_(0, 0, 2.4f);
  in[1] = world_lab(39.4f, 7.1f);
  if (!place_of("lab_back_door", &out[1], &y)) out[1] = v3_(3.7f, 0, -42.5f);
}
/* where the marker should sit now on the way to dest, and the walk left */
static v3 wf_aim(v3 dest, float dt, float *left) {
  v3 p = player_pos(), via = dest;
  bool town = world_get_stage() == STAGE_TOWN, pin = town && world_in_lab(p), din = town && world_in_lab(dest);
  float extra = 0;
  if (town && pin != din) {
    v3 di[2], dq[2]; wf_doors(di, dq);
    int best = 0; float bd = 1e9f;
    for (int d = 0; d < 2; d++) { float c = pin ? flat_dist(p, di[d]) + flat_dist(dq[d], dest) : flat_dist(p, dq[d]) + flat_dist(di[d], dest); if (c < bd) { bd = c; best = d; } }
    via = pin ? di[best] : dq[best];
    v3 other = pin ? dq[best] : di[best];
    if (flat_dist(p, via) < 1.6f) via = other;   /* at the door: through it */
    extra = flat_dist(via, dest);
  }
  /* indoors: the route, when the straight line is blocked */
  bool route = pin && world_in_lab(via);
  WF.t -= dt;
  if (!route) { WF.n = 0; *left = flat_dist(p, via) + extra; return via; }
  bool stale = WF.t <= 0 || flat_dist(WF.via, via) > .6f || (WF.n && WF.k < WF.n && flat_dist(p, WF.wp[WF.k]) > 6);
  if (stale) {
    WF.via = via; WF.t = .5f; WF.k = 0;
    WF.n = nav_line(p, via) ? 0 : nav_path(p, via, WF.wp, WPN);
  }
  while (WF.k < WF.n && flat_dist(p, WF.wp[WF.k]) < 1.f) WF.k++;
  if (WF.k < WF.n - 1 && nav_line(p, WF.wp[WF.k + 1])) WF.k++;   /* pulled tight as the player cuts the corner */
  if (WF.k < WF.n && nav_line(p, via)) WF.k = WF.n;
  float len = 0; v3 q = p;
  for (int i = WF.k; i < WF.n; i++) { len += flat_dist(q, WF.wp[i]); q = WF.wp[i]; }
  len += flat_dist(q, via);
  *left = len + extra;
  return WF.k < WF.n ? WF.wp[WF.k] : via;
}
/* the target: the task in hand, else the one already marked while it is open, else a required one before an
 * optional one and the nearest of those (a visitor waiting at the door counts as near). Returns the task or -1. */
static int wf_frame(bool open, float dt) {
  story_ui_t *U = &STORY_UI;
  int tgt = -1;
  if (tcur >= 0 && tphase == TP_PLAY) tgt = tcur;
  else if (WF.task >= 0 && WF.task < tn && T[WF.task].st == TS_OPEN && !(open && task_def(WF.task)->optional && roam_has_place)) tgt = WF.task;
  else {
    float td = 1e9f; bool treq = false;
    for (int i = 0; i < tn; i++) if (T[i].st == TS_OPEN) {
      bool req = !task_def(i)->optional;
      if (open && !req && roam_has_place) continue;   /* the work is done: the way on, the rest as grey pins */
      float d = flat_dist(task_point(i), player_pos());
      if (task_def(i)->kind == TK_GREET && T[i].arrived) d *= .25f;
      if ((req && !treq) || (req == treq && d < td)) { tgt = i; td = d; treq = req; }
    }
  }
  WF.task = tgt;
  for (int i = 0; i < U->ntask; i++) U->task[i].target = false;
  v3 dest; const char *label;
  if (tgt >= 0) {
    dest = task_point(tgt); label = wf_task_label(tgt);
    int r = 0; for (int i = 0; i < tn; i++) if (T[i].st != TS_HIDDEN) { if (i == tgt && r < UI_TASKS) U->task[r].target = true; r++; }
  } else if (open && roam_has_place) {
    dest = roam_pos; label = roam->s2 ? roam->s2 : where_of(&(sc_mark){MK_PLACE, roam->s, 0, 0, 0});
  } else return -1;
  float left; v3 aim = wf_aim(dest, dt, &left);
  bool beh; float mx, my; project(v3_add(aim, v3_(0, 1.4f, 0)), &beh, &mx, &my);
  U->marker_on = true; U->marker_behind = beh; U->marker_x = mx; U->marker_y = my; U->marker_dist = left; U->marker_task = tgt >= 0;
  U->marker_label = label; U->marker_corner = flat_dist(aim, dest) > .5f;
  return tgt;
}

/* the roam: free walking, side talks, tasks, the way on */
static void roam_frame(float dt, const intent_t *in) {
  story_ui_t *U = &STORY_UI;
  gear_use_first(in);
  if (tcur >= 0 && tphase == TP_PRE) { tphase = TP_PLAY; mg_setup(tcur); if (block != B_ROAM) return; }
  if (tcur >= 0 && tphase == TP_POST) { tcur = -1; tphase = TP_NONE; }
  roam_t += dt;
  tasks_tick(dt);
  actor_t *me = actor_of(W_ME);
  /* a minigame that holds the player still */
  if (tcur >= 0 && tphase == TP_PLAY && locked_mg(U->mg)) {
    int r = mg_run(dt, in);
    kneel_hold(me, U->mg == MG_MASH || U->mg == MG_STICK ? "Fixing_Kneeling" : NULL, &me_kneel, dt);
    actor_update(me, dt);
    U->npin = 0;
    if (r > 0) { task_finish(tcur, 2); return; }
    if (r < 0) { U->mg = MG_NONE; tcur = -1; tphase = TP_NONE; actor_play(me, "Idle_Loop", 1, true, .2f); player_camera_snap(); }
    return;
  }
  /* carrying slows you down to a careful walk */
  intent_t walk_in = *in;
  bool carrying = tcur >= 0 && tphase == TP_PLAY && task_def(tcur)->kind == TK_CARRY && T[tcur].holding;
  if (carrying) { walk_in.mx *= .62f; walk_in.my *= .62f; walk_in.sprint = false; walk_in.jump = false; }
  player_update(dt, &walk_in);
  if (carrying) {
    if (ovr_clip && !strcmp(ovr_clip, "PickUp_Table") && ovr_t < .75f) ovr_apply(me, dt);
    else {
      float sp = flat_dist(player_pos(), last_ppos) / fmaxf(dt, 1e-4f);
      if (!ovr_clip || strcmp(ovr_clip, "Walk_Carry_Loop")) { ovr_clip = "Walk_Carry_Loop"; ovr_t = .1f; ovr_loop = true; }
      actor_play(me, ovr_clip, 1, true, 0);
      float g = actor_clip_ground_speed(me);
      ovr_rate = sp > .15f && g > .05f ? ds_clamp(sp / g, .4f, 2) : 0;
      ovr_apply(me, dt);
    }
  } else if (ovr_clip) {
    /* a gesture over the walk: it ends with its clip, or when the player moves off */
    ovr_apply(me, dt);
    if (ovr_t >= actor_clip_len(me) - .03f || fabsf(in->mx) + fabsf(in->my) > .3f) ovr_clip = NULL;
  }
  last_ppos = player_pos();
  U->mg_t += dt;
  /* someone following along: walk after the player */
  if (roam_follow >= 0 && A[roam_follow].a) follow_player(A[roam_follow].a, &A[roam_follow].w, dt, 1.5f);
  /* the door opens for people walking in */
  {
    bool near = false;
    for (int v = 0; v < NVIS; v++) if (VIS[v].a && VIS[v].w.on && hypotf(actor_pos(VIS[v].a).x, actor_pos(VIS[v].a).z + 5.45f) < 2.6f) near = true;
    for (int i = 0; i < WHO_COUNT; i++) if (A[i].a && A[i].w.on && hypotf(actor_pos(A[i].a).x, actor_pos(A[i].a).z + 5.45f) < 2.6f) near = true;
    if (near) { world_door(1); door_forced = true; } else if (door_forced) { world_door_auto(true); door_forced = false; }
  }
  /* banter: who says what on the way, one at a time */
  if (U->sub_t <= 0 && !voice_busy()) {
    for (int k = 0; k < roam->nk && k < 48; k++) if (!banter_done[k]) {
      const sc_banter *bt = &BANTER[roam->k0 + k];
      bool ok = roam_t >= bt->delay;
      if (bt->mark.kind != MK_NONE) { sc_mark m = bt->mark; ok = ok && flat_dist(resolve(&m, NULL, NULL), player_pos()) < 4.5f; }
      if (ok) { banter_done[k] = true; sub_say(bt->who, bt->text); break; }
    }
  }
  /* what is in reach: a task, a side talk, the way on */
  int near_task = -1; float nd = 1e9f;
  for (int i = 0; i < tn; i++) {
    if (T[i].st != TS_OPEN || (tcur >= 0 && tcur != i)) continue;
    float d = flat_dist(task_point(i), player_pos());
    if (task_def(i)->kind == TK_POINT) continue;
    float reach = task_def(i)->kind == TK_CARRY ? 1.5f : 1.9f;
    if (d < reach && d < nd) { nd = d; near_task = i; }
  }
  side_near = -1;
  if (near_task < 0 && tcur < 0) for (int i = 0; i < roam->i2 && i < 8; i++) if (!side_done[i]) {
    const sc_side *sd = &SIDES[roam->i + i];
    slot_t *s = &A[canon(sd->who)];
    if (s->a && actor_is_visible(s->a) && v3_len(v3_sub(actor_pos(s->a), player_pos())) < 2.6f) { side_near = i; U->prompt_use = sd->prompt; break; }
  }
  if (near_task >= 0) {
    U->prompt_use = task_prompt(near_task);
    if (in->interact && !(tcur == near_task && T[near_task].holding && actor_clip_len(me) > 0 && U->mg_t < .4f)) { U->mg_t = 0; task_use(near_task); if (block != B_ROAM) return; }
  } else if (side_near >= 0 && in->interact) {
    const sc_side *sd = &SIDES[roam->i + side_near];
    side_done[side_near] = true;
    call_beats(sd->start);
    face_now(W_ME, sd->who); face_now(sd->who, W_ME);
    return;
  }
  /* the roped-off attic stairs: a member warns you off going up alone, but you can go up anyway (world.c
   * blocks the climb; the minigame warps you up). Only when nothing else is in reach. */
  if (near_task < 0 && side_near < 0 && tcur < 0 && world_get_stage() == STAGE_TOWN) {
    float sd = flat_dist(player_pos(), v3_(0.35f, 0, -31.4f));
    if (sd < 4.0f && !attic_warned && !attic_done) {
      attic_warned = true;
      int w = roam_follow >= 0 ? roam_follow : canon(W_M1);
      sub_say(w, "Hey, don't go up there alone. That floor's soft as cake since the leak.");
    }
    if (sd < 2.0f) {
      U->prompt_use = attic_done ? "UP TO THE ATTIC" : "GO UPSTAIRS";
      if (in->interact) { attic_want = true; return; }
    }
  }
  /* pointing a station out: the nearest open tour stop in range, aimed at with the camera */
  U->mg_anchor = false; U->mg_aim = false;
  if (U->mg == MG_POINT) U->mg = MG_NONE;
  if (tcur < 0 && near_task < 0) {
    int pi = -1; float pd = 6;
    for (int i = 0; i < tn; i++) if (T[i].st == TS_OPEN && task_def(i)->kind == TK_POINT) { float d = flat_dist(player_pos(), T[i].to); if (d < pd) { pd = d; pi = i; } }
    if (pi >= 0) {
      v3 tg = v3_add(T[pi].to, v3_add(v3_scale(fwd_of(T[pi].to_yaw), .9f), v3_(0, 1.05f, 0)));
      bool beh; float x, y; project(tg, &beh, &x, &y);
      U->mg = MG_POINT; U->mg_title = task_label(pi);
      U->mg_anchor = !beh; U->mg_anchor_behind = beh; U->mg_ax = x; U->mg_ay = y;
      U->mg_aim = !beh && fabsf(x - DS_W / 2) < 110 && fabsf(y - DS_H * .45f) < 160;
      U->prompt_use = U->mg_aim ? task_prompt(pi) : NULL;
      if (in->interact && U->mg_aim) { U->mg = MG_NONE; task_use(pi); if (block != B_ROAM) return; }
    }
  }
  /* greet: the look up window over the visitor's head */
  if (U->mg == MG_NONE || U->mg == MG_GREET) {
    U->mg = MG_NONE;
    for (int i = 0; i < tn; i++) if (T[i].st == TS_OPEN && task_def(i)->kind == TK_GREET && T[i].arrived && tcur < 0) {
      actor_t *a = task_actor(i); if (!a) continue;
      bool beh; float x, y; project(v3_add(actor_head_pos(a), v3_(0, .42f, 0)), &beh, &x, &y);
      if (beh || flat_dist(actor_pos(a), player_pos()) > 18) continue;
      U->mg = MG_GREET; U->mg_window = T[i].win; U->mg_ax = x; U->mg_ay = y; U->mg_anchor = true; U->mg_title = task_label(i);
      break;
    }
  }
  /* the marker (wf_frame): one target at a time, held until it is done; then the way on */
  U->npin = 0;
  bool open = gate_open();
  if (open) gate_t += dt; else gate_t = 0;
  int tgt = wf_frame(open, dt);
  /* grey pins on the other open tasks nearby */
  for (int i = 0; i < tn && U->npin < UI_PINS; i++) if (T[i].st == TS_OPEN && i != tgt && tcur < 0) {
    v3 p = task_point(i); if (flat_dist(p, player_pos()) > 25) continue;
    bool beh; float x, y; project(v3_add(p, v3_(0, 1.5f, 0)), &beh, &x, &y);
    if (beh || x < 0 || x > DS_W || y < 0 || y > DS_H) continue;
    U->pin_x[U->npin] = x; U->pin_y[U->npin] = y; U->pin_behind[U->npin] = false; U->npin++;
  }
  /* the roam ends once the work is done: when the player gets to the place (a generous circle, no button; for
   * the lab's doors, stepping through counts), or by itself a little after the work for a roam whose walk adds
   * nothing (roam->target), or at once if there is no place */
  bool end = false;
  if (open && roam_has_place && tcur < 0) {
    snprintf(goal_buf, sizeof goal_buf, "%s", roam->s2 ? roam->s2 : obj_buf[0] ? obj_buf : "Go on");
    U->objective = goal_buf;
  }
  if (tcur < 0 && open && gate_t > .6f) {
    if (!roam_has_place) end = true;
    else {
      v3 pp = player_pos();
      float R = fmaxf(roam->f, 2.5f) + (world_in_lab(roam_pos) ? .3f : 2.f);
      bool there = flat_dist(roam_pos, pp) < R;
      /* an outdoor roam to one of the lab's doors also arrives when the player steps inside */
      bool town = world_get_stage() == STAGE_TOWN && !roam->flag;
      if (town && roam->s && !strcmp(roam->s, "lab_door") && world_in_lab(pp)) there = true;
      if (town && roam->s && !strcmp(roam->s, "lab_back_door") && world_in_lab(pp) && pp.z < -30) there = true;
      if (there && near_task < 0) end = true;
      if (roam->target > 0 && gate_t > roam->target / 10.f) end = true;
    }
  }
  if (end) {
#ifdef KH_AUTO
    ds_log("[auto] roam %s ends: %d of %d done, %.1f s after, %.1f m off", roam->s ? roam->s : "-", done_n(), tn, gate_t, roam_has_place ? flat_dist(roam_pos, player_pos()) : 0.f);
#endif
    block = B_NONE;
    U->objective = obj_buf[0] ? obj_buf : NULL;
    WF.task = -1;
    /* leave the unused side people where they stand no longer */
    for (int i = 0; i < roam->i2 && i < 8; i++) { const sc_side *sd = &SIDES[roam->i + i]; slot_t *s = &A[canon(sd->who)]; if (s->a) actor_visible(s->a, false); }
    memset(side_done, 0, sizeof side_done);
    tasks_end();
    if (roam->f3 > 0) hour_target = roam->f3;
    if (door_forced) { world_door_auto(true); door_forced = false; }
    actor_play(me, "Idle_Loop", 1, true, .2f);
    last_speaker = -1;
  }
}

/* ---- v1.2 roam gear: the drawn weapon and the bike ----
 * Kept here, apart from the roam itself: input in, the player's weapon and ride out, the HUD's hints. A weapon
 * is drawn and sheathed at will (input draw) and swung at nothing; the bike is unlocked at the lab's bike
 * station and then summoned anywhere outdoors (input bike). Talking, a task, a scene or a fight puts both
 * away first. Effects and weapon ribbons age here outside fights (fight.c ages them during one). */
static int gear_wp = -1;
static bool gear_was_roam, gear_prompt_was;
static actor_t *gear_me(void) { return actor_of(W_ME); }
static bool gear_drawn(void) { actor_t *m = gear_me(); return m && weapon_of(m) >= 0; }
static void gear_draw(int w) {
  actor_t *m = gear_me();
  weapon_equip(m, w); weapon_draw_flash(m); sfx(SFX_SUMMON, 0, .75f);
}
static void gear_sheathe(void) {
  actor_t *m = gear_me();
  if (!m || weapon_of(m) < 0) return;
  v3 b, t; uint32_t col = weapon_color(weapon_of(m));
  if (weapon_points(m, 0, &b, &t)) { fx_streak(b, t, col, .08f, .2f); fx_motes(t, col, 6, 1.2f); }
  weapon_equip(m, -1); weapon_trail(m, false);
  sfx(SFX_SHEATHE, 0, .7f);
}
static void gear_stow(void) { if (player_riding()) player_ride(false); gear_sheathe(); }
/* a press that talks or uses: put the gear away before it happens */
static void gear_use_first(const intent_t *in) {
  if (in->interact && gear_prompt_was && (gear_drawn() || player_riding())) gear_stow();
}
static int gear_lowest(void) { for (int w = 0; w < WP_COUNT; w++) if (weapons & (1u << w)) return w; return -1; }
static void gear_frame(float dt, const intent_t *in) {
  story_ui_t *U = &STORY_UI;
  if (block != B_FIGHT) { fx_update(dt, cam); weapon_update(dt); }
  bool roaming = block == B_ROAM && roam;
  if (!roaming) {
    /* a scene, a talk or a fight begins: off the bike (it goes away) and the weapon put up */
    /* (the roam ends inside roam_frame, so this runs before the next beat can draw anything for the scene) */
    if (gear_was_roam) { gear_stow(); if (!bike_scene) bike_show(false); }
    gear_was_roam = false; gear_prompt_was = false;
    U->gear_wp = NULL; U->gear_bike = U->gear_riding = U->gear_drawn = false;
    return;
  }
  bike_init(scene);
  actor_t *me = gear_me();
  v3 p = player_pos();
  bool lab = world_in_lab(p), outdoors = world_get_stage() == STAGE_TOWN && !world_indoors(p);   /* the lab, or a room behind a door */
  if (!gear_was_roam) {
    gear_was_roam = true;
#ifdef KH_GEAR_TEST
    ds_log("[gear] roam %s at frame %d, %s", roam->s ? roam->s : "", (int)(ds_now() * 60 + .5), lab ? "lab" : "outdoors");
    /* a test run: every weapon, and a lab roam starts a few steps from the bike station */
    weapons |= (1u << WP_COUNT) - 1;
    if (roam->s && !strcmp(roam->s, "lab_door")) player_teleport(v3_(.3f, 0, 7.f), DS_PI);   /* facing the front door, for a ride in */
    { static bool once; v3 bp; float by; if (!once && lab && lab_place("lab_bike", &bp, &by)) { once = true; player_teleport(v3_sub(bp, v3_scale(fwd_of(by), 3.f)), by); } }
#endif
  }
#if defined(KH_GEAR_TEST) && defined(KH_AUTO)
  /* hold the autoplay off the first lab roam (25 s) and every outdoor one (45 s), for a test to drive */
  { static float held; static int nroam; static const sc_beat *last;
    if (roam != last) { last = roam; held = 0; nroam++; }
    held += dt;
    if ((outdoors && held < 45) || (nroam == 1 && held < 25)) auto_t = 0; }
#endif
  bool busy = tcur >= 0 || (U->mg && U->mg != MG_GREET && U->mg != MG_POINT);
  /* which weapon: the one the story last drew, else the first owned; 1 to 5 or the d-pad change it (the wheel zooms) */
  if (weapons && (gear_wp < 0 || !(weapons & (1u << gear_wp)))) {
    int sw = A[canon(W_ME)].weapon;
    gear_wp = sw >= 0 && (weapons & (1u << sw)) ? sw : gear_lowest();
  }
  if (!weapons) gear_wp = -1;
  if (gear_wp >= 0 && !busy) {
    int pick = in->weapon_pick >= 0 && (weapons & (1u << in->weapon_pick)) ? in->weapon_pick : -1;
    int step = in->weapon_step;
    if (pick < 0 && step) for (int k = 1; k <= WP_COUNT; k++) { int w = ((gear_wp + step * k) % WP_COUNT + WP_COUNT) % WP_COUNT; if (weapons & (1u << w)) { pick = w; break; } }
    if (pick >= 0 && pick != gear_wp) { gear_wp = pick; if (gear_drawn()) gear_draw(gear_wp); else sfx(SFX_WEAPON, 0, .5f); }
  }
  /* work in hand puts both away */
  if (busy) gear_stow();
  if (in->draw && gear_wp >= 0 && !busy) {
    if (gear_drawn()) gear_sheathe();
    else { if (player_riding()) player_ride(false); gear_draw(gear_wp); }
  }
  if (gear_drawn() && !busy && !U->prompt_use && !player_riding() && me) {
    if (in->heavy) player_swing(true); else if (in->light) player_swing(false);
  }
  /* the bike */
  if (in->bike && gear_bike_have && !busy) {
    if (player_riding()) player_ride(false);
    else if (outdoors) { gear_sheathe(); player_ride(true); }
    else { U->note = "The bike is for outside. Ride it once you are out the door."; U->note_t = 2.5f; sfx(SFX_UI, 0, .6f); }
  }
  if (!gear_bike_have) {
    /* the bike station: the first walk up to it takes the lab's loaner bike for the day */
    v3 bp; float by;
    if (lab && lab_place("lab_bike", &bp, &by)) {
      v3 stand = v3_add(world_lab(17.5f, 6.55f), v3_(0, 0, 0));
      float d = flat_dist(bp, p);
      if (d < 20) { if (!bike_shown()) bike_show(true); bike_pose(stand, world_lab_yaw(-90), .08f, .3f, 0, 0); }
      if (d < 1.8f && !U->prompt_use && !busy) {
        bool take = in->interact;
#ifdef KH_AUTO
        take = input_get()->interact;   /* the autoplay hides the use button from the story; a test drives it */
#endif
        U->prompt_use = "Take the bike";
        if (take) {
          gear_bike_have = true; bike_show(false); bike_sync();
          U->note = "The lab's loaner bike is yours for the day. Outdoors, call it up any time."; U->note_t = 4;
          sfx(SFX_BIKE_BELL, 0, .8f); sfx(SFX_PICKUP, 0, .7f);
          gesture("Interact", 1.2f);
        }
      }
    } else if (bike_shown() && !player_riding()) bike_show(false);
  } else if (bike_shown() && !player_riding() && flat_dist(bike_pos(), p) > 70) bike_show(false);   /* left far behind */
  U->gear_wp = gear_wp >= 0 ? weapon_name((weapon_id)gear_wp) : NULL;
  U->gear_drawn = gear_drawn();
  U->gear_riding = player_riding();
  U->gear_bike = gear_bike_have && (outdoors || U->gear_riding);
  gear_prompt_was = U->prompt_use != NULL;
}

void story_update(float dt, const intent_t *in) {
  story_ui_t *U = &STORY_UI;
#ifdef KH_AUTO
  { static intent_t fake; fake = *in; fake.ok = false; fake.interact = false; in = &fake;
    auto_t += dt;
    if (block == B_SAY && say_t > fminf(1.5f, say_len)) { voice_stop(); block = B_NONE; U->talking = false; }
    if (block == B_CHOICE && U->choose_t > .8f) { U->sel = (auto_seed++ * 7) % U->nopt; fake.ok = true; auto_log("choice", U->opt[U->sel]); }
    if (block == B_ROAM && tcur >= 0 && tphase == TP_PLAY) {
      /* slow runs show the minigame part way, for frames */
      float f = fminf(1, auto_t / AUTO_STEP);
      if (U->mg == MG_MASH || U->mg == MG_HOLD || U->mg == MG_STICK) { U->mg_v = f * .8f; U->mg_angle = f * .8f * DS_PI * 5; }
      if (U->mg == MG_SIGNIN) U->mg_step = (int)(f * U->mg_nseq * .8f);
      if (U->mg == MG_TIMING) U->mg_hits = (int)(f * 2.5f);
    }
    if (block == B_ROAM && auto_t > AUTO_STEP) {
      static int point_wait = -1;
      auto_t = 0;
      if (tcur >= 0 && tphase == TP_PLAY) {
        const sc_task *k = task_def(tcur);
        if (k->kind == TK_CARRY) {
          int i = tcur; int n = k->n > 0 ? k->n : 1;
          if (AUTO_STEP > 2) {   /* a trip a step, walked by teleport */
            if (!T[i].holding) { player_teleport(v3_add(box_spot(i), v3_scale(fwd_of(T[i].at_yaw), -1.2f)), T[i].at_yaw); task_use(i); auto_log("task pick up", k->name); }
            else { player_teleport(v3_add(T[i].to, v3_scale(fwd_of(T[i].to_yaw), -1.5f)), T[i].to_yaw); task_use(i); auto_log("task put down", k->name); }
          } else {
            while (tcur == i && T[i].trips < n) { if (!T[i].holding) task_use(i); task_use(i); }
            auto_log("task carried", k->name);
          }
        } else { auto_log("task played", k->name); task_finish(tcur, 2); }
      } else {
        int pick = -1;
        for (int i = 0; i < tn; i++) if (T[i].st == TS_OPEN) { pick = i; break; }
        if (pick >= 0) {
          const sc_task *k = task_def(pick);
          if (k->kind == TK_GREET && !T[pick].arrived) {
            actor_t *a = task_actor(pick); walk_t *w = task_walk(pick);
            if (AUTO_STEP > 2) { player_teleport(v3_add(T[pick].at, v3_scale(fwd_of(T[pick].at_yaw), 4)), T[pick].at_yaw + DS_PI); auto_log("task waits for", k->name); goto auto_done; }
            if (a && w && w->on) { w->on = false; actor_place(a, T[pick].at, T[pick].at_yaw); }
            T[pick].arrived = true;
          }
          if (k->kind == TK_POINT) {
            player_teleport(v3_add(T[pick].to, v3_scale(fwd_of(T[pick].to_yaw), -2.6f)), T[pick].to_yaw);
            if (AUTO_STEP > 2 && point_wait != pick) { point_wait = pick; auto_log("task aims", k->name); goto auto_done; }
          } else {
            /* through the door, as a player would: a task at a counter inside a room enters the room (street) */
            v3 q = task_point(pick); int r = street_room_of(q);
            if (r >= 0 && interior_current() != r) { interior_enter(r); auto_log("enters", interior_id(r)); }
            player_teleport(r >= 0 ? q : v3_add(q, v3_(.8f, 0, .3f)), 0);
          }
          auto_log("task", k->name);
          task_use(pick);
        } else if (gate_open() || !roam_has_place) { if (interior_current() >= 0) interior_exit(); player_teleport(roam_pos, 0); fake.interact = true; auto_log("roam", roam->s); }
      }
      auto_done: ;
    }
    if (block == B_FIGHT && auto_t > KH_AUTO) { fight_end(); for (int k = 0; k < WHO_COUNT; k++) A[k].in_fight = false; fight_won(); actor_play(actor_of(W_ME), "Idle_Loop", 1, true, .3f); auto_log("fight won", FD.title); block = B_NONE; }
    if (block == B_FIGHT && U->retry) U->retry = false;
    if (block != B_ROAM && block != B_FIGHT) auto_t = 0;
    if (block == B_EPILOGUE || block == B_DONE) { static int once; if (!once++) { auto_log("END", story_end_title()); for (int i = 0; i < ST_COUNT; i++) ds_log("stat %d = %d", i, U->stats[i]); } }
  }
#endif
  /* the clock eases, so a time jump sweeps the sky */
  if (fabsf(hour - hour_target) > .001f) {
    hour = ds_damp(hour, hour_target, 1.2f, dt);
    if (fabsf(hour - hour_target) < .002f) hour = hour_target;
    world_set_time(hour);
  }
  U->hour = hour;
  step();
  U->roam = block == B_ROAM;
  U->cine = !(block == B_ROAM || block == B_FIGHT || block == B_EPILOGUE);
  U->letterbox = ds_damp(U->letterbox, U->cine ? 1 : 0, 7, dt);
  U->fade += ds_clamp(fade_to - U->fade, -fade_rate * dt, fade_rate * dt);
  U->flash = fmaxf(0, U->flash - flash_rate * dt);
  U->dim = ds_damp(U->dim, dim_to, 3, dt);
  if (U->chapter) { U->ch_t += dt; if (U->ch_t > U->ch_dur) U->chapter = false; }
  if (U->card) { U->card_t += dt; if (U->card_t > U->card_dur) U->card = false; }
  /* a note waits out a chapter card (the HUD shows it after the card, never over it) */
  if (U->note_t > 0 && !U->chapter) { U->note_t -= dt; if (U->note_t <= 0) U->note = NULL; }
  if (U->toast_t > 0) U->toast_t -= dt;
  if (U->got_t > 0) U->got_t -= dt;
  if (U->sub_t > 0) { U->sub_t -= dt; if (U->sub_t <= 0) { U->sub_text = NULL; sub_who = -1; } }
  if (bark_t > 0) bark_t -= dt;
  U->marker_on = false; U->prompt_use = NULL;

  /* hold START: skip to the next choice, fight or walk; and skip the orientation tutorial fight outright */
  bool skip_fight = block == B_FIGHT && FD.tutorial && !U->retry;
  U->skip_hint = skip_fight;
  if ((U->cine || skip_fight) && !U->choosing && input_start_held()) {
    U->skip += dt / (skip_fight ? 1.1f : .8f);
    if (U->skip >= 1) {
      U->skip = 0;
      if (skip_fight) {
        /* count the orientation as passed (it cannot be lost anyway) and get on with the day */
        fight_won(); fight_end();
        for (int k = 0; k < WHO_COUNT; k++) A[k].in_fight = false;
        actor_t *me = actor_of(W_ME); actor_play(me, "Idle_Loop", 1, true, .3f); A[canon(W_ME)].base = "Idle_Loop";
        block = B_NONE;
      } else { skip_scene = true; voice_stop(); sub_clear(); if (block == B_SAY || block == B_WAIT || block == B_WALK) block = B_NONE; U->talking = false; }
    }
  } else U->skip = 0;

  switch (block) {
    case B_WAIT: wait_t -= dt; if (wait_t <= 0) block = B_NONE; break;
    case B_SAY: {
      say_t += dt; U->line_t += dt;
      float shown = U->line_t * U->line_cps;
      if (shown >= (float)strlen(U->line)) U->line_done = true;
      if (in->ok) {
        if (!U->line_done) { U->line_t = 1e6f; U->line_done = true; }
        else { voice_stop(); block = B_NONE; U->talking = false; }
      } else if (say_t > say_len + .35f && U->line_done) { block = B_NONE; U->talking = false; }
      if (block == B_NONE && A[say_who].a) {
        const char *base = A[say_who].base;
        if (base && !strcmp(base, "Idle_Loop")) actor_play(A[say_who].a, "Idle_Loop", 1, true, .4f);
      }
      break;
    }
    case B_WALK: if (!A[say_who].w.on) block = B_NONE; break;
    case B_CHOICE: {
      U->choose_t += dt;
      if (in->nav_y) { U->sel = (U->sel + in->nav_y + U->nopt) % U->nopt; sfx(SFX_UI, 0, 1); }
      if (in->ok && U->choose_t > .25f) {
        sfx(SFX_UI_OK, 0, 1);
        const sc_beat *o = &SCRIPT[choice_first + U->sel];
        U->choosing = false;
        pc = o->target;
        block = B_NONE;
        /* the choice is spoken */
        if (o->s2) {
          actor_t *a = actor_of(W_ME);
          expand(line_buf, sizeof line_buf, o->s2);
          if (voice_say(WHO_ID[me_id], line_buf)) {
            say_len = voice_seconds(WHO_ID[me_id], line_buf);
            say_t = 0; say_who = canon(W_ME); block = B_SAY;
            snprintf(name_buf, sizeof name_buf, "%s", CAST[me_id].name);
            U->talking = true; U->speaker = name_buf; U->line = line_buf; U->line_t = 0;
            U->line_cps = fmaxf(20, strlen(line_buf) / fmaxf(.5f, say_len * .94f)); U->line_done = false;
            U->speaker_col = CAST[me_id].color;
            actor_play(a, "Idle_Talking_Loop", 1, true, .3f);
            auto_cover(W_ME, -1, SH_AUTO);
          }
        }
      }
      break;
    }
    case B_ROAM: if (!street_busy()) roam_frame(dt, in); street_roam(dt, in); break;   /* street */
    case B_FIGHT: {
      if (U->retry) {
        if (in->nav_y) { U->retry_sel = !U->retry_sel; sfx(SFX_UI, 0, 1); }
        if (in->ok) {
          sfx(SFX_UI_OK, 0, 1); U->retry = false;
          if (U->retry_sel == 0) { int ph = fight_phase(); fight_end(); begin_fight(fight_beat, ph >= 1 ? 1 : 0); }   /* a checkpoint at phase two */
          else {
            /* talk it down: you lose face, the day goes on */
            fight_end(); for (int k = 0; k < WHO_COUNT; k++) A[k].in_fight = false; U->stats[ST_VIBE] -= 1; U->stats[ST_ENERGY] -= 1;
            U->note = "It took longer than a sword would have. You lost some of the day."; U->note_t = 4;
            block = B_NONE;
          }
        }
        break;
      }
#ifdef KH_AUTO
      fight_update(dt, in); break;
#endif
      fight_state st = fight_update(dt, in);
      if (st == FIGHT_WON || st == FIGHT_LOST) {
        for (int k = 0; k < WHO_COUNT; k++) if (!(st == FIGHT_LOST && !(fight_flags & 6))) A[k].in_fight = false;
        bool won = st == FIGHT_WON || (fight_flags & 2);
        if (won || (fight_flags & 4)) {
          if (won) fight_won();
          fight_end();
          actor_t *me = actor_of(W_ME); actor_play(me, "Idle_Loop", 1, true, .3f);
          A[canon(W_ME)].base = "Idle_Loop";
          U->stats[ST_ENERGY] -= fight_player_hp() < .3f ? 1 : 0;
          block = B_NONE;
        } else { U->retry = true; U->retry_sel = 0; U->retry_allow_skip = true; }
      }
      break;
    }
    default: break;
  }
  gear_frame(dt, in);
  extras_update(dt);
  /* the light rail: its bell, and the people out on Main turn to look */
  { v3 bp; if (world_train_bell(&bp)) { sfx_at(SFX_TRAIN_BELL, bp.x, bp.y, bp.z, 1); extras_look(bp); } }
  /* cars stop for cutscenes framed across Main, and run while you walk */
  { static int traffic = -1; int want = world_get_stage() == STAGE_TOWN && !U->cine ? 1 : 0;
    if (want != traffic) { traffic = want; world_traffic(want != 0); } }
  bool ran_player = block == B_ROAM;
  U->roam = ran_player;
  street_after(dt);   /* street */
  if (!ran_player) { U->mg = MG_NONE; U->npin = 0; }
  update_actors(dt);
  if (block != B_ROAM && block != B_FIGHT) bike_scene_tick();
  prop_tick(dt);
  if (block != B_ROAM && block != B_FIGHT) director_update(dt);
  world_update(dt, v3_(((t3_object *)cam)->position.x, ((t3_object *)cam)->position.y, ((t3_object *)cam)->position.z));
}

/* the epilogue of the part just finished (the day, or a night): its first ending that matches, then its lines */
const char *story_end_title(void) {
  for (const sc_ending *e = ENDINGS; e->flag >= 0; e++) if (e->part == epi_part && flags[e->flag]) return e->title;
  for (const sc_ending *e = ENDINGS; e->flag >= 0; e++) if (e->part == epi_part) return e->title;
  return "KEYHOLDER";
}
int story_end_lines(const char **out, int cap) {
  int n = 0;
  for (const sc_ending *e = ENDINGS; e->flag >= 0; e++) if (e->part == epi_part && flags[e->flag]) {
    for (int i = 0; i < 4 && e->lines[i] && n < cap; i++) out[n++] = e->lines[i];
    break;
  }
  for (const sc_statline *s = STAT_LINES; s->flag >= 0 && n < cap; s++)
    if (s->part == epi_part && flags[s->flag] && !(s->not_flag >= 0 && flags[s->not_flag])) out[n++] = s->text;
  if (!story_more()) n = street_end_lines(out, n, cap);   /* street: once, on the last page */
  return n;
}
bool story_more(void) { return block == B_EPILOGUE && pc < SCRIPT_COUNT && SCRIPT[pc].op != OP_END; }
int story_part(void) { return part_now; }   /* street */
bool story_flag(const char *n) { for (int i = 0; i < FLAG_COUNT; i++) if (!strcmp(FLAG_NAMES[i], n)) return flags[i]; return false; }   /* street */
