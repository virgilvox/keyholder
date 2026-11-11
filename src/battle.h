/* battle.h: the Battle Zone, a replayable ladder of the day's fights with ranks, scores and modifiers.
 * Progress lives in DS_SAVE.data bytes 520..1019 (the day's save stays below 520). */
#ifndef KH_BATTLE_H
#define KH_BATTLE_H
#include "ds.h"
#include "input.h"
#include "three.h"
#include "actor.h"
#include "fight.h"
void battle_unlock_fight(int fi);     /* FI_ id: won in the story */
void battle_unlock_weapon(int w);     /* weapon_id: given in the story */
bool battle_available(void);          /* anything unlocked: the title shows the entry */
void battle_init(t3_scene *scene, t3_camera *cam);
void battle_enter(void);              /* from the title, as BZ.who (battle_set_fighters first) */
bool battle_update(float dt, const intent_t *in);   /* false when the player backs out to the title */
void battle_draw(void);               /* after the scene: menus, results */
/* game.c: the three keyholders' actors (Vee, Dex, Ruth) and who the title has picked */
void battle_set_fighters(actor_t *const players[3], int who);
/* the title's entry (hud_battle.c): sel when the title's cursor is on it */
void hud_battle_title(float t, bool sel);

/* ---- shared with hud_battle.c ---- */
#define BZ_MAXE 16          /* fight entries: the day's fights plus the secret rung */
#define BZ_NMOD 8
typedef enum { BZ_HUB, BZ_SETUP, BZ_RUNG, BZ_FIGHT, BZ_PAUSE, BZ_RESULT, BZ_RECORDS } bz_screen;
enum { BZM_LADDER, BZM_CHALLENGE, BZM_SURVIVAL, BZM_RECORDS, BZM_COUNT };
/* setup rows */
enum { BZR_FIGHTER, BZR_WEAPON, BZR_FIGHT, BZR_MOD0, BZR_START = BZR_MOD0 + BZ_NMOD, BZR_COUNT };
typedef struct {
  const char *title, *sub, *arena; int tier; bool unlocked, boss, secret;
  const char *foe[4]; uint32_t foe_col[4]; int nfoe;
  uint32_t best; char best_rank; unsigned best_mods; int wins;
} bz_entry;
typedef struct {
  bz_screen screen; float t;          /* seconds on this screen */
  int mode, hub_sel, sel, pause_sel;
  /* setup */
  int who, weapon, fight;             /* fight: an entry */
  unsigned mods, mods_open;           /* chosen, unlocked */
  unsigned weapons;                   /* unlocked */
  /* entries and records */
  bz_entry e[BZ_MAXE]; int ne, nopen; bool secret_open;
  uint32_t ladder_best, surv_best; char ladder_rank, surv_rank; int surv_wave_best, ladder_clears;
  /* the run */
  int rung, nrungs, order[BZ_MAXE], lives, wave;
  unsigned run_mods;                  /* this fight's: chosen plus the ladder's escalation */
  float carry_hp; uint32_t score;     /* the run's total so far */
  int cur;                            /* the entry being fought (-1 a survival wave) */
  const char *rung_title, *rung_sub, *rung_arena; const char *rung_foe[4]; uint32_t rung_col[4]; int rung_nfoe; bool rung_boss;
  /* results */
  fight_result r; bool won; float mult; uint32_t fight_score;
  bool run_over, run_cleared, new_best, new_run_best; char run_rank;
  const char *unlocked_msg[4]; int nunlocked;
  float flash;                        /* a white flash, decays */
} bz_ui_t;
extern bz_ui_t BZ;
float battle_mult(unsigned mods);
const char *battle_mod_name(int i);
const char *battle_mod_blurb(int i);
const char *battle_mod_lock(int i);   /* how to unlock it, while locked */
const char *battle_mode_name(int m);
void hud_battle_draw(void);           /* battle_draw calls it */
#endif
