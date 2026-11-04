/* fight_int.h: what fight.c shares with hud_fight.c and the test bot. */
#ifndef KH_FIGHT_INT_H
#define KH_FIGHT_INT_H
#include "fight.h"

typedef enum {
  F_FREE, F_ATTACK, F_RECOVER, F_ROLL, F_GUARD, F_STAGGER, F_HIT, F_KNOCK, F_GETUP,
  F_AIR, F_PLUNGE, F_LAND, F_CAST, F_CHARGE, F_SPECIAL, F_ROAR, F_LEAP,
  F_KO, F_KNEEL, F_DOWNED
} f_state;

typedef struct {
  actor_t *a; bool player, pooled, story;
  fighter_def d;
  v3 pos, vel; float yaw; bool grounded;
  float hp, hpmax, ghost, power, speed, skill;
  int wp;
  f_state st; float t;
  /* the move in hand */
  int mv; uint32_t hitmask; bool whoosh, hold, released, trail, multi;
  float tele, tele_max, lunge_v, lunge_t, hold_at;
  const char *clip;
  int chain_l, chain_h;
  char buf; float buf_t;
  /* timers */
  float freeze, flash, inv, parry_t, parry_cd, guard_hp, armor, dash_ok, run_t, stun, charge;
  int fired;
  /* ai */
  int ai_mode; float ai_t, atk_cd, circ, react_cd; bool token; int plan[3]; int nplan; int phase; int flinch; float flinch_t; v3 leap_to;
} fighter_t;

typedef enum { PH_INTRO, PH_FIGHT, PH_WON, PH_LOST } f_phase;

typedef struct {
  fight_def def;
  f_phase phase; float phase_t, clock;
  fighter_t P; fighter_t foe[4]; int nfoe;
  int lock;           /* the locked foe, or -1 */
  float meter;        /* 0..100 */
  int combo; float combo_t, combo_pop;
  float screen_flash; uint32_t screen_col;
  float slow_t, slow_k, witch_t;
  float shake;
  /* the special's cinematic: t runs on real time, -1 is off */
  float cine_t; int cine_target; int cine_hits; const char *cine_name;
  /* a banner across the middle: PERFECT, PARRY, GUARD BREAK, SECOND WIND */
  const char *banner; float banner_t;
  float damage_taken;
  v3 cam_pos, cam_look; float cam_fov;
  float weapon_card_t;
} fight_state_t;
extern fight_state_t FG;

/* the screen position of a world point in 1280x720: false when behind the camera */
bool fight_project(v3 p, float *x, float *y);
void fight_hud_init(void);
void fight_hud_reset(void);
#endif
