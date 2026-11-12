/* attic_env.c: the STAGE_ATTIC stage (see attic_env.h). Each layout is baked by a tool into a glb of vertex
 * coloured chunks, like the arenas, with its collision and anchors in a generated header:
 *   the room      tools/prep-attic.mjs         assets/models/attic.glb          src/gen/attic_data.h
 *   the nightmare tools/prep-attic-deep.mjs    assets/models/attic_deep.glb     src/gen/attic_deep_data.h
 *   the desert    tools/prep-memory-desert.mjs assets/models/memory_desert.glb  src/gen/memory_desert_data.h
 * This file loads them (the room at init, the others the first time they are shown), answers floor,
 * collision and raycast for world.c, and owns the moods: the room's glitched to normal lerp, the nightmare's
 * glitch (red in the boss's phase two), the desert's dawn. The gameplay (attic.c, story.c) is kept out. */
#include "attic_env.h"
#include "world.h"
#include "gen/attic_data.h"
#include "gen/attic_deep_data.h"
#include "gen/memory_desert_data.h"
#include "ds.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

const v3 ATTIC_ENTRY = { ATTIC_ENTRY_X, 0.0f, ATTIC_ENTRY_Z };
const v3 ATTIC_RACK  = { ATTIC_RACK_X, 0.0f, ATTIC_RACK_Z };
const v3 ATTIC_TERM  = { ATTIC_TERM_X, 0.0f, ATTIC_TERM_Z };
#define N_OF(a) ((int)(sizeof (a) / sizeof (a)[0]))

/* ---------------- the layouts ---------------- */
#define MAXCH 48
typedef struct {
  const char *file;
  bool loaded;
  t3_object *grp;                       /* everything the layout draws */
  const attic_box_t *sol; int nsol;     /* solids */
  const attic_box_t *sol2; int nsol2;   /* solids in phase two only */
  const attic_drop_t *soft; int nsoft;
  const attic_mark_t *land; int nland;
  const attic_ring_t *ring; int nring;
  const attic_anchor_t *anch; int nanch;
  const attic_chunk_t *chunks; int nchunks;
  t3_object *ch_obj[MAXCH];             /* per entry of chunks, once loaded */
  v3 entry; float entry_yaw;
  v3 rack, term; float term_yaw;
  v3 boss; float boss_r, boss_r2;
} layout_t;

/* the room's tables are older than the shared types: copied over at init */
static attic_box_t room_sol[N_OF(ATTIC_SOLIDS)];
static attic_drop_t room_soft[N_OF(ATTIC_SOFT)];
static const attic_mark_t room_land[1] = { { ATTIC_ENTRY_X, ATTIC_ENTRY_Z, DS_PI } };
static const attic_anchor_t room_anch[] = {
  { "entry", ATTIC_ENTRY_X, 0, ATTIC_ENTRY_Z, DS_PI }, { "rack", ATTIC_RACK_X, 0, ATTIC_RACK_Z, 0 }, { "term", ATTIC_TERM_X, 0, ATTIC_TERM_Z, DS_PI },
};
static const attic_anchor_t deep_anch[] = {
  { "entry", ATTIC_DEEP_ENTRY_X, 0, ATTIC_DEEP_ENTRY_Z, ATTIC_DEEP_ENTRY_YAW }, { "rack", ATTIC_DEEP_RACK_X, 0, ATTIC_DEEP_RACK_Z, 0 },
  { "term", ATTIC_DEEP_TERM_X, 0, ATTIC_DEEP_TERM_Z, ATTIC_DEEP_TERM_YAW }, { "boss", ATTIC_DEEP_BOSS_X, 0, ATTIC_DEEP_BOSS_Z, DS_PI },
};

static layout_t LY[ATTIC_LAYOUT_COUNT] = {
  [ATTIC_LAYOUT_ROOM] = { .file = "attic.glb", .sol = room_sol, .nsol = N_OF(room_sol), .soft = room_soft, .nsoft = N_OF(room_soft),
    .land = room_land, .nland = 1, .anch = room_anch, .nanch = N_OF(room_anch),
    .entry = { ATTIC_ENTRY_X, 0, ATTIC_ENTRY_Z }, .entry_yaw = DS_PI, .rack = { ATTIC_RACK_X, 0, ATTIC_RACK_Z },
    .term = { ATTIC_TERM_X, 0, ATTIC_TERM_Z }, .term_yaw = DS_PI, .boss = { 0, 0, 0 }, .boss_r = 4, .boss_r2 = 4 },
  [ATTIC_LAYOUT_DEEP] = { .file = "attic_deep.glb", .sol = ATTIC_DEEP_SOLIDS, .nsol = N_OF(ATTIC_DEEP_SOLIDS),
    .sol2 = ATTIC_DEEP_SOLIDS_P2, .nsol2 = N_OF(ATTIC_DEEP_SOLIDS_P2), .soft = ATTIC_DEEP_SOFT, .nsoft = N_OF(ATTIC_DEEP_SOFT),
    .land = ATTIC_DEEP_LANDINGS, .nland = N_OF(ATTIC_DEEP_LANDINGS), .ring = ATTIC_DEEP_CLEARINGS, .nring = N_OF(ATTIC_DEEP_CLEARINGS),
    .anch = deep_anch, .nanch = N_OF(deep_anch), .chunks = ATTIC_DEEP_CHUNKS, .nchunks = N_OF(ATTIC_DEEP_CHUNKS),
    .entry = { ATTIC_DEEP_ENTRY_X, 0, ATTIC_DEEP_ENTRY_Z }, .entry_yaw = ATTIC_DEEP_ENTRY_YAW,
    .rack = { ATTIC_DEEP_RACK_X, 0, ATTIC_DEEP_RACK_Z }, .term = { ATTIC_DEEP_TERM_X, 0, ATTIC_DEEP_TERM_Z }, .term_yaw = ATTIC_DEEP_TERM_YAW,
    .boss = { ATTIC_DEEP_BOSS_X, 0, ATTIC_DEEP_BOSS_Z }, .boss_r = ATTIC_DEEP_BOSS_R, .boss_r2 = ATTIC_DEEP_BOSS_R2 },
  [ATTIC_LAYOUT_DESERT] = { .file = "memory_desert.glb", .sol = MEMORY_DESERT_SOLIDS, .nsol = N_OF(MEMORY_DESERT_SOLIDS),
    .anch = MEMORY_DESERT_ANCHORS, .nanch = N_OF(MEMORY_DESERT_ANCHORS),
    .entry = { MEMORY_DESERT_ENTRY_X, 0, MEMORY_DESERT_ENTRY_Z }, .entry_yaw = MEMORY_DESERT_ENTRY_YAW,
    .rack = { MEMORY_DESERT_ENTRY_X, 0, MEMORY_DESERT_ENTRY_Z }, .term = { MEMORY_DESERT_ENTRY_X, 0, MEMORY_DESERT_ENTRY_Z },
    .boss = { MEMORY_DESERT_KEEP_X, 0, MEMORY_DESERT_KEEP_Z }, .boss_r = 10, .boss_r2 = 10 },
};

static t3_scene *SC;
static t3_object *env_root;     /* all the layouts */
static int cur = ATTIC_LAYOUT_ROOM;
static bool shown;
static t3_material *m_lit, *m_glow, *m_paint, *m_sky, *m_boss;
static float env_normal;        /* the room: 0 glitched .. 1 normal */
static float env_flick;         /* the glitch light flicker */
static float ramp_to;
static int phase; static float blend;   /* the boss's phase two, and how far its look has come */
static v3 arena_c; static float arena_r;
static v3 focus; static bool has_focus;
/* the nightmare's moving parts */
static t3_object *topple_piv[N_OF(ATTIC_DEEP_TOPPLE)];
static t3_object *p2_obj; static t3_vec3 p2_base;
static t3_object *scr_obj, *red_obj;   /* the boss chamber's screens: green, and their red twin for phase two */
/* the desert's balloon */
static t3_object *bal_grp, *pay_grp, *chute_grp, *line_grp;
static int bal_mode = -1; static float bal_t, bal_fill, bal_h, bal_v; static v3 bal_drift;

static t3_object *mesh_node(t3_object *o) {   /* a glb node may carry its mesh itself or as its one child */
  return o->type == T3_MESH ? o : o->child_count && o->children[0]->type == T3_MESH ? o->children[0] : NULL;
}
static void set_material(t3_object *o, t3_material *nm) {
  t3_mesh *m = (t3_mesh *)mesh_node(o);
  if (!m) return;
  t3_retain(nm); t3_release(m->materials[0]); m->materials[0] = nm;
}
/* wrap a loaded node in a group at `at`, so it can be turned and scaled about that point */
static t3_object *wrap_at(t3_object *parent, t3_object *o, v3 at) {
  t3_object *g = t3_group_new();
  t3_object_set_position(g, at.x, at.y, at.z);
  t3_object_add(parent, g);
  t3_vec3 p = o->position;
  t3_object_add(g, o);
  t3_object_set_position(o, p.x - at.x, p.y - at.y, p.z - at.z);
  t3_release(g);
  return g;
}

static void load_layout(int li) {
  layout_t *L = &LY[li];
  if (L->loaded || !SC) return;
  L->loaded = true;
  L->grp = t3_group_new();
  t3_object_add(env_root, L->grp);
  t3_release(L->grp);
  L->grp->visible = false;
  size_t n; uint8_t *b = ds_asset(L->file, &n);
  if (!b) return;
  t3_gltf *g = t3_gltf_parse(b, n, NULL, NULL);
  free(b);
  if (!g) return;
  t3_object *r = g->scene;
  t3_object_add(L->grp, r);
  /* the nodes first, then any re-parenting (it changes r's children) */
  t3_object *nodes[64]; int nn = 0;
  for (int k = 0; k < r->child_count && nn < 64; k++) nodes[nn++] = r->children[k];
  for (int k = 0; k < nn; k++) {
    t3_object *o = nodes[k];
    const char *nm = o->name ? o->name : "";
    set_material(o, !strncmp(nm, "glow_boss", 9) ? m_boss : !strncmp(nm, "glow_", 5) ? m_glow : !strncmp(nm, "paint_", 6) ? m_paint : !strncmp(nm, "sky_", 4) ? m_sky : m_lit);
    for (int c = 0; c < L->nchunks && c < MAXCH; c++) if (!strcmp(L->chunks[c].name, nm)) L->ch_obj[c] = o;
    if (li == ATTIC_LAYOUT_DEEP) {
      for (int t = 0; t < N_OF(ATTIC_DEEP_TOPPLE); t++) if (!strcmp(ATTIC_DEEP_TOPPLE[t].name, nm))
        topple_piv[t] = wrap_at(r, o, v3_(ATTIC_DEEP_TOPPLE[t].px, ATTIC_DEEP_TOPPLE[t].py, ATTIC_DEEP_TOPPLE[t].pz));
      if (!strcmp(nm, "p2_boss")) { p2_obj = o; p2_base = o->position; o->visible = false; }
      if (!strcmp(nm, "glow_scr_boss")) scr_obj = o;
      if (!strcmp(nm, "glow_red_boss")) { red_obj = o; o->visible = false; }
    }
    if (li == ATTIC_LAYOUT_DESERT) {
      v3 neck = v3_(MEMORY_DESERT_BALLOON_X, MEMORY_DESERT_BALLOON_Y, MEMORY_DESERT_BALLOON_Z);
      v3 pay = v3_(MEMORY_DESERT_PAYLOAD_X, MEMORY_DESERT_PAYLOAD_Y, MEMORY_DESERT_PAYLOAD_Z);
      if (!strcmp(nm, "balloon")) bal_grp = wrap_at(r, o, neck);
      else if (!strcmp(nm, "balloon_payload")) pay_grp = wrap_at(r, o, pay);
      else if (!strcmp(nm, "balloon_chute")) chute_grp = wrap_at(r, o, v3_(neck.x, neck.y - MEMORY_DESERT_LINE * 0.4f, neck.z));
      else if (!strcmp(nm, "balloon_line")) line_grp = wrap_at(r, o, v3_(neck.x, 0, neck.z));
    }
  }
}

static void build_env(void) {
  if (env_root) return;
  env_root = t3_group_new();
  t3_object_add(&SC->base, env_root);
  t3_release(env_root);
  m_lit = t3_mesh_lambert_material_new(0xffffff); m_lit->vertex_colors = true; m_lit->side = T3_DOUBLE_SIDE;
  m_glow = t3_mesh_basic_material_new(0xffffff); m_glow->vertex_colors = true; m_glow->side = T3_DOUBLE_SIDE;
  m_glow->color = (t3_color){ 1.6f, 1.6f, 1.6f };
  m_boss = t3_mesh_basic_material_new(0xffffff); m_boss->vertex_colors = true; m_boss->side = T3_DOUBLE_SIDE;
  m_boss->color = (t3_color){ 1.6f, 1.6f, 1.6f };
  /* spray paint reads in the dark, but stays under the bloom */
  m_paint = t3_mesh_basic_material_new(0xffffff); m_paint->vertex_colors = true; m_paint->side = T3_DOUBLE_SIDE;
  m_paint->color = (t3_color){ 0.62f, 0.62f, 0.62f };
  /* the desert's far mountains: unlit and unfogged, their haze is baked in */
  m_sky = t3_mesh_basic_material_new(0xffffff); m_sky->vertex_colors = true; m_sky->side = T3_DOUBLE_SIDE; m_sky->fog = false;
  for (int i = 0; i < N_OF(ATTIC_SOLIDS); i++) room_sol[i] = (attic_box_t){ ATTIC_SOLIDS[i].x0, ATTIC_SOLIDS[i].z0, ATTIC_SOLIDS[i].x1, ATTIC_SOLIDS[i].z1, ATTIC_SOLIDS[i].top };
  for (int i = 0; i < N_OF(ATTIC_SOFT); i++) room_soft[i] = (attic_drop_t){ ATTIC_SOFT[i].x, ATTIC_SOFT[i].z, ATTIC_SOFT[i].r, 0 };
  load_layout(ATTIC_LAYOUT_ROOM);
  env_root->visible = false;
}

void attic_env_init(t3_scene *scene) { if (SC) return; SC = scene; build_env(); }

static void show_groups(void) {
  if (!env_root) return;
  env_root->visible = shown;
  for (int i = 0; i < ATTIC_LAYOUT_COUNT; i++) if (LY[i].grp) LY[i].grp->visible = shown && i == cur;
}

void attic_env_layout(int layout) {
  if (layout < 0 || layout >= ATTIC_LAYOUT_COUNT) layout = ATTIC_LAYOUT_ROOM;
  cur = layout;
  if (SC) { build_env(); load_layout(cur); }
  phase = 0; blend = 0; arena_r = 0; has_focus = false;
  if (m_boss) m_boss->color = (t3_color){ 1.6f, 1.6f, 1.6f };
  for (int t = 0; t < N_OF(topple_piv); t++) if (topple_piv[t]) t3_object_set_rotation(topple_piv[t], 0, 0, 0);
  for (int c = 0; c < LY[cur].nchunks && c < MAXCH; c++) if (LY[cur].ch_obj[c]) LY[cur].ch_obj[c]->visible = true;
  if (p2_obj) p2_obj->visible = false;
  if (red_obj) red_obj->visible = false;
  attic_env_balloon(0);
  show_groups();
  /* already on the stage: repaint the sky and lights for this layout now */
  if (world_get_stage() == STAGE_ATTIC) world_set_stage(STAGE_ATTIC);
}
int attic_env_get_layout(void) { return cur; }

/* ---------------- queries the gameplay uses ---------------- */
int attic_env_soft_at(v3 p) {
  const layout_t *L = &LY[cur];
  for (int i = 0; i < L->nsoft; i++) if (hypotf(p.x - L->soft[i].x, p.z - L->soft[i].z) < L->soft[i].r * 0.8f) return L->soft[i].landing;
  return -1;
}
bool attic_env_soft(v3 p) { return attic_env_soft_at(p) >= 0; }
void attic_env_reset(float normal) { env_normal = ramp_to = normal; }
void attic_env_target(float normal) { ramp_to = normal; }
float attic_env_level(void) { return env_normal; }

v3 attic_env_entry(float *yaw) { if (yaw) *yaw = LY[cur].entry_yaw; return LY[cur].entry; }
v3 attic_env_rack(void) { return LY[cur].rack; }
v3 attic_env_term(float *yaw) { if (yaw) *yaw = LY[cur].term_yaw; return LY[cur].term; }
int attic_env_landing_count(void) { return LY[cur].nland; }
v3 attic_env_landing(int i, float *yaw) {
  const layout_t *L = &LY[cur];
  if (i < 0 || i >= L->nland) return attic_env_entry(yaw);
  if (yaw) *yaw = L->land[i].yaw;
  return v3_(L->land[i].x, 0, L->land[i].z);
}
int attic_env_clearing_count(void) { return LY[cur].nring; }
v3 attic_env_clearing(int i, float *radius, const char **zone) {
  const layout_t *L = &LY[cur];
  if (i < 0 || i >= L->nring) { if (radius) *radius = L->boss_r; if (zone) *zone = "boss"; return L->boss; }
  if (radius) *radius = L->ring[i].r;
  if (zone) *zone = L->ring[i].zone;
  return v3_(L->ring[i].x, 0, L->ring[i].z);
}
v3 attic_env_boss(float *radius) { if (radius) *radius = LY[cur].boss_r; return LY[cur].boss; }
float attic_env_boss2(void) { return LY[cur].boss_r2; }
bool attic_env_anchor(const char *name, v3 *pos, float *yaw) {
  const layout_t *L = &LY[cur];
  for (int i = 0; i < L->nanch; i++) if (!strcmp(L->anch[i].name, name)) {
    if (pos) *pos = v3_(L->anch[i].x, L->anch[i].y, L->anch[i].z);
    if (yaw) *yaw = L->anch[i].yaw;
    return true;
  }
  return false;
}

void attic_env_set_arena(v3 center, float radius) { arena_c = center; arena_r = radius; }
void attic_env_phase(int p) { phase = p ? 1 : 0; }
int attic_env_get_phase(void) { return phase; }
float attic_env_phase_blend(void) { return blend; }
void attic_env_focus(v3 p) { focus = p; has_focus = true; }

/* ---------------- the balloon ---------------- */
void attic_env_balloon(int mode) {
  bal_mode = mode < 0 ? -1 : mode > 2 ? 2 : mode;
  bal_t = 0;
  if (bal_mode <= 0) { bal_fill = 0; bal_h = 0; bal_v = 0; bal_drift = v3_(0, 0, 0); }
  if (bal_mode == 1) { bal_h = 0; bal_v = 0; bal_drift = v3_(0, 0, 0); }
  if (bal_mode == 2) bal_fill = 1;
}
int attic_env_balloon_mode(void) { return bal_mode; }
float attic_env_balloon_height(void) { return bal_h; }
static void balloon_pose(float dt) {
  if (!bal_grp) return;
  bal_t += dt;
  if (bal_mode == 1) bal_fill = fminf(1, bal_fill + dt / 3.f);
  if (bal_mode == 2) {   /* let go: it climbs, gathering to about 5 m/s, and the wind takes it */
    bal_v = fminf(5.f, bal_v + 2.2f * dt);
    bal_h += bal_v * dt;
    bal_drift = v3_add(bal_drift, v3_scale(v3_(MEMORY_DESERT_WIND_X, 0, MEMORY_DESERT_WIND_Z), dt * fminf(1.6f, bal_h * 0.08f)));
  }
  float nx = MEMORY_DESERT_BALLOON_X + bal_drift.x, nz = MEMORY_DESERT_BALLOON_Z + bal_drift.z, ny = MEMORY_DESERT_BALLOON_Y + bal_h;
  bool gone = bal_h > 320 || bal_mode < 0;   /* out of sight, or not out yet */
  bal_grp->visible = !gone;
  if (bal_mode == 0) {   /* laid out flat on its tarp */
    t3_object_set_position(bal_grp, MEMORY_DESERT_BALLOON_X, 0.11f, MEMORY_DESERT_BALLOON_Z + 0.9f);
    t3_object_set_rotation(bal_grp, -DS_PI * 0.5f, 0, 0);
    t3_object_set_scale(bal_grp, 0.55f, 0.75f, 0.12f);
  } else {   /* rounding out from the neck as the helium goes in; a slight bob once it pulls at its line */
    float f = bal_fill, s = 0.3f + 0.7f * f * f * (3 - 2 * f);
    float bob = bal_mode == 1 ? sinf(bal_t * 2.3f) * 0.03f * f : 0;
    t3_object_set_position(bal_grp, nx, ny + bob, nz);
    t3_object_set_rotation(bal_grp, sinf(bal_t * 1.1f) * 0.05f * f, 0, cosf(bal_t * 0.9f) * 0.05f * f);
    t3_object_set_scale(bal_grp, s, s * (0.55f + 0.45f * f), s);
  }
  /* the payload: on the table until it flies; then on its line, sitting on the ground until the line is taut */
  bool flying = bal_mode == 2;
  if (pay_grp) {
    float py = flying ? fmaxf(0.f, ny - MEMORY_DESERT_LINE) : MEMORY_DESERT_PAYLOAD_Y;
    t3_object_set_position(pay_grp, flying ? nx : MEMORY_DESERT_PAYLOAD_X, py, flying ? nz : MEMORY_DESERT_PAYLOAD_Z);
    pay_grp->visible = !gone || bal_mode < 0;
  }
  if (chute_grp) {
    chute_grp->visible = flying && !gone;
    t3_object_set_position(chute_grp, nx, fmaxf(0.3f, ny - MEMORY_DESERT_LINE * 0.4f), nz);
  }
  if (line_grp) {
    float y0 = fmaxf(0.f, ny - MEMORY_DESERT_LINE) + 0.3f, len = fmaxf(0.01f, ny - y0);
    line_grp->visible = flying && !gone;
    t3_object_set_position(line_grp, nx, y0, nz);
    t3_object_set_scale(line_grp, 1, len, 1);
  }
}

/* ---------------- shown / collision / raycast / mood (world.c calls these for STAGE_ATTIC) ---------------- */
void attic_env_show(bool on) {
  if (!SC) return;   /* world_init hides stages before attic_env_init builds anything */
  build_env();
  load_layout(cur);
  shown = on;
  show_groups();
}

static int nearest_ring(v3 p, v3 *c, float *r) {
  const layout_t *L = &LY[cur];
  float best = hypotf(p.x - L->boss.x, p.z - L->boss.z); *c = L->boss; *r = L->boss_r; int bi = -1;
  for (int i = 0; i < L->nring; i++) { float d = hypotf(p.x - L->ring[i].x, p.z - L->ring[i].z); if (d < best) { best = d; *c = v3_(L->ring[i].x, 0, L->ring[i].z); *r = L->ring[i].r; bi = i; } }
  return bi;
}
void attic_env_floor(v3 *center, float *radius) {
  v3 c = v3_(0, 0, 0); float r = 4.0f;
  if (arena_r > 0) { c = arena_c; r = arena_r; }
  else if (cur == ATTIC_LAYOUT_DEEP) nearest_ring(has_focus ? focus : LY[cur].entry, &c, &r);
  else if (cur == ATTIC_LAYOUT_DESERT) { c = LY[cur].boss; r = LY[cur].boss_r; }
  if (center) *center = c;
  if (radius) *radius = r;
}

static void push_box(const attic_box_t *b, v3 *p, float r, v3 *n, bool *hit) {
  if (p->y > b->top - 0.05f) return;
  if (p->x < b->x0 - r || p->x > b->x1 + r || p->z < b->z0 - r || p->z > b->z1 + r) return;
  float qx = ds_clamp(p->x, b->x0, b->x1), qz = ds_clamp(p->z, b->z0, b->z1);
  float dx = p->x - qx, dz = p->z - qz, d2 = dx * dx + dz * dz;
  if (d2 >= r * r) return;
  if (d2 > 1e-8f) { float d = sqrtf(d2); p->x = qx + dx / d * r; p->z = qz + dz / d * r; *n = v3_(dx / d, 0, dz / d); }
  else {   /* inside: out the nearest face */
    float l = p->x - b->x0, rr = b->x1 - p->x, f = p->z - b->z0, bk = b->z1 - p->z, m = fminf(fminf(l, rr), fminf(f, bk));
    if (m == l) { p->x = b->x0 - r; *n = v3_(-1, 0, 0); } else if (m == rr) { p->x = b->x1 + r; *n = v3_(1, 0, 0); }
    else if (m == f) { p->z = b->z0 - r; *n = v3_(0, 0, -1); } else { p->z = b->z1 + r; *n = v3_(0, 0, 1); }
  }
  *hit = true;
}
void attic_env_push(v3 *p, float r, float h, v3 *n, bool *hit) {
  (void)h;
  const layout_t *L = &LY[cur];
  if (cur == ATTIC_LAYOUT_ROOM) {
    const float W = 0.12f;   /* the studs stand off the walls */
    if (p->x < -ATTIC_RX + W + r) { p->x = -ATTIC_RX + W + r; *n = v3_(1, 0, 0); *hit = true; }
    if (p->x >  ATTIC_RX - W - r) { p->x =  ATTIC_RX - W - r; *n = v3_(-1, 0, 0); *hit = true; }
    if (p->z < -ATTIC_RZ + W + r) { p->z = -ATTIC_RZ + W + r; *n = v3_(0, 0, 1); *hit = true; }
    if (p->z >  ATTIC_RZ - W - r) { p->z =  ATTIC_RZ - W - r; *n = v3_(0, 0, -1); *hit = true; }
  } else if (cur == ATTIC_LAYOUT_DEEP) {   /* the walls close every way out; this only keeps a stray body in the plan */
    p->x = ds_clamp(p->x, ATTIC_DEEP_X0 + r, ATTIC_DEEP_X1 - r); p->z = ds_clamp(p->z, ATTIC_DEEP_Z0 + r, ATTIC_DEEP_Z1 - r);
  } else {   /* the desert: the flats fade into haze, but you stay near the launch */
    float dx = p->x - MEMORY_DESERT_KEEP_X, dz = p->z - MEMORY_DESERT_KEEP_Z, d = hypotf(dx, dz), lim = MEMORY_DESERT_KEEP_R - r;
    if (d > lim && d > 1e-4f) { p->x = MEMORY_DESERT_KEEP_X + dx * lim / d; p->z = MEMORY_DESERT_KEEP_Z + dz * lim / d; *n = v3_(-dx / d, 0, -dz / d); *hit = true; }
  }
  for (int i = 0; i < L->nsol; i++) push_box(&L->sol[i], p, r, n, hit);
  if (blend > 0.5f) for (int i = 0; i < L->nsol2; i++) push_box(&L->sol2[i], p, r, n, hit);
}

/* the slab test, from outside in: entry distance and the face's normal */
static bool ray_box(v3 o, v3 d, v3 mn, v3 mx, float *t_in, float *t_out, v3 *nrm) {
  float tmin = -1e9f, tmax = 1e9f; int ax = 0; float sg = -1;
  for (int a = 0; a < 3; a++) {
    float od = (&o.x)[a], dd = (&d.x)[a], lo = (&mn.x)[a], hi = (&mx.x)[a];
    if (fabsf(dd) < 1e-8f) { if (od < lo || od > hi) return false; continue; }
    float ta = (lo - od) / dd, tb = (hi - od) / dd, s = -1;
    if (ta > tb) { float k = ta; ta = tb; tb = k; s = 1; }
    if (ta > tmin) { tmin = ta; ax = a; sg = s; }
    tmax = fminf(tmax, tb);
    if (tmin > tmax) return false;
  }
  *t_in = tmin; *t_out = tmax;
  if (nrm) *nrm = ax == 0 ? v3_(sg, 0, 0) : ax == 1 ? v3_(0, sg, 0) : v3_(0, 0, sg);
  return true;
}

/* the roof slope over x: the camera must stay under the rafters */
static float roof_y(float x) { float t = fminf(1.f, fabsf(x) / ATTIC_RX); return ATTIC_RIDGE - (ATTIC_RIDGE - ATTIC_KNEE) * t - 0.2f; }

bool attic_env_raycast(v3 o, v3 d, float max_dist, float *t_out, v3 *n_out) {
  const layout_t *L = &LY[cur];
  float best = max_dist; bool any = false; v3 bn = v3_(0, 1, 0);
  float t0, t1; v3 nn;
  if (cur == ATTIC_LAYOUT_ROOM) {
    /* the room's box (walls), from inside */
    if (ray_box(o, d, v3_(-ATTIC_RX + 0.1f, 0, -ATTIC_RZ + 0.1f), v3_(ATTIC_RX - 0.1f, ATTIC_RIDGE, ATTIC_RZ - 0.1f), &t0, &t1, NULL) && t1 > 0 && t1 < best) { best = t1; any = true; bn = v3_(0, 1, 0); }
    /* the sloped roof: march the ray (cheap; the room is small) */
    for (float t = 0.1f; t < best; t += 0.1f) {
      v3 q = v3_add(o, v3_scale(d, t));
      if (q.y > roof_y(q.x)) { best = t; any = true; bn = v3_(q.x > 0 ? -0.5f : 0.5f, -0.85f, 0); break; }
    }
  } else {
    /* the floor, and the nightmare's roof far above */
    if (d.y < -1e-6f) { float t = -o.y / d.y; if (t >= 0 && t < best) { best = t; any = true; bn = v3_(0, 1, 0); } }
    if (cur == ATTIC_LAYOUT_DEEP && d.y > 1e-6f) { float t = (ATTIC_DEEP_ROOF - o.y) / d.y; if (t >= 0 && t < best) { best = t; any = true; bn = v3_(0, -1, 0); } }
  }
  for (int i = 0; i < L->nsol + (blend > 0.5f ? L->nsol2 : 0); i++) {
    const attic_box_t *b = i < L->nsol ? &L->sol[i] : &L->sol2[i - L->nsol];
    if (ray_box(o, d, v3_(b->x0, 0, b->z0), v3_(b->x1, b->top, b->z1), &t0, &t1, &nn) && t0 > 0 && t0 < best) { best = t0; any = true; bn = nn; }
  }
  if (any) { *t_out = best; if (n_out) *n_out = bn; }
  return any;
}

/* the moods: sky gradient, fog, the two lights. Linear colours */
static arena_mood mood_glitch(float flick) {   /* dim, cold, a green cast, close fog, flicker */
  return (arena_mood){ t3_color_hex(0x0a1210), t3_color_hex(0x10201a), t3_color_hex(0x0c1a14),
    t3_color_hex(0x244c3e), t3_color_hex(0x10160f), t3_color_hex(0x40ff8a), 0.85f * flick, 0.7f * flick, 3.f, 22.f, v3_norm(v3_(0.2f, 0.7f, -0.6f)), 0 };
}
static t3_color cmix(t3_color a, t3_color b, float t) { return (t3_color){ a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t }; }
static void mood_lerp(arena_mood *m, const arena_mood *a, const arena_mood *b, float t) {
  m->zenith = cmix(a->zenith, b->zenith, t); m->horizon = cmix(a->horizon, b->horizon, t); m->fog = cmix(a->fog, b->fog, t);
  m->hemi_sky = cmix(a->hemi_sky, b->hemi_sky, t); m->hemi_ground = cmix(a->hemi_ground, b->hemi_ground, t); m->sun = cmix(a->sun, b->sun, t);
  m->hemi = ds_lerp(a->hemi, b->hemi, t); m->dir = ds_lerp(a->dir, b->dir, t);
  m->fog_near = ds_lerp(a->fog_near, b->fog_near, t); m->fog_far = ds_lerp(a->fog_far, b->fog_far, t);
  m->sun_dir = v3_norm(v3_lerp(a->sun_dir, b->sun_dir, t)); m->stars = ds_lerp(a->stars, b->stars, t);
}
void attic_env_mood(arena_mood *m) {
  if (cur == ATTIC_LAYOUT_DEEP) {
    /* the glitch, seen further: the aisles run on into green murk. Phase two: the bulbs go red */
    arena_mood g = { t3_color_hex(0x070d0b), t3_color_hex(0x0e1a16), t3_color_hex(0x0f1b17),
      t3_color_hex(0x3e6468), t3_color_hex(0x1c201c), t3_color_hex(0x8ee6d0), 1.3f * env_flick, 0.75f * env_flick, 5.f, 34.f, v3_norm(v3_(0.2f, 0.8f, -0.5f)), 0 };
    arena_mood red = { t3_color_hex(0x0c0303), t3_color_hex(0x220707), t3_color_hex(0x200706),
      t3_color_hex(0x7a2c26), t3_color_hex(0x1c0a08), t3_color_hex(0xff4a36), 1.1f * env_flick, 0.95f * env_flick, 4.f, 28.f, v3_norm(v3_(-0.2f, 0.8f, 0.5f)), 0 };
    float e = blend * blend * (3 - 2 * blend);
    mood_lerp(m, &g, &red, e);
    return;
  }
  if (cur == ATTIC_LAYOUT_DESERT) {
    /* the Sonoran desert at dawn: a clear sky going from deep blue overhead to pink and gold at the horizon,
     * the sun just up in the east and throwing long light across the flats, a pink haze far off */
    /* (the fog and horizon stay under the bloom's threshold; the fog is the mountains' foot colour in the tool) */
    *m = (arena_mood){ t3_color_hex(0x2c3e86), t3_color_hex(0xd6a080), t3_color_hex(0xc8947e),
      t3_color_hex(0xa8acdc), t3_color_hex(0x9a6450), t3_color_hex(0xffb070), 1.15f, 2.1f, 24.f, 165.f,
      v3_norm(v3_(MEMORY_DESERT_SUN_X, MEMORY_DESERT_SUN_Y, MEMORY_DESERT_SUN_Z)), 0 };
    return;
  }
  /* the room: wrong (the glitch) easing to a warm dusty attic under bare bulbs */
  float t = ds_clamp(env_normal, 0, 1);
  arena_mood wrong = mood_glitch(env_flick);
  arena_mood norm = { t3_color_hex(0x2a2218), t3_color_hex(0x30261a), t3_color_hex(0x2c2318),
    t3_color_hex(0xf4e2bc), t3_color_hex(0x6a5640), t3_color_hex(0xffe2b0), 1.55f, 1.25f, 9.f, 26.f, v3_norm(v3_(0.25f, 0.9f, 0.35f)), 0 };
  mood_lerp(m, &wrong, &norm, t);
}

bool attic_env_update(float dt) {
  bool glitch = cur == ATTIC_LAYOUT_DEEP || (cur == ATTIC_LAYOUT_ROOM && env_normal < 0.5f);
  float f = glitch ? (0.55f + 0.45f * (sinf(ds_now() * 23) * 0.5f + 0.5f) * (ds_rand() < 0.1f ? 0.3f : 1.f)) : 1.f;
  if (cur == ATTIC_LAYOUT_DEEP) f = 0.75f + (f - 0.55f) * 0.55f;   /* the nightmare flickers less: it has to be walked */
  float old_n = env_normal, old_f = env_flick, old_b = blend;
  env_flick = ds_damp(env_flick, f, 12, dt);
  env_normal = ds_damp(env_normal, ramp_to, 2.2f, dt);
  if (cur == ATTIC_LAYOUT_DEEP) {
    blend = ds_clamp(blend + (phase ? dt : -dt) / 1.5f, 0, 1);
    float e = blend * blend * (3 - 2 * blend);
    for (int t = 0; t < N_OF(topple_piv); t++) if (topple_piv[t]) {
      const attic_topple_t *T = &ATTIC_DEEP_TOPPLE[t];
      /* it goes over late and fast, and settles with a little bounce */
      float k = e < 0.7f ? e * e / 0.49f : 1.f + sinf((e - 0.7f) / 0.3f * DS_PI) * 0.06f;
      float a = T->angle * k;
      t3_object_set_rotation(topple_piv[t], T->axis == 0 ? a : 0, 0, T->axis == 2 ? a : 0);
    }
    if (p2_obj) {   /* what was on the towers drops into the margin as they go over */
      float fall = ds_clamp((e - 0.35f) / 0.65f, 0, 1);
      p2_obj->visible = blend > 0.36f;
      t3_object_set_position(p2_obj, p2_base.x, p2_base.y + 6.f * (1 - fall * fall), p2_base.z);
    }
    if (m_boss) m_boss->color = (t3_color){ 1.6f + 0.8f * e, 1.6f - 1.2f * e, 1.6f - 1.3f * e };
    /* the screens go red with it, stuttering over while the towers go */
    bool red = blend > 0.5f || (blend > 0.15f && ds_rand() < blend);
    bool near_boss = !has_focus || hypotf(focus.x - ATTIC_DEEP_BOSS_X, focus.z - ATTIC_DEEP_BOSS_Z) < 50.f;
    if (scr_obj) scr_obj->visible = near_boss && !red;
    if (red_obj) red_obj->visible = near_boss && red;
    /* the far chunks hide: fog takes them anyway */
    if (has_focus) for (int c = 0; c < LY[cur].nchunks && c < MAXCH; c++) {
      t3_object *o = LY[cur].ch_obj[c];
      if (!o || o == p2_obj || o == scr_obj || o == red_obj) continue;
      const attic_chunk_t *k = &LY[cur].chunks[c];
      float dx = fmaxf(0, fmaxf(k->x0 - focus.x, focus.x - k->x1)), dz = fmaxf(0, fmaxf(k->z0 - focus.z, focus.z - k->z1));
      o->visible = dx * dx + dz * dz < 46.f * 46.f;
    }
  }
  if (cur == ATTIC_LAYOUT_DESERT) balloon_pose(dt);
  if (cur == ATTIC_LAYOUT_DESERT) return false;
  return fabsf(env_normal - old_n) > 0.002f || fabsf(env_flick - old_f) > 0.01f || blend != old_b;
}
