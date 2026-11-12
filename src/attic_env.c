/* attic_env.c: the STAGE_ATTIC room (see attic_env.h). The gameplay (attic.c) is kept out of here: this
 * is only the room's geometry, collision, the soft-floor grid, and the glitched->normal mood. */
#include "attic_env.h"
#include "fx.h"
#include "ds.h"
#include <math.h>

/* ---------------- the room ---------------- */
#define RX   7.0f          /* half width (x) */
#define RZ   6.0f          /* half depth (z) */
#define CEIL 3.6f
const v3 ATTIC_ENTRY = { 0.0f, 0.0f, 5.0f };    /* the stair head, by the south wall, facing in */
const v3 ATTIC_RACK  = { -3.3f, 0.0f, -5.2f };  /* the server rack (and its terminal), north-west */

/* the soft-floor grid: 1 is a board that gives (a red X). A sound path snakes entry->rack up the west side. */
#define COLS 9
#define ROWS 8
#define GX0 (-4.0f)
#define GXD  1.0f
#define GZ0  3.6f
#define GZD (-1.0f)
#define SOFT_R 0.42f
static const uint8_t SOFT[ROWS][COLS] = {
  {1,0,1,0,0,1,0,1,0},
  {1,0,1,0,0,1,0,1,0},
  {0,1,0,0,1,0,1,0,1},
  {1,0,0,0,1,0,1,0,1},
  {0,1,0,1,0,1,0,1,0},
  {1,0,0,1,0,1,0,1,0},
  {0,0,1,0,1,0,1,0,1},
  {0,0,1,0,1,0,1,0,1},
};
static inline float col_x(int c) { return GX0 + c * GXD; }
static inline float row_z(int r) { return GZ0 + r * GZD; }

/* solid boxes that block the player and stop the camera (the shelving, the rack, junk). Axis aligned
 * {cx, cz, hx, hz, top}. The four walls are handled as room bounds, not boxes. */
typedef struct { float cx, cz, hx, hz, top; } box_t;
static const box_t SOLID[] = {
  { 6.3f,  0.5f, 0.45f, 4.5f, 2.3f },   /* east wall shelving */
  {-6.3f,  2.8f, 0.45f, 2.6f, 2.3f },   /* west wall shelving (south half; the rack end stays clear) */
  { 0.0f, -5.4f, 1.3f,  0.5f, 1.7f },   /* a run of totes along the north wall, east of the rack */
  { 5.2f, -4.2f, 0.7f,  0.7f, 1.1f },   /* a junk pile in the north-east corner */
  {-3.3f, -5.3f, 0.6f,  0.5f, 1.6f },   /* the server rack */
};
#define NSOLID ((int)(sizeof SOLID / sizeof SOLID[0]))

/* ---------------- state ---------------- */
static t3_scene *SC;
static t3_object *env_grp, *glitch_grp;            /* the room; the red/flicker accents shown while wrong */
static bool env_built;
static float env_normal;                           /* 0 glitched .. 1 normal (ramps up on the reboot) */
static float env_flick;                            /* the glitch light flicker, per frame */
static float ramp_to;                              /* env_normal eases toward this */

static uint32_t wire(uint32_t a, uint32_t b, float t) {   /* lerp two 0xRRGGBB */
  int ar=(a>>16)&255, ag=(a>>8)&255, ab=a&255, br=(b>>16)&255, bg=(b>>8)&255, bb=b&255;
  int r=(int)(ar+(br-ar)*t), g=(int)(ag+(bg-ag)*t), bl=(int)(ab+(bb-ab)*t);
  return ((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)bl;
}

/* ---------------- geometry ---------------- */
static void build_env(void) {
  if (env_built) return;
  env_built = true;
  const v3 RACK = ATTIC_RACK;
  const uint32_t WOOD_A = 0x6b5436, WOOD_B = 0x5a4530, WALL = 0x4b463d, BEAM = 0x3a2f24, SCORCH = 0x241c15;
  fxp_begin(); fxp_layer(FXP_BODY);
  /* the plank floor: boards running in z, alternating shade */
  for (int i = 0; i < 28; i++) {
    float x0 = -RX + i * (2*RX/28), x1 = x0 + (2*RX/28);
    fxp_quad(v3_(x0, 0, -RZ), v3_(x1, 0, -RZ), v3_(x1, 0, RZ), v3_(x0, 0, RZ), (i & 1) ? WOOD_A : WOOD_B);
  }
  /* a real attic: low knee walls at the eaves, gable ends, a peaked roof along the ridge (z axis) */
  const float EAVE = 2.15f;
  fxp_quad(v3_(-RX,0,-RZ), v3_(-RX,0,RZ), v3_(-RX,EAVE,RZ), v3_(-RX,EAVE,-RZ), WALL);      /* west knee wall */
  fxp_quad(v3_(RX,0,RZ), v3_(RX,0,-RZ), v3_(RX,EAVE,-RZ), v3_(RX,EAVE,RZ), WALL);          /* east knee wall */
  fxp_quad(v3_(-RX,0,RZ), v3_(RX,0,RZ), v3_(RX,EAVE,RZ), v3_(-RX,EAVE,RZ), WALL);          /* south wall, lower */
  fxp_quad(v3_(-RX,EAVE,RZ), v3_(RX,EAVE,RZ), v3_(0,CEIL,RZ), v3_(0,CEIL,RZ), WALL);       /* south gable */
  fxp_quad(v3_(RX,0,-RZ), v3_(-RX,0,-RZ), v3_(-RX,EAVE,-RZ), v3_(RX,EAVE,-RZ), WALL);      /* north wall, lower */
  fxp_quad(v3_(RX,EAVE,-RZ), v3_(-RX,EAVE,-RZ), v3_(0,CEIL,-RZ), v3_(0,CEIL,-RZ), WALL);   /* north gable */
  fxp_quad(v3_(-RX,EAVE,-RZ), v3_(-RX,EAVE,RZ), v3_(0,CEIL,RZ), v3_(0,CEIL,-RZ), 0x3a2f24);/* west roof slope */
  fxp_quad(v3_(0,CEIL,-RZ), v3_(0,CEIL,RZ), v3_(RX,EAVE,RZ), v3_(RX,EAVE,-RZ), 0x342a20);  /* east roof slope */
  fxp_box(v3_(0, CEIL-0.12f, 0), v3_(0.2f, 0.24f, 2*RZ), 0x241c14);                        /* the ridge beam */
  for (float z = -RZ+1.1f; z < RZ; z += 2.0f) {                                            /* angled rafters */
    fxp_bar(v3_(-RX+0.1f, EAVE, z), v3_(0, CEIL-0.1f, z), 0.08f, BEAM);
    fxp_bar(v3_(RX-0.1f, EAVE, z), v3_(0, CEIL-0.1f, z), 0.08f, BEAM);
  }
  for (float z = -RZ+2.2f; z < RZ; z += 3.2f) fxp_bar(v3_(-RX+0.2f, EAVE+0.05f, z), v3_(RX-0.2f, EAVE+0.05f, z), 0.07f, BEAM);  /* collar ties */
  /* the scorched patches under the red X paint (the X itself is emissive, built in the glow pass) */
  for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) {
    if (!SOFT[r][c]) continue;
    float x = col_x(c), z = row_z(r), s = 0.46f;
    fxp_quad(v3_(x-s,0.012f,z-s), v3_(x+s,0.012f,z-s), v3_(x+s,0.012f,z+s), v3_(x-s,0.012f,z+s), SCORCH);
  }
  /* shelving: steel uprights and shelves along the east and part of the west wall, with totes on them */
  for (int side = 0; side < 2; side++) {
    float wx = side ? 6.35f : -6.35f; float z0 = side ? -4.0f : 0.2f, z1 = side ? 4.6f : 5.4f;
    const uint32_t STEEL = 0x555b63, TOTE = 0x3e5a6e;
    fxp_box(v3_(wx, 1.1f, (z0+z1)/2), v3_(0.06f, 2.2f, z1-z0), STEEL);
    for (float sy = 0.5f; sy < 2.2f; sy += 0.55f) fxp_box(v3_(wx, sy, (z0+z1)/2), v3_(0.5f, 0.05f, z1-z0), STEEL);
    for (float sz = z0+0.5f; sz < z1; sz += 1.3f) { fxp_box(v3_(wx, 0.8f, sz), v3_(0.42f, 0.28f, 0.42f), TOTE); fxp_box(v3_(wx, 1.9f, sz), v3_(0.42f, 0.28f, 0.42f), wire(TOTE,0x6e5a3e,0.6f)); }
  }
  /* scattered junk: a run of totes by the north wall, a corner pile, a dead CRT */
  for (int i = 0; i < 5; i++) fxp_box(v3_(-1.0f + i*0.55f, 0.28f, -5.4f), v3_(0.26f, 0.28f, 0.26f), 0x4a4a52);
  fxp_box(v3_(5.2f, 0.4f, -4.2f), v3_(0.6f, 0.4f, 0.6f), 0x6e5a3e);
  fxp_box(v3_(5.2f, 0.95f, -4.2f), v3_(0.42f, 0.32f, 0.5f), 0x2a2d33);      /* CRT body */
  /* the server rack: a tall dark cabinet, a glass front over a stack of 1U servers (one is the "pizza
   * box" that runs the terminal). A monitor and keyboard sit on a shelf on top, tilted toward you. */
  fxp_box(v3_(RACK.x, 0.9f, RACK.z), v3_(0.62f, 1.8f, 0.52f), 0x2b2f35);
  fxp_box(v3_(RACK.x, 0.95f, RACK.z+0.27f), v3_(0.52f, 1.5f, 0.02f), 0x14161a);        /* glass door */
  for (int i = 0; i < 7; i++) fxp_box(v3_(RACK.x, 0.35f+i*0.2f, RACK.z+0.26f), v3_(0.48f, 0.15f, 0.04f), i==3 ? 0x3a3f46 : 0x23262c);  /* 1U servers; the pizza box lighter */
  fxp_box(v3_(RACK.x, 1.82f, RACK.z), v3_(0.64f, 0.04f, 0.54f), 0x22262b);              /* the top shelf */
  fxp_box(v3_(RACK.x, 1.96f, RACK.z+0.02f), v3_(0.42f, 0.30f, 0.03f), 0x1a1d22);        /* the monitor */
  fxp_box(v3_(RACK.x, 1.86f, RACK.z+0.26f), v3_(0.40f, 0.03f, 0.16f), 0x2a2e34);        /* the keyboard */
  t3_object *g = fxp_end(); t3_object_add(&SC->base, g); env_grp = g; env_grp->visible = false;

  /* the glow layer (emissive, additive): the red X paint, a soft window, the terminal screen, the LEDs */
  fxp_begin(); fxp_layer(FXP_GLOW);
  for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) {
    if (!SOFT[r][c]) continue;
    float x = col_x(c), z = row_z(r), s = 0.36f, h = 0.016f;
    fxp_bar(v3_(x-s,h,z-s), v3_(x+s,h,z+s), 0.055f, 0x7a1410);
    fxp_bar(v3_(x-s,h,z+s), v3_(x+s,h,z-s), 0.055f, 0x7a1410);
  }
  fxp_quad(v3_(2.8f, 1.0f, -RZ+0.03f), v3_(4.8f, 1.0f, -RZ+0.03f), v3_(4.8f, 2.5f, -RZ+0.03f), v3_(2.8f, 2.5f, -RZ+0.03f), 0x4a3a1c);  /* a soft daylight window */
  fxp_box(v3_(RACK.x, 1.97f, RACK.z+0.055f), v3_(0.36f, 0.24f, 0.02f), 0x2fd06a);       /* the terminal screen */
  for (int i = 0; i < 6; i++) fxp_box(v3_(RACK.x-0.34f, 0.35f+i*0.2f, RACK.z+0.27f), v3_(0.015f, 0.02f, 0.01f), (i%3) ? 0x30c050 : 0x40b0d0);  /* the rack LEDs */
  t3_object *gg = fxp_end(); t3_object_add(g, gg);   /* reparent the glow under the room so it hides with it */

  /* the glitch accents: a red alarm bar and a flickering bulb, shown only while the room is wrong */
  fxp_begin(); fxp_layer(FXP_GLOW);
  fxp_box(v3_(RACK.x, 1.55f, RACK.z+0.27f), v3_(0.44f, 0.04f, 0.02f), 0xff2b2b);
  fxp_box(v3_(0, CEIL-0.25f, 1.5f), v3_(0.18f, 0.06f, 0.18f), 0xbfe0ff);
  t3_object *gl = fxp_end(); t3_object_add(&SC->base, gl); glitch_grp = gl; glitch_grp->visible = false;
}

void attic_env_init(t3_scene *scene) { if (SC) return; SC = scene; build_env(); }

/* ---------------- queries the gameplay uses ---------------- */
bool attic_env_soft(v3 p) {
  int c = (int)floorf((p.x - GX0) / GXD + 0.5f), r = (int)floorf((p.z - GZ0) / GZD + 0.5f);
  if (c < 0 || c >= COLS || r < 0 || r >= ROWS || !SOFT[r][c]) return false;
  return hypotf(p.x - col_x(c), p.z - row_z(r)) < SOFT_R;
}
void attic_env_reset(float normal) { env_normal = ramp_to = normal; }
void attic_env_target(float normal) { ramp_to = normal; }
float attic_env_level(void) { return env_normal; }

/* ---------------- shown / collision / raycast / mood (world.c calls these for STAGE_ATTIC) ---------------- */
void attic_env_show(bool on) {
  if (!on && !env_built) return;   /* world_init hides stages before attic_env_init builds anything */
  build_env();
  env_grp->visible = on;
  glitch_grp->visible = on && env_normal < 0.5f;
}

void attic_env_floor(v3 *center, float *radius) { if (center) *center = v3_(0,0,0); if (radius) *radius = 8.0f; }

void attic_env_push(v3 *p, float r, float h, v3 *n, bool *hit) {
  (void)h;
  if (p->x < -RX+0.25f+r) { p->x = -RX+0.25f+r; *n = v3_(1,0,0); *hit = true; }
  if (p->x >  RX-0.25f-r) { p->x =  RX-0.25f-r; *n = v3_(-1,0,0); *hit = true; }
  if (p->z < -RZ+0.25f+r) { p->z = -RZ+0.25f+r; *n = v3_(0,0,1); *hit = true; }
  if (p->z >  RZ-0.25f-r) { p->z =  RZ-0.25f-r; *n = v3_(0,0,-1); *hit = true; }
  for (int i = 0; i < NSOLID; i++) {
    const box_t *b = &SOLID[i];
    if (p->y > b->top - 0.05f) continue;
    float qx = ds_clamp(p->x, b->cx-b->hx, b->cx+b->hx), qz = ds_clamp(p->z, b->cz-b->hz, b->cz+b->hz);
    float dx = p->x-qx, dz = p->z-qz, d2 = dx*dx+dz*dz;
    if (d2 >= r*r) continue;
    if (d2 > 1e-8f) { float d = sqrtf(d2); p->x = qx + dx/d*r; p->z = qz + dz/d*r; *n = v3_(dx/d,0,dz/d); }
    else {   /* inside: out the nearest face */
      float l=p->x-(b->cx-b->hx), rr=(b->cx+b->hx)-p->x, f=p->z-(b->cz-b->hz), bk=(b->cz+b->hz)-p->z, m=fminf(fminf(l,rr),fminf(f,bk));
      if (m==l){p->x=b->cx-b->hx-r;*n=v3_(-1,0,0);} else if(m==rr){p->x=b->cx+b->hx+r;*n=v3_(1,0,0);}
      else if(m==f){p->z=b->cz-b->hz-r;*n=v3_(0,0,-1);} else {p->z=b->cz+b->hz+r;*n=v3_(0,0,1);}
    }
    *hit = true;
  }
}

static bool ray_aabb(v3 o, v3 d, v3 mn, v3 mx, float *t0, float *t1) {
  float tmin = -1e9f, tmax = 1e9f;
  for (int a = 0; a < 3; a++) {
    float od = (&o.x)[a], dd = (&d.x)[a], lo = (&mn.x)[a], hi = (&mx.x)[a];
    if (fabsf(dd) < 1e-8f) { if (od < lo || od > hi) return false; continue; }
    float ta = (lo-od)/dd, tb = (hi-od)/dd; if (ta > tb) { float s=ta; ta=tb; tb=s; }
    tmin = fmaxf(tmin, ta); tmax = fminf(tmax, tb);
    if (tmin > tmax) return false;
  }
  *t0 = tmin; *t1 = tmax; return true;
}

bool attic_env_raycast(v3 o, v3 d, float max_dist, float *t_out, v3 *n_out) {
  float best = max_dist; bool any = false; v3 bn = v3_(0,1,0);
  float t0, t1;
  if (ray_aabb(o, d, v3_(-RX,0,-RZ), v3_(RX,CEIL,RZ), &t0, &t1) && t1 > 0 && t1 < best) { best = t1; any = true; bn = v3_(0,1,0); }
  for (int i = 0; i < NSOLID; i++) {
    const box_t *b = &SOLID[i];
    if (ray_aabb(o, d, v3_(b->cx-b->hx,0,b->cz-b->hz), v3_(b->cx+b->hx,b->top,b->cz+b->hz), &t0, &t1) && t0 > 0 && t0 < best) { best = t0; any = true; bn = v3_(o.x<b->cx?-1:1,0,0); }
  }
  if (any) { *t_out = best; *n_out = bn; }
  return any;
}

void attic_env_mood(arena_mood *m) {
  float t = ds_clamp(env_normal, 0, 1);
  /* wrong: dim, cold, green cast, close fog, flicker. normal: warm dusty attic. */
  arena_mood wrong = { t3_color_hex(0x0a1210), t3_color_hex(0x10201a), t3_color_hex(0x0c1a14),
    t3_color_hex(0x244c3e), t3_color_hex(0x10160f), t3_color_hex(0x40ff8a), 0.85f*env_flick, 0.7f*env_flick, 3.f, 22.f, v3_norm(v3_(0.2f,0.7f,-0.6f)), 0 };
  arena_mood norm = { t3_color_hex(0x3a301f), t3_color_hex(0x40321f), t3_color_hex(0x3c2e1c),
    t3_color_hex(0xece0b8), t3_color_hex(0x564634), t3_color_hex(0xffe8b8), 1.75f, 1.7f, 10.f, 60.f, v3_norm(v3_(0.5f,0.55f,-0.6f)), 0 };
#define CM(f) (t3_color){ wrong.f.r+(norm.f.r-wrong.f.r)*t, wrong.f.g+(norm.f.g-wrong.f.g)*t, wrong.f.b+(norm.f.b-wrong.f.b)*t }
  m->zenith = CM(zenith); m->horizon = CM(horizon); m->fog = CM(fog);
  m->hemi_sky = CM(hemi_sky); m->hemi_ground = CM(hemi_ground); m->sun = CM(sun);
  m->hemi = ds_lerp(wrong.hemi, norm.hemi, t); m->dir = ds_lerp(wrong.dir, norm.dir, t);
  m->fog_near = ds_lerp(wrong.fog_near, norm.fog_near, t); m->fog_far = ds_lerp(wrong.fog_far, norm.fog_far, t);
  m->sun_dir = v3_norm(v3_lerp(wrong.sun_dir, norm.sun_dir, t)); m->stars = 0;
#undef CM
}

bool attic_env_update(float dt) {
  float f = env_normal < 0.5f ? (0.55f + 0.45f * (sinf(ds_now()*23)*0.5f+0.5f) * (ds_rand() < 0.1f ? 0.3f : 1.f)) : 1.f;
  float old_n = env_normal, old_f = env_flick;
  env_flick = ds_damp(env_flick, f, 12, dt);
  env_normal = ds_damp(env_normal, ramp_to, 2.2f, dt);
  if (glitch_grp) glitch_grp->visible = env_grp && env_grp->visible && env_normal < 0.5f;
  return fabsf(env_normal-old_n) > 0.002f || fabsf(env_flick-old_f) > 0.01f;
}
