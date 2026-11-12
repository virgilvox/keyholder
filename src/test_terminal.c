/* test_terminal.c: the pizza-box terminal on its own, headless, with a scripted bot. It boots a bare scene,
 * opens the terminal with all four programs available, then walks a fixed sequence of scenarios: for each of
 * the four games it first MASHES ok and holds a direction for a long stretch (to show the game cannot be won
 * that way), then PLAYS round one to the win screen through real intents. The bot reads small test-only
 * hooks in each game (the current sub-state and, for the win pass, the solution) so it can steer the real
 * intents the same way a player would, then presses the real commit button; the win still comes out of the
 * game's own update, not a shortcut.
 *   build: EXTRA_CFLAGS=-DKH_TEST_TERMINAL (drop game.c, attic.c, attic_env.c), then tools/shot.mjs.
 * The [tt] log lines name the frame where each scenario starts, the mash window, and the win banner, so the
 * right frames can be shot and looked at. */
#ifdef KH_TEST_TERMINAL
#include "game.h"
#include "world.h"
#include "arena.h"
#include "fx.h"
#include "terminal.h"
#include "term_game.h"
#include "input.h"
#include "hud_int.h"
#include "ds.h"
#include <string.h>
#include <math.h>

/* ---- test hooks exported by the games and the shell ---- */
int term_phase(void); int term_sel(void);
void glitch_solution(float *off, float *w, int *plo, int *phi, int *sub);
float glitch_cur_off(void); float glitch_cur_w(void); int glitch_cur_pow(void);
int wiretap_sub(void); int wiretap_n(void); int wiretap_cursor(void);
void wiretap_cur_bits(int *out); void wiretap_target_bits(int *out);
int core_sub(void); int core_cur_line(void); int core_cur_cell(void);
void core_get_prog(int *ops, int *as, int *bs); void core_get_sol(int *ops, int *as, int *bs, int *n);
int daemon_sub(void); int daemon_round(void); int daemon_cur_line(void); int daemon_cur_cell(void);
int daemon_load(void); int daemon_d_adr(void);
void daemon_get_line(int i, int *op, int *am, int *a, int *bm, int *b);

static t3_scene *scene; static t3_camera *cam;

void game_init(void) {
  scene = t3_scene_new();
  cam = t3_perspective_camera_new(38, 16.f / 9.f, .06f, 1600);
  t3_object_add(&scene->base, cam);
  hg_init();
  world_init(scene);
  fx_init(scene);
  world_set_stage(STAGE_TOWN); world_set_time(22.f);
  term_init();
  term_open(0x0F);                 /* all four programs available */
}

/* ---- the bot ---- */
enum { B_BOOT, B_SELECT, B_RUN, B_DWELL, B_EXIT, B_DONE };
#define MASH_LEN 360
#define DWELL 90
static int bstate = B_BOOT, scen = 0, timer = 0, dwell = 0, fr = -1, logged_run = -1;

static int nargs(int op) { if (op == 1) return 2; if (op == 2 || op == 3 || op >= 7) return 1; return 0; }

/* each drives the game all the way to its final unlock banner, returning true then (proving every round).
 * Acts once per call (the caller gates to every other frame for clean edges). */
static bool win_glitch(intent_t *in) {
  float off, w; int plo, phi, sub; glitch_solution(&off, &w, &plo, &phi, &sub);
  if (sub == 4) return true;                  /* G_WIN */
  if (sub == 2 || sub == 3) { in->ok = true; return false; }   /* advance a round / retry */
  if (sub != 1) { in->ok = true; return false; }               /* howto */
  float co = glitch_cur_off(), cw = glitch_cur_w(); int cp = glitch_cur_pow();
  if (cp != plo) { in->heavy = true; return false; }            /* arm power to the low edge of the band */
  if (fabsf(co - off) > 0.011f) { in->nav_x = co < off ? 1 : -1; return false; }
  if (fabsf(cw - w) > 0.02f) { in->nav_y = cw < w ? -1 : 1; return false; }  /* nav_y -1 = up = wider */
  in->ok = true;                              /* fire */
  return false;
}

static bool win_wiretap(intent_t *in) {
  int sub = wiretap_sub();
  if (sub == 4) return true;                  /* W_WIN */
  if (sub == 2 || sub == 3) { in->ok = true; return false; }
  if (sub != 1) { in->ok = true; return false; }
  int n = wiretap_n(), cur = wiretap_cursor();
  int tb[40], cb[40]; wiretap_target_bits(tb); wiretap_cur_bits(cb);
  int want = -1;
  for (int i = 0; i < n; i++) { int j = (cur + i) % n; if (cb[j] != tb[j]) { want = j; break; } }
  if (want < 0) { in->special = true; return false; }   /* all bits set: replay */
  if (cur != want) { in->nav_x = 1; return false; }      /* walk right to the next wrong bit */
  in->ok = true;                              /* flip it */
  return false;
}

static bool win_core(intent_t *in) {
  int sub = core_sub();
  if (sub == 3) return true;                  /* C_WIN */
  if (sub == 2) { in->ok = true; return false; }   /* C_RWIN: next puzzle */
  if (sub != 1) { in->ok = true; return false; }
  int so[8], sa[8], sb[8], n, po[8], pa[8], pb[8];
  core_get_sol(so, sa, sb, &n); core_get_prog(po, pa, pb);
  int tl = -1, tc = -1;
  for (int i = 0; i < 8 && tl < 0; i++) {
    if (po[i] != so[i]) { tl = i; tc = 0; break; }
    int na = nargs(so[i]);
    if (na >= 1 && pa[i] != sa[i]) { tl = i; tc = 1; break; }
    if (na == 2 && pb[i] != sb[i]) { tl = i; tc = 2; break; }
  }
  if (tl < 0) { in->special = true; return false; }   /* program matches: run the tests */
  int cl = core_cur_line(), cc = core_cur_cell();
  if (cl != tl) { in->nav_y = cl < tl ? 1 : -1; return false; }
  if (cc != tc) { in->nav_x = cc < tc ? 1 : -1; return false; }
  in->ok = true;                              /* cycle the field forward toward the target */
  return false;
}

static bool win_daemon(intent_t *in) {
  int sub = daemon_sub();
  if (sub == 5) return true;                  /* D_WIN */
  if (sub == 3 || sub == 4) { in->ok = true; return false; }   /* RWIN next / RFAIL retry */
  if (sub == 2) return false;                 /* battle running: watch */
  if (sub != 1) { in->ok = true; return false; }
  /* per-round target. imp round: survive by looping (JMP $0). bomber/scanner: plant a DAT on lockd
   * (MOV #0 $5) with the warrior loaded 5 cells before it, then loop. The starter already loads at the
   * right spot, so only the offset/op fields need a nudge. */
  int r = daemon_round(), T[2][5], nlines;
  if (r == 0) { T[0][0] = 4; T[0][1] = 1; T[0][2] = 0; T[0][3] = 1; T[0][4] = 0; nlines = 1; }   /* JMP $0 */
  else { T[0][0] = 2; T[0][1] = 0; T[0][2] = 0; T[0][3] = 1; T[0][4] = 5;        /* MOV #0 $5 */
         T[1][0] = 4; T[1][1] = 1; T[1][2] = 0; T[1][3] = 1; T[1][4] = 0; nlines = 2; }  /* JMP $0 */
  for (int li = 0; li < nlines; li++) {
    int op, am, a, bm, b; daemon_get_line(li, &op, &am, &a, &bm, &b);
    int cur5[5] = { op, am, a, bm, b };
    for (int cc = 0; cc < 5; cc++) {
      if (cur5[cc] == T[li][cc]) continue;
      int cl = daemon_cur_line(), ccur = daemon_cur_cell();
      if (cl != li + 1) { in->nav_y = cl < li + 1 ? 1 : -1; return false; }   /* warrior lines are curl 1.. */
      if (ccur != cc) { in->nav_x = ccur < cc ? 1 : -1; return false; }
      if (cc == 2 || cc == 4) { if (cur5[cc] > T[li][cc]) in->heavy = true; else in->ok = true; }   /* step value */
      else in->ok = true;                     /* op or mode: cycle forward */
      return false;
    }
  }
  in->special = true;                         /* warrior ready: run the battle */
  return false;
}

static bool win_drive(int g, intent_t *in) {
  return g == 0 ? win_glitch(in) : g == 1 ? win_wiretap(in) : g == 2 ? win_core(in) : win_daemon(in);
}
static int game_sub(int g) {
  if (g == 0) { float a, b; int c, d, s; glitch_solution(&a, &b, &c, &d, &s); return s; }
  return g == 1 ? wiretap_sub() : g == 2 ? core_sub() : daemon_sub();
}

static void drive(intent_t *in) {
  int ph = term_phase();
  if (bstate == B_BOOT) { if (ph == 1) bstate = B_SELECT; else { in->ok = true; return; } }
  if (bstate == B_SELECT) {
    if (scen >= 8) { bstate = B_DONE; return; }
    int g = scen / 2;
    if (ph == 1) {
      if (term_sel() != g) { in->nav_y = term_sel() < g ? 1 : -1; return; }
      in->ok = true; bstate = B_RUN; timer = 0; logged_run = -1;   /* launch */
    }
    return;
  }
  if (bstate == B_RUN) {
    int g = scen / 2, mode = scen % 2;
    if (ph != 2) { bstate = B_EXIT; return; }
    if (logged_run < 0) { logged_run = fr; ds_log("[tt] scen %d (game %d %s) running at fr %d", scen, g, mode ? "WIN" : "MASH", fr); }
    timer++;
    if (mode == 0) {                           /* MASH: ok + hold right, every frame */
      in->ok = true; in->mx = 1.f;
      if (timer > MASH_LEN) { ds_log("[tt] scen %d MASH done at fr %d, game sub %d (no win)", scen, fr, game_sub(g)); bstate = B_EXIT; }
    } else {                                   /* WIN: steer, gated every other frame for clean edges */
      bool done = (timer % 2 == 0) ? win_drive(g, in) : false;
      if (done) { ds_log("[tt] scen %d WIN banner at fr %d", scen, fr); bstate = B_DWELL; dwell = 0; }
    }
    return;
  }
  if (bstate == B_DWELL) { dwell++; if (dwell > DWELL) bstate = B_EXIT; return; }   /* hold on the banner for a shot */
  if (bstate == B_EXIT) {
    int ph2 = term_phase(), mode = scen % 2;
    if (ph2 == 2) { if (mode) in->ok = true; else in->back = true; }   /* win: claim it (ok); mash: quit (back) */
    else if (ph2 == 1) { scen++; bstate = B_SELECT; ds_log("[tt] -> scenario %d", scen); }
    return;
  }
  /* B_DONE */ in->back = true;
}

void game_frame(float dt) {
  fr++;
  intent_t in; memset(&in, 0, sizeof in); in.weapon_pick = -1; in.used_pad = true;
  drive(&in);
  world_update(dt, v3_(0, 0, 0));
  term_update(dt, &in);
}

void game_draw(void) {
  render_scene(scene, cam);
  render_target();
  term_draw();
}

/* ---- stubs for the attic env the terminal test does not link (world.c references these for STAGE_ATTIC,
 *      which this harness never enters) ---- */
void attic_env_show(bool on) { (void)on; }
void attic_env_floor(v3 *c, float *r) { if (c) *c = v3_(0, 0, 0); if (r) *r = 50.f; }
void attic_env_push(v3 *p, float r, float h, v3 *n, bool *hit) { (void)p; (void)r; (void)h; (void)n; if (hit) *hit = false; }
bool attic_env_raycast(v3 o, v3 d, float m, float *t, v3 *n) { (void)o; (void)d; (void)m; (void)t; (void)n; return false; }
void attic_env_mood(arena_mood *m) { if (m) arena_mood_of(STAGE_TOWN, m); }
bool attic_env_update(float dt) { (void)dt; return false; }
#endif
