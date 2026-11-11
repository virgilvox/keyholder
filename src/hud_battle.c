/* hud_battle.c: the Battle Zone's screens. The hub, the setup, the versus card before each rung, the
 * results with the rank slamming in, records, the pause menu and the title's entry. Slanted plates,
 * label tape and stripes as the fight HUD; the posed fighters stand in the arena behind. 1280x720. */
#if !defined(KH_TEST_WORLD) && !defined(KH_TEST_FIGHT)
#include "battle.h"
#include "hud_int.h"
#include "weapon.h"
#include <stdio.h>

#define INK 0x0b0c12u
#define HOT 0xff3a78u     /* the zone's second colour */
#define DIM 0x9aa0acu
#define SK -.42f

static float ez(float t) { return hud_ease_out(t); }
static uint32_t rank_col(char r) { return r == 'S' ? 0xffd23a : r == 'A' ? C_HIVIS : r == 'B' ? 0x2ec4b6 : r == 'C' ? 0x8fa0b8 : 0x9a6a6a; }
static void fmt_num(char *b, size_t n, uint32_t v) {
  char t[16]; snprintf(t, sizeof t, "%u", v);
  int l = (int)strlen(t), k = 0;
  for (int i = 0; i < l && k < (int)n - 1; i++) { if (i && (l - i) % 3 == 0 && k < (int)n - 2) b[k++] = ','; b[k++] = t[i]; }
  b[k] = 0;
}
static float text_w(int font, float px, float ls, const char *s) {
  static cv *m; if (!m) m = cv_new(16, 16);
  cv_font(m, font, px); cv_letter_spacing(m, ls);
  return cv_measure(m, s);
}
/* one line of type in its own layer, outlined when edge is set */
static void tl(layer_t *L, int font, float px, float ls, const char *s, uint32_t col, uint32_t edge, uint32_t salt) {
  uint32_t key = hash_str(s, 2166136261u ^ col ^ salt ^ (uint32_t)(px * 8) ^ edge);
  int w = (int)fminf(1280, text_w(font, px, ls, s) + px * .5f + 24), h = (int)(px * 1.45f) + 10;
  cv *c = layer_begin(L, w, h, key);
  if (!c) return;
  float base = px * 1.1f + 3;
  if (edge) {
    cv_font(c, font, px); cv_letter_spacing(c, ls);
    cv_line_join(c, CV_JOIN_ROUND); cv_line_width(c, px * .14f + 3); cv_stroke_hex(c, edge, 1);
    cv_text_align(c, CV_LEFT); cv_text_baseline(c, CV_ALPHABETIC); cv_stroke_text(c, s, 10, base);
  }
  p_text(c, font, px, ls, CV_LEFT, s, 10, base, col, 1);
  layer_end(L, c);
}
/* a layer sheared to lean forward, its top left at x, y, scaled about its centre */
static void tex_lean(const layer_t *L, float x, float y, float sc, float a) {
  float m[6]; mat_place(m, x + L->w * .5f, y + L->h * .5f, 0, L->w * .5f, L->h * .5f, sc, sc);
  m[2] -= .16f * sc;
  m[4] += .16f * L->h * .5f * sc;
  hg_tex_m(L, m, a, 0xffffff);
}
static bool pad(void) { return input_get()->used_pad; }

/* ---- the frame every menu shares: a dark slanted band, the logo, the hint line ---- */
static layer_t L_logo, L_hint, L_crumb;
static void frame(float band_w, const char *crumb, const char *hint, float t) {
  float in = ez(t * 3);
  hg_vgrad(0, 260, INK, .55f, 0);
  hg_vgrad(520, 720, INK, 0, .7f);
  hg_para(-80, 0, (band_w + 80) * in, DS_H, -.18f, INK, .82f);
  hg_stripes(0, 0, band_w * in, DS_H, 0xffffff, .02f, 0, 0, 30, t * .6f);
  hg_para(band_w * in - 6, 0, 6, DS_H, -.18f, C_HIVIS, 1);
  hg_para(band_w * in + 10, 0, 2, DS_H, -.18f, HOT, .8f);
  cv *c = layer_begin(&L_logo, 620, 120, 0xb2);
  if (c) {
    cv_font(c, F_DISPLAY, 66); cv_letter_spacing(c, 1);
    cv_line_join(c, CV_JOIN_ROUND); cv_line_width(c, 10); cv_stroke_hex(c, INK, 1);
    cv_text_align(c, CV_LEFT); cv_text_baseline(c, CV_ALPHABETIC); cv_stroke_text(c, "BATTLE ZONE", 14, 74);
    p_text(c, F_DISPLAY, 66, 1, CV_LEFT, "BATTLE ZONE", 14, 74, C_CREAM, 1);
    p_tape(c, 18, 86, 26, F_MONO, 14, 2, "KEYHOLDER EXHIBITION", C_HIVIS, INK);
    layer_end(&L_logo, c);
  }
  tex_lean(&L_logo, 34 - (1 - in) * 80, 24, 1, in);
  if (crumb) {
    tl(&L_crumb, F_DISPLAY, 20, 3, crumb, HOT, 0, 0xb3);
    hg_tex(&L_crumb, 296, 101, in);
  }
  if (hint) {
    bool p = pad();
    c = layer_begin(&L_hint, 1200, 40, hash_str(hint, p ? 7 : 3));
    if (c) { g_markup(c, 6, 27, CV_LEFT, F_DISPLAY, 15, 1.5f, C_CREAM, 26, p, hint, true); layer_end(&L_hint, c); }
    hg_rect(0, DS_H - 52, DS_W, 52, INK, .55f);
    hg_rect(0, DS_H - 52, DS_W, 2, C_HIVIS, .7f);
    hg_tex(&L_hint, 34, DS_H - 46, 1);
  }
}

/* ---- the fight tag over the posed foes: its title, arena and who ---- */
static layer_t L_tag;
static void fight_tag(int e, float x, float y, float a) {
  if (e < 0 || e >= BZ.ne) return;
  const bz_entry *E = &BZ.e[e];
  uint32_t key = hash_str(E->title, (uint32_t)e * 977 + (E->best_rank << 8) + E->best + (uint32_t)BZ.who);
  cv *c = layer_begin(&L_tag, 560, 150, key);
  if (c) {
    char b[64];
    snprintf(b, sizeof b, E->secret ? "SECRET RUNG" : "TIER %d", E->tier);
    float w = p_tape(c, 0, 0, 24, F_MONO, 13, 2, b, E->secret ? HOT : C_HIVIS, INK);
    p_tape(c, w + 6, 0, 24, F_MONO, 13, 2, E->arena, 0x2a2c38, C_CREAM);
    float px = 30; while (px > 18 && text_w(F_DISPLAY, px, 1, E->title) > 520) px -= 2;   /* long titles shrink to fit */
    cv_font(c, F_DISPLAY, px); cv_letter_spacing(c, 1);
    cv_line_join(c, CV_JOIN_ROUND); cv_line_width(c, 7); cv_stroke_hex(c, INK, 1);
    cv_text_align(c, CV_LEFT); cv_text_baseline(c, CV_ALPHABETIC); cv_stroke_text(c, E->title, 2, 62);
    p_text(c, F_DISPLAY, px, 1, CV_LEFT, E->title, 2, 62, C_CREAM, 1);
    float x0 = 2;
    for (int i = 0; i < E->nfoe; i++) {
      char n[48]; snprintf(n, sizeof n, "%s", E->foe[i]);
      x0 += p_tape(c, x0, 76, 26, F_DISPLAY, 14, 1.5f, n, E->foe_col[i], INK) + 6;
    }
    if (E->best) {
      char s[32]; fmt_num(s, sizeof s, E->best);
      snprintf(b, sizeof b, "BEST %s", s);
      p_text(c, F_MONO, 16, 1, CV_LEFT, b, 4, 132, DIM, 1);
      if (E->best_rank) { char r[2] = {E->best_rank, 0}; p_text(c, F_DISPLAY, 26, 0, CV_LEFT, r, 200, 136, rank_col(E->best_rank), 1); }
    }
    layer_end(&L_tag, c);
  }
  hg_tex(&L_tag, x, y, a);
}

/* ---- the hub ---- */
static layer_t L_mode[BZM_COUNT];
static const char *mode_line(int m, char *b, size_t n) {
  char s[32];
  switch (m) {
    case BZM_LADDER:
      fmt_num(s, sizeof s, BZ.ladder_best);
      if (BZ.ladder_best) snprintf(b, n, "%d rungs by tier, three lives.  BEST %s %c", BZ.nopen + (BZ.secret_open ? 1 : 0), s, BZ.ladder_rank ? BZ.ladder_rank : ' ');
      else snprintf(b, n, "%d rungs by tier, three lives, the heat goes up.", BZ.nopen + (BZ.secret_open ? 1 : 0));
      break;
    case BZM_CHALLENGE: snprintf(b, n, "Any fight you have won. Your rules."); break;
    case BZM_SURVIVAL:
      fmt_num(s, sizeof s, BZ.surv_best);
      if (BZ.surv_wave_best) snprintf(b, n, "Everyone from the day, wave on wave.  BEST WAVE %d", BZ.surv_wave_best);
      else snprintf(b, n, "Everyone from the day, wave on wave.");
      break;
    default: snprintf(b, n, "Ranks, scores and what is still locked."); break;
  }
  return b;
}
static void draw_hub(float t) {
  frame(560, NULL, "{UPDOWN} MOVE    {PICK} SELECT    {BACK} TITLE", t);
  for (int m = 0; m < BZM_COUNT; m++) {
    bool s = m == BZ.hub_sel;
    char line[96]; mode_line(m, line, sizeof line);
    cv *c = layer_begin(&L_mode[m], 560, 96, hash_str(line, (uint32_t)m * 31 + (s ? 1 : 0)));
    if (c) {
      cv_fill_hex(c, s ? C_HIVIS : 0x1a1c26, s ? 1.f : .92f);
      cv_begin(c); cv_move(c, 26, 0); cv_line(c, 530, 0); cv_line(c, 504, 84); cv_line(c, 0, 84); cv_close(c); cv_fill(c);
      if (s) { cv_fill_hex(c, INK, 1); cv_fill_rect(c, 0, 84, 504, 5); }
      p_text(c, F_DISPLAY, 40, 1, CV_LEFT, battle_mode_name(m), 32, 48, s ? INK : C_CREAM, 1);
      p_text(c, F_TYPE, 15, 0, CV_LEFT, line, 34, 72, s ? INK : DIM, 1);
      char num[4]; snprintf(num, sizeof num, "0%d", m + 1);
      p_text(c, F_MONO, 15, 1, CV_RIGHT, num, 480, 26, s ? INK : C_HIVIS, 1);
      layer_end(&L_mode[m], c);
    }
    float in = ez(t * 3.5f - m * .18f);
    hg_tex(&L_mode[m], 40 + (1 - in) * -200 + (s ? 22 : 0) - m * 10, 172 + m * 104, in);
  }
  /* the attract: the fight standing behind */
  int e = BZ.hub_sel == BZM_CHALLENGE ? BZ.fight : -1;
  if (e < 0) { int k = (int)(ds_now() / 6), n = 0, list[BZ_MAXE]; for (int i = 0; i < BZ.ne; i++) if (BZ.e[i].unlocked) list[n++] = i; e = n ? list[k % n] : 0; }
  fight_tag(e, 700, 500, ez(t * 2));
}

/* ---- setup ---- */
static layer_t L_setup, L_blurb, L_go;
static void draw_setup(float t) {
  char crumb[40]; snprintf(crumb, sizeof crumb, "%s", battle_mode_name(BZ.mode));
  frame(600, crumb, "{UPDOWN} MOVE    {LEFTRIGHT} CHANGE    {PICK} TOGGLE, START    {BACK} BACK", t);
  static const char *WHO[3] = {"VEE", "DEX", "RUTH"};
  uint32_t key = (uint32_t)BZ.sel * 7919u ^ (BZ.mods << 4) ^ (BZ.mods_open << 12) ^ (uint32_t)BZ.who << 20 ^ (uint32_t)BZ.weapon << 23 ^ (uint32_t)BZ.fight << 26 ^ (uint32_t)BZ.mode * 131;
  const float RH = 34, Y0 = 6;
  cv *c = layer_begin(&L_setup, 560, 520, key);
  if (c) {
    float y = Y0;
    for (int r = 0; r < BZR_START; r++) {
      if (r == BZR_FIGHT && BZ.mode != BZM_CHALLENGE) continue;
      bool s = r == BZ.sel;
      if (r == BZR_MOD0) { p_text(c, F_MONO, 13, 3, CV_LEFT, "MODIFIERS", 20, y + 22, C_HIVIS, 1); y += 30; }
      cv_fill_hex(c, s ? C_HIVIS : 0x1a1c26, s ? 1.f : .9f);
      cv_begin(c); cv_move(c, 12, y); cv_line(c, 520, y); cv_line(c, 508, y + RH - 4); cv_line(c, 0, y + RH - 4); cv_close(c); cv_fill(c);
      uint32_t fg = s ? INK : C_CREAM;
      const char *label = r == BZR_FIGHTER ? "FIGHTER" : r == BZR_WEAPON ? "WEAPON" : r == BZR_FIGHT ? "FIGHT" : battle_mod_name(r - BZR_MOD0);
      if (r < BZR_MOD0) {
        p_text(c, F_MONO, 13, 2, CV_LEFT, label, 22, y + 21, s ? INK : DIM, 1);
        char v[80];
        if (r == BZR_FIGHTER) snprintf(v, sizeof v, "%s", WHO[BZ.who]);
        else if (r == BZR_WEAPON) snprintf(v, sizeof v, "%s", weapon_name((weapon_id)BZ.weapon));
        else snprintf(v, sizeof v, "%s", BZ.e[BZ.fight].title);
        p_text(c, F_DISPLAY, 17, 1, CV_LEFT, v, 132, y + 22, fg, 1);
        if (s) { p_text(c, F_DISPLAY, 17, 0, CV_RIGHT, "<   >", 500, y + 22, INK, 1); }
        if (r == BZR_WEAPON) { cv_fill_hex(c, weapon_color(BZ.weapon), 1); cv_fill_rect(c, 118, y + 8, 5, RH - 20); }
      } else {
        int i = r - BZR_MOD0; unsigned b = 1u << i;
        bool open = (BZ.mods_open & b) != 0, on = (BZ.mods & b) != 0;
        /* the switch */
        cv_fill_hex(c, s ? INK : 0x2c2f3c, 1); cv_fill_rect(c, 22, y + 8, 30, 14);
        if (open) { cv_fill_hex(c, on ? (s ? 0xffffff : C_HIVIS) : 0x5a5e6c, 1); cv_fill_rect(c, on ? 38 : 24, y + 10, 12, 10); }
        p_text(c, F_DISPLAY, 16, 1.5f, CV_LEFT, label, 66, y + 21, open ? fg : (s ? INK : 0x60646f), 1);
        char m[24];
        if (open) { snprintf(m, sizeof m, "x%.2f", battle_mult(b)); p_text(c, F_MONO, 14, 0, CV_RIGHT, m, 500, y + 21, s ? INK : (on ? C_HIVIS : DIM), 1); }
        else p_text(c, F_MONO, 12, 0, CV_RIGHT, battle_mod_lock(i), 500, y + 21, s ? INK : 0x70747f, 1);
      }
      y += RH;
    }
    layer_end(&L_setup, c);
  }
  float in = ez(t * 3);
  hg_tex(&L_setup, 40 - (1 - in) * 120, 140, in);
  /* the selected modifier's line, and the start plate with the multiplier */
  int r = BZ.sel;
  const char *bl = r >= BZR_MOD0 && r < BZR_START ? battle_mod_blurb(r - BZR_MOD0) :
                   r == BZR_FIGHTER ? "Who walks in. The day's three keyholders." : r == BZR_WEAPON ? "What you start with. The rest are a switch away." :
                   r == BZR_FIGHT ? "Any fight you have won in the day." :
                   BZ.mode == BZM_LADDER ? "Every fight by tier. The heat climbs; health carries over." :
                   BZ.mode == BZM_SURVIVAL ? "Mixed foes from every fight. A boss each fifth wave." : "One fight, your rules. Best scores are kept.";
  c = layer_begin(&L_blurb, 620, 34, hash_str(bl, 5));
  if (c) { p_text(c, F_TYPE, 17, 0, CV_LEFT, bl, 6, 23, C_CREAM, 1); layer_end(&L_blurb, c); }
  float sy = BZ.mode == BZM_CHALLENGE ? 140 + 34 * 11 + 30 + 14 : 140 + 34 * 10 + 30 + 14;
  hg_tex(&L_blurb, 44, sy + 66, in);
  bool s = r == BZR_START;
  char go[64]; snprintf(go, sizeof go, "START %s", battle_mode_name(BZ.mode));
  char mul[24]; snprintf(mul, sizeof mul, "SCORE x%.2f", battle_mult(BZ.mods));
  c = layer_begin(&L_go, 560, 64, hash_str(go, hash_str(mul, s ? 9 : 4)));
  if (c) {
    cv_fill_hex(c, s ? C_HIVIS : 0x1a1c26, 1);
    cv_begin(c); cv_move(c, 18, 0); cv_line(c, 540, 0); cv_line(c, 522, 56); cv_line(c, 0, 56); cv_close(c); cv_fill(c);
    if (!s) { cv_fill_hex(c, C_HIVIS, 1); cv_fill_rect(c, 0, 52, 522, 4); }
    p_text(c, F_DISPLAY, 28, 1, CV_LEFT, go, 30, 39, s ? INK : C_CREAM, 1);
    p_text(c, F_MONO, 15, 1, CV_RIGHT, mul, 506, 37, s ? INK : C_HIVIS, 1);
    layer_end(&L_go, c);
  }
  float pulse = s ? 1 + .015f * sinf(t * 7) : 1;
  float m[6]; mat_place(m, 40 + 270 + (s ? 14 : 0), sy + 28, 0, 280, 32, pulse, pulse); hg_tex_m(&L_go, m, in, 0xffffff);
  int e = BZ.mode == BZM_CHALLENGE ? BZ.fight : -1;
  if (e >= 0) fight_tag(e, 690, 520, in);
}

/* ---- the versus card before each rung ---- */
static layer_t L_vs, L_rt, L_rs, L_rfoe, L_rme, L_rinfo, L_rmods;
static void draw_rung(float t) {
  float in = ez(t * 2.6f);
  /* slashes in from the corners */
  hg_vgrad(0, 200, INK, .7f, 0);
  hg_vgrad(430, 720, INK, 0, .85f);
  float sl = ez(t * 4);
  hg_para(-200 + (1 - sl) * -900, 470, 900, 86, SK * .6f, INK, .9f);
  hg_para(-200 + (1 - sl) * -900, 552, 900, 5, SK * .6f, C_HIVIS, 1);
  hg_para(620 + (1 - sl) * 900, 470, 900, 86, SK * .6f, INK, .9f);
  hg_para(620 + (1 - sl) * 900, 552, 900, 5, SK * .6f, BZ.rung_boss ? C_ALERT : HOT, 1);
  /* the rung's number */
  char top[48];
  if (BZ.mode == BZM_LADDER) snprintf(top, sizeof top, "RUNG %d / %d", BZ.rung + 1, BZ.nrungs);
  else if (BZ.mode == BZM_SURVIVAL) snprintf(top, sizeof top, "%s", BZ.rung_title);
  else snprintf(top, sizeof top, "CHALLENGE");
  tl(&L_rt, F_DISPLAY, 46, 2, top, C_CREAM, INK, 0xc1);
  tex_lean(&L_rt, 40 - (1 - in) * 60, 26, 1, in);
  /* the fight's title across the top right */
  const char *ti = BZ.mode == BZM_SURVIVAL ? "SURVIVAL" : BZ.rung_title;
  tl(&L_rs, F_DISPLAY, 24, 2, ti, C_HIVIS, INK, 0xc2);
  hg_tex(&L_rs, 46, 92, in);
  /* score, lives, carried health */
  char info[96]; char sc[24]; fmt_num(sc, sizeof sc, BZ.score);
  if (BZ.mode == BZM_LADDER) snprintf(info, sizeof info, "SCORE %s   LIVES %d   HEALTH %d%%", sc, BZ.lives, (int)(BZ.carry_hp * 100 + .5f));
  else if (BZ.mode == BZM_SURVIVAL) snprintf(info, sizeof info, "SCORE %s   HEALTH %d%%", sc, (int)(BZ.carry_hp * 100 + .5f));
  else snprintf(info, sizeof info, "%s", BZ.rung_sub ? BZ.rung_sub : "");
  cv *c = layer_begin(&L_rinfo, 700, 36, hash_str(info, 3));
  if (c) { p_text(c, F_MONO, 17, 1, CV_RIGHT, info, 690, 25, C_CREAM, 1); layer_end(&L_rinfo, c); }
  hg_tex(&L_rinfo, DS_W - 740, 34, in);
  /* the modifiers in play as tapes */
  char mk[160] = ""; for (int i = 0; i < BZ_NMOD; i++) if (BZ.run_mods & (1u << i)) { size_t l = strlen(mk); snprintf(mk + l, sizeof mk - l, "%s|", battle_mod_name(i)); }
  c = layer_begin(&L_rmods, 900, 34, hash_str(mk, BZ.run_mods + 77));
  if (c) {
    float x = 890; char tmp[160]; snprintf(tmp, sizeof tmp, "%s", mk);
    char mul[24]; snprintf(mul, sizeof mul, "x%.2f", battle_mult(BZ.run_mods));
    float w = text_w(F_MONO, 13, 1.5f, mul) + 24; x -= w; p_tape(c, x, 4, 24, F_MONO, 13, 1.5f, mul, C_HIVIS, INK);
    for (char *tok = strtok(tmp, "|"); tok; tok = strtok(NULL, "|")) {
      float tw = text_w(F_MONO, 13, 1.5f, tok) + 26; x -= tw; p_tape(c, x, 4, 24, F_MONO, 13, 1.5f, tok, HOT, INK);
    }
    layer_end(&L_rmods, c);
  }
  hg_tex(&L_rmods, DS_W - 940, 70, in);
  /* the names on their slashes */
  static const char *WHO[3] = {"VEE", "DEX", "RUTH"};
  char me[64]; snprintf(me, sizeof me, "%s", WHO[BZ.who]);
  c = layer_begin(&L_rme, 560, 90, hash_str(me, (uint32_t)BZ.weapon));
  if (c) {
    p_text(c, F_DISPLAY, 50, 2, CV_LEFT, me, 6, 54, C_CREAM, 1);
    p_text(c, F_DISPLAY, 17, 2, CV_LEFT, weapon_name((weapon_id)BZ.weapon), 10, 80, weapon_color(BZ.weapon), 1);
    layer_end(&L_rme, c);
  }
  hg_tex(&L_rme, 60 + (1 - sl) * -400, 466, 1);
  char fn[128] = ""; for (int i = 0; i < BZ.rung_nfoe; i++) { size_t l = strlen(fn); snprintf(fn + l, sizeof fn - l, "%s%s", i ? "  " : "", BZ.rung_foe[i]); }
  c = layer_begin(&L_rfoe, 640, 90, hash_str(fn, hash_str(BZ.rung_arena ? BZ.rung_arena : "", 9)));
  if (c) {
    float px = BZ.rung_nfoe > 1 ? 30 : 46;
    p_text(c, F_DISPLAY, px, 1, CV_RIGHT, fn, 630, 54, C_CREAM, 1);
    p_text(c, F_DISPLAY, 17, 2, CV_RIGHT, BZ.rung_arena ? BZ.rung_arena : "", 626, 80, BZ.rung_boss ? C_ALERT : HOT, 1);
    layer_end(&L_rfoe, c);
  }
  hg_tex(&L_rfoe, DS_W - 680 + (1 - sl) * 400, 466, 1);
  /* VS: slams in */
  float k = ez((t - .25f) / .18f), vsc = ds_lerp(3.2f, 1, k);
  if (t > .25f) {
    tl(&L_vs, F_DISPLAY, 120, 0, "VS", C_CREAM, INK, 0xc3);
    float m[6]; mat_place(m, 640, 470, -.08f, L_vs.w * .5f, L_vs.h * .5f, vsc, vsc); m[2] -= .18f * vsc;
    hg_tex_m(&L_vs, m, k, 0xffffff);
    if (t < .5f) hg_rect(0, 0, DS_W, DS_H, 0xffffff, .5f * (1 - (t - .25f) / .25f));
  }
  /* the count to the fight */
  float left = ds_clamp(1 - t / 9, 0, 1);
  hg_rect(0, DS_H - 6, DS_W * left, 6, C_HIVIS, .9f);
  bool p = pad();
  c = layer_begin(&L_hint, 1200, 40, hash_str("rung", p ? 7 : 3));
  if (c) { g_markup(c, 600, 27, CV_CENTER, F_DISPLAY, 17, 1.5f, C_CREAM, 28, p, "{PICK} FIGHT     {BACK} MENU", true); layer_end(&L_hint, c); }
  hg_tex(&L_hint, 40, DS_H - 56, .7f + .3f * sinf(t * 5));
}

/* ---- results ---- */
static layer_t L_res_h, L_rank, L_stats, L_score, L_best, L_best2, L_run, L_runrank, L_unl[4], L_rh;
static void draw_result(float t) {
  hg_rect(0, 0, DS_W, DS_H, INK, .25f);
  hg_para(-120, 0, 820, DS_H, -.2f, INK, .88f);
  hg_stripes(0, 0, 690, DS_H, 0xffffff, .02f, 0, 0, 30, t * .5f);
  hg_para(686, 0, 8, DS_H, -.2f, BZ.won ? C_HIVIS : C_ALERT, 1);
  const char *h = BZ.won ? (BZ.mode == BZM_SURVIVAL ? "WAVE CLEAR" : "K.O.") : "TAPPED OUT";
  tl(&L_res_h, F_DISPLAY, 54, 2, h, BZ.won ? C_CREAM : 0xffc0b0, INK, 0xd1);
  float in = ez(t * 3);
  tex_lean(&L_res_h, 40 - (1 - in) * 100, 30, 1, in);
  char sub[96];
  if (BZ.mode == BZM_SURVIVAL) snprintf(sub, sizeof sub, "WAVE %d", BZ.wave);
  else snprintf(sub, sizeof sub, "%s", BZ.cur >= 0 ? BZ.e[BZ.cur].title : "");
  tl(&L_rh, F_DISPLAY, 20, 2, sub, C_HIVIS, 0, 0xd2);
  hg_tex(&L_rh, 46, 104, in);
  /* the stats, one by one */
  const fight_result *r = &BZ.r;
  char st[6][64];
  snprintf(st[0], 64, "TIME            %d:%02d", (int)r->time / 60, (int)r->time % 60);
  snprintf(st[1], 64, "DAMAGE TAKEN    %d", (int)(r->damage_taken + .5f));
  snprintf(st[2], 64, "BEST COMBO      %d", r->max_combo);
  snprintf(st[3], 64, "PARRIES         %d", r->parries);
  snprintf(st[4], 64, "PERFECT DODGES  %d", r->perfect_dodges);
  snprintf(st[5], 64, "MODIFIERS       x%.2f", BZ.mult);
  int shown = (int)ds_clamp((t - .2f) * 9, 0, 6);
  cv *c = layer_begin(&L_stats, 460, 220, 0xd300u + (uint32_t)shown + hash_str(st[0], hash_str(st[1], (uint32_t)r->max_combo * 7 + r->parries)));
  if (c) { for (int i = 0; i < shown; i++) p_text(c, F_MONO, 18, 0, CV_LEFT, st[i], 4, 24 + i * 32, i == 5 ? C_HIVIS : C_CREAM, 1); layer_end(&L_stats, c); }
  hg_tex(&L_stats, 52, 150, 1);
  /* the score counts up */
  float ct = ds_clamp((t - .9f) / 1.1f, 0, 1);
  uint32_t v = (uint32_t)(BZ.fight_score * ez(ct));
  char sc[32]; fmt_num(sc, sizeof sc, v);
  tl(&L_score, F_DISPLAY, 58, 1, sc, C_CREAM, INK, 0xd4);
  if (t > .9f) hg_tex(&L_score, 44, 372, 1);
  /* the rank slams in */
  char rk[2] = { BZ.won ? (r->rank ? r->rank : 'D') : 'D', 0 };
  float rt = t - 2.1f;
  if (rt > 0) {
    tl(&L_rank, F_DISPLAY, 210, 0, rk, rank_col(rk[0]), INK, 0xd5);
    float k = ez(rt / .14f), sc2 = ds_lerp(3.4f, 1, k);
    float shake = rt < .35f ? (1 - rt / .35f) * 10 * sinf(rt * 90) : 0;
    float m[6]; mat_place(m, 520 + shake, 270, -.1f, L_rank.w * .5f, L_rank.h * .5f, sc2, sc2); m[2] -= .14f * sc2;
    hg_circle(520, 290, 150 * k, rank_col(rk[0]), .12f);
    hg_tex_m(&L_rank, m, k, 0xffffff);
    if (rt < .25f) hg_rect(0, 0, DS_W, DS_H, 0xffffff, .55f * (1 - rt / .25f));
  }
  /* NEW BEST flashes */
  if (BZ.new_best && t > 2.4f) {
    tl(&L_best, F_DISPLAY, 24, 3, "NEW BEST", INK, 0, 0xd6);
    bool on = fmodf(t * 3, 1) < .6f;
    hg_para(40, 448, L_best.w + 30, 38, SK * .5f, on ? HOT : C_HIVIS, 1);
    hg_tex(&L_best, 52, 448, 1);
  }
  /* the run, when it is over */
  float y = 510;
  if (BZ.run_over && BZ.mode != BZM_CHALLENGE) {
    char run[96]; char tot[32]; fmt_num(tot, sizeof tot, (uint32_t)(BZ.score * ez(ds_clamp((t - 2.8f) / 1.2f, 0, 1))));
    snprintf(run, sizeof run, "%s  %s", BZ.mode == BZM_LADDER ? (BZ.run_cleared ? "LADDER CLEARED" : "RUN OVER") : "RUN OVER", tot);
    c = layer_begin(&L_run, 640, 50, hash_str(run, 1));
    if (c) { p_text(c, F_DISPLAY, 30, 1, CV_LEFT, run, 6, 38, C_CREAM, 1); layer_end(&L_run, c); }
    if (t > 2.8f) hg_tex(&L_run, 44, y, 1);
    if (t > 4.1f) {
      char rr[2] = { BZ.run_rank ? BZ.run_rank : 'D', 0 };
      tl(&L_runrank, F_DISPLAY, 64, 0, rr, rank_col(rr[0]), INK, 0xd7);
      float k = ez((t - 4.1f) / .14f), s3 = ds_lerp(3, 1, k);
      float rx = 44 + text_w(F_DISPLAY, 30, 1, run) + 56;
      float m[6]; mat_place(m, rx, y + 26, -.1f, L_runrank.w * .5f, L_runrank.h * .5f, s3, s3); hg_tex_m(&L_runrank, m, k, 0xffffff);
      if (BZ.new_run_best) {
        tl(&L_best2, F_DISPLAY, 18, 3, "NEW BEST RUN", INK, 0, 0xd8);
        bool on = fmodf(t * 3, 1) < .6f;
        hg_para(40, y + 52, L_best2.w + 24, 30, SK * .5f, on ? HOT : C_HIVIS, 1);
        hg_tex(&L_best2, 50, y + 52, 1);
      }
    }
    y += BZ.new_run_best ? 30 : 0;
    y += 60;
  }
  for (int i = 0; i < BZ.nunlocked; i++) {
    char u[80]; snprintf(u, sizeof u, "UNLOCKED  %s", BZ.unlocked_msg[i]);
    c = layer_begin(&L_unl[i], 640, 34, hash_str(u, 5));
    if (c) { p_tape(c, 0, 2, 28, F_MONO, 14, 1.5f, u, 0xffd23a, INK); layer_end(&L_unl[i], c); }
    float a = ez((t - 3 - i * .3f) * 3);
    hg_tex(&L_unl[i], 44 + (1 - a) * -60, y + i * 36, a);
  }
  bool p = pad();
  const char *hint = !BZ.run_over ? (BZ.won ? (BZ.mode == BZM_SURVIVAL ? "{PICK} NEXT WAVE" : "{PICK} NEXT RUNG") : "{PICK} TRY THE RUNG AGAIN") : "{PICK} AGAIN     {BACK} MENU";
  c = layer_begin(&L_hint, 1200, 40, hash_str(hint, p ? 7 : 3));
  if (c) { g_markup(c, 6, 27, CV_LEFT, F_DISPLAY, 16, 1.5f, C_CREAM, 26, p, hint, true); layer_end(&L_hint, c); }
  if (t > .9f) { hg_rect(0, DS_H - 52, 760, 52, INK, .6f); hg_tex(&L_hint, 40, DS_H - 46, 1); }
}

/* ---- records ---- */
static layer_t L_rec, L_rtop;
static void draw_records(float t) {
  frame(900, "RECORDS", "{UPDOWN} MOVE    {BACK} BACK", t);
  char top[160], a[24], b[24];
  fmt_num(a, sizeof a, BZ.ladder_best); fmt_num(b, sizeof b, BZ.surv_best);
  snprintf(top, sizeof top, "LADDER  %s %c   CLEARS %d      SURVIVAL  %s   WAVE %d", a, BZ.ladder_rank ? BZ.ladder_rank : '-', BZ.ladder_clears, b, BZ.surv_wave_best);
  cv *c = layer_begin(&L_rtop, 900, 30, hash_str(top, 1));
  if (c) { p_text(c, F_MONO, 15, 1, CV_LEFT, top, 4, 21, C_CREAM, 1); layer_end(&L_rtop, c); }
  hg_tex(&L_rtop, 44, 132, 1);
  c = layer_begin(&L_rec, 860, 450, hash_str("rec", (uint32_t)BZ.sel * 31 + BZ.nopen + (BZ.secret_open ? 999 : 0) + BZ.weapons * 7 + BZ.ladder_best + BZ.who));
  if (c) {
    for (int i = 0; i < BZ.ne; i++) {
      const bz_entry *E = &BZ.e[i];
      float y = i * 40; bool s = i == BZ.sel;
      cv_fill_hex(c, s ? C_HIVIS : 0x1a1c26, s ? 1.f : .9f);
      cv_begin(c); cv_move(c, 10, y); cv_line(c, 850, y); cv_line(c, 840, y + 35); cv_line(c, 0, y + 35); cv_close(c); cv_fill(c);
      uint32_t fg = s ? INK : C_CREAM;
      char tier[12]; snprintf(tier, sizeof tier, E->secret ? "??" : "T%d", E->tier);
      p_text(c, F_MONO, 13, 1, CV_LEFT, tier, 20, y + 23, s ? INK : C_HIVIS, 1);
      if (!E->unlocked) {
        p_text(c, F_DISPLAY, 16, 1, CV_LEFT, E->secret ? "SECRET RUNG" : "LOCKED", 66, y + 24, s ? INK : 0x60646f, 1);
        p_text(c, F_MONO, 13, 0, CV_LEFT, E->secret ? "Rank A or better on a cleared ladder" : "Win it in the day to fight it here", 300, y + 23, s ? INK : 0x70747f, 1);
        continue;
      }
      p_text(c, F_DISPLAY, 16, 1, CV_LEFT, E->title, 66, y + 24, fg, 1);
      char sc[24]; if (E->best) fmt_num(sc, sizeof sc, E->best); else snprintf(sc, sizeof sc, "-");
      p_text(c, F_MONO, 15, 0, CV_RIGHT, sc, 720, y + 23, fg, 1);
      char w[16]; snprintf(w, sizeof w, "%dW", E->wins); p_text(c, F_MONO, 13, 0, CV_RIGHT, w, 780, y + 23, s ? INK : DIM, 1);
      if (E->best_rank) { char r[2] = {E->best_rank, 0}; p_text(c, F_DISPLAY, 24, 0, CV_CENTER, r, 818, y + 27, s ? INK : rank_col(E->best_rank), 1); }
    }
    layer_end(&L_rec, c);
  }
  float in = ez(t * 3);
  hg_tex(&L_rec, 40 - (1 - in) * 100, 172, in);
}

/* ---- pause ---- */
static layer_t L_pause;
static void draw_pause(void) {
  hg_rect(0, 0, DS_W, DS_H, INK, .7f);
  hg_para(-80, 0, 640, DS_H, -.18f, INK, .7f);
  hg_para(554, 0, 6, DS_H, -.18f, C_HIVIS, 1);
  const char *retry = BZ.mode == BZM_LADDER ? (BZ.lives > 1 ? "Retry the rung (a life)" : "Restart the ladder") : BZ.mode == BZM_SURVIVAL ? "Restart the run" : "Retry";
  cv *c = layer_begin(&L_pause, 560, 320, hash_str(retry, 600u + (unsigned)BZ.pause_sel));
  if (c) {
    p_text(c, F_DISPLAY, 50, 2, CV_LEFT, "PAUSED", 4, 54, C_CREAM, 1);
    p_tape(c, 8, 70, 24, F_MONO, 13, 2, battle_mode_name(BZ.mode), HOT, INK);
    const char *rows[3] = {"Resume", retry, "Back to the Battle Zone"};
    for (int i = 0; i < 3; i++) {
      bool s = i == BZ.pause_sel; float y = 116 + i * 60;
      cv_fill_hex(c, s ? C_HIVIS : 0x1a1c26, 1);
      cv_begin(c); cv_move(c, 16, y); cv_line(c, 480, y); cv_line(c, 464, y + 50); cv_line(c, 0, y + 50); cv_close(c); cv_fill(c);
      p_text(c, F_DISPLAY, 21, 1, CV_LEFT, rows[i], 26, y + 33, s ? INK : C_CREAM, 1);
    }
    layer_end(&L_pause, c);
  }
  hg_tex(&L_pause, 50, 130, 1);
}

void hud_battle_draw(void) {
  float t = BZ.t;
  hg_begin();
  switch (BZ.screen) {
    case BZ_HUB: draw_hub(t); break;
    case BZ_SETUP: draw_setup(t); break;
    case BZ_RUNG: draw_rung(t); break;
    case BZ_RESULT: draw_result(t); break;
    case BZ_RECORDS: draw_records(t); break;
    case BZ_PAUSE: draw_pause(); break;
    case BZ_FIGHT: break;
  }
  if (BZ.flash > .01f) hg_rect(0, 0, DS_W, DS_H, 0xffffff, BZ.flash * .6f);
  hg_end();
}

/* ---- the title's entry, right of the logo, over the street ---- */
static layer_t L_te;
void hud_battle_title(float t, bool sel) {
  bool open = battle_available();
  int n = 0; for (int i = 0; i < BZ.ne; i++) if (BZ.e[i].unlocked && !BZ.e[i].secret) n++;
  if (!BZ.ne) n = 0;
  hg_begin();
  char line[80];
  if (open) snprintf(line, sizeof line, "%d FIGHT%s OPEN", n, n == 1 ? "" : "S"); else snprintf(line, sizeof line, "Win a fight in the day to open it.");
  cv *c = layer_begin(&L_te, 400, 92, hash_str(line, (open ? 2 : 1) + (sel ? 10 : 0)));
  if (c) {
    uint32_t bg = sel ? (open ? C_HIVIS : 0x5a5e6c) : INK;
    cv_fill_hex(c, bg, sel ? 1.f : .82f);
    cv_begin(c); cv_move(c, 18, 0); cv_line(c, 392, 0); cv_line(c, 374, 62); cv_line(c, 0, 62); cv_close(c); cv_fill(c);
    cv_fill_hex(c, open ? HOT : 0x5a5e6c, 1); cv_fill_rect(c, 0, 58, 374, 4);
    p_text(c, F_DISPLAY, 30, 2, CV_LEFT, "BATTLE ZONE", 28, 47, sel ? INK : (open ? C_CREAM : DIM), 1);
    p_text(c, F_MONO, 11, 2, CV_RIGHT, open ? "" : "LOCKED", 356, 17, sel ? INK : DIM, 1);
    if (open) p_text(c, F_MONO, 12, 2, CV_RIGHT, line, 356, 17, sel ? INK : C_HIVIS, 1);
    else p_text(c, F_TYPE, 14, 0, CV_LEFT, line, 22, 84, C_CREAM, 1);
    layer_end(&L_te, c);
  }
  float a = ez(t * 1.5f - .4f);
  hg_tex(&L_te, 846 + (sel ? -14 : 0) + (1 - a) * 60, open ? 414 : 396, a);
  hg_end();
}
#endif
