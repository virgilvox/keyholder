/* term_game.h: the small interface every terminal game implements, plus shared bits (palette, the content
 * rect, a local RNG, held-direction auto-repeat, and shell draw helpers). terminal.c is the shell; each
 * game is its own file exposing one `const term_game`.
 *
 * Drawing contract: a game's draw() runs BETWEEN the shell's hg_begin and hg_end, over the phosphor field,
 * under the CRT overlays. A game must NOT call hg_begin/hg_end; it just paints with hg_* and layers. Never
 * fold time into a layer key; animate with cheap hg primitives. Button prompts must be device aware: draw
 * them with g_markup(..., input_get()->used_pad, ...) using {PICK}/{BACK}/{HEAVY}/... tokens, and fold
 * used_pad into the layer key.
 */
#ifndef KH_TERM_GAME_H
#define KH_TERM_GAME_H
#include "ds.h"
#include "input.h"
#include "hud_int.h"
#include <math.h>

/* a game's update returns one of these */
enum { TG_RUN = 0, TG_WIN = 1, TG_QUIT = 2 };

typedef struct term_game {
  const char *name;   /* menu label, e.g. "GLITCH" */
  const char *desc;   /* one line under it */
  void (*start)(uint32_t seed);                     /* fresh run from round one */
  int  (*update)(float dt, const intent_t *in);     /* TG_RUN / TG_WIN / TG_QUIT */
  void (*draw)(void);
} term_game;

extern const term_game TG_GLITCH;
extern const term_game TG_WIRETAP;
extern const term_game TG_CORE;
extern const term_game TG_DAEMON;

/* ---- palette (the whole terminal shares it) ---- */
#define TG_BG    0x001208u   /* phosphor black-green */
#define TG_GRN   0x33ff66u   /* body text */
#define TG_DIM   0x1f8a44u   /* dim lines, idle */
#define TG_AMBER 0xffcc33u
#define TG_RED   0xff5a4au
#define TG_PAPER 0xd8ffe4u
#define TG_CYAN  0x66ffccu
#define TG_PANEL 0x002413u

/* ---- the content rect, between the header strip and the footer ---- */
#define TG_CX0 60.f
#define TG_CY0 118.f
#define TG_CX1 1220.f
#define TG_CY1 650.f

/* ---- a small local RNG so a run's randomness does not disturb the world RNG ---- */
typedef struct { uint32_t s; } trng;
static inline void tr_seed(trng *r, uint32_t s) { r->s = s ? s : 0x1234567u; }
static inline uint32_t tr_u32(trng *r) { uint32_t x = r->s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return r->s = x ? x : 0x9e3779b9u; }
static inline int tr_int(trng *r, int lo, int hi) { if (hi <= lo) return lo; return lo + (int)(tr_u32(r) % (uint32_t)(hi - lo + 1)); }
static inline float tr_f(trng *r) { return (float)(tr_u32(r) & 0xffffff) / (float)0x1000000; }
static inline float tr_rf(trng *r, float a, float b) { return a + (b - a) * tr_f(r); }

/* ---- held-direction auto-repeat: a fresh press steps once, then repeats while held ---- */
typedef struct { int dir; float t; } trepeat;
static inline int tr_step(trepeat *r, int dir, float dt) {
  if (dir == 0) { r->dir = 0; r->t = 0; return 0; }
  if (dir != r->dir) { r->dir = dir; r->t = 0; return dir; }
  r->t += dt;
  if (r->t >= 0.13f) { r->t = 0; return dir; }
  return 0;
}
/* combined d-pad / stick / wasd / arrows direction (x: +1 right, y_up: +1 up) */
static inline int tg_ix(const intent_t *in) {
  if (in->nav_x) return in->nav_x > 0 ? 1 : -1;
  if (in->mx > 0.4f || in->ax > 0.4f) return 1;
  if (in->mx < -0.4f || in->ax < -0.4f) return -1;
  return 0;
}
static inline int tg_iy_up(const intent_t *in) {   /* +1 = up, -1 = down */
  if (in->nav_y) return in->nav_y > 0 ? -1 : 1;
  if (in->my > 0.4f || in->ay > 0.4f) return 1;
  if (in->my < -0.4f || in->ay < -0.4f) return -1;
  return 0;
}

/* ---- shell draw helpers (terminal.c) a game calls from its draw() ---- */
void tg_header(const char *crumb);                 /* the top strip: HEATSYNC OPS // crumb */
void tg_footer(const char *markup);                /* the bottom hint bar; markup has {PICK}/... tokens */
void tg_panel(float x, float y, float w, float h, uint32_t accent);   /* a framed sub-panel */
void tg_pips(float x, float y, int round, int total, uint32_t accent); /* round-of-total dots */
void tg_banner(bool win, const char *title, const char *sub, float t); /* centred win/fail banner */
void tg_howto(const char *title, const char *const *lines, int n, uint32_t accent); /* a how-to card */
void tg_wrap(cv *c, const char *s, float x, float y, float w, float lh, float px, uint32_t col);  /* F_MONO word wrap */

#endif
