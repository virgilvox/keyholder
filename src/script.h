/* script.h: the shape of the generated day (tools/script/compile.mjs writes gen/script.c). */
#ifndef KH_SCRIPT_H
#define KH_SCRIPT_H
#include "ds.h"
#include "actor.h"
#include "audio.h"
#include "world.h"
#include "fight.h"
#include "director.h"
#ifdef KH_GEN_ENUM
#include KH_GEN_ENUM      /* a private compiled script: -DKH_GEN_ENUM='"../build-x/gen/script_enum.h"' */
#else
#include "gen/script_enum.h"
#endif

typedef enum { MK_NONE, MK_LAB, MK_PLACE, MK_STAGE } mark_kind;
typedef struct { mark_kind kind; const char *name; float x, z, yaw; } sc_mark;
typedef enum { FX_FADE_IN, FX_FADE_OUT, FX_FLASH, FX_SLOWMO, FX_SHAKE, FX_LIGHTS_OUT } fx_kind;

typedef struct {
  int op, who, who2, i, i2, flag, target;
  float f, f2;
  const char *s, *s2;
  sc_mark mark, mark2;
  int subj[3];
  /* v1.1: OP_ROAM: tasks [t0, t0+nt) in TASKS, need of them done before the place shows (place NULL: done when
   * need are done), banter [k0, k0+nk) in BANTER; OP_SHOT: part; OP_ROAM until: the clock runs to f3 as tasks finish */
  int t0, nt, need, k0, nk;
  float f3;
} sc_beat;
/* a hosting task inside a roam. kind: TK_GREET TK_POINT TK_ANSWER TK_CARRY TK_FIX TK_SIGNIN TK_ERRAND.
 * start: beats before the minigame (or -1), done: beats after it succeeds (or -1); both end in OP_RET. */
typedef struct {
  const char *name; int kind, mode, who; sc_mark mark, mark2; const char *prompt, *label;
  int n; int stat[2], stat_n[2]; int start, done; bool optional; int requires; float delay;
} sc_task;
/* ambient lines while walking: who says text when the roam has run delay seconds (or when near mark, if set) */
typedef struct { int who; const char *text; float delay; sc_mark mark; } sc_banter;
typedef struct { const char *name; int who; sc_mark mark; const char *prompt; int start; } sc_side;
typedef struct { const char *id, *name; actor_model model; actor_palette pal; float scale; uint32_t color; } sc_cast;
typedef struct { int who, weapon, ai; float hp, power, speed, x, z; const char *palette, *name; } sc_foe;
typedef struct { int on, who; const char *text; } sc_bark;
typedef struct { int who, flag; float trust; } sc_assist;
typedef struct { const char *title, *sub, *where; float radius; int count; sc_foe foes[4];
  /* v1.1 */
  int twist; float twist_a, twist_b; float obj_x, obj_z; const char *obj_label; int protect;   /* protect: a who, or -1 */
  int nbark; sc_bark barks[10]; int nassist; sc_assist assists[3];
  float phase2_at; bool finisher_qte; int tier;   /* tier 1..5: the Battle Zone ladder's order */
} sc_fight;
typedef struct { int flag; const char *title; const char *lines[4]; } sc_ending;
typedef struct { int flag, not_flag; const char *text; } sc_statline;

extern const sc_beat SCRIPT[]; extern const int SCRIPT_COUNT;
extern const sc_side SIDES[];
extern const sc_task TASKS[];
extern const sc_banter BANTER[];
extern const sc_cast CAST[WHO_COUNT];
extern const sc_fight FIGHT_TABLE[FI_COUNT];
extern const sc_ending ENDINGS[];
extern const sc_statline STAT_LINES[];
extern const char *const WHO_ID[WHO_COUNT];
extern const char *const FLAG_NAMES[FLAG_COUNT];
#endif
