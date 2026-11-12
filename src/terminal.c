/* terminal.c: the attic server's green-CRT "pizza box" terminal. A boot log streams in, then a menu of
 * hacking programs; each program is a Johnny-Mnemonic node-trace run: step a cursor along the links from
 * the start node to the target before the countdown runs out. Drawn full screen over the frame. See
 * terminal.h. Guarded like attic.c so test builds that stub the world still link, but kept in the
 * KH_TEST_TERMINAL harness. */
#if !defined(KH_TEST_WORLD) && !defined(KH_TEST_FIGHT)
#include "terminal.h"
#include "hud_int.h"
#include "input.h"
#include "audio.h"
#include "ds.h"
#include "cv.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

/* ---- palette ---- */
#define BG     0x001208u     /* phosphor black-green */
#define GRN    0x33ff66u     /* the body text */
#define DIM    0x1f8a44u     /* dim links, idle nodes */
#define AMBER  0xffcc33u
#define RED    0xff5a4au
#define PAPER  0xd8ffe4u

/* ---- the node graphs ---- */
#define NMAX 14
typedef struct {
  const char *name, *desc;
  unsigned bit, grants;        /* bit: which unlock flag makes it available; grants: set on a clear */
  uint32_t accent;
  int n, start, target, ne;
  float secs;
  const float *px, *py;        /* node positions, 0..1 in the play rect */
  const int (*edges)[2];
} prog_t;

/* prog 0: TRACE.RUN - breach the door controller */
static const float X0[] = {.08f,.34f,.62f,.90f,.20f,.50f,.80f,.40f,.90f};
static const float Y0[] = {.20f,.08f,.14f,.22f,.52f,.46f,.56f,.86f,.86f};
static const int E0[][2] = {{0,1},{1,2},{2,3},{0,4},{1,5},{3,6},{4,5},{5,6},{4,7},{5,7},{6,8},{7,8},{5,8}};
/* prog 1: CIPHER.LOCK - crack the members vault */
static const float X1[] = {.06f,.24f,.24f,.44f,.44f,.62f,.62f,.70f,.86f,.86f,.95f};
static const float Y1[] = {.50f,.18f,.82f,.40f,.64f,.12f,.88f,.50f,.28f,.72f,.50f};
static const int E1[][2] = {{0,1},{0,2},{1,3},{2,4},{3,4},{3,5},{4,6},{3,7},{4,7},{5,8},{6,9},{7,8},{7,9},{8,10},{9,10}};
/* prog 2: WATCHDOG.KILL - silence the alarm */
static const float X2[] = {.06f,.30f,.54f,.80f,.94f,.16f,.42f,.68f,.90f,.10f,.38f,.64f,.90f};
static const float Y2[] = {.14f,.06f,.12f,.08f,.30f,.40f,.36f,.40f,.54f,.70f,.74f,.70f,.88f};
static const int E2[][2] = {{0,1},{1,2},{2,3},{3,4},{0,5},{1,6},{2,6},{3,7},{4,8},{5,6},{6,7},{7,8},{5,9},{6,10},{7,11},{8,12},{9,10},{10,11},{11,12}};

static const prog_t PROG[] = {
  {"TRACE.RUN",   "breach the door controller", 1u<<0, 1u<<1, GRN,   9, 0, 8, 13, 18.f, X0, Y0, E0},
  {"CIPHER.LOCK", "crack the members vault",    1u<<1, 1u<<2, 0x66ffccu, 11, 0, 10, 15, 22.f, X1, Y1, E1},
  {"WATCHDOG.KILL","silence the door alarm",    1u<<2, 0u,    AMBER, 13, 0, 12, 19, 16.f, X2, Y2, E2},
};
#define NPROG ((int)(sizeof PROG / sizeof PROG[0]))

static uint16_t ADJ[NPROG][NMAX];   /* symmetric adjacency, built at init */

/* ---- state ---- */
static bool inited;
static unsigned g_unlocked, g_cleared;
static enum { TP_BOOT, TP_MENU, TP_HACK, TP_RESULT } phase;
static float phase_t;
static int sel;                 /* menu row */
/* the run */
static int h_prog, h_cur, h_hov;
static uint16_t h_claimed;
static int h_path[48], h_pathn;
static float h_clock;
static bool h_won;

/* ---- boot log ---- */
static const char *BOOT[] = {
  "HEATSYNC OPS // PIZZA-BOX NODE",
  "booting services ...",
  "members: UP  wiki: UP  door-ctl: UP",
  "intrusion countermeasures: ARMED",
  "> run a program, or LOG OFF",
};
#define NBOOT ((int)(sizeof BOOT / sizeof BOOT[0]))
#define BOOT_CPS 46.f
#define BOOT_GAP 3           /* idle "chars" between lines */
static float boot_chars(void) {    /* total reveal units */
  float t = 0; for (int i = 0; i < NBOOT; i++) t += (float)strlen(BOOT[i]) + BOOT_GAP; return t;
}

void term_init(void) {
  if (inited) return;
  inited = true;
  for (int p = 0; p < NPROG; p++) {
    memset(ADJ[p], 0, sizeof ADJ[p]);
    for (int e = 0; e < PROG[p].ne; e++) {
      int a = PROG[p].edges[e][0], b = PROG[p].edges[e][1];
      ADJ[p][a] |= (uint16_t)(1u << b);
      ADJ[p][b] |= (uint16_t)(1u << a);
    }
  }
}

void term_open(unsigned unlocked) {
  g_unlocked = unlocked; g_cleared = 0;
  phase = TP_BOOT; phase_t = 0;
  sel = 0;
  for (int i = 0; i < NPROG; i++) if (g_unlocked & PROG[i].bit) { sel = i; break; }
}

unsigned term_unlocked(void) { return g_unlocked; }

/* the neighbour of cur that best faces the target: the default hover after a claim */
static int toward_target(const prog_t *P, int cur) {
  float tx = P->px[P->target] - P->px[cur], ty = P->py[P->target] - P->py[cur];
  float tl = hypotf(tx, ty); if (tl > 1e-5f) { tx /= tl; ty /= tl; }
  int best = cur; float bd = -2;
  for (int nb = 0; nb < P->n; nb++) if (ADJ[h_prog][cur] & (1u << nb)) {
    float dx = P->px[nb] - P->px[cur], dy = P->py[nb] - P->py[cur], l = hypotf(dx, dy);
    if (l < 1e-5f) continue; dx /= l; dy /= l;
    float dot = dx * tx + dy * ty;
    if (dot > bd) { bd = dot; best = nb; }
  }
  return best;
}
/* the neighbour nearest the pressed direction, or -1 if none leans that way */
static int pick_dir(const prog_t *P, int cur, int ix, int iy) {
  int best = -1; float bd = 0.30f;
  for (int nb = 0; nb < P->n; nb++) if (ADJ[h_prog][cur] & (1u << nb)) {
    float dx = P->px[nb] - P->px[cur], dy = P->py[nb] - P->py[cur], l = hypotf(dx, dy);
    if (l < 1e-5f) continue; dx /= l; dy /= l;
    float dot = dx * ix + dy * iy;
    if (dot > bd) { bd = dot; best = nb; }
  }
  return best;
}

static void launch(int prog) {
  const prog_t *P = &PROG[prog];
  h_prog = prog; h_cur = P->start; h_claimed = (uint16_t)(1u << P->start);
  h_path[0] = P->start; h_pathn = 1;
  h_clock = P->secs; h_hov = toward_target(P, P->start);
  phase = TP_HACK; phase_t = 0;
  sfx(SFX_UI_OK, 0, .9f);
}

bool term_update(float dt, const intent_t *in) {
  phase_t += dt;
  if (phase == TP_BOOT) {
    float total = boot_chars(), need = total / BOOT_CPS;
    float shown = phase_t * BOOT_CPS;
    if ((in->ok || in->interact) && shown < total) { phase_t = need; return true; }  /* skip the typing */
    if (shown >= total && (in->ok || in->interact || phase_t > need + 0.8f)) { phase = TP_MENU; phase_t = 0; sfx(SFX_UI_OK, 0, .7f); }
    return true;
  }
  if (phase == TP_MENU) {
    if (in->nav_y) { int d = in->nav_y > 0 ? 1 : -1; sel = (sel + d + NPROG) % NPROG; sfx(SFX_UI, 0, .8f); }
    if (in->ok) {
      if (g_unlocked & PROG[sel].bit) launch(sel);
      else sfx(SFX_UI, 0, .5f);   /* locked: a dull tick */
    }
    if (in->back) { sfx(SFX_UI, 0, .6f); return false; }   /* log off */
    return true;
  }
  if (phase == TP_HACK) {
    const prog_t *P = &PROG[h_prog];
    h_clock -= dt;
    if (h_clock <= 0) { h_won = false; phase = TP_RESULT; phase_t = 0; sfx(SFX_ALARM, 0, .7f); return true; }
    if (in->nav_x || in->nav_y) {
      int ix = in->nav_x > 0 ? 1 : in->nav_x < 0 ? -1 : 0;
      int iy = in->nav_y > 0 ? 1 : in->nav_y < 0 ? -1 : 0;
      int nb = pick_dir(P, h_cur, ix, iy);
      if (nb >= 0 && nb != h_hov) { h_hov = nb; sfx(SFX_UI, 0, .7f); }
    }
    if (in->ok && h_hov >= 0 && (ADJ[h_prog][h_cur] & (1u << h_hov))) {
      h_cur = h_hov; h_claimed |= (uint16_t)(1u << h_cur);
      if (h_pathn < (int)(sizeof h_path / sizeof h_path[0])) h_path[h_pathn++] = h_cur;
      sfx(SFX_UI_OK, 0, .9f);
      if (h_cur == P->target) {
        h_won = true; g_cleared |= PROG[h_prog].bit; g_unlocked |= PROG[h_prog].grants;
        phase = TP_RESULT; phase_t = 0; sfx(SFX_PICKUP, 0, 1);
      } else h_hov = toward_target(P, h_cur);
    }
    if (in->back) { phase = TP_MENU; phase_t = 0; sfx(SFX_UI, 0, .6f); }   /* abort to the menu, not log off */
    return true;
  }
  /* TP_RESULT */
  if (phase_t > 0.9f && (in->ok || in->interact || in->back || phase_t > 4.5f)) { phase = TP_MENU; phase_t = 0; }
  return true;
}

/* ---- drawing ---- */
#define GX 348.f
#define GY 184.f
#define GW 584.f
#define GH 392.f
#define NR 15.f
static float nx_px(const prog_t *P, int i) { return GX + P->px[i] * GW; }
static float ny_px(const prog_t *P, int i) { return GY + P->py[i] * GH; }

static layer_t L_boot[NBOOT], L_hdr, L_menu, L_graph, L_path, L_hud, L_ban;

/* the fixed header strip, top-left */
static void draw_header(const char *crumb) {
  cv *c = layer_begin(&L_hdr, 760, 46, hash_str(crumb, 2));
  if (c) {
    p_text(c, F_MONO, 22, 1, CV_LEFT, "HEATSYNC OPS", 2, 28, GRN, 1);
    p_text(c, F_MONO, 15, 1, CV_LEFT, crumb, 230, 27, DIM, 1);
    layer_end(&L_hdr, c);
  }
  hg_rect(40, 42, 1200, 2, DIM, .6f);
  hg_tex(&L_hdr, 44, 28, 1);
}

static void draw_boot(void) {
  const float X = 120, Y = 150, LH = 34, PX = 22, CW = PX * .6f + 1;
  int shown = (int)(phase_t * BOOT_CPS), consumed = 0, typing = -1;
  for (int i = 0; i < NBOOT; i++) {
    int len = (int)strlen(BOOT[i]);
    cv *c = layer_begin(&L_boot[i], (int)(len * CW) + 10, (int)(PX + 12), hash_str(BOOT[i], (uint32_t)i + 1));
    if (c) { p_text(c, F_MONO, PX, 1, CV_LEFT, BOOT[i], 2, PX + 2, i == 0 ? PAPER : GRN, 1); layer_end(&L_boot[i], c); }
    int vis = shown - consumed; if (vis < 0) vis = 0; if (vis > len) vis = len;
    if (vis > 0) hg_sub(&L_boot[i], 0, 0, (int)(vis * CW) + 4, (int)(PX + 12), X, Y + i * LH, 1);
    if (vis < len && typing < 0) typing = i;   /* the first unfinished line carries the cursor */
    consumed += len + BOOT_GAP;
  }
  /* a block cursor at the write head, blinking */
  int line = typing < 0 ? NBOOT - 1 : typing;
  if (fmodf((float)ds_now(), 0.9f) < 0.5f) {
    int before = 0; for (int i = 0; i < line; i++) before += (int)strlen(BOOT[i]) + BOOT_GAP;
    int col = shown - before, ll = (int)strlen(BOOT[line]); col = col < 0 ? 0 : col > ll ? ll : col;
    hg_rect(X + col * CW + 3, Y + line * LH + 3, CW, PX, GRN, .85f);
  }
}

static void draw_menu(void) {
  draw_header("// PROGRAMS");
  const float X = 150, Y = 180, RW = 640, RH = 72;
  uint32_t key = (uint32_t)sel * 131u + g_unlocked * 7u + g_cleared;
  cv *c = layer_begin(&L_menu, (int)RW, (int)(NPROG * RH + 12), key);
  if (c) {
    for (int i = 0; i < NPROG; i++) {
      const prog_t *P = &PROG[i];
      bool open = (g_unlocked & P->bit) != 0, s = i == sel, done = (g_cleared & P->bit) != 0;
      float y = i * RH;
      if (s) { cv_fill_hex(c, open ? P->accent : DIM, open ? .22f : .12f); cv_fill_rect(c, 0, y, RW, RH - 10); }
      cv_fill_hex(c, s ? (open ? P->accent : DIM) : DIM, 1); cv_fill_rect(c, 0, y, 5, RH - 10);
      uint32_t fg = open ? (s ? PAPER : GRN) : DIM;
      char name[48]; snprintf(name, sizeof name, "%s", open ? P->name : "[LOCKED]");
      p_text(c, F_MONO, 26, 1, CV_LEFT, name, 22, y + 32, fg, 1);
      p_text(c, F_MONO, 15, 0, CV_LEFT, open ? P->desc : "clear the program above to open", 24, y + 56, open ? DIM : 0x155c30, 1);
      if (done) p_text(c, F_MONO, 14, 1, CV_RIGHT, "CLEARED", RW - 16, y + 32, P->accent, 1);
      else if (open) p_text(c, F_MONO, 14, 1, CV_RIGHT, "READY", RW - 16, y + 32, s ? P->accent : DIM, 1);
    }
    layer_end(&L_menu, c);
  }
  hg_tex(&L_menu, X, Y, 1);
  /* the hint bar */
  cv *h = layer_begin(&L_hud, 900, 34, hash_str("menuhint", input_get()->used_pad));
  if (h) { g_markup(h, 0, 24, CV_LEFT, F_MONO, 17, 1, DIM, 26, input_get()->used_pad, "{UPDOWN} SELECT   {PICK} RUN   {BACK} LOG OFF", true); layer_end(&L_hud, h); }
  hg_rect(40, 672, 1200, 2, DIM, .5f);
  hg_tex(&L_hud, 120, 680, 1);
}

static void draw_hack(void) {
  const prog_t *P = &PROG[h_prog];
  char crumb[48]; snprintf(crumb, sizeof crumb, "// %s", P->name);
  draw_header(crumb);
  /* the panel behind the grid */
  hg_rect(GX - 24, GY - 58, GW + 48, GH + 92, 0x002413, .55f);
  hg_rect(GX - 24, GY - 58, GW + 48, 2, P->accent, .5f);
  /* the countdown */
  float frac = ds_clamp(h_clock / P->secs, 0, 1);
  bool low = h_clock < 5.f;
  float bw = GW, bx = GX, by = GY - 44;
  hg_rect(bx, by, bw, 16, 0x06200f, 1);
  hg_rect(bx, by, bw * frac, 16, low && fmodf((float)ds_now(), .4f) < .2f ? RED : P->accent, 1);
  /* the static links and idle node rings (rastered once per program) */
  cv *c = layer_begin(&L_graph, (int)GW, (int)GH, 0xA00u + (uint32_t)h_prog);
  if (c) {
    cv_line_cap(c, CV_CAP_ROUND); cv_line_width(c, 3);
    for (int e = 0; e < P->ne; e++) {
      int a = P->edges[e][0], b = P->edges[e][1];
      cv_stroke_hex(c, DIM, .9f);
      cv_begin(c); cv_move(c, P->px[a] * GW, P->py[a] * GH); cv_line(c, P->px[b] * GW, P->py[b] * GH); cv_stroke(c);
    }
    for (int i = 0; i < P->n; i++) {
      float x = P->px[i] * GW, y = P->py[i] * GH;
      cv_fill_hex(c, BG, 1); cv_begin(c); cv_arc(c, x, y, NR, 0, 6.2832f, false); cv_fill(c);
      cv_line_width(c, 3); cv_stroke_hex(c, DIM, 1); cv_begin(c); cv_arc(c, x, y, NR, 0, 6.2832f, false); cv_stroke(c);
      if (i == P->target) { cv_line_width(c, 2); cv_stroke_hex(c, AMBER, 1); cv_begin(c); cv_arc(c, x, y, NR + 6, 0, 6.2832f, false); cv_stroke(c); }
    }
    layer_end(&L_graph, c);
  }
  hg_tex(&L_graph, GX, GY, 1);
  /* the traced path, bright over the dim links (re-rastered only when the path grows) */
  cv *pc = layer_begin(&L_path, (int)GW, (int)GH, (uint32_t)h_prog * 977u + (uint32_t)h_pathn * 7u + (uint32_t)h_cur);
  if (pc && h_pathn > 1) {
    cv_line_cap(pc, CV_CAP_ROUND); cv_line_join(pc, CV_JOIN_ROUND); cv_line_width(pc, 5); cv_stroke_hex(pc, P->accent, 1);
    cv_begin(pc); cv_move(pc, P->px[h_path[0]] * GW, P->py[h_path[0]] * GH);
    for (int i = 1; i < h_pathn; i++) cv_line(pc, P->px[h_path[i]] * GW, P->py[h_path[i]] * GH);
    cv_stroke(pc);
    layer_end(&L_path, pc);
  } else if (pc) layer_end(&L_path, pc);
  if (h_pathn > 1) hg_tex(&L_path, GX, GY, 1);
  /* the live node states on top */
  bool blink = fmodf((float)ds_now(), 0.7f) < 0.42f;
  for (int i = 0; i < P->n; i++) {
    float x = nx_px(P, i), y = ny_px(P, i);
    if (i == h_hov) hg_circle(x, y, NR + 7, P->accent, blink ? .8f : .3f);
    if (h_claimed & (1u << i)) hg_circle(x, y, NR - 3, P->accent, .95f);
    if (i == P->target) hg_circle(x, y, NR - 6, AMBER, blink ? 1 : .5f);
    if (i == h_cur) hg_circle(x, y, NR - 4, PAPER, 1);
  }
  /* the hud line: time left and the goal */
  char s[80]; snprintf(s, sizeof s, "T-%04.1f   LINK TO TARGET", h_clock < 0 ? 0 : h_clock);
  cv *hh = layer_begin(&L_hud, 900, 34, hash_str(s, 51 + input_get()->used_pad));
  if (hh) {
    p_text(hh, F_MONO, 18, 1, CV_LEFT, s, 0, 24, low ? RED : GRN, 1);
    g_markup(hh, 900, 24, CV_RIGHT, F_MONO, 16, 1, DIM, 24, input_get()->used_pad, "{UPDOWN}{LEFTRIGHT} STEP  {PICK} JACK IN  {BACK} ABORT", true);
    layer_end(&L_hud, hh);
  }
  hg_tex(&L_hud, GX - 20, 644, 1);
}

static void draw_result(void) {
  const prog_t *P = &PROG[h_prog];   /* the hack grid behind keeps the header; the banner lands over it */
  float a = hud_ease_out(phase_t * 3);
  hg_rect(0, 286, DS_W, 150, h_won ? 0x062414 : 0x240a08, .7f * a);
  hg_rect(0, 286, DS_W, 3, h_won ? P->accent : RED, a);
  hg_rect(0, 433, DS_W, 3, h_won ? P->accent : RED, a);
  cv *c = layer_begin(&L_ban, 980, 150, hash_str(P->name, h_won ? 7u : 8u));
  if (c) {
    p_text(c, F_DISPLAY, 58, 2, CV_CENTER, h_won ? "ACCESS GRANTED" : "TRACE LOST", 490, 66, h_won ? P->accent : RED, 1);
    char sub[96];
    if (h_won) snprintf(sub, sizeof sub, "%s cleared. %s", P->name, P->grants ? "new program unlocked." : "the node is yours.");
    else snprintf(sub, sizeof sub, "the trace timed out. the controller reset you.");
    p_text(c, F_MONO, 20, 0, CV_CENTER, sub, 490, 104, PAPER, 1);
    g_markup(c, 490, 134, CV_CENTER, F_MONO, 16, 1, DIM, 20, input_get()->used_pad, "{PICK} back to programs", true);
    layer_end(&L_ban, c);
  }
  hg_tex(&L_ban, 150, 296, a);
}

void term_draw(void) {
  hg_begin();
  hg_rect(0, 0, DS_W, DS_H, BG, 1);               /* the phosphor field */
  hg_vgrad(0, 120, 0x0b3a1e, .35f, 0);            /* a faint top bloom */
  switch (phase) {
    case TP_BOOT:   draw_boot(); break;
    case TP_MENU:   draw_menu(); break;
    case TP_HACK:   draw_hack(); break;
    case TP_RESULT: draw_hack(); draw_result(); break;   /* the grid stays behind the banner */
  }
  /* the CRT overlays, last so they sit on top */
  hg_stripes(0, 0, DS_W, DS_H, 0x000000, .10f, 0, 0, 6, (float)ds_now() * 8);
  hg_scan(.22f);
  hg_vgrad(0, 60, 0x031a0d, .55f, 0);
  hg_vgrad(660, 720, 0x031a0d, 0, .55f);
  hg_vignette(0x000000, .55f, .35f);
  hg_end();
}
#endif
