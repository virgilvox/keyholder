/* terminal.c: the shell of the attic server `doorman`. A green-phosphor CRT: a streaming boot log, a
 * program menu, and the four terminal games behind the term_game interface, unlocked one after another
 * (GLITCH -> WIRETAP -> CORE -> DAEMON WAR). This file owns the boot/menu, the unlock chain and the shared
 * draw helpers (header, footer, panels, banners, how-to cards); each game is its own file. See terminal.h
 * for the unlock-mask bits, and term_game.h for the game interface. Guarded like attic.c so test builds
 * that stub the world still link. */
#if !defined(KH_TEST_WORLD) && !defined(KH_TEST_FIGHT)
#include "terminal.h"
#include "term_game.h"
#include "hud_int.h"
#include "audio.h"
#include "ds.h"
#include "cv.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

static const term_game *GAMES[4] = { &TG_GLITCH, &TG_WIRETAP, &TG_CORE, &TG_DAEMON };
#define NGAME 4

static bool inited;
static unsigned g_unlocked;
static enum { TP_BOOT, TP_MENU, TP_GAME } phase;
static float phase_t;
static int sel;
static uint32_t launch_ctr;

/* ---- boot log ---- */
static const char *BOOT[] = {
  "doorman console  //  HeatSync Labs door node",
  "POST ok.  cpu: 1  mem: fine  disk: noisy",
  "mounting /wiki ................ UP",
  "starting lockd (door lock daemon) ... ARMED",
  "  doors relock nightly at 22:00.",
  "intrusion countermeasures: ONLINE",
  "> pick a program, or LOG OFF",
};
#define NBOOT ((int)(sizeof BOOT / sizeof BOOT[0]))
#define BOOT_CPS 52.f
#define BOOT_GAP 3
static float boot_chars(void) { float t = 0; for (int i = 0; i < NBOOT; i++) t += (float)strlen(BOOT[i]) + BOOT_GAP; return t; }

void term_init(void) { if (inited) return; inited = true; }

void term_open(unsigned unlocked) {
  g_unlocked = unlocked | TERM_AV_GLITCH;        /* GLITCH is always available */
  phase = TP_BOOT; phase_t = 0; sel = 0;
}
unsigned term_unlocked(void) { return g_unlocked; }

static void launch(int i) {
  uint32_t seed = (uint32_t)(ds_now() * 997.0) + (++launch_ctr) * 2654435761u + (uint32_t)i * 40503u;
  GAMES[i]->start(seed ? seed : 1);
  phase = TP_GAME; phase_t = 0;
  sfx(SFX_UI_OK, 0, .9f);
}

bool term_update(float dt, const intent_t *in) {
  phase_t += dt;
  if (phase == TP_BOOT) {
    float total = boot_chars(), need = total / BOOT_CPS, shown = phase_t * BOOT_CPS;
    if ((in->ok || in->interact) && shown < total) { phase_t = need; return true; }
    if (shown >= total && (in->ok || in->interact || phase_t > need + 0.8f)) { phase = TP_MENU; phase_t = 0; sfx(SFX_UI_OK, 0, .7f); }
    return true;
  }
  if (phase == TP_MENU) {
    if (in->nav_y) { sel = (sel + (in->nav_y > 0 ? 1 : -1) + NGAME) % NGAME; sfx(SFX_UI, 0, .8f); }
    if (in->ok) {
      if (g_unlocked & (1u << sel)) launch(sel);
      else sfx(SFX_UI, 0, .45f);                  /* locked: a dull tick */
    }
    if (in->back) { sfx(SFX_UI, 0, .6f); return false; }   /* log off */
    return true;
  }
  /* TP_GAME */
  int r = GAMES[sel]->update(dt, in);
  if (r == TG_WIN) {
    g_unlocked |= (1u << (4 + sel));              /* cleared */
    if (sel + 1 < NGAME) g_unlocked |= (1u << (sel + 1));   /* open the next */
    phase = TP_MENU; phase_t = 0; sfx(SFX_PICKUP, 0, 1);
  } else if (r == TG_QUIT) { phase = TP_MENU; phase_t = 0; sfx(SFX_UI, 0, .6f); }
  return true;
}

/* ================= shared draw helpers ================= */
void tg_header(const char *crumb) {
  static layer_t L;
  cv *c = layer_begin(&L, 900, 44, hash_str(crumb, 2));
  if (c) {
    p_text(c, F_MONO, 22, 1, CV_LEFT, "doorman", 2, 28, TG_GRN, 1);
    p_text(c, F_MONO, 15, 1, CV_LEFT, crumb, 150, 27, TG_DIM, 1);
    layer_end(&L, c);
  }
  hg_rect(40, 42, 1200, 2, TG_DIM, .6f);
  hg_tex(&L, 44, 24, 1);
}
void tg_footer(const char *markup) {
  static layer_t L;
  bool pad = input_get()->used_pad;
  cv *c = layer_begin(&L, 1170, 32, hash_str(markup, 900 + pad));
  if (c) { g_markup(c, 0, 24, CV_LEFT, F_MONO, 16, 1, TG_DIM, 24, pad, markup, true); layer_end(&L, c); }
  hg_rect(40, 668, 1200, 2, TG_DIM, .5f);
  hg_tex(&L, 56, 680, 1);
}
void tg_panel(float x, float y, float w, float h, uint32_t accent) {
  hg_rect(x, y, w, h, TG_PANEL, .55f);
  hg_rect(x, y, w, 2, accent, .5f);
}
void tg_pips(float x, float y, int round, int total, uint32_t accent) {
  for (int i = 0; i < total; i++) hg_circle(x + i * 22, y + 8, 6, i <= round ? accent : TG_DIM, i <= round ? 1 : .4f);
}
void tg_wrap(cv *c, const char *s, float x, float y, float w, float lh, float px, uint32_t col) {
  char line[220]; int n = 0; line[0] = 0; const char *p = s;
  cv_font(c, F_MONO, px); cv_letter_spacing(c, 0);
  while (*p) {
    char word[72]; int k = 0; while (*p && *p != ' ' && k < 71) word[k++] = *p++; word[k] = 0; while (*p == ' ') p++;
    char trial[300]; snprintf(trial, sizeof trial, n ? "%s %s" : "%s%s", line, word);
    if (n && cv_measure(c, trial) > w) { p_text(c, F_MONO, px, 0, CV_LEFT, line, x, y, col, 1); y += lh; snprintf(line, sizeof line, "%s", word); }
    else snprintf(line, sizeof line, "%s", trial);
    n++;
  }
  if (line[0]) p_text(c, F_MONO, px, 0, CV_LEFT, line, x, y, col, 1);
}
void tg_banner(bool win, const char *title, const char *sub, float t) {
  static layer_t L;
  float a = hud_ease_out(t * 3);
  uint32_t ac = win ? TG_GRN : TG_RED;
  hg_rect(0, 286, DS_W, 150, win ? 0x062414 : 0x240a08, .72f * a);
  hg_rect(0, 286, DS_W, 3, ac, a);
  hg_rect(0, 433, DS_W, 3, ac, a);
  bool pad = input_get()->used_pad;
  cv *c = layer_begin(&L, 1000, 150, hash_str(title, hash_str(sub, (win ? 7u : 8u) + (unsigned)pad * 2)));
  if (c) {
    p_text(c, F_DISPLAY, 52, 2, CV_CENTER, title, 500, 62, ac, 1);
    p_text(c, F_MONO, 19, 0, CV_CENTER, sub, 500, 98, TG_PAPER, 1);
    g_markup(c, 500, 132, CV_CENTER, F_MONO, 16, 1, TG_DIM, 20, pad, "{PICK} continue", true);
    layer_end(&L, c);
  }
  hg_tex(&L, 140, 296, a);
}
void tg_howto(const char *title, const char *const *lines, int n, uint32_t accent) {
  static layer_t L;
  bool pad = input_get()->used_pad;
  hg_rect(60, 118, 1160, 522, TG_PANEL, .6f);
  hg_rect(60, 118, 1160, 3, accent, .6f);
  uint32_t key = hash_str(title, (uint32_t)n * 7u + (unsigned)pad);
  for (int i = 0; i < n; i++) key = hash_str(lines[i], key);
  cv *c = layer_begin(&L, 1120, 504, key);
  if (c) {
    p_text(c, F_DISPLAY, 30, 1, CV_LEFT, title, 20, 50, accent, 1);
    float y = 110;
    for (int i = 0; i < n; i++) { if (lines[i][0]) g_markup(c, 20, y, CV_LEFT, F_MONO, 18, 0, TG_GRN, 22, pad, lines[i], true); y += 34; }
    g_markup(c, 20, 476, CV_LEFT, F_MONO, 16, 1, TG_AMBER, 24, pad, "{PICK} begin     {BACK} back to menu", true);
    layer_end(&L, c);
  }
  hg_tex(&L, 80, 128, 1);
}

/* ================= boot + menu drawing ================= */
static layer_t L_boot[NBOOT], L_menu, L_mhint;

static void draw_boot(void) {
  const float X = 120, Y = 150, LH = 40, PX = 22, CW = PX * .6f + 1;
  int shown = (int)(phase_t * BOOT_CPS), consumed = 0, typing = -1;
  for (int i = 0; i < NBOOT; i++) {
    int len = (int)strlen(BOOT[i]);
    cv *c = layer_begin(&L_boot[i], (int)(len * CW) + 10, (int)(PX + 12), hash_str(BOOT[i], (uint32_t)i + 1));
    if (c) { p_text(c, F_MONO, PX, 1, CV_LEFT, BOOT[i], 2, PX + 2, i == 0 ? TG_PAPER : TG_GRN, 1); layer_end(&L_boot[i], c); }
    int vis = shown - consumed; if (vis < 0) vis = 0; if (vis > len) vis = len;
    if (vis > 0) hg_sub(&L_boot[i], 0, 0, (int)(vis * CW) + 4, (int)(PX + 12), X, Y + i * LH, 1);
    if (vis < len && typing < 0) typing = i;
    consumed += len + BOOT_GAP;
  }
  int line = typing < 0 ? NBOOT - 1 : typing;
  if (fmodf((float)ds_now(), 0.9f) < 0.5f) {
    int before = 0; for (int i = 0; i < line; i++) before += (int)strlen(BOOT[i]) + BOOT_GAP;
    int col = shown - before, ll = (int)strlen(BOOT[line]); col = col < 0 ? 0 : col > ll ? ll : col;
    hg_rect(X + col * CW + 3, Y + line * LH + 3, CW, PX, TG_GRN, .85f);
  }
}

static void draw_menu(void) {
  tg_header("// PROGRAMS");
  const float X = 150, Y = 170, RW = 820, RH = 92;
  uint32_t key = (uint32_t)sel * 131u + g_unlocked * 7u;
  cv *c = layer_begin(&L_menu, (int)RW, (int)(NGAME * RH + 12), key);
  if (c) {
    for (int i = 0; i < NGAME; i++) {
      bool open = (g_unlocked & (1u << i)) != 0, s = i == sel, done = (g_unlocked & (1u << (4 + i))) != 0;
      float y = i * RH;
      if (s) { cv_fill_hex(c, open ? TG_GRN : TG_DIM, open ? .20f : .10f); cv_fill_rect(c, 0, y, RW, RH - 12); }
      cv_fill_hex(c, s ? (open ? TG_GRN : TG_DIM) : TG_DIM, 1); cv_fill_rect(c, 0, y, 5, RH - 12);
      uint32_t fg = open ? (s ? TG_PAPER : TG_GRN) : TG_DIM;
      char name[40]; snprintf(name, sizeof name, "%d. %s", i + 1, open ? GAMES[i]->name : "[LOCKED]");
      p_text(c, F_DISPLAY, 28, 1, CV_LEFT, name, 22, y + 40, fg, 1);
      p_text(c, F_MONO, 16, 0, CV_LEFT, open ? GAMES[i]->desc : "clear the program above to open this one", 24, y + 68, open ? TG_DIM : 0x155c30, 1);
      if (done) p_text(c, F_MONO, 15, 1, CV_RIGHT, "CLEARED", RW - 18, y + 40, TG_AMBER, 1);
      else if (open) p_text(c, F_MONO, 15, 1, CV_RIGHT, "READY", RW - 18, y + 40, s ? TG_GRN : TG_DIM, 1);
    }
    layer_end(&L_menu, c);
  }
  hg_tex(&L_menu, X, Y, 1);
  bool pad = input_get()->used_pad;
  cv *h = layer_begin(&L_mhint, 900, 32, hash_str("mh", (unsigned)pad));
  if (h) { g_markup(h, 0, 24, CV_LEFT, F_MONO, 17, 1, TG_DIM, 26, pad, "{UPDOWN} SELECT   {PICK} RUN   {BACK} LOG OFF", true); layer_end(&L_mhint, h); }
  hg_rect(40, 668, 1200, 2, TG_DIM, .5f);
  hg_tex(&L_mhint, 150, 680, 1);
}

void term_draw(void) {
  hg_begin();
  hg_rect(0, 0, DS_W, DS_H, TG_BG, 1);
  hg_vgrad(0, 120, 0x0b3a1e, .35f, 0);
  switch (phase) {
    case TP_BOOT: draw_boot(); break;
    case TP_MENU: draw_menu(); break;
    case TP_GAME: GAMES[sel]->draw(); break;
  }
  /* CRT overlays on top */
  hg_stripes(0, 0, DS_W, DS_H, 0x000000, .10f, 0, 0, 6, (float)ds_now() * 8);
  hg_scan(.20f);
  hg_vgrad(0, 60, 0x031a0d, .55f, 0);
  hg_vgrad(660, 720, 0x031a0d, 0, .55f);
  hg_vignette(0x000000, .52f, .35f);
  hg_end();
}

#ifdef KH_TEST_TERMINAL
int term_phase(void) { return (int)phase; }   /* 0 boot, 1 menu, 2 in a game */
int term_sel(void) { return sel; }
#endif
#endif
