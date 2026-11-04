/* hud_vn.c: everything 2D outside a fight. The dialogue box with its colored name plate, the choices,
 * chapter and title cards, the clock and objective while walking, the title screen, the pause menu and the
 * ending. Painted with cv once per change and composited with hud_gl. */
#include "hud_vn.h"
#include "hud_int.h"
#include "story.h"
#include "input.h"
#include <math.h>

#define ORANGE 0xff7a1au
#define INK 0x12121au
#define PAPER 0xf4eee2u
#define DIMT 0xa8a49au
static layer_t L_box, L_name, L_opt[5], L_prompt, L_ch, L_card, L_note, L_obj, L_clock, L_use, L_toast, L_got, L_stats, L_title, L_menu, L_cards[3], L_retry, L_end, L_mk;

static float ez(float t) { t = ds_clamp(t, 0, 1); return 1 - (1 - t) * (1 - t) * (1 - t); }

/* a word wrapped mono paragraph, returns lines; the line starts for reveal */
static int wrap(cv *c, const char *s, float w, char out[][120], int cap) {
  int n = 0, len = (int)strlen(s), i = 0; char cur[120]; cur[0] = 0; int cl = 0;
  while (i < len && n < cap) {
    int j = i; while (j < len && s[j] != ' ') j++;
    char t[240]; snprintf(t, sizeof t, "%s%s%.*s", cur, cl ? " " : "", j - i, s + i);
    if (cl && cv_measure(c, t) > w) { snprintf(out[n++], 120, "%s", cur); cur[0] = 0; cl = 0; continue; }
    snprintf(cur, sizeof cur, "%s", t); cl = (int)strlen(cur);
    i = j; while (i < len && s[i] == ' ') i++;
  }
  if (cl && n < cap) snprintf(out[n++], 120, "%s", cur);
  return n;
}

static char box_lines[6][120]; static int box_n;
/* IBM Plex Mono advances 0.6 em, plus the box text's letter spacing */
static float cv_measure_mono(float px) { return px * .6f + .2f; }
static void draw_dialogue(const story_ui_t *U) {
  const float X = 120, Y = 528, W = 1040, H = 146;
  uint32_t key = hash_str(U->line ? U->line : "", 11);
  hg_para(X, Y, W, H, .12f, INK, .88f);
  cv *c = layer_begin(&L_box, (int)W, (int)H, key);
  if (c) {
    cv_font(c, F_TYPE, 25); cv_letter_spacing(c, .2f);
    box_n = wrap(c, U->line, W - 92, box_lines, 5);
    float lh = box_n > 3 ? 27 : 33, y0 = box_n > 3 ? 32 : 44;
    for (int i = 0; i < box_n; i++) p_text(c, F_TYPE, box_n > 3 ? 23 : 25, .2f, CV_LEFT, box_lines[i], 46, y0 + i * lh, PAPER, 1);
    layer_end(&L_box, c);
  }
  /* typed on: each line's text is revealed left to right (the face is mono, so a character is a fixed width) */
  float shown = U->line_t * U->line_cps; float lh = box_n > 3 ? 27 : 33, y0 = box_n > 3 ? 32 : 44;
  float cw = cv_measure_mono(box_n > 3 ? 23 : 25);
  int done = 0;
  for (int i = 0; i < box_n; i++) {
    int n = (int)strlen(box_lines[i]);
    float vis = fminf(shown - done, (float)n + 1);
    if (vis > 0) hg_sub(&L_box, 40, (int)(y0 + i * lh - 25), (int)(6 + vis * cw), (int)lh + 2, X + 40, Y + y0 + i * lh - 25, 1);
    done += n + 1;
  }
  key = hash_str(U->speaker ? U->speaker : "", U->speaker_col);
  c = layer_begin(&L_name, 320, 44, key);
  if (c) {
    cv_font(c, F_DISPLAY, 21); cv_letter_spacing(c, 2);
    char up[64]; int k = 0; for (const char *p = U->speaker; p && *p && k < 63; p++) up[k++] = (*p >= 'a' && *p <= 'z') ? (char)(*p - 32) : *p; up[k] = 0;
    float tw = cv_measure(c, up) + 40;
    cv_fill_hex(c, U->speaker_col, 1);
    cv_begin(c); cv_move(c, 12, 0); cv_line(c, tw, 0); cv_line(c, tw - 12, 40); cv_line(c, 0, 40); cv_close(c); cv_fill(c);
    p_text(c, F_DISPLAY, 21, 2, CV_LEFT, up, 18, 28, INK, 1);
    layer_end(&L_name, c);
  }
  hg_tex(&L_name, X + 22, Y - 26, 1);
  if (U->line_done) hg_rect_m(X + W - 42, Y + H - 24 + sinf((float)ds_now() * 6) * 2, 11, 11, .785f, ORANGE, 1);
}
static void draw_choice(const story_ui_t *U) {
  const float W = 900, RH = 58, X = (DS_W - W) / 2;
  float top = 512 - U->nopt * (RH + 8) - 2;
  cv *c = layer_begin(&L_prompt, 1040, 96, hash_str(U->prompt ? U->prompt : "", 3));
  if (c) {
    cv_fill_hex(c, INK, .88f); cv_begin(c); cv_move(c, 18, 0); cv_line(c, 1040, 0); cv_line(c, 1022, 96); cv_line(c, 0, 96); cv_close(c); cv_fill(c);
    p_text(c, F_DISPLAY, 13, 3, CV_LEFT, "WHAT DO YOU DO", 44, 26, ORANGE, 1);
    cv_font(c, F_TYPE, 21); cv_letter_spacing(c, .1f);
    char lines[3][120]; int n = wrap(c, U->prompt ? U->prompt : "", 950, lines, 2);
    for (int i = 0; i < n; i++) p_text(c, F_TYPE, 21, .1f, CV_LEFT, lines[i], 44, 52 + i * 26, PAPER, 1);
    layer_end(&L_prompt, c);
  }
  hg_tex(&L_prompt, 120, 560, 1);
  for (int i = 0; i < U->nopt && i < 5; i++) {
    bool sel = i == U->sel;
    float in = ez(U->choose_t * 4 - i * .25f);
    float y = top + i * (RH + 8);
    float x = X + (1 - in) * 60 + (sel ? -10 : 0);
    uint32_t k = hash_str(U->opt[i], sel ? 91 : 17);
    c = layer_begin(&L_opt[i], (int)W, (int)RH, k);
    if (c) {
      cv_fill_hex(c, sel ? ORANGE : INK, sel ? 1.f : .84f);
      cv_begin(c); cv_move(c, 16, 0); cv_line(c, W, 0); cv_line(c, W - 16, RH); cv_line(c, 0, RH); cv_close(c); cv_fill(c);
      cv_font(c, F_TYPE, 21); cv_letter_spacing(c, 0);
      char lines[3][120]; int n = wrap(c, U->opt[i], W - 80, lines, 2);
      uint32_t col = sel ? INK : PAPER;
      if (n == 1) p_text(c, F_TYPE, 21, 0, CV_LEFT, lines[0], 40, 37, col, 1);
      else { p_text(c, F_TYPE, 19, 0, CV_LEFT, lines[0], 40, 24, col, 1); p_text(c, F_TYPE, 19, 0, CV_LEFT, lines[1], 34, 47, col, 1); }
      layer_end(&L_opt[i], c);
    }
    hg_tex(&L_opt[i], x, y, in);
  }
}
static void fmt_hour(char *b, size_t n, float h) {
  int t = (int)(h * 60 + .5f), hh = t / 60 % 24, mm = t % 60;
  int d = hh % 12 ? hh % 12 : 12;
  snprintf(b, n, "%d:%02d %s", d, mm / 5 * 5, hh < 12 ? "AM" : "PM");
}
static void draw_chapter(const story_ui_t *U) {
  float t = U->ch_t, a = ez(t * 2.5f) * ds_clamp((U->ch_dur - t) * 2.5f, 0, 1);
  cv *c = layer_begin(&L_ch, 900, 150, hash_str(U->ch_title, hash_str(U->ch_sub ? U->ch_sub : "", 5)));
  if (c) {
    char tm[32]; fmt_hour(tm, sizeof tm, U->ch_hour);
    p_text(c, F_MONO, 24, 2, CV_LEFT, tm, 16, 32, ORANGE, 1);
    p_text(c, F_DISPLAY, 58, 1, CV_LEFT, U->ch_title, 12, 94, PAPER, 1);
    p_text(c, F_TYPE, 22, 0, CV_LEFT, U->ch_sub ? U->ch_sub : "", 16, 132, PAPER, .85f);
    layer_end(&L_ch, c);
  }
  float x = 70 + (1 - ez(t * 3)) * -70;
  hg_grad(x + 300, 186, 900, 190, 0, INK, .8f * a, 0);
  hg_rect(x - 12, 112, 6, 150 * ez(t * 4), ORANGE, a);
  hg_tex(&L_ch, x, 112, a);
}
static void draw_card(const story_ui_t *U) {
  float t = U->card_t, a = ez(t * 3) * ds_clamp((U->card_dur - t) * 3, 0, 1);
  hg_rect(0, 0, DS_W, DS_H, INK, a * (U->card_title && U->card_title[0] ? .55f : .75f));
  cv *c = layer_begin(&L_card, 1200, 180, hash_str(U->card_title, hash_str(U->card_sub ? U->card_sub : "", 9)));
  if (c) {
    if (U->card_title[0]) p_text(c, F_DISPLAY, 86, 3, CV_CENTER, U->card_title, 600, 90, PAPER, 1);
    p_text(c, F_TYPE, 25, 0, CV_CENTER, U->card_sub ? U->card_sub : "", 600, 140, U->card_title[0] ? ORANGE : PAPER, 1);
    layer_end(&L_card, c);
  }
  hg_tex(&L_card, 40, 270 + (1 - ez(t * 3)) * 14, a);
}
static void draw_note(const story_ui_t *U) {
  float a = ds_clamp(U->note_t * 2, 0, 1) * ds_clamp((4 - U->note_t) * 3, 0, 1);
  cv *c = layer_begin(&L_note, 1120, 72, hash_str(U->note, 13));
  if (c) {
    cv_font(c, F_TYPE, 21); char lines[3][120]; int n = wrap(c, U->note, 1040, lines, 2);
    for (int i = 0; i < n; i++) p_text(c, F_TYPE, 21, 0, CV_CENTER, lines[i], 560, 26 + i * 28, PAPER, 1);
    layer_end(&L_note, c);
  }
  hg_vgrad(80, 170, INK, 0, 0);
  hg_rect(80, 76, 1120, 74, INK, .55f * a);
  hg_tex(&L_note, 80, 78, a);
}
static void draw_roam(const story_ui_t *U) {
  char tm[32]; fmt_hour(tm, sizeof tm, U->hour);
  cv *c = layer_begin(&L_clock, 260, 92, hash_str(tm, (U->stats[ST_TRUST] << 16) ^ (U->stats[ST_SAFETY] << 12) ^ (U->stats[ST_VIBE] << 8) ^ (U->stats[ST_DUES] << 4) ^ U->stats[ST_ENERGY]));
  if (c) {
    cv_fill_hex(c, INK, .7f); cv_begin(c); cv_move(c, 16, 0); cv_line(c, 260, 0); cv_line(c, 260, 92); cv_line(c, 0, 92); cv_close(c); cv_fill(c);
    p_text(c, F_DISPLAY, 30, 1, CV_RIGHT, tm, 244, 38, PAPER, 1);
    static const char *lab[4] = {"TRUST", "SAFETY", "VIBE", "DUES"};
    static const int idx[4] = {ST_TRUST, ST_SAFETY, ST_VIBE, ST_DUES};
    for (int i = 0; i < 4; i++) {
      float x = 34 + i * 54, v = ds_clamp(U->stats[idx[i]] / 10.f, 0, 1);
      p_text(c, F_MONO, 9, .5f, CV_LEFT, lab[i], x, 58, DIMT, 1);
      cv_fill_hex(c, 0x3a3a44, 1); cv_fill_rect(c, x, 64, 46, 7);
      cv_fill_hex(c, ORANGE, 1); cv_fill_rect(c, x, 64, 46 * v, 7);
    }
    layer_end(&L_clock, c);
  }
  hg_tex(&L_clock, DS_W - 260, 20, 1);
  if (U->objective) {
    c = layer_begin(&L_obj, 720, 44, hash_str(U->objective, 21));
    if (c) {
      cv_font(c, F_TYPE, 20);
      float w = cv_measure(c, U->objective) + 60;
      cv_fill_hex(c, INK, .7f); cv_begin(c); cv_move(c, 0, 0); cv_line(c, w + 16, 0); cv_line(c, w, 40); cv_line(c, 0, 40); cv_close(c); cv_fill(c);
      cv_fill_hex(c, ORANGE, 1); cv_fill_rect(c, 0, 0, 6, 40);
      p_text(c, F_TYPE, 20, 0, CV_LEFT, U->objective, 22, 27, PAPER, 1);
      layer_end(&L_obj, c);
    }
    hg_tex(&L_obj, 0, 24, 1);
  }
  if (U->marker_on) {
    float x = U->marker_x, y = U->marker_y;
    if (U->marker_behind) { y = DS_H - 90; x = x < DS_W / 2 ? DS_W - 70 : 70; }
    x = ds_clamp(x, 60, DS_W - 60); y = ds_clamp(y, 110, DS_H - 90);
    float bob = sinf((float)ds_now() * 4) * 4;
    hg_rect_m(x, y - 18 + bob, 15, 15, .785f, ORANGE, 1);
    hg_rect_m(x, y - 18 + bob, 7, 7, .785f, INK, 1);
    char d[24]; snprintf(d, sizeof d, "%d m", (int)(U->marker_dist + .5f));
    c = layer_begin(&L_mk, 90, 24, hash_str(d, 3));
    if (c) { p_text(c, F_MONO, 15, 0, CV_CENTER, d, 45, 17, PAPER, 1); layer_end(&L_mk, c); }
    hg_tex(&L_mk, x - 45, y - 2 + bob, .95f);
  }
  if (U->prompt_use) {
    const intent_t *in = input_get();
    char s[120]; snprintf(s, sizeof s, "%s   %s", in->used_pad ? "X" : "E", U->prompt_use);
    c = layer_begin(&L_use, 620, 54, hash_str(s, 41));
    if (c) {
      cv_font(c, F_DISPLAY, 22); float w = cv_measure(c, s) + 56, x0 = 310 - w / 2;
      cv_fill_hex(c, ORANGE, 1); cv_begin(c); cv_move(c, x0 + 14, 2); cv_line(c, x0 + w, 2); cv_line(c, x0 + w - 14, 50); cv_line(c, x0, 50); cv_close(c); cv_fill(c);
      p_text(c, F_DISPLAY, 22, 0, CV_CENTER, s, 310, 34, INK, 1);
      layer_end(&L_use, c);
    }
    hg_tex(&L_use, 330, 560, 1);
  }
}
static void draw_toasts(const story_ui_t *U) {
  if (U->toast_t > 0 && U->toast) {
    char s[40]; snprintf(s, sizeof s, "%s %s%d", U->toast, U->toast_n > 0 ? "+" : "", U->toast_n);
    cv *c = layer_begin(&L_toast, 240, 40, hash_str(s, 3));
    if (c) { p_text(c, F_DISPLAY, 20, 2, CV_RIGHT, s, 232, 28, U->toast_n > 0 ? 0x8fe388 : 0xff8a80, 1); layer_end(&L_toast, c); }
    float a = ds_clamp(U->toast_t * 2, 0, 1) * ds_clamp((2.4f - U->toast_t) * 6, 0, 1);
    hg_rect(DS_W - 250, 112, 250, 40, INK, .6f * a);
    hg_tex(&L_toast, DS_W - 250, 112, a);
  }
  if (U->got_t > 0 && U->got) {
    char s[60]; snprintf(s, sizeof s, "NEW WEAPON  %s", U->got);
    cv *c = layer_begin(&L_got, 620, 56, hash_str(s, 7));
    if (c) { cv_fill_hex(c, ORANGE, 1); cv_fill_rect(c, 0, 0, 620, 52); p_text(c, F_DISPLAY, 25, 2, CV_CENTER, s, 310, 36, INK, 1); layer_end(&L_got, c); }
    float a = ds_clamp(U->got_t * 2, 0, 1) * ds_clamp((3 - U->got_t) * 5, 0, 1);
    hg_tex(&L_got, 330, 100, a);
  }
}
static void draw_retry(const story_ui_t *U) {
  hg_rect(0, 0, DS_W, DS_H, INK, .7f);
  cv *c = layer_begin(&L_retry, 760, 280, 77 + U->retry_sel);
  if (c) {
    p_text(c, F_DISPLAY, 48, 2, CV_CENTER, "YOU ARE ON THE FLOOR", 380, 60, PAPER, 1);
    p_text(c, F_TYPE, 20, 0, CV_CENTER, "The lab is still open. Somebody has to get up.", 380, 100, DIMT, 1);
    const char *o[2] = {"Get up. Again.", "Talk them down. It costs you."};
    for (int i = 0; i < 2; i++) {
      bool s = i == U->retry_sel;
      cv_fill_hex(c, s ? ORANGE : 0x2a2a34, 1); cv_fill_rect(c, 130, 140 + i * 62, 500, 52);
      p_text(c, F_TYPE, 22, 0, CV_CENTER, o[i], 380, 174 + i * 62, s ? INK : PAPER, 1);
    }
    layer_end(&L_retry, c);
  }
  hg_tex(&L_retry, 260, 210, 1);
}

void hud_vn_draw(void) {
  const story_ui_t *U = &STORY_UI;
  hg_begin();
  if (U->dim > .01f) hg_rect(0, 0, DS_W, DS_H, 0x05060c, U->dim);
  if (U->letterbox > .01f) { float h = 62 * U->letterbox; hg_rect(0, 0, DS_W, h, 0x000000, 1); hg_rect(0, DS_H - h, DS_W, h, 0x000000, 1); }
  if (U->roam) draw_roam(U);
  if (U->chapter) draw_chapter(U);
  if (U->note) draw_note(U);
  if (U->talking && U->line) draw_dialogue(U);
  if (U->choosing) draw_choice(U);
  draw_toasts(U);
  if (U->skip > 0) { hg_rect(DS_W - 160, DS_H - 40, 130 * U->skip, 5, ORANGE, 1); }
  if (U->card) draw_card(U);
  if (U->retry) draw_retry(U);
  if (U->flash > .01f) hg_rect(0, 0, DS_W, DS_H, 0xffffff, U->flash);
  if (U->fade > .01f) hg_rect(0, 0, DS_W, DS_H, 0x000000, U->fade);
  hg_end();
}

void hud_title_draw(float t, int row, int who, bool has_save, const char *const *names, const char *const *blurbs) {
  hg_begin();
  hg_vgrad(0, 300, INK, .75f, 0);
  hg_vgrad(420, 720, INK, 0, .92f);
  cv *c = layer_begin(&L_title, 1100, 170, 1);
  if (c) {
    p_text(c, F_DISPLAY, 112, 4, CV_LEFT, "KEYHOLDER", 0, 104, PAPER, 1);
    p_text(c, F_TYPE, 24, 0, CV_LEFT, "One day hosting at HeatSync Labs. 9 am to midnight. Somebody has to.", 6, 146, ORANGE, 1);
    layer_end(&L_title, c);
  }
  float a = ez(t * 1.5f);
  hg_tex(&L_title, 70 - (1 - a) * 40, 44, a);
  for (int i = 0; i < 3; i++) {
    bool s = i == who;
    c = layer_begin(&L_cards[i], 330, 112, hash_str(names[i], (s ? 31 : 7) + (row == 0 ? 100 : 0)));
    if (c) {
      cv_fill_hex(c, s ? ORANGE : INK, s ? 1.f : .8f);
      cv_begin(c); cv_move(c, 14, 0); cv_line(c, 330, 0); cv_line(c, 316, 112); cv_line(c, 0, 112); cv_close(c); cv_fill(c);
      p_text(c, F_DISPLAY, 32, 2, CV_LEFT, names[i], 26, 44, s ? INK : PAPER, 1);
      cv_font(c, F_TYPE, 15); char lines[3][120]; int n = wrap(c, blurbs[i], 280, lines, 2);
      for (int k = 0; k < n; k++) p_text(c, F_TYPE, 15, 0, CV_LEFT, lines[k], 24, 70 + k * 20, s ? INK : DIMT, 1);
      layer_end(&L_cards[i], c);
    }
    hg_tex(&L_cards[i], 90 + i * 370, 500 + (s ? -6 : 0), 1);
  }
  const intent_t *in = input_get();
  char m[120];
  snprintf(m, sizeof m, has_save ? "%s  start a new day     %s  continue" : "%s  start the day", in->used_pad ? "A" : "ENTER", in->used_pad ? "Y" : "C");
  c = layer_begin(&L_menu, 800, 40, hash_str(m, 1));
  if (c) { p_text(c, F_DISPLAY, 19, 1, CV_CENTER, m, 400, 28, PAPER, 1); layer_end(&L_menu, c); }
  hg_tex(&L_menu, 240, 642, .6f + .4f * sinf(t * 3));
  hg_end();
}

void hud_pause_draw(int sel, bool music) {
  hg_begin();
  hg_rect(0, 0, DS_W, DS_H, INK, .72f);
  const intent_t *in = input_get();
  cv *c = layer_begin(&L_menu, 1000, 560, 500u + (unsigned)sel * 3 + (music ? 1 : 0) + (in->used_pad ? 50 : 0));
  if (c) {
    p_text(c, F_DISPLAY, 50, 2, CV_LEFT, "PAUSED", 0, 54, PAPER, 1);
    const char *rows[3] = {"Back to the day", music ? "Music on" : "Music off", "Leave (the day saves at each chapter)"};
    for (int i = 0; i < 3; i++) {
      bool s = i == sel;
      cv_fill_hex(c, s ? ORANGE : 0x24242e, 1); cv_fill_rect(c, 0, 84 + i * 56, 470, 48);
      p_text(c, F_TYPE, 19, 0, CV_LEFT, rows[i], 18, 115 + i * 56, s ? INK : PAPER, 1);
    }
    static const char *K[][2] = { {"Move", "WASD"}, {"Camera", "Arrows, mouse"}, {"Light attack", "J, F, click"}, {"Heavy attack", "K"}, {"Guard, parry", "L, right click"}, {"Dodge", "Shift"}, {"Jump", "Space"}, {"Special", "U"}, {"Lock on", "Tab"}, {"Weapon", "1 to 5, Q"}, {"Talk, use", "E"}, {"Skip scene", "hold Enter"} };
    static const char *P[] = {"Left stick", "Right stick", "X or RT", "Y", "LT", "B", "A", "RB", "LB or R3", "D-pad left, right", "X", "Hold START"};
    for (int i = 0; i < 12; i++) {
      p_text(c, F_MONO, 15, 0, CV_LEFT, K[i][0], 520, 104 + i * 30, DIMT, 1);
      p_text(c, F_MONO, 15, 0, CV_LEFT, in->used_pad ? P[i] : K[i][1], 700, 104 + i * 30, PAPER, 1);
    }
    layer_end(&L_menu, c);
  }
  hg_tex(&L_menu, 140, 90, 1);
  hg_end();
}

void hud_end_draw(float t, const char *title, const char *const *lines, int n, const int *stats) {
  hg_begin();
  hg_rect(0, 0, DS_W, DS_H, INK, ez(t * .5f) * .88f);
  /* two pages: how the day went, then the credits */
  cv *c = layer_begin(&L_end, 1100, 640, 900);
  if (c) {
    p_text(c, F_DISPLAY, 66, 3, CV_LEFT, title, 0, 70, PAPER, 1);
    p_text(c, F_MONO, 20, 1, CV_LEFT, "HEATSYNC LABS. MIDNIGHT.", 4, 106, ORANGE, 1);
    char row[120];
    snprintf(row, sizeof row, "Trust %d    Safety %d    Vibe %d    Dues %d", stats[ST_TRUST], stats[ST_SAFETY], stats[ST_VIBE], stats[ST_DUES]);
    p_text(c, F_MONO, 19, 0, CV_LEFT, row, 4, 146, PAPER, 1);
    float y = 200;
    cv_font(c, F_TYPE, 22);
    for (int i = 0; i < n && y < 620; i++) {
      char w[4][120]; int m = wrap(c, lines[i], 1040, w, 3);
      for (int k = 0; k < m; k++) { p_text(c, F_TYPE, 22, 0, CV_LEFT, w[k], 4, y, PAPER, 1); y += 30; }
      y += 12;
    }
    layer_end(&L_end, c);
  }
  static layer_t L_cred;
  c = layer_begin(&L_cred, 1100, 560, 901);
  if (c) {
    float y = 60;
    static const char *CR[] = {
      "KEYHOLDER", "",
      "The lab: the HSL-108 model of HeatSync Labs, 108 West Main, Mesa",
      "Downtown Mesa: street and building data from OpenStreetMap contributors (ODbL)",
      "Storefronts after BikeMesa by Luis Montes, cartwheel-examples (MIT)",
      "Venue names and signs belong to their owners, see CREDITS",
      "Characters: VRoid Project sample avatars",
      "Motion: Quaternius Universal Animation Library (CC0)",
      "Voices: Kokoro 82M (Apache 2.0)",
      "Engine: three.c and wasmcart by Luis Montes", "",
      "This is a story. HeatSync Labs does not settle anything by sword.",
      "Everyone is welcome. Open hours are free. Come say hi.", "108 W Main St, Mesa. heatsynclabs.org",
    };
    for (unsigned i = 0; i < sizeof CR / sizeof CR[0]; i++) { p_text(c, F_TYPE, 21, 0, CV_LEFT, CR[i], 4, y, i == 0 ? ORANGE : PAPER, i == 0 ? 1.f : .85f); y += 34; }
    layer_end(&L_cred, c);
  }
  float swap = ds_clamp((t - 14) * 1.2f, 0, 1);
  hg_tex(&L_end, 100, 60, ez(t - 1) * (1 - swap));
  hg_tex(&L_cred, 100, 80, swap);
  hg_end();
}
