/* fight.c: see fight.h. One state machine per fighter, moves as sections of the mocap
 * clips, hit windows read off the clips' hand speed, an AI that takes turns. Everything a
 * fighter does runs on its own local time, so hit stop freezes just the two who touched
 * and slow motion can leave the player quicker than the world. */
#include "fight_int.h"
#include "weapon.h"
#include "player.h"
#include "fx.h"
#include "audio.h"
#include <stdlib.h>
#include <string.h>

fight_state_t FG;
static t3_scene *SC; static t3_camera *CAM;

/* ---- moves: clip seconds ---- */
enum { MF_HEAVY = 1, MF_KNOCK = 2, MF_BREAK = 4, MF_UNBLOCK = 8, MF_SWEEP = 16, MF_WAVE = 32 };
typedef struct {
  const char *clip; float from, to, hit0, hit1, cancel;
  const char *rec;
  float dmg, stun, kb, arc, lunge; int flags;
} move_t;
enum { MV_L1, MV_L2, MV_L3, MV_L4, MV_H1, MV_H2, MV_H3, MV_H4, MV_SA, MV_DASH, MV_C, MV_WAVE, MV_COUNT };
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
  [WP_MAUL]  = { .80f, 9.5f, 1.75f, .12f, {MV_L1, MV_L3}, 2, {MV_SA, MV_H4}, 2, 11.f, 1.4f, 1.25f, SFX_SWING_MAUL, "COLD FORGE HAMMERFALL" },
  [WP_TWIN]  = { 1.5f, 3.1f, 1.2f, .035f, {MV_L1, MV_L2, MV_L4, MV_L1, MV_L2, MV_L3}, 6, {MV_H1, MV_H2}, 2, 4.f, .8f, .8f, SFX_SWING_TWIN, "THOUSAND BURR CUT" },
  [WP_SPEAR] = { 1.f, 6.2f, 2.35f, .065f, {MV_L1, MV_L2, MV_L3}, 3, {MV_DASH, MV_SA}, 2, 7.5f, 1.1f, 1, SFX_SWING_SPEAR, "FIRST LAYER ADHESION" },
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
static void meter_add(float m) { FG.meter = fminf(100, FG.meter + m); }

/* a blow lands. pt is where; dir pushes the victim. */
static void hurt(fighter_t *att, fighter_t *def, float dmg, float stun, float kb, int flags, v3 pt, v3 dir, float hitstop, uint32_t col) {
  bool heavy = (flags & MF_HEAVY) != 0;
  if (dir.x == 0 && dir.z == 0) dir = facing(att);
  def->hp -= dmg;
  if (def->player) { FG.damage_taken += dmg; FG.combo = 0; meter_add(dmg * .35f); }
  else { FG.combo++; FG.combo_t = 2.2f; FG.combo_pop = 1; meter_add(WP[att->wp].meter * (heavy ? 1.3f : 1)); }
  def->flash = 1; def->vel.x = dir.x * kb; def->vel.z = dir.z * kb;
  fx_sparks(pt, v3_norm(v3_add(v3_scale(dir, -1), v3_(0, .4f, 0))), col, heavy ? 16 : 9);
  shake(heavy ? .5f : .22f);
  att->freeze = fmaxf(att->freeze, hitstop); def->freeze = fmaxf(def->freeze, hitstop * 1.15f);
  sound(heavy || att->wp == WP_MAUL ? SFX_HIT_HEAVY : SFX_HIT_LIGHT, pt, 1);
  actor_expr_set(def->a, EX_SORROW, 1);

  if (def->hp <= 0) {
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
      FG.witch_t = .9f; say_slow(.22f, .9f); meter_add(14);
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
      if (def->player) { meter_add(24); banner("PARRY", .7f); say_slow(.3f, .4f); }
      return true;
    }
    bool breaks = (flags & MF_BREAK) != 0;
    def->guard_hp -= dmg * (flags & MF_HEAVY ? 2.2f : 1.3f);
    def->vel = v3_scale(d, kb * .7f);
    if (breaks || def->guard_hp <= 0) {
      def->hp -= dmg * .35f;
      if (def->player) { FG.damage_taken += dmg * .35f; }
      stagger(def, .95f, "Idle_Shield_Break");
      def->guard_hp = 30;
      fx_clash(pt, col); fx_ring_up(pt, .1f, 1.9f, 0xffffff, .3f);
      sound(SFX_GUARD_BREAK, pt, 1); shake(.5f);
      if (!def->player) banner("GUARD BREAK", .7f);
      att->freeze = .08f; def->freeze = .1f;
      if (def->hp <= 0) { def->hp = 0; if (def->player) player_ko(); else foe_ko(def); }
      return true;
    }
    float chip = dmg * .12f;
    def->hp = fmaxf(1, def->hp - chip);   /* a guard is never the blow that ends it */
    if (def->player) FG.damage_taken += chip;
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
static float move_damage(const fighter_t *f, const move_t *m) { return m->dmg * WP[f->wp].dmg * f->power * (f->player ? 1 : FOE_DMG); }
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
      FG.witch_t = .9f; say_slow(.22f, .9f); meter_add(14);
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
  P->inv = 2.3f; P->vel = v3_(0, 0, 0); P->freeze = 0;
  play(P, idle_clip(P), 1, true, .1f);
  sfx(SFX_SPECIAL_CHARGE, 0, 1);
  fx_burst(chest(P), weapon_color(P->wp), .55f);
  FG.screen_flash = .35f; FG.screen_col = weapon_color(P->wp);
  actor_expr_set(P->a, EX_ANGRY, 1);
  for (int i = 0; i < FG.nfoe; i++) { fighter_t *o = &FG.foe[i]; if (o->st == F_ATTACK) { attack_end(o); o->hold = false; } }
  for (int i = 0; i < MAX_BOLTS; i++) BOLT[i].on = false;
}
/* hits at these clip times of Sword_Heavy_Combo; the last is the big one */
static const float SPEC_T[3] = { .42f, 1.2f, 1.82f };
static void special_hit(int n, bool last) {
  fighter_t *P = &FG.P;
  int w = P->wp;
  float total = 46.f * (w == WP_MAUL ? 1.15f : w == WP_TWIN ? .95f : 1) * P->power;
  float dmg = total * (n == 0 ? .2f : n == 1 ? .2f : .6f);
  uint32_t c = weapon_color(w);
  for (int i = 0; i < FG.nfoe; i++) {
    fighter_t *o = &FG.foe[i];
    if (!alive_foe(o)) continue;
    bool main_ = i == FG.cine_target;
    if (!main_ && dist2d(o->pos, P->pos) > 6.f) continue;
    v3 d = flat(v3_sub(o->pos, P->pos)); d = v3_len(d) > .01f ? v3_norm(d) : facing(P);
    float save = o->inv; o->inv = 0; o->armor = last ? 0 : 1;
    if (o->st == F_GUARD) set_st(o, F_FREE);
    hurt(P, o, main_ ? dmg : dmg * .4f, .7f, last ? 7 : 1.5f, MF_HEAVY | (last ? MF_KNOCK : 0), v3_lerp(chest(P), chest(o), .75f), d, last ? .14f : .05f, c);
    o->armor = 0; if (alive_foe(o) && !last) o->inv = save;
  }
  v3 at = FG.cine_target >= 0 ? chest(&FG.foe[FG.cine_target]) : chest(P);
  P->freeze = 0;
  fx_burst(at, c, last ? 1.05f : .5f);
  if (last) { fx_shock(v3_(P->pos.x, P->pos.y + .05f, P->pos.z), .3f, 7, c, .5f); FG.screen_flash = .75f; FG.screen_col = 0xffffff; shake(1.1f); sfx(SFX_SPECIAL_RELEASE, 0, 1); }
  else { FG.screen_flash = .2f; FG.screen_col = c; shake(.5f); sfx(SFX_HIT_HEAVY, 0, 1); }
}
static void cine_update(float rdt) {
  fighter_t *P = &FG.P;
  if (FG.cine_t < 0) return;
  float t0 = FG.cine_t; FG.cine_t += rdt; float t = FG.cine_t;
  fighter_t *T = FG.cine_target >= 0 ? &FG.foe[FG.cine_target] : NULL;
  /* the rush: a blink to the foe with streaks behind */
  if (t0 < .5f && t >= .5f && T) {
    v3 from = chest(P), d = flat(v3_sub(T->pos, P->pos));
    d = v3_len(d) > .01f ? v3_norm(d) : facing(P);
    v3 to = v3_sub(T->pos, v3_scale(d, WP[P->wp].reach * .6f + .25f));
    if (P->wp != WP_STAFF) {
      v3 step = v3_sub(to, P->pos);
      bool g = true;
      world_move(&P->pos, &step, PLAYER_R, 1.6f, 1, &g);
      actor_set_floor(P->a, P->pos.y);
      for (int i = 0; i < 4; i++) fx_streak(v3_add(from, v3_(ds_rr(-.3f, .3f), ds_rr(-.5f, .4f), ds_rr(-.3f, .3f))), chest(P), weapon_color(P->wp), .1f, .3f);
    }
    P->yaw = atan2f(d.x, d.z);
    if (P->wp == WP_STAFF) { replay(P, "Spell_Simple_Shoot", 1, false, .05f); }
    else { replay(P, "Sword_Heavy_Combo", 1.65f, false, .05f); actor_clip_seek(P->a, .15f); P->trail = true; weapon_trail(P->a, true); }
    sfx((sfx_t)WP[P->wp].swing, 0, 1);
    FG.cine_hits = 0;
  }
  if (t >= .5f) {
    if (P->wp == WP_STAFF) {
      /* a held beam, the crystal to the foe and past it */
      if (t > .62f && t < 1.5f && T) {
        v3 b, tip;
        if (!weapon_points(P->a, 0, &b, &tip)) tip = chest(P);
        v3 dir = v3_norm(v3_sub(chest(T), tip)), end = v3_add(tip, v3_scale(dir, 22));
        float w = .35f * sinf(ds_clamp((t - .62f) / .88f, 0, 1) * DS_PI) + .05f;
        fx_beam(tip, end, 0xffffff, w * .6f); fx_beam(tip, end, weapon_color(WP_STAFF), w * 1.8f); fx_glow(tip, 1.6f, weapon_color(WP_STAFF));
        fx_glow(chest(T), 2.2f, 0xffb0c8);
        if ((int)(t * 30) % 3 == 0) fx_motes(chest(T), weapon_color(WP_STAFF), 2, 5);
      }
      static const float BT[3] = { .7f, 1.0f, 1.35f };
      while (FG.cine_hits < 3 && t >= BT[FG.cine_hits]) { special_hit(FG.cine_hits, FG.cine_hits == 2); FG.cine_hits++; }
    } else {
      float ct = actor_clip_time(P->a);
      while (FG.cine_hits < 3 && ct >= SPEC_T[FG.cine_hits]) { special_hit(FG.cine_hits, FG.cine_hits == 2); FG.cine_hits++; }
    }
  }
  if (t >= CINE_LEN) {
    while (FG.cine_hits < 3) { special_hit(FG.cine_hits, FG.cine_hits == 2); FG.cine_hits++; }
    FG.cine_t = -1;
    P->inv = .5f;
    if (FG.phase == PH_FIGHT) to_free(P, .2f);
    say_slow(.5f, .25f);
  }
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
  if (T && dist2d(T->pos, P->pos) < 7) { float dx = T->pos.x - P->pos.x, dz = T->pos.z - P->pos.z; P->yaw = ds_wrap_pi(P->yaw + ds_wrap_pi(atan2f(dx, dz) - P->yaw) * .6f); }
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
  if (in->weapon_step) {
    for (int k = 1; k <= WP_COUNT; k++) {
      int w = ((P->wp + in->weapon_step * k) % WP_COUNT + WP_COUNT) % WP_COUNT;
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
  if (FG.phase != PH_FIGHT) { f->vel.x *= .9f; f->vel.z *= .9f; return; }
  switch (f->st) {
    case F_FREE: {
      v3 to = flat(v3_sub(P->pos, f->pos));
      float dist = v3_len(to);
      v3 dir = dist > .01f ? v3_scale(to, 1 / dist) : facing(f);
      float pref = ai_range(f);
      /* ask for a turn: at most two swing at once */
      if (!f->token && f->atk_cd <= 0 && tokens_out() < 2 && P->st != F_KO) {
        f->token = true; ai_plan(f); f->ai_t = 0;
      }
      v3 want = v3_(0, 0, 0); float sp = 0;
      if (f->token) {
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
          sound(SFX_ROAR, f->pos, .7f);
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
    turn_to(f, FG.P.pos, 7 * f->speed, dt);
    if (f->tele <= 0) {
      f->hold = false; f->released = true;
      play(f, m->clip, rate * 1.15f, false, 0);
      if (f->lunge_t < 0) f->lunge_t = -f->lunge_t;
      /* the gap may have changed: close what is left */
      float dist = dist2d(f->pos, FG.P.pos);
      f->lunge_v = ds_clamp(dist - (move_reach(f, m) * .72f + .15f), 0, m->lunge + .6f) / fmaxf(f->lunge_t, .1f);
    }
    return;
  }
  /* facing: the player keeps turning till the blow; foes commit at release */
  if (f->player) {
    int t = pick_target(f, facing(f));
    if (t >= 0 && ct < m->hit0) turn_to(f, FG.foe[t].pos, 13, dt);
  } else if (!f->released) turn_to(f, FG.P.pos, 9 * f->speed, dt);

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
  /* the blow */
  if (ct >= m->hit0 && ct <= m->hit1 && !(m->flags & MF_WAVE)) {
    if (f->player) { for (int i = 0; i < FG.nfoe; i++) if (alive_foe(&FG.foe[i])) strike_try(f, m, &FG.foe[i], i); }
    else if (FG.phase == PH_FIGHT) strike_try(f, m, &FG.P, 0);
  }
  if (f->st != F_ATTACK) return;
  /* a foe's next move in its plan */
  if (!f->player && ct >= m->cancel && f->nplan > 0) {
    int mv = f->plan[0];
    for (int i = 0; i + 1 < f->nplan; i++) f->plan[i] = f->plan[i + 1];
    f->nplan--;
    attack_start(f, mv, &FG.P, ai_tele(f, &MOVES[mv]) * .45f);
    return;
  }
  if (ct >= m->to - .01f || near_end(f)) {
    attack_end(f);
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

static void land_impact(fighter_t *f, float radius, float dmg, int flags, uint32_t col) {
  v3 g = v3_(f->pos.x, f->pos.y + .05f, f->pos.z);
  fx_shock(g, .2f, radius * 1.15f, col, .4f);
  fx_shock(g, .1f, radius * .7f, 0xffffff, .25f);
  fx_dust(f->pos, 1.1f, 7); fx_burst(v3_add(g, v3_(0, .2f, 0)), col, .8f);
  shake(.7f);
  sound(SFX_SHOCK, f->pos, 1);
  if (f->player) {
    for (int i = 0; i < FG.nfoe; i++) {
      fighter_t *o = &FG.foe[i];
      if (alive_foe(o) && dist2d(o->pos, f->pos) < radius) land_blow(f, o, dmg, .7f, 4.5f, flags, chest(o), .1f);
    }
  } else if (FG.P.pos.y - f->pos.y < .9f && dist2d(FG.P.pos, f->pos) < radius && FG.phase == PH_FIGHT) land_blow(f, &FG.P, dmg, .7f, 5, flags, chest(&FG.P), .1f);
}

static void tick(fighter_t *f, int idx, float dt, const intent_t *in) {
  f->t += dt;
  if (f->inv > 0) f->inv -= dt;
  if (f->parry_cd > 0) f->parry_cd -= dt;
  if (f->armor > 0 && f->armor < 5) f->armor -= dt;
  if (f->flinch_t > 0) { f->flinch_t -= dt; if (f->flinch_t <= 0) f->flinch = 0; }
  f->guard_hp = fminf(f->player ? 34 : 24 + f->skill * 26, f->guard_hp + 7 * dt);

  if (f->player) {
    if (FG.phase == PH_FIGHT && FG.cine_t < 0 && in) player_think(f, in, dt);
  } else ai_think(idx, f, dt);
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
      if (!f->player && near_end(f) && f->t > .75f) kneel(f);
      break;
    default: break;
  }
  /* rolls glide and drag */
  float drag = f->st == F_ROLL ? 2.6f : f->st == F_FREE ? 0 : f->st == F_ATTACK ? 8 : f->st == F_AIR || f->st == F_PLUNGE || f->st == F_LEAP ? 0 : f->st == F_KO || f->st == F_KNOCK ? 3.2f : 5.5f;
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
  if (f->d.ai != AI_BOSS || !alive_foe(f)) return;
  int want = f->hp < f->hpmax * .33f ? 2 : f->hp < f->hpmax * .66f ? 1 : 0;
  if (want > f->phase) {
    f->phase = want;
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
  fighter_t *all[5]; int n = 0;
  all[n++] = &FG.P;
  for (int i = 0; i < FG.nfoe; i++) all[n++] = &FG.foe[i];
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
  if (FG.lock >= 0 && lock_flick <= 0 && fabsf(in->ax) > .72f && foes_up() > 1) {
    /* the next foe that way round, seen from the camera */
    v3 f = v3_norm(flat(v3_sub(FG.cam_look, FG.cam_pos)));
    v3 r = v3_(f.z, 0, -f.x);
    float cur = v3_dot(flat(v3_sub(FG.foe[FG.lock].pos, FG.cam_pos)), r), best = 1e9f; int pick = -1;
    float s = in->ax > 0 ? -1.f : 1.f;   /* r here points left of the view */
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
static v3 spring_arm(v3 pivot, v3 want) {
  v3 d = v3_sub(want, pivot); float l = v3_len(d), t; v3 n;
  if (l < .01f) return want;
  v3 u = v3_scale(d, 1 / l);
  if (world_raycast(pivot, u, l + .25f, &t, &n)) { float k = fmaxf(.5f, t - .25f); if (k < l) return v3_add(pivot, v3_scale(u, k)); }
  return want;
}
static void cam_update(const intent_t *in, float rdt) {
  fighter_t *P = &FG.P;
  v3 pivot = v3_(P->pos.x, P->pos.y + 1.3f, P->pos.z), pos, look;
  float tfov = 46, rate = 7;
  bool in_lab = world_in_lab(P->pos);
  float base = in_lab ? 2.9f : 3.6f;
  fighter_t *T = FG.lock >= 0 ? &FG.foe[FG.lock] : NULL;

  if (FG.cine_t >= 0) {
    /* the special: a low close look, a whip round, then the blow from the side */
    float t = FG.cine_t;
    fighter_t *V = FG.cine_target >= 0 ? &FG.foe[FG.cine_target] : P;
    float py = P->yaw;
    if (t < .5f) {
      float a = py + ds_lerp(.75f, -.45f, t / .5f);
      pos = v3_add(P->pos, v3_(sinf(a) * 1.75f, .78f, cosf(a) * 1.75f));
      look = v3_add(P->pos, v3_(0, 1.12f, 0));
      tfov = 40; rate = 22;
    } else {
      float k = ds_clamp((t - .5f) / .75f, 0, 1); k = k * k * (3 - 2 * k);
      v3 mid = v3_lerp(P->pos, V->pos, .5f);
      float a = py + ds_lerp(-.5f, -2.2f, k), d = ds_lerp(3.2f, 4.4f, ds_clamp((t - 1.2f) / .7f, 0, 1));
      pos = v3_add(mid, v3_(sinf(a) * d, ds_lerp(.65f, 1.15f, k), cosf(a) * d));
      look = v3_add(mid, v3_(0, 1.f, 0));
      tfov = ds_lerp(44, 52, k); rate = 30;
    }
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
    /* the foes first, then back behind the player */
    v3 mid = FG.nfoe ? FG.foe[0].pos : P->pos;
    v3 d = flat(v3_sub(P->pos, mid)); d = v3_len(d) > .01f ? v3_norm(d) : v3_(0, 0, 1);
    v3 s = v3_(d.z, 0, -d.x);
    float k = ds_clamp(FG.phase_t / 1.6f, 0, 1);
    float dd = dist2d(P->pos, mid);
    pos = v3_add(P->pos, v3_add(v3_scale(d, ds_lerp(1.8f, base, k)), v3_add(v3_scale(s, ds_lerp(1.5f, .8f, k)), v3_(0, ds_lerp(.9f, 1.75f, k), 0))));
    look = v3_add(v3_lerp(P->pos, mid, ds_lerp(.8f, .5f, k)), v3_(0, ds_lerp(1.1f, 1.f, k), 0));
    tfov = 44 + dd * .4f; rate = 8;
    cam_yaw = atan2f(-d.x, -d.z);
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
  if (!FG.def.weapons) FG.def.weapons = 1u << WP_ARC;
  memset(POOL_USED, 0, sizeof POOL_USED);
  memset(BOLT, 0, sizeof BOLT);
  fx_clear();

  fighter_t *P = &FG.P;
  P->a = player; P->pos = actor_pos(player);
  fighter_setup(P, true);
  P->hpmax = 100; P->hp = fmaxf(1, 100 * (def->player_hp > 0 ? def->player_hp : 1));
  P->ghost = P->hp; P->power = 1; P->speed = 1;
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
      f->pos = v3_add(def->center, v3_(f->d.place_x, 0, f->d.place_z));
      float fy = world_floor_y(v3_(f->pos.x, def->center.y + 1.5f, f->pos.z));
      if (fy > -1e8f) f->pos.y = fy;
      actor_visible(f->a, true);
      actor_place(f->a, f->pos, atan2f(P->pos.x - f->pos.x, P->pos.z - f->pos.z));
    }
    fighter_setup(f, false);
    f->yaw = atan2f(P->pos.x - f->pos.x, P->pos.z - f->pos.z);
    f->hp = f->hpmax = f->ghost = f->d.hp * scale;
    f->power = f->d.power * scale;
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
  fight_hud_reset();
}

static void intro_update(float rdt) {
  fighter_t *P = &FG.P;
  float t0 = FG.phase_t; FG.phase_t += rdt; float t = FG.phase_t;
  /* the weapons appear one by one */
  if (t0 < .25f && t >= .25f) { weapon_equip(P->a, P->wp); weapon_draw_flash(P->a); sfx(SFX_SUMMON, 0, .9f); }
  for (int i = 0; i < FG.nfoe; i++) {
    float at = .45f + i * .17f;
    fighter_t *f = &FG.foe[i];
    if (t0 < at && t >= at) {
      if (weapon_of(f->a) != f->wp) weapon_equip(f->a, f->wp);
      weapon_draw_flash(f->a); sound(SFX_SUMMON, f->pos, .7f);
    }
  }
  if (t >= 1.6f) { FG.phase = PH_FIGHT; FG.phase_t = 0; sfx(SFX_UI_OK, 0, .6f); }
}

fight_state fight_update(float rdt, const intent_t *in) {
  static const intent_t none = {0};
  const intent_t *inp = in ? in : &none;
  FG.clock += rdt;

  /* the world's clock */
  float rate = 1;
  if (FG.slow_t > 0) { FG.slow_t -= rdt; rate = FG.slow_k; if (FG.slow_t < .12f) rate = ds_lerp(FG.slow_k, 1, 1 - FG.slow_t / .12f); }
  float prate = rate;
  if (FG.witch_t > 0) { FG.witch_t -= rdt; prate = fmaxf(rate, .85f); }
  /* the special stops the world for everyone but the player */
  if (FG.cine_t >= 0) { bool after = FG.cine_hits >= 3; rate = after ? .32f : .05f; prate = after ? .55f : 1; }

  if (FG.screen_flash > 0) FG.screen_flash = fmaxf(0, FG.screen_flash - rdt * 3.2f);
  if (FG.banner_t > 0) FG.banner_t -= rdt;
  if (FG.weapon_card_t > 0) FG.weapon_card_t -= rdt;
  if (FG.combo_t > 0) { FG.combo_t -= rdt * (rate > .5f ? 1 : .5f); if (FG.combo_t <= 0) FG.combo = 0; }
  if (FG.combo_pop > 0) FG.combo_pop = fmaxf(0, FG.combo_pop - rdt * 5);

  if (FG.phase == PH_INTRO) intro_update(rdt);
  else if (FG.phase != PH_FIGHT) FG.phase_t += rdt;

  if (FG.phase == PH_FIGHT && FG.cine_t < 0) lock_input(inp, rdt);
  if (FG.lock >= 0 && !alive_foe(&FG.foe[FG.lock])) FG.lock = -1;

  /* fighters */
  fighter_t *P = &FG.P;
  float pdt = rdt * prate;
  if (P->freeze > 0) { P->freeze -= rdt; pdt = 0; }
  if (pdt > 0) tick(P, -1, pdt, inp);
  /* the player's i frames and clock during the special come from the cinematic */
  cine_update(rdt);
  for (int i = 0; i < FG.nfoe; i++) {
    fighter_t *f = &FG.foe[i];
    float fdt = rdt * rate;
    if (f->freeze > 0) { f->freeze -= rdt; fdt = 0; }
    if (fdt > 0) tick(f, i, fdt, NULL);
    boss_watch(f);
  }
  end_check();
  separate();
  bolts_update(rdt * rate, true);

  /* actors */
  for (int i = -1; i < FG.nfoe; i++) {
    fighter_t *f = i < 0 ? P : &FG.foe[i];
    float dt_a = f->freeze > 0 ? 0 : rdt * (i < 0 ? prate : rate);
    actor_move(f->a, f->pos, f->yaw);
    if (f->flash > 0) { f->flash = fmaxf(0, f->flash - rdt * 5.5f); float k = 1 + f->flash * 2.2f; actor_flash(f->a, k, k * (f->player ? .9f : .95f), k * .85f); }
    else actor_flash(f->a, 1, 1, 1);
    v3 at = i < 0 ? (FG.nfoe ? chest(&FG.foe[FG.lock >= 0 ? FG.lock : 0]) : chest(P)) : chest(P);
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

  if (FG.phase == PH_WON && FG.phase_t > 2.5f) return FIGHT_WON;
  if (FG.phase == PH_LOST && FG.phase_t > 2.6f) return FIGHT_LOST;
  return FIGHT_RUNNING;
}

float fight_player_hp(void) { return FG.P.hpmax > 0 ? FG.P.hp / FG.P.hpmax : 1; }
actor_t *fight_foe_actor(int i) { return i >= 0 && i < FG.nfoe ? FG.foe[i].a : NULL; }

void fight_end(void) {
  fighter_t *P = &FG.P;
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
    if (f->story) {
      /* left as they are: on one knee when beaten; the story takes it from here */
      if (f->st != F_KNEEL) { if (f->st == F_KO) kneel(f); else { weapon_equip(f->a, -1); actor_play(f->a, "Idle_Loop", 1, true, .3f); } }
      continue;
    }
    weapon_equip(f->a, -1);
    actor_hands_up(f->a, 0); actor_visible(f->a, false);
  }
  memset(POOL_USED, 0, sizeof POOL_USED);
  if (RING) { t3_object_remove(&SC->base, RING); RING = NULL; }
  for (int i = 0; i < MAX_BOLTS; i++) BOLT[i].on = false;
  fx_clear();
  FG.nfoe = 0;
}
