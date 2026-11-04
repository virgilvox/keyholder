/* script.h: the shape of the generated day (tools/script/compile.mjs writes gen/script.c). */
#ifndef KH_SCRIPT_H
#define KH_SCRIPT_H
#include "ds.h"
#include "actor.h"
#include "audio.h"
#include "world.h"
#include "fight.h"
#include "director.h"
#include "gen/script_enum.h"

typedef enum { MK_NONE, MK_LAB, MK_PLACE, MK_STAGE } mark_kind;
typedef struct { mark_kind kind; const char *name; float x, z, yaw; } sc_mark;
typedef enum { FX_FADE_IN, FX_FADE_OUT, FX_FLASH, FX_SLOWMO, FX_SHAKE, FX_LIGHTS_OUT } fx_kind;

typedef struct {
  int op, who, who2, i, i2, flag, target;
  float f, f2;
  const char *s, *s2;
  sc_mark mark, mark2;
  int subj[3];
} sc_beat;
typedef struct { const char *name; int who; sc_mark mark; const char *prompt; int start; } sc_side;
typedef struct { const char *id, *name; actor_model model; actor_palette pal; float scale; uint32_t color; } sc_cast;
typedef struct { int who, weapon, ai; float hp, power, speed, x, z; const char *palette, *name; } sc_foe;
typedef struct { const char *title, *sub, *where; float radius; int count; sc_foe foes[4]; } sc_fight;
typedef struct { int flag; const char *title; const char *lines[4]; } sc_ending;
typedef struct { int flag, not_flag; const char *text; } sc_statline;

extern const sc_beat SCRIPT[]; extern const int SCRIPT_COUNT;
extern const sc_side SIDES[];
extern const sc_cast CAST[WHO_COUNT];
extern const sc_fight FIGHT_TABLE[FI_COUNT];
extern const sc_ending ENDINGS[];
extern const sc_statline STAT_LINES[];
extern const char *const WHO_ID[WHO_COUNT];
extern const char *const FLAG_NAMES[FLAG_COUNT];
#endif
