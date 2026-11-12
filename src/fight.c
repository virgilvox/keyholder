/* fight.c: see fight.h. One state machine per fighter, moves as sections of the mocap
 * clips, hit windows read off the clips' hand speed, an AI that takes turns. Everything a
 * fighter does runs on its own local time, so hit stop freezes just the two who touched
 * and slow motion can leave the player quicker than the world. */
#include "fight_int.h"
#include "weapon.h"
#include "player.h"
#include "fx.h"
#include "audio.h"
#include "voice.h"
#include "enemy.h"
#include <stdlib.h>
#include <string.h>

fight_state_t FG;

static t3_scene *SC; static t3_camera *CAM;

/* ---- moves: clip seconds ---- */
enum { MF_HEAVY = 1, MF_KNOCK = 2, MF_BREAK = 4, MF_UNBLOCK = 8, MF_SWEEP = 16, MF_WAVE = 32, MF_SLAM = 64, MF_MULTI = 128, MF_SPIN = 256 };
typedef struct {
  const char *clip; float from, to, hit0, hit1, cancel;
  const char *rec;
  float dmg, stun, kb, arc, lunge; int flags;
} move_t;
enum { MV_L1, MV_L2, MV_L3, MV_L4, MV_H1, MV_H2, MV_H3, MV_H4, MV_SA, MV_DASH, MV_C, MV_WAVE, MV_SLAM, MV_FLURRY, MV_SPIN, MV_COUNT };
static const move_t MOVES[MV_COUNT] = {
  /*            clip                    from  to    hit0  hit1  cancel rec                dmg  stun kb   arc  lunge flags */
  [MV_L1] = { "Sword_Regular_Combo",   0.f, .43f, .19f, .31f, .29f, "Sword_Regular_A_Rec", 1.f, .36f, 1.4f, 1.25f, 1.3f, 0 },
  [MV_L2] = { "Sword_Regular_Combo",  .43f, .98f, .66f, .80f, .78f, "Sword_Regular_B_Rec", 1.f, .38f, 1.6f, 1.25f, 1.2f, 0 },
  [MV_L3] = { "Sword_Regular_Combo",  .98f, 2.3f, 1.46f, 1.62f, 1.72f, NULL,               1.8f, .6f, 3.6f, 1.4f, 1.5f, MF_SWEEP },
  [MV_L4] = { "Melee_Hook",            0.f, .47f, .2f, .33f, .3f, "Melee_Hook_Rec",        1.f, .36f, 1.4f, 1.25f, 1.2f, 0 },
  [MV_H1] = { "Sword_Heavy_Combo",     0.f, .8f,  .35f, .56f, .6f, "Sword_Regular_A_Rec",  1.6f, .55f, 2.8f, 1.3f, 1.4f, MF_HEAVY },
  [MV_H2] = { "Sword_Heavy_Combo",    .8f, 1.45f, 1.08f, 1.28f, 1.28f, "Sword_Regular_B_Rec", 1.7f, .55f, 2.8f, 1.3f, 1.3f, MF_HEAVY },
  [MV_H3] = { "Sword_Heavy_Combo",   1.45f, 2.2f, 1.72f, 1.92f, 1.95f, "Sword_Regular_A_Rec", 2.f, .7f, 4.f, 1.4f, 1.4f, MF_HEAVY | MF_SWEEP },
  [MV_H4] = { "Sword_Heavy_Combo",    2.2f, 3.6f, 2.48f, 2.68f, 2.9f, NULL,                2.6f, .8f, 5.f, 1.5f, 1.4f, MF_HEAVY | MF_KNOCK },
  [MV_SA] = { "Sword_Attack",          0.f, 1.3f, .35f, .52f, .75f, NULL,                  2.2f, .7f, 4.f, 1.2f, 1.6f, MF_HEAVY },
  [MV_DASH] = { "Sword_Dash",          0.f, 1.2f, .28f, .44f, .6f, NULL,                   1.5f, .5f, 3.4f, 1.1f, 5.f, 0 },
  [MV_C]  = { "Sword_Regular_C",       0.f, 1.3f, .61f, .77f, .85f, NULL,                  1.4f, .45f, 2.2f, 1.3f, 1.8f, 0 },
  [MV_WAVE] = { "Sword_Attack",        0.f, 1.2f, .35f, .5f, .8f, NULL,                    1.6f, .5f, 3.f, .1f, 0.f, MF_WAVE },
  /* the maul's ground slam: the head into the floor, a shock ring round it */
  [MV_SLAM] = { "Sword_Attack",        0.f, 1.35f, .36f, .5f, .9f, NULL,                   2.4f, .8f, 4.6f, 1.3f, 1.2f, MF_HEAVY | MF_SLAM },
  /* the twins' flurry: a run of quick cuts, many small hits */
  [MV_FLURRY] = { "Sword_Regular_Combo", .43f, 1.0f, .5f, .92f, .9f, "Sword_Regular_B_Rec", .42f, .25f, .5f, 1.3f, 1.1f, MF_MULTI },
  /* the naginata's spinning sweep: a full turn, everyone in reach */
  [MV_SPIN] = { "Sword_Regular_C",     0.f, 1.3f, .5f, .86f, .95f, NULL,                   1.9f, .6f, 4.2f, 3.2f, .6f, MF_HEAVY | MF_SWEEP | MF_SPIN },
};

typedef struct {
  float rate, dmg, reach, stop;
  int light[6], nlight, heavy[4], nheavy;
  float meter, kb, stun;
  int swing;
  const char *special;
} weapon_t;
static const weapon_t WP[WP_COUNT] = {
  [WP_ARC]   = { 1.15f, 5.2f, 1.65f, .055f, {MV_L1, MV_L2, MV_L3}, 3, {MV_H1, MV_H2, MV_H3}, 3, 6.5f, 1, 1, SFX_SWING_ARC, "FINAL WELD" },
  [WP_MAUL]  = { .80f, 9.5f, 1.75f, .12f, {MV_L1, MV_L3}, 2, {MV_SA, MV_SLAM}, 2, 11.f, 1.4f, 1.25f, SFX_SWING_MAUL, "COLD FORGE HAMMERFALL" },
  [WP_TWIN]  = { 1.5f, 3.1f, 1.2f, .035f, {MV_L1, MV_L2, MV_L4, MV_L1, MV_L2, MV_L3}, 6, {MV_FLURRY, MV_H2}, 2, 4.f, .8f, .8f, SFX_SWING_TWIN, "THOUSAND BURR CUT" },
  [WP_SPEAR] = { 1.f, 6.2f, 2.35f, .065f, {MV_L1, MV_L2, MV_L3}, 3, {MV_DASH, MV_SA, MV_SPIN}, 3, 7.5f, 1.1f, 1, SFX_SWING_SPEAR, "FIRST LAYER ADHESION" },
  [WP_STAFF] = { 1.25f, 4.6f, 1.6f, .03f, {MV_L1, MV_L2, MV_L3}, 3, {MV_H1}, 1, 5.f, 1, 1, SFX_SWING_ARC, "FULL POWER, NO AIR ASSIST" },
};

#define FOE_DMG .72f     /* foes hit softer than the player does with the same move */
#define PLAYER_R .34f
#define GRAVITY 16.f
#define RUN_V 4.4f
#define SPRINT_V 6.4f

/* ---- the actor pool ---- */
#define POOL_PER 2
static actor_t *POOL[AM_COUNT][POOL_PER]; static bool POOL_USED[AM_COUNT][POOL_PER];
static actor_t *pool_take(actor_model m, const actor_palette *pal, float scale) {
  for (int i = 0; i < POOL_PER; i++) {
    if (POOL_USED[m][i]) continue;
    if (!POOL[m][i]) POOL[m][i] = actor_new(m, &ACTOR_PAL_NONE, 1, SC);
    POOL_USED[m][i] = true;
    actor_repalette(POOL[m][i], pal);
    return POOL[m][i];
  }
  ds_log("fight: a third fighter of model %d, parsing one more", (int)m);
  return actor_new(m, pal, scale, SC);
}

/* ---- the duel ring on the ground ---- */
static t3_mesh *RING; static t3_material *RING_M;
static void ring_build(v3 c, float r) {
  if (RING) { t3_object_remove(&SC->base, RING); RING = NULL; }
  enum { SEG = 96, QV = SEG * 18 };
  float *P = malloc(sizeof(float) * 3 * QV * 3), *C = malloc(sizeof(float) * 3 * QV * 3);
  int n = 0;
  t3_color o = t3_color_hex(C_HIVIS);
#define PUT(X, Z, K) do { P[n*3] = c.x + (X); P[n*3+1] = c.y + .035f; P[n*3+2] = c.z + (Z); C[n*3] = o.r * (K); C[n*3+1] = o.g * (K); C[n*3+2] = o.b * (K); n++; } while (0)
  for (int i = 0; i < SEG; i++) {
    float a0 = i * 6.28318f / SEG, a1 = (i + 1) * 6.28318f / SEG, c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
    float rs[4] = { r - 1.1f, r - .1f, r - .03f, r + .05f }; float ks[4] = { 0, .35f, 2.4f, 2.4f };
    for (int b = 0; b < 3; b++) {
      PUT(c0*rs[b], s0*rs[b], ks[b]); PUT(c1*rs[b], s1*rs[b], ks[b]); PUT(c1*rs[b+1], s1*rs[b+1], ks[b+1]);
      PUT(c0*rs[b], s0*rs[b], ks[b]); PUT(c1*rs[b+1], s1*rs[b+1], ks[b+1]); PUT(c0*rs[b+1], s0*rs[b+1], ks[b+1]);
    }
    if (i % 4 == 0) {   /* slanted ticks outside it */
      float t0 = r + .13f, t1 = r + .42f, sk = .05f;
      float c2 = cosf(a0 + sk), s2 = sinf(a0 + sk), c3 = cosf(a0 + sk + .018f), s3 = sinf(a0 + sk + .018f), c4 = cosf(a0 + .018f), s4 = sinf(a0 + .018f);
      PUT(c0*t0, s0*t0, 1.5f); PUT(c4*t0, s4*t0, 1.5f); PUT(c3*t1, s3*t1, .2f);
      PUT(c0*t0, s0*t0, 1.5f); PUT(c3*t1, s3*t1, .2f); PUT(c2*t1, s2*t1, .2f);
    }
  }
#undef PUT
  t3_geometry *g = t3_geometry_new();
  t3_attribute *ap = t3_attribute_new(T3_FLOAT32, P, n, 3), *ac = t3_attribute_new(T3_FLOAT32, C, n, 3);
  t3_geometry_set_attribute(g, T3_ATTR_POSITION, ap); t3_release(ap);
  t3_geometry_set_attribute(g, T3_ATTR_COLOR, ac); t3_release(ac);
  free(P); free(C);
  if (!RING_M) {
    RING_M = t3_mesh_basic_material_new(0xffffff);
    RING_M->vertex_colors = true; RING_M->fog = false; RING_M->side = T3_DOUBLE_SIDE; RING_M->tone_mapped = false;
    RING_M->transparent = true; RING_M->blending = T3_ADDITIVE_BLENDING; RING_M->depth_write = false; RING_M->version++;
  }
  RING = t3_mesh_new(g, RING_M); t3_release(g);
  ((t3_object *)RING)->frustum_culled = false; ((t3_object *)RING)->render_order = 2;
  t3_object_add(&SC->base, RING);
  t3_release(RING);
}

/* ---- small helpers ---- */
static v3 flat(v3 v) { return v3_(v.x, 0, v.z); }
static float dist2d(v3 a, v3 b) { float dx = a.x - b.x, dz = a.z - b.z; return sqrtf(dx * dx + dz * dz); }
static v3 facing(const fighter_t *f) { return v3_(sinf(f->yaw), 0, cosf(f->yaw)); }
static v3 chest(const fighter_t *f) { return v3_(f->pos.x, f->pos.y + 1.05f, f->pos.z); }
static bool alive_foe(const fighter_t *f) { return f->st != F_KO && f->st != F_KNEEL; }
static int foes_up(void) { int n = 0; for (int i = 0; i < FG.nfoe; i++) if (alive_foe(&FG.foe[i])) n++; return n; }
static void set_st(fighter_t *f, f_state s) { f->st = s; f->t = 0; }
static void turn_to(fighter_t *f, v3 p, float rate, float dt) {
  float dx = p.x - f->pos.x, dz = p.z - f->pos.z;
  if (dx * dx + dz * dz < .0004f) return;
  f->yaw = ds_wrap_pi(f->yaw + ds_wrap_pi(atan2f(dx, dz) - f->yaw) * (1 - expf(-rate * dt)));
}
static void play(fighter_t *f, const char *clip, float rate, bool loop, float fade) { actor_play(f->a, clip, rate, loop, fade); f->clip = clip; }
static void replay(fighter_t *f, const char *clip, float rate, bool loop, float fade) {
  if (f->clip && !strcmp(f->clip, clip)) actor_replay(f->a, clip, rate, loop, 0);
  else actor_play(f->a, clip, rate, loop, fade);
  f->clip = clip;
}
/* one shot clips bake their first pose as the very last frame: moves leave before it, holds stop short of it */
static bool near_end(const fighter_t *f) { return actor_clip_time(f->a) >= actor_clip_len(f->a) - .09f; }
static void hold_end(fighter_t *f) { if (near_end(f)) { actor_clip_seek(f->a, actor_clip_len(f->a) - .08f); actor_play(f->a, f->clip, 0, false, 0); } }
static const char *idle_clip(const fighter_t *f) { return f->wp == WP_STAFF ? "Spell_Simple_Idle_Loop" : "Sword_Idle"; }
static void say_slow(float k, float t) { FG.slow_k = k; FG.slow_t = t; }
static void shake(float s) { FG.shake = fmaxf(FG.shake, s); }
static void sound(sfx_t s, v3 at, float gain) { sfx_at(s, at.x, at.y, at.z, gain); }
static void banner(const char *s, float t) {
  FG.banner = s; FG.banner_t = t;
#ifdef KH_TEST_FIGHT
  ds_log("banner %s at clock %.2f", s, FG.clock);
#endif
}

/* ---- v1.1: what the twists, barks, assists and the finisher hook into ---- */
static bool mod(unsigned m) { return (FG.def.mods & m) != 0; }
static void bark(int on);
static void style_add(float pts, int kind);
static void twist_hit(fighter_t *att, fighter_t *def, float dmg, int flags);
static void twist_player_hurt(float dmg);
static void tut_done(int step);
static bool fin_try(fighter_t *def);
static bool foe_hold_up(fighter_t *def);
static void obj_strike(fighter_t *f, const move_t *m);
static v3 foe_aim(const fighter_t *f);
static void ally_think(fighter_t *f, float dt);
static int ally_target(const fighter_t *f);
static void land_impact_at(fighter_t *f, v3 at, float radius, float dmg, int flags, uint32_t col);
static float twist_goal_reach(const fighter_t *f);
static float twist_goal_speed(const fighter_t *f);
static int twist_solids(v3 *p, float *r, int max);
static void boss_phase_enter(fighter_t *f, int phase);
static void fighter_setup(fighter_t *f, bool player);
static bool obj_bolt(v3 p);
static bool obj_in_reach(const fighter_t *P, v3 *at);
/* the world agent's arena change; a do nothing stand in until it links */
__attribute__((weak)) void world_arena_phase(int phase) { (void)phase; }
enum { SK_LIGHT = 1, SK_HEAVY = 2, SK_DASH = 4, SK_AIR = 8, SK_SPECIAL = 16, SK_BOLT = 32, SK_PARRY = 64, SK_DODGE = 128, SK_BREAK = 256, SK_SWITCH = 512, SK_ALLY = 1024 };

static int pick_target(const fighter_t *f, v3 pref) {
  if (FG.lock >= 0 && alive_foe(&FG.foe[FG.lock])) return FG.lock;
  int best = -1; float bs = 1e9f;
  for (int i = 0; i < FG.nfoe; i++) {
    const fighter_t *o = &FG.foe[i];
    if (!alive_foe(o)) continue;
    v3 d = flat(v3_sub(o->pos, f->pos));
    float l = v3_len(d), s = l - 1.6f * (l > .01f ? v3_dot(v3_scale(d, 1 / l), pref) : 0);
    if (s < bs) { bs = s; best = i; }
  }
  return best;
}

/* the stick in the world, from where the camera really stands */
static v3 stick_world(const intent_t *in) {
  v3 f = flat(v3_sub(FG.cam_look, FG.cam_pos));
  f = v3_len(f) > .01f ? v3_norm(f) : facing(&FG.P);
  v3 r = v3_(f.z, 0, -f.x);
  return v3_add(v3_scale(f, in->my), v3_scale(r, -in->mx));
}

/* ---- bolts ---- */
#define MAX_BOLTS 24
enum { B_BOLT, B_WAVE };
typedef struct { bool on; int kind, owner; v3 p, v; float dmg, ttl, r; uint32_t col; bool deflected; int homing; uint32_t mask; } bolt_t;
static bolt_t BOLT[MAX_BOLTS];
static void bolt_fire(int owner, int kind, v3 p, v3 v, float dmg, float r, uint32_t col, int homing) {
  for (int i = 0; i < MAX_BOLTS; i++) if (!BOLT[i].on) {
    BOLT[i] = (bolt_t){ true, kind, owner, p, v, dmg, kind == B_WAVE ? 1.6f : 1.8f, r, col, false, homing, 0 };
    return;
  }
}
/* owner: -1 the player, else foe index */
static fighter_t *by_owner(int o) { return o < 0 ? &FG.P : &FG.foe[o]; }

/* ---- combat resolution ---- */
static void release_token(fighter_t *f) { f->token = false; }
static void attack_end(fighter_t *f) { f->trail = false; weapon_trail(f->a, false); }
static void to_free(fighter_t *f, float fade) {
  attack_end(f); release_token(f);
  set_st(f, F_FREE); f->hold = false; f->armor = 0;
  f->chain_l = f->chain_h = 0;
  play(f, idle_clip(f), 1, true, fade);
}
static void stagger(fighter_t *f, float t, const char *clip) {
  attack_end(f); release_token(f);
  set_st(f, F_STAGGER); f->stun = t; f->hold = false; f->armor = 0;
  replay(f, clip, 1.05f, false, .06f);
  actor_expr_set(f->a, EX_SORROW, 1);
}
static void foe_ko(fighter_t *f);
static void player_ko(void);
/* hits a foe takes before it stops flinching, per ai_kind */
static const int FLINCH_LIMIT[5] = { 4, 2, 3, 3, 5 };
static void knock_down(fighter_t *f) {
  attack_end(f); release_token(f);
  set_st(f, F_KNOCK); f->hold = false; f->armor = 0;
  replay(f, "Hit_Knockback", 1.1f, false, .05f);
  f->inv = 1.1f; f->flinch = 0;
  sound(SFX_BODY_FALL, f->pos, .5f);
}
static void meter_add(float m) { if (mod(MOD_NO_SPECIAL)) { FG.meter = 0; return; } FG.meter = fminf(100, FG.meter + m); }

/* a blow lands. pt is where; dir pushes the victim. */
static void hurt(fighter_t *att, fighter_t *def, float dmg, float stun, float kb, int flags, v3 pt, v3 dir, float hitstop, uint32_t col) {
  bool heavy = (flags & MF_HEAVY) != 0;
  if (dir.x == 0 && dir.z == 0) dir = facing(att);
  if (def->player && mod(MOD_GLASS)) dmg *= 2;
  if (mod(MOD_SUDDEN) && dmg > 0) dmg = fmaxf(dmg, def->hp);   /* sudden death: one clean hit ends anyone */
  def->hp -= dmg;
  if (def->player) { FG.damage_taken += dmg; FG.combo = 0; meter_add(dmg * .35f); twist_player_hurt(dmg); }
  else {
    FG.combo++; FG.combo_t = 2.2f; FG.combo_pop = 1; if (FG.combo > FG.max_combo) FG.max_combo = FG.combo;
    meter_add(WP[att->wp].meter * (heavy ? 1.3f : 1) * (att->ally ? .5f : 1));
    if (att->player && mod(MOD_VAMPIRE)) FG.P.hp = fminf(FG.P.hpmax, FG.P.hp + dmg * .2f);
    twist_hit(att, def, dmg, flags);
    foe_hold_up(def);
  }
  def->flash = 1; def->vel.x = dir.x * kb; def->vel.z = dir.z * kb;
  fx_sparks(pt, v3_norm(v3_add(v3_scale(dir, -1), v3_(0, .4f, 0))), col, heavy ? 16 : 9);
  shake(heavy ? .5f : .22f);
  att->freeze = fmaxf(att->freeze, hitstop); def->freeze = fmaxf(def->freeze, hitstop * 1.15f);
  sound(heavy || att->wp == WP_MAUL ? SFX_HIT_HEAVY : SFX_HIT_LIGHT, pt, 1);
  actor_expr_set(def->a, EX_SORROW, 1);

  if (def->hp <= 0) {
    if (!def->player && fin_try(def)) return;
    def->hp = 0;
    if (def->player) { player_ko(); return; }
    foe_ko(def);
    return;
  }
  if (def->armor > 0 || def->st == F_ROAR || def->st == F_LEAP) return;   /* hit, but does not stop */
  if (flags & MF_KNOCK) { knock_down(def); return; }
  /* a foe once committed to a swing is not turned aside by a light cut: read the glint and get out, or hit it heavy */
  if (!def->player && def->st == F_ATTACK && !(flags & MF_HEAVY)) return;
  /* nobody is stun locked for ever: past a few flinches the next blows only hurt */
  def->flinch++; def->flinch_t = 1.3f;
  int limit = def->player ? 3 : FLINCH_LIMIT[def->d.ai] - (def->skill > .6f ? 1 : 0);
  if (def->flinch > limit) {
    /* a white ring: this one has had enough and comes back through the combo */
    def->armor = 1.1f; def->flinch = 0;
    fx_ring_up(chest(def), .2f, 1.4f, 0xffffff, .25f);
    if (!def->player) def->atk_cd = 0;
    if (def->st == F_HIT || def->st == F_FREE) { if (def->st == F_HIT) to_free(def, .08f); }
    return;
  }
  attack_end(def); release_token(def);
  set_st(def, F_HIT); def->stun = stun; def->hold = false;
  if (def->player) def->inv = fmaxf(def->inv, .22f);
  replay(def, ds_rand() < .5f ? "Hit_Chest" : "Hit_Head", 1, false, .04f);
}

/* returns true when it counted as a contact (the move should not hit this target again) */
static bool land_blow(fighter_t *att, fighter_t *def, float dmg, float stun, float kb, int flags, v3 pt, float hitstop) {
  v3 d = flat(v3_sub(def->pos, att->pos));
  d = v3_len(d) > .01f ? v3_norm(d) : facing(att);
  uint32_t col = weapon_color(att->wp);
  /* i frames: a clean dodge */
  if (def->inv > 0 || def->st == F_KO || def->st == F_KNEEL) {
    if (def->player && def->st == F_ROLL && def->t < .34f && FG.witch_t <= 0) {
      FG.witch_t = .9f; say_slow(.22f, .9f); meter_add(14); FG.perfect_dodges++; style_add(1000, SK_DODGE);
      banner("PERFECT DODGE", .9f); sfx(SFX_PERFECT, 0, 1);
      fx_ring_up(chest(def), .2f, 2.6f, 0x9fe0ff, .4f);
      fx_motes(chest(def), 0x9fe0ff, 16, 3);
    }
    return true;
  }
  /* guard: the blow must come from the front half */
  bool front = v3_dot(facing(def), v3_scale(d, -1)) > -.25f;
  if (def->st == F_GUARD && front && !(flags & MF_UNBLOCK)) {
    if (def->parry_t > 0) {
      /* parried: the attacker's guard is wide open */
      def->parry_t = 0; def->parry_cd = 0;
      stagger(att, 1.05f, "Idle_Shield_Break");
      att->inv = 0; att->vel = v3_scale(d, -3);
      fx_clash(pt, 0xffffff); fx_ring_up(pt, .1f, 1.6f, C_HIVIS, .3f);
      sound(SFX_PARRY, pt, 1); shake(.35f);
      att->freeze = .1f; def->freeze = .06f;
      if (def->player) { meter_add(24); banner("PARRY", .7f); say_slow(.3f, .4f); FG.parries++; bark(FB_PARRY); style_add(1500, SK_PARRY); tut_done(TUT_PARRY); tut_done(TUT_GUARD); }
      return true;
    }
    bool breaks = (flags & MF_BREAK) != 0;
    def->guard_hp -= dmg * (flags & MF_HEAVY ? 2.2f : 1.3f);
    def->vel = v3_scale(d, kb * .7f);
    if (breaks || def->guard_hp <= 0) {
      def->hp -= dmg * .35f;
      if (def->player) { FG.damage_taken += dmg * .35f; twist_player_hurt(dmg * .35f); }
      else { foe_hold_up(def); if (att->player) style_add(800, SK_BREAK); twist_hit(att, def, dmg * .35f, flags | MF_BREAK); }
      stagger(def, .95f, "Idle_Shield_Break");
      def->guard_hp = 30;
      fx_clash(pt, col); fx_ring_up(pt, .1f, 1.9f, 0xffffff, .3f);
      sound(SFX_GUARD_BREAK, pt, 1); shake(.5f);
      if (!def->player) banner("GUARD BREAK", .7f);
      att->freeze = .08f; def->freeze = .1f;
      if (def->hp <= 0) { if (!def->player && fin_try(def)) return true; def->hp = 0; if (def->player) player_ko(); else foe_ko(def); }
      return true;
    }
    float chip = dmg * .12f;
    def->hp = fmaxf(1, def->hp - chip);   /* a guard is never the blow that ends it */
    if (def->player) { FG.damage_taken += chip; tut_done(TUT_GUARD); tut_done(TUT_PARRY); }   /* a block clears the parry lesson too: parry is a bonus, not a gate */
    fx_clash(pt, col); sound(SFX_CLASH, pt, 1); shake(.15f);
    att->freeze = fmaxf(att->freeze, hitstop * .6f); def->freeze = fmaxf(def->freeze, hitstop * .6f);
    return true;
  }
  hurt(att, def, dmg, stun, kb, flags, pt, d, hitstop, col);
  return true;
}

/* where a blade meets a body: the nearest point on it to the chest */
static v3 contact(fighter_t *att, const fighter_t *def) {
  v3 b, t, c = chest(def);
  if (!weapon_points(att->a, 0, &b, &t)) return v3_lerp(chest(att), c, .7f);
  v3 s = v3_sub(t, b);
  float k = ds_clamp(v3_dot(v3_sub(c, b), s) / fmaxf(v3_dot(s, s), 1e-4f), 0, 1);
  v3 p = v3_add(b, v3_scale(s, k));
  /* keep the spark on the body side even when the mocap arc misses */
  return v3_lerp(p, v3_lerp(chest(att), c, .75f), .4f);
}

static float move_rate(const fighter_t *f) { return WP[f->wp].rate * (f->player ? 1 : ds_lerp(.9f, 1.f, ds_clamp(f->speed - .7f, 0, 1)) * f->speed); }
static float move_damage(const fighter_t *f, const move_t *m) { return m->dmg * WP[f->wp].dmg * f->power * (f->player ? 1 : f->ally ? fmaxf(1, f->charge) * .8f : FOE_DMG); }
static float move_reach(const fighter_t *f, const move_t *m) { return WP[f->wp].reach + (m->flags & MF_SWEEP ? .1f : 0) + (m == &MOVES[MV_DASH] ? .25f : 0); }

static void strike_try(fighter_t *f, const move_t *m, fighter_t *def, int bit) {
  if (f->hitmask & (1u << bit)) return;
  float reach = move_reach(f, m) + PLAYER_R;
  v3 d = flat(v3_sub(def->pos, f->pos));
  float l = v3_len(d);
  if (l > reach || fabsf(def->pos.y - f->pos.y) > 1.6f) return;
  if (l > .35f && v3_dot(v3_scale(d, 1 / l), facing(f)) < cosf(m->arc)) return;
  f->hitmask |= 1u << bit;
  const weapon_t *W = &WP[f->wp];
  int flags = m->flags;
  if (f->wp == WP_MAUL && (flags & MF_HEAVY)) flags |= MF_BREAK | MF_KNOCK;
  float stop = W->stop * ((flags & MF_HEAVY) ? 1.5f : 1);
  land_blow(f, def, move_damage(f, m), m->stun * W->stun, m->kb * W->kb, flags, contact(f, def), stop);
}

/* ---- starting things ---- */
static void attack_start(fighter_t *f, int mv, fighter_t *target, float tele) {
  const move_t *m = &MOVES[mv];
  bool cont = f->st == F_ATTACK && f->mv >= 0 && MOVES[f->mv].clip == m->clip && fabsf(MOVES[f->mv].to - m->from) < .02f;
  float rate = move_rate(f);
  if (cont) { play(f, m->clip, rate, false, 0); }
  else {
    replay(f, m->clip, rate, false, .07f);
    if (m->from > 0) actor_clip_seek(f->a, m->from);
  }
  set_st(f, F_ATTACK);
  f->mv = mv; f->hitmask = 0; f->whoosh = false; f->released = tele <= 0; f->hold = false; f->fired = 0;
  f->tele = tele; f->tele_max = tele; f->hold_at = fmaxf(m->from, m->hit0 - .13f);
  f->lunge_v = 0; f->lunge_t = 0;
  actor_expr_set(f->a, EX_ANGRY, 1); actor_expr_set(f->a, EX_SORROW, 0);
  /* a boss's big move reads from across the room: the glint, the held pose, a sound of its own */
  if (!f->player && !f->ally && f->d.ai == AI_BOSS && tele > .2f && (m->flags & (MF_HEAVY | MF_WAVE))) { f->tell = tele + .15f; sound(SFX_TELL, f->pos, 1); }
  if (target) {
    float dist = dist2d(f->pos, target->pos);
    float want = ds_clamp(dist - (move_reach(f, m) * .72f + .15f), 0, m->lunge);
    float dur = f->player ? fmaxf(.08f, (m->hit0 - m->from) / rate * .9f) : .13f;
    f->lunge_v = want / dur; f->lunge_t = dur;
    if (!f->player && tele > 0) f->lunge_t = -dur;   /* waits for the release */
  }
}
static void roll_start(fighter_t *f, v3 dir) {
  attack_end(f); release_token(f);
  if (v3_len(dir) < .1f) dir = v3_scale(facing(f), -1);
  dir = v3_norm(dir);
  f->yaw = atan2f(dir.x, dir.z);
  f->vel = v3_scale(dir, 7.2f);
  replay(f, "Roll", 1.7f, false, .05f);
  set_st(f, F_ROLL); f->inv = .38f; f->armor = 0;
  if (f->player) tut_done(TUT_DODGE);
  sound(SFX_DODGE, f->pos, .9f);
  fx_dust(f->pos, .6f, 3);
  /* rolling just as a blow arrives is the perfect dodge, even a little before it would land */
  if (f->player) for (int i = 0; i < FG.nfoe; i++) {
    fighter_t *o = &FG.foe[i];
    if (o->st != F_ATTACK || !o->released || FG.witch_t > 0) continue;
    const move_t *m = &MOVES[o->mv];
    float t = actor_clip_time(o->a);
    if (t < m->hit0 + .12f && t > m->hit0 - .3f && dist2d(o->pos, f->pos) < WP[o->wp].reach + 1.2f) {
      o->hitmask |= 1;
      FG.witch_t = .9f; say_slow(.22f, .9f); meter_add(14); FG.perfect_dodges++; style_add(1000, SK_DODGE);
      banner("PERFECT DODGE", .9f); sfx(SFX_PERFECT, 0, 1);
      fx_ring_up(chest(f), .2f, 2.6f, 0x9fe0ff, .4f); fx_motes(chest(f), 0x9fe0ff, 16, 3);
      break;
    }
  }
}
static void guard_start(fighter_t *f, bool fresh) {
  attack_end(f); release_token(f);
  replay(f, "Sword_Block", 1.7f, false, .05f);
  set_st(f, F_GUARD); f->hold = false;
  if (fresh && f->parry_cd <= 0) { f->parry_t = .2f; f->parry_cd = .55f; }
  f->vel.x = f->vel.z = 0;
}

/* ---- the special ---- */
static const float CINE_LEN = 2.f;
static void special_start(int target) {
  fighter_t *P = &FG.P;
  FG.meter = 0;
  attack_end(P); P->trail = false;
  set_st(P, F_SPECIAL);
  FG.cine_t = 0; FG.cine_target = target; FG.cine_hits = 0;
  FG.cine_name = WP[P->wp].special;
  FG.specials++; style_add(3000, SK_SPECIAL); tut_done(TUT_SPECIAL);
  P->inv = 2.3f; P->vel = v3_(0, 0, 0); P->freeze = 0;
  play(P, idle_clip(P), 1, true, .1f);
  sfx(SFX_SPECIAL_CHARGE, 0, 1);
  fx_burst(chest(P), weapon_color(P->wp), .55f);
  FG.screen_flash = .35f; FG.screen_col = weapon_color(P->wp);
  actor_expr_set(P->a, EX_ANGRY, 1);
  for (int i = 0; i < FG.nfoe; i++) { fighter_t *o = &FG.foe[i]; if (o->st == F_ATTACK) { attack_end(o); o->hold = false; } }
  for (int i = 0; i < MAX_BOLTS; i++) BOLT[i].on = false;
}
/* each weapon's special: how many blows, and the big one last */
static int spec_hits(int w) { return w == WP_TWIN ? 7 : w == WP_SPEAR ? 5 : 3; }
static void special_hit(int n, bool last) {
  fighter_t *P = &FG.P;
  int w = P->wp, N = spec_hits(w);
  float total = 46.f * (w == WP_MAUL ? 1.15f : w == WP_TWIN ? .95f : 1) * P->power * (FG.cine_fin ? 1.4f : 1);
  float dmg = last ? total * .6f : total * .4f / (N - 1);
  /* the reach of each weapon's special: the maul's slam and the naginata's turn take in the room */
  float wide = w == WP_MAUL ? 6.5f : w == WP_SPEAR ? 4.5f : 6.f;
  uint32_t c = weapon_color(w);
  for (int i = 0; i < FG.nfoe; i++) {
    fighter_t *o = &FG.foe[i];
    if (!alive_foe(o)) continue;
    bool main_ = i == FG.cine_target;
    if (!main_ && dist2d(o->pos, P->pos) > wide) continue;
    v3 d = flat(v3_sub(o->pos, P->pos)); d = v3_len(d) > .01f ? v3_norm(d) : facing(P);
    float save = o->inv; o->inv = 0; o->armor = last ? 0 : 1;
    if (o->st == F_GUARD) set_st(o, F_FREE);
    float k = main_ ? 1 : w == WP_MAUL || w == WP_SPEAR ? .7f : .4f;
    float hit = dmg * k;
    if (FG.cine_fin && main_ && !last) hit = 0;   /* the finisher's last blow is the one that ends it */
    hurt(P, o, hit, .7f, last ? 7 : 1.5f, MF_HEAVY | (last ? MF_KNOCK : 0), v3_lerp(chest(P), chest(o), .75f), d, last ? .14f : .05f, c);
    o->armor = 0; if (alive_foe(o) && !last) o->inv = save;
  }
  v3 at = FG.cine_target >= 0 ? chest(&FG.foe[FG.cine_target]) : chest(P);
  P->freeze = 0;
  fx_burst(at, c, last ? 1.05f : .5f);
  if (last) {
    fx_shock(v3_(P->pos.x, P->pos.y + .05f, P->pos.z), .3f, w == WP_MAUL ? 9 : 7, c, .5f);
    FG.screen_flash = .75f; FG.screen_col = 0xffffff; shake(1.1f); sfx(SFX_SPECIAL_RELEASE, 0, 1);
    if (FG.tw.kind && dist2d(FG.tw.obj, P->pos) < wide) obj_strike(P, NULL);
  }
  else { FG.screen_flash = .2f; FG.screen_col = c; shake(.5f); sfx(w == WP_TWIN ? SFX_HIT_LIGHT : SFX_HIT_HEAVY, 0, 1); }
}
static void spec_place(fighter_t *P, v3 to) {
  v3 step = v3_sub(to, P->pos); step.y = 0;
  bool g = true;
  world_move(&P->pos, &step, PLAYER_R, 1.6f, 1, &g);
}
static void cine_update(float rdt) {
  fighter_t *P = &FG.P;
  if (FG.cine_t < 0) return;
  float t0 = FG.cine_t; FG.cine_t += rdt; float t = FG.cine_t;
  fighter_t *T = FG.cine_target >= 0 ? &FG.foe[FG.cine_target] : NULL;
  int w = P->wp, N = spec_hits(w);
  uint32_t col = weapon_color(w);
  v3 d = T ? flat(v3_sub(T->pos, P->pos)) : facing(P);
  d = v3_len(d) > .01f ? v3_norm(d) : facing(P);
#define FIRE(n) do { while (FG.cine_hits < (n) && FG.cine_hits < N) { special_hit(FG.cine_hits, FG.cine_hits == N - 1); FG.cine_hits++; } } while (0)
#define AT(x) (t0 < (x) && t >= (x))
  switch (w) {
  case WP_ARC: {   /* FINAL WELD: a blink to the foe, three cuts, the last a cross of light */
    if (AT(.5f) && T) {
      v3 from = chest(P), to = v3_sub(T->pos, v3_scale(d, WP[w].reach * .6f + .25f));
      spec_place(P, to); actor_set_floor(P->a, P->pos.y);
      for (int i = 0; i < 4; i++) fx_streak(v3_add(from, v3_(ds_rr(-.3f, .3f), ds_rr(-.5f, .4f), ds_rr(-.3f, .3f))), chest(P), col, .1f, .3f);
      P->yaw = atan2f(d.x, d.z);
      replay(P, "Sword_Heavy_Combo", 1.65f, false, .05f); actor_clip_seek(P->a, .15f); P->trail = true; weapon_trail(P->a, true);
      sfx((sfx_t)WP[w].swing, 0, 1); FG.cine_hits = 0;
    }
    if (t >= .5f) {
      static const float ST[3] = { .42f, 1.2f, 1.82f };
      float ct = actor_clip_time(P->a);
      while (FG.cine_hits < 3 && ct >= ST[FG.cine_hits]) { special_hit(FG.cine_hits, FG.cine_hits == 2); FG.cine_hits++; }
      if (FG.cine_hits == 3 && T && t < 1.5f) {   /* the weld: an X burnt into the air */
        v3 c = chest(T), s = v3_(d.z, 0, -d.x);
        float k = ds_clamp((1.5f - t) * 2, 0, 1);
        fx_beam(v3_add(c, v3_add(v3_scale(s, .9f), v3_(0, .9f, 0))), v3_sub(c, v3_add(v3_scale(s, .9f), v3_(0, .9f, 0))), col, .22f * k);
        fx_beam(v3_add(c, v3_sub(v3_scale(s, .9f), v3_(0, .9f, 0))), v3_sub(c, v3_sub(v3_scale(s, .9f), v3_(0, .9f, 0))), col, .22f * k);
      }
    }
    break; }
  case WP_MAUL: {   /* COLD FORGE HAMMERFALL: up out of frame, down on them, the floor rings */
    static v3 from, to;
    if (AT(.5f)) {
      from = P->pos; to = T ? v3_sub(T->pos, v3_scale(d, 1.2f)) : v3_add(P->pos, v3_scale(d, 2));
      replay(P, "NinjaJump_Start", 1.4f, false, .05f); actor_clip_seek(P->a, .3f);
      sound(SFX_JUMP, P->pos, 1); fx_dust(P->pos, 1, 6); sfx(SFX_SWING_MAUL, 0, 1);
      FG.cine_hits = 0;
    }
    if (t >= .5f && t < 1.15f) {
      float k = (t - .5f) / .65f;
      v3 p = v3_lerp(from, to, k * k * (3 - 2 * k));
      spec_place(P, p);
      P->pos.y = ds_lerp(from.y, to.y, k) + 3.4f * sinf(fminf(k, 1) * DS_PI * .85f) * (k < .85f ? 1 : (1 - k) / .15f);
      P->vel = v3_(0, 0, 0); P->yaw = atan2f(d.x, d.z);
      if (AT(.82f)) { replay(P, "Sword_Attack", .5f, false, .05f); actor_clip_seek(P->a, .2f); P->trail = true; weapon_trail(P->a, true); }
      v3 b, tip; if (weapon_points(P->a, 0, &b, &tip)) fx_glow(tip, .8f + k, col);
    }
    if (AT(1.15f)) {
      P->pos.y = to.y; actor_set_floor(P->a, P->pos.y);
      play(P, "Sword_Attack", 1.2f, false, 0); actor_clip_seek(P->a, .36f);
      v3 g = v3_add(P->pos, v3_(0, .06f, 0));
      fx_shock(g, .2f, 8, col, .6f); fx_shock(g, .1f, 5, 0xffffff, .35f); fx_dust(P->pos, 2.2f, 14);
      for (int i = 0; i < 10; i++) { float a = i * .628f; fx_streak(g, v3_add(g, v3_(sinf(a) * 5, ds_rr(.2f, 1.6f), cosf(a) * 5)), col, .12f, .4f); }
      sound(SFX_SHOCK, P->pos, 1);
    }
    if (t >= 1.15f) FIRE(1);
    if (t >= 1.4f) FIRE(2);
    if (t >= 1.62f) FIRE(3);
    break; }
  case WP_TWIN: {   /* THOUSAND BURR CUT: everywhere around them at once */
    static const float BT[7] = { .55f, .7f, .85f, 1.f, 1.15f, 1.3f, 1.62f };
    if (AT(.5f)) FG.cine_hits = 0;
    if (T && FG.cine_hits < 7 && t >= BT[FG.cine_hits]) {
      int n = FG.cine_hits;
      v3 from = chest(P);
      float a = atan2f(-d.x, -d.z) + n * 2.4f;
      float r = n == 6 ? 1.3f : 1.25f;
      v3 to = v3_add(T->pos, v3_(sinf(a) * r, 0, cosf(a) * r));
      if (n == 6) to = v3_add(T->pos, v3_scale(d, 1.4f));   /* the last: clean through and out the far side */
      spec_place(P, to); actor_set_floor(P->a, P->pos.y);
      v3 dd = flat(v3_sub(T->pos, P->pos)); if (v3_len(dd) > .01f) P->yaw = atan2f(dd.x, dd.z);
      if (n == 6) P->yaw = atan2f(-d.x, -d.z) + DS_PI;
      fx_streak(from, chest(P), col, .14f, .35f); fx_streak(from, chest(P), 0xffffff, .05f, .25f);
      replay(P, n & 1 ? "Sword_Regular_B" : "Sword_Regular_A", 2.2f, false, .03f);
      P->trail = true; weapon_trail(P->a, true);
      sfx(SFX_SWING_TWIN, 0, 1);
      special_hit(n, n == 6); FG.cine_hits++;
      if (n == 6) {
        v3 c = chest(T);
        for (int k = 0; k < 6; k++) { float b = k * 1.047f; fx_streak(v3_add(c, v3_(sinf(b) * 1.2f, cosf(b) * 1.2f, 0)), v3_sub(c, v3_(sinf(b) * 1.2f, cosf(b) * 1.2f, 0)), col, .08f, .5f); }
      }
    }
    if (!T) FIRE(t >= 1.6f ? 7 : 0);
    break; }
  case WP_SPEAR: {   /* FIRST LAYER ADHESION: a turning sweep that draws them in, then the thrust */
    if (AT(.5f)) { replay(P, "Sword_Regular_Combo", 1.6f, false, .05f); actor_clip_seek(P->a, .98f); P->trail = true; weapon_trail(P->a, true); FG.cine_hits = 0; sfx(SFX_SWING_SPEAR, 0, 1); }
    if (t >= .5f && t < 1.45f) {
      P->yaw = ds_wrap_pi(P->yaw + rdt * 15.f);
      if (actor_clip_time(P->a) > 2.1f) { actor_clip_seek(P->a, 1.0f); sfx(SFX_SWING_SPEAR, 0, .8f); }
      /* a ring of light round the turn, and everyone close pulled to its edge */
      v3 g = v3_add(P->pos, v3_(0, .9f, 0));
      for (int k = 0; k < 3; k++) { float a = P->yaw + k * 2.094f; fx_streak(v3_add(g, v3_(sinf(a) * 2.6f, 0, cosf(a) * 2.6f)), v3_add(g, v3_(sinf(a + .5f) * 2.6f, 0, cosf(a + .5f) * 2.6f)), col, .12f, .25f); }
      for (int i = 0; i < FG.nfoe; i++) {
        fighter_t *o = &FG.foe[i];
        if (!alive_foe(o)) continue;
        float l = dist2d(o->pos, P->pos);
        if (l < 7 && l > 1.7f) { v3 pull = v3_scale(v3_norm(flat(v3_sub(P->pos, o->pos))), fminf(l - 1.7f, rdt * 6)); bool g2 = o->grounded; world_move(&o->pos, &pull, PLAYER_R, 1.6f, 1, &g2); }
      }
    }
    static const float HT[4] = { .65f, .85f, 1.05f, 1.25f };
    while (FG.cine_hits < 4 && t >= HT[FG.cine_hits]) { special_hit(FG.cine_hits, false); FG.cine_hits++; }
    if (AT(1.45f)) {
      P->yaw = atan2f(d.x, d.z);
      replay(P, "Sword_Dash", 1.6f, false, .04f); actor_clip_seek(P->a, .2f);
      v3 a = chest(P), e = v3_add(a, v3_scale(d, 9));
      fx_streak(a, e, 0xffffff, .2f, .35f); fx_streak(a, e, col, .5f, .45f);
      sfx(SFX_LANCE, 0, 1);
    }
    if (t >= 1.55f) FIRE(5);
    break; }
  default: {   /* the staff: a held beam, the crystal to the foe and past it */
    if (AT(.5f)) { replay(P, "Spell_Simple_Shoot", 1, false, .05f); P->yaw = atan2f(d.x, d.z); sfx((sfx_t)WP[w].swing, 0, 1); FG.cine_hits = 0; }
    if (t > .62f && t < 1.5f && T) {
      v3 b, tip;
      if (!weapon_points(P->a, 0, &b, &tip)) tip = chest(P);
      v3 dir = v3_norm(v3_sub(chest(T), tip)), end = v3_add(tip, v3_scale(dir, 22));
      float wd = .35f * sinf(ds_clamp((t - .62f) / .88f, 0, 1) * DS_PI) + .05f;
      fx_beam(tip, end, 0xffffff, wd * .6f); fx_beam(tip, end, col, wd * 1.8f); fx_glow(tip, 1.6f, col);
      fx_glow(chest(T), 2.2f, 0xffb0c8);
      if ((int)(t * 30) % 3 == 0) fx_motes(chest(T), col, 2, 5);
    }
    static const float BT[3] = { .7f, 1.0f, 1.35f };
    while (FG.cine_hits < 3 && t >= BT[FG.cine_hits]) { special_hit(FG.cine_hits, FG.cine_hits == 2); FG.cine_hits++; }
    break; }
  }
  if (t >= CINE_LEN) {
    FIRE(N);
    FG.cine_t = -1; FG.cine_fin = false;
    P->inv = .5f;
    float fy = world_floor_y(v3_(P->pos.x, P->pos.y + 1.5f, P->pos.z)); if (fy > -1e8f) P->pos.y = fy;
    if (FG.phase == PH_FIGHT) to_free(P, .2f);
    say_slow(.5f, .25f);
  }
#undef FIRE
#undef AT
}

/* ---- ko ---- */
static void end_check(void) {
  if (FG.phase != PH_FIGHT) return;
  if (foes_up() == 0) {
    FG.phase = PH_WON; FG.phase_t = 0;
    FG.screen_flash = .9f; FG.screen_col = 0xffffff;
    say_slow(.16f, 1.1f); shake(.9f);
    sfx(SFX_KO, 0, 1);
    music_mood(MOOD_CLEAR);
    if (FG.P.st != F_SPECIAL) { attack_end(&FG.P); }
    FG.lock = -1;
    bark(FB_FINISH);
  }
}
static void foe_ko(fighter_t *f) {
  attack_end(f); release_token(f);
  set_st(f, F_KO); f->hold = false; f->armor = 0;
  replay(f, "Hit_Knockback", 1, false, .05f);
  f->inv = 99;
  f->vel = v3_scale(f->vel, 1.3f);
  sound(SFX_BODY_FALL, f->pos, .9f);
  fx_burst(chest(f), 0xffffff, .4f);
  if (FG.lock >= 0 && &FG.foe[FG.lock] == f) {
    FG.lock = -1;
    for (int i = 0; i < FG.nfoe; i++) if (alive_foe(&FG.foe[i])) { FG.lock = i; break; }
  }
  if (FG.def.tutorial || foes_up() == 0) end_check();
  else say_slow(.3f, .3f);
  if (FG.def.tutorial && FG.phase != PH_WON) {
    FG.phase = PH_WON; FG.phase_t = 0; FG.screen_flash = .9f; FG.screen_col = 0xffffff; say_slow(.16f, 1.1f); sfx(SFX_KO, 0, 1);
    for (int i = 0; i < FG.nfoe; i++) if (alive_foe(&FG.foe[i])) { attack_end(&FG.foe[i]); FG.foe[i].hp = 0; foe_ko(&FG.foe[i]); }
  }
}
static void player_ko(void) {
  fighter_t *P = &FG.P;
  attack_end(P);
  if (FG.def.tutorial) {
    /* the tutorial never kills: the player stops at 1 hp and the scene carries on as won */
    P->hp = 1;
    set_st(P, F_HIT); P->stun = .9f;
    replay(P, "Hit_Knockback", 1.2f, false, .05f);
    P->inv = 99;
    if (FG.phase == PH_FIGHT) {
      FG.phase = PH_WON; FG.phase_t = 0; FG.screen_flash = .7f; FG.screen_col = 0xffffff; say_slow(.2f, .9f);
      for (int i = 0; i < FG.nfoe; i++) if (alive_foe(&FG.foe[i])) { attack_end(&FG.foe[i]); release_token(&FG.foe[i]); FG.foe[i].hold = false; set_st(&FG.foe[i], F_FREE); play(&FG.foe[i], idle_clip(&FG.foe[i]), 1, true, .2f); }
    }
    return;
  }
  set_st(P, F_KO);
  replay(P, "Hit_Knockback", 1, false, .05f);
  P->inv = 99;
  sound(SFX_BODY_FALL, P->pos, 1);
  if (FG.phase == PH_FIGHT) {
    FG.phase = PH_LOST; FG.phase_t = 0;
    FG.screen_flash = .7f; FG.screen_col = C_ALERT; say_slow(.2f, 1.2f); shake(.8f);
    sfx(SFX_KO, 0, 1);
    FG.lock = -1;
    for (int i = 0; i < FG.nfoe; i++) { fighter_t *o = &FG.foe[i]; if (alive_foe(o)) { attack_end(o); release_token(o); o->hold = false; set_st(o, F_FREE); play(o, idle_clip(o), 1, true, .3f); } }
  }
}
static void kneel(fighter_t *f) {
  set_st(f, F_KNEEL);
  weapon_equip(f->a, -1);
  fx_motes(chest(f), weapon_color(f->wp), 8, 1.5f);
  actor_play(f->a, "Fixing_Kneeling", 1, false, .45f);
  f->clip = "Fixing_Kneeling";
  actor_clip_seek(f->a, 1.5f);
  actor_play(f->a, "Fixing_Kneeling", 0, false, .45f);
  actor_hands_up(f->a, 1);
  actor_look(f->a, chest(&FG.P), 1);
  actor_expr_clear(f->a); actor_expr_set(f->a, EX_SORROW, 1);
}

/* ---- the player ---- */
static void lance_fire(fighter_t *P, float ch);
static void bolt_cast(fighter_t *f, fighter_t *target, int owner);
static void player_weapon(int w) {
  fighter_t *P = &FG.P;
  if (w == P->wp || w < 0 || w >= WP_COUNT || !(FG.def.weapons & (1u << w))) return;
  P->wp = w;
  style_add(300, SK_SWITCH);
  weapon_equip(P->a, w); weapon_draw_flash(P->a);
  sfx(SFX_WEAPON, 0, .8f);
  FG.weapon_card_t = 1.6f;
  P->chain_l = P->chain_h = 0;
  if (P->st == F_FREE) play(P, idle_clip(P), 1, true, .15f);
}
static bool player_action(fighter_t *P, const intent_t *in, char want, bool can_guard) {
  v3 stick = stick_world(in);
  float mag = v3_len(stick);
  if (in->special && FG.meter >= 100) {
    int t = pick_target(P, mag > .1f ? stick : facing(P));
    if (t >= 0) { P->buf = 0; special_start(t); return true; }
  }
  if (in->dodge) { P->buf = 0; roll_start(P, stick); return true; }
  if (can_guard && in->guard) { P->buf = 0; guard_start(P, in->guard_pressed); return true; }
  if (in->jump) {
    attack_end(P); P->buf = 0;
    set_st(P, F_AIR); P->vel.y = 7.4f; P->grounded = false;
    replay(P, "NinjaJump_Start", 1.5f, false, .06f); actor_clip_seek(P->a, .4f);
    sound(SFX_JUMP, P->pos, .9f); fx_dust(P->pos, .5f, 3);
    return true;
  }
  if (!want) return false;
  P->buf = 0;
  int t = pick_target(P, mag > .1f ? v3_norm(stick) : facing(P));
  fighter_t *T = t >= 0 ? &FG.foe[t] : NULL;
  /* face the stick when there is nothing in reach; the foe when there is */
  if (P->wp == WP_STAFF) {
    if (T) turn_to(P, T->pos, 40, 1);
    else if (mag > .1f) P->yaw = atan2f(stick.x, stick.z);
    if (want == 'L') { replay(P, "Spell_Simple_Shoot", 1.5f, false, .06f); set_st(P, F_CAST); P->fired = 0; P->armor = 0; return true; }
    replay(P, "Spell_Simple_Enter", 1.4f, false, .08f);
    set_st(P, F_CHARGE); P->charge = 0;
    return true;
  }
  const weapon_t *W = &WP[P->wp];
  int mv;
  bool dash = (P->run_t > .28f || P->dash_ok > 0) && want == 'L' && P->st != F_ATTACK && P->st != F_RECOVER;
  if (dash) mv = MV_DASH;
  else if (want == 'L') { mv = W->light[P->chain_l % W->nlight]; P->chain_l++; P->chain_h = 0; }
  else { mv = W->heavy[P->chain_h % W->nheavy]; P->chain_h++; P->chain_l = 0; }
  v3 op;
  if (obj_in_reach(P, &op) && (!T || dist2d(op, P->pos) < dist2d(T->pos, P->pos))) P->yaw = atan2f(op.x - P->pos.x, op.z - P->pos.z);
  else if (T && dist2d(T->pos, P->pos) < 7) { float dx = T->pos.x - P->pos.x, dz = T->pos.z - P->pos.z; P->yaw = ds_wrap_pi(P->yaw + ds_wrap_pi(atan2f(dx, dz) - P->yaw) * .6f); }
  else if (mag > .1f) P->yaw = atan2f(stick.x, stick.z);
  attack_start(P, mv, T, 0);
  P->dash_ok = 0; P->run_t = 0;
  return true;
}
static void player_think(fighter_t *P, const intent_t *in, float dt) {
  v3 stick = stick_world(in);
  float mag = fminf(1, v3_len(stick));
  /* the input buffer */
  if (in->light) { P->buf = 'L'; P->buf_t = .36f; }
  if (in->heavy) { P->buf = 'H'; P->buf_t = .36f; }
  if (P->buf_t > 0) { P->buf_t -= dt; if (P->buf_t <= 0) P->buf = 0; }
  if (in->weapon_pick >= 0) player_weapon(in->weapon_pick);
  /* the mouse wheel cycles weapons when nothing is locked on (locked on, it picks the target) */
  int step = in->weapon_step ? in->weapon_step : 0;
  if (step) {
    for (int k = 1; k <= WP_COUNT; k++) {
      int w = ((P->wp + step * k) % WP_COUNT + WP_COUNT) % WP_COUNT;
      if (FG.def.weapons & (1u << w)) { player_weapon(w); break; }
    }
  }
  if (P->dash_ok > 0) P->dash_ok -= dt;
  switch (P->st) {
    case F_FREE: {
      if (player_action(P, in, P->buf, true)) break;
      float want = 0;
      if (mag > .02f) {
        if (in->sprint) want = SPRINT_V;
        else if (!in->used_pad && mag > .98f) want = RUN_V;
        else want = mag > .95f ? (P->run_t > .5f ? SPRINT_V : RUN_V) : ds_lerp(1.2f, RUN_V, mag);
      }
      v3 fwd = facing(P);
      float cur = v3_len(flat(P->vel));
      float sp = cur < want ? fminf(want, cur + 22 * dt) : fmaxf(want, cur - 26 * dt);
      if (mag > .02f) {
        float tgt = atan2f(stick.x, stick.z);
        float d = ds_wrap_pi(tgt - P->yaw);
        P->yaw = ds_wrap_pi(P->yaw + d * (1 - expf(-14 * dt)));
        fwd = facing(P);
        P->run_t = sp > 3.2f ? P->run_t + dt : 0;
      } else {
        P->run_t = 0;
        int t = FG.lock >= 0 ? FG.lock : pick_target(P, facing(P));
        if (t >= 0 && sp < .6f) turn_to(P, FG.foe[t].pos, 8, dt);
      }
      P->vel.x = fwd.x * sp; P->vel.z = fwd.z * sp;
      const char *clip = sp < .15f ? idle_clip(P) : sp < 2.2f ? "Walk_Loop" : sp < 5.f ? "Jog_Fwd_Loop" : "Sprint_Loop";
      play(P, clip, 1, true, .14f);
      float g = actor_clip_ground_speed(P->a);
      if (sp >= .15f && g > .05f) play(P, clip, ds_clamp(sp / g, .4f, 2.6f), true, .14f);
      break; }
    case F_ATTACK: {
      const move_t *m = &MOVES[P->mv];
      float ct = actor_clip_time(P->a);
      if (ct >= m->cancel) {
        if (P->buf && player_action(P, in, P->buf, true)) break;
        if (player_action(P, in, 0, true)) break;
      }
      break; }
    case F_RECOVER: {
      if (P->buf && player_action(P, in, P->buf, true)) break;
      if (player_action(P, in, 0, true)) break;
      if (P->t > .12f && mag > .1f) { to_free(P, .12f); break; }
      if (P->t > P->stun) to_free(P, .18f);
      break; }
    case F_ROLL:
      if (P->t > .6f) {
        P->dash_ok = .3f;
        if (player_action(P, in, P->buf, true)) break;
        if (mag > .1f || P->t > .82f) { P->run_t = .3f; to_free(P, .14f); }
      }
      break;
    case F_GUARD:
      if (P->parry_t > 0) P->parry_t -= dt;
      if (in->guard_pressed && P->parry_cd <= 0) { P->parry_t = .2f; P->parry_cd = .55f; }
      {
        int t = FG.lock >= 0 ? FG.lock : pick_target(P, mag > .1f ? v3_norm(stick) : facing(P));
        if (t >= 0) turn_to(P, FG.foe[t].pos, 12, dt);
        if (actor_clip_time(P->a) > .32f) play(P, "Sword_Block", 0, false, 0);
        P->vel.x = 0; P->vel.z = 0;
      }
      if (in->dodge) { roll_start(P, stick); break; }
      if (!in->guard) { to_free(P, .1f); }
      break;
    case F_AIR:
      if (in->light || in->heavy) {
        set_st(P, F_PLUNGE);
        replay(P, "Sword_Attack", .1f, false, .06f); actor_clip_seek(P->a, .26f);
        P->vel.y = -3; P->trail = true; weapon_trail(P->a, true);
        break;
      }
      {
        float ax = 12 * dt;
        P->vel.x += stick.x * ax; P->vel.z += stick.z * ax;
        v3 h = flat(P->vel); float l = v3_len(h);
        if (l > 5.f) { P->vel.x *= 5.f / l; P->vel.z *= 5.f / l; }
        int t = pick_target(P, facing(P));
        if (t >= 0) turn_to(P, FG.foe[t].pos, 6, dt);
      }
      if (near_end(P)) play(P, "NinjaJump_Idle_Loop", 1, true, .15f);
      break;
    case F_PLUNGE:
      P->vel.y -= 34 * dt;
      {
        int t = pick_target(P, facing(P));
        if (t >= 0 && dist2d(FG.foe[t].pos, P->pos) > 1.2f) {
          v3 d = v3_norm(flat(v3_sub(FG.foe[t].pos, P->pos)));
          P->vel.x = d.x * 3.2f; P->vel.z = d.z * 3.2f;
          turn_to(P, FG.foe[t].pos, 12, dt);
        }
      }
      break;
    case F_LAND:
      if (P->t > .24f) { if (player_action(P, in, P->buf, true)) break; }
      if (P->t > .34f && mag > .1f) to_free(P, .14f);
      if (P->t > P->stun) to_free(P, .18f);
      break;
    case F_CAST: {
      int t = pick_target(P, facing(P));
      if (t >= 0) turn_to(P, FG.foe[t].pos, 12, dt);
      if (P->t > .09f && !P->fired) { P->fired = 1; bolt_cast(P, t >= 0 ? &FG.foe[t] : NULL, -1); }
      if (P->t > .2f && player_action(P, in, P->buf, true)) break;
      if (P->t > .24f && player_action(P, in, 0, true)) break;
      if (P->t > .42f) to_free(P, .14f);
      break; }
    case F_CHARGE: {
      int t = pick_target(P, facing(P));
      if (t >= 0) turn_to(P, FG.foe[t].pos, 10, dt);
      P->charge = fminf(1, P->charge + dt / 1.1f);
      if (near_end(P)) play(P, "Spell_Simple_Idle_Loop", 1, true, .1f);
      sfx(SFX_CHARGE, 0, P->charge);
      v3 b, tip;
      if (weapon_points(P->a, 0, &b, &tip)) {
        fx_glow(tip, .4f + P->charge * 1.1f, weapon_color(WP_STAFF));
        if (P->charge >= 1) fx_glint(tip, .9f, 0xffffff);
        if ((int)(P->t * 40) % 3 == 0) fx_motes(tip, weapon_color(WP_STAFF), 1, 1.6f);
      }
      if (in->dodge) { roll_start(P, stick); break; }
      if (!in->heavy_held) {
        if (P->charge < .25f) { replay(P, "Spell_Simple_Shoot", 1.5f, false, .06f); set_st(P, F_CAST); P->fired = 0; }
        else { lance_fire(P, P->charge); replay(P, "Spell_Simple_Shoot", 1.3f, false, .04f); set_st(P, F_CAST); P->fired = 1; P->t = .08f; }
      }
      break; }
    default: break;
  }
}

/* ---- the staff's shots ---- */
static v3 staff_tip(fighter_t *f) { v3 b, t; if (weapon_points(f->a, 0, &b, &t)) return t; return v3_add(chest(f), v3_scale(facing(f), .6f)); }
static void bolt_cast(fighter_t *f, fighter_t *target, int owner) {
  v3 tip = staff_tip(f);
  v3 aim = target ? chest(target) : v3_add(chest(f), v3_scale(facing(f), 10));
  v3 d = v3_norm(v3_sub(aim, tip));
  float sp = owner < 0 ? 19 : 10.5f;
  bolt_fire(owner, B_BOLT, tip, v3_scale(d, sp), WP[WP_STAFF].dmg * f->power * (owner < 0 ? 1 : 1.25f), .28f, weapon_color(WP_STAFF), owner < 0 ? 1 : 0);
  fx_flash(tip, .9f, weapon_color(WP_STAFF));
  sound(SFX_BOLT, tip, .85f);
}
static void lance_fire(fighter_t *P, float ch) {
  v3 tip = staff_tip(P);
  int t = pick_target(P, facing(P));
  v3 aim = t >= 0 ? chest(&FG.foe[t]) : v3_add(chest(P), v3_scale(facing(P), 10));
  v3 d = v3_norm(v3_sub(aim, tip)), end = v3_add(tip, v3_scale(d, 16));
  float w = .12f + ch * .3f;
  fx_streak(tip, end, 0xffffff, w * .7f, .3f); fx_streak(tip, end, weapon_color(WP_STAFF), w * 1.8f, .4f);
  fx_burst(tip, weapon_color(WP_STAFF), .4f + ch * .4f);
  sound(SFX_LANCE, tip, 1); shake(.2f + ch * .35f);
  P->vel = v3_scale(flat(d), -1.5f - ch * 2);
  for (int i = 0; i < FG.nfoe; i++) {
    fighter_t *o = &FG.foe[i];
    if (!alive_foe(o)) continue;
    v3 c = chest(o), r = v3_sub(c, tip);
    float k = v3_dot(r, d);
    if (k < 0 || k > 16) continue;
    if (v3_len(v3_sub(c, v3_add(tip, v3_scale(d, k)))) > .7f + ch * .4f) continue;
    land_blow(P, o, WP[WP_STAFF].dmg * P->power * (1.6f + ch * 4.4f), .6f, 2 + ch * 4, MF_HEAVY | (ch >= 1 ? MF_KNOCK | MF_BREAK : 0), c, .06f);
  }
}
static void bolts_update(float dt, bool draw) {
  for (int i = 0; i < MAX_BOLTS; i++) {
    bolt_t *b = &BOLT[i];
    if (!b->on) continue;
    if (draw) {
      v3 d = v3_norm(b->v);
      if (b->kind == B_BOLT) {
        v3 back = v3_sub(b->p, v3_scale(d, .55f));
        fx_beam(back, b->p, b->col, .11f); fx_beam(v3_sub(b->p, v3_scale(d, .28f)), b->p, 0xffffff, .05f);
        fx_glow(b->p, .55f, b->col);
      } else {
        /* a crescent running along the ground */
        v3 s = v3_(d.z, 0, -d.x);
        v3 a = v3_add(b->p, v3_add(v3_scale(s, 1.1f), v3_(0, 1.15f, 0))), c = v3_add(b->p, v3_add(v3_scale(s, -1.1f), v3_(0, 1.15f, 0))), m = v3_add(b->p, v3_add(v3_scale(d, .7f), v3_(0, .05f, 0)));
        fx_beam(a, m, 0xffffff, .13f); fx_beam(m, c, 0xffffff, .13f);
        fx_beam(a, m, b->col, .36f); fx_beam(m, c, b->col, .36f);
      }
    }
    if (dt <= 0) continue;
    b->ttl -= dt;
    if (b->ttl <= 0) { b->on = false; continue; }
    if (b->homing && FG.lock >= 0 && alive_foe(&FG.foe[FG.lock])) {
      v3 want = v3_norm(v3_sub(chest(&FG.foe[FG.lock]), b->p));
      float sp = v3_len(b->v);
      b->v = v3_scale(v3_norm(v3_lerp(v3_norm(b->v), want, 1 - expf(-5 * dt))), sp);
    }
    b->p = v3_add(b->p, v3_scale(b->v, dt));
    float fy = world_floor_y(v3_(b->p.x, b->p.y + .5f, b->p.z));
    if (b->kind == B_BOLT && b->p.y < fy + .02f) { fx_sparks(b->p, v3_(0, 1, 0), b->col, 5); b->on = false; continue; }
    if (b->kind == B_WAVE) { b->p.y = fy; if ((int)(b->ttl * 30) % 2 == 0) fx_dust(b->p, .5f, 1); }
    if (b->owner < 0 && b->kind == B_BOLT && FG.tw.kind && obj_bolt(b->p)) { fx_sparks(b->p, v3_(0, 1, 0), b->col, 6); b->on = false; continue; }
    /* whom it can hit */
    int n = b->owner < 0 ? FG.nfoe : 1;
    for (int k = 0; k < n; k++) {
      fighter_t *o = b->owner < 0 ? &FG.foe[k] : &FG.P;
      if (o->st == F_KO || o->st == F_KNEEL) continue;
      if (b->mask & (1u << k)) continue;
      float hr = b->kind == B_WAVE ? 1.0f : b->r + .3f;
      v3 c = chest(o);
      float dx = c.x - b->p.x, dz = c.z - b->p.z, dy = b->kind == B_WAVE ? 0 : c.y - b->p.y;
      if (dx * dx + dz * dz + dy * dy * .35f > hr * hr) continue;
      b->mask |= 1u << k;
      fighter_t *own = by_owner(b->owner);
      /* a parried bolt goes back */
      if (o->st == F_GUARD && o->parry_t > 0 && b->kind == B_BOLT && !b->deflected) {
        b->deflected = true; b->v = v3_scale(b->v, -1.2f); b->owner = -1; b->mask = 0; b->dmg *= 1.5f; b->col = C_HIVIS;
        fx_clash(b->p, 0xffffff); sound(SFX_PARRY, b->p, 1);
        o->parry_t = 0; o->parry_cd = 0; meter_add(20); banner("DEFLECT", .7f);
        break;
      }
      int w = own->wp;
      own->wp = b->kind == B_WAVE ? own->wp : WP_STAFF;
      /* the shooter is not frozen by a far off hit */
      float keep = own->freeze;
      land_blow(own, o, b->dmg, b->kind == B_WAVE ? .6f : .3f, b->kind == B_WAVE ? 3 : 1.6f, b->kind == B_WAVE ? MF_HEAVY : 0, b->p, 0.04f);
      own->freeze = keep;
      own->wp = w;
      if (b->kind == B_BOLT) { b->on = false; break; }
    }
  }
}

/* ---- the ai ---- */
static int tokens_out(void) { int n = 0; for (int i = 0; i < FG.nfoe; i++) if (FG.foe[i].token) n++; return n; }
static void ai_plan(fighter_t *f) {
  float s = f->skill;
  f->nplan = 0;
  #define PLAN(m) (f->plan[f->nplan++] = (m))
  float r = ds_rand();
  switch (f->d.ai) {
    case AI_DUELIST:
      if (r < .4f) PLAN(MV_C); else if (r < .75f) PLAN(MV_SA); else PLAN(MV_DASH);
      if (s > .3f && ds_rand() < s) PLAN(MV_L2);
      if (s > .6f && ds_rand() < s - .3f) PLAN(MV_L3);
      break;
    case AI_BRUISER:
      if (r < .55f) PLAN(MV_SA); else PLAN(MV_H1);
      if (s > .5f && ds_rand() < s) PLAN(MV_H4);
      break;
    case AI_SKIRMISH:
      PLAN(r < .5f ? MV_DASH : MV_C);
      PLAN(MV_L2);
      if (s > .3f) PLAN(MV_L4);
      break;
    case AI_BOSS:
      if (f->phase >= 2 && r < .28f) { f->plan[0] = -1; f->nplan = 1; break; }   /* the leap */
      if (f->phase >= 1 && r < .55f) { PLAN(MV_WAVE); if (ds_rand() < .5f) PLAN(MV_DASH); break; }
      if (ds_rand() < .5f) { PLAN(MV_C); PLAN(MV_L2); if (f->phase >= 1) PLAN(MV_L3); }
      else { PLAN(MV_SA); if (f->phase >= 1) PLAN(MV_H3); }
      break;
    case AI_CASTER: PLAN(MV_L1); break;
  }
  #undef PLAN
}
static float ai_tele(const fighter_t *f, const move_t *m) {
  float t = ds_lerp(.62f, .30f, f->skill) / fmaxf(f->speed, .5f);
  if (m->flags & MF_HEAVY) t *= 1.25f;
  if (f->d.ai == AI_BOSS && (m->flags & (MF_HEAVY | MF_WAVE))) t = fmaxf(t * 1.3f, .55f);   /* time to read the tell */
  if (FG.tw.kind == FT_TUTORIAL) t *= 1.6f;
  if (f->d.ai == AI_BRUISER) t *= 1.25f;
  return t;
}
static float ai_cooldown(const fighter_t *f) {
  static const float BASE[5] = { 1.9f, 2.8f, 1.7f, 2.3f, 1.5f };
  return BASE[f->d.ai] * ds_rr(.8f, 1.35f) * ds_lerp(1.2f, .68f, f->skill) / fmaxf(f->speed, .5f);
}
static float ai_range(const fighter_t *f) {
  switch (f->d.ai) { case AI_BRUISER: return 2.3f; case AI_SKIRMISH: return 3.9f; case AI_CASTER: return 7.5f; case AI_BOSS: return 2.9f; default: return 3.1f; }
}
/* the foe sees the player's swing coming */
static void ai_react(int who_attacked) {
  (void)who_attacked;
  for (int i = 0; i < FG.nfoe; i++) {
    fighter_t *f = &FG.foe[i];
    if (f->st != F_FREE || f->react_cd > 0) continue;
    if (dist2d(f->pos, FG.P.pos) > WP[FG.P.wp].reach + 2.2f) continue;
    f->react_cd = ds_rr(1.2f, 2.4f);
    float s = f->skill, r = ds_rand();
    float guard = 0, dodge = 0;
    switch (f->d.ai) {
      case AI_DUELIST: guard = .1f + s * .5f; dodge = s * .18f; break;
      case AI_BRUISER: guard = s * .3f; break;
      case AI_SKIRMISH: dodge = .08f + s * .5f; break;
      case AI_CASTER: dodge = .1f + s * .45f; break;
      case AI_BOSS: guard = .25f + s * .35f; dodge = .1f + s * .2f; break;
    }
    if (r < guard) {
      guard_start(f, f->d.ai == AI_BOSS || s > .7f);
      f->stun = ds_rr(.5f, 1.f);
    } else if (r < guard + dodge) {
      v3 away = flat(v3_sub(f->pos, FG.P.pos));
      v3 side = v3_(away.z, 0, -away.x);
      roll_start(f, v3_add(v3_norm(away), v3_scale(v3_norm(side), ds_rand() < .5f ? -.7f : .7f)));
    }
  }
}
static void ai_think(int idx, fighter_t *f, float dt) {
  fighter_t *P = &FG.P;
  f->atk_cd -= dt; if (f->react_cd > 0) f->react_cd -= dt;
  if (f->aggro > 0) f->aggro -= dt;
  if (FG.phase != PH_FIGHT) { f->vel.x *= .9f; f->vel.z *= .9f; return; }
  switch (f->st) {
    case F_FREE: {
      bool on_obj = f->goal && f->aggro <= 0;
      v3 aim = foe_aim(f);
      v3 to = flat(v3_sub(aim, f->pos));
      float dist = v3_len(to);
      v3 dir = dist > .01f ? v3_scale(to, 1 / dist) : facing(f);
      float pref = ai_range(f);
      if (on_obj && f->token) release_token(f);
      /* ask for a turn: at most two swing at once */
      if (!on_obj && !f->token && f->atk_cd <= 0 && tokens_out() < 2 && P->st != F_KO) {
        f->token = true; ai_plan(f); f->ai_t = 0;
      }
      v3 want = v3_(0, 0, 0); float sp = 0;
      if (on_obj) {
        /* the objective: walk to it and go at it (or, carrying the bike, run for the edge) */
        float reach = twist_goal_reach(f);
        if (reach > 0 && dist <= reach && f->atk_cd <= 0) {
          if (f->wp == WP_STAFF) { if (weapon_of(f->a) != WP_STAFF) weapon_equip(f->a, WP_STAFF); }
          attack_start(f, f->d.ai == AI_BRUISER ? MV_SA : MV_L1, NULL, ai_tele(f, &MOVES[MV_L1]) * .9f);
          f->atk_cd = ai_cooldown(f) * .75f;
          sound(SFX_GLINT, f->pos, .6f);
          break;
        }
        if (dist > fmaxf(reach, .6f) * .8f) { want = dir; sp = twist_goal_speed(f) * f->speed; }
        else turn_to(f, aim, 8, dt);
      } else if (f->token) {
        f->ai_t += dt;
        if (f->d.ai == AI_CASTER) {
          /* stand off and shoot */
          if (f->wp != WP_STAFF) { f->wp = WP_STAFF; weapon_equip(f->a, WP_STAFF); }
          replay(f, "Spell_Simple_Enter", 1, false, .1f);
          set_st(f, F_CHARGE); f->fired = 0; f->stun = ai_tele(f, &MOVES[MV_L1]) * 1.1f; f->charge = 0;
          f->nplan = 1 + (int)(f->skill * 2.4f);
          f->hold = f->skill > .45f && ds_rand() < .35f;   /* a lance */
          if (f->hold) { f->stun *= 1.9f; f->nplan = 1; f->leap_to = P->pos; }
          sound(SFX_GLINT, f->pos, .8f);
          break;
        }
        if (f->plan[0] < 0) {   /* the boss's leap */
          f->leap_to = P->pos; set_st(f, F_LEAP); f->armor = 9;
#ifdef KH_TEST_FIGHT
          ds_log("leap at clock %.2f", FG.clock);
#endif
          replay(f, "NinjaJump_Start", 1.3f, false, .08f); actor_clip_seek(f->a, .25f);
          f->vel = v3_(0, 0, 0); f->fired = 0;
          sound(SFX_ROAR, f->pos, .7f); sound(SFX_TELL, f->pos, 1); f->tell = .9f;
          break;
        }
        const move_t *m = &MOVES[f->plan[0]];
        float trigger = m->flags & MF_WAVE ? 9.f : move_reach(f, m) + m->lunge * .9f;
        if (dist <= trigger || f->ai_t > 3.5f) {
          int mv = f->plan[0];
          for (int i = 0; i + 1 < f->nplan; i++) f->plan[i] = f->plan[i + 1];
          f->nplan--;
          attack_start(f, mv, P, ai_tele(f, m));
          if (f->d.ai == AI_BRUISER) f->armor = 9;
          sound(SFX_GLINT, f->pos, .8f);
          break;
        }
        want = dir; sp = (f->d.ai == AI_BRUISER ? 2.6f : 3.9f) * f->speed;
      } else {
        /* hold at range and circle, a little apart from the others */
        float err = dist - pref;
        v3 side = v3_(dir.z, 0, -dir.x);
        f->ai_t += dt;
        if (f->ai_t > f->circ) { f->ai_t = 0; f->circ = ds_rr(1.1f, 2.8f); f->ai_mode = ds_rand() < .5f ? -1 : 1; }
        want = v3_add(v3_scale(dir, ds_clamp(err * 1.2f, -1, 1)), v3_scale(side, (float)f->ai_mode * .75f));
        for (int k = 0; k < FG.nfoe; k++) if (k != idx && alive_foe(&FG.foe[k])) {
          v3 a = flat(v3_sub(f->pos, FG.foe[k].pos)); float l = v3_len(a);
          if (l < 2.2f && l > .01f) want = v3_add(want, v3_scale(a, (2.2f - l) / l * .8f));
        }
        float l = v3_len(want);
        if (l > .01f) want = v3_scale(want, 1 / fmaxf(l, 1));
        sp = fabsf(err) > 2.f ? 3.4f * f->speed : 1.35f * f->speed;
      }
      /* a walk that looks at the player: facing the travel, turned a good way to them */
      float wl = v3_len(want);
      float speed_now = wl * sp;
      v3 h = v3_scale(want, sp);
      f->vel.x = ds_damp(f->vel.x, h.x, 9, dt); f->vel.z = ds_damp(f->vel.z, h.z, 9, dt);
      float real = v3_len(flat(f->vel));
      const char *clip = real < .2f ? idle_clip(f) : real < 2.2f ? "Walk_Loop" : "Jog_Fwd_Loop";
      if (FG.tw.kind == FT_RETRIEVE && FG.tw.carrier == idx) clip = real < .2f ? "Idle_Loop" : "Walk_Carry_Loop";
      play(f, clip, 1, true, .2f);
      float g = actor_clip_ground_speed(f->a);
      if (real >= .2f && g > .05f) play(f, clip, ds_clamp(real / g, .4f, 2.4f), true, .2f);
      if (real > 1.0f) {
        float tv = atan2f(f->vel.x, f->vel.z), tp = atan2f(dir.x, dir.z);
        float mix = speed_now < 2.2f ? .55f : .15f;
        float t = tv + ds_wrap_pi(tp - tv) * mix;
        f->yaw = ds_wrap_pi(f->yaw + ds_wrap_pi(t - f->yaw) * (1 - expf(-9 * dt)));
      } else turn_to(f, P->pos, 7, dt);
      break; }
    case F_GUARD:
      turn_to(f, P->pos, 10, dt);
      if (f->parry_t > 0) f->parry_t -= dt;
      if (actor_clip_time(f->a) > .32f) play(f, "Sword_Block", 0, false, 0);
      f->stun -= dt;
      if (f->stun <= 0) { to_free(f, .12f); f->atk_cd = fminf(f->atk_cd, .25f); }
      break;
    case F_ROLL:
      if (f->t > .7f) { to_free(f, .14f); f->atk_cd = fminf(f->atk_cd, .4f); }
      break;
    case F_CHARGE: {   /* the caster's windup */
      turn_to(f, P->pos, 8, dt);
      f->charge += dt;
      v3 tip = staff_tip(f);
      float k = ds_clamp(f->charge / fmaxf(f->stun, .05f), 0, 1);
      fx_glow(tip, .3f + k * .9f, weapon_color(f->wp));
      if (k > .4f) fx_glint(tip, .3f + k * .7f, 0xffffff);
      if (f->hold) {   /* the lance's line on the ground */
        v3 d = v3_norm(flat(v3_sub(f->leap_to, f->pos)));
        v3 a = v3_add(f->pos, v3_(0, .06f, 0)), b = v3_add(a, v3_scale(d, 14));
        fx_beam(a, b, C_ALERT, .1f + k * .35f);
        f->yaw = atan2f(d.x, d.z);
      }
      if (f->charge >= f->stun) {
        if (f->hold) {
          v3 t0 = staff_tip(f), d = v3_norm(flat(v3_sub(f->leap_to, f->pos))), end = v3_add(t0, v3_scale(d, 16));
          fx_streak(t0, end, 0xffffff, .3f, .3f); fx_streak(t0, end, weapon_color(f->wp), .6f, .4f);
          sound(SFX_LANCE, t0, 1); shake(.35f);
          v3 r = flat(v3_sub(P->pos, f->pos));
          float along = v3_dot(r, d), off = v3_len(v3_sub(r, v3_scale(d, along)));
          if (along > 0 && along < 16 && off < .85f) land_blow(f, P, WP[WP_STAFF].dmg * f->power * 3.4f, .6f, 4, MF_HEAVY, chest(P), .06f);
          f->hold = false; f->nplan = 0; f->charge = 0; f->stun = 0;
        } else if (f->nplan > 0) {
          bolt_cast(f, P, idx);
          f->nplan--;
          replay(f, "Spell_Simple_Shoot", 1.5f, false, .05f);
          f->charge = 0; f->stun = .28f;
        }
        if (f->nplan <= 0 && !f->hold) { f->atk_cd = ai_cooldown(f); to_free(f, .2f); }
      }
      break; }
    case F_LEAP: {
      float T = f->t;
      /* up, hang, down on the marked spot */
      float fy = f->leap_to.y;
      if (T < .55f) {
        if (T > .18f) { f->vel.y = ds_lerp(11, 2, (T - .18f) / .37f); }
        else f->vel.y = 0;
        f->vel.x = f->vel.z = 0;
      } else if (!f->fired) {
        f->fired = 1;
        replay(f, "Sword_Attack", .12f, false, .06f); actor_clip_seek(f->a, .26f);
        f->trail = true; weapon_trail(f->a, true);
      } else if (T > .95f) {
        v3 d = flat(v3_sub(f->leap_to, f->pos));
        float rem = fmaxf(1.15f - T, .06f);
        f->vel.x = d.x / rem; f->vel.z = d.z / rem; f->vel.y = -22;
      } else f->vel = v3_(f->vel.x * .8f, .4f, f->vel.z * .8f);
      turn_to(f, f->leap_to, 8, dt);
      /* the mark */
      float k = ds_clamp(T / 1.1f, 0, 1);
      fx_ring_flat(v3_(f->leap_to.x, fy + .05f, f->leap_to.z), 3.2f, C_ALERT);
      fx_ring_flat(v3_(f->leap_to.x, fy + .05f, f->leap_to.z), 3.2f * k, 0xffb040);
      break; }
    case F_ROAR:
      f->vel.x = f->vel.z = 0;
      if (f->t > 1.1f) { to_free(f, .2f); f->atk_cd = .4f; f->armor = 0; f->inv = 0; }
      break;
    default: break;
  }
}

/* ---- a fighter's clock ---- */
static void attack_tick(fighter_t *f, int idx, float dt) {
  const move_t *m = &MOVES[f->mv];
  float ct = actor_clip_time(f->a), rate = move_rate(f);
  /* the telegraph: hold just before the swing, the blade glints */
  if (f->tele > 0 && ct >= f->hold_at) {
    if (!f->hold) { f->hold = true; play(f, m->clip, .04f, false, 0); }
    f->tele -= dt;
    v3 b, t;
    if (weapon_points(f->a, 0, &b, &t)) {
      float k = 1 - ds_clamp(f->tele / fmaxf(f->tele_max, .01f), 0, 1);
      fx_glint(t, .35f + k * .75f, (m->flags & MF_BREAK) || f->d.ai == AI_BRUISER ? 0xffc080 : 0xffffff);
    }
    f->flash = fmaxf(f->flash, .22f);
    turn_to(f, foe_aim(f), 7 * f->speed, dt);
    if (f->tele <= 0) {
      f->hold = false; f->released = true;
      play(f, m->clip, rate * 1.15f, false, 0);
      if (f->lunge_t < 0) f->lunge_t = -f->lunge_t;
      /* the gap may have changed: close what is left */
      float dist = dist2d(f->pos, foe_aim(f));
      f->lunge_v = ds_clamp(dist - (move_reach(f, m) * .72f + .15f), 0, m->lunge + .6f) / fmaxf(f->lunge_t, .1f);
    }
    return;
  }
  /* facing: the player keeps turning till the blow; foes commit at release */
  v3 op_;
  if (f->player && obj_in_reach(f, &op_) && fabsf(ds_wrap_pi(atan2f(op_.x - f->pos.x, op_.z - f->pos.z) - f->yaw)) < .5f) { if (ct < m->hit0) turn_to(f, op_, 13, dt); }
  else if (f->player) {
    int t = pick_target(f, facing(f));
    if (t >= 0 && ct < m->hit0) turn_to(f, FG.foe[t].pos, 13, dt);
  } else if (f->ally) {
    int t = ally_target(f);
    if (t >= 0 && ct < m->hit0) turn_to(f, FG.foe[t].pos, 13, dt);
  } else if (!f->released) turn_to(f, foe_aim(f), 9 * f->speed, dt);
  /* the spinning sweep turns a whole way round through its blow */
  if ((m->flags & MF_SPIN) && ct >= m->hit0 - .06f && ct <= m->hit1 + .08f) f->yaw = ds_wrap_pi(f->yaw + dt * rate * 6.28318f / (m->hit1 - m->hit0 + .14f));
  /* the flurry hits again and again */
  if ((m->flags & MF_MULTI) && ct >= m->hit0 && ct <= m->hit1) { f->rehit -= dt; if (f->rehit <= 0) { f->rehit = .075f; f->hitmask = 0; if (ct > m->hit0 + .05f) sound((sfx_t)WP[f->wp].swing, f->pos, .5f); } }

  /* the step into the blow */
  if (f->lunge_t > 0) {
    f->lunge_t -= dt;
    v3 fw = facing(f);
    f->vel.x = fw.x * f->lunge_v; f->vel.z = fw.z * f->lunge_v;
    if (f->lunge_t <= 0) { f->vel.x *= .25f; f->vel.z *= .25f; }
  }
  /* trail and whoosh */
  if (!f->trail && ct >= m->hit0 - .12f && ct <= m->hit1) { f->trail = true; weapon_trail(f->a, true); }
  if (f->trail && ct > m->hit1 + .1f) { f->trail = false; weapon_trail(f->a, false); }
  if (!f->whoosh && ct >= m->hit0 - .09f) { f->whoosh = true; sound((sfx_t)WP[f->wp].swing, f->pos, .9f); if (f->player) ai_react(idx); }
  /* the wave: a crescent let go at the swing */
  if ((m->flags & MF_WAVE) && !f->fired && ct >= m->hit0) {
    f->fired = 1;
    v3 d = facing(f);
#ifdef KH_TEST_FIGHT
    ds_log("wave at clock %.2f", FG.clock);
#endif
    bolt_fire(idx, B_WAVE, v3_add(f->pos, v3_scale(d, 1)), v3_scale(d, 9.5f), move_damage(f, m), 1, weapon_color(f->wp), 0);
    sound(SFX_LANCE, f->pos, .7f); shake(.2f);
  }
  /* the slam: the head meets the floor in front */
  if ((m->flags & MF_SLAM) && !f->fired && ct >= m->hit0) {
    f->fired = 1;
    v3 at = v3_add(f->pos, v3_scale(facing(f), 1.3f));
    land_impact_at(f, at, 2.7f, move_damage(f, m) * .7f, MF_HEAVY | MF_BREAK | MF_KNOCK, weapon_color(f->wp));
  }
  /* the blow */
  if (ct >= m->hit0 && ct <= m->hit1 && !(m->flags & MF_WAVE)) {
    if (f->player || f->ally) { for (int i = 0; i < FG.nfoe; i++) if (alive_foe(&FG.foe[i])) strike_try(f, m, &FG.foe[i], i); if (f->player) obj_strike(f, m); }
    else if (FG.phase == PH_FIGHT) {
      if (f->goal && f->aggro <= 0) obj_strike(f, m);
      else strike_try(f, m, &FG.P, 0);
    }
  }
  if (f->st != F_ATTACK) return;
  /* a foe's next move in its plan */
  if (!f->player && !f->ally && ct >= m->cancel && f->nplan > 0) {
    int mv = f->plan[0];
    for (int i = 0; i + 1 < f->nplan; i++) f->plan[i] = f->plan[i + 1];
    f->nplan--;
    attack_start(f, mv, &FG.P, ai_tele(f, &MOVES[mv]) * .45f);
    return;
  }
  if (ct >= m->to - .01f || near_end(f)) {
    attack_end(f);
    if (f->ally) { to_free(f, .15f); return; }
    if (!f->player) {
      f->atk_cd = ai_cooldown(f);
      release_token(f);
      /* a skirmisher slips away after its pass */
      if (f->d.ai == AI_SKIRMISH && f->skill > .3f && ds_rand() < .7f) { v3 a = flat(v3_sub(f->pos, FG.P.pos)); roll_start(f, a); return; }
      if (m->rec) { actor_play(f->a, m->rec, 1, false, .1f); f->clip = m->rec; set_st(f, F_RECOVER); f->stun = .5f; f->armor = 0; }
      else to_free(f, .2f);
      return;
    }
    if (m->rec) { actor_play(f->a, m->rec, 1.1f, false, .08f); f->clip = m->rec; set_st(f, F_RECOVER); f->stun = .55f; return; }
    to_free(f, .2f);
  }
}

static void land_impact(fighter_t *f, float radius, float dmg, int flags, uint32_t col) { land_impact_at(f, f->pos, radius, dmg, flags, col); }
static void land_impact_at(fighter_t *f, v3 at, float radius, float dmg, int flags, uint32_t col) {
  v3 g = v3_(at.x, at.y + .05f, at.z);
  fx_shock(g, .2f, radius * 1.15f, col, .4f);
  fx_shock(g, .1f, radius * .7f, 0xffffff, .25f);
  fx_dust(at, 1.1f, 7); fx_burst(v3_add(g, v3_(0, .2f, 0)), col, .8f);
  shake(.7f);
  sound(SFX_SHOCK, at, 1);
  if (f->player || f->ally) {
    for (int i = 0; i < FG.nfoe; i++) {
      fighter_t *o = &FG.foe[i];
      if (alive_foe(o) && dist2d(o->pos, at) < radius) land_blow(f, o, dmg, .7f, 4.5f, flags, chest(o), .1f);
    }
    if (f->player && dist2d(FG.tw.obj, at) < radius + .8f) obj_strike(f, NULL);
  } else if (FG.P.pos.y - at.y < .9f && dist2d(FG.P.pos, at) < radius && FG.phase == PH_FIGHT) land_blow(f, &FG.P, dmg, .7f, 5, flags, chest(&FG.P), .1f);
}

static void tick(fighter_t *f, int idx, float dt, const intent_t *in) {
  f->t += dt;
  if (f->tell > 0) {
    f->tell -= dt;
    v3 h = v3_add(f->pos, v3_(0, 1.75f, 0));
    float k = .5f + .5f * sinf(FG.clock * 30);
    fx_glint(h, .9f + k * .5f, 0xff5030); fx_glow(h, 1.2f, 0xff3010);
  }
  if (f->inv > 0) f->inv -= dt;
  if (f->parry_cd > 0) f->parry_cd -= dt;
  if (f->armor > 0 && f->armor < 5) f->armor -= dt;
  if (f->flinch_t > 0) { f->flinch_t -= dt; if (f->flinch_t <= 0) f->flinch = 0; }
  f->guard_hp = fminf(f->player ? 34 : 24 + f->skill * 26, f->guard_hp + 7 * dt);

  if (f->haz_inv > 0) f->haz_inv -= dt;
  v3 v0 = f->vel;
  if (f->player) {
    if (FG.phase == PH_FIGHT && FG.cine_t < 0 && in) player_think(f, in, dt);
  } else if (f->ally) ally_think(f, dt);
  else ai_think(idx, f, dt);
  /* frost: what you were doing carries on; the feet barely grip */
  if (f->ice > 0 && (f->st == F_FREE || f->st == F_ROLL || f->st == F_ATTACK) && f->grounded) {
    float k = expf(-1.6f * dt) * f->ice;
    f->vel.x = ds_lerp(f->vel.x, v0.x, k); f->vel.z = ds_lerp(f->vel.z, v0.z, k);
  }
  /* the fight is over: the player's hands finish what they were doing, unprompted */
  if (f->player && FG.phase != PH_FIGHT && FG.phase != PH_INTRO && (f->st == F_RECOVER || f->st == F_LAND || f->st == F_CAST || f->st == F_ROLL || f->st == F_GUARD || f->st == F_CHARGE) && f->t > .5f) to_free(f, .3f);

  switch (f->st) {
    case F_ATTACK: attack_tick(f, idx, dt); break;
    case F_STAGGER: case F_HIT:
      hold_end(f);
      if (f->t >= f->stun) { if (FG.phase == PH_WON && f->player) { set_st(f, F_DOWNED); break; } to_free(f, .15f); if (!f->player) { f->atk_cd = fmaxf(f->atk_cd, .35f); } }
      break;
    case F_RECOVER:
      if (!f->player && f->t >= f->stun) to_free(f, .2f);
      break;
    case F_KNOCK:
      hold_end(f);
      if (near_end(f) && f->t > .75f) {
        if (f->player || f->t > 1.05f) { replay(f, "LayToIdle", 1.5f, false, .12f); set_st(f, F_GETUP); f->inv = 1.05f; }
      }
      break;
    case F_GETUP:
      if (near_end(f) || f->t > .95f) { to_free(f, .15f); if (!f->player) f->atk_cd = fmaxf(f->atk_cd, .3f); }
      break;
    case F_KO:
      hold_end(f);
      if (FG.def.battle_zone && !f->player) {
        /* no story after: they stay down, then go in a flash */
        f->down_t += dt;
        if (f->down_t > 1.8f && actor_is_visible(f->a)) { fx_burst(chest(f), weapon_color(f->wp), .6f); weapon_equip(f->a, -1); actor_visible(f->a, false); }
        break;
      }
      if (!f->player && near_end(f) && f->t > .75f) kneel(f);
      break;
    default: break;
  }
  /* rolls glide and drag */
  float drag = f->st == F_ROLL ? 2.6f * (1 - f->ice * .7f) : f->st == F_FREE ? 0 : f->st == F_ATTACK ? 8 : f->st == F_AIR || f->st == F_PLUNGE || f->st == F_LEAP ? 0 : f->st == F_KO || f->st == F_KNOCK ? 3.2f : 5.5f;
  if (drag > 0) { float k = expf(-drag * dt); f->vel.x *= k; f->vel.z *= k; }

  /* gravity and the ground */
  bool was = f->grounded;
  f->vel.y -= GRAVITY * dt;
  world_move(&f->pos, &f->vel, PLAYER_R, 1.6f, dt, &f->grounded);
  if (f->grounded) {
    actor_set_floor(f->a, f->pos.y);
    if (!was) {
      if (f->st == F_AIR) {
        replay(f, "NinjaJump_Land", 1.8f, false, .05f); actor_clip_seek(f->a, .3f);
        set_st(f, F_LAND); f->stun = .5f;
        sound(SFX_LAND, f->pos, .7f); fx_dust(f->pos, .6f, 3);
      } else if (f->st == F_PLUNGE) {
        attack_end(f);
        play(f, "Sword_Attack", 1.5f, false, 0); actor_clip_seek(f->a, .34f);
        set_st(f, F_LAND); f->stun = .55f; f->vel.x = f->vel.z = 0;
        int flags = MF_HEAVY | (f->wp == WP_MAUL ? MF_BREAK | MF_KNOCK : 0);
        land_impact(f, 2.5f + (f->wp == WP_SPEAR ? .4f : 0), WP[f->wp].dmg * 2.3f * f->power, flags, weapon_color(f->wp));
      } else if (f->st == F_LEAP) {
        attack_end(f);
        play(f, "Sword_Attack", 1.3f, false, 0); actor_clip_seek(f->a, .34f);
        f->vel.x = f->vel.z = 0; f->armor = 0;
        land_impact(f, 3.2f, WP[f->wp].dmg * 2.3f * f->power, MF_HEAVY | MF_KNOCK | MF_UNBLOCK, C_ALERT);
        set_st(f, F_RECOVER); f->stun = .9f;
        f->atk_cd = ai_cooldown(f); release_token(f);
      }
    }
  } else if (f->st == F_FREE) { set_st(f, F_AIR); play(f, "NinjaJump_Idle_Loop", 1, true, .15f); }
  /* a lander out of the air with no weapon move */
  if (f->st == F_LAND && !f->player && f->t > f->stun) to_free(f, .2f);
}

/* boss phases */
static void boss_watch(fighter_t *f) {
  if (f->d.ai != AI_BOSS || !alive_foe(f) || FG.fin_t >= 0) return;
  float p1 = FG.def.phase2_at > 0 ? FG.def.phase2_at : .66f, p2 = FG.def.phase2_at > 0 ? FG.def.phase2_at * .45f : .33f;
  int want = f->hp < f->hpmax * p2 ? 2 : f->hp < f->hpmax * p1 ? 1 : 0;
  if (want > f->phase) {
    f->phase = want;
    boss_phase_enter(f, want);
    attack_end(f); release_token(f); f->hold = false;
    set_st(f, F_ROAR); f->inv = 1.1f; f->armor = 9;
    replay(f, "Idle_Shield_Break", 1, false, .08f);
    f->speed *= 1.1f;
    fx_burst(chest(f), C_ALERT, 1.5f);
    fx_shock(v3_(f->pos.x, f->pos.y + .05f, f->pos.z), .3f, 5, C_ALERT, .5f);
    sound(SFX_ROAR, f->pos, 1); shake(.6f);
    banner(want == 1 ? "SECOND WIND" : "LAST RESERVES", 1.2f);
    FG.screen_flash = .4f; FG.screen_col = C_ALERT;
    /* the blast pushes the player clear */
    fighter_t *P = &FG.P;
    float d = dist2d(P->pos, f->pos);
    if (d < 4.5f && P->st != F_ROLL) { v3 a = v3_norm(flat(v3_sub(P->pos, f->pos))); P->vel.x += a.x * 7; P->vel.z += a.z * 7; }
    if (FG.lock < 0) for (int i = 0; i < FG.nfoe; i++) if (&FG.foe[i] == f) FG.lock = i;
  }
}

/* keep fighters apart and inside the ring */
static void separate(void) {
  fighter_t *all[6]; int n = 0;
  all[n++] = &FG.P;
  for (int i = 0; i < FG.nfoe; i++) all[n++] = &FG.foe[i];
  if (FG.ally_on) all[n++] = &FG.ally;
  /* the twist's props stand in the way */
  v3 sp[8]; float sr[8]; int ns = twist_solids(sp, sr, 8);
  for (int i = 0; i < n; i++) for (int k = 0; k < ns; k++) {
    fighter_t *f = all[i];
    if (f->st == F_KNEEL || f->st == F_LEAP || f->pos.y > sp[k].y + 1.4f) continue;
    v3 d = flat(v3_sub(f->pos, sp[k])); float l = v3_len(d), want = sr[k] + PLAYER_R;
    if (l >= want) continue;
    if (l < 1e-3f) { d = v3_(1, 0, 0); l = 1; }
    v3 push = v3_scale(d, (want - l) / l);
    bool g = f->grounded; world_move(&f->pos, &push, PLAYER_R, 1.6f, 1, &g); f->grounded = g;
  }
  for (int i = 0; i < n; i++) for (int k = i + 1; k < n; k++) {
    fighter_t *a = all[i], *b = all[k];
    if (a->st == F_KNEEL || b->st == F_KNEEL) continue;
    if (a->st == F_ROLL || b->st == F_ROLL || a->st == F_LEAP || b->st == F_LEAP) continue;
    if (fabsf(a->pos.y - b->pos.y) > 1.2f) continue;
    v3 d = flat(v3_sub(b->pos, a->pos));
    float l = v3_len(d), want = PLAYER_R * 2 + .06f;
    if (l >= want || l < 1e-3f) continue;
    v3 v2 = v3_scale(d, (want - l) / l * .5f), v1 = v3_scale(v2, -1);
    bool g;
    g = a->grounded; world_move(&a->pos, &v1, PLAYER_R, 1.6f, 1, &g); a->grounded = g;
    g = b->grounded; world_move(&b->pos, &v2, PLAYER_R, 1.6f, 1, &g); b->grounded = g;
  }
  if (FG.def.radius > 0) for (int i = 0; i < n; i++) {
    fighter_t *f = all[i];
    v3 d = flat(v3_sub(f->pos, FG.def.center));
    float l = v3_len(d), r = FG.def.radius - .35f;
    if (l > r && l > .01f) {
      /* a soft shove back; firm a metre past it */
      float k = fminf(1, (l - r) * .22f);
      f->pos.x -= d.x / l * (l - r) * k; f->pos.z -= d.z / l * (l - r) * k;
      if (l - r > .9f) { f->pos.x -= d.x / l * (l - r - .9f); f->pos.z -= d.z / l * (l - r - .9f); }
    }
  }
}

/* ---- the camera ---- */
static float cam_yaw, cam_pitch = .2f, fov_now = 46;
static float lock_flick;
static void lock_input(const intent_t *in, float rdt) {
  if (in->lock) {
    if (FG.lock >= 0) FG.lock = -1;
    else FG.lock = pick_target(&FG.P, facing(&FG.P));
    sfx(SFX_UI, 0, .5f);
  }
  if (lock_flick > 0) lock_flick -= rdt;
  /* the next target: a right stick flick, or the mouse wheel (a captured mouse never flicks: looking
   * around with it must not keep changing who you are locked on to) */
  bool wheel = FG.lock >= 0 && in->wheel != 0;
  float flick = wheel ? (float)-in->wheel : in->mouse_look ? 0 : in->ax;
  if (FG.lock >= 0 && (wheel || lock_flick <= 0) && fabsf(flick) > .72f && foes_up() > 1) {
    /* the next foe that way round, seen from the camera */
    v3 f = v3_norm(flat(v3_sub(FG.cam_look, FG.cam_pos)));
    v3 r = v3_(f.z, 0, -f.x);
    float cur = v3_dot(flat(v3_sub(FG.foe[FG.lock].pos, FG.cam_pos)), r), best = 1e9f; int pick = -1;
    float s = flick > 0 ? -1.f : 1.f;   /* r here points left of the view */
    for (int i = 0; i < FG.nfoe; i++) {
      if (i == FG.lock || !alive_foe(&FG.foe[i])) continue;
      float x = v3_dot(flat(v3_sub(FG.foe[i].pos, FG.cam_pos)), r);
      float dx = (x - cur) * s;
      if (dx > 0 && dx < best) { best = dx; pick = i; }
    }
    if (pick < 0) for (int i = 0; i < FG.nfoe; i++) if (i != FG.lock && alive_foe(&FG.foe[i])) { pick = i; break; }
    if (pick >= 0) { FG.lock = pick; sfx(SFX_UI, 0, .5f); }
    lock_flick = .35f;
  }
}
/* the passing train is a wall to the lens too: a camera across the track saw nothing but its side for the
 * two seconds it took to go by. The distance along u to its box, or -1 */
static float track_z(void); static float floor_at(v3 p);
static float train_hit(v3 o, v3 u, float max) {
  if (FG.tw.train_st != 2) return -1;
  float tz = track_z(), fy = floor_at(v3_(FG.def.center.x, 0, tz));
  const float lo[3] = {FG.tw.train_x - 13.8f, fy, tz - 1.4f}, hi[3] = {FG.tw.train_x + 13.8f, fy + 3.6f, tz + 1.4f};
  const float oo[3] = {o.x, o.y, o.z}, dd[3] = {u.x, u.y, u.z};
  float t0 = 0, t1 = max;
  for (int a = 0; a < 3; a++) {
    if (fabsf(dd[a]) < 1e-6f) { if (oo[a] < lo[a] || oo[a] > hi[a]) return -1; continue; }
    float ta = (lo[a] - oo[a]) / dd[a], tb = (hi[a] - oo[a]) / dd[a];
    if (ta > tb) { float q = ta; ta = tb; tb = q; }
    t0 = fmaxf(t0, ta); t1 = fminf(t1, tb);
    if (t0 > t1) return -1;
  }
  return t0 > 0 ? t0 : -1;   /* from inside it (a fighter thrown against it), nothing to stop at */
}
static v3 spring_arm(v3 pivot, v3 want) {
  v3 d = v3_sub(want, pivot); float l = v3_len(d), t; v3 n;
  if (l < .01f) return want;
  v3 u = v3_scale(d, 1 / l);
  float hit = world_raycast(pivot, u, l + .25f, &t, &n) ? t : 1e9f, tr = train_hit(pivot, u, l + .25f);
  if (tr >= 0) hit = fminf(hit, tr);
  if (hit < 1e8f) { float k = fmaxf(.5f, hit - .25f); if (k < l) return v3_add(pivot, v3_scale(u, k)); }
  return want;
}
static void cam_update(const intent_t *in, float rdt) {
  fighter_t *P = &FG.P;
  v3 pivot = v3_(P->pos.x, P->pos.y + 1.3f, P->pos.z), pos, look;
  float tfov = 46, rate = 7;
  bool in_lab = world_in_lab(P->pos);
  if (FG.lock < 0 && in->wheel) player_zoom(in->wheel, false);
  float base = (in_lab ? 2.9f : 3.6f) * player_cam_zoom();
  fighter_t *T = FG.lock >= 0 ? &FG.foe[FG.lock] : NULL;

  if (FG.cine_t >= 0) {
    /* the special: a low close look, a whip round, then the blow from the side */
    float t = FG.cine_t;
    fighter_t *V = FG.cine_target >= 0 ? &FG.foe[FG.cine_target] : P;
    static float cy; static bool held;
    if (t < .5f || !held) { cy = P->yaw; held = t >= .5f; }
    if (t < .1f) held = false;
    float py = t < .5f ? P->yaw : cy;   /* after the rush the camera keeps its own angle: a spin must not spin it */
    int w = P->wp;
    if (t >= .5f && w == WP_MAUL && t < 1.3f) {
      /* low and wide, looking up into the leap */
      v3 mid = v3_lerp(P->pos, V->pos, .5f); mid.y = V->pos.y;
      float a = py - 1.4f;
      pos = v3_add(mid, v3_(sinf(a) * 6.5f, .5f, cosf(a) * 6.5f));
      look = v3_add(v3_lerp(chest(P), chest(V), .4f), v3_(0, .3f, 0));
      tfov = 50; rate = 14;
    } else if (t >= .5f && w == WP_SPEAR && t < 1.45f) {
      /* above the turn, circling slowly */
      float a = py + 2.4f + (t - .5f) * .8f;
      pos = v3_add(P->pos, v3_(sinf(a) * 4.6f, 2.6f, cosf(a) * 4.6f));
      look = v3_add(P->pos, v3_(0, .7f, 0));
      tfov = 52; rate = 12;
    } else if (t < .5f) {
      float a = py + ds_lerp(.75f, -.45f, t / .5f);
      pos = v3_add(P->pos, v3_(sinf(a) * 1.75f, .78f, cosf(a) * 1.75f));
      look = v3_add(P->pos, v3_(0, 1.12f, 0));
      tfov = 40; rate = 22;
    } else {
      float k = ds_clamp((t - .5f) / .75f, 0, 1); k = k * k * (3 - 2 * k);
      v3 mid = v3_lerp(P->pos, V->pos, .5f);
      float a = py + ds_lerp(-.5f, -2.2f, k), d = ds_lerp(3.2f, 4.4f, ds_clamp((t - 1.2f) / .7f, 0, 1));
      pos = v3_add(mid, v3_(sinf(a) * d, ds_lerp(.65f, 1.15f, k) + (w == WP_TWIN ? .9f : 0), cosf(a) * d));
      look = v3_add(mid, v3_(0, 1.f, 0));
      tfov = ds_lerp(44, 52, k); rate = 30;
    }
  } else if (FG.fin_t >= 0) {
    /* the finisher's held breath: low, close, the beaten one big in frame */
    fighter_t *V = &FG.foe[FG.fin_target];
    v3 d = flat(v3_sub(P->pos, V->pos)); d = v3_len(d) > .05f ? v3_norm(d) : v3_(0, 0, 1);
    float a = atan2f(d.x, d.z) + .9f + FG.fin_t * .25f;
    pos = v3_add(V->pos, v3_(sinf(a) * 3.6f, .8f, cosf(a) * 3.6f));
    look = v3_add(v3_lerp(V->pos, P->pos, .4f), v3_(0, 1.1f, 0));
    tfov = 40; rate = 10;
  } else if (FG.phase == PH_WON || FG.phase == PH_LOST) {
    /* a slow turn on the last blow: the beaten one's face, the winner off to one side */
    fighter_t *W = P;
    if (FG.phase == PH_WON) for (int i = 0; i < FG.nfoe; i++) if (FG.foe[i].st == F_KO || FG.foe[i].st == F_KNEEL) W = &FG.foe[i];
    v3 from = W == P ? (FG.nfoe ? FG.foe[0].pos : v3_add(P->pos, v3_(0, 0, 2))) : P->pos;
    v3 d = flat(v3_sub(from, W->pos)); d = v3_len(d) > .05f ? v3_norm(d) : v3_(0, 0, 1);
    float a = atan2f(d.x, d.z) + .6f + FG.phase_t * .12f;
    pos = v3_add(W->pos, v3_(sinf(a) * 2.7f, 1.0f, cosf(a) * 2.7f));
    look = v3_add(W->pos, v3_(0, .75f, 0));
    rate = 3.6f;
  } else if (FG.phase == PH_INTRO) {
    fighter_t *H = NULL;   /* the hero shot's subject: the boss if there is one */
    for (int i = 0; i < FG.nfoe; i++) if (!H || FG.foe[i].d.ai == AI_BOSS) H = &FG.foe[i];
    v3 mid = H ? H->pos : v3_add(P->pos, v3_(0, 0, 3));
    v3 d = flat(v3_sub(P->pos, mid)); d = v3_len(d) > .01f ? v3_norm(d) : v3_(0, 0, 1);
    v3 s = v3_(d.z, 0, -d.x);
    float t = FG.phase_t;
    v3 back_pos = v3_add(P->pos, v3_add(v3_scale(d, base), v3_add(v3_scale(s, .9f), v3_(0, 1.7f, 0))));
    v3 back_look = v3_add(v3_lerp(P->pos, mid, .45f), v3_(0, 1.f, 0));
    cam_yaw = atan2f(-d.x, -d.z);
    bool snap = false;
    if (FG.quick || t >= INTRO_BACK) {
      pos = back_pos; look = back_look; rate = 9; tfov = 46;
      if (!FG.quick && t - rdt < INTRO_BACK) snap = true;
    } else if (t >= INTRO_HERO) {
      /* low and close on the foe, looking up, a slow push in */
      float k = (t - INTRO_HERO) / (INTRO_CARD - INTRO_HERO);
      pos = v3_add(mid, v3_add(v3_scale(d, ds_lerp(3.6f, 3.1f, k)), v3_add(v3_scale(s, .8f), v3_(0, .28f, 0))));
      look = v3_add(mid, v3_(0, 1.18f, 0));
      tfov = 38; rate = 40; pivot = v3_add(mid, v3_(0, 1.2f, 0));
      if (t - rdt < INTRO_HERO) snap = true;
    } else {
      /* the whip: from over the player's shoulder round to the foe, fast */
      float k = t / INTRO_HERO; k = k * k * (3 - 2 * k);
      float a0 = atan2f(d.x, d.z) + 1.9f, a1 = atan2f(d.x, d.z) + .35f;
      float a = ds_lerp(a0, a1, k);
      pos = v3_add(P->pos, v3_(sinf(a) * 2.4f, 1.5f, cosf(a) * 2.4f));
      look = v3_add(v3_lerp(chest(P), mid, k), v3_(0, .4f, 0));
      tfov = 50; rate = 60;
      if (t < rdt * 1.5f) snap = true;
    }
    if (snap) { FG.cam_pos = spring_arm(pivot, pos); FG.cam_look = look; fov_now = tfov; }
  } else if (T) {
    /* both in frame: behind the player on the line between them, a shoulder to one side */
    v3 d = flat(v3_sub(P->pos, T->pos));
    float l = v3_len(d);
    d = l > .05f ? v3_scale(d, 1 / l) : v3_(sinf(cam_yaw), 0, cosf(cam_yaw));
    v3 s = v3_(d.z, 0, -d.x);
    float dist = ds_clamp(base + l * .33f, base, in_lab ? 5.f : 8.5f);
    cam_yaw = atan2f(-d.x, -d.z);
    float side = 1.05f + l * .05f;
    pos = v3_add(P->pos, v3_add(v3_scale(d, dist), v3_add(v3_scale(s, side), v3_(0, 1.68f + l * .1f, 0))));
    look = v3_add(v3_lerp(P->pos, T->pos, .42f), v3_(0, ds_lerp(1.f, .8f, ds_clamp(l / 8, 0, 1)), 0));
    tfov = ds_clamp(44 + l * 1.2f, 44, 58);
    rate = 6;
  } else {
    /* free: right stick orbits */
    cam_yaw -= in->ax * 2.4f * rdt;
    cam_pitch = ds_clamp(cam_pitch - in->ay * 1.4f * rdt, -.25f, .9f);
    float cp = cosf(cam_pitch);
    v3 out = v3_(-sinf(cam_yaw) * cp, sinf(cam_pitch), -cosf(cam_yaw) * cp);
    pos = v3_add(pivot, v3_scale(out, base + 1.f));
    look = v3_add(pivot, v3_(sinf(cam_yaw) * 1.f, -.25f, cosf(cam_yaw) * 1.f));
    rate = 10;
  }
  pos = spring_arm(pivot, pos);
  float fy = world_floor_y(v3_(pos.x, pos.y + .4f, pos.z));
  if (fy > -1e8f && pos.y < fy + .25f) pos.y = fy + .25f;
  float k = 1 - expf(-rate * rdt);
  FG.cam_pos = v3_lerp(FG.cam_pos, pos, k);
  /* never let the damping carry the camera through a wall */
  FG.cam_pos = spring_arm(pivot, FG.cam_pos);
  FG.cam_look = v3_lerp(FG.cam_look, look, 1 - expf(-(rate + 3) * rdt));
  fov_now = ds_damp(fov_now, tfov, 6, rdt);
  if (FG.witch_t > 0) fov_now = ds_damp(fov_now, tfov - 7, 8, rdt);
  v3 sp = FG.cam_pos, lk = FG.cam_look;
  if (FG.shake > 0) {
    float s = FG.shake * FG.shake;
    sp = v3_add(sp, v3_(ds_rr(-1, 1) * s * .11f, ds_rr(-1, 1) * s * .09f, ds_rr(-1, 1) * s * .11f));
    lk = v3_add(lk, v3_(ds_rr(-1, 1) * s * .05f, ds_rr(-1, 1) * s * .05f, 0));
    FG.shake = fmaxf(0, FG.shake - rdt * 2.6f);
  }
  t3_object_set_position(CAM, sp.x, sp.y, sp.z);
  t3_object_look_at(CAM, lk.x, lk.y, lk.z);
  if (fabsf(CAM->fov - fov_now) > .01f) { CAM->fov = fov_now; t3_camera_update_projection_matrix(CAM); }
  t3_object_update_world_matrix(CAM, true, true);
  FG.cam_fov = fov_now;
  audio_listener(FG.cam_pos.x, FG.cam_pos.y, FG.cam_pos.z, cam_yaw);
}

bool fight_project(v3 p, float *x, float *y) {
  const float *mw = CAM->base.matrix_world.e;
  v3 r = v3_(mw[0], mw[1], mw[2]), u = v3_(mw[4], mw[5], mw[6]), b = v3_(mw[8], mw[9], mw[10]), o = v3_(mw[12], mw[13], mw[14]);
  v3 d = v3_sub(p, o);
  float zc = -v3_dot(d, b);
  if (zc < .05f) return false;
  float t = tanf(CAM->fov * DS_PI / 360);
  *x = 640 + v3_dot(d, r) / (zc * t * (16.f / 9)) * 640;
  *y = 360 - v3_dot(d, u) / (zc * t) * 360;
  return true;
}

/* ==== v1.1: twists, hazards, props, barks, assists, the finisher ==== */

/* ---- props: small meshes the twist stands in the arena ---- */
enum { PR_OBJ, PR_DOOR, PR_BIKE, PR_TRAIN, PR_FROST, PR_GANTRY, PR_COUNT };
static t3_object *PROP[PR_COUNT];
static void props_clear(void) { for (int i = 0; i < PR_COUNT; i++) if (PROP[i]) { t3_object_remove(&SC->base, PROP[i]); PROP[i] = NULL; } }
static void prop_at(int i, v3 p, float yaw) { if (PROP[i]) { t3_object_set_position(PROP[i], p.x, p.y, p.z); t3_object_set_rotation(PROP[i], 0, yaw, 0); } }
static void prop_show(int i, bool on) { if (PROP[i]) PROP[i]->visible = on; }
static float floor_at(v3 p) { float fy = world_floor_y(v3_(p.x, FG.def.center.y + 1.5f, p.z)); return fy > -1e8f ? fy : FG.def.center.y; }
static float yaw_to(v3 from, v3 to) { return atan2f(to.x - from.x, to.z - from.z); }

/* the laser's emergency stop: a yellow post, a red mushroom, hazard tape round the foot */
static t3_object *build_estop(void) {
  fxp_begin();
  fxp_box(v3_(0, .5f, 0), v3_(.42f, 1.f, .42f), 0xe8c020);
  for (int i = 0; i < 4; i++) fxp_box(v3_(0, .12f + i * .2f, 0), v3_(.44f, .07f, .44f), 0x18181c);
  fxp_box(v3_(0, 1.04f, 0), v3_(.5f, .08f, .5f), 0x303238);
  fxp_cyl(v3_(0, 1.08f, 0), .1f, .2f, .22f, 8, 0xd02020);
  fxp_layer(FXP_GLOW); fxp_cyl(v3_(0, 1.18f, 0), .04f, .22f, .16f, 8, 0xff4030);
  fxp_layer(FXP_HALO); fxp_disc(v3_(0, .03f, 0), 1.1f, 12, 0x402008, 0);
  return fxp_end();
}
/* the fridge: a white box, its door on a hinge (its own prop), a cold glow from inside */
static t3_object *build_fridge(void) {
  fxp_begin();
  fxp_box(v3_(0, 1.f, -.05f), v3_(1.5f, 2.f, .9f), 0xe8ecef);
  fxp_box(v3_(0, 1.f, .38f), v3_(1.36f, 1.86f, .04f), 0x8ab8d0);   /* the inside, seen when it opens */
  fxp_box(v3_(0, 1.f, .36f), v3_(1.3f, .04f, .02f), 0xc8d8e0);
  fxp_box(v3_(0, 1.5f, .36f), v3_(1.3f, .04f, .02f), 0xc8d8e0);
  for (int i = 0; i < 5; i++) fxp_box(v3_(-.45f + i * .22f, 1.15f, .3f), v3_(.14f, .26f, .12f), i & 1 ? 0x70a040 : 0xd8b040);   /* jars */
  fxp_layer(FXP_GLOW); fxp_box(v3_(0, 1.88f, .3f), v3_(.6f, .04f, .06f), 0xd8f0ff);
  fxp_layer(FXP_HALO); fxp_disc(v3_(0, .03f, .9f), 1.6f, 14, 0x1a3040, 0);
  return fxp_end();
}
static t3_object *build_door(void) {   /* the hinge at the origin, the door along +x */
  fxp_begin();
  fxp_box(v3_(.72f, 1.f, 0), v3_(1.44f, 1.96f, .1f), 0xf2f4f6);
  fxp_box(v3_(1.25f, 1.2f, .09f), v3_(.06f, .7f, .08f), 0x9098a0);
  fxp_box(v3_(.72f, 1.5f, .055f), v3_(.5f, .3f, .01f), 0xf0e080);   /* a note: LABEL IT */
  return fxp_end();
}
/* the scrap cage: a steel frame with bars, copper and old electronics inside */
static t3_object *build_cage(void) {
  fxp_begin();
  float h = 1.7f, w = 1.1f;
  for (int i = 0; i < 4; i++) { float x = i & 1 ? w : -w, z = i & 2 ? w : -w; fxp_bar(v3_(x, 0, z), v3_(x, h, z), .08f, 0x6a707a); }
  for (int k = 0; k < 2; k++) { float y = k ? h : .05f;
    fxp_bar(v3_(-w, y, -w), v3_(w, y, -w), .06f, 0x6a707a); fxp_bar(v3_(-w, y, w), v3_(w, y, w), .06f, 0x6a707a);
    fxp_bar(v3_(-w, y, -w), v3_(-w, y, w), .06f, 0x6a707a); fxp_bar(v3_(w, y, -w), v3_(w, y, w), .06f, 0x6a707a); }
  for (int i = 1; i < 8; i++) { float a = -w + i * w / 4;
    fxp_bar(v3_(a, 0, -w), v3_(a, h, -w), .025f, 0x8a909a); fxp_bar(v3_(a, 0, w), v3_(a, h, w), .025f, 0x8a909a);
    fxp_bar(v3_(-w, 0, a), v3_(-w, h, a), .025f, 0x8a909a); fxp_bar(v3_(w, 0, a), v3_(w, h, a), .025f, 0x8a909a); }
  fxp_ring(v3_(-.4f, .35f, -.3f), v3_(0, 1, 0), .35f, .07f, 10, 0xc87038);
  fxp_ring(v3_(-.4f, .5f, -.3f), v3_(0, 1, 0), .3f, .07f, 10, 0xd88040);
  fxp_ring(v3_(.4f, .3f, .3f), v3_(1, 1, 0), .32f, .06f, 10, 0xc87038);
  fxp_box(v3_(.35f, .25f, -.45f), v3_(.6f, .3f, .4f), 0x2a2c30);   /* a VCR */
  fxp_box(v3_(-.3f, .2f, .45f), v3_(.5f, .4f, .4f), 0x8a8478);
  fxp_box(v3_(.2f, .6f, .1f), v3_(.4f, .25f, .5f), 0x3a6a40);   /* a board */
  fxp_layer(FXP_HALO); fxp_disc(v3_(0, .03f, 0), 2.f, 12, 0x30200c, 0);
  return fxp_end();
}
/* v1.3: the lab's welding table, rolled out to the back lot: a steel top on four legs and casters, a vise, the
 * welder on its cart beside it with a gas bottle */
static t3_object *build_weld_table(void) {
  fxp_begin();
  uint32_t st = 0x5a5e66, dk = 0x2a2c30;
  fxp_box(v3_(0, .86f, 0), v3_(1.5f, .06f, .9f), 0x7a7f88);                 /* the top */
  for (int i = 0; i < 4; i++) { float x = i & 1 ? .66f : -.66f, z = i & 2 ? .36f : -.36f;
    fxp_bar(v3_(x, .1f, z), v3_(x, .84f, z), .07f, st); fxp_cyl(v3_(x, 0, z), .1f, .05f, .05f, 8, dk); }
  fxp_bar(v3_(-.66f, .3f, -.36f), v3_(.66f, .3f, -.36f), .05f, st); fxp_bar(v3_(-.66f, .3f, .36f), v3_(.66f, .3f, .36f), .05f, st);
  fxp_box(v3_(.62f, .96f, .3f), v3_(.18f, .14f, .14f), 0x3a5a8a);           /* the vise */
  fxp_box(v3_(-.3f, .92f, -.1f), v3_(.5f, .05f, .08f), 0xb87838);           /* a bracket mid weld */
  fxp_box(v3_(-1.1f, .45f, 0), v3_(.4f, .5f, .55f), 0x2a6a9a);              /* the welder on its cart */
  fxp_cyl(v3_(-1.25f, 0, .35f), 1.1f, .11f, .11f, 10, 0x3a7a3a);            /* the gas bottle */
  fxp_layer(FXP_GLOW); fxp_box(v3_(-.88f, .62f, 0), v3_(.02f, .05f, .08f), 0xffb040);
  fxp_layer(FXP_HALO); fxp_disc(v3_(0, .03f, 0), 2.f, 12, 0x30200c, 0);
  return fxp_end();
}
t3_object *fight_weld_table_model(void) { return build_weld_table(); }   /* the story stands the same table in its scenes */
/* night 3: the fix-it table, a folding table with the fix-it bins on it and the things waiting in them (a
 * desk lamp with its shade off, a toaster, a radio), each bin with its white label */
static t3_object *build_fixit_bins(void) {
  fxp_begin();
  uint32_t leg = 0x2a2c30;
  fxp_box(v3_(0, .74f, 0), v3_(1.8f, .04f, .76f), 0xe8e6e0);                 /* the top */
  for (int i = 0; i < 4; i++) { float x = i & 1 ? .82f : -.82f, z = i & 2 ? .32f : -.32f; fxp_bar(v3_(x, 0, z), v3_(x, .72f, z), .035f, leg); }
  static const uint32_t BIN[3] = {0x2c6ac4, 0x8a8e94, 0xd8b030};
  for (int k = 0; k < 3; k++) {
    float x = -.56f + k * .56f;
    fxp_box(v3_(x, .9f, 0), v3_(.5f, .28f, .4f), BIN[k]);
    fxp_box(v3_(x, .93f, .205f), v3_(.3f, .08f, .01f), 0xf4f4f0);             /* the label */
  }
  fxp_box(v3_(-.6f, 1.08f, -.04f), v3_(.22f, .1f, .16f), 0xb8bcc0);         /* a toaster */
  fxp_box(v3_(0, 1.1f, .02f), v3_(.3f, .14f, .1f), 0x6a4a30);               /* a radio */
  fxp_bar(v3_(.56f, 1.04f, 0), v3_(.62f, 1.36f, .04f), .02f, 0x2a6a4a);     /* the lamp's arm */
  fxp_cyl(v3_(.7f, .76f, .22f), .09f, .1f, .14f, 8, 0x2a6a4a);              /* its shade, off, on the table */
  fxp_layer(FXP_HALO); fxp_disc(v3_(0, .03f, 0), 2.f, 12, 0x30200c, 0);
  return fxp_end();
}
/* a bike along +x, wheels standing */
static t3_object *build_bike(void) {
  fxp_begin();
  uint32_t fr = 0x2a9a8a, bl = 0x18181c;
  fxp_ring(v3_(-.52f, .33f, 0), v3_(0, 0, 1), .31f, .03f, 12, bl);
  fxp_ring(v3_(.52f, .33f, 0), v3_(0, 0, 1), .31f, .03f, 12, bl);
  fxp_bar(v3_(-.52f, .33f, 0), v3_(-.05f, .33f, 0), .04f, fr);
  fxp_bar(v3_(-.52f, .33f, 0), v3_(-.12f, .78f, 0), .04f, fr);
  fxp_bar(v3_(-.05f, .33f, 0), v3_(-.12f, .78f, 0), .045f, fr);
  fxp_bar(v3_(-.05f, .33f, 0), v3_(.4f, .74f, 0), .045f, fr);
  fxp_bar(v3_(-.12f, .74f, 0), v3_(.4f, .74f, 0), .045f, fr);
  fxp_bar(v3_(.52f, .33f, 0), v3_(.42f, .9f, 0), .04f, fr);
  fxp_bar(v3_(.42f, .9f, -.24f), v3_(.42f, .9f, .24f), .035f, bl);
  fxp_box(v3_(-.14f, .82f, 0), v3_(.24f, .05f, .1f), bl);
  fxp_box(v3_(-.62f, .62f, 0), v3_(.3f, .03f, .16f), 0x9098a0);   /* a rack */
  fxp_layer(FXP_HALO); fxp_disc(v3_(0, .02f, 0), .9f, 10, 0x40300a, 0);
  return fxp_end();
}
/* the light rail: two white sections, a teal band, dark glass, lamps, along +x */
static t3_object *build_train(void) {
  fxp_begin();
  for (int s = -1; s <= 1; s += 2) {
    float x = s * 6.9f;
    fxp_box(v3_(x, 2.f, 0), v3_(13.4f, 2.6f, 2.65f), 0xdadede);
    fxp_box(v3_(x, .55f, 0), v3_(12.5f, .7f, 2.4f), 0x1a1a1c);
    fxp_box(v3_(x, 1.1f, 0), v3_(13.45f, .5f, 2.7f), 0x1a6a74);
    fxp_box(v3_(x, 2.35f, 0), v3_(13.f, 1.1f, 2.7f), 0x1c2228);
    fxp_box(v3_(x, 3.42f, 0), v3_(2.5f, .25f, 1.2f), 0x1a1a1c);
  }
  fxp_box(v3_(0, 2, 0), v3_(.6f, 2.6f, 2.4f), 0x1a1a1c);
  fxp_layer(FXP_GLOW);
  for (int s = -1; s <= 1; s += 2) for (int z = -1; z <= 1; z += 2) fxp_box(v3_(s * 13.62f, 1.3f, z * .85f), v3_(.06f, .18f, .3f), 0xfff4c0);
  return fxp_end();
}
static t3_object *build_frost(void) {
  fxp_begin(); fxp_layer(FXP_HALO);
  for (int i = 0; i < FG.tw.nfrost; i++) {
    v3 c = FG.tw.frost[i]; c.y = floor_at(c) + .03f + i * .002f;
    fxp_disc(c, FG.tw.frost_r[i], 16, 0x4a7088, 0x0a1620);
    for (int k = 0; k < 5; k++) { float a = k * 1.256f + i, r = FG.tw.frost_r[i] * .75f; v3 p = v3_add(c, v3_(sinf(a) * r * .6f, .005f, cosf(a) * r * .6f));
      fxp_quad(p, v3_add(p, v3_(sinf(a + 1.4f) * r * .4f, 0, cosf(a + 1.4f) * r * .4f)), v3_add(p, v3_(sinf(a) * r * .5f, 0, cosf(a) * r * .5f)), p, 0x5a8aa8); }
  }
  return fxp_end();
}
/* the laser's head over the floor in the timer fight: where the sweep comes from */
static t3_object *build_gantry(void) {
  fxp_begin();
  fxp_box(v3_(0, .3f, 0), v3_(.7f, .6f, .7f), 0x303238);
  fxp_box(v3_(0, .62f, 0), v3_(.9f, .06f, .9f), 0xe8c020);
  fxp_layer(FXP_GLOW); fxp_cyl(v3_(0, .3f, 0), .2f, .2f, .2f, 8, 0xff3a3a);
  return fxp_end();
}

/* ---- twist rules ---- */
/* the defended object's short name for the tags over it */
const char *fight_obj_name(void) { return FG.def.obj_kind == 1 ? "TABLE" : "BINS"; }   /* 2, the fix-it bins, are BINS too */
static const char *twist_label(void) {
  if (FG.def.obj_label) return FG.def.obj_label;
  switch (FG.tw.kind) {
    case FT_TIMER: return "STOP THE CUT"; case FT_RETRIEVE: return "GET THE BIKE BACK"; case FT_SCORE: return "BEAT THE HIGH SCORE";
    case FT_HOLD: return "KEEP THE DOOR SHUT"; case FT_DEFEND: return FG.def.obj_kind == 1 ? "GUARD THE WELDING TABLE" : FG.def.obj_kind == 2 ? "GUARD THE FIX-IT BINS" : "GUARD THE BINS"; case FT_PROTECT: return "KEEP SAM SAFE";
    default: return NULL;
  }
}
const char *fight_twist_label(void) { return twist_label(); }
static bool tw_is(int k) { return FG.tw.kind == k; }
static v3 sam_pos(void) { return FG.def.protect_actor ? actor_pos(FG.def.protect_actor) : FG.tw.obj; }
static v3 door_front(void) { v3 d = flat(v3_sub(FG.def.center, FG.tw.obj)); d = v3_len(d) > .01f ? v3_norm(d) : v3_(0, 0, 1); return v3_add(FG.tw.obj, v3_scale(d, 1.25f)); }
static int carrier_idx(const fighter_t *f) { return (int)(f - FG.foe); }
static v3 escape_point(const fighter_t *f) {
  v3 d = flat(v3_sub(f->pos, FG.P.pos)); d = v3_len(d) > .01f ? v3_norm(d) : v3_(0, 0, -1);
  v3 c = flat(v3_sub(f->pos, FG.def.center));
  v3 want = v3_norm(v3_add(d, v3_scale(c, .08f)));
  float r = (FG.def.radius > 0 ? FG.def.radius : 10) - 1.f;
  return v3_add(FG.def.center, v3_scale(want, r));
}
static v3 foe_aim(const fighter_t *f) {
  if (!f->goal || f->aggro > 0) return FG.P.pos;
  switch (FG.tw.kind) {
    case FT_DEFEND: return FG.tw.obj;
    case FT_PROTECT: return sam_pos();
    case FT_HOLD: return door_front();
    case FT_RETRIEVE: return FG.tw.carrier == carrier_idx(f) ? escape_point(f) : FG.tw.carrier == -1 ? FG.tw.obj : FG.P.pos;
    default: return FG.P.pos;
  }
}
static float twist_goal_reach(const fighter_t *f) {
  float r = WP[f->wp].reach * .8f;
  switch (FG.tw.kind) {
    case FT_DEFEND: return 1.2f + r; case FT_PROTECT: return .35f + r; case FT_HOLD: return .4f + r;
    default: return 0;   /* the bike: no swinging at it */
  }
}
static float twist_goal_speed(const fighter_t *f) {
  if (tw_is(FT_RETRIEVE) && FG.tw.carrier == carrier_idx(f)) return 3.1f;
  return 3.7f;
}
static int twist_solids(v3 *p, float *r, int max) {
  int n = 0;
  switch (FG.tw.kind) {
    case FT_TIMER: if (n < max) { p[n] = FG.tw.obj; r[n++] = .35f; } break;
    case FT_HOLD: if (n < max) { p[n] = FG.tw.obj; r[n++] = .85f; } break;
    case FT_DEFEND: if (n < max) { p[n] = FG.tw.obj; r[n++] = 1.15f; } break;
    case FT_PROTECT: if (n < max && FG.tw.obj_hp > 0) { p[n] = sam_pos(); r[n++] = .3f; } break;
    default: break;
  }
  return n;
}
static void twist_warn(const char *s, float t) { FG.tw.warn = s; FG.tw.warn_t = t; }

/* the fight ends on the twist: won (the objective itself) or lost (it failed) */
static void twist_end(bool won, const char *line) {
  if (FG.phase != PH_FIGHT) return;
  twist_t *T = &FG.tw;
  T->fail = line;
  FG.lock = -1;
  if (won) {
    T->won_obj = true;
    FG.phase = PH_WON; FG.phase_t = 0; FG.screen_flash = .9f; FG.screen_col = 0xffffff;
    say_slow(.2f, 1.f); shake(.6f); sfx(SFX_KO, 0, 1); music_mood(MOOD_CLEAR);
    if (FG.P.st != F_SPECIAL) attack_end(&FG.P);
    /* the rest give up where they stand */
    for (int i = 0; i < FG.nfoe; i++) if (alive_foe(&FG.foe[i])) foe_ko(&FG.foe[i]);
    bark(FB_FINISH);
  } else {
    FG.phase = PH_LOST; FG.phase_t = 0; FG.screen_flash = .7f; FG.screen_col = C_ALERT;
    say_slow(.25f, 1.f); shake(.6f); sfx(SFX_KO, 0, 1);
    if (FG.P.st != F_KO && FG.P.st != F_SPECIAL) to_free(&FG.P, .2f);
    for (int i = 0; i < FG.nfoe; i++) { fighter_t *o = &FG.foe[i]; if (alive_foe(o)) { attack_end(o); release_token(o); o->hold = false; set_st(o, F_FREE); play(o, idle_clip(o), 1, true, .3f); } }
  }
}

/* a foe that should not go down yet: the tutorial's lessons, the arcade champ's continue */
static bool foe_hold_up(fighter_t *def) {
  twist_t *T = &FG.tw;
  if (tw_is(FT_TUTORIAL) && T->step < TUT_SPECIAL && def->hp < def->hpmax * .35f) { def->hp = def->hpmax * .35f; return true; }
  if (tw_is(FT_SCORE) && !T->won_obj && def->hp <= 0) {
    def->hp = def->hpmax * .6f; def->ghost = def->hp;
    banner("INSERT COIN", 1.f); sfx(SFX_UI_OK, 0, 1);
    fx_ring_up(chest(def), .2f, 2, 0xfff0a0, .4f);
    return true;
  }
  return false;
}

/* style points (the arcade); the other twists still keep the kinds of move used for the result */
static int style_last[2];
static float style_seen[11];
static void style_add(float pts, int kind) {
  FG.moves_used |= (uint32_t)kind;
  int k = 0; while (k < 11 && !(kind & (1 << k))) k++;
  if (k < 11) style_seen[k] = FG.clock;
  if (!tw_is(FT_SCORE) || FG.phase != PH_FIGHT) return;
  twist_t *T = &FG.tw;
  int variety = 0; for (int i = 0; i < 11; i++) if (style_seen[i] > 0 && FG.clock - style_seen[i] < 6) variety++;
  bool stale = kind == style_last[0] && kind == style_last[1];
  style_last[1] = style_last[0]; style_last[0] = kind;
  float mult = (1 + FG.combo * .04f) * (1 + .2f * (variety - 1)) * (FG.damage_taken <= 0 ? 1.5f : 1) * (stale ? .35f : 1);
  T->score += pts * mult;
  T->style = fminf(100, T->style + pts / 45 * (stale ? .3f : 1) * (1 + .15f * variety));
  int rank = T->style >= 80 ? 4 : T->style >= 60 ? 3 : T->style >= 40 ? 2 : T->style >= 20 ? 1 : 0;
  if (rank > T->ranks) { sfx(SFX_STYLE, 0, rank / 4.f); FG.combo_pop = 1; }
  T->ranks = rank;
}

/* a lesson's step done once */
static const float TUT_NEED[TUT_FREE] = { 1.2f, 3, 2, 2, 1, 1, 1 };
static void tut_done(int step) {
  twist_t *T = &FG.tw;
  if (!tw_is(FT_TUTORIAL) || T->step != step || FG.phase != PH_FIGHT) return;
  T->step_n += 1;
}
static void tut_next(void) {
  twist_t *T = &FG.tw;
  T->step++; T->step_n = 0; T->step_t = 0;
  banner(T->step >= TUT_FREE ? "NOW FOR REAL" : "GOOD", .8f); sfx(SFX_UI_OK, 0, .9f);
  if (T->step == TUT_SPECIAL) { FG.meter = 100; }
}

/* the twist hears about every blow the player lands */
static void twist_hit(fighter_t *att, fighter_t *def, float dmg, int flags) {
  twist_t *T = &FG.tw;
  if (att->player) {
    int kind = att->wp == WP_STAFF && att->st != F_ATTACK ? SK_BOLT : att->st == F_PLUNGE || att->st == F_LAND ? SK_AIR
      : att->st == F_ATTACK && att->mv == MV_DASH ? SK_DASH : (flags & MF_HEAVY) ? SK_HEAVY : SK_LIGHT;
    if (att->st != F_SPECIAL) style_add(dmg * 22, kind);
    if (att->st != F_SPECIAL) tut_done((flags & MF_HEAVY) ? TUT_HEAVY : TUT_LIGHT);
  }
  if ((att->player || att->ally) && def->goal) {
    if (tw_is(FT_RETRIEVE) && T->carrier == carrier_idx(def)) {
      T->carry_hits += (flags & MF_HEAVY) ? 2 : 1;
    } else def->aggro = 3.5f;
  }
}
static void twist_player_hurt(float dmg) {
  if (tw_is(FT_SCORE)) { FG.tw.style = fmaxf(0, FG.tw.style - 30); FG.tw.ranks = 0; }
}

/* the player's blade meets the objective (m NULL: an area blow, heavy) or a foe's does */
static void obj_strike(fighter_t *f, const move_t *m) {
  twist_t *T = &FG.tw;
  if (FG.phase != PH_FIGHT) return;
  bool heavy = !m || (m->flags & MF_HEAVY);
  if (f->player) {
    v3 o; float r;
    if (tw_is(FT_TIMER) && !T->obj_done) { o = T->obj; r = .35f; }
    else if (tw_is(FT_HOLD)) { o = door_front(); r = .6f; }
    else return;
    if (m) {
      if (f->hitmask & (1u << 30)) return;
      v3 d = flat(v3_sub(o, f->pos)); float l = v3_len(d);
      if (l > move_reach(f, m) + r + PLAYER_R + .15f) return;
      if (l > .5f && v3_dot(v3_scale(d, 1 / l), facing(f)) < cosf(fminf(m->arc + .35f, 3.1f))) return;
      f->hitmask |= 1u << 30;
    }
    v3 at = v3_add(o, v3_(0, 1.05f, 0));
    T->obj_flash = 1; shake(.3f);
    fx_sparks(at, v3_(0, 1, 0), weapon_color(f->wp), 12);
    if (tw_is(FT_TIMER)) {
      T->obj_hp -= heavy ? 2 : 1;
      sound(SFX_SLAM, o, .9f);
      if (T->obj_hp <= 0) {
        T->obj_done = true; T->obj_hp = 0;
        banner("LASER STOPPED", 1.2f); sfx(SFX_PERFECT, 0, .8f);
        FG.screen_flash = .4f; FG.screen_col = 0xffffff;
        fx_burst(at, 0xff4030, .9f); style_add(2000, SK_BREAK);
      }
    } else {
      float was = T->meter;
      T->meter = fmaxf(0, T->meter - (heavy ? .5f : .28f));
      sound(SFX_SLAM, o, 1);
      fx_smoke(v3_add(T->obj, v3_(0, 1, 0)), .9f, 0xc0e0f0, 6, .5f);
      if (was > .25f) banner("SLAM", .6f);
    }
    return;
  }
  /* a foe at the objective */
  if (f->hitmask & 1) return;
  v3 o = foe_aim(f);
  if (dist2d(f->pos, o) > twist_goal_reach(f) + .7f) return;
  f->hitmask |= 1;
  float dmg = move_damage(f, m ? m : &MOVES[MV_L1]);
  switch (T->kind) {
    case FT_DEFEND:
      T->obj_hp -= dmg * 1.1f; T->obj_flash = 1;
      fx_sparks(v3_add(T->obj, v3_(0, 1, 0)), v3_(0, 1, 0), 0xd88040, 10); sound(SFX_CLASH, T->obj, .9f);
      break;
    case FT_PROTECT: {
      T->obj_hp -= dmg * 1.1f; T->obj_flash = 1;
      actor_t *s = FG.def.protect_actor;
      v3 c = v3_add(sam_pos(), v3_(0, 1.05f, 0));
      fx_sparks(c, v3_(0, 1, 0), 0xffffff, 8); sound(SFX_HIT_LIGHT, c, 1);
      if (s) { actor_replay(s, "Hit_Chest", 1, false, .05f); actor_expr_set(s, EX_SORROW, 1); }
      T->sam_st = 2; T->sam_t = 0;
      break; }
    case FT_HOLD:
      T->meter = fminf(1, T->meter + .1f); T->obj_flash = 1;
      sound(SFX_SLAM, T->obj, .6f); fx_smoke(v3_add(T->obj, v3_(0, 1, 0)), .7f, 0xc0e0f0, 4, .5f);
      break;
    default: break;
  }
}
/* the stop or the door close enough to swing at */
static bool obj_in_reach(const fighter_t *P, v3 *at) {
  const twist_t *T = &FG.tw;
  if (tw_is(FT_TIMER) && !T->obj_done) *at = T->obj;
  else if (tw_is(FT_HOLD)) *at = door_front();
  else return false;
  return dist2d(*at, P->pos) < WP[P->wp].reach + .9f;
}
/* a player's bolt into the objective */
static bool obj_bolt(v3 p) {
  twist_t *T = &FG.tw;
  if (!(tw_is(FT_TIMER) && !T->obj_done) && !tw_is(FT_HOLD)) return false;
  v3 o = tw_is(FT_HOLD) ? door_front() : T->obj;
  if (dist2d(p, o) > (tw_is(FT_HOLD) ? .8f : .5f) || p.y > o.y + 2.2f) return false;
  obj_strike(&FG.P, &MOVES[MV_L1]);
  FG.P.hitmask &= ~(1u << 30);
  return true;
}

/* hazards hurt whoever is there, foes too */
static void haz_hit(fighter_t *f, float dmg, v3 dir, bool knock) {
  if (FG.phase != PH_FIGHT || f->ally || f->haz_inv > 0 || f->inv > 0 || f->st == F_KO || f->st == F_KNEEL || f->st == F_SPECIAL) return;
  f->haz_inv = 1.3f;
  if (!f->player) dmg *= .6f;
  if (mod(MOD_HAZARD_PLUS)) dmg *= 1.25f;
  if (f->player && mod(MOD_GLASS)) dmg *= 2;
  f->hp -= dmg; f->flash = 1;
  dir = v3_len(flat(dir)) > .01f ? v3_norm(flat(dir)) : v3_(0, 0, 1);
  f->vel.x = dir.x * 5; f->vel.z = dir.z * 5;
  fx_sparks(chest(f), v3_(0, 1, 0), 0xffc080, 10); sound(SFX_HIT_HEAVY, f->pos, 1); shake(.4f);
  if (f->player) {
    FG.damage_taken += dmg; FG.combo = 0; twist_player_hurt(dmg);
    bark(FB_HAZARD);
    if (FG.def.tutorial && f->hp < 1) f->hp = 1;
  } else foe_hold_up(f);
  if (f->hp <= 0) {
    if (!f->player && fin_try(f)) return;
    f->hp = 0;
    if (f->player) player_ko(); else foe_ko(f);
    return;
  }
  if (f->armor > 0 || f->st == F_ROAR || f->st == F_LEAP) return;
  if (knock) knock_down(f);
  else { attack_end(f); release_token(f); set_st(f, F_HIT); f->stun = .4f; replay(f, "Hit_Chest", 1, false, .04f); }
}
static fighter_t *each_fighter(int i) { return i == 0 ? &FG.P : i <= FG.nfoe ? &FG.foe[i - 1] : NULL; }

/* the laser sweep: a low beam turning round the ring from its middle; jump it or roll through */
static void sweep_update(float dt, float speed) {
  twist_t *T = &FG.tw;
  v3 c = FG.def.center; c.y = floor_at(c) + .42f;
  float L = (FG.def.radius > 0 ? FG.def.radius : 9) - .2f;
  T->sweep = ds_wrap_pi(T->sweep + dt * speed);
  v3 d = v3_(sinf(T->sweep), 0, cosf(T->sweep)), e = v3_add(c, v3_scale(d, L));
  float k = T->sweep_on;
  fx_beam(c, e, 0xff2a2a, .24f * k); fx_beam(c, e, 0xffffff, .07f * k); fx_glow(c, 1.2f * k, 0xff4040); fx_glow(e, .5f * k, 0xff4040);
  if ((int)(FG.clock * 20) % 2 == 0) fx_sparks(e, v3_(0, 1, 0), 0xff6040, 1);
  if (k < .9f) return;
  for (int i = 0; i < 1 + FG.nfoe; i++) {
    fighter_t *f = each_fighter(i);
    v3 r = flat(v3_sub(f->pos, c));
    float along = v3_dot(r, d), off = fabsf(v3_dot(r, v3_(d.z, 0, -d.x)));
    if (along < 0 || along > L || off > .42f) continue;
    if (f->pos.y - floor_at(f->pos) > .3f) continue;   /* jumped it */
    haz_hit(f, 8, v3_(d.z * (v3_dot(r, v3_(d.z, 0, -d.x)) > 0 ? 1 : -1), 0, -d.x * (v3_dot(r, v3_(d.z, 0, -d.x)) > 0 ? 1 : -1)), true);
  }
}
/* marks on the floor that erupt: the boss's arena once it turns */
static uint32_t strike_col(void) {
  switch (world_get_stage()) { case STAGE_ARENA_LASER: return 0xff3a78; case STAGE_ARENA_FRIDGE: return 0x9fe0ff; case STAGE_ARENA_SHOP: return 0xffb040; default: return 0xff6a30; }
}
static void strikes_update(float dt, int phase) {
  twist_t *T = &FG.tw;
  uint32_t col = strike_col();
  if (T->nstrike == 0) {
    T->haz_t -= dt * (mod(MOD_HAZARD_PLUS) ? 1.8f : 1);
    if (T->haz_t > 0) return;
    T->haz_t = phase >= 2 ? 4.2f : 5.6f;
    T->nstrike = phase >= 2 ? 3 : 2; T->strike_t = 1.45f;
    float R = (FG.def.radius > 0 ? FG.def.radius : 9) - 1.5f;
    for (int i = 0; i < T->nstrike; i++) {
      v3 p = i == 0 ? FG.P.pos : v3_add(FG.def.center, v3_(ds_rr(-R, R) * .8f, 0, ds_rr(-R, R) * .8f));
      p.y = floor_at(p) + .05f; T->strike[i] = p;
    }
    sound(SFX_TELL, T->strike[0], .7f);
    return;
  }
  T->strike_t -= dt;
  float k = 1 - ds_clamp(T->strike_t / 1.45f, 0, 1);
  for (int i = 0; i < T->nstrike; i++) {
    v3 p = T->strike[i];
    fx_ring_flat(p, 1.5f, C_ALERT); fx_ring_flat(p, 1.5f * (1 - k) + .1f, col);
    fx_glow(v3_add(p, v3_(0, .1f, 0)), .6f + k, col);
    if (world_get_stage() == STAGE_ARENA_LASER && k > .6f) fx_beam(v3_add(p, v3_(0, 9, 0)), p, col, .03f + (k - .6f) * .2f);
  }
  if (T->strike_t <= 0) {
    for (int i = 0; i < T->nstrike; i++) {
      v3 p = T->strike[i];
      fx_shock(p, .2f, 2.2f, col, .4f); fx_burst(v3_add(p, v3_(0, .4f, 0)), col, .7f);
      for (int s = 0; s < 6; s++) fx_streak(p, v3_add(p, v3_(ds_rr(-.6f, .6f), ds_rr(1.5f, 3.f), ds_rr(-.6f, .6f))), col, .1f, .4f);
      if (world_get_stage() == STAGE_ARENA_FRIDGE) fx_smoke(p, 1, 0xd0f0ff, 5, 1);
      sound(SFX_SHOCK, p, .8f);
      for (int k2 = 0; k2 < 1 + FG.nfoe; k2++) {
        fighter_t *f = each_fighter(k2);
        if (dist2d(f->pos, p) < 1.55f && f->pos.y - p.y < 1.2f) haz_hit(f, 11, v3_sub(f->pos, p), true);
      }
    }
    T->nstrike = 0;
  }
}
/* the light rail along Main: a bell, stripes on the track, then the train */
static float track_z(void) {
  v3 c = FG.def.center;
  if (FG.def.twist_b != 0) return c.z + FG.def.twist_b;
  if (world_get_stage() == STAGE_TOWN && fabsf(17.3f - c.z) < (FG.def.radius > 0 ? FG.def.radius : 9) - 1.5f) return 17.3f;   /* the near track of Main (city: MAIN_CENTER - 2.2) */
  return c.z + 3.5f;
}
static void train_update(float dt) {
  twist_t *T = &FG.tw;
  v3 c = FG.def.center; float tz = track_z(), R = FG.def.radius > 0 ? FG.def.radius : 9, fy = floor_at(v3_(c.x, 0, tz));
  if (T->train_st == 0) {
    T->train_t -= dt * (mod(MOD_HAZARD_PLUS) ? 1.8f : 1);
    if (T->train_t <= 3.6f) { T->train_st = 1; sfx(SFX_TRAIN, 0, 1); twist_warn("TRAIN COMING", 3.6f); bark(FB_HAZARD); }
  } else if (T->train_st == 1) {
    T->train_t -= dt;
    bool blink = fmodf(FG.clock * 4, 1) < .5f;
    for (int e = -1; e <= 1; e += 2) fx_beam(v3_(c.x - R, fy + .06f, tz + e * 1.45f), v3_(c.x + R, fy + .06f, tz + e * 1.45f), blink ? 0xffc020 : 0x402008, .1f);
    if (T->train_t <= 0) { T->train_st = 2; T->train_x = c.x - R - 16; prop_show(PR_TRAIN, true); }
  } else {
    T->train_x += dt * 17;
    prop_at(PR_TRAIN, v3_(T->train_x, fy, tz), DS_PI * .5f * 0);
    for (int i = 0; i < 1 + FG.nfoe; i++) {
      fighter_t *f = each_fighter(i);
      if (fabsf(f->pos.x - T->train_x) < 13.8f + PLAYER_R && fabsf(f->pos.z - tz) < 1.4f + PLAYER_R) {
        float side = f->pos.z >= tz ? 1.f : -1.f;
        haz_hit(f, 18, v3_(.6f, 0, side), true);
        f->pos.z = tz + side * (1.45f + PLAYER_R);
      }
    }
    if (T->carrier == -1 && fabsf(T->obj.x - T->train_x) < 14 && fabsf(T->obj.z - tz) < 1.6f) T->obj.z = tz + (T->obj.z >= tz ? 1.8f : -1.8f);
    if (T->train_x > c.x + R + 16) { T->train_st = 0; T->train_t = 20; prop_show(PR_TRAIN, false); }
  }
}

/* Sam: cowers, then runs from whoever comes close */
static void sam_update(float dt) {
  twist_t *T = &FG.tw;
  actor_t *s = FG.def.protect_actor;
  if (!s) return;
  T->sam_t += dt;
  v3 p = actor_pos(s);
  int near = -1; float nd = 1e9f;
  for (int i = 0; i < FG.nfoe; i++) { fighter_t *o = &FG.foe[i]; if (!alive_foe(o)) continue; float d = dist2d(o->pos, p); if (d < nd) { nd = d; near = i; } }
  float yaw = actor_yaw(s);
  if (T->sam_st == 2) {   /* just hit */
    if (T->sam_t > .5f) { T->sam_st = 0; T->sam_t = 1; }
  } else if (T->sam_st == 1) {   /* running */
    v3 d = flat(v3_sub(T->sam_to, p)); float l = v3_len(d);
    if (l < .3f || T->sam_t > 3) { T->sam_st = 0; T->sam_t = 0; actor_play(s, "Crouch_Idle_Loop", 1, true, .2f); actor_hands_up(s, 1); }
    else {
      v3 v = v3_scale(d, 3.6f / l); bool g = true;
      world_move(&p, &v, PLAYER_R, 1.6f, dt, &g); v3 fy = p; fy.y = floor_at(p); if (fy.y > p.y - .5f) p.y = fy.y;
      yaw = atan2f(d.x, d.z);
      actor_play(s, "Jog_Fwd_Loop", 1.2f, true, .15f); actor_hands_up(s, 0);
    }
  } else {
    if (near >= 0 && nd < 3.4f && T->sam_t > .9f && FG.phase == PH_FIGHT) {
      /* away from the nearest, toward the keyholder's side, inside the ring */
      v3 away = flat(v3_sub(p, FG.foe[near].pos)); away = v3_len(away) > .01f ? v3_norm(away) : v3_(1, 0, 0);
      v3 to = v3_add(p, v3_add(v3_scale(away, 4.f), v3_scale(v3_norm(flat(v3_sub(FG.P.pos, p))), 1.5f)));
      v3 cd = flat(v3_sub(to, FG.def.center)); float R = (FG.def.radius > 0 ? FG.def.radius : 9) * .7f;
      if (v3_len(cd) > R) to = v3_add(FG.def.center, v3_scale(v3_norm(cd), R));
      T->sam_to = to; T->sam_st = 1; T->sam_t = 0;
    } else {
      actor_play(s, FG.phase == PH_FIGHT ? "Crouch_Idle_Loop" : "Idle_Loop", 1, true, .25f);
      actor_hands_up(s, FG.phase == PH_FIGHT ? 1 : 0);
      if (near >= 0) yaw = yaw_to(p, FG.foe[near].pos);
    }
  }
  if (T->obj_flash > 0) { float k = 1 + T->obj_flash * 2; actor_flash(s, k, k * .8f, k * .8f); } else actor_flash(s, 1, 1, 1);
  actor_look(s, chest(near >= 0 ? &FG.foe[near] : &FG.P), .8f);
  actor_move(s, p, yaw);
  actor_update(s, dt);
}

static void twist_start(void) {
  twist_t *T = &FG.tw;
  memset(T, 0, sizeof *T);
  memset(style_seen, 0, sizeof style_seen); style_last[0] = style_last[1] = 0;
  T->kind = FG.def.twist > 0 && FG.def.twist < FT_COUNT ? FG.def.twist : FT_NONE;
  props_clear();
  v3 c = FG.def.center; float R = FG.def.radius > 0 ? FG.def.radius : 9;
  bool given = FG.def.obj_pos.x != 0 || FG.def.obj_pos.z != 0;
  T->carrier = -2;
  T->haz_t = 4; T->train_t = 8;
  switch (T->kind) {
    case FT_TUTORIAL: T->step = TUT_MOVE; break;
    case FT_TIMER:
      T->meter_max = FG.def.twist_a > 0 ? FG.def.twist_a : 60;
      T->obj = given ? FG.def.obj_pos : v3_add(c, v3_(R - 2.5f, 0, 0));
      T->obj_hp = T->obj_hpmax = 3;
      T->obj.y = floor_at(T->obj);
      PROP[PR_OBJ] = build_estop(); prop_at(PR_OBJ, T->obj, yaw_to(T->obj, c));
      PROP[PR_GANTRY] = build_gantry(); prop_at(PR_GANTRY, v3_(c.x, floor_at(c), c.z), 0);
      T->sweep = ds_rr(-3, 3); T->sweep_on = 1;
      break;
    case FT_RETRIEVE: {
      T->meter_max = FG.def.twist_a > 0 ? FG.def.twist_a : 55;
      int ci = 0; for (int i = 0; i < FG.nfoe; i++) if (FG.foe[i].d.ai == AI_SKIRMISH) { ci = i; break; }
      if (FG.nfoe > 0) { T->carrier = ci; FG.foe[ci].goal = true; }
      PROP[PR_BIKE] = build_bike();
      PROP[PR_TRAIN] = build_train(); prop_show(PR_TRAIN, false);
      break; }
    case FT_SCORE:
      T->target = FG.def.twist_a > 0 ? FG.def.twist_a : 30000;
      T->meter_max = FG.def.twist_b > 0 ? FG.def.twist_b : 99;
      break;
    case FT_HOLD:
      T->meter_max = 1;
      T->obj = given ? FG.def.obj_pos : v3_add(c, v3_(0, 0, -(R - 1.4f)));
      T->obj.y = floor_at(T->obj);
      PROP[PR_OBJ] = build_fridge(); prop_at(PR_OBJ, T->obj, yaw_to(T->obj, c));
      PROP[PR_DOOR] = build_door();
      T->nfrost = 4;
      for (int i = 0; i < 4; i++) { float a = i * 1.571f + .6f, r = R * ds_rr(.35f, .6f); T->frost[i] = v3_add(c, v3_(sinf(a) * r, 0, cosf(a) * r)); T->frost_r[i] = ds_rr(1.5f, 2.2f); }
      T->step_t = 9;   /* the foe's first trip to the door */
      break;
    case FT_DEFEND:
      T->obj_hp = T->obj_hpmax = FG.def.twist_a > 0 ? FG.def.twist_a : 70;
      T->obj = given ? FG.def.obj_pos : v3_add(c, v3_(0, 0, -(R * .55f)));
      T->obj.y = floor_at(T->obj);
      PROP[PR_OBJ] = FG.def.obj_kind == 1 ? build_weld_table() : FG.def.obj_kind == 2 ? build_fixit_bins() : build_cage(); prop_at(PR_OBJ, T->obj, yaw_to(T->obj, c));
      for (int i = 0; i < FG.nfoe; i++) if (FG.foe[i].d.ai != AI_BRUISER && FG.foe[i].d.ai != AI_BOSS) FG.foe[i].goal = true;
      T->step_t = 8;
      break;
    case FT_PROTECT:
      T->obj_hp = T->obj_hpmax = FG.def.twist_a > 0 ? FG.def.twist_a : 60;
      if (FG.def.protect_actor) {
        actor_t *s = FG.def.protect_actor;
        v3 dd = v3_norm(flat(v3_sub(c, FG.P.pos)));
        T->obj = given ? FG.def.obj_pos : v3_add(FG.P.pos, v3_add(v3_scale(v3_(dd.z, 0, -dd.x), 2.2f), v3_scale(dd, 1.2f)));
        T->obj.y = floor_at(T->obj);
        actor_visible(s, true); actor_place(s, T->obj, yaw_to(T->obj, c));
        actor_play(s, "Crouch_Idle_Loop", 1, true, .2f); actor_hands_up(s, 1);
      }
      for (int i = 0; i < FG.nfoe; i++) if (FG.foe[i].d.ai != AI_BOSS) FG.foe[i].goal = true;
      T->sam_t = 1;
      break;
    case FT_BOSS: break;
    default: break;
  }
  if (T->nfrost) PROP[PR_FROST] = build_frost();
}
static void frost_add_arena(void) {
  /* the fridge's arena frosts over in a boss's second phase */
  twist_t *T = &FG.tw;
  if (T->nfrost) return;
  v3 c = FG.def.center; float R = FG.def.radius > 0 ? FG.def.radius : 9;
  T->nfrost = 5;
  for (int i = 0; i < 5; i++) { float a = i * 1.257f + .3f, r = R * ds_rr(.3f, .65f); T->frost[i] = v3_add(c, v3_(sinf(a) * r, 0, cosf(a) * r)); T->frost_r[i] = ds_rr(1.4f, 2.3f); }
  PROP[PR_FROST] = build_frost();
}

static int boss_phase_now(void) { int p = 0; for (int i = 0; i < FG.nfoe; i++) if (FG.foe[i].d.ai == AI_BOSS && FG.foe[i].phase > p) p = FG.foe[i].phase; return p; }
static void boss_phase_enter(fighter_t *f, int phase) {
  (void)f;
  if (phase == 1) {
    if (FG.def.phase2_at > 0) { world_arena_phase(1); FG.checkpoint = 1; FG.tw.haz_t = 2.5f; }
    bark(FB_PHASE2);
    if (world_get_stage() == STAGE_ARENA_FRIDGE && FG.def.phase2_at > 0) frost_add_arena();
  } else if (phase == 2 && tw_is(FT_BOSS)) bark(FB_OBJ_WARN);
}

static void twist_update(float dt) {
  twist_t *T = &FG.tw;
  if (T->obj_flash > 0) T->obj_flash = fmaxf(0, T->obj_flash - dt * 4);
  if (T->warn_t > 0) T->warn_t -= dt;
  bool fight = FG.phase == PH_FIGHT && FG.fin_t < 0;
  /* frost under the feet */
  for (int i = 0; i < 1 + FG.nfoe + (FG.ally_on ? 1 : 0); i++) {
    fighter_t *f = i <= FG.nfoe ? each_fighter(i) : &FG.ally;
    float ice = 0;
    for (int k = 0; k < T->nfrost; k++) if (dist2d(f->pos, T->frost[k]) < T->frost_r[k]) ice = 1;
    f->ice = ice;
  }
  /* the boss's arena, turned */
  int bp = boss_phase_now();
  if (fight && ((bp >= 1 && FG.def.phase2_at > 0) || mod(MOD_HAZARD_PLUS))) strikes_update(dt, bp);
  if (fight && bp >= 1 && FG.def.phase2_at > 0 && world_get_stage() == STAGE_ARENA_LASER) { T->sweep_on = fminf(1, T->sweep_on + dt); sweep_update(dt, .55f); }
  switch (T->kind) {
    case FT_TUTORIAL: {
      fighter_t *P = &FG.P;
      bool calm0 = T->step < TUT_DODGE || T->step == TUT_SPECIAL;
      if (calm0) for (int i = 0; i < FG.nfoe; i++) FG.foe[i].atk_cd = fmaxf(FG.foe[i].atk_cd, .6f);
      if (!fight) break;
      T->step_t += dt;
      if (T->step == TUT_MOVE && P->st == F_FREE && v3_len(flat(P->vel)) > 1.f) T->step_n += dt;
      float step_cap = T->step == TUT_PARRY ? 6 : 18;   /* parry is optional: don't make a mouse player wait it out */
      if (T->step < TUT_FREE && (T->step_n >= TUT_NEED[T->step] || T->step_t > step_cap)) tut_next();
      bool calm = T->step < TUT_DODGE || T->step == TUT_SPECIAL;
      /* the lessons that need a swing to meet get one every couple of seconds */
      for (int i = 0; i < FG.nfoe; i++) FG.foe[i].atk_cd = calm ? fmaxf(FG.foe[i].atk_cd, .6f) : fminf(FG.foe[i].atk_cd, 1.6f);
      if (T->step == TUT_SPECIAL) FG.meter = fmaxf(FG.meter, 100);
      break; }
    case FT_TIMER: {
      float k = T->meter / T->meter_max;
      if (fight && !T->obj_done) T->meter += dt;
      if (T->obj_done) T->meter = fmaxf(0, T->meter - dt * 4);
      if (!T->obj_done && fight) { T->sweep_on = fminf(1, T->sweep_on + dt * 2); sweep_update(dt, .5f + k * .35f); }
      else if (T->obj_done) T->sweep_on = fmaxf(0, T->sweep_on - dt * 2);
      if (T->sweep_on > 0 && T->obj_done) sweep_update(0, 0);
      /* the haze: green puffs from the laser's bed, thicker as it fills */
      if (k > .05f && (int)(FG.clock * 8) != (int)((FG.clock - dt) * 8)) {
        float R = FG.def.radius > 0 ? FG.def.radius : 9;
        v3 p = v3_add(FG.def.center, v3_(ds_rr(-R, R) * .8f, 0, ds_rr(-R, R) * .8f)); p.y = floor_at(p) + .3f;
        fx_smoke(p, .8f + k * 1.4f, 0x6aa040, 1 + (int)(k * 3), .4f);
        if (!T->obj_done && (int)(FG.clock * 2) % 6 == 0) sfx(SFX_HISS, 0, .2f + k * .3f);
      }
      if (fight && k > .6f && !T->obj_done) { FG.P.hp = fmaxf(1, FG.P.hp - dt * 1.4f); if (T->warn_t <= 0) { twist_warn("GAS RISING", 2); bark(FB_OBJ_WARN); } }
      if (fight && T->meter >= T->meter_max) twist_end(false, "THE AIR WENT GREEN");
      if (!T->obj_done) { v3 b = v3_add(T->obj, v3_(0, 1.25f, 0)); fx_glow(b, .45f + .25f * sinf(FG.clock * 8), 0xff3020); }
      break; }
    case FT_RETRIEVE: {
      if (fight) train_update(dt);
      if (T->carrier >= 0) {
        fighter_t *c = &FG.foe[T->carrier];
        bool loose = c->st == F_KNOCK || c->st == F_STAGGER || c->st == F_KO || c->st == F_KNEEL || T->carry_hits >= 4;
        if (loose) {
          T->obj = v3_add(c->pos, v3_scale(facing(c), .7f)); T->obj.y = floor_at(T->obj);
          T->carrier = -1; T->carry_hits = 0; T->take_t = 0; T->loose_t = 0;
          banner("KNOCKED LOOSE", .9f); sound(SFX_CLASH, T->obj, 1); sound(SFX_BODY_FALL, T->obj, .6f);
          fx_dust(T->obj, .8f, 5);
          for (int i = 0; i < FG.nfoe; i++) { FG.foe[i].goal = true; FG.foe[i].aggro = 0; }   /* everyone goes for it */
          c->aggro = 1.5f;
        } else {
          v3 at = v3_add(c->pos, v3_add(v3_scale(facing(c), .4f), v3_(0, .95f, 0)));
          prop_at(PR_BIKE, at, c->yaw);   /* held up across the body: reads from any side */
          if (fight) {
            float edge = dist2d(c->pos, FG.def.center) > (FG.def.radius > 0 ? FG.def.radius : 9) - 2.5f ? 1.4f : 1;
            T->meter += dt * edge;
            if (T->meter > T->meter_max * .65f && T->warn_t <= 0) { twist_warn("THEY ARE GETTING AWAY", 2.5f); bark(FB_OBJ_WARN); }
            if (T->meter >= T->meter_max) twist_end(false, "THE BIKE GOT AWAY");
          }
        }
      }
      if (T->carrier == -1) {
        prop_at(PR_BIKE, T->obj, 0);
        fx_ring_flat(v3_add(T->obj, v3_(0, .04f, 0)), 1.1f, C_HIVIS);
        if (fight) {
          if (dist2d(FG.P.pos, T->obj) < 1.15f && FG.P.st != F_KO) {
            T->take_t += dt;
            if (T->take_t >= 1.5f) { T->carrier = -2; sfx(SFX_PICKUP, 0, 1); fx_burst(v3_add(T->obj, v3_(0, .6f, 0)), C_HIVIS, .6f); twist_end(true, "BIKE RECOVERED"); }
          } else T->take_t = fmaxf(0, T->take_t - dt * 2);
          /* a foe standing on it takes it again */
          int on = -1;
          for (int i = 0; i < FG.nfoe; i++) if (alive_foe(&FG.foe[i]) && (FG.foe[i].st == F_FREE) && dist2d(FG.foe[i].pos, T->obj) < .95f) on = i;
          if (on >= 0) { T->loose_t += dt; if (T->loose_t > 1.2f) { T->carrier = on; for (int i = 0; i < FG.nfoe; i++) FG.foe[i].goal = i == on; FG.foe[on].aggro = 0; T->loose_t = 0; banner("THEY HAVE IT AGAIN", .9f); sound(SFX_PICKUP, T->obj, .7f); } }
          else T->loose_t = 0;
        }
      }
      if (T->carrier == -2 && PROP[PR_BIKE]) prop_at(PR_BIKE, v3_add(FG.P.pos, v3_scale(facing(&FG.P), .9f)), FG.P.yaw);
      break; }
    case FT_SCORE: {
      if (fight) {
        T->meter += dt;
        T->style = fmaxf(0, T->style - dt * (T->style > 60 ? 7 : 4.5f));
        int rank = T->style >= 80 ? 4 : T->style >= 60 ? 3 : T->style >= 40 ? 2 : T->style >= 20 ? 1 : 0;
        T->ranks = rank;
        if (T->score >= T->target) twist_end(true, "NEW HIGH SCORE");
        else if (T->meter >= T->meter_max) twist_end(false, "TIME OVER");
        else if (T->meter > T->meter_max - 15 && T->warn_t <= 0) { twist_warn("15 SECONDS", 3); bark(FB_OBJ_WARN); }
      }
      T->shown = T->shown < T->score ? fminf(T->score, T->shown + fmaxf(60, (T->score - T->shown) * 8) * dt) : T->score;
      break; }
    case FT_HOLD: {
      if (fight) {
        float rate = (FG.def.twist_a > 0 ? FG.def.twist_a : 1) * (1.f / 30) * (1 + T->meter * .8f);
        T->meter = fminf(1, T->meter + dt * rate);
        /* now and then the foe goes to pry it */
        T->step_t -= dt;
        fighter_t *o = FG.nfoe ? &FG.foe[0] : NULL;
        if (o && alive_foe(o)) {
          if (!o->goal && T->step_t <= 0) { o->goal = true; o->aggro = 0; T->step_t = 7; }
          else if (o->goal && (T->step_t <= 0 || o->aggro > 0)) { o->goal = false; o->aggro = 0; T->step_t = ds_rr(10, 15); }
        }
        if (T->meter > .7f && T->warn_t <= 0) { twist_warn("THE DOOR IS OPENING", 2.5f); bark(FB_OBJ_WARN); }
        if (T->meter >= 1) twist_end(false, "THE FRIDGE GOT OUT");
      }
      /* the door, the cold coming out */
      float ang = T->meter * 1.7f;
      v3 c = FG.def.center; float y0 = yaw_to(T->obj, c);
      v3 right = v3_(cosf(y0), 0, -sinf(y0)), fwd = v3_(sinf(y0), 0, cosf(y0));
      v3 hinge = v3_add(T->obj, v3_add(v3_scale(right, -.72f), v3_scale(fwd, .42f)));
      if (PROP[PR_DOOR]) { t3_object_set_position(PROP[PR_DOOR], hinge.x, hinge.y, hinge.z); t3_object_set_rotation(PROP[PR_DOOR], 0, y0 - ang, 0); }
      if (T->meter > .15f) {
        v3 gap = v3_add(T->obj, v3_add(v3_scale(fwd, .6f), v3_(0, 1, 0)));
        fx_glow(gap, .8f + T->meter * 1.5f, 0x80c0ff);
        if ((int)(FG.clock * 6) != (int)((FG.clock - dt) * 6)) fx_smoke(v3_sub(gap, v3_(0, .7f, 0)), .6f + T->meter, 0xd8f0ff, 1 + (int)(T->meter * 3), .1f);
        if (T->meter > .6f && (int)(FG.clock * 3) != (int)((FG.clock - dt) * 3)) fx_motes(gap, 0x9fe060, 2, 1);   /* something in there is alive */
      }
      break; }
    case FT_DEFEND: {
      if (fight) {
        /* one foe alone splits its time */
        if (FG.nfoe == 1 && alive_foe(&FG.foe[0])) {
          T->step_t -= dt;
          if (T->step_t <= 0) { FG.foe[0].goal = !FG.foe[0].goal; T->step_t = FG.foe[0].goal ? 7 : 11; }
        }
        if (T->obj_hp < T->obj_hpmax * .35f && T->warn_t <= 0) { twist_warn(FG.def.obj_kind == 1 ? "THE TABLE IS GOING" : "THE BINS ARE GOING", 2.5f); bark(FB_OBJ_WARN); }
        if (T->obj_hp <= 0) { T->obj_hp = 0; fx_burst(v3_add(T->obj, v3_(0, 1, 0)), 0xd88040, 1.2f); twist_end(false, FG.def.obj_kind == 2 ? "THE FIX-IT BINS WALKED OFF" : "THE SCRAP WALKED OFF"); }
      }
      break; }
    case FT_PROTECT: {
      sam_update(dt);
      if (fight) {
        if (T->obj_hp < T->obj_hpmax * .4f && T->warn_t <= 0) { twist_warn("SAM IS CORNERED", 2.5f); bark(FB_OBJ_WARN); }
        if (T->obj_hp <= 0) { T->obj_hp = 0; twist_end(false, "SAM WAS CORNERED"); }
      }
      break; }
    default: break;
  }
}

/* ---- barks: one line at a time, never twice ---- */
static void bark_play(int i) {
  const fight_bark *b = &FG.def.barks[i];
  FG.bark_said |= 1u << i; FG.bark_cur = i; FG.bark_pend = -1;
  bool said = b->speaker && voice_say(b->speaker, b->text);
  float len = said ? voice_seconds(b->speaker, b->text) : 0;
  FG.bark_len = FG.bark_t = (len > 0 ? len : 1.f + strlen(b->text ? b->text : "") * .055f) + .5f;
#ifdef KH_TEST_FIGHT
  ds_log("bark %d %s: %s", b->on, b->name ? b->name : "", b->text ? b->text : "");
#endif
}
static void bark(int on) {
  for (int i = 0; i < FG.def.nbarks && i < 32; i++) {
    if (FG.def.barks[i].on != on || (FG.bark_said & (1u << i))) continue;
    if (FG.bark_t > 0 || voice_busy()) {
      /* someone is talking: hold it a moment, the important ones first */
      bool keen = on == FB_START || on == FB_PHASE2 || on == FB_FINISH || on == FB_ASSIST;
      if (FG.bark_pend < 0 || keen) { FG.bark_pend = i; FG.bark_pend_t = keen ? 4 : 2; }
      return;
    }
    bark_play(i);
    return;
  }
}
static void bark_update(float rdt) {
  if (FG.bark_t > 0) { FG.bark_t -= rdt; if (FG.bark_t <= 0) FG.bark_cur = -1; }
  if (FG.bark_pend >= 0) {
    FG.bark_pend_t -= rdt;
    if (FG.bark_pend_t <= 0) FG.bark_pend = -1;
    else if (FG.bark_t <= 0 && !voice_busy() && !(FG.bark_said & (1u << FG.bark_pend))) bark_play(FG.bark_pend);
  }
  if (FG.phase != PH_FIGHT) return;
  /* the low ones */
  if (FG.P.hp < FG.P.hpmax * .3f) bark(FB_PLAYER_LOW);
  for (int i = 0; i < FG.nfoe; i++) { fighter_t *o = &FG.foe[i]; if (alive_foe(o) && o->hp < o->hpmax * .3f && (o->d.ai == AI_BOSS || FG.nfoe == 1 || i == 0)) bark(FB_FOE_LOW); }
}

/* ---- assists: an ally steps in for a few seconds ---- */
#define ALLY_STAY 8.f
#define ALLY_COOL 22.f
static int ally_target(const fighter_t *f) {
  int best = -1; float bd = 1e9f;
  for (int i = 0; i < FG.nfoe; i++) { const fighter_t *o = &FG.foe[i]; if (!alive_foe(o)) continue; float d = dist2d(o->pos, f->pos) + (o->goal ? -2.f : 0); if (d < bd) { bd = d; best = i; } }
  return best;
}
static void ally_call(void) {
  fighter_t *P = &FG.P, *A = &FG.ally;
  int n = FG.def.nassist; if (n <= 0) return;
  int i = FG.ally_next % n; FG.ally_next++;
  const fighter_def *d = &FG.def.assist[i];
  memset(A, 0, sizeof *A);
  A->d = *d;
  if (d->actor) { A->a = d->actor; A->story = true; FG.ally_vis = actor_is_visible(A->a); FG.ally_was = actor_pos(A->a); FG.ally_was_yaw = actor_yaw(A->a); }
  else { A->a = pool_take(d->model, &d->pal, d->scale > 0 ? d->scale : 1); A->pooled = true; enemy_dress(A->a, d->dress); }
  v3 f = facing(P), s = v3_(f.z, 0, -f.x);
  A->pos = v3_add(P->pos, v3_add(v3_scale(s, 1.4f), v3_scale(f, -.5f)));
  A->pos.y = floor_at(A->pos);
  actor_visible(A->a, true); actor_place(A->a, A->pos, P->yaw);
  fighter_setup(A, false);
  A->ally = true; A->yaw = P->yaw;
  A->hp = A->hpmax = 999; A->power = d->power > 0 ? d->power : 1; A->speed = d->speed > 0 ? d->speed : 1; A->skill = .8f;
  A->wp = d->weapon >= 0 && d->weapon < WP_COUNT ? d->weapon : WP_ARC;
  weapon_equip(A->a, A->wp); weapon_draw_flash(A->a);
  A->inv = 999; A->charge = 2.4f;   /* the entrance: one big blow */
  FG.ally_on = true; FG.ally_t = 0; FG.ally_i = i; FG.ally_flash = 1;
  fx_burst(chest(A), weapon_color(A->wp), 1.1f); fx_shock(v3_add(A->pos, v3_(0, .05f, 0)), .2f, 3, weapon_color(A->wp), .4f);
  fx_streak(v3_add(A->pos, v3_(0, 6, 0)), chest(A), 0xffffff, .3f, .3f);
  sfx(SFX_ASSIST, 0, 1); FG.screen_flash = .3f; FG.screen_col = weapon_color(A->wp);
  banner(d->name ? d->name : "ASSIST", 1.f);
  style_add(1200, SK_ALLY);
  bark(FB_ASSIST);
  int t = ally_target(A);
  if (t >= 0) attack_start(A, A->wp == WP_MAUL ? MV_SLAM : A->wp == WP_SPEAR ? MV_SPIN : MV_DASH, &FG.foe[t], 0);
}
static void ally_leave(void) {
  fighter_t *A = &FG.ally;
  if (!FG.ally_on) return;
  fx_burst(chest(A), weapon_color(A->wp), .8f); sfx(SFX_SUMMON, 0, .6f);
  attack_end(A); weapon_equip(A->a, -1); actor_flash(A->a, 1, 1, 1);
  if (A->story) {
    actor_place(A->a, FG.ally_was, FG.ally_was_yaw); actor_visible(A->a, FG.ally_vis);
    actor_play(A->a, "Idle_Loop", 1, true, 0);
  } else { enemy_undress(A->a); actor_visible(A->a, false); }
  FG.ally_on = false; FG.ally_cd = ALLY_COOL;
}
static void ally_think(fighter_t *f, float dt) {
  f->atk_cd -= dt;
  if (f->st != F_FREE) return;
  f->charge = 1;
  int t = ally_target(f);
  if (t < 0 || FG.phase != PH_FIGHT) { f->vel.x *= .8f; f->vel.z *= .8f; play(f, idle_clip(f), 1, true, .2f); return; }
  fighter_t *T = &FG.foe[t];
  float dist = dist2d(f->pos, T->pos), reach = WP[f->wp].reach + .2f;
  v3 dir = v3_norm(flat(v3_sub(T->pos, f->pos)));
  if (f->wp == WP_STAFF && dist < 8) {
    turn_to(f, T->pos, 12, dt); f->vel.x *= .8f; f->vel.z *= .8f;
    if (f->atk_cd <= 0) { replay(f, "Spell_Simple_Shoot", 1.5f, false, .05f); bolt_cast(f, T, -1); f->atk_cd = .7f; }
    return;
  }
  if (dist > reach) {
    f->vel.x = ds_damp(f->vel.x, dir.x * 4.6f, 10, dt); f->vel.z = ds_damp(f->vel.z, dir.z * 4.6f, 10, dt);
    f->yaw = atan2f(dir.x, dir.z);
    play(f, "Jog_Fwd_Loop", 1.3f, true, .15f);
  } else if (f->atk_cd <= 0) {
    const weapon_t *W = &WP[f->wp];
    int mv = f->chain_l % 4 == 3 ? W->heavy[0] : W->light[f->chain_l % W->nlight];
    f->chain_l++;
    attack_start(f, mv, T, 0);
    f->atk_cd = .15f;
  } else { turn_to(f, T->pos, 12, dt); f->vel.x *= .8f; f->vel.z *= .8f; }
}

/* ---- the finisher: the last blow on a boss becomes a prompt ---- */
static bool fin_try(fighter_t *def) {
  if (!FG.def.finisher_qte || FG.fin_done || FG.fin_t >= 0 || FG.cine_t >= 0 || FG.phase != PH_FIGHT) return false;
  if (foes_up() > 1) return false;
  def->hp = 1;
  FG.fin_t = 0; FG.fin_target = (int)(def - FG.foe);
  attack_end(def); release_token(def);
  set_st(def, F_STAGGER); def->stun = 99; def->inv = 99; def->armor = 0;
  replay(def, "Idle_Shield_Break", .5f, false, .06f);
  attack_end(&FG.P);
  ally_leave();
  for (int i = 0; i < MAX_BOLTS; i++) BOLT[i].on = false;
  sfx(SFX_QTE, 0, 1); FG.screen_flash = .35f; FG.screen_col = 0xffffff; shake(.4f);
  return true;
}
static void fin_update(float rdt, const intent_t *in) {
  if (FG.fin_t < 0) return;
  FG.fin_t += rdt;
  fighter_t *T = &FG.foe[FG.fin_target];
  if (FG.fin_t > .3f && FG.fin_t < 1.9f && (in->light || in->heavy || in->special)) {
    FG.fin_t = -1; FG.fin_done = true; FG.fin_ok = true;
    T->inv = 0; T->stun = 0;
    FG.cine_fin = true; special_start(FG.fin_target);
    FG.cine_name = "FINISH";
    return;
  }
  if (FG.fin_t >= 1.9f) {
    /* missed: it still ends, just plainer */
    FG.fin_t = -1; FG.fin_done = true;
    T->inv = 0; T->hp = 0;
    fx_burst(chest(T), weapon_color(FG.P.wp), .6f); sound(SFX_HIT_HEAVY, T->pos, 1);
    foe_ko(T);
  }
}

/* ---- the contract ---- */
void fight_init(t3_scene *scene, t3_camera *cam) {
  SC = scene; CAM = cam;
  fx_init(scene); weapon_init(scene);
  fight_hud_init();
  FG.cine_t = -1;
}

static void fighter_setup(fighter_t *f, bool player) {
  f->player = player;
  f->yaw = actor_yaw(f->a); f->grounded = true;
  f->guard_hp = 30; f->mv = -1; f->clip = NULL;
  f->flash = 0; f->freeze = 0; f->inv = 0; f->armor = 0;
  set_st(f, F_FREE);
  actor_visible(f->a, true);
  actor_expr_clear(f->a); actor_hands_up(f->a, 0);
}

void fight_start(const fight_def *def, actor_t *player) {
  memset(&FG, 0, sizeof FG);
  FG.def = *def; FG.cine_t = -1; FG.lock = -1;
  /* the town's light rail would run through the ring and fill the frame; the bike fight has its own */
  world_train_hold(world_get_stage() == STAGE_TOWN);
  if (!FG.def.weapons) FG.def.weapons = 1u << WP_ARC;
  memset(POOL_USED, 0, sizeof POOL_USED);
  memset(BOLT, 0, sizeof BOLT);
  fx_clear();

  fighter_t *P = &FG.P;
  P->a = player; P->pos = actor_pos(player);
  fighter_setup(P, true);
  P->hpmax = 100; P->hp = fmaxf(1, 100 * (def->player_hp > 0 ? def->player_hp : 1));
  P->ghost = P->hp; P->power = 1; P->speed = 1;
  if (mod(MOD_ONE_WEAPON)) { int sw = def->start_weapon >= 0 && def->start_weapon < WP_COUNT && (FG.def.weapons & (1u << def->start_weapon)) ? def->start_weapon : WP_ARC; FG.def.weapons = 1u << sw; }
  int w = def->start_weapon;
  if (w < 0 || w >= WP_COUNT || !(FG.def.weapons & (1u << w))) { w = WP_ARC; for (int i = 0; i < WP_COUNT; i++) if (FG.def.weapons & (1u << i)) { w = i; break; } }
  P->wp = w;
  weapon_equip(player, -1);
  play(P, idle_clip(P), 1, true, .25f);

  float scale = (def->foe_scale > 0 ? def->foe_scale : 1) * (def->easy ? .8f : 1);
  FG.nfoe = def->count > 4 ? 4 : def->count;
  for (int i = 0; i < FG.nfoe; i++) {
    fighter_t *f = &FG.foe[i];
    f->d = def->foes[i];
    if (f->d.actor) { f->a = f->d.actor; f->story = true; f->pos = actor_pos(f->a); }
    else {
      f->a = pool_take(f->d.model, &f->d.pal, f->d.scale > 0 ? f->d.scale : 1); f->pooled = true;
      enemy_dress(f->a, f->d.dress);   /* the props, the stiff steps, the size; fight_end takes them off */
      f->pos = v3_add(def->center, v3_(f->d.place_x, 0, f->d.place_z));
      float fy = world_floor_y(v3_(f->pos.x, def->center.y + 1.5f, f->pos.z));
      if (fy > -1e8f) f->pos.y = fy;
      actor_visible(f->a, true);
      actor_place(f->a, f->pos, atan2f(P->pos.x - f->pos.x, P->pos.z - f->pos.z));
    }
    fighter_setup(f, false);
    f->yaw = atan2f(P->pos.x - f->pos.x, P->pos.z - f->pos.z);
    f->hp = f->hpmax = f->ghost = f->d.hp * scale * (mod(MOD_HEAVY) ? 1.5f : 1);
    f->power = f->d.power * scale * (mod(MOD_HEAVY) ? 1.15f : 1);
    f->speed = f->d.speed > 0 ? f->d.speed : 1;
    /* skill reads off speed and power: a slow weak foe never guards, a fast strong one reads you */
    f->skill = ds_clamp((f->speed * .6f + f->power * .4f - .85f) / .55f, 0, 1);
    f->wp = f->d.ai == AI_CASTER ? WP_STAFF : f->d.weapon;
    if (weapon_of(f->a) != f->wp) weapon_equip(f->a, -1);
    play(f, idle_clip(f), 1, true, .25f);
    f->atk_cd = ds_rr(.5f, 1.6f) + i * .9f;
    f->circ = ds_rr(1, 2); f->ai_mode = i & 1 ? 1 : -1;
    actor_look(f->a, chest(P), .9f);
  }
  actor_look(player, FG.nfoe ? chest(&FG.foe[0]) : chest(P), .8f);

  /* the duel ring: outdoors a bright circle on the ground; arenas have walls of their own */
  if (RING) { t3_object_remove(&SC->base, RING); RING = NULL; }
  if (def->radius > 0 && world_get_stage() == STAGE_TOWN && !world_in_lab(def->center)) {
    v3 c = def->center;
    float fy = world_floor_y(v3_(c.x, c.y + 1.5f, c.z));
    if (fy > -1e8f) c.y = fy;
    ring_build(c, def->radius);
  }

  FG.phase = PH_INTRO; FG.phase_t = 0;
  FG.lock = FG.nfoe > 0 ? 0 : -1;
  v3 d = FG.nfoe ? flat(v3_sub(P->pos, FG.foe[0].pos)) : v3_(0, 0, 1);
  d = v3_len(d) > .01f ? v3_norm(d) : v3_(0, 0, 1);
  cam_yaw = atan2f(-d.x, -d.z);
  v3 s = v3_(d.z, 0, -d.x);
  FG.cam_pos = v3_add(P->pos, v3_add(v3_scale(d, 1.8f), v3_add(v3_scale(s, 1.5f), v3_(0, .9f, 0))));
  FG.cam_look = v3_add(FG.nfoe ? FG.foe[0].pos : P->pos, v3_(0, 1.1f, 0));
  fov_now = 46;
  bool boss = false; for (int i = 0; i < FG.nfoe; i++) if (FG.foe[i].d.ai == AI_BOSS) boss = true;
  music_mood(boss ? MOOD_BOSS : MOOD_FIGHT);
  /* v1.1 */
  FG.fin_t = -1; FG.bark_cur = FG.bark_pend = -1; FG.mod_rate = mod(MOD_TURBO) ? 1.25f : 1;
  twist_start();
  /* a checkpoint: the boss starts in its second phase, the arena already turned, and the intro is short */
  if (def->start_phase > 0) {
    float p1 = def->phase2_at > 0 ? def->phase2_at : .66f;
    for (int i = 0; i < FG.nfoe; i++) { fighter_t *f = &FG.foe[i]; if (f->d.ai != AI_BOSS) continue; f->phase = 1; f->hp = f->ghost = f->hpmax * p1; f->speed *= 1.1f; }
    if (def->phase2_at > 0) { world_arena_phase(1); FG.checkpoint = 1; if (world_get_stage() == STAGE_ARENA_FRIDGE) frost_add_arena(); }
    FG.quick = true;
  } else if (def->phase2_at > 0) world_arena_phase(0);
  fight_hud_reset();
}

/* the intro: a whip pan to them, a low hero shot of the foe, a smash cut to the card, back to the player.
 * A retry from a checkpoint skips to the last part. Times are in fight_int.h's terms: the HUD reads them too. */
static void intro_update(float rdt) {
  fighter_t *P = &FG.P;
  float t0 = FG.phase_t; FG.phase_t += rdt; float t = FG.phase_t;
  float foes_at = FG.quick ? .1f : INTRO_HERO + .1f, me_at = FG.quick ? .05f : INTRO_BACK + .08f, end = FG.quick ? .8f : INTRO_END;
  if (!FG.quick && t0 < .02f) sfx(SFX_DODGE, 0, 1);   /* the whip's whoosh */
  if (!FG.quick && t0 < INTRO_CARD && t >= INTRO_CARD) sfx(SFX_SMASH, 0, 1);
  if (!FG.quick && t0 < INTRO_BACK && t >= INTRO_BACK) sfx(SFX_HIT_HEAVY, 0, .7f);
  if (t0 < me_at && t >= me_at) { weapon_equip(P->a, P->wp); weapon_draw_flash(P->a); sfx(SFX_SUMMON, 0, .9f); }
  for (int i = 0; i < FG.nfoe; i++) {
    float at = foes_at + i * .12f;
    fighter_t *f = &FG.foe[i];
    if (t0 < at && t >= at) {
      if (weapon_of(f->a) != f->wp) weapon_equip(f->a, f->wp);
      weapon_draw_flash(f->a); sound(SFX_SUMMON, f->pos, .7f);
    }
  }
  if (t >= end) { FG.phase = PH_FIGHT; FG.phase_t = 0; sfx(SFX_UI_OK, 0, .6f); bark(FB_START); }
}

fight_state fight_update(float rdt, const intent_t *in) {
  static const intent_t none = {0};
  const intent_t *inp = in ? in : &none;
  if (FG.mod_rate > 0) rdt *= FG.mod_rate;
  FG.clock += rdt;
  if (FG.phase == PH_FIGHT) FG.time_fight += rdt;

  /* the world's clock */
  float rate = 1;
  if (FG.slow_t > 0) { FG.slow_t -= rdt; rate = FG.slow_k; if (FG.slow_t < .12f) rate = ds_lerp(FG.slow_k, 1, 1 - FG.slow_t / .12f); }
  float prate = rate;
  if (FG.witch_t > 0) { FG.witch_t -= rdt; prate = fmaxf(rate, .85f); }
  /* the special stops the world for everyone but the player */
  if (FG.cine_t >= 0) { bool after = FG.cine_hits >= spec_hits(FG.P.wp); rate = after ? .32f : .05f; prate = after ? .55f : 1; }
  /* the finisher's prompt: almost still */
  if (FG.fin_t >= 0) { rate = prate = .06f; }

  if (FG.screen_flash > 0) FG.screen_flash = fmaxf(0, FG.screen_flash - rdt * 3.2f);
  if (FG.banner_t > 0) FG.banner_t -= rdt;
  if (FG.weapon_card_t > 0) FG.weapon_card_t -= rdt;
  if (FG.combo_t > 0) { FG.combo_t -= rdt * (rate > .5f ? 1 : .5f); if (FG.combo_t <= 0) FG.combo = 0; }
  if (FG.combo_pop > 0) FG.combo_pop = fmaxf(0, FG.combo_pop - rdt * 5);
  if (FG.ally_flash > 0) FG.ally_flash = fmaxf(0, FG.ally_flash - rdt * 3);

  if (FG.phase == PH_INTRO) intro_update(rdt);
  else if (FG.phase != PH_FIGHT) FG.phase_t += rdt;

  bool live = FG.phase == PH_FIGHT && FG.cine_t < 0 && FG.fin_t < 0;
  if (live) lock_input(inp, rdt);
  if (FG.lock >= 0 && !alive_foe(&FG.foe[FG.lock])) FG.lock = -1;
  fin_update(rdt, inp);
  /* an ally on call */
  if (FG.ally_cd > 0 && !FG.ally_on) FG.ally_cd -= rdt;
  if (live && inp->assist && FG.def.nassist > 0 && !FG.ally_on && FG.ally_cd <= 0) ally_call();

  /* fighters */
  fighter_t *P = &FG.P;
  float pdt = rdt * prate;
  if (P->freeze > 0) { P->freeze -= rdt; pdt = 0; }
  if (pdt > 0) tick(P, -1, pdt, FG.fin_t >= 0 ? &none : inp);
  /* the player's i frames and clock during the special come from the cinematic */
  cine_update(rdt);
  for (int i = 0; i < FG.nfoe; i++) {
    fighter_t *f = &FG.foe[i];
    float fdt = rdt * rate;
    if (f->freeze > 0) { f->freeze -= rdt; fdt = 0; }
    if (fdt > 0) tick(f, i, fdt, NULL);
    boss_watch(f);
  }
  if (FG.ally_on) {
    fighter_t *A = &FG.ally;
    float adt = rdt * rate;
    if (A->freeze > 0) { A->freeze -= rdt; adt = 0; }
    if (adt > 0) tick(A, -2, adt, NULL);
    FG.ally_t += rdt * rate;
    if ((FG.ally_t > ALLY_STAY && A->st != F_ATTACK) || FG.ally_t > ALLY_STAY + 1.5f || (FG.phase != PH_FIGHT && FG.phase_t > .8f)) ally_leave();
  }
  twist_update(rdt * rate);
  end_check();
  separate();
  bolts_update(rdt * rate, true);
  bark_update(rdt);

  /* actors */
  for (int i = -1; i <= FG.nfoe; i++) {
    fighter_t *f = i < 0 ? P : i < FG.nfoe ? &FG.foe[i] : FG.ally_on ? &FG.ally : NULL;
    if (!f) continue;
    float dt_a = f->freeze > 0 ? 0 : rdt * (i < 0 ? prate : rate);
    actor_move(f->a, f->pos, f->yaw);
    if (f->flash > 0) { f->flash = fmaxf(0, f->flash - rdt * 5.5f); float k = 1 + f->flash * 2.2f; actor_flash(f->a, k, k * (f->player ? .9f : .95f), k * .85f); }
    else if (f->ally && FG.ally_flash > 0) { float k = 1 + FG.ally_flash * 1.5f; actor_flash(f->a, k, k, k); }
    else actor_flash(f->a, 1, 1, 1);
    v3 at = i < 0 ? (FG.nfoe ? chest(&FG.foe[FG.lock >= 0 ? FG.lock : 0]) : chest(P)) : f->ally ? (ally_target(f) >= 0 ? chest(&FG.foe[ally_target(f)]) : chest(P)) : chest(P);
    if (f->st != F_ROLL && f->st != F_KNOCK && f->st != F_KO) actor_look(f->a, v3_add(at, v3_(0, .3f, 0)), .85f);
    else actor_look(f->a, at, 0);
    if (dt_a > 0) { if (f->st == F_FREE) { actor_expr_set(f->a, EX_ANGRY, .35f); actor_expr_set(f->a, EX_SORROW, 0); } }
    actor_update(f->a, dt_a);
    /* the ghost bar */
    f->ghost = f->ghost > f->hp ? fmaxf(f->hp, f->ghost - rdt * (f->hpmax * .28f)) : f->hp;
  }
  /* the winner relaxes */
  if (FG.phase == PH_WON && P->st == F_FREE) { actor_expr_set(P->a, EX_ANGRY, 0); }
  weapon_update(fmaxf(rdt * prate, 0));
  cam_update(inp, rdt);
  fx_update(rdt * rate, CAM);

  /* vampire: the blood runs out on its own */
  if (mod(MOD_VAMPIRE) && FG.phase == PH_FIGHT && FG.cine_t < 0) P->hp = fmaxf(1, P->hp - rdt * 1.2f);
  if (FG.phase == PH_WON && FG.phase_t > 2.5f) return FIGHT_WON;
  if (FG.phase == PH_LOST && FG.phase_t > 2.6f) return FIGHT_LOST;
  return FIGHT_RUNNING;
}

float fight_player_hp(void) { return FG.P.hpmax > 0 ? FG.P.hp / FG.P.hpmax : 1; }
actor_t *fight_foe_actor(int i) { return i >= 0 && i < FG.nfoe ? FG.foe[i].a : NULL; }

void fight_end(void) {
  fighter_t *P = &FG.P;
  world_train_hold(false);
  ally_leave(); FG.ally_cd = 0;
  props_clear();
  if (FG.tw.kind == FT_PROTECT && FG.def.protect_actor) { actor_t *s = FG.def.protect_actor; actor_hands_up(s, 0); actor_flash(s, 1, 1, 1); actor_expr_clear(s); actor_play(s, "Idle_Loop", 1, true, .3f); }
  if (P->a) {
    weapon_trail(P->a, false);
    weapon_equip(P->a, -1);
    actor_flash(P->a, 1, 1, 1); actor_expr_clear(P->a); actor_look(P->a, P->pos, 0);
    actor_play(P->a, "Idle_Loop", 1, true, .3f);
    player_teleport(P->pos, P->yaw);
    player_camera_from(FG.cam_pos);
  }
  for (int i = 0; i < FG.nfoe; i++) {
    fighter_t *f = &FG.foe[i];
    if (!f->a) continue;
    weapon_trail(f->a, false);
    actor_flash(f->a, 1, 1, 1);
    if (f->story && !FG.def.battle_zone) {
      /* left as they are: on one knee when beaten; the story takes it from here */
      if (f->st != F_KNEEL) { if (f->st == F_KO) kneel(f); else { weapon_equip(f->a, -1); actor_play(f->a, "Idle_Loop", 1, true, .3f); } }
      continue;
    }
    weapon_equip(f->a, -1);
    if (f->pooled) enemy_undress(f->a);
    actor_hands_up(f->a, 0); actor_visible(f->a, false);
  }
  memset(POOL_USED, 0, sizeof POOL_USED);
  if (RING) { t3_object_remove(&SC->base, RING); RING = NULL; }
  for (int i = 0; i < MAX_BOLTS; i++) BOLT[i].on = false;
  fx_clear();
  FG.nfoe = 0;
}

int fight_phase(void) { int p = FG.def.start_phase; for (int i = 0; i < FG.nfoe; i++) if (FG.foe[i].phase > p) p = FG.foe[i].phase; if (FG.checkpoint > p) p = FG.checkpoint; return p; }
/* the score: a win, quick, clean, varied, with the objective kept; Battle Zone modifiers raise it */
void fight_result_get(fight_result *r) {
  memset(r, 0, sizeof *r);
  bool won = FG.phase == PH_WON;
  r->time = FG.time_fight; r->damage_taken = FG.damage_taken; r->max_combo = FG.max_combo;
  r->parries = FG.parries; r->perfect_dodges = FG.perfect_dodges; r->specials = FG.specials; r->finisher = FG.fin_ok;
  const twist_t *T = &FG.tw;
  r->objective_ok = won && (T->kind == FT_SCORE ? T->score >= T->target : T->kind == FT_TIMER ? true : true);
  float par = 30 + 25 * FG.nfoe + (T->kind == FT_BOSS || FG.def.phase2_at > 0 ? 40 : 0);   /* a fair time for this fight */
  float s = 0;
  if (won) {
    s += 3000;
    s += fmaxf(0, 1500 * (1.5f - FG.time_fight / par));
    s += fmaxf(0, 2000 - FG.damage_taken * 25);
    s += fminf(FG.max_combo, 40) * 30 + FG.parries * 150 + FG.perfect_dodges * 120 + FG.specials * 150 + (FG.fin_ok ? 800 : 0);
    s += __builtin_popcount(FG.moves_used & 0x3ff) * 80;
    if (T->kind == FT_SCORE) s += T->score * .1f;
    if (T->kind == FT_DEFEND || T->kind == FT_PROTECT) s += 1000 * (T->obj_hpmax > 0 ? T->obj_hp / T->obj_hpmax : 0);
    if (T->kind == FT_TIMER || T->kind == FT_HOLD || T->kind == FT_RETRIEVE) s += 1000 * (1 - (T->meter_max > 0 ? T->meter / T->meter_max : 0));
  } else s = 300 + FG.max_combo * 10;
  s *= 1 + .15f * __builtin_popcount(FG.def.mods & 0xff);
  r->score = (int)s;
  r->rank = !won ? 'D' : s >= 8300 ? 'S' : s >= 7200 ? 'A' : s >= 6000 ? 'B' : s >= 4500 ? 'C' : 'D';
}
