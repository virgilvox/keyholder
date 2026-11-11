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
  /* v1.1 */
  bool ally;          /* a called assist: fights the foes, never hurt */
  bool goal;          /* goes for the twist's objective (cage, Sam, the door, the bike) instead of the player */
  float aggro;        /* seconds it stays on the player after being hit off the objective */
  float tell;         /* a boss's big move is coming: the glint, the pose, the "!" */
  float ice;          /* standing on frost: 0..1 */
  float haz_inv;      /* hazards do not hit twice in a row */
  float rehit;        /* the flurry: the hit mask clears on this clock */
  float down_t;       /* battle zone: time lying beaten */
} fighter_t;

typedef enum { PH_INTRO, PH_FIGHT, PH_WON, PH_LOST } f_phase;
/* the intro's beats, seconds: whip pan, hero shot of the foe, the smash card, back behind the player */
#define INTRO_HERO .42f
#define INTRO_CARD 1.12f
#define INTRO_BACK 1.72f
#define INTRO_END 2.3f

/* the twist's state: one block, meanings per twist (see fight.c, "twists") */
enum { TUT_MOVE, TUT_LIGHT, TUT_HEAVY, TUT_DODGE, TUT_GUARD, TUT_PARRY, TUT_SPECIAL, TUT_FREE };
typedef struct {
  int kind;
  float meter, meter_max;    /* TIMER gas, HOLD door, RETRIEVE escape, SCORE clock: 0..meter_max */
  float obj_hp, obj_hpmax;   /* DEFEND cage, PROTECT Sam, TIMER the stop's three knocks */
  float obj_flash;           /* the objective was just hit */
  v3 obj;                    /* the objective's place */
  bool obj_done;             /* TIMER: the stop is struck */
  int carrier;               /* RETRIEVE: foe carrying the bike, -1 on the ground, -2 taken back */
  float take_t, loose_t;     /* RETRIEVE: standing on it; foes grabbing it */
  int carry_hits;
  float score, target, style, style_t, shown;   /* SCORE */
  uint32_t used; int ranks;  /* SCORE: variety in the last few seconds; the rank reached */
  int step; float step_n, step_t;   /* TUTORIAL */
  float sweep, sweep_on;     /* the laser sweep's angle, and how strongly it is on */
  float train_t; int train_st; float train_x;   /* RETRIEVE: the light rail */
  float haz_t; int nstrike; v3 strike[4]; float strike_t;   /* boss hazards: marks on the floor that erupt */
  int nfrost; v3 frost[6]; float frost_r[6];
  float warn_t;              /* a HUD warning flash */
  const char *warn, *fail;   /* the warning line, the loss card */
  float sam_t; v3 sam_to; int sam_st;   /* PROTECT: Sam's cower and run */
  bool won_obj;              /* the objective itself ended it (bike taken, high score) */
} twist_t;

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
  /* v1.1 */
  twist_t tw;
  /* barks: who is talking mid fight */
  uint32_t bark_said; int bark_cur, bark_pend; float bark_t, bark_pend_t, bark_len;
  /* assists */
  fighter_t ally; bool ally_on; int ally_i, ally_next; float ally_t, ally_cd, ally_flash;
  bool ally_vis; v3 ally_was; float ally_was_yaw;
  /* the finisher: -1 off */
  float fin_t; int fin_target; bool fin_done, fin_ok, cine_fin;
  /* the intro's shots and the smash card */
  float card_t; bool quick;
  /* results */
  int max_combo, parries, perfect_dodges, specials; uint32_t moves_used;
  float time_fight, result_t; int checkpoint;
  float mod_rate;
} fight_state_t;
extern fight_state_t FG;

/* the screen position of a world point in 1280x720: false when behind the camera */
bool fight_project(v3 p, float *x, float *y);
void fight_hud_init(void);
void fight_hud_reset(void);
const char *fight_twist_label(void);   /* the objective's HUD line, or NULL */
#endif
