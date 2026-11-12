/* attic.c: the roped-off attic, its own stage (STAGE_ATTIC) and the hub built on it. See attic.h.
 *
 * A purpose-built room, not the real crawlspace mesh: flat floor at y=0, ~14x12 m, 3.6 m ceiling, its own
 * lighting (world.c routes STAGE_ATTIC collision/raycast/mood here). You arrive with the room glitched and
 * wrong; cross the soft floor (red X = a board that gives) through the junk to the server rack, reboot it,
 * and the room snaps back to normal. After that it is a hub: walk it freely, replay the run, examine old
 * things (each plays a memory), and use the rack's pizza-box terminal (terminal.c). */
#if !defined(KH_TEST_WORLD) && !defined(KH_TEST_FIGHT)
#include "attic.h"
#include "terminal.h"
#include "story.h"
#include "player.h"
#include "world.h"
#include "actor.h"
#include "fx.h"
#include "audio.h"
#include "hud_int.h"
#include "ds.h"
#include <string.h>
#include <math.h>

/* ---------------- the room ---------------- */
#define RX   7.0f          /* half width (x) */
#define RZ   6.0f          /* half depth (z) */
#define CEIL 3.6f
static const v3 ENTRY = { 0.0f, 0.0f, 5.0f };   /* the stair head, by the south wall, facing in */
static const v3 RACK  = { -3.3f, 0.0f, -5.2f };  /* the server rack (and its terminal), north-west */

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

/* solid boxes that block the player and stop the camera: the shelving, the rack, the terminal crate, junk.
 * Axis aligned {cx, cz, hx, hz, top}. The four walls are handled as room bounds, not boxes. */
typedef struct { float cx, cz, hx, hz, top; } box_t;
static const box_t SOLID[] = {
  { 6.3f,  0.5f, 0.45f, 4.5f, 2.3f },   /* east wall shelving */
  {-6.3f,  2.8f, 0.45f, 2.6f, 2.3f },   /* west wall shelving (south half; the rack end stays clear) */
  { 0.0f, -5.4f, 1.3f,  0.5f, 1.7f },   /* a run of totes along the north wall, east of the rack */
  { 5.2f, -4.2f, 0.7f,  0.7f, 1.1f },   /* a junk pile in the north-east corner */
  {-3.3f, -5.3f, 0.6f,  0.5f, 1.6f },   /* the server rack */
  {-1.3f, -5.3f, 0.55f, 0.45f, 1.0f },  /* the terminal crate */
};
#define NSOLID ((int)(sizeof SOLID / sizeof SOLID[0]))

/* ---------------- environment: geometry, collision, mood ---------------- */
static t3_scene *SC; static t3_camera *CAM;
static t3_object *env_grp, *glitch_grp;            /* the room; the red/flicker accents shown while wrong */
static bool env_built;
static float env_normal;                           /* 0 glitched .. 1 normal (ramps up on the reboot) */
static float env_flick;                            /* the glitch light flicker, per frame */

static uint32_t wire(uint32_t a, uint32_t b, float t) {   /* lerp two 0xRRGGBB */
  int ar=(a>>16)&255, ag=(a>>8)&255, ab=a&255, br=(b>>16)&255, bg=(b>>8)&255, bb=b&255;
  int r=(int)(ar+(br-ar)*t), g=(int)(ag+(bg-ag)*t), bl=(int)(ab+(bb-ab)*t);
  return ((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)bl;
}

static void build_env(void) {
  if (env_built) return;
  env_built = true;
  const uint32_t WOOD_A = 0x6b5436, WOOD_B = 0x5a4530, WALL = 0x4b463d, BEAM = 0x3a2f24, TAPE = 0xc23a2c, SCORCH = 0x241c15;
  fxp_begin(); fxp_layer(FXP_BODY);
  /* the plank floor: boards running in z, alternating shade */
  for (int i = 0; i < 28; i++) {
    float x0 = -RX + i * (2*RX/28), x1 = x0 + (2*RX/28);
    uint32_t c = (i & 1) ? WOOD_A : WOOD_B;
    fxp_quad(v3_(x0, 0, -RZ), v3_(x1, 0, -RZ), v3_(x1, 0, RZ), v3_(x0, 0, RZ), c);
  }
  /* the walls (inner faces) and a ceiling */
  fxp_quad(v3_(-RX,0,RZ), v3_(RX,0,RZ), v3_(RX,CEIL,RZ), v3_(-RX,CEIL,RZ), WALL);          /* south */
  fxp_quad(v3_(RX,0,-RZ), v3_(-RX,0,-RZ), v3_(-RX,CEIL,-RZ), v3_(RX,CEIL,-RZ), WALL);      /* north */
  fxp_quad(v3_(-RX,0,-RZ), v3_(-RX,0,RZ), v3_(-RX,CEIL,RZ), v3_(-RX,CEIL,-RZ), WALL);      /* west */
  fxp_quad(v3_(RX,0,RZ), v3_(RX,0,-RZ), v3_(RX,CEIL,-RZ), v3_(RX,CEIL,RZ), WALL);          /* east */
  fxp_quad(v3_(-RX,CEIL,-RZ), v3_(RX,CEIL,-RZ), v3_(RX,CEIL,RZ), v3_(-RX,CEIL,RZ), BEAM);  /* ceiling */
  for (float x = -RX+1.4f; x < RX; x += 2.1f)                                              /* rafters */
    fxp_box(v3_(x, CEIL-0.12f, 0), v3_(0.14f, 0.2f, 2*RZ), BEAM);
  /* the scorched patches under the red X paint (the X itself is emissive, built in the glow pass) */
  (void)TAPE;
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
  /* the server rack: a tall dark cabinet, a glass front over a stack of 1U servers (one is the
   * "pizza box" that runs the terminal). A small monitor and keyboard sit on a shelf on top, tilted
   * toward you: that is the terminal you use. */
  fxp_box(v3_(RACK.x, 0.9f, RACK.z), v3_(0.62f, 1.8f, 0.52f), 0x2b2f35);
  fxp_box(v3_(RACK.x, 0.95f, RACK.z+0.27f), v3_(0.52f, 1.5f, 0.02f), 0x14161a);        /* glass door */
  for (int i = 0; i < 7; i++) fxp_box(v3_(RACK.x, 0.35f+i*0.2f, RACK.z+0.26f), v3_(0.48f, 0.15f, 0.04f), i==3 ? 0x3a3f46 : 0x23262c);  /* 1U servers; the pizza box lighter */
  /* a shelf on top with the monitor and keyboard */
  fxp_box(v3_(RACK.x, 1.82f, RACK.z), v3_(0.64f, 0.04f, 0.54f), 0x22262b);
  fxp_box(v3_(RACK.x, 1.96f, RACK.z+0.02f), v3_(0.42f, 0.30f, 0.03f), 0x1a1d22);        /* the monitor, tilted up */
  fxp_box(v3_(RACK.x, 1.86f, RACK.z+0.26f), v3_(0.40f, 0.03f, 0.16f), 0x2a2e34);        /* the keyboard */
  t3_object *g = fxp_end(); t3_object_add(&SC->base, g); env_grp = g; env_grp->visible = false;

  /* the glow layer (emissive, additive): the red X paint, a soft window, the terminal screen */
  fxp_begin(); fxp_layer(FXP_GLOW);
  for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) {
    if (!SOFT[r][c]) continue;
    float x = col_x(c), z = row_z(r), s = 0.36f, h = 0.016f;
    fxp_bar(v3_(x-s,h,z-s), v3_(x+s,h,z+s), 0.055f, 0x7a1410);
    fxp_bar(v3_(x-s,h,z+s), v3_(x+s,h,z-s), 0.055f, 0x7a1410);
  }
  fxp_quad(v3_(2.8f, 1.0f, -RZ+0.03f), v3_(4.8f, 1.0f, -RZ+0.03f), v3_(4.8f, 2.5f, -RZ+0.03f), v3_(2.8f, 2.5f, -RZ+0.03f), 0x4a3a1c);  /* a soft daylight window */
  fxp_box(v3_(RACK.x, 1.97f, RACK.z+0.055f), v3_(0.36f, 0.24f, 0.02f), 0x2fd06a);  /* the terminal monitor's green screen, on top of the rack */
  for (int i = 0; i < 6; i++)   /* the rack's status LEDs, live after the reboot */
    fxp_box(v3_(RACK.x-0.36f+0.02f, 0.35f+i*0.2f, RACK.z+0.27f), v3_(0.015f, 0.02f, 0.01f), (i%3) ? 0x30c050 : 0x40b0d0);
  t3_object *gg = fxp_end(); t3_object_add(g, gg);   /* reparent the glow under the room so it hides with it */

  /* the glitch accents: red hazard strips and a flickering bulb, shown only while the room is wrong */
  fxp_begin(); fxp_layer(FXP_GLOW);
  fxp_box(v3_(RACK.x, 1.55f, RACK.z+0.27f), v3_(0.44f, 0.04f, 0.02f), 0xff2b2b);   /* rack alarm bar */
  fxp_box(v3_(0, CEIL-0.25f, 1.5f), v3_(0.18f, 0.06f, 0.18f), 0xbfe0ff);           /* a bare bulb */
  t3_object *gl = fxp_end(); t3_object_add(&SC->base, gl); glitch_grp = gl; glitch_grp->visible = false;
}

void attic_env_show(bool on) {
  if (!on && !env_built) return;   /* world_init hides stages before attic_init builds anything */
  build_env();
  env_grp->visible = on;
  glitch_grp->visible = on && env_normal < 0.5f;
}

void attic_env_floor(v3 *center, float *radius) { if (center) *center = v3_(0,0,0); if (radius) *radius = 8.0f; }

/* push the player inside the room and out of the solid boxes */
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

/* for the follow camera: how far a ray gets before the room shell or a solid box */
bool attic_env_raycast(v3 o, v3 d, float max_dist, float *t_out, v3 *n_out) {
  float best = max_dist; bool any = false; v3 bn = v3_(0,1,0);
  float t0, t1;
  /* the room: the ray exits the shell at t1 */
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
  arena_mood norm = { t3_color_hex(0x2e2619), t3_color_hex(0x34291b), t3_color_hex(0x33271a),
    t3_color_hex(0xe6d2a6), t3_color_hex(0x4a3c2c), t3_color_hex(0xffe3ad), 1.35f, 1.55f, 8.f, 55.f, v3_norm(v3_(0.5f,0.55f,-0.6f)), 0 };
#define CM(f) (t3_color){ wrong.f.r+(norm.f.r-wrong.f.r)*t, wrong.f.g+(norm.f.g-wrong.f.g)*t, wrong.f.b+(norm.f.b-wrong.f.b)*t }
  m->zenith = CM(zenith); m->horizon = CM(horizon); m->fog = CM(fog);
  m->hemi_sky = CM(hemi_sky); m->hemi_ground = CM(hemi_ground); m->sun = CM(sun);
  m->hemi = ds_lerp(wrong.hemi, norm.hemi, t); m->dir = ds_lerp(wrong.dir, norm.dir, t);
  m->fog_near = ds_lerp(wrong.fog_near, norm.fog_near, t); m->fog_far = ds_lerp(wrong.fog_far, norm.fog_far, t);
  m->sun_dir = v3_norm(v3_lerp(wrong.sun_dir, norm.sun_dir, t)); m->stars = 0;
#undef CM
}

static float ramp_to = 0;   /* env_normal eases toward this */
bool attic_env_update(float dt) {
  float f = env_normal < 0.5f ? (0.55f + 0.45f * (sinf(ds_now()*23)*0.5f+0.5f) * (ds_rand() < 0.1f ? 0.3f : 1.f)) : 1.f;
  float old_n = env_normal, old_f = env_flick;
  env_flick = ds_damp(env_flick, f, 12, dt);
  env_normal = ds_damp(env_normal, ramp_to, 2.2f, dt);
  if (glitch_grp) glitch_grp->visible = env_grp && env_grp->visible && env_normal < 0.5f;
  return fabsf(env_normal-old_n) > 0.002f || fabsf(env_flick-old_f) > 0.01f;
}

/* ---------------- the mode ---------------- */
static enum { A_PLAY, A_FELL, A_WON, A_HUB, A_MEMORY, A_TERMINAL } phase;
static float phase_t, run_t, creak_t;
static int tries;
static bool was_won, near_use;
static int use_kind, use_id;                       /* the examinable in reach */
static v3 ret_pos; static float ret_yaw;
static unsigned games_mask;                        /* hacking minigames unlocked (persisted) */
static uint16_t found_mask;                        /* examinables whose memory has played (persisted) */
static int mem_id;                                 /* the memory now playing */

/* examinables in the hub: old things that hold a memory, the replay spot, the terminal */
enum { EX_MEMORY, EX_REPLAY, EX_TERMINAL };
typedef struct { v3 pos; float r; int kind; int id; const char *label; } exam_t;
static const exam_t EXAM[] = {
  { { -3.3f, 0, -3.9f }, 1.6f, EX_TERMINAL, 0, "USE TERMINAL" },   /* in front of the rack */
  { {  5.2f, 0, -3.3f }, 1.4f, EX_MEMORY, 0, "EXAMINE" },   /* the dead CRT */
  { { -6.0f, 0,  3.0f }, 1.4f, EX_MEMORY, 1, "EXAMINE" },   /* a tote on the west shelf */
  { {  4.2f, 0,  4.4f }, 1.6f, EX_REPLAY, 0, "RUN THE DRILL" },
};
#define NEXAM ((int)(sizeof EXAM / sizeof EXAM[0]))

/* the memories, keyed by id */
static const struct { const char *title, *l1, *l2; } MEM[] = {
  { "THE OLD RENDER NODE", "This CRT ran the first wiki, back when the space was one room.", "Somebody soldered the power switch on. It never turned off." },
  { "A BOX OF BADGES", "Laminated member badges, a few names you still see on the calendar.", "Most of them learned to solder at that bench downstairs." },
};
#define NMEM ((int)(sizeof MEM / sizeof MEM[0]))

/* ---------------- the entrance: a roped-off stair with a little PVC sign ---------------- */
static void build_sign(void) {
  const uint32_t PVC = 0xe9e9e4, POST = 0x8a8f96, ROPE = 0x6b1f1f, SIGN = 0xf2efe6, RED = 0xc62828;
  float z = -32.0f, x0 = -0.35f, x1 = 1.15f;
  fxp_begin(); fxp_layer(FXP_BODY);
  /* two stanchion posts with a sagging rope across the stair mouth */
  fxp_cyl(v3_(x0, 0, z), 1.02f, 0.03f, 0.03f, 8, POST); fxp_cyl(v3_(x0, 1.0f, z), 0.06f, 0.05f, 0.05f, 8, POST);
  fxp_cyl(v3_(x1, 0, z), 1.02f, 0.03f, 0.03f, 8, POST); fxp_cyl(v3_(x1, 1.0f, z), 0.06f, 0.05f, 0.05f, 8, POST);
  fxp_bar(v3_(x0, 0.92f, z), v3_((x0+x1)/2, 0.78f, z), 0.02f, ROPE);
  fxp_bar(v3_((x0+x1)/2, 0.78f, z), v3_(x1, 0.92f, z), 0.02f, ROPE);
  /* a small PVC-framed sign hung on the rope */
  float sx = (x0+x1)/2, sy = 0.5f;
  fxp_bar(v3_(sx-0.03f, 0.78f, z), v3_(sx-0.03f, sy, z), 0.012f, PVC);   /* the hanger */
  fxp_box(v3_(sx, sy-0.14f, z), v3_(0.5f, 0.32f, 0.02f), SIGN);          /* the panel */
  fxp_box(v3_(sx, sy-0.02f, z+0.015f), v3_(0.52f, 0.06f, 0.012f), RED);  /* a red bar top */
  fxp_box(v3_(sx, sy-0.26f, z+0.015f), v3_(0.52f, 0.06f, 0.012f), RED);  /* a red bar bottom */
  /* the PVC frame around the panel: four thin white pipes */
  fxp_cyl(v3_(sx-0.27f, sy-0.30f, z+0.01f), 0.32f, 0.018f, 0.018f, 6, PVC);
  fxp_cyl(v3_(sx+0.27f, sy-0.30f, z+0.01f), 0.32f, 0.018f, 0.018f, 6, PVC);
  fxp_bar(v3_(sx-0.28f, sy-0.30f, z+0.01f), v3_(sx+0.28f, sy-0.30f, z+0.01f), 0.018f, PVC);
  fxp_bar(v3_(sx-0.28f, sy+0.02f, z+0.01f), v3_(sx+0.28f, sy+0.02f, z+0.01f), 0.018f, PVC);
  t3_object *s = fxp_end(); world_add_prop(s);
}

void attic_init(t3_scene *scene, t3_camera *cam) {
  if (SC) return;
  SC = scene; CAM = cam;
  build_sign();
  build_env();
  term_init();
}

/* ---------------- enter / leave ---------------- */
static void dodge_arm(void) { phase = A_PLAY; phase_t = 0; run_t = 0; creak_t = 0; tries = 0; }

void attic_enter(void) {
  ret_pos = player_pos(); ret_yaw = player_yaw();
  was_won = false; mem_id = -1;
  { uint16_t f, g; story_attic_masks(&f, &g); found_mask = f; games_mask = g ? g : 1; }
  bool beaten = story_attic_beaten();
  attic_env_show(true);
  world_set_stage(STAGE_ATTIC);
  music_mood(beaten ? MOOD_NIGHT : MOOD_TENSION);
  if (beaten) { env_normal = 1; ramp_to = 1; phase = A_HUB; phase_t = 0; player_teleport(ENTRY, DS_PI); }
  else { env_normal = 0; ramp_to = 0; dodge_arm(); player_teleport(ENTRY, DS_PI); }
  sfx(SFX_STEP, 0, .4f);
}

static void leave(void) {
  story_attic_set_masks(found_mask, games_mask);
  attic_env_show(false);
  world_set_stage(STAGE_TOWN);          /* apply_lights repaints the town from the (unchanged) story clock */
  player_teleport(ret_pos, ret_yaw);
  player_camera_snap();
  music_mood(MOOD_HOSTING);             /* back in the lab; the next scene resets it if need be */
}

/* walk-only intent: no jumping (kept so nothing clips), no weapons, no bike */
static intent_t walk_only(const intent_t *in) {
  intent_t w = *in;
  w.jump = w.jump_held = w.light = w.heavy = w.special = w.dodge = false;
  w.light_held = w.heavy_held = w.guard = w.guard_pressed = w.draw = w.bike = w.assist = w.lock = false;
  w.weapon_step = 0; w.weapon_pick = -1; w.wheel = 0;
  return w;
}

static bool on_soft_cell(v3 p) {
  int c = (int)floorf((p.x - GX0) / GXD + 0.5f), r = (int)floorf((p.z - GZ0) / GZD + 0.5f);
  if (c < 0 || c >= COLS || r < 0 || r >= ROWS || !SOFT[r][c]) return false;
  return hypotf(p.x - col_x(c), p.z - row_z(r)) < SOFT_R;
}

/* ---------------- update ---------------- */
bool attic_update(float dt, const intent_t *in) {
  phase_t += dt;

  if (phase == A_TERMINAL) {
    if (!term_update(dt, in)) { games_mask = term_unlocked(); phase = A_HUB; phase_t = 0; player_camera_snap(); }
    world_update(dt, player_pos());
    return true;
  }
  if (phase == A_MEMORY) {
    world_update(dt, player_pos());
    if (phase_t > 1.0f && (in->ok || in->interact || in->back || phase_t > 7.0f)) { phase = A_HUB; phase_t = 0; }
    return true;
  }

  intent_t w = walk_only(in);
  player_update(dt, &w);
  world_update(dt, player_pos());
  v3 p = player_pos();

  if (phase == A_PLAY) {
    run_t += dt;
    if (on_soft_cell(p)) {
      if (creak_t == 0) sfx(SFX_STEP, 0, .5f);
      creak_t += dt;
      fx_glow(v3_(p.x, 0.03f, p.z), 0.5f + 0.2f*sinf(run_t*40), 0xe23b2e);
      if (creak_t > 0.10f) {
        phase = A_FELL; phase_t = 0; tries++;
        actor_play(player_actor(), "Hit_Knockback", 1, false, .05f);
        fx_dust(v3_(p.x, 0, p.z), 1.2f, 22); fx_smoke(v3_(p.x, -0.1f, p.z), 0.8f, 0x6b5436, 10, 0.4f);
        sfx(SFX_BODY_FALL, 0, .9f); sfx(SFX_SLAM, 0, .5f);
      }
    } else creak_t = 0;
    /* reach the rack: reboot, and the room comes back */
    if (hypotf(p.x-RACK.x, p.z-RACK.z) < 1.4f) {
      phase = A_WON; phase_t = 0; was_won = true; ramp_to = 1;
      fx_burst(v3_(RACK.x, 0.9f, RACK.z+0.3f), 0x58d06a, 1.1f);
      fx_motes(v3_(RACK.x, 1.0f, RACK.z+0.3f), 0x8fe6a0, 26, 0.6f);
      sfx(SFX_PICKUP, 0, 1); music_mood(MOOD_CLEAR);
    }
    if (in->back) { leave(); return false; }
  } else if (phase == A_FELL) {
    if (in->back) { leave(); return false; }
    if (phase_t > 1.2f && (in->interact || in->ok || phase_t > 3.0f)) { player_teleport(ENTRY, DS_PI); phase = A_PLAY; phase_t = 0; creak_t = 0; }
  } else if (phase == A_WON) {
    fx_glow(v3_(RACK.x, 1.1f, RACK.z+0.3f), 0.35f, 0x58d06a);
    if (phase_t > 2.6f) { phase = A_HUB; phase_t = 0; music_mood(MOOD_NIGHT); }
  } else {   /* A_HUB */
    near_use = false; use_kind = -1;
    float best = 1e9f;
    for (int i = 0; i < NEXAM; i++) {
      float d = hypotf(p.x-EXAM[i].pos.x, p.z-EXAM[i].pos.z);
      if (d < EXAM[i].r && d < best) { best = d; near_use = true; use_kind = EXAM[i].kind; use_id = EXAM[i].id; }
    }
    if (near_use && in->interact) {
      if (use_kind == EX_TERMINAL) { phase = A_TERMINAL; phase_t = 0; term_open(games_mask); sfx(SFX_UI_OK,0,1); }
      else if (use_kind == EX_REPLAY) { ramp_to = 1; dodge_arm(); player_teleport(ENTRY, DS_PI); sfx(SFX_UI_OK,0,1); }
      else if (use_kind == EX_MEMORY) { mem_id = use_id % NMEM; found_mask |= (uint16_t)(1u << use_id); phase = A_MEMORY; phase_t = 0; sfx(SFX_UI,0,1); }
    }
    if (in->back) { leave(); return false; }
  }
  return true;
}

bool attic_won(void) { return was_won; }

/* ---------------- HUD ---------------- */
#define AT_INK 0x0c0f14u
#define AT_PAPER 0xf4eee2u
#define AT_RED 0xe23b2eu
#define AT_GREEN 0x58d06au
#define AT_YEL 0xf2c21eu
static layer_t L_top, L_obj, L_banner, L_mem;

void attic_draw(void) {
  if (phase == A_TERMINAL) { term_draw(); return; }
  bool pad = input_get()->used_pad;   /* device-aware button glyphs, like the rest of the HUD */
  hg_begin();
  /* the tag and objective (not in the terminal or a memory) */
  if (phase != A_MEMORY) {
    cv *c = layer_begin(&L_top, 360, 92, 0xA77);
    if (c) { p_hazard(c, 0, 0, 360, 10, 26, AT_YEL, AT_INK);
      p_text(c, F_DISPLAY, 34, 1, CV_LEFT, "THE ATTIC", 14, 52, AT_PAPER, 1);
      p_text(c, F_MONO, 15, 1, CV_LEFT, phase==A_HUB||phase==A_WON ? "AUTHORIZED, APPARENTLY" : "WATCH YOUR STEP", 15, 78, AT_YEL, 1);
      layer_end(&L_top, c); }
    hg_tex(&L_top, 44, 36, 1);

    uint32_t key = (uint32_t)phase*16 + (near_use ? (uint32_t)use_kind+1 : 0)*4 + (uint32_t)(tries<<8);
    cv *cc = layer_begin(&L_obj, 600, 40, 0xB00 ^ key ^ (pad?0x40000:0));
    if (cc) { p_plate(cc, 0, 0, 600, 40, 10, AT_INK, .82f);
      const char *s = "Reach the server rack. Keep off the red X.";
      if (phase == A_HUB) s = near_use
        ? (use_kind==EX_TERMINAL ? "USE THE TERMINAL  {USE}" : use_kind==EX_REPLAY ? "RUN THE DRILL  {USE}" : "EXAMINE  {USE}")
        : "The attic is yours. Look around.";
      g_markup(cc, 16, 26, CV_LEFT, F_TYPE, 20, .1f, AT_PAPER, 20, pad, s, true);
      if (tries > 0 && phase == A_PLAY) { char t[24]; snprintf(t,sizeof t,"FELL: %d", tries); p_text(cc, F_MONO, 15, 1, CV_LEFT, t, 500, 25, AT_RED, 1); }
      layer_end(&L_obj, cc); }
    hg_tex(&L_obj, 44, 648, 1);
  }
  /* win / fail banners */
  if (phase == A_FELL || phase == A_WON) {
    bool won = phase == A_WON; float a = hud_ease_out(phase_t*3);
    hg_rect(0, 280, 1280, 160, won ? 0x0b2414 : 0x2a0e0a, .68f*a);
    cv *c = layer_begin(&L_banner, 920, 150, (won ? 0xD01u : 0xD02u) ^ (pad?0x40000u:0u));
    if (c) { p_text(c, F_DISPLAY, 58, 2, CV_CENTER, won ? "BACK ONLINE" : "THROUGH THE FLOOR", 460, 64, won ? AT_GREEN : AT_RED, 1);
      p_text(c, F_TYPE, 22, 0, CV_CENTER, won ? "Server up. The room settles. Members site, wiki, door controller." : "A boot through the ceiling.", 460, 102, AT_PAPER, 1);
      if (!won) g_markup(c, 460, 132, CV_CENTER, F_MONO, 15, 1, AT_YEL, 16, pad, "{USE} try again    {BACK} climb down", true);
      layer_end(&L_banner, c); }
    hg_tex_m(&L_banner, (float[6]){1,0,0,1,640-460,300}, a, 0xffffff);
  }
  /* a memory sequence: a sepia card over a dim frame */
  if (phase == A_MEMORY && mem_id >= 0) {
    float a = hud_ease_out(phase_t*2) * (phase_t > 6.0f ? hud_ease_out((7.0f-phase_t)*2) : 1);
    hg_rect(0, 0, 1280, 720, 0x140f08, .82f*a);
    hg_vignette(0x000000, .5f*a, .4f);
    cv *c = layer_begin(&L_mem, 900, 220, (0xE00u + (uint32_t)mem_id) ^ (pad?0x40000u:0u));
    if (c) { p_text(c, F_MONO, 15, 3, CV_CENTER, "A MEMORY", 450, 30, AT_YEL, 1);
      p_text(c, F_DISPLAY, 40, 1, CV_CENTER, MEM[mem_id].title, 450, 82, 0xf0e2c4, 1);
      p_text(c, F_TYPE, 22, 0, CV_CENTER, MEM[mem_id].l1, 450, 134, AT_PAPER, 1);
      p_text(c, F_TYPE, 22, 0, CV_CENTER, MEM[mem_id].l2, 450, 166, AT_PAPER, 1);
      g_markup(c, 450, 206, CV_CENTER, F_MONO, 14, 1, 0x9a8f7a, 15, pad, "{USE} let it go", true);
      layer_end(&L_mem, c); }
    hg_tex_m(&L_mem, (float[6]){1,0,0,1,640-450,250}, a, 0xffffff);
  }
  hg_end();
}
#endif
