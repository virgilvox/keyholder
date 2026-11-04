/* fight.h: real time third person combat. One player against one to four fighters in
 * any stage, with anime weapons. game.c starts a fight, calls fight_update and
 * fight_draw_hud each frame until it ends, then goes back to the story. */
#ifndef KH_FIGHT_H
#define KH_FIGHT_H
#include "ds.h"
#include "actor.h"
#include "input.h"
#include "world.h"

typedef enum { WP_ARC, WP_MAUL, WP_TWIN, WP_SPEAR, WP_STAFF, WP_COUNT } weapon_id;
/* WP_ARC   Arc Blade: a light sword, plasma off a welding handle. Fast, the start weapon.
 * WP_MAUL  Ball Peen Maul: a great hammer with a fantasy head. Slow, breaks guards.
 * WP_TWIN  Deburr Twins: paired curved daggers. Fastest, weakest, long combos.
 * WP_SPEAR Spool Naginata: a long glaive wound from printer filament. Reach and sweeps.
 * WP_STAFF Beam Staff: a laser tube staff. Ranged bolts, a charged lance. */
const char *weapon_name(weapon_id w);
/* a weapon prop on an actor's right hand (off with -1), also for cutscenes */
void weapon_equip(actor_t *a, int w);
void weapon_draw_flash(actor_t *a);            /* the anime summon: a burst as it appears */

typedef enum { AI_DUELIST, AI_BRUISER, AI_SKIRMISH, AI_CASTER, AI_BOSS } ai_kind;
typedef struct {
  actor_model model; actor_palette pal; float scale;
  const char *name;
  weapon_id weapon;
  ai_kind ai;
  float hp;            /* the player has 100 */
  float power;         /* damage scale, 1 = normal */
  float speed;         /* move and attack speed, 1 = normal */
  float place_x, place_z;   /* start, relative to the fight's centre */
  actor_t *actor;      /* optional: a story actor to use as it stands (palette, place, any weapon already
                        * equipped). NULL takes one from the fight's pool. A story actor is left visible, on
                        * one knee, after the fight: fight_end neither hides nor frees it. */
} fighter_def;

typedef enum { FIGHT_RUNNING, FIGHT_WON, FIGHT_LOST } fight_state;
typedef struct {
  const char *title, *sub;     /* the opening card, e.g. "CONFLICT RESOLUTION" / "versus Vinyl Vic" */
  int count;
  fighter_def foes[4];
  v3 center; float radius;     /* the ring; fighters are pushed back in at the edge in the open */
  float player_hp;             /* 0..1, what the player walks in with (their energy today) */
  float foe_scale;             /* difficulty: hp and power scale from the story's stats, 1 = normal */
  unsigned weapons;            /* bit per weapon_id: what the player owns */
  int start_weapon;
  bool tutorial;               /* show the controls helper; the player stops at 1 hp, and the fight ends as won
                                * when the foes' hp or the player's hp runs out */
  bool easy;                   /* foes' hp and power scaled by 0.8 */
} fight_def;

/* the player's actor comes from game.c. Takes over the camera and the player until it ends. */
void fight_init(t3_scene *scene, t3_camera *cam);
void fight_start(const fight_def *def, actor_t *player);
fight_state fight_update(float dt, const intent_t *in);
void fight_draw_hud(void);                    /* cv/hud: bars, names, combo, the card, prompts */
float fight_player_hp(void);                  /* 0..1 after the fight */
void fight_end(void);                         /* restores: hides the foes, puts the player's weapon away */
/* for the story: a foe's actor after the fight (they get up and talk) */
actor_t *fight_foe_actor(int i);

/* player.h is the third person walk: see player.h */
#endif
