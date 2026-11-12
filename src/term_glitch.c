/* term_glitch.c: voltage fault injection. The door chip boots and checks a password in a loop. The screen
 * shows an oscilloscope trace of its power draw: a setup ramp, the compare loop as repeating humps, and a
 * tall branch spike at the end. The player positions a glitch (offset along the trace, width, power) and
 * fires. Too weak does nothing. Adequate power at the wrong hump or too much power browns the chip out and
 * costs an attempt; a brown-out dents the trace near where it hit, more the closer it was, so the player
 * triangulates. The right hump at the right width with adequate power skips the check. Later rounds narrow
 * the window and add a decoy hump and display jitter. Randomised per run. See term_game.h. */
#if !defined(KH_TEST_WORLD) && !defined(KH_TEST_FIGHT)
#include "term_game.h"
#include "terminal.h"
#include "audio.h"
#include "cv.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* sub-states */
enum { G_HOWTO, G_PLAY, G_RWIN, G_RFAIL, G_WIN };

#define HUMPS_MAX 10
#define POW_N 6
#define FIRES_MAX 6

static trng rng;
static int sub, rnd;
static float t;                 /* sub-state timer (animation only) */

/* the trace model (per round) */
static int nhumps;
static float hump_x[HUMPS_MAX]; /* centres, 0..1 across the scope */
static float hump_w[HUMPS_MAX]; /* widths, in x units */
static float hump_h[HUMPS_MAX]; /* heights 0..1 */
static int target;              /* the vulnerable hump */
static int decoy;               /* a tempting but wrong hump, or -1 */
static bool jitter;
static float tol_x, tol_w;      /* win tolerances */
static int pow_lo, pow_hi;      /* adequate power band (level indices); above pow_hi browns out */
static int attempts, attempts_max;
static uint32_t jseed;          /* fixed jitter seed so the trace is steady between fires */

/* the player's glitch */
static float g_off, g_w;        /* 0..1 */
static int g_pow;               /* 0..POW_N-1 */

/* fire feedback for triangulation */
static float fire_x[FIRES_MAX], fire_mag[FIRES_MAX];
static int nfire, firecount;
static int last_result;         /* 0 none, 1 brownout, 2 win */

static trepeat repx, repy;

/* player width (0..1) -> x-units */
static float wx_of(float gw) { return 0.010f + gw * 0.026f; }

static void gen_round(void) {
  nhumps = rnd == 0 ? 6 : rnd == 1 ? 8 : 9;
  float x0 = 0.16f, x1 = 0.80f;
  for (int i = 0; i < nhumps; i++) {
    hump_x[i] = x0 + (x1 - x0) * (float)i / (float)(nhumps - 1);
    hump_w[i] = tr_rf(&rng, 0.013f, 0.030f);
    hump_h[i] = tr_rf(&rng, 0.34f, 0.50f);
  }
  target = tr_int(&rng, 1, nhumps - 2);          /* never the first or last hump */
  decoy = -1;
  if (rnd >= 1) {                              /* a taller decoy somewhere else */
    do { decoy = tr_int(&rng, 1, nhumps - 2); } while (decoy == target);
    hump_h[decoy] = 0.62f;
  }
  jitter = rnd >= 2;
  jseed = tr_u32(&rng) | 1u;
  tol_x = rnd == 0 ? 0.045f : rnd == 1 ? 0.034f : 0.028f;
  tol_w = rnd == 0 ? 0.0055f : rnd == 1 ? 0.0040f : 0.0030f;
  pow_lo = 2; pow_hi = rnd == 2 ? 3 : 4;       /* round 3: only one adequate level */
  attempts_max = rnd == 0 ? 6 : rnd == 1 ? 5 : 4;
  attempts = attempts_max;
  g_off = 0.08f; g_w = 0.5f; g_pow = 0;   /* power starts OFF: you must arm it with {HEAVY}, so mashing fire does nothing */
  nfire = firecount = 0; last_result = 0;
}

static void glitch_start(uint32_t seed) {
  tr_seed(&rng, seed);
  rnd = 0; sub = G_HOWTO; t = 0;
  gen_round();
}

/* fire the glitch; returns the result code */
static int do_fire(void) {
  float depth = (float)g_pow;
  if (depth < (float)pow_lo - 0.5f) return 0;    /* too weak: nothing, no cost */
  firecount++; attempts--;
  float dx = fabsf(g_off - hump_x[target]);
  float dw = fabsf(wx_of(g_w) - hump_w[target]);
  bool pow_ok = g_pow >= pow_lo && g_pow <= pow_hi;
  if (pow_ok && dx < tol_x && dw < tol_w) return 2;   /* skipped the check */
  /* brown-out: record where it landed, strength by closeness in offset */
  if (nfire < FIRES_MAX) {
    float mag = 1.f - dx / 0.34f; if (mag < 0) mag = 0;
    fire_x[nfire] = g_off; fire_mag[nfire] = mag; nfire++;
  }
  return 1;
}

static int glitch_update(float dt, const intent_t *in) {
  t += dt;
  if (sub == G_HOWTO) {
    if (in->back) return TG_QUIT;
    if (in->ok) { sub = G_PLAY; t = 0; }
    return TG_RUN;
  }
  if (sub == G_RWIN) {
    if (in->back) return TG_QUIT;
    if (t > 0.6f && in->ok) {
      rnd++;
      if (rnd >= 3) { sub = G_WIN; t = 0; }
      else { gen_round(); sub = G_PLAY; t = 0; }
    }
    return TG_RUN;
  }
  if (sub == G_RFAIL) {                            /* out of attempts: retry the same round */
    if (t > 0.6f && in->ok) { gen_round(); sub = G_PLAY; t = 0; }
    if (in->back) return TG_QUIT;
    return TG_RUN;
  }
  if (sub == G_WIN) {
    if (t > 0.6f && in->ok) return TG_WIN;
    return TG_RUN;
  }
  /* G_PLAY */
  if (in->back) return TG_QUIT;
  int sx = tr_step(&repx, tg_ix(in), dt);
  if (sx) { g_off = ds_clamp(g_off + sx * 0.012f, 0.06f, 0.94f); sfx(SFX_UI, 0, .5f); }
  int sy = tr_step(&repy, tg_iy_up(in), dt);
  if (sy) { g_w = ds_clamp(g_w + sy * 0.03f, 0.f, 1.f); sfx(SFX_UI, 0, .5f); }
  if (in->heavy) { g_pow = (g_pow + 1) % POW_N; sfx(SFX_UI, 0, .7f); }   /* cycle power */
  if (in->ok) {
    last_result = do_fire();
    if (last_result == 2) { sfx(SFX_PICKUP, 0, 1); sub = G_RWIN; t = 0; }
    else if (last_result == 1) {
      sfx(SFX_SHOCK, 0, .8f);
      if (attempts <= 0) { sub = G_RFAIL; t = 0; }
    } else sfx(SFX_UI, 0, .35f);                    /* weak: a dull tick */
  }
  return TG_RUN;
}

/* ---- drawing ---- */
#define SX 90.f
#define SY 150.f
#define SW 1100.f
#define SH 352.f
#define BASE (SY + SH)

static layer_t L_trace, L_read, L_info;

/* the power-draw value at x in 0..1 (humps + setup + branch + brown-out dents) */
static float trace_v(float x) {
  float v = 0.12f;
  /* setup ramp then a short plateau */
  if (x < 0.13f) { float s = x / 0.13f; v += 0.30f * (s < 0.7f ? s / 0.7f : 1.f); }
  for (int i = 0; i < nhumps; i++) {
    float d = (x - hump_x[i]) / hump_w[i];
    v += hump_h[i] * expf(-d * d);
  }
  float db = (x - 0.90f) / 0.009f; v += 0.66f * expf(-db * db);   /* the branch spike */
  for (int i = 0; i < nfire; i++) {
    float d = (x - fire_x[i]) / 0.020f;
    v -= fire_mag[i] * 0.55f * expf(-d * d);                      /* a dent where a shot hit */
  }
  if (jitter) {                                                   /* a little display noise */
    uint32_t h = jseed ^ (uint32_t)(x * 4096.f);
    h ^= h << 13; h ^= h >> 17; h ^= h << 5;
    v += ((float)(h & 1023) / 1023.f - 0.5f) * 0.05f;
  }
  return v < 0 ? 0 : v > 1.15f ? 1.15f : v;
}

static void draw_scope(void) {
  tg_panel(SX - 26, SY - 20, SW + 52, SH + 54, TG_GRN);
  /* the grid */
  for (int i = 0; i <= 10; i++) hg_rect(SX + SW * i / 10.f, SY, 1, SH, TG_DIM, .22f);
  for (int i = 0; i <= 4; i++) hg_rect(SX, SY + SH * i / 4.f, SW, 1, TG_DIM, .22f);
  /* the trace, re-rastered only when a shot lands (key = round + firecount) */
  cv *c = layer_begin(&L_trace, (int)SW, (int)(SH + 4), 0x6100u + (uint32_t)rnd * 17u + (uint32_t)firecount);
  if (c) {
    cv_line_cap(c, CV_CAP_ROUND); cv_line_join(c, CV_JOIN_ROUND);
    cv_line_width(c, 2.4f); cv_stroke_hex(c, TG_GRN, .95f);
    cv_begin(c);
    for (int px = 0; px <= (int)SW; px += 2) {
      float x = (float)px / SW;
      float y = SH - trace_v(x) * (SH * 0.82f);
      if (px == 0) cv_move(c, (float)px, y); else cv_line(c, (float)px, y);
    }
    cv_stroke(c);
    layer_end(&L_trace, c);
  }
  hg_tex(&L_trace, SX, SY, 1);
}

static void draw_cursor(void) {
  float cx = SX + g_off * SW;
  float half = (wx_of(g_w) / 1.f) * SW * 0.5f;
  bool blink = fmodf((float)ds_now(), 0.6f) < 0.4f;
  uint32_t col = g_pow < pow_lo ? TG_DIM : g_pow > pow_hi ? TG_RED : TG_CYAN;
  /* the glitch window: a bright band of the chosen width at the chosen offset */
  hg_rect(cx - half, SY, half * 2, SH, col, 0.14f);
  hg_rect(cx - half, SY, 2, SH, col, blink ? .9f : .5f);
  hg_rect(cx + half, SY, 2, SH, col, blink ? .9f : .5f);
  hg_rect(cx - 1, SY - 14, 3, SH + 24, col, .9f);
  /* little width brackets at the baseline */
  hg_rect(cx - half, BASE + 6, half * 2, 3, col, .8f);
}

static void draw_info(void) {
  bool pad = input_get()->used_pad;
  /* attempts + power + readouts, across the bottom band */
  char s[96];
  snprintf(s, sizeof s, "ATTEMPTS %d/%d", attempts, attempts_max);
  /* the key folds in the live knobs so the offset/width readout keeps up (not time) */
  uint32_t rkey = (uint32_t)(g_pow * 37 + attempts * 7 + rnd * 131) + (uint32_t)(g_off * 400) * 17u + (uint32_t)(g_w * 200) * 101u;
  cv *c = layer_begin(&L_read, 760, 110, rkey);
  if (c) {
    p_text(c, F_MONO, 20, 1, CV_LEFT, s, 2, 24, attempts <= 1 ? TG_RED : TG_GRN, 1);
    /* the power dial */
    p_text(c, F_MONO, 16, 1, CV_LEFT, "POWER", 2, 58, TG_DIM, 1);
    for (int i = 0; i < POW_N; i++) {
      bool on = i <= g_pow;
      bool band = i >= pow_lo && i <= pow_hi;
      uint32_t col = !on ? 0x0b3a1e : band ? TG_CYAN : TG_AMBER;
      cv_fill_hex(c, col, on ? 1 : .5f); cv_fill_rect(c, 96 + i * 30, 44, 24, 16);
    }
    const char *pw = g_pow < pow_lo ? "too weak" : g_pow > pow_hi ? "BROWN-OUT RISK" : "armed";
    p_text(c, F_MONO, 15, 0, CV_LEFT, pw, 96 + POW_N * 30 + 12, 58, g_pow > pow_hi ? TG_RED : TG_DIM, 1);
    snprintf(s, sizeof s, "OFFSET %.2f   WIDTH %.2f", g_off, g_w);
    p_text(c, F_MONO, 16, 1, CV_LEFT, s, 2, 92, TG_DIM, 1);
    layer_end(&L_read, c);
  }
  hg_tex(&L_read, SX, 524, 1);
  /* the goal note + last result */
  const char *msg = last_result == 1 ? "BROWN-OUT. the trace dented where it hit: aim for the deepest dent." :
                    "find the vulnerable compare hump. match the window width. keep power in the cyan band.";
  cv *g = layer_begin(&L_info, 580, 70, hash_str(msg, 400 + pad));
  if (g) {
    p_text(g, F_MONO, 15, 0, CV_LEFT, "FAULT INJECTION", 0, 18, TG_AMBER, 1);
    tg_wrap(g, msg, 0, 44, 560, 20, 15, TG_GRN);
    layer_end(&L_info, g);
  }
  hg_tex(&L_info, 628, 530, 1);
  tg_pips(1120, 540, rnd, 3, TG_GRN);
  tg_footer("{LEFTRIGHT} OFFSET   {UPDOWN} WIDTH   {HEAVY} POWER   {PICK} FIRE   {BACK} QUIT");
}

static void glitch_draw(void) {
  tg_header("// GLITCH : door chip");
  if (sub == G_HOWTO) {
    static const char *L[] = {
      "The door chip checks the keypad code in a tight loop. Each compare shows on the scope as a hump;",
      "the tall spike at the end is the branch that accepts or rejects.",
      "",
      "Slip a voltage glitch into the right compare and the chip skips the check.",
      "{LEFTRIGHT} slides the glitch along the trace. {UPDOWN} sets its width. {HEAVY} cycles power.",
      "{PICK} fires. Too weak and nothing happens. Wrong place or too much power browns the chip out",
      "and costs an attempt, but it dents the trace where it hit so you can triangulate.",
      "Three rounds: the window narrows, a decoy hump appears, then the trace jitters.",
    };
    tg_howto("GLITCH  //  voltage fault injection", L, (int)(sizeof L / sizeof *L), TG_GRN);
    return;
  }
  draw_scope();
  if (sub == G_PLAY || sub == G_RWIN || sub == G_RFAIL) draw_cursor();
  if (sub == G_PLAY) draw_info();
  if (sub == G_RWIN)  tg_banner(true,  rnd + 1 >= 3 ? "CHECK SKIPPED" : "CHECK SKIPPED", "the chip accepted it. next round is tighter.", t);
  if (sub == G_RFAIL) tg_banner(false, "CHIP LOCKED OUT", "too many brown-outs. the chip reset. {PICK} to try the round again.", t);
  if (sub == G_WIN)   tg_banner(true,  "DOOR CHIP OWNED", "all three checks skipped. WIRETAP unlocked.", t);
}

const term_game TG_GLITCH = { "GLITCH", "voltage-fault the door chip", glitch_start, glitch_update, glitch_draw };

#ifdef KH_TEST_TERMINAL
/* test bot hook: the current round's winning glitch, so the harness can steer real intents to it */
void glitch_solution(float *off, float *w, int *powlo, int *powhi, int *sub_out) {
  if (off) *off = hump_x[target];
  if (w) { /* invert wx_of to a 0..1 width that matches the target hump */
    float gw = (hump_w[target] - 0.010f) / 0.026f; *w = ds_clamp(gw, 0.f, 1.f);
  }
  if (powlo) *powlo = pow_lo;
  if (powhi) *powhi = pow_hi;
  if (sub_out) *sub_out = sub;
}
float glitch_cur_off(void) { return g_off; }
float glitch_cur_w(void) { return g_w; }
int glitch_cur_pow(void) { return g_pow; }
#endif
#endif
