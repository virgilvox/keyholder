/* term_wiretap.c: sniff the badge reader on the old door controller. Captured swipes come in as Wiegand
 * bit strings: a leading parity bit, a facility code, a card number, a trailing parity bit. From several
 * sniffed cards the player works out the field layout and the parity scheme, then builds the bits for a
 * target card (shown in decimal on a badge), computes both parity bits, and replays it before the 22:00
 * relock clock runs out. A wrong replay costs time and the reader answers PARITY, FACILITY or UNKNOWN CARD.
 * Rounds: 26 bit with the format shown, 34 bit, then a round where the facility code must be read off the
 * sniffed cards. Randomised per run. See term_game.h. */
#if !defined(KH_TEST_WORLD) && !defined(KH_TEST_FIGHT)
#include "term_game.h"
#include "terminal.h"
#include "audio.h"
#include "cv.h"
#include <string.h>
#include <stdio.h>

enum { W_HOWTO, W_PLAY, W_RWIN, W_RFAIL, W_WIN };
#define NBITS_MAX 34
#define NSNIFF 3

static trng rng;
static int sub, rnd;
static float t;

/* layout */
static int fcbits, cnbits, nbits;
static int datalo, datahi, half;   /* data span and the parity split */
static bool hide_fc;               /* round 3: the target facility is not printed */
static int tgt_fc, tgt_cn;
static int sniff_fc[NSNIFF], sniff_cn[NSNIFF];

/* the player's card */
static int bit[NBITS_MAX];
static int cur;                    /* cursor over bit cells */
static float clock_s;              /* seconds until relock */
static int last_hint;              /* 0 none, 1 parity, 2 facility, 3 unknown, 4 granted */
static float hint_t;
static trepeat repx;

static int fc_start(void) { return 1; }
static int cn_start(void) { return 1 + fcbits; }

static int get_field(int start, int n) { int v = 0; for (int i = 0; i < n; i++) v = (v << 1) | bit[start + i]; return v; }
static void set_field(int start, int n, int v) { for (int i = 0; i < n; i++) bit[start + i] = (v >> (n - 1 - i)) & 1; }
static int xor_span(int lo, int hi) { int x = 0; for (int i = lo; i <= hi; i++) x ^= bit[i]; return x; }
static bool lead_ok(void) { return bit[0] == xor_span(datalo, half); }           /* even parity */
static bool trail_ok(void) { return bit[nbits - 1] == (xor_span(half + 1, datahi) ^ 1); } /* odd parity */

static void layout_for_round(void) {
  if (rnd == 1) { fcbits = 16; cnbits = 16; }
  else { fcbits = 8; cnbits = 16; }
  nbits = 1 + fcbits + cnbits + 1;
  datalo = 1; datahi = nbits - 2; half = (datalo + datahi) / 2;
  hide_fc = (rnd == 2);
}

static void gen_round(void) {
  layout_for_round();
  int fcmax = (1 << fcbits) - 1;
  int cnmax = (1 << cnbits) - 1;
  tgt_fc = tr_int(&rng, 1, fcmax - 1);
  tgt_cn = tr_int(&rng, 1, cnmax - 1);
  for (int i = 0; i < NSNIFF; i++) { sniff_fc[i] = tgt_fc; sniff_cn[i] = tr_int(&rng, 1, cnmax - 1); }
  /* start the editor blank */
  for (int i = 0; i < nbits; i++) bit[i] = 0;
  cur = cn_start();
  clock_s = rnd == 0 ? 60.f : 55.f;
  last_hint = 0; hint_t = 0;
}

static void wiretap_start(uint32_t seed) {
  tr_seed(&rng, seed);
  rnd = 0; sub = W_HOWTO; t = 0;
  gen_round();
}

static void do_send(void) {
  int fc = get_field(fc_start(), fcbits), cn = get_field(cn_start(), cnbits);
  bool par = lead_ok() && trail_ok();
  if (!par) last_hint = 1;                       /* PARITY */
  else if (fc != tgt_fc) last_hint = 2;          /* FACILITY */
  else if (cn != tgt_cn) last_hint = 3;          /* UNKNOWN CARD */
  else last_hint = 4;                            /* GRANTED */
  hint_t = 0;
  if (last_hint == 4) { sfx(SFX_PICKUP, 0, 1); sub = W_RWIN; t = 0; return; }
  clock_s -= 8.f;                                /* a wrong replay burns time */
  sfx(SFX_SHOCK, 0, .7f);
  if (clock_s <= 0) { clock_s = 0; sub = W_RFAIL; t = 0; }
}

static int wiretap_update(float dt, const intent_t *in) {
  t += dt; hint_t += dt;
  if (sub == W_HOWTO) { if (in->back) return TG_QUIT; if (in->ok) { sub = W_PLAY; t = 0; } return TG_RUN; }
  if (sub == W_RWIN)  { if (in->back) return TG_QUIT; if (t > 0.6f && in->ok) { rnd++; if (rnd >= 3) { sub = W_WIN; t = 0; } else { gen_round(); sub = W_PLAY; t = 0; } } return TG_RUN; }
  if (sub == W_RFAIL) { if (in->back) return TG_QUIT; if (t > 0.6f && in->ok) { gen_round(); sub = W_PLAY; t = 0; } return TG_RUN; }
  if (sub == W_WIN)   { if (t > 0.6f && in->ok) return TG_WIN; return TG_RUN; }
  /* W_PLAY */
  if (in->back) return TG_QUIT;
  clock_s -= dt;
  if (clock_s <= 0) { clock_s = 0; sub = W_RFAIL; t = 0; return TG_RUN; }
  int sx = tr_step(&repx, tg_ix(in), dt);
  if (sx) { cur = (cur + sx + nbits) % nbits; sfx(SFX_UI, 0, .5f); }
  int sy = tg_iy_up(in);                          /* dial the field the cursor sits in */
  if (sy && (in->nav_y || fabsf(in->my) > 0.4f || fabsf(in->ay) > 0.4f)) {
    static float dialt; dialt += dt;
    if (in->nav_y || dialt > 0.10f) {
      dialt = 0;
      if (cur >= fc_start() && cur < fc_start() + fcbits) {
        int v = get_field(fc_start(), fcbits) + sy; int m = (1 << fcbits) - 1;
        if (v < 0) v = 0; if (v > m) v = m; set_field(fc_start(), fcbits, v); sfx(SFX_UI, 0, .5f);
      } else if (cur >= cn_start() && cur < cn_start() + cnbits) {
        int v = get_field(cn_start(), cnbits) + sy; int m = (1 << cnbits) - 1;
        if (v < 0) v = 0; if (v > m) v = m; set_field(cn_start(), cnbits, v); sfx(SFX_UI, 0, .5f);
      }
    }
  }
  if (in->ok) { bit[cur] ^= 1; sfx(SFX_UI_OK, 0, .7f); }   /* toggle the bit under the cursor */
  if (in->special) do_send();                              /* replay the card */
  return TG_RUN;
}

/* ---- drawing ---- */
static layer_t L_sniff, L_badge, L_edit, L_hint;
static uint32_t field_col(int i) {
  if (i == 0 || i == nbits - 1) return TG_AMBER;                 /* parity */
  if (i >= fc_start() && i < fc_start() + fcbits) return TG_CYAN; /* facility */
  return TG_GRN;                                                  /* card number */
}

static void bits_str(char *out, int fc_v, int cn_v, int fcb, int cnb) {
  int n = 1 + fcb + cnb + 1, k = 0;
  int tmp[NBITS_MAX]; for (int i = 0; i < n; i++) tmp[i] = 0;
  for (int i = 0; i < fcb; i++) tmp[1 + i] = (fc_v >> (fcb - 1 - i)) & 1;
  for (int i = 0; i < cnb; i++) tmp[1 + fcb + i] = (cn_v >> (cnb - 1 - i)) & 1;
  for (int i = 0; i < n; i++) { out[k++] = tmp[i] ? '1' : '0'; if (i == 0 || i == fcb || i == fcb + cnb) out[k++] = ' '; }
  out[k] = 0;
}

static void draw_sniffs(void) {
  cv *c = layer_begin(&L_sniff, 1120, 150, 0x7700u + (uint32_t)rnd * 7u);
  if (c) {
    p_text(c, F_MONO, 17, 1, CV_LEFT, "SNIFFED SWIPES  (same door, same facility)", 0, 18, TG_AMBER, 1);
    for (int s = 0; s < NSNIFF; s++) {
      char bs[NBITS_MAX + 6]; bits_str(bs, sniff_fc[s], sniff_cn[s], fcbits, cnbits);
      float y = 46 + s * 34;
      p_text(c, F_MONO, 16, 0, CV_LEFT, bs, 0, y, TG_GRN, 1);
      char dec[48]; snprintf(dec, sizeof dec, "FC %-5d CN %-6d", sniff_fc[s], sniff_cn[s]);
      p_text(c, F_MONO, 16, 1, CV_LEFT, dec, 760, y, TG_DIM, 1);
    }
    layer_end(&L_sniff, c);
  }
  hg_tex(&L_sniff, TG_CX0, 128, 1);
}

static void draw_badge(void) {
  /* the target card as a badge */
  float bx = TG_CX0, by = 300, bw = 320, bh = 150;
  tg_panel(bx, by, bw, bh, TG_CYAN);
  cv *c = layer_begin(&L_badge, (int)bw, (int)bh, hash_str(hide_fc ? "hid" : "fc", (uint32_t)(tgt_fc * 131 + tgt_cn + rnd * 7)));
  if (c) {
    p_text(c, F_MONO, 15, 1, CV_LEFT, "TARGET BADGE", 16, 28, TG_DIM, 1);
    char cn[16]; snprintf(cn, sizeof cn, "%d", tgt_cn);
    p_text(c, F_DISPLAY, 46, 1, CV_LEFT, cn, 16, 92, TG_PAPER, 1);
    p_text(c, F_MONO, 14, 1, CV_LEFT, "CARD NUMBER", 16, 112, TG_DIM, 1);
    char fc[24]; snprintf(fc, sizeof fc, "FACILITY %s", hide_fc ? "??  (read the sniffs)" : "");
    if (hide_fc) p_text(c, F_MONO, 16, 1, CV_LEFT, fc, 16, 138, TG_AMBER, 1);
    else { snprintf(fc, sizeof fc, "FACILITY %d", tgt_fc); p_text(c, F_MONO, 16, 1, CV_LEFT, fc, 16, 138, TG_CYAN, 1); }
    layer_end(&L_badge, c);
  }
  hg_tex(&L_badge, bx, by, 1);
}

static void draw_parity_panel(void) {
  float px = 420, py = 300;
  bool pad = input_get()->used_pad;
  char key[64]; snprintf(key, sizeof key, "%d%d%d%d", rnd, lead_ok(), trail_ok(), pad);
  cv *c = layer_begin(&L_hint, 420, 150, hash_str(key, 480));
  if (c) {
    p_text(c, F_MONO, 16, 1, CV_LEFT, "PARITY CHECK", 0, 18, TG_AMBER, 1);
    char l[40]; snprintf(l, sizeof l, "P1 (leading, even): %s", lead_ok() ? "OK" : "BAD");
    p_text(c, F_MONO, 15, 0, CV_LEFT, l, 0, 46, lead_ok() ? TG_GRN : TG_RED, 1);
    snprintf(l, sizeof l, "P2 (trailing, odd):  %s", trail_ok() ? "OK" : "BAD");
    p_text(c, F_MONO, 15, 0, CV_LEFT, l, 0, 70, trail_ok() ? TG_GRN : TG_RED, 1);
    if (rnd == 0) tg_wrap(c, "P1 covers the left half of the data, P2 the right half. Flip the amber end bits until both read OK.", 0, 98, 410, 20, 14, TG_DIM);
    else tg_wrap(c, "Set facility and card, then flip the amber parity bits so both read OK, and replay.", 0, 98, 410, 20, 14, TG_DIM);
    layer_end(&L_hint, c);
  }
  hg_tex(&L_hint, px, py, 1);
}

static void draw_editor(void) {
  float ex = TG_CX0, ey = 476, ew = 1100;
  float cw = ew / (float)nbits;
  if (cw > 34) cw = 34;
  float row = ex;
  /* field bands + cells + cursor + decoded values */
  char key[80];
  int bh = 0; for (int i = 0; i < nbits; i++) bh = bh * 31 + bit[i] + 1;
  snprintf(key, sizeof key, "%d.%d.%d", rnd, cur, bh);
  cv *c = layer_begin(&L_edit, (int)(nbits * (int)cw) + 8, 128, hash_str(key, 512));
  if (c) {
    for (int i = 0; i < nbits; i++) {
      float x = i * cw;
      uint32_t col = field_col(i);
      bool sel = i == cur;
      cv_fill_hex(c, col, sel ? .30f : .10f); cv_fill_rect(c, x + 1, 24, cw - 2, 40);
      char b[2] = { bit[i] ? '1' : '0', 0 };
      p_text(c, F_MONO, 20, 0, CV_CENTER, b, x + cw / 2, 52, bit[i] ? TG_PAPER : col, 1);
      if (sel) { cv_fill_hex(c, TG_PAPER, 1); cv_fill_rect(c, x + 1, 20, cw - 2, 3); cv_fill_rect(c, x + 1, 66, cw - 2, 3); }
    }
    /* labels */
    p_text(c, F_MONO, 13, 1, CV_LEFT, "P", 0, 18, TG_AMBER, 1);
    char fl[32]; snprintf(fl, sizeof fl, "FACILITY = %d", get_field(fc_start(), fcbits));
    p_text(c, F_MONO, 14, 1, CV_LEFT, fl, fc_start() * cw, 90, TG_CYAN, 1);
    char cl[32]; snprintf(cl, sizeof cl, "CARD = %d", get_field(cn_start(), cnbits));
    p_text(c, F_MONO, 14, 1, CV_LEFT, cl, cn_start() * cw, 90, TG_GRN, 1);
    p_text(c, F_MONO, 13, 1, CV_RIGHT, "P", nbits * cw, 18, TG_AMBER, 1);
    layer_end(&L_edit, c);
  }
  hg_tex(&L_edit, row, ey, 1);
}

static void draw_play(void) {
  draw_sniffs();
  draw_badge();
  draw_parity_panel();
  draw_editor();
  /* the relock clock */
  float frac = ds_clamp(clock_s / (rnd == 0 ? 60.f : 55.f), 0, 1);
  bool low = clock_s < 12.f;
  hg_rect(TG_CX0, 618, 1100, 14, 0x06200f, 1);
  hg_rect(TG_CX0, 618, 1100 * frac, 14, low && fmodf((float)ds_now(), .4f) < .2f ? TG_RED : TG_CYAN, 1);
  char ct[40]; snprintf(ct, sizeof ct, "RELOCK IN %4.1fs", clock_s);
  static layer_t L_clk;
  cv *c = layer_begin(&L_clk, 300, 26, hash_str(ct, 520));
  if (c) { p_text(c, F_MONO, 16, 1, CV_LEFT, ct, 0, 20, low ? TG_RED : TG_GRN, 1); layer_end(&L_clk, c); }
  hg_tex(&L_clk, 980, 596, 1);
  /* the reader's last answer */
  if (last_hint && last_hint != 4 && hint_t < 3.5f) {
    const char *h = last_hint == 1 ? "READER: PARITY" : last_hint == 2 ? "READER: FACILITY" : "READER: UNKNOWN CARD";
    static layer_t L_ans;
    cv *c2 = layer_begin(&L_ans, 420, 30, hash_str(h, 530));
    if (c2) { p_text(c2, F_MONO, 19, 1, CV_LEFT, h, 0, 22, TG_RED, 1); layer_end(&L_ans, c2); }
    hg_tex(&L_ans, TG_CX0, 596, ds_clamp(3.5f - hint_t, 0, 1));
  }
  tg_pips(1130, 128, rnd, 3, TG_GRN);
  tg_footer("{LEFTRIGHT} MOVE   {UPDOWN} DIAL FIELD   {PICK} FLIP BIT   {SPECIAL} REPLAY   {BACK} QUIT");
}

static void wiretap_draw(void) {
  tg_header("// WIRETAP : badge reader");
  if (sub == W_HOWTO) {
    static const char *L[] = {
      "The door reader speaks Wiegand: a leading parity bit, a facility code, a card number,",
      "then a trailing parity bit. Three sniffed swipes are shown, decoded, so you can see the layout.",
      "",
      "Build the target card: dial the facility and card number with {UPDOWN}, or flip single bits",
      "with {PICK}. Then set the two amber parity bits so both checks read OK.",
      "{SPECIAL} replays the card at the reader. A wrong replay burns time off the 22:00 relock clock;",
      "the reader answers PARITY, FACILITY or UNKNOWN CARD so you know what is wrong.",
      "Rounds: 26 bit, then 34 bit, then a card whose facility you must read off the sniffs.",
    };
    tg_howto("WIRETAP  //  replay a badge", L, (int)(sizeof L / sizeof *L), TG_CYAN);
    return;
  }
  draw_play();
  if (sub == W_RWIN)  tg_banner(true,  "ACCESS GRANTED", "the reader clicked. next round.", t);
  if (sub == W_RFAIL) tg_banner(false, "DOORS RELOCKED", "the clock ran out. {PICK} to sniff again.", t);
  if (sub == W_WIN)   tg_banner(true,  "READER DEFEATED", "all three cards replayed. CORE unlocked.", t);
}

const term_game TG_WIRETAP = { "WIRETAP", "replay the door badge reader", wiretap_start, wiretap_update, wiretap_draw };

#ifdef KH_TEST_TERMINAL
int wiretap_sub(void) { return sub; }
int wiretap_n(void) { return nbits; }
int wiretap_cursor(void) { return cur; }
void wiretap_cur_bits(int *out) { for (int i = 0; i < nbits; i++) out[i] = bit[i]; }
void wiretap_target_bits(int *out) {
  int save[NBITS_MAX]; for (int i = 0; i < nbits; i++) save[i] = bit[i];
  set_field(fc_start(), fcbits, tgt_fc); set_field(cn_start(), cnbits, tgt_cn);
  out[0] = xor_span(datalo, half);
  out[nbits - 1] = xor_span(half + 1, datahi) ^ 1;
  for (int i = 1; i < nbits - 1; i++) out[i] = bit[i];
  for (int i = 0; i < nbits; i++) bit[i] = save[i];   /* restore */
}
#endif
#endif
