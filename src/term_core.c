/* term_core.c: a TIS-100 style one-node assembly puzzle. The node has ACC and BAK, reads an IN stream and
 * writes an OUT stream. You edit up to 8 lines by menu (cycle the op, cycle the operands: no typing), then
 * RUN against randomised tests or STEP one cycle at a time watching the input, expected and actual streams.
 * Win when every test passes; the cycle count is shown against a par for a bonus mark. Three puzzles of
 * rising difficulty: double each input, absolute value, then the sign of each input. See term_game.h. */
#if !defined(KH_TEST_WORLD) && !defined(KH_TEST_FIGHT)
#include "term_game.h"
#include "terminal.h"
#include "audio.h"
#include "cv.h"
#include <string.h>
#include <stdio.h>

enum { C_HOWTO, C_EDIT, C_RWIN, C_WIN };
#define NLINE 8
#define MAXCYC 800
#define ACC_LO (-9999)
#define ACC_HI 9999

/* ops */
enum { OP_NOP, OP_MOV, OP_ADD, OP_SUB, OP_NEG, OP_SWP, OP_SAV, OP_JMP, OP_JEZ, OP_JNZ, OP_JGZ, OP_JLZ, OP_N };
static const char *OPNAME[OP_N] = { "NOP", "MOV", "ADD", "SUB", "NEG", "SWP", "SAV", "JMP", "JEZ", "JNZ", "JGZ", "JLZ" };
/* arg kinds */
enum { AK_NONE, AK_SRC, AK_DST, AK_LINE };
static void op_args(int op, int *a1, int *a2) {
  switch (op) {
    case OP_MOV: *a1 = AK_SRC; *a2 = AK_DST; break;
    case OP_ADD: case OP_SUB: *a1 = AK_SRC; *a2 = AK_NONE; break;
    case OP_JMP: case OP_JEZ: case OP_JNZ: case OP_JGZ: case OP_JLZ: *a1 = AK_LINE; *a2 = AK_NONE; break;
    default: *a1 = AK_NONE; *a2 = AK_NONE; break;   /* NOP NEG SWP SAV */
  }
}
/* SRC operand codes: ACC, IN, NIL, then immediates */
static const int IMM[] = { -2, -1, 0, 1, 2, 3 };
#define NIMM ((int)(sizeof IMM / sizeof IMM[0]))
enum { SRC_ACC, SRC_IN, SRC_NIL, SRC_IMM0 };
#define NSRC (SRC_IMM0 + NIMM)
enum { DST_ACC, DST_OUT, DST_NIL, NDST };
static void src_name(int code, char *out) {
  if (code == SRC_ACC) strcpy(out, "ACC");
  else if (code == SRC_IN) strcpy(out, "IN");
  else if (code == SRC_NIL) strcpy(out, "NIL");
  else snprintf(out, 8, "#%d", IMM[code - SRC_IMM0]);
}
static const char *DSTNAME[NDST] = { "ACC", "OUT", "NIL" };

typedef struct { int op, a, b; } Line;
static trng rng;
static int sub, rnd;
static float t;
static Line prog[NLINE];
static int curl, curc;            /* cursor line, cell (0 op, 1 arg1, 2 arg2) */

/* the puzzle */
enum { PZ_DOUBLE, PZ_ABS, PZ_SIGN };
static int puzzle;
#define NIN_MAX 8
static int in_v[NIN_MAX], nin;
static int exp_v[NIN_MAX], nexp;
static int par;

/* last run */
static bool ran, run_pass;
static int run_cycles, run_nout, run_out[NIN_MAX + 3], run_failtest;
/* a few test cases are run; we keep test 0's streams for display */

/* step mode */
static bool stepping; static int st_acc, st_bak, st_pc, st_inp, st_nout, st_cyc; static bool st_alive;
static int st_out[NIN_MAX + 3];

static int clampi(int v) { return v < ACC_LO ? ACC_LO : v > ACC_HI ? ACC_HI : v; }

static void gen_case(trng *r) {
  if (puzzle == PZ_DOUBLE) { nin = 6; for (int i = 0; i < nin; i++) in_v[i] = tr_int(r, -9, 9); for (int i = 0; i < nin; i++) exp_v[i] = clampi(2 * in_v[i]); nexp = nin; }
  else if (puzzle == PZ_ABS) { nin = 6; for (int i = 0; i < nin; i++) in_v[i] = tr_int(r, -9, 9); for (int i = 0; i < nin; i++) exp_v[i] = in_v[i] < 0 ? -in_v[i] : in_v[i]; nexp = nin; }
  else { nin = 6; for (int i = 0; i < nin; i++) in_v[i] = tr_int(r, -9, 9); for (int i = 0; i < nin; i++) exp_v[i] = in_v[i] > 0 ? 1 : in_v[i] < 0 ? -1 : 0; nexp = nin; }
}

static void gen_round(void) {
  puzzle = rnd == 0 ? PZ_DOUBLE : rnd == 1 ? PZ_ABS : PZ_SIGN;
  par = rnd == 0 ? 42 : rnd == 1 ? 58 : 80;
  gen_case(&rng);
  for (int i = 0; i < NLINE; i++) { prog[i].op = OP_NOP; prog[i].a = 0; prog[i].b = 0; }
  curl = 0; curc = 0;
  ran = false; stepping = false;
}

static void core_start(uint32_t seed) {
  tr_seed(&rng, seed);
  rnd = 0; sub = C_HOWTO; t = 0;
  gen_round();
}

/* ---- the VM ---- */
static int src_val(int code, const int *in, int ninp, int *inp, int acc, bool *stall) {
  if (code == SRC_ACC) return acc;
  if (code == SRC_NIL) return 0;
  if (code == SRC_IN) { if (*inp < ninp) return in[(*inp)++]; *stall = true; return 0; }
  return IMM[code - SRC_IMM0];
}
/* run the program on one input case; returns pass, fills cycles and out[] */
static bool run_case(const int *in, int ninp, const int *expv, int ne, int *cycles, int *outn, int *out) {
  int acc = 0, bak = 0, pc = 0, inp = 0, nout = 0, cyc = 0;
  while (cyc < MAXCYC && nout <= ne + 2) {
    cyc++;
    Line L = prog[pc];
    bool jumped = false, stall = false;
    switch (L.op) {
      case OP_NOP: break;
      case OP_MOV: { int v = src_val(L.a, in, ninp, &inp, acc, &stall); if (stall) goto done;
        if (L.b == DST_ACC) acc = clampi(v); else if (L.b == DST_OUT) { if (nout < ne + 3) out[nout++] = v; } break; }
      case OP_ADD: { int v = src_val(L.a, in, ninp, &inp, acc, &stall); if (stall) goto done; acc = clampi(acc + v); break; }
      case OP_SUB: { int v = src_val(L.a, in, ninp, &inp, acc, &stall); if (stall) goto done; acc = clampi(acc - v); break; }
      case OP_NEG: acc = clampi(-acc); break;
      case OP_SWP: { int tmp = acc; acc = bak; bak = tmp; break; }
      case OP_SAV: bak = acc; break;
      case OP_JMP: pc = L.a & 7; jumped = true; break;
      case OP_JEZ: if (acc == 0) { pc = L.a & 7; jumped = true; } break;
      case OP_JNZ: if (acc != 0) { pc = L.a & 7; jumped = true; } break;
      case OP_JGZ: if (acc > 0) { pc = L.a & 7; jumped = true; } break;
      case OP_JLZ: if (acc < 0) { pc = L.a & 7; jumped = true; } break;
    }
    if (!jumped) pc = (pc + 1) & 7;
  }
done:
  *cycles = cyc; *outn = nout;
  if (nout != ne) return false;
  for (int i = 0; i < ne; i++) if (out[i] != expv[i]) return false;
  return true;
}

static void do_run(void) {
  trng tr; tr_seed(&tr, rng.s ^ 0xABCDu);
  run_pass = true; run_cycles = 0; run_failtest = -1;
  /* test 0 is the visible case; a few more are random to stop a lookup table passing */
  int savein[NIN_MAX], savex[NIN_MAX], savn = nin, savnx = nexp;
  for (int i = 0; i < nin; i++) savein[i] = in_v[i]; for (int i = 0; i < nexp; i++) savex[i] = exp_v[i];
  for (int tcase = 0; tcase < 4; tcase++) {
    if (tcase == 0) { nin = savn; nexp = savnx; for (int i = 0; i < nin; i++) in_v[i] = savein[i]; for (int i = 0; i < nexp; i++) exp_v[i] = savex[i]; }
    else gen_case(&tr);
    int cyc, no, out[NIN_MAX + 3];
    bool ok = run_case(in_v, nin, exp_v, nexp, &cyc, &no, out);
    if (tcase == 0) { run_nout = no; for (int i = 0; i < no; i++) run_out[i] = out[i]; }
    if (cyc > run_cycles) run_cycles = cyc;
    if (!ok && run_failtest < 0) { run_failtest = tcase; run_pass = false; }
  }
  /* restore the visible case */
  nin = savn; nexp = savnx; for (int i = 0; i < nin; i++) in_v[i] = savein[i]; for (int i = 0; i < nexp; i++) exp_v[i] = savex[i];
  ran = true;
  if (run_pass) { sfx(SFX_PICKUP, 0, 1); sub = C_RWIN; t = 0; }
  else sfx(SFX_SHOCK, 0, .6f);
}

static void step_reset(void) { stepping = true; st_acc = st_bak = st_pc = st_inp = st_nout = st_cyc = 0; st_alive = true; }
static void do_step(void) {
  if (!stepping) { step_reset(); return; }
  if (st_cyc >= MAXCYC || !st_alive) return;
  st_cyc++;
  Line L = prog[st_pc];
  bool jumped = false, stall = false;
  switch (L.op) {
    case OP_NOP: break;
    case OP_MOV: { int v = src_val(L.a, in_v, nin, &st_inp, st_acc, &stall); if (stall) { st_alive = false; break; }
      if (L.b == DST_ACC) st_acc = clampi(v); else if (L.b == DST_OUT) { if (st_nout < nexp + 2) st_out[st_nout++] = v; } break; }
    case OP_ADD: { int v = src_val(L.a, in_v, nin, &st_inp, st_acc, &stall); if (stall) { st_alive = false; break; } st_acc = clampi(st_acc + v); break; }
    case OP_SUB: { int v = src_val(L.a, in_v, nin, &st_inp, st_acc, &stall); if (stall) { st_alive = false; break; } st_acc = clampi(st_acc - v); break; }
    case OP_NEG: st_acc = clampi(-st_acc); break;
    case OP_SWP: { int tmp = st_acc; st_acc = st_bak; st_bak = tmp; break; }
    case OP_SAV: st_bak = st_acc; break;
    case OP_JMP: st_pc = L.a & 7; jumped = true; break;
    case OP_JEZ: if (st_acc == 0) { st_pc = L.a & 7; jumped = true; } break;
    case OP_JNZ: if (st_acc != 0) { st_pc = L.a & 7; jumped = true; } break;
    case OP_JGZ: if (st_acc > 0) { st_pc = L.a & 7; jumped = true; } break;
    case OP_JLZ: if (st_acc < 0) { st_pc = L.a & 7; jumped = true; } break;
  }
  if (!jumped) st_pc = (st_pc + 1) & 7;
}

/* cycle the value in the current cell by dir (+1/-1) */
static void cycle_cell(int dir) {
  Line *L = &prog[curl];
  int a1, a2; op_args(L->op, &a1, &a2);
  if (curc == 0) {
    L->op = (L->op + dir + OP_N) % OP_N;
    op_args(L->op, &a1, &a2);
    L->a = a1 == AK_SRC ? SRC_ACC : 0;
    L->b = a2 == AK_DST ? DST_ACC : 0;
  } else if (curc == 1) {
    int n = a1 == AK_SRC ? NSRC : a1 == AK_LINE ? NLINE : 0;
    if (n) L->a = (L->a + dir + n) % n;
  } else {
    int n = a2 == AK_DST ? NDST : 0;
    if (n) L->b = (L->b + dir + n) % n;
  }
  ran = false; stepping = false;
}

static int cells_in(int line) { int a1, a2; op_args(prog[line].op, &a1, &a2); return 1 + (a1 != AK_NONE) + (a2 != AK_NONE); }

static int core_update(float dt, const intent_t *in) {
  t += dt;
  if (sub == C_HOWTO) { if (in->back) return TG_QUIT; if (in->ok) { sub = C_EDIT; t = 0; } return TG_RUN; }
  if (sub == C_RWIN) { if (in->back) return TG_QUIT; if (t > 0.6f && in->ok) { rnd++; if (rnd >= 3) { sub = C_WIN; t = 0; } else { gen_round(); sub = C_EDIT; t = 0; } } return TG_RUN; }
  if (sub == C_WIN) { if (t > 0.6f && in->ok) return TG_WIN; return TG_RUN; }
  /* C_EDIT */
  if (in->back) return TG_QUIT;
  int iy = tg_iy_up(in);
  if (in->nav_y) { curl = (curl - iy + NLINE) % NLINE; int cc = cells_in(curl); if (curc >= cc) curc = cc - 1; sfx(SFX_UI, 0, .5f); }
  if (in->nav_x) { int cc = cells_in(curl); curc += in->nav_x > 0 ? 1 : -1; if (curc < 0) curc = 0; if (curc >= cc) curc = cc - 1; sfx(SFX_UI, 0, .5f); }
  if (in->ok) { cycle_cell(+1); sfx(SFX_UI_OK, 0, .7f); }
  if (in->heavy) { cycle_cell(-1); sfx(SFX_UI_OK, 0, .6f); }
  if (in->special) do_run();
  if (in->assist) { do_step(); sfx(SFX_UI, 0, .6f); }
  return TG_RUN;
}

/* ---- drawing ---- */
static layer_t L_prog, L_side;

static void line_text(int i, char *out, int cap) {
  Line L = prog[i];
  int a1, a2; op_args(L.op, &a1, &a2);
  char sa[12] = "", sb[12] = "";
  if (a1 == AK_SRC) src_name(L.a, sa); else if (a1 == AK_LINE) snprintf(sa, sizeof sa, "L%d", L.a & 7);
  if (a2 == AK_DST) snprintf(sb, sizeof sb, "%s", DSTNAME[L.b % NDST]);
  snprintf(out, cap, "%-4s%-5s%-4s", OPNAME[L.op], sa, sb);
}

static void draw_prog(void) {
  float px = TG_CX0 + 8, py = 150, rh = 40;
  tg_panel(TG_CX0, 128, 560, 480, TG_GRN);
  char key[64]; snprintf(key, sizeof key, "%d.%d.%d", curl, curc, rnd);
  uint32_t h = 0; for (int i = 0; i < NLINE; i++) h = h * 131 + (uint32_t)(prog[i].op * 97 + prog[i].a * 7 + prog[i].b);
  cv *c = layer_begin(&L_prog, 544, 470, hash_str(key, h | 1u));
  if (c) {
    /* column x offsets (fixed-width mono) */
    float colL = 10, colOp = 70, colA = 190, colB = 320;
    for (int i = 0; i < NLINE; i++) {
      float y = 20 + i * rh;
      bool sel = i == curl;
      if (sel) { cv_fill_hex(c, TG_GRN, .14f); cv_fill_rect(c, 0, y - 24, 544, rh - 4); }
      if (stepping && i == st_pc && st_alive) { cv_fill_hex(c, TG_AMBER, .20f); cv_fill_rect(c, 0, y - 24, 544, rh - 4); p_text(c, F_MONO, 18, 0, CV_LEFT, ">", colL - 8, y, TG_AMBER, 1); }
      char ln[4]; snprintf(ln, sizeof ln, "%d", i);
      p_text(c, F_MONO, 20, 0, CV_LEFT, ln, colL, y, TG_DIM, 1);
      Line L = prog[i]; int a1, a2; op_args(L.op, &a1, &a2);
      char sa[12] = "", sb[12] = "";
      if (a1 == AK_SRC) src_name(L.a, sa); else if (a1 == AK_LINE) snprintf(sa, sizeof sa, "L%d", L.a & 7);
      if (a2 == AK_DST) snprintf(sb, sizeof sb, "%s", DSTNAME[L.b % NDST]);
      p_text(c, F_MONO, 22, 0, CV_LEFT, OPNAME[L.op], colOp, y, L.op == OP_NOP ? TG_DIM : TG_PAPER, 1);
      if (sa[0]) p_text(c, F_MONO, 22, 0, CV_LEFT, sa, colA, y, TG_CYAN, 1);
      if (sb[0]) p_text(c, F_MONO, 22, 0, CV_LEFT, sb, colB, y, TG_AMBER, 1);
      if (sel) {   /* the cursor box on the active cell */
        float cx = curc == 0 ? colOp : curc == 1 ? colA : colB;
        float cw = curc == 0 ? 56 : 70;
        cv_stroke_hex(c, TG_PAPER, 1); cv_line_width(c, 2);
        cv_begin(c); cv_rect(c, cx - 6, y - 24, cw, 30); cv_stroke(c);
      }
    }
    layer_end(&L_prog, c);
  }
  hg_tex(&L_prog, px, py, 1);
}

static void stream_line(cv *c, const char *lbl, const int *v, int n, float y, uint32_t col, int hi) {
  char s[96]; int k = 0; k += snprintf(s + k, sizeof s - k, "%-5s", lbl);
  for (int i = 0; i < n && k < 80; i++) k += snprintf(s + k, sizeof s - k, "%4d", v[i]);
  p_text(c, F_MONO, 17, 0, CV_LEFT, s, 0, y, col, 1); (void)hi;
}

static void draw_side(void) {
  float sx = 660, sy = 128, sw = 560;
  tg_panel(sx, sy, sw, 480, TG_CYAN);
  const char *pname = puzzle == PZ_DOUBLE ? "double each input" : puzzle == PZ_ABS ? "absolute value" : "sign of each input";
  char key[80]; snprintf(key, sizeof key, "%d.%d.%d.%d.%d", rnd, ran, run_pass, run_cycles, stepping ? st_cyc : -1);
  cv *c = layer_begin(&L_side, (int)sw - 20, 460, hash_str(key, hash_str(pname, 600)));
  if (c) {
    p_text(c, F_DISPLAY, 20, 1, CV_LEFT, "TASK", 10, 26, TG_AMBER, 1);
    p_text(c, F_MONO, 18, 0, CV_LEFT, pname, 90, 26, TG_PAPER, 1);
    stream_line(c, "IN", in_v, nin, 70, TG_GRN, -1);
    stream_line(c, "WANT", exp_v, nexp, 98, TG_CYAN, -1);
    if (ran) stream_line(c, "GOT", run_out, run_nout, 126, run_pass ? TG_GRN : TG_RED, -1);
    else if (stepping) stream_line(c, "GOT", st_out, st_nout, 126, TG_AMBER, -1);
    else p_text(c, F_MONO, 17, 0, CV_LEFT, "GOT  (run or step)", 0, 126, TG_DIM, 1);
    /* registers for step mode */
    if (stepping) {
      char rs[64]; snprintf(rs, sizeof rs, "ACC %-6d BAK %-6d PC %d  CYC %d", st_acc, st_bak, st_pc, st_cyc);
      p_text(c, F_MONO, 17, 1, CV_LEFT, rs, 0, 162, st_alive ? TG_GRN : TG_DIM, 1);
      if (!st_alive) p_text(c, F_MONO, 15, 0, CV_LEFT, "halted (waiting on empty IN)", 0, 186, TG_DIM, 1);
    }
    /* run verdict */
    if (ran) {
      char v[64];
      if (run_pass) snprintf(v, sizeof v, "ALL TESTS PASS   %d cycles (par %d)", run_cycles, par);
      else snprintf(v, sizeof v, "FAIL on test %d", run_failtest + 1);
      p_text(c, F_MONO, 18, 1, CV_LEFT, v, 0, 220, run_pass ? TG_GRN : TG_RED, 1);
      if (run_pass && run_cycles <= par) p_text(c, F_MONO, 16, 1, CV_LEFT, "UNDER PAR  +bonus", 0, 244, TG_AMBER, 1);
    }
    /* a short op reference */
    p_text(c, F_MONO, 14, 1, CV_LEFT, "OPS  MOV ADD SUB NEG SWP SAV  JMP JEZ JNZ JGZ JLZ", 0, 300, TG_DIM, 1);
    p_text(c, F_MONO, 14, 0, CV_LEFT, "SRC: ACC IN NIL #n   DST: ACC OUT NIL", 0, 324, TG_DIM, 1);
    p_text(c, F_MONO, 14, 0, CV_LEFT, "jumps target a line (L0..L7). the node wraps L7 -> L0.", 0, 348, TG_DIM, 1);
    layer_end(&L_side, c);
  }
  hg_tex(&L_side, sx + 10, sy + 10, 1);
}

static void core_draw(void) {
  tg_header("// CORE : door controller node");
  if (sub == C_HOWTO) {
    static const char *L[] = {
      "The door node runs a tiny assembly. It has two registers, ACC and BAK, reads values from IN and",
      "writes them to OUT. Write up to 8 lines so the OUT stream matches WANT for every test.",
      "",
      "{UPDOWN} picks a line, {LEFTRIGHT} picks a field. {PICK} cycles that field forward, {HEAVY} back.",
      "No typing: you choose the op and its operands from the lists.",
      "{SPECIAL} runs all tests. {ASSIST} single-steps one test so you can watch ACC, BAK and the streams.",
      "Pass every test to clear the round; beat the cycle par for a bonus. Three puzzles, rising.",
    };
    tg_howto("CORE  //  program the node", L, (int)(sizeof L / sizeof *L), TG_GRN);
    return;
  }
  draw_prog();
  draw_side();
  tg_pips(1130, 128, rnd, 3, TG_GRN);
  tg_footer("{UPDOWN}{LEFTRIGHT} MOVE   {PICK}/{HEAVY} CYCLE   {SPECIAL} RUN   {ASSIST} STEP   {BACK} QUIT");
  if (sub == C_RWIN) tg_banner(true, "NODE SOLVED", "all tests pass. next puzzle.", t);
  if (sub == C_WIN)  tg_banner(true, "CONTROLLER OWNED", "three puzzles cleared. DAEMON WAR unlocked.", t);
}

const term_game TG_CORE = { "CORE", "program the door controller node", core_start, core_update, core_draw };

#ifdef KH_TEST_TERMINAL
int core_sub(void) { return sub; }
int core_cur_line(void) { return curl; }
int core_cur_cell(void) { return curc; }
void core_get_prog(int *ops, int *as, int *bs) { for (int i = 0; i < NLINE; i++) { ops[i] = prog[i].op; as[i] = prog[i].a; bs[i] = prog[i].b; } }
int core_cells_in(int line) { return cells_in(line); }
/* the canonical solution for the current round */
void core_get_sol(int *ops, int *as, int *bs, int *n) {
  int o[NLINE], a[NLINE], b[NLINE];
  for (int i = 0; i < NLINE; i++) { o[i] = OP_NOP; a[i] = 0; b[i] = 0; }
  if (puzzle == PZ_DOUBLE) {
    o[0] = OP_MOV; a[0] = SRC_IN; b[0] = DST_ACC;
    o[1] = OP_ADD; a[1] = SRC_ACC;
    o[2] = OP_MOV; a[2] = SRC_ACC; b[2] = DST_OUT;
    o[3] = OP_JMP; a[3] = 0;
    *n = 4;
  } else if (puzzle == PZ_ABS) {
    o[0] = OP_MOV; a[0] = SRC_IN; b[0] = DST_ACC;
    o[1] = OP_JGZ; a[1] = 3;
    o[2] = OP_NEG;
    o[3] = OP_MOV; a[3] = SRC_ACC; b[3] = DST_OUT;
    o[4] = OP_JMP; a[4] = 0;
    *n = 5;
  } else {
    o[0] = OP_MOV; a[0] = SRC_IN; b[0] = DST_ACC;
    o[1] = OP_JLZ; a[1] = 5;
    o[2] = OP_JGZ; a[2] = 7;
    o[3] = OP_MOV; a[3] = SRC_NIL; b[3] = DST_OUT;
    o[4] = OP_JMP; a[4] = 0;
    o[5] = OP_MOV; a[5] = SRC_IMM0 + 1; b[5] = DST_OUT;   /* #-1 */
    o[6] = OP_JMP; a[6] = 0;
    o[7] = OP_MOV; a[7] = SRC_IMM0 + 3; b[7] = DST_OUT;   /* #1 */
    *n = 8;
  }
  for (int i = 0; i < NLINE; i++) { ops[i] = o[i]; as[i] = a[i]; bs[i] = b[i]; }
}
#endif
#endif
