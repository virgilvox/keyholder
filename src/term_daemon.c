/* term_daemon.c: Core War lite. A circular memory of 72 cells, drawn as a grid and coloured by owner. You
 * write a short warrior (up to 6 instructions) from a reduced set (DAT, MOV, ADD, JMP, JMZ with immediate #
 * and relative $ operands) and pick where it loads. It then alternates execution with the old daemon
 * `lockd`. A process that executes a DAT, or runs off into empty memory, dies. Three rounds: lockd as an
 * imp you must outlive, as a bomber you must kill, as a scanner you must beat to the trigger. A reference
 * screen and a starter warrior to modify. See term_game.h. */
#if !defined(KH_TEST_WORLD) && !defined(KH_TEST_FIGHT)
#include "term_game.h"
#include "terminal.h"
#include "audio.h"
#include "cv.h"
#include <string.h>
#include <stdio.h>

enum { D_HOWTO, D_EDIT, D_RUN, D_RWIN, D_RFAIL, D_WIN };
#define M 72
#define GCOLS 12
#define GROWS 6
#define NWL 6                 /* warrior lines the player can write */

/* ops and modes */
enum { W_EMPTY, W_DAT, W_MOV, W_ADD, W_JMP, W_JMZ, W_OPN };
static const char *WOP[W_OPN] = { "---", "DAT", "MOV", "ADD", "JMP", "JMZ" };
enum { AM_IMM, AM_REL };       /* # and $ */
typedef struct { int op, am, a, bm, b; } Cell;

static trng rng;
static int sub, rnd;
static float t;

/* the player's warrior + load */
static Cell war[NWL];
static int load_pos;
static int curl, curc;         /* cursor: curl 0 = LOAD row, 1..NWL = warrior lines; curc 0..4 fields */

/* the battle */
static Cell mem[M];
static uint8_t owner[M];       /* 0 none, 1 player, 2 daemon */
static int ppc, dpc; static bool palive, dalive;
static int d_adr;              /* the daemon's load address (shown) */
static int cyc, limit, winmode; /* winmode 0 survive, 1 kill */
static float run_acc;
static const char *d_label;

static int modm(int x) { x %= M; if (x < 0) x += M; return x; }

/* ---- daemon warriors ---- */
static void load_cells(const Cell *src, int n, int at, int own) {
  for (int i = 0; i < n; i++) { int p = modm(at + i); mem[p] = src[i]; owner[p] = (uint8_t)own; }
}
static void set_daemon(void) {
  if (rnd == 0) {               /* an imp: copies itself one cell forward every cycle */
    static const Cell imp[] = { { W_MOV, AM_REL, 0, AM_REL, 1 } };
    d_adr = 36; load_cells(imp, 1, d_adr, 2); limit = 40; winmode = 0; d_label = "IMP";
  } else if (rnd == 1) {        /* a bomber: sweeps DAT bombs forward around the ring */
    static const Cell bomb[] = { { W_MOV, AM_IMM, 0, AM_REL, 10 }, { W_ADD, AM_IMM, 1, AM_REL, -1 }, { W_JMP, AM_REL, -2, AM_REL, 0 } };
    d_adr = 34; load_cells(bomb, 3, d_adr, 2); limit = 64; winmode = 1; d_label = "BOMBER";
  } else {                        /* a scanner: sweeps faster and further */
    static const Cell scan[] = { { W_MOV, AM_IMM, 0, AM_REL, 12 }, { W_ADD, AM_IMM, 7, AM_REL, -1 }, { W_JMP, AM_REL, -2, AM_REL, 0 } };
    d_adr = 40; load_cells(scan, 3, d_adr, 2); limit = 80; winmode = 1; d_label = "SCANNER";
  }
}

/* starter warrior the player edits */
static void set_starter(void) {
  for (int i = 0; i < NWL; i++) war[i] = (Cell){ W_EMPTY, AM_REL, 0, AM_REL, 0 };
  if (rnd == 0) { war[0] = (Cell){ W_JMP, AM_REL, 1, AM_REL, 0 }; load_pos = 30; }   /* JMP $1 runs into empty: fix to $0 */
  else { war[0] = (Cell){ W_MOV, AM_IMM, 0, AM_REL, 1 }; war[1] = (Cell){ W_JMP, AM_REL, 0, AM_REL, 0 }; load_pos = modm((rnd == 1 ? 34 : 40) - 5); }
}

static void gen_round(void) {
  for (int i = 0; i < M; i++) { mem[i] = (Cell){ W_EMPTY, AM_REL, 0, AM_REL, 0 }; owner[i] = 0; }
  set_daemon();
  set_starter();
  curl = 0; curc = 0;
}

static void daemon_start(uint32_t seed) {
  tr_seed(&rng, seed);
  rnd = 0; sub = D_HOWTO; t = 0;
  gen_round();
}

/* ---- the VM ---- */
static bool step_proc(int *pc, bool *alive, int own) {
  if (!*alive) return false;
  Cell c = mem[*pc];
  if (c.op == W_EMPTY || c.op == W_DAT) { *alive = false; return false; }   /* dead memory or a bomb */
  bool jumped = false;
  int here = *pc;
  if (c.op == W_MOV) {
    int dst = modm(here + c.b);
    if (c.am == AM_IMM) { mem[dst] = (Cell){ W_DAT, AM_IMM, 0, AM_IMM, c.a }; }   /* plant a lethal DAT */
    else { int src = modm(here + c.a); mem[dst] = mem[src]; }
    owner[dst] = (uint8_t)own;
  } else if (c.op == W_ADD) {
    int dst = modm(here + c.b);
    if (c.am == AM_IMM) mem[dst].b += c.a; else { int src = modm(here + c.a); mem[dst].b += mem[src].b; }
    owner[dst] = (uint8_t)own;
  } else if (c.op == W_JMP) {
    *pc = modm(here + c.a); jumped = true;
  } else if (c.op == W_JMZ) {
    int test = modm(here + c.b);
    if (mem[test].b == 0) { *pc = modm(here + c.a); jumped = true; }
  }
  if (!jumped) *pc = modm(here + 1);
  return true;
}

static void battle_load(void) {
  for (int i = 0; i < M; i++) { mem[i] = (Cell){ W_EMPTY, AM_REL, 0, AM_REL, 0 }; owner[i] = 0; }
  set_daemon();                          /* daemon first... */
  int n = 0; for (int i = 0; i < NWL; i++) if (war[i].op != W_EMPTY) n = i + 1;
  load_cells(war, n, load_pos, 1);       /* ...player loads over it, so overlapping the daemon only hurts you */
  dalive = palive = true; dpc = d_adr; ppc = load_pos; cyc = 0; run_acc = 0;
}

/* one full cycle: player then daemon. returns 0 running, 1 player lost, 2 daemon dead, 3 limit survived */
static int battle_cycle(void) {
  step_proc(&ppc, &palive, 1);
  if (dalive) step_proc(&dpc, &dalive, 2);
  cyc++;
  if (!palive) return 1;
  if (!dalive) return 2;
  if (cyc >= limit) return winmode == 0 ? 3 : 1;   /* survive mode: timeout is a win; kill mode: a loss */
  return 0;
}

/* ---- editing ---- */
static int cells_in(int row) { return row == 0 ? 1 : 5; }   /* LOAD row has one field */
static void cycle_field(int dir) {
  if (curl == 0) { load_pos = modm(load_pos + dir); return; }
  Cell *c = &war[curl - 1];
  switch (curc) {
    case 0: c->op = (c->op + dir + W_OPN) % W_OPN; break;
    case 1: c->am = (c->am + dir + 2) % 2; break;
    case 2: { int v = c->a + dir; if (v > 36) v = -36; if (v < -36) v = 36; c->a = v; break; }
    case 3: c->bm = (c->bm + dir + 2) % 2; break;
    case 4: { int v = c->b + dir; if (v > 36) v = -36; if (v < -36) v = 36; c->b = v; break; }
  }
}

static int daemon_update(float dt, const intent_t *in) {
  t += dt;
  if (sub == D_HOWTO) { if (in->back) return TG_QUIT; if (in->ok) { sub = D_EDIT; t = 0; } return TG_RUN; }
  if (sub == D_RWIN)  { if (in->back) return TG_QUIT; if (t > 0.6f && in->ok) { rnd++; if (rnd >= 3) { sub = D_WIN; t = 0; } else { gen_round(); sub = D_EDIT; t = 0; } } return TG_RUN; }
  if (sub == D_RFAIL) { if (in->back) return TG_QUIT; if (t > 0.6f && in->ok) { gen_round(); sub = D_EDIT; t = 0; } return TG_RUN; }
  if (sub == D_WIN)   { if (t > 0.6f && in->ok) return TG_WIN; return TG_RUN; }
  if (sub == D_RUN) {
    if (in->back) { sub = D_EDIT; t = 0; return TG_RUN; }
    run_acc += dt;
    int st = 0;
    while (run_acc >= 0.045f && st == 0) { st = battle_cycle(); run_acc -= 0.045f; }
    if (st == 2 || st == 3) { sfx(SFX_PICKUP, 0, 1); sub = D_RWIN; t = 0; }
    else if (st == 1) { sfx(SFX_SHOCK, 0, .7f); sub = D_RFAIL; t = 0; }
    return TG_RUN;
  }
  /* D_EDIT */
  if (in->back) return TG_QUIT;
  int iy = tg_iy_up(in);
  if (in->nav_y) { curl = (curl - iy + (NWL + 1)) % (NWL + 1); int cc = cells_in(curl); if (curc >= cc) curc = cc - 1; sfx(SFX_UI, 0, .5f); }
  if (in->nav_x) { int cc = cells_in(curl); curc += in->nav_x > 0 ? 1 : -1; if (curc < 0) curc = 0; if (curc >= cc) curc = cc - 1; sfx(SFX_UI, 0, .5f); }
  if (in->ok) { cycle_field(+1); sfx(SFX_UI_OK, 0, .7f); }
  if (in->heavy) { cycle_field(-1); sfx(SFX_UI_OK, 0, .6f); }
  if (in->special) { battle_load(); sub = D_RUN; t = 0; sfx(SFX_UI_OK, 0, 1); }
  return TG_RUN;
}

/* ---- drawing ---- */
static layer_t L_edit, L_ref;
#define GX 70.f
#define GY 150.f
#define CW 70.f
#define CH 40.f

static void cell_label(const Cell *c, char *out, int cap) {
  if (c->op == W_EMPTY) { snprintf(out, cap, "---"); return; }
  if (c->op == W_DAT) { snprintf(out, cap, "DAT"); return; }
  char am = c->am == AM_IMM ? '#' : '$', bm = c->bm == AM_IMM ? '#' : '$';
  if (c->op == W_JMP) snprintf(out, cap, "JMP %c%d", bm == '#' ? '#' : '$', c->a);
  else if (c->op == W_JMZ) snprintf(out, cap, "JMZ %c%d %c%d", am, c->a, bm, c->b);
  else snprintf(out, cap, "%s %c%d %c%d", WOP[c->op], am, c->a, bm, c->b);
}

static void draw_grid(void) {
  tg_panel(GX - 14, GY - 30, GCOLS * CW + 28, GROWS * CH + 54, TG_GRN);
  static layer_t L_glab;
  cv *c = layer_begin(&L_glab, 400, 24, hash_str(d_label ? d_label : "", (uint32_t)rnd));
  if (c) { char s[48]; snprintf(s, sizeof s, "CORE MEMORY   lockd: %s", d_label ? d_label : ""); p_text(c, F_MONO, 16, 1, CV_LEFT, s, 0, 18, TG_AMBER, 1); layer_end(&L_glab, c); }
  hg_tex(&L_glab, GX, GY - 10, 1);
  bool run = sub == D_RUN || sub == D_RWIN || sub == D_RFAIL;
  for (int i = 0; i < M; i++) {
    float x = GX + (i % GCOLS) * CW, y = GY + 16 + (i / GCOLS) * CH;
    const Cell *mc = run ? &mem[i] : NULL;
    uint32_t col = TG_PANEL; float a = 1;
    if (run) {
      bool bomb = mem[i].op == W_DAT;
      if (bomb) col = TG_AMBER;
      else if (owner[i] == 1) col = mem[i].op == W_EMPTY ? 0x0b3a1e : TG_GRN;
      else if (owner[i] == 2) col = mem[i].op == W_EMPTY ? 0x3a1010 : TG_RED;
      else col = 0x061a0d;
    }
    hg_rect(x + 2, y + 2, CW - 4, CH - 4, col, a);
    (void)mc;
  }
  if (run) {   /* the two program counters */
    bool blink = fmodf((float)ds_now(), 0.4f) < 0.25f;
    if (palive) { float x = GX + (ppc % GCOLS) * CW, y = GY + 16 + (ppc / GCOLS) * CH; hg_rect(x + 1, y + 1, CW - 2, 3, TG_CYAN, 1); hg_rect(x + 1, y + CH - 4, CW - 2, 3, TG_CYAN, 1); }
    if (dalive) { float x = GX + (dpc % GCOLS) * CW, y = GY + 16 + (dpc / GCOLS) * CH; hg_rect(x + 1, y + 1, CW - 2, 3, blink ? TG_PAPER : TG_RED, 1); hg_rect(x + 1, y + CH - 4, CW - 2, 3, blink ? TG_PAPER : TG_RED, 1); }
    /* a cycle readout */
    static layer_t L_cy;
    char s[48]; snprintf(s, sizeof s, "CYCLE %d / %d", cyc, limit);
    cv *cc = layer_begin(&L_cy, 240, 24, hash_str(s, 700));
    if (cc) { p_text(cc, F_MONO, 16, 1, CV_LEFT, s, 0, 18, TG_GRN, 1); layer_end(&L_cy, cc); }
    hg_tex(&L_cy, GX + GCOLS * CW - 230, GY - 10, 1);
  }
}

static void draw_editor(void) {
  float ex = 70, ey = GY + GROWS * CH + 44;
  tg_panel(ex - 10, ey - 24, 1150, 182, TG_CYAN);
  char key[48]; snprintf(key, sizeof key, "%d.%d.%d", curl, curc, load_pos);
  uint32_t h = (uint32_t)rnd * 7 + 1;
  for (int i = 0; i < NWL; i++) h = h * 131 + (uint32_t)(war[i].op * 97 + war[i].am * 13 + war[i].a * 3 + war[i].bm * 5 + war[i].b);
  cv *c = layer_begin(&L_edit, 1140, 176, hash_str(key, h));
  if (c) {
    /* LOAD row */
    bool selL = curl == 0;
    if (selL) { cv_fill_hex(c, TG_PAPER, .14f); cv_fill_rect(c, 0, 0, 1140, 26); }
    char ls[32]; snprintf(ls, sizeof ls, "LOAD AT  %d", load_pos);
    p_text(c, F_MONO, 18, 1, CV_LEFT, ls, 8, 20, TG_AMBER, 1);
    p_text(c, F_MONO, 14, 0, CV_LEFT, "(pick a cell near lockd, but not on it)", 230, 20, TG_DIM, 1);
    if (selL) { cv_stroke_hex(c, TG_PAPER, 1); cv_line_width(c, 2); cv_begin(c); cv_rect(c, 86, 2, 70, 22); cv_stroke(c); }
    /* warrior lines */
    float colOp = 70, colAm = 190, colAv = 250, colBm = 360, colBv = 420;
    for (int i = 0; i < NWL; i++) {
      float y = 52 + i * 21;
      Cell *w = &war[i];
      bool sel = curl == i + 1;
      if (sel) { cv_fill_hex(c, TG_GRN, .14f); cv_fill_rect(c, 0, y - 16, 560, 20); }
      char ln[4]; snprintf(ln, sizeof ln, "%d", i);
      p_text(c, F_MONO, 16, 0, CV_LEFT, ln, 8, y, TG_DIM, 1);
      p_text(c, F_MONO, 16, 0, CV_LEFT, WOP[w->op], colOp, y, w->op == W_EMPTY ? TG_DIM : TG_PAPER, 1);
      bool args = w->op == W_MOV || w->op == W_ADD || w->op == W_JMZ;
      bool jmp = w->op == W_JMP;
      char s[8];
      if (args || jmp) {
        snprintf(s, sizeof s, "%c", w->am == AM_IMM ? '#' : '$'); p_text(c, F_MONO, 16, 0, CV_LEFT, s, colAm, y, TG_CYAN, 1);
        snprintf(s, sizeof s, "%d", w->a); p_text(c, F_MONO, 16, 0, CV_LEFT, s, colAv, y, TG_CYAN, 1);
      }
      if (args) {
        snprintf(s, sizeof s, "%c", w->bm == AM_IMM ? '#' : '$'); p_text(c, F_MONO, 16, 0, CV_LEFT, s, colBm, y, TG_AMBER, 1);
        snprintf(s, sizeof s, "%d", w->b); p_text(c, F_MONO, 16, 0, CV_LEFT, s, colBv, y, TG_AMBER, 1);
      }
      if (sel) {
        float cx[5] = { colOp, colAm, colAv, colBm, colBv };
        float cw[5] = { 54, 24, 44, 24, 44 };
        cv_stroke_hex(c, TG_PAPER, 1); cv_line_width(c, 2);
        cv_begin(c); cv_rect(c, cx[curc] - 6, y - 16, cw[curc], 20); cv_stroke(c);
      }
    }
    layer_end(&L_edit, c);
  }
  hg_tex(&L_edit, ex, ey, 1);
}

static void draw_ref(void) {
  float rx = 650, ry = GY + GROWS * CH + 44;
  bool pad = input_get()->used_pad;
  cv *c = layer_begin(&L_ref, 560, 176, hash_str("ref", (uint32_t)rnd * 2 + pad));
  if (c) {
    p_text(c, F_MONO, 15, 1, CV_LEFT, "REDCODE-LITE", 0, 16, TG_AMBER, 1);
    p_text(c, F_MONO, 13, 0, CV_LEFT, "DAT      a bomb: executing it kills a process", 0, 40, TG_DIM, 1);
    p_text(c, F_MONO, 13, 0, CV_LEFT, "MOV #v $d  plant a DAT (value v) at pc+d", 0, 60, TG_DIM, 1);
    p_text(c, F_MONO, 13, 0, CV_LEFT, "MOV $s $d  copy the whole cell pc+s to pc+d", 0, 80, TG_DIM, 1);
    p_text(c, F_MONO, 13, 0, CV_LEFT, "ADD #v $d  add v to the B field of pc+d", 0, 100, TG_DIM, 1);
    p_text(c, F_MONO, 13, 0, CV_LEFT, "JMP $o     jump to pc+o     JMZ $o $t if B(t)=0", 0, 120, TG_DIM, 1);
    const char *goal = rnd == 0 ? "lockd is an IMP: it paves forward and cannot be killed. Loop in place behind it and outlive the clock." :
                        rnd == 1 ? "lockd is a BOMBER on a fixed loop. Plant a DAT on its code before it bombs you." :
                        "lockd is a SCANNER. Beat it to the trigger: drop a DAT on its code on the first cycle.";
    tg_wrap(c, goal, 0, 148, 560, 18, 14, TG_GRN);
    layer_end(&L_ref, c);
  }
  hg_tex(&L_ref, rx, ry, 1);
}

static void daemon_draw(void) {
  tg_header("// DAEMON WAR : lockd");
  if (sub == D_HOWTO) {
    static const char *L[] = {
      "lockd still runs in the core memory. You get a warrior of up to six instructions and you choose",
      "where it loads. Then your warrior and lockd take turns, one instruction each, around the ring.",
      "",
      "A process dies if it executes a DAT or runs off into empty memory. MOV #v $d plants a DAT bomb;",
      "MOV $s $d copies a cell; ADD advances a pointer; JMP and JMZ move execution.",
      "{UPDOWN}{LEFTRIGHT} move, {PICK}/{HEAVY} cycle a field, {SPECIAL} run the battle.",
      "Round 1: outlive the imp. Round 2: kill the bomber. Round 3: out-draw the scanner.",
    };
    tg_howto("DAEMON WAR  //  fight lockd for the core", L, (int)(sizeof L / sizeof *L), TG_RED);
    return;
  }
  draw_grid();
  if (sub == D_EDIT) { draw_editor(); draw_ref(); tg_footer("{UPDOWN}{LEFTRIGHT} MOVE   {PICK}/{HEAVY} CYCLE   {SPECIAL} RUN   {BACK} QUIT"); }
  else if (sub == D_RUN) tg_footer("{BACK} STOP AND EDIT");
  tg_pips(1130, GY - 10, rnd, 3, TG_GRN);
  if (sub == D_RWIN)  tg_banner(true,  winmode == 0 ? "OUTLIVED LOCKD" : "LOCKD KILLED", rnd + 1 >= 3 ? "the core is yours." : "next round.", t);
  if (sub == D_RFAIL) tg_banner(false, "WARRIOR DOWN", "lockd got you. {PICK} to rewrite and retry.", t);
  if (sub == D_WIN)   tg_banner(true,  "CORE SEIZED", "lockd is dead on all three. the doors are yours.", t);
}

const term_game TG_DAEMON = { "DAEMON WAR", "fight lockd for the core memory", daemon_start, daemon_update, daemon_draw };

#ifdef KH_TEST_TERMINAL
int daemon_sub(void) { return sub; }
int daemon_round(void) { return rnd; }
int daemon_cur_line(void) { return curl; }
int daemon_cur_cell(void) { return curc; }
int daemon_load(void) { return load_pos; }
int daemon_d_adr(void) { return d_adr; }
void daemon_get_line(int i, int *op, int *am, int *a, int *bm, int *b) { Cell *c = &war[i]; *op = c->op; *am = c->am; *a = c->a; *bm = c->bm; *b = c->b; }
#endif
#endif
