/* battle.c: the Battle Zone. A ladder through the day's fights by tier, single challenges with chosen
 * modifiers and an endless survival of mixed foes, with ranks, scores and unlocks kept in the save's
 * last 500 bytes. The menus pose the next foe in its arena behind the plates; hud_battle.c draws them.
 *
 * Score: a fight's score is fight.c's own (fight_result.score) times the modifiers' product
 * (battle_mult) times a tier bonus (1 + 0.1 per tier above 1). A ladder adds 1500 a clear times the
 * chosen modifiers, plus 500 a life left. Survival multiplies each wave by 1 + 0.1 per wave. */
#if !defined(KH_TEST_WORLD) && !defined(KH_TEST_FIGHT)
#include "battle.h"
#include "game.h"
#include "script.h"
#include "fight.h"
#include "weapon.h"
#include "world.h"
#include "audio.h"
#include "player.h"
#include "fx.h"
#include <string.h>
#include <stdio.h>

bz_ui_t BZ;
static uint32_t hash_title(int e) {
  const char *s = e < FI_COUNT ? FIGHT_TABLE[e].title : "SPARRING NIGHT";
  uint32_t h = 2166136261u; for (; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
  return h ^ (h >> 16);
}
static t3_scene *SC; static t3_camera *CAM;
static actor_t *PL[3];

/* ---- modifiers ---- */
static const struct { const char *name, *blurb, *lock; float mult; } MODS[BZ_NMOD] = {
  {"GLASS", "You take double damage.", NULL, 1.5f},
  {"TURBO", "Everyone moves faster.", NULL, 1.25f},
  {"HEAVY", "Foes hit harder and take more to drop.", NULL, 1.25f},
  {"NO SPECIAL", "The meter never fills.", NULL, 1.2f},
  {"HAZARDS PLUS", "The arena fights back harder.", "Rank A on any fight", 1.2f},
  {"VAMPIRE", "Hits heal you; time hurts you.", "Clear the ladder", 1.15f},
  {"ONE WEAPON", "No switching. What you start with.", "Own all five weapons", 1.15f},
  {"SUDDEN DEATH", "One clean hit ends anyone.", "Rank S on any fight", 1.75f},
};
static const unsigned MOD_BASE = MOD_GLASS | MOD_TURBO | MOD_HEAVY | MOD_NO_SPECIAL;
float battle_mult(unsigned mods) { float m = 1; for (int i = 0; i < BZ_NMOD; i++) if (mods & (1u << i)) m *= MODS[i].mult; return m; }
const char *battle_mod_name(int i) { return MODS[i].name; }
const char *battle_mod_blurb(int i) { return MODS[i].blurb; }
const char *battle_mod_lock(int i) { return MODS[i].lock ? MODS[i].lock : ""; }
const char *battle_mode_name(int m) { static const char *N[] = {"LADDER", "CHALLENGE", "SURVIVAL", "RECORDS"}; return m >= 0 && m < BZM_COUNT ? N[m] : ""; }
static int rank_pts(char r) { return r == 'S' ? 4 : r == 'A' ? 3 : r == 'B' ? 2 : r == 'C' ? 1 : 0; }
static char pts_rank(float p) { return p >= 3.5f ? 'S' : p >= 2.6f ? 'A' : p >= 1.8f ? 'B' : p >= 1 ? 'C' : 'D'; }

/* ---- the save: DS_SAVE.data[520..1019] ---- */
#define BZ_OFF 520
#define BZ_MAGIC 0x5a42484bu   /* "KHBZ" */
typedef struct {
  uint32_t magic;
  uint8_t ver, who, weapon, fight;
  uint8_t weapons, mods;       /* unlocked */
  uint8_t last_mods, flags;
  /* a slot per fight, keyed by a hash of its title: fights added to the script later keep everyone's records */
  struct { uint16_t id; uint8_t open, rank; uint32_t score; uint16_t wins; uint8_t mods, pad; } best[BZ_MAXE];
  uint32_t ladder_score; uint8_t ladder_rank, ladder_clears, pad2[2];
  uint32_t surv_score; uint16_t surv_wave; uint8_t surv_rank, pad3;
  uint32_t plays;
  uint32_t sum;
} bz_save_t;
_Static_assert(sizeof(bz_save_t) <= 1020 - BZ_OFF, "the Battle Zone save is 500 bytes at most");
_Static_assert(FI_COUNT + 1 <= BZ_MAXE, "too many fights for the Battle Zone");
static bz_save_t SV;
static int SLOT[BZ_MAXE];    /* entry to its slot in SV.best, or -1 */
static uint16_t entry_id(int e) { uint16_t h = (uint16_t)(hash_title(e) & 0xffff); return h ? h : 1; }
static uint32_t sv_sum(const bz_save_t *s) {
  const uint8_t *p = (const uint8_t *)s; uint32_t h = 2166136261u;
  for (size_t i = 0; i < offsetof(bz_save_t, sum); i++) h = (h ^ p[i]) * 16777619u;
  return h;
}
static void sv_write(void) { SV.magic = BZ_MAGIC; SV.ver = 1; SV.sum = sv_sum(&SV); memcpy(DS_SAVE.data + BZ_OFF, &SV, sizeof SV); }
static void slots_map(void) {
  for (int e = 0; e <= FI_COUNT; e++) {
    uint16_t id = entry_id(e); SLOT[e] = -1;
    for (int k = 0; k < BZ_MAXE; k++) if (SV.best[k].id == id) { SLOT[e] = k; break; }
    if (SLOT[e] < 0) for (int k = 0; k < BZ_MAXE; k++) if (!SV.best[k].id) { memset(&SV.best[k], 0, sizeof SV.best[k]); SV.best[k].id = id; SLOT[e] = k; break; }
  }
}
static void sv_read(void) {
  memcpy(&SV, DS_SAVE.data + BZ_OFF, sizeof SV);
  if (SV.magic != BZ_MAGIC || SV.sum != sv_sum(&SV)) { memset(&SV, 0, sizeof SV); }
  slots_map();
#ifdef KH_UNLOCK_ALL
  for (int e = 0; e <= FI_COUNT; e++) if (SLOT[e] >= 0) SV.best[SLOT[e]].open = 1;
  SV.weapons = (1u << WP_COUNT) - 1; SV.mods = 0xff;
#endif
}

/* ---- the entries: where each of the day's fights happens ---- */
typedef struct { world_stage stage; v3 c; float r, hour; const char *arena; } bz_place;
static bz_place AR[BZ_MAXE];
static const char *stage_name(world_stage s) {
  switch (s) {
    case STAGE_ARENA_BENCH: return "THE LONG BENCH"; case STAGE_ARENA_LASER: return "LASER CORNER";
    case STAGE_ARENA_FRIDGE: return "THE FRIDGE"; case STAGE_ARENA_SHOP: return "MACHINE SHOP";
    case STAGE_ARENA_UNFOLD: return "THE WHOLE LAB, UNFOLDED"; default: return "DOWNTOWN MESA";
  }
}
static const char *town_name(const char *w) {
  if (!strcmp(w, "lounge")) return "THE LOUNGE"; if (!strcmp(w, "mid_block")) return "WEST MAIN, MID BLOCK";
  if (!strcmp(w, "level1")) return "LEVEL 1 ARCADE"; if (!strcmp(w, "lot_fight")) return "THE BACK LOT";
  return "DOWNTOWN MESA";
}
static void places_scan(void) {
  for (int fi = 0; fi < FI_COUNT; fi++) {
    world_stage st = STAGE_TOWN; float hour = 12; bool found = false;
    for (int k = 0; k < SCRIPT_COUNT && !found; k++) {
      const sc_beat *b = &SCRIPT[k];
      if (b->op == OP_STAGE) st = (world_stage)b->i;
      else if (b->op == OP_CLOCK) hour = b->f;
      else if (b->op == OP_FIGHT && b->i == fi) found = true;
    }
    const sc_fight *F = &FIGHT_TABLE[fi];
    bz_place *P = &AR[fi];
    P->hour = hour; P->r = F->radius;
    float y;
    if (!strcmp(F->where, "stage")) { P->stage = st == STAGE_TOWN ? STAGE_ARENA_BENCH : st; world_stage_floor(P->stage, &P->c, &P->r); P->arena = stage_name(P->stage); }
    else {
      P->stage = STAGE_TOWN; P->arena = town_name(F->where);
      if (!strcmp(F->where, "lounge")) P->c = world_lab(15.6f, 4.8f);
      else if (!world_place(F->where, &P->c, &y)) world_stage_floor(STAGE_TOWN, &P->c, NULL);
    }
  }
  bz_place *S = &AR[FI_COUNT];
  S->stage = STAGE_ARENA_UNFOLD; world_stage_floor(S->stage, &S->c, &S->r); S->hour = 23.9f; S->arena = stage_name(S->stage);
}

/* ---- who is who: the day's W_ME, W_M1, W_M2 for the picked keyholder ---- */
static int me_w(void) { return W_VEE + BZ.who; }
static int canon(int w) {
  int me = me_w();
  if (w == W_ME) return me;
  if (w == W_M1) return me == W_VEE ? W_DEX : W_VEE;
  if (w == W_M2) return me == W_RUTH ? W_DEX : W_RUTH;
  return w;
}
static const actor_palette SHADOW = { {250, .35f, .28f}, {250, .4f, .26f}, {250, .4f, .22f}, {250, .4f, .2f} };
static void foe_from(fighter_def *d, const sc_foe *e) {
  memset(d, 0, sizeof *d);
  bool shadow = e->palette && !strcmp(e->palette, "shadow"), alt = e->palette && !strcmp(e->palette, "alt");
  int w = shadow ? me_w() : canon(e->who);
  const sc_cast *cc = &CAST[w];
  d->model = cc->model; d->pal = shadow ? SHADOW : cc->pal; d->scale = cc->scale;
  d->name = e->name ? e->name : cc->name;
  d->weapon = (weapon_id)e->weapon; d->ai = (ai_kind)e->ai;
  d->hp = e->hp; d->power = e->power; d->speed = e->speed; d->place_x = e->x; d->place_z = e->z;
  if (alt) { d->pal.top.hue += 40; d->pal.hair.hue += 40; }
}
static uint32_t foe_col(const sc_foe *e) { bool shadow = e->palette && !strcmp(e->palette, "shadow"); return shadow ? 0x8a7aff : CAST[canon(e->who)].color; }
/* the secret rung: every keyholder on one floor, after close */
static sc_foe SECRET[3];
static void secret_build(void) {
  sc_foe s0 = {W_ME, WP_SPEAR, AI_BOSS, 380, 1.1f, 1.15f, 0, -7, "shadow", "Midnight"};
  sc_foe s1 = {W_M1, WP_TWIN, AI_DUELIST, 150, .95f, 1.15f, -4, -5, NULL, NULL};
  sc_foe s2 = {W_M2, WP_MAUL, AI_BRUISER, 170, 1, .95f, 4, -5, NULL, NULL};
  SECRET[0] = s0; SECRET[1] = s1; SECRET[2] = s2;
}
static const sc_foe *entry_foes(int e, int *n) {
  if (e == FI_COUNT) { *n = 3; return SECRET; }
  *n = FIGHT_TABLE[e].count; return FIGHT_TABLE[e].foes;
}

static bool open_e(int e) { return e >= 0 && e <= FI_COUNT && SLOT[e] >= 0 && SV.best[SLOT[e]].open; }
static void open_set(int e) { if (e >= 0 && e <= FI_COUNT && SLOT[e] >= 0) SV.best[SLOT[e]].open = 1; }
static void refresh(void) {
  BZ.ne = FI_COUNT + 1; BZ.nopen = 0;
  BZ.weapons = SV.weapons | 1u;
  BZ.mods_open = SV.mods | MOD_BASE;
  BZ.secret_open = open_e(FI_COUNT);
  for (int e = 0; e < BZ.ne; e++) {
    bz_entry *E = &BZ.e[e];
    bool sec = e == FI_COUNT;
    E->secret = sec; E->unlocked = open_e(e);
    if (E->unlocked && !sec) BZ.nopen++;
    E->title = sec ? "SPARRING NIGHT" : FIGHT_TABLE[e].title;
    E->sub = sec ? "Every keyholder on one floor, after close" : FIGHT_TABLE[e].sub;
    E->arena = AR[e].arena;
    E->tier = sec ? 6 : FIGHT_TABLE[e].tier;
    int n; const sc_foe *f = entry_foes(e, &n);
    E->nfoe = n > 4 ? 4 : n; E->boss = false;
    for (int i = 0; i < E->nfoe; i++) {
      fighter_def d; foe_from(&d, &f[i]); E->foe[i] = d.name; E->foe_col[i] = foe_col(&f[i]);
      if (f[i].ai == AI_BOSS) E->boss = true;
    }
    int k = SLOT[e];
    E->best = k >= 0 ? SV.best[k].score : 0; E->best_rank = k >= 0 ? (char)SV.best[k].rank : 0;
    E->best_mods = k >= 0 ? SV.best[k].mods : 0; E->wins = k >= 0 ? SV.best[k].wins : 0;
  }
  BZ.ladder_best = SV.ladder_score; BZ.ladder_rank = (char)SV.ladder_rank; BZ.ladder_clears = SV.ladder_clears;
  BZ.surv_best = SV.surv_score; BZ.surv_rank = (char)SV.surv_rank; BZ.surv_wave_best = SV.surv_wave;
}

void battle_unlock_fight(int fi) { if (fi < 0 || fi >= FI_COUNT || open_e(fi)) return; open_set(fi); sv_write(); }
void battle_unlock_weapon(int w) {
  if (w < 0 || w >= WP_COUNT || (SV.weapons >> w & 1)) return;
  SV.weapons |= (uint8_t)(1u << w);
  if ((SV.weapons & 31) == 31) SV.mods |= MOD_ONE_WEAPON;
  sv_write();
}
bool battle_available(void) { for (int e = 0; e < FI_COUNT; e++) if (open_e(e)) return true; return false; }

/* ---- the stage behind the menus: the next foe posed in its arena ---- */
#define PV_PER 2
static actor_t *PV[AM_COUNT][PV_PER];
static actor_t *SHOWN[4]; static int nshown;
static int st_key = -1;   /* what is posed: entry, player shown, who, weapon (-1 nothing or a wave) */
static v3 stage_c;
static fighter_def STAGED_D[4]; static int staged_n;

static void unstage(void) {
  for (int i = 0; i < nshown; i++) { weapon_equip(SHOWN[i], -1); actor_visible(SHOWN[i], false); }
  nshown = 0;
}
static actor_t *pv_take(const fighter_def *d) {
  for (int i = 0; i < PV_PER; i++) {
    actor_t **a = &PV[d->model][i];
    bool used = false; for (int k = 0; k < nshown; k++) if (SHOWN[k] == *a) used = true;
    if (*a && used) continue;
    if (!*a) *a = actor_new(d->model, &d->pal, 1, SC); else actor_repalette(*a, &d->pal);
    return *a;
  }
  return NULL;
}
static void world_at(int e) {
  const bz_place *P = &AR[e];
  if (world_get_stage() != P->stage) world_set_stage(P->stage);
  world_set_time(P->hour);
  stage_c = P->c;
  float fy = world_floor_y(v3_(stage_c.x, stage_c.y + 1.5f, stage_c.z)); if (fy > -1e8f) stage_c.y = fy;
}
/* the player on the left, the foes on the right, all facing the camera a little turned to each other */
static void pose(const fighter_def *d, int n, bool with_player) {
  unstage();
  for (int i = 0; i < n && nshown < 4; i++) {
    actor_t *a = pv_take(&d[i]); if (!a) continue;
    SHOWN[nshown++] = a;
    static const float X[4] = {1.0f, 2.1f, .2f, 2.9f}, Z[4] = {0, -1.3f, -1.5f, -2.4f};
    v3 p = v3_add(stage_c, v3_(X[i] + (with_player ? .3f : 0), 0, Z[i]));
    float fy = world_floor_y(v3_(p.x, p.y + 1.5f, p.z)); if (fy > -1e8f) p.y = fy;
    actor_visible(a, true);
    actor_place(a, p, -.45f);
    actor_play(a, i == 0 ? "Sword_Idle" : (i & 1) ? "Idle_FoldArms_Loop" : "Sword_Idle", 1, true, 0);
    weapon_equip(a, i == 0 || i == 2 ? (d[i].ai == AI_CASTER ? WP_STAFF : (int)d[i].weapon) : -1);
    actor_expr_clear(a); actor_expr_set(a, d[i].ai == AI_BOSS ? EX_ANGRY : EX_FUN, .6f);
    actor_set_floor(a, p.y);
  }
  for (int i = 0; i < 3; i++) if (PL[i]) actor_visible(PL[i], false);
  actor_t *me = PL[BZ.who];
  if (with_player && me) {
    v3 p = v3_add(stage_c, v3_(-1.5f, 0, .3f));
    float fy = world_floor_y(v3_(p.x, p.y + 1.5f, p.z)); if (fy > -1e8f) p.y = fy;
    actor_visible(me, true); actor_place(me, p, .5f);
    actor_play(me, "Sword_Idle", 1, true, 0); weapon_equip(me, BZ.weapon);
    actor_expr_clear(me); actor_expr_set(me, EX_JOY, .4f); actor_set_floor(me, p.y);
  }
  for (int i = 0; i < nshown; i++) if (i == 0) weapon_draw_flash(SHOWN[i]);
}
static void stage_entry(int e, bool with_player) {
  int key = e * 64 + (with_player ? 32 : 0) + BZ.who * 8 + BZ.weapon;
  if (key == st_key && nshown) return;
  if (st_key < 0 || st_key / 64 != e) BZ.flash = fmaxf(BZ.flash, .6f);   /* a cut to another arena */
  st_key = key;
  world_at(e);
  int n; const sc_foe *f = entry_foes(e, &n);
  staged_n = n > 4 ? 4 : n;
  for (int i = 0; i < staged_n; i++) foe_from(&STAGED_D[i], &f[i]);
  pose(STAGED_D, staged_n, with_player);
}

/* ---- building a fight ---- */
static fight_def FD;
static int wave_e;   /* survival: the arena's entry for this wave */
static void fd_common(void) {
  FD.player_hp = BZ.carry_hp;
  FD.weapons = BZ.weapons; FD.start_weapon = BZ.weapon;
  if (BZ.run_mods & MOD_ONE_WEAPON) FD.weapons = 1u << BZ.weapon;
  FD.mods = BZ.run_mods; FD.battle_zone = true;
}
static void fd_entry(int e, float scale) {
  memset(&FD, 0, sizeof FD);
  const bz_entry *E = &BZ.e[e];
  FD.title = E->title; FD.sub = E->sub;
  int n; const sc_foe *f = entry_foes(e, &n);
  FD.count = n > 4 ? 4 : n;
  for (int i = 0; i < FD.count; i++) foe_from(&FD.foes[i], &f[i]);
  FD.center = AR[e].c; FD.radius = AR[e].r;
  FD.foe_scale = scale;
  if (e < FI_COUNT) {
    const sc_fight *F = &FIGHT_TABLE[e];
    FD.twist = F->twist; FD.twist_a = F->twist_a; FD.twist_b = F->twist_b;
    if (F->obj_x != 0 || F->obj_z != 0) FD.obj_pos = v3_add(AR[e].c, v3_(F->obj_x, 0, F->obj_z));
    FD.obj_label = F->obj_label; FD.phase2_at = F->phase2_at; FD.finisher_qte = F->finisher_qte;
    /* FT_PROTECT needs a story actor; the zone has none, so it plays as a straight fight */
    if (FD.twist == FT_PROTECT || FD.twist == FT_TUTORIAL) FD.twist = FT_NONE;
  } else { FD.twist = FT_BOSS; FD.phase2_at = .5f; }
  fd_common();
}
/* survival: a mixed wave. Every foe of every open fight is in the draw; each fifth wave is a boss. */
static sc_foe POOLF[48]; static int npool, POOLE[48];
static void pool_build(void) {
  npool = 0;
  for (int e = 0; e < FI_COUNT + 1; e++) {
    if (!open_e(e)) continue;
    int n; const sc_foe *f = entry_foes(e, &n);
    for (int i = 0; i < n && npool < 48; i++) { POOLF[npool] = f[i]; POOLE[npool] = e; npool++; }
  }
}
static void fd_wave(int w) {
  memset(&FD, 0, sizeof FD);
  static char title[24], sub[96];
  snprintf(title, sizeof title, "WAVE %d", w);
  bool boss = w % 5 == 0;
  int want = boss ? (w >= 10 ? 2 : 1) : 1 + (w >= 3) + (w >= 7);
  int picked[4], np = 0, bosses[48], nb = 0, grunts[48], ng = 0;
  for (int i = 0; i < npool; i++) { if (POOLF[i].ai == AI_BOSS) bosses[nb++] = i; else grunts[ng++] = i; }
  if (boss && nb) picked[np++] = bosses[(w / 5 - 1) % nb];
  for (int tries = 0; np < want && tries < 40; tries++) {
    int k = ng ? grunts[(int)(ds_rand() * ng)] : (int)(ds_rand() * npool);
    bool dup = false; for (int j = 0; j < np; j++) if (picked[j] == k) dup = true;
    if (!dup || tries > 30) picked[np++] = k;
  }
  /* the arena: a new one every third wave, from the fights the foes come from */
  if (w == 1 || w % 3 == 1) wave_e = POOLE[picked[0]];
  sub[0] = 0;
  static const float X[4] = {0, -3.2f, 3.2f, 0}, Z[4] = {-6, -4.6f, -4.6f, -8};
  for (int i = 0; i < np; i++) {
    foe_from(&FD.foes[i], &POOLF[picked[i]]);
    FD.foes[i].place_x = X[i]; FD.foes[i].place_z = Z[i];
    if (POOLF[picked[i]].ai == AI_BOSS && i > 0) FD.foes[i].ai = AI_BRUISER;
    size_t l = strlen(sub); snprintf(sub + l, sizeof sub - l, "%s%s", i ? ", " : "", FD.foes[i].name);
  }
  FD.count = np; FD.title = title; FD.sub = sub;
  FD.center = AR[wave_e].c; FD.radius = AR[wave_e].r > 0 ? AR[wave_e].r : 10;
  FD.foe_scale = ds_clamp(.72f + .045f * w, .72f, 1.7f);
  FD.twist = boss ? FT_BOSS : FT_NONE; FD.phase2_at = boss ? .5f : 0;
  fd_common();
}

/* ---- the run ---- */
static float fight_clock; static int ladder_pts;
static void set_screen(bz_screen s) {
  BZ.screen = s; BZ.t = 0; BZ.sel = 0;
#ifdef KH_BZ_AUTO
  ds_log("[bz] screen %d at %.2f s", (int)s, ds_now());
#endif
}
static unsigned ladder_mods(int rung, int n) {
  unsigned m = BZ.mods;
  float f = n > 1 ? (float)rung / (n - 1) : 0;
  if (f >= .34f) m |= MOD_TURBO;
  if (f >= .6f) m |= MOD_HAZARD_PLUS;
  if (f >= .85f) m |= MOD_HEAVY;
  return m;
}
static void rung_info_entry(int e) {
  const bz_entry *E = &BZ.e[e];
  BZ.rung_title = E->title; BZ.rung_sub = E->sub; BZ.rung_arena = E->arena;
  BZ.rung_nfoe = E->nfoe; BZ.rung_boss = E->boss;
  for (int i = 0; i < E->nfoe; i++) { BZ.rung_foe[i] = E->foe[i]; BZ.rung_col[i] = E->foe_col[i]; }
}
static void rung_prepare(void) {
  if (BZ.mode == BZM_SURVIVAL) {
    fd_wave(BZ.wave);
    BZ.cur = -1; BZ.run_mods = BZ.mods;
    BZ.rung_title = FD.title; BZ.rung_sub = "Survive"; BZ.rung_arena = AR[wave_e].arena;
    BZ.rung_nfoe = FD.count; BZ.rung_boss = BZ.wave % 5 == 0;
    for (int i = 0; i < FD.count; i++) { BZ.rung_foe[i] = FD.foes[i].name; BZ.rung_col[i] = 0xe8433a; }
    world_at(wave_e);
    st_key = -1;
    staged_n = FD.count; memcpy(STAGED_D, FD.foes, sizeof(fighter_def) * staged_n);
    pose(STAGED_D, staged_n, true);
  } else {
    int e = BZ.mode == BZM_LADDER ? BZ.order[BZ.rung] : BZ.fight;
    BZ.cur = e;
    BZ.run_mods = BZ.mode == BZM_LADDER ? ladder_mods(BZ.rung, BZ.nrungs) : BZ.mods;
    rung_info_entry(e);
    stage_entry(e, true);
  }
  set_screen(BZ_RUNG);
  music_mood(MOOD_ZONE);
}
static void run_begin(void) {
  BZ.score = 0; BZ.rung = 0; BZ.wave = 1; BZ.lives = BZ.mode == BZM_LADDER ? 3 : 1; BZ.carry_hp = 1;
  BZ.run_over = false; BZ.run_cleared = false; BZ.nunlocked = 0;
  if (BZ.mode == BZM_LADDER) {
    BZ.nrungs = 0;
    for (int e = 0; e < FI_COUNT; e++) if (open_e(e)) BZ.order[BZ.nrungs++] = e;
    for (int i = 1; i < BZ.nrungs; i++) for (int j = i; j > 0 && BZ.e[BZ.order[j]].tier < BZ.e[BZ.order[j - 1]].tier; j--) { int t = BZ.order[j]; BZ.order[j] = BZ.order[j - 1]; BZ.order[j - 1] = t; }
    if (BZ.secret_open) BZ.order[BZ.nrungs++] = FI_COUNT;
  }
  if (BZ.mode == BZM_SURVIVAL) pool_build();
  ladder_pts = 0;
  SV.who = (uint8_t)BZ.who; SV.weapon = (uint8_t)BZ.weapon; SV.fight = (uint8_t)BZ.fight; SV.last_mods = (uint8_t)BZ.mods; SV.plays++;
  sv_write();
  rung_prepare();
}
static void fight_go(void) {
  /* a wave was drawn when it was previewed; a fight is built fresh (a retry carries the same hp in) */
  if (BZ.mode != BZM_SURVIVAL) fd_entry(BZ.cur, BZ.mode == BZM_LADDER ? 1 + .04f * BZ.rung : 1);
  else FD.player_hp = BZ.carry_hp;
  unstage();
  actor_t *me = PL[BZ.who];
  v3 p = v3_add(FD.center, v3_(0, 0, 1.5f));
  float fy = world_floor_y(v3_(p.x, p.y + 1.5f, p.z)); if (fy > -1e8f) p.y = fy;
  actor_visible(me, true); weapon_equip(me, -1); actor_expr_clear(me);
  actor_place(me, p, DS_PI);
  player_set_actor(me);
  fight_start(&FD, me);
  fight_clock = 0;
  set_screen(BZ_FIGHT);
}
static void unlock_note(const char *s) { if (BZ.nunlocked < 4) BZ.unlocked_msg[BZ.nunlocked++] = s; }
static void check_unlocks(char rank, bool ladder_clear, char run_rank) {
  if (rank_pts(rank) >= 3 && !(SV.mods & MOD_HAZARD_PLUS)) { SV.mods |= MOD_HAZARD_PLUS; unlock_note("MODIFIER  HAZARDS PLUS"); }
  if (rank == 'S' && !(SV.mods & MOD_SUDDEN)) { SV.mods |= MOD_SUDDEN; unlock_note("MODIFIER  SUDDEN DEATH"); }
  if (ladder_clear && !(SV.mods & MOD_VAMPIRE)) { SV.mods |= MOD_VAMPIRE; unlock_note("MODIFIER  VAMPIRE"); }
  if (ladder_clear && rank_pts(run_rank) >= 3 && !open_e(FI_COUNT)) { open_set(FI_COUNT); unlock_note("SECRET RUNG  SPARRING NIGHT"); }
}
static void fight_over(bool won, const fight_result *r, float hp) {
  BZ.r = *r; BZ.won = won;
  int tier = BZ.cur >= 0 ? BZ.e[BZ.cur].tier : 1;
  BZ.mult = battle_mult(BZ.run_mods) * (1 + .1f * (tier > 1 ? tier - 1 : 0));
  if (BZ.mode == BZM_SURVIVAL) BZ.mult = battle_mult(BZ.run_mods) * (1 + .1f * BZ.wave);
  BZ.fight_score = won ? (uint32_t)(r->score * BZ.mult + .5f) : 0;
  BZ.new_best = false; BZ.new_run_best = false; BZ.nunlocked = 0;
  if (won && BZ.cur >= 0 && SLOT[BZ.cur] >= 0) {
    __typeof__(SV.best[0]) *B = &SV.best[SLOT[BZ.cur]];
    if (BZ.fight_score > B->score) { B->score = BZ.fight_score; B->mods = (uint8_t)BZ.run_mods; BZ.new_best = true; }
    if (rank_pts(r->rank) > rank_pts((char)B->rank) || !B->rank) B->rank = (uint8_t)r->rank;
    if (B->wins < 65535) B->wins++;
  }
  BZ.score += BZ.fight_score;
  BZ.run_over = false; BZ.run_cleared = false; BZ.run_rank = 0;
  if (BZ.mode == BZM_CHALLENGE) { BZ.run_over = true; BZ.run_cleared = won; BZ.run_rank = won ? r->rank : 'D'; }
  else if (BZ.mode == BZM_LADDER) {
    if (won) {
      ladder_pts += rank_pts(r->rank);
      BZ.carry_hp = fminf(1, hp + .3f);
      if (BZ.rung + 1 >= BZ.nrungs) {
        BZ.run_over = true; BZ.run_cleared = true;
        BZ.score += (uint32_t)(1500 * battle_mult(BZ.mods) + .5f) + 500u * (unsigned)BZ.lives;
        BZ.run_rank = pts_rank((float)ladder_pts / BZ.nrungs - .5f * (3 - BZ.lives));
      }
    } else {
      BZ.lives--; BZ.carry_hp = fmaxf(.6f, BZ.carry_hp);
      if (BZ.lives <= 0) { BZ.run_over = true; float p = BZ.rung ? (float)ladder_pts / BZ.nrungs : 0; BZ.run_rank = pts_rank(fminf(p, 1.5f)); }
    }
    if (BZ.run_over) {
      if (BZ.score > SV.ladder_score) { SV.ladder_score = BZ.score; BZ.new_run_best = true; }
      if (rank_pts(BZ.run_rank) > rank_pts((char)SV.ladder_rank) || !SV.ladder_rank) SV.ladder_rank = (uint8_t)BZ.run_rank;
      if (BZ.run_cleared && SV.ladder_clears < 255) SV.ladder_clears++;
    }
  } else {
    if (won) BZ.carry_hp = fminf(1, hp + .2f);
    else {
      BZ.run_over = true;
      int w = BZ.wave - 1;
      BZ.run_rank = w >= 15 ? 'S' : w >= 10 ? 'A' : w >= 6 ? 'B' : w >= 3 ? 'C' : 'D';
      if (BZ.score > SV.surv_score) { SV.surv_score = BZ.score; BZ.new_run_best = true; }
      if (w > SV.surv_wave) SV.surv_wave = (uint16_t)w;
      if (rank_pts(BZ.run_rank) > rank_pts((char)SV.surv_rank) || !SV.surv_rank) SV.surv_rank = (uint8_t)BZ.run_rank;
    }
  }
  check_unlocks(won ? r->rank : 0, BZ.mode == BZM_LADDER && BZ.run_cleared, BZ.run_rank);
  sv_write(); refresh();
  set_screen(BZ_RESULT);
  BZ.flash = 1;
  music_mood(won ? MOOD_CLEAR : MOOD_ZONE);
}
static void after_result(bool again) {
  if (BZ.run_over) {
    if (again) { run_begin(); return; }
    st_key = -1; set_screen(BZ_HUB); music_mood(MOOD_ZONE); return;
  }
  if (BZ.mode == BZM_LADDER) { if (BZ.won) BZ.rung++; }
  else if (BZ.mode == BZM_SURVIVAL) BZ.wave++;
  rung_prepare();
}

/* ---- entry points ---- */
void battle_set_fighters(actor_t *const players[3], int who) { for (int i = 0; i < 3; i++) PL[i] = players[i]; BZ.who = who; }
void battle_init(t3_scene *scene, t3_camera *cam) {
  SC = scene; CAM = cam;
  sv_read(); secret_build(); places_scan(); refresh();
}
void battle_enter(void) {
  sv_read(); refresh();
  BZ.weapon = SV.weapon < WP_COUNT && (BZ.weapons >> SV.weapon & 1) ? SV.weapon : WP_ARC;
  BZ.mods = SV.last_mods & BZ.mods_open;
  BZ.fight = SV.fight <= FI_COUNT && open_e(SV.fight) ? SV.fight : 0;
  if (!open_e(BZ.fight)) for (int e = 0; e <= FI_COUNT; e++) if (open_e(e)) { BZ.fight = e; break; }
  BZ.hub_sel = 0; BZ.mode = BZM_LADDER; st_key = -1;
  set_screen(BZ_HUB);
  music_mood(MOOD_ZONE);
}

static int step_open(int e, int d) {
  for (int k = 0; k < BZ.ne; k++) { e = (e + d + BZ.ne) % BZ.ne; if (open_e(e)) return e; }
  return e;
}
static int step_weapon(int w, int d) {
  for (int k = 0; k < WP_COUNT; k++) { w = (w + d + WP_COUNT) % WP_COUNT; if (BZ.weapons >> w & 1) return w; }
  return w;
}
static bool row_on(int r) { return r != BZR_FIGHT || BZ.mode == BZM_CHALLENGE; }

/* the menu camera: a slow drift on the posed fighters. kind 0: foes alone, right of the plates; 1: the
 * player and the foes right of the setup panel; 2: the versus shot, centred; 3: the results, right */
static void menu_camera(float dt, int kind) {
  float t = (float)ds_now();
  static const float EX[4] = {-1.1f, -3.1f, -.25f, -2.2f}, EY[4] = {1.35f, 1.4f, 1.3f, 1.35f}, EZ[4] = {4.4f, 6.0f, 5.6f, 5.8f};
  static const float AX[4] = {-.35f, -2.5f, -.25f, -1.45f};
  v3 c = stage_c;
  float sway = sinf(t * .21f) * .22f;
  v3 eye = v3_add(c, v3_(EX[kind] + sway, EY[kind], EZ[kind]));
  v3 at = v3_add(c, v3_(AX[kind] + sway * .5f, 1.08f, 0));
  t3_object_set_position(CAM, eye.x, eye.y, eye.z);
  t3_object_look_at(CAM, at.x, at.y, at.z);
  if (CAM->fov != 36) { CAM->fov = 36; t3_camera_update_projection_matrix(CAM); }
  world_update(dt, eye);
}
static void stage_update(float dt) {
  for (int i = 0; i < nshown; i++) {
    actor_t *a = SHOWN[i];
    t3_object *co = (t3_object *)CAM;
    actor_look(a, v3_(co->position.x, co->position.y, co->position.z), .5f);
    actor_update(a, dt);
  }
  actor_t *me = PL[BZ.who];
  if (me && actor_is_visible(me)) { if (nshown) actor_look(me, actor_head_pos(SHOWN[0]), .7f); actor_update(me, dt); }
  fx_update(dt, CAM);
}
/* what the menus pose: the hub cycles through the open fights every six seconds, the challenge setup shows
 * its fight, the ladder its first rung, records the row */
static int first_rung(void) {
  int best = -1;
  for (int e = 0; e < FI_COUNT; e++) if (open_e(e) && (best < 0 || BZ.e[e].tier < BZ.e[best].tier)) best = e;
  return best < 0 ? BZ.fight : best;
}
static void menu_stage(void) {
  int e = BZ.fight; bool wp = false;
  int k = (int)(ds_now() / 6), n = 0, list[BZ_MAXE];
  for (int i = 0; i <= FI_COUNT; i++) if (open_e(i)) list[n++] = i;
  int cyc = n ? list[k % n] : BZ.fight;
  switch (BZ.screen) {
    case BZ_HUB: e = BZ.hub_sel == BZM_CHALLENGE ? BZ.fight : cyc; break;
    case BZ_SETUP: wp = true; e = BZ.mode == BZM_CHALLENGE ? BZ.fight : BZ.mode == BZM_LADDER ? first_rung() : cyc; break;
    case BZ_RECORDS: e = BZ.e[BZ.sel].unlocked ? BZ.sel : (st_key >= 0 ? st_key / 64 : cyc); break;
    default: return;
  }
  if (open_e(e)) stage_entry(e, wp);
}

bool battle_update(float dt, const intent_t *in) {
  BZ.t += dt; BZ.flash = fmaxf(0, BZ.flash - dt * 2.5f);
  switch (BZ.screen) {
    case BZ_HUB: {
      if (in->nav_y) { int n = BZ.hub_sel + in->nav_y; if (n >= 0 && n < BZM_COUNT) { BZ.hub_sel = n; sfx(SFX_UI, 0, 1); } }
      if (in->back) {
        unstage(); st_key = -1; sfx(SFX_UI, 0, 1);
        for (int i = 0; i < 3; i++) if (PL[i]) { weapon_equip(PL[i], -1); actor_expr_clear(PL[i]); }
        return false;
      }
      if (in->ok) {
        sfx(SFX_UI_OK, 0, 1);
        if (BZ.hub_sel == BZM_RECORDS) set_screen(BZ_RECORDS);
        else { BZ.mode = BZ.hub_sel; set_screen(BZ_SETUP); BZ.sel = BZR_START; }
      }
      menu_stage(); menu_camera(dt, 0); stage_update(dt);
      break;
    }
    case BZ_SETUP: {
      if (in->nav_y) {
        int n = BZ.sel;
        do { n += in->nav_y; } while (n >= 0 && n < BZR_COUNT && !row_on(n));
        if (n >= 0 && n < BZR_COUNT) { BZ.sel = n; sfx(SFX_UI, 0, 1); }
      }
      int r = BZ.sel;
      if (in->nav_x) {
        sfx(SFX_UI, 0, 1);
        if (r == BZR_FIGHTER) { BZ.who = (BZ.who + in->nav_x + 3) % 3; refresh(); }
        else if (r == BZR_WEAPON) BZ.weapon = step_weapon(BZ.weapon, in->nav_x);
        else if (r == BZR_FIGHT) BZ.fight = step_open(BZ.fight, in->nav_x);
      }
      if (in->ok) {
        if (r >= BZR_MOD0 && r < BZR_START) {
          unsigned b = 1u << (r - BZR_MOD0);
          if (BZ.mods_open & b) { BZ.mods ^= b; sfx(SFX_UI_OK, 0, 1); } else sfx(SFX_CLASH, 0, .5f);
        } else if (r == BZR_START) { sfx(SFX_UI_OK, 0, 1); run_begin(); break; }
        else if (r == BZR_WEAPON) { BZ.weapon = step_weapon(BZ.weapon, 1); sfx(SFX_UI, 0, 1); }
        else if (r == BZR_FIGHTER) { BZ.who = (BZ.who + 1) % 3; refresh(); sfx(SFX_UI, 0, 1); }
        else if (r == BZR_FIGHT) { BZ.fight = step_open(BZ.fight, 1); sfx(SFX_UI, 0, 1); }
      }
      if (in->back) { sfx(SFX_UI, 0, 1); set_screen(BZ_HUB); BZ.hub_sel = BZ.mode; break; }
      if (BZ.screen == BZ_SETUP) { menu_stage(); menu_camera(dt, 1); stage_update(dt); }
      break;
    }
    case BZ_RECORDS:
      if (in->nav_y) { int n = BZ.sel + in->nav_y; if (n >= 0 && n < BZ.ne) { BZ.sel = n; sfx(SFX_UI, 0, 1); } }
      if (in->back || in->ok) { sfx(SFX_UI, 0, 1); set_screen(BZ_HUB); BZ.hub_sel = BZM_RECORDS; break; }
      menu_stage(); menu_camera(dt, 0); stage_update(dt);
      break;
    case BZ_RUNG:
      if (in->back) { sfx(SFX_UI, 0, 1); unstage(); st_key = -1; set_screen(BZ_HUB); BZ.hub_sel = BZ.mode; break; }
      if ((in->ok && BZ.t > .5f) || BZ.t > 9) { sfx(SFX_UI_OK, 0, 1); fight_go(); break; }
      menu_camera(dt, 2); stage_update(dt);
      break;
    case BZ_FIGHT: {
      if (in->pause) { set_screen(BZ_PAUSE); sfx(SFX_UI, 0, 1); break; }
      fight_clock += dt;
      fight_state st = fight_update(dt, in);
      world_update(dt, v3_(((t3_object *)CAM)->position.x, ((t3_object *)CAM)->position.y, ((t3_object *)CAM)->position.z));
#ifdef KH_BZ_AUTO
      if (st == FIGHT_RUNNING && fight_clock > KH_BZ_AUTO) {
        /* the test: a win after a moment, with a seeded score */
        fight_result r; fight_result_get(&r);
        r.score = 900 + (int)(ds_rand() * 900); r.time = fight_clock; r.max_combo = 12; r.parries = 2;
        r.rank = r.score >= 1700 ? 'S' : r.score >= 1450 ? 'A' : r.score >= 1200 ? 'B' : r.score >= 900 ? 'C' : 'D';
        ds_log("[bz] auto win: %s score %d rank %c", FD.title, r.score, r.rank);
        fight_end(); fight_over(true, &r, .7f);
        st_key = -1; if (BZ.cur >= 0) stage_entry(BZ.cur, true); else pose(STAGED_D, staged_n, true);
        break;
      }
#endif
      if (st == FIGHT_WON || st == FIGHT_LOST) {
        fight_result r; fight_result_get(&r);
        float hp = fight_player_hp();
        fight_end();
        ds_log("[bz] %s %s score %d rank %c", FD.title, st == FIGHT_WON ? "won" : "lost", r.score, r.rank);
        fight_over(st == FIGHT_WON, &r, hp);
        st_key = -1; if (BZ.cur >= 0) stage_entry(BZ.cur, true); else pose(STAGED_D, staged_n, true);
      }
      break;
    }
    case BZ_PAUSE:
      if (in->pause || in->back) { set_screen(BZ_FIGHT); break; }
      if (in->nav_y) { int n = BZ.pause_sel + in->nav_y; if (n >= 0 && n < 3) { BZ.pause_sel = n; sfx(SFX_UI, 0, 1); } }
      if (in->ok) {
        sfx(SFX_UI_OK, 0, 1);
        if (BZ.pause_sel == 0) set_screen(BZ_FIGHT);
        else if (BZ.pause_sel == 1) {
          /* retry: the ladder spends a life on it (the last one starts the run over); survival starts over */
          fight_end();
          if (BZ.mode == BZM_LADDER && BZ.lives > 1) { BZ.lives--; fight_go(); }
          else if (BZ.mode == BZM_CHALLENGE) fight_go();
          else run_begin();
        } else { fight_end(); st_key = -1; set_screen(BZ_HUB); BZ.hub_sel = BZ.mode; music_mood(MOOD_ZONE); }
        BZ.pause_sel = 0;
      }
      break;
    case BZ_RESULT:
      if (BZ.t > .9f && (in->ok || in->back)) { sfx(SFX_UI_OK, 0, 1); after_result(in->ok); break; }
      menu_camera(dt, 3); stage_update(dt);
      break;
  }
  return true;
}

void battle_draw(void) {
  if (BZ.screen == BZ_FIGHT || BZ.screen == BZ_PAUSE) fight_draw_hud();
  hud_battle_draw();
}
#endif
