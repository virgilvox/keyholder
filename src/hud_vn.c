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
static layer_t L_box, L_name, L_opt[5], L_prompt, L_ch, L_card, L_note, L_obj, L_clock, L_use, L_toast, L_got, L_title, L_menu, L_cards[3], L_retry, L_end, L_mk, L_tasks, L_mgt, L_mgh, L_seq, L_sub, L_trip, L_greet, L_gear[2], L_nights;

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
  c = layer_begin(&L_name, 600, 44, key);   /* the widest tag: an 18 character stranger's name, letter spaced, and its slant */
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
static float chapter_alpha(const story_ui_t *U) { return U->chapter ? ez(U->ch_t * 2.5f) * ds_clamp((U->ch_dur - U->ch_t) * 2.5f, 0, 1) : 0; }
static float note_alpha(const story_ui_t *U) { return U->note && !U->chapter ? ds_clamp(U->note_t * 2, 0, 1) * ds_clamp((4 - U->note_t) * 3, 0, 1) : 0; }
static float got_alpha(const story_ui_t *U) { return U->got_t > 0 && U->got ? ds_clamp(U->got_t * 2, 0, 1) * ds_clamp((3 - U->got_t) * 5, 0, 1) : 0; }
static void draw_chapter(const story_ui_t *U) {
  float t = U->ch_t, a = chapter_alpha(U);
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
  float a = note_alpha(U);
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
    static const float full[4] = {24, 22, 22, 12};   /* about the best a careful day reaches */
    for (int i = 0; i < 4; i++) {
      float x = 34 + i * 54, v = ds_clamp(U->stats[idx[i]] / full[i], 0, 1);
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
    hg_tex(&L_obj, 0, 24, 1 - chapter_alpha(U));   /* a chapter card takes the top left: the old goal steps aside */
  }
  if (chapter_alpha(U) > .01f) return;   /* and the pins and the wayfinder wait for the card to go */
  for (int i = 0; i < U->npin; i++) { hg_rect_m(U->pin_x[i], U->pin_y[i], 10, 10, .785f, 0x9a968c, .8f); hg_rect_m(U->pin_x[i], U->pin_y[i], 4, 4, .785f, INK, .8f); }
  if (U->marker_on) {
    float x = U->marker_x, y = U->marker_y;
    if (U->marker_behind) { y = DS_H - 90; x = x < DS_W / 2 ? DS_W - 70 : 70; }
    x = ds_clamp(x, 60, DS_W - 60); y = ds_clamp(y, 110, DS_H - 90);
    float bob = sinf((float)ds_now() * 4) * 4;
    /* a corner of the route is a smaller, hollow diamond: the way round, not the place */
    float sz = U->marker_corner ? 11 : 15;
    hg_rect_m(x, y - 18 + bob, sz, sz, .785f, U->marker_task ? ORANGE : PAPER, 1);
    hg_rect_m(x, y - 18 + bob, U->marker_corner ? 7 : 7, U->marker_corner ? 7 : 7, .785f, INK, 1);
    /* what it points at and how far: "Soda fridge  12 m" */
    char d[128]; int m = (int)(U->marker_dist + .5f);
    if (U->marker_label && U->marker_label[0]) snprintf(d, sizeof d, "%s  %d m", U->marker_label, m); else snprintf(d, sizeof d, "%d m", m);
    c = layer_begin(&L_mk, 420, 28, hash_str(d, 3));
    if (c) {
      cv_font(c, F_MONO, 15); float w = cv_measure(c, d) + 18;
      cv_fill_hex(c, INK, .55f); cv_fill_rect(c, 210 - w / 2, 2, w, 22);
      p_text(c, F_MONO, 15, 0, CV_CENTER, d, 210, 18, PAPER, 1); layer_end(&L_mk, c);
    }
    hg_tex(&L_mk, ds_clamp(x, 220, DS_W - 220) - 210, y - 2 + bob, .95f);   /* the label stays on screen at the edges */
  }
  if (U->prompt_use) {
    const intent_t *in = input_get();
    char s[140]; snprintf(s, sizeof s, "{USE}  %s", U->prompt_use);
    c = layer_begin(&L_use, 620, 58, hash_str(s, 41 + in->used_pad));
    if (c) {
      float w = g_markup(c, 0, 37, CV_LEFT, F_DISPLAY, 23, 0, INK, 34, in->used_pad, s, false) + 56, x0 = 310 - w / 2;
      cv_fill_hex(c, ORANGE, 1); cv_begin(c); cv_move(c, x0 + 14, 2); cv_line(c, x0 + w, 2); cv_line(c, x0 + w - 14, 54); cv_line(c, x0, 54); cv_close(c); cv_fill(c);
      g_markup(c, 310, 37, CV_CENTER, F_DISPLAY, 23, 0, INK, 34, in->used_pad, s, true);
      layer_end(&L_use, c);
    }
    hg_tex(&L_use, 330, U->mg && U->mg != MG_GREET && U->mg != MG_POINT ? 450 : 556, 1);
  }
}
/* the roam's standing hints, bottom left: the weapon (once one is owned) and the bike (once it is unlocked,
 * outdoors), in the use prompt's style, with this device's glyphs */
static int gear_rows(const story_ui_t *U) {
  if (U->mg && U->mg != MG_GREET && U->mg != MG_POINT) return 0;
  return (U->gear_wp ? 1 : 0) + (U->gear_bike ? 1 : 0);
}
static void draw_gear(const story_ui_t *U) {
  if (U->mg && U->mg != MG_GREET && U->mg != MG_POINT) return;
  bool pad = input_get()->used_pad;
  char s[2][120]; int n = 0;
  if (U->gear_wp) {
    if (U->gear_drawn) snprintf(s[n++], sizeof s[0], "{DRAW} SHEATHE    {LIGHT} SWING");
    else { char up[48]; int k = 0; for (const char *p = U->gear_wp; *p && k < 47; p++) up[k++] = (*p >= 'a' && *p <= 'z') ? (char)(*p - 32) : *p; up[k] = 0;
      snprintf(s[n++], sizeof s[0], "{DRAW} DRAW %s", up); }
  }
  if (U->gear_bike) snprintf(s[n++], sizeof s[0], U->gear_riding ? "{BIKE} GET OFF THE BIKE" : "{BIKE} RIDE THE BIKE");
  for (int i = 0; i < n; i++) {
    cv *c = layer_begin(&L_gear[i], 560, 40, hash_str(s[i], 61 + pad));
    if (c) {
      float w = g_markup(c, 0, 26, CV_LEFT, F_DISPLAY, 16, 1, PAPER, 26, pad, s[i], false) + 40;
      cv_fill_hex(c, INK, .72f); cv_begin(c); cv_move(c, 0, 2); cv_line(c, w + 12, 2); cv_line(c, w, 38); cv_line(c, 0, 38); cv_close(c); cv_fill(c);
      cv_fill_hex(c, ORANGE, 1); cv_fill_rect(c, 0, 2, 5, 36);
      g_markup(c, 18, 26, CV_LEFT, F_DISPLAY, 16, 1, PAPER, 26, pad, s[i], true);
      layer_end(&L_gear[i], c);
    }
    hg_tex(&L_gear[i], 0, DS_H - 30 - (n - i) * 46, .95f);
  }
}
/* the roam's task list: required bright, optional grey, done ticked, the marker's one arrowed */
static void draw_tasks(const story_ui_t *U) {
  if (!U->ntask) return;
  uint32_t key = 77;
  for (int i = 0; i < U->ntask; i++) key = hash_str(U->task[i].label, key * 31 + U->task[i].state * 7 + U->task[i].target * 3 + U->task[i].optional);
  /* v1.3: a long label wraps to a second line rather than losing its end */
  const float RH = 34, LH = 22, LW = 390; int n = U->ntask;
  char lines[UI_TASKS][3][120]; int nl[UI_TASKS]; float hgt = 0;
  cv *c = layer_begin(&L_tasks, 560, (int)(n * (RH + LH) + 40), key);
  if (c) {
    cv_font(c, F_TYPE, 19);
    for (int i = 0; i < n; i++) { nl[i] = wrap(c, U->task[i].label ? U->task[i].label : "", LW, lines[i], 2); if (nl[i] < 1) { nl[i] = 1; lines[i][0][0] = 0; } hgt += RH + (nl[i] - 1) * LH; }
    cv_fill_hex(c, INK, .66f); cv_begin(c); cv_move(c, 0, 0); cv_line(c, 540, 0); cv_line(c, 526, hgt + 34); cv_line(c, 0, hgt + 34); cv_close(c); cv_fill(c);
    p_text(c, F_MONO, 13, 2, CV_LEFT, "HOSTING", 22, 22, ORANGE, 1);
    float y = 30;
    for (int i = 0; i < n; i++) {
      const ui_task *t = &U->task[i];
      float rh = RH + (nl[i] - 1) * LH, cy = y + RH / 2;
      bool done = t->state == UT_DONE;
      uint32_t col = done ? DIMT : t->optional ? 0xb8b2a4 : PAPER;
      cv_stroke_hex(c, done ? ORANGE : t->optional ? 0x8a867c : PAPER, 1); cv_line_width(c, 2);
      cv_begin(c); cv_move(c, 22, cy - 8); cv_line(c, 38, cy - 8); cv_line(c, 38, cy + 8); cv_line(c, 22, cy + 8); cv_close(c); cv_stroke(c);
      if (done) { cv_stroke_hex(c, ORANGE, 1); cv_line_width(c, 3); cv_begin(c); cv_move(c, 24, cy); cv_line(c, 29, cy + 5); cv_line(c, 41, cy - 10); cv_stroke(c); }
      if (t->state == UT_ACTIVE) { cv_fill_hex(c, ORANGE, 1); cv_fill_rect(c, 26, cy - 4, 8, 8); }
      float lw = 0;
      for (int l = 0; l < nl[i]; l++) {
        p_text(c, F_TYPE, 19, 0, CV_LEFT, lines[i][l], 50, cy + 7 + l * LH, col, 1);
        cv_font(c, F_TYPE, 19); lw = cv_measure(c, lines[i][l]);
        if (done) { cv_fill_hex(c, DIMT, .9f); cv_fill_rect(c, 50, cy + l * LH, lw, 2); }
      }
      if (t->optional && !done) p_text(c, F_MONO, 11, 1, CV_LEFT, "OPTIONAL", 58 + lw, cy + 5 + (nl[i] - 1) * LH, 0x8a867c, 1);
      if (t->target && !done) { cv_fill_hex(c, ORANGE, 1); cv_begin(c); cv_move(c, 6, cy - 7); cv_line(c, 15, cy); cv_line(c, 6, cy + 7); cv_close(c); cv_fill(c); }
      y += rh;
    }
    layer_end(&L_tasks, c);
  }
  /* the announcements (a chapter card, a note, the weapon banner) take the top of the screen: the list steps aside */
  float under = fmaxf(chapter_alpha(U), fmaxf(note_alpha(U), got_alpha(U)));
  if (under < .99f) hg_tex(&L_tasks, 0, U->objective ? 74 : 24, 1 - under);
}
/* the hosting minigames: a panel low in the middle; the greet window and the pointing reticle in the scene */
static void draw_minigame(const story_ui_t *U) {
  const intent_t *in = input_get();
  bool pad = in->used_pad;
  if (U->mg == MG_GREET) {
    if (!U->mg_anchor) return;
    float x = ds_clamp(U->mg_ax, 80, DS_W - 80), y = ds_clamp(U->mg_ay, 60, DS_H - 120), w = U->mg_window;
    uint32_t col = w > .5f ? ORANGE : w > 0 ? 0xffc040 : 0xe8433a;
    cv *c = layer_begin(&L_greet, 220, 34, hash_str(w > 0 ? "LOOK UP" : "MISSED IT", 6));
    if (c) {
      const char *t = w > 0 ? "LOOK UP" : "THEY ARE WAITING";
      cv_font(c, F_DISPLAY, 17); cv_letter_spacing(c, 2); float tw = cv_measure(c, t) + 24;
      cv_fill_hex(c, INK, .8f); cv_fill_rect(c, 110 - tw / 2, 2, tw, 30);
      p_text(c, F_DISPLAY, 17, 2, CV_CENTER, t, 110, 24, PAPER, 1); layer_end(&L_greet, c);
    }
    hg_rect(x - 62, y - 4, 124, 13, INK, .8f);
    hg_rect(x - 59, y - 1, 118 * w, 7, col, 1);
    hg_tex(&L_greet, x - 110, y - 40, .95f);
    return;
  }
  if (U->mg == MG_POINT) {
    if (U->mg_anchor) { float r = 26 + sinf((float)ds_now() * 5) * 3; hg_circle(U->mg_ax, U->mg_ay, r, U->mg_aim ? ORANGE : PAPER, .22f); hg_circle(U->mg_ax, U->mg_ay, 6, U->mg_aim ? ORANGE : PAPER, .9f); }
    float cx = DS_W / 2, cy = DS_H * .45f, k = U->mg_aim ? 36 : 46; uint32_t col = U->mg_aim ? ORANGE : PAPER;
    for (int i = 0; i < 4; i++) {
      float sx = i & 1 ? 1 : -1, sy = i & 2 ? 1 : -1;
      hg_rect(cx + sx * k - (sx > 0 ? 14 : 0), cy + sy * k - (sy > 0 ? 3 : 0), 14, 3, col, .9f);
      hg_rect(cx + sx * k - (sx > 0 ? 3 : 0), cy + sy * k - (sy > 0 ? 14 : 0), 3, 14, col, .9f);
    }
    cv *c = layer_begin(&L_mgh, 620, 40, hash_str(U->mg_title, 300 + pad + U->mg_aim * 2));
    if (c) {
      char s[160]; snprintf(s, sizeof s, U->mg_aim ? "%s: {USE} point it out" : "%s: aim with {LOOK}", U->mg_title ? U->mg_title : "");
      g_markup(c, 310, 27, CV_CENTER, F_TYPE, 19, 0, PAPER, 28, pad, s, true); layer_end(&L_mgh, c);
    }
    hg_rect(330, 610, 620, 40, INK, .55f); hg_tex(&L_mgh, 330, 610, 1);
    return;
  }
  if (U->mg == MG_CARRY) {
    char s[64]; snprintf(s, sizeof s, "%s  %d / %d", U->mg_carrying ? "CARRYING" : "TRIPS", U->mg_trip + (U->mg_carrying ? 1 : 0), U->mg_trips);
    cv *c = layer_begin(&L_trip, 300, 40, hash_str(s, 9));
    if (c) { cv_fill_hex(c, INK, .7f); cv_fill_rect(c, 0, 0, 300, 40); cv_fill_hex(c, ORANGE, 1); cv_fill_rect(c, 0, 0, 6, 40); p_text(c, F_DISPLAY, 18, 2, CV_LEFT, s, 20, 28, PAPER, 1); layer_end(&L_trip, c); }
    hg_tex(&L_trip, 490, 616, 1);
    return;
  }
  /* the locked ones: a panel */
  const float W = 600, H = 150, X = (DS_W - W) / 2 + sinf(U->mg_t * 70) * 9 * U->mg_shake, Y = 470;
  float a = ds_clamp(U->mg_t * 5, 0, 1);
  hg_para(X, Y, W, H, .1f, INK, .84f * a);
  const char *help = U->mg == MG_TIMING ? "{USE} when the needle is in the orange" : U->mg == MG_MASH ? "Tap {USE} fast" : U->mg == MG_HOLD ? "Hold {USE}, do not let go" :
    U->mg == MG_STICK ? "Turn {MOVE} round and round" : "Press them in order";
  cv *c = layer_begin(&L_mgt, (int)W, 76, hash_str(U->mg_title, hash_str(help, 500 + pad)));
  if (c) {
    p_text(c, F_DISPLAY, 19, 2, CV_LEFT, U->mg_title ? U->mg_title : "", 34, 30, ORANGE, 1);
    g_markup(c, 34, 62, CV_LEFT, F_TYPE, 19, 0, PAPER, 28, pad, help, true);
    g_markup(c, W - 30, 30, CV_RIGHT, F_MONO, 13, 0, DIMT, 22, pad, "{BACK} leave", true);
    layer_end(&L_mgt, c);
  }
  hg_tex(&L_mgt, X, Y + 4, a);
  float bx = X + 40, by = Y + 96, bw = W - 90, bh = 26;
  switch (U->mg) {
    case MG_TIMING: {
      hg_rect(bx, by, bw, bh, 0x2a2a34, a);
      hg_rect(bx + bw * U->mg_z0, by, bw * (U->mg_z1 - U->mg_z0), bh, ORANGE, a * (.75f + .25f * U->mg_flash));
      hg_rect(bx + bw * U->mg_needle - 2, by - 8, 5, bh + 16, U->mg_shake > .1f ? 0xe8433a : PAPER, a);
      for (int i = 0; i < U->mg_need; i++) hg_rect_m(bx + bw + 22, by - 4 + i * 14 + 6, 9, 9, .785f, i < U->mg_hits ? ORANGE : 0x4a4a54, a);
      break;
    }
    case MG_MASH: case MG_HOLD: {
      hg_rect(bx, by, bw, bh, 0x2a2a34, a);
      hg_rect(bx, by, bw * ds_clamp(U->mg_v, 0, 1), bh, U->mg_shake > .1f ? 0xffc040 : ORANGE, a);
      if (U->mg_flash > 0) hg_rect(bx, by, bw * ds_clamp(U->mg_v, 0, 1), bh, 0xffffff, U->mg_flash * .35f);
      break;
    }
    case MG_STICK: {
      float cx = bx + bw - 30, cy = by - 22, r = 34;
      hg_circle(cx, cy, r + 4, 0x2a2a34, a); hg_circle(cx, cy, r - 6, 0x3a3a44, a);
      float ang = U->mg_angle;
      hg_circle(cx + sinf(ang) * (r - 6), cy - cosf(ang) * (r - 6), 7, ORANGE, a);
      hg_rect(bx, by, bw - 90, bh, 0x2a2a34, a);
      hg_rect(bx, by, (bw - 90) * U->mg_v, bh, ORANGE, a);
      break;
    }
    case MG_SIGNIN: {
      uint32_t k = hash_str("seq", 600 + pad + U->mg_step * 7 + U->mg_nseq);
      for (int i = 0; i < U->mg_nseq; i++) k = hash_str(U->mg_seq[i], k);
      c = layer_begin(&L_seq, (int)bw, 52, k);
      if (c) {
        float x = 0;
        for (int i = 0; i < U->mg_nseq; i++) {
          bool done = i < U->mg_step, cur = i == U->mg_step;
          float gw = g_glyphs(c, 0, 26, 38, g_act(U->mg_seq[i], pad), false);
          if (cur) { cv_fill_hex(c, ORANGE, 1); cv_fill_rect(c, x - 6, 0, gw + 12, 52); }
          g_glyphs(c, x, 26, 38, g_act(U->mg_seq[i], pad), true);
          if (done) { cv_fill_hex(c, INK, .6f); cv_fill_rect(c, x - 6, 0, gw + 12, 52); cv_stroke_hex(c, ORANGE, 1); cv_line_width(c, 4); cv_begin(c); cv_move(c, x + gw / 2 - 10, 26); cv_line(c, x + gw / 2 - 2, 34); cv_line(c, x + gw / 2 + 12, 16); cv_stroke(c); }
          x += gw + 34;
        }
        layer_end(&L_seq, c);
      }
      hg_tex(&L_seq, bx, by - 22, a);
      break;
    }
    default: break;
  }
}
/* banter and barks: a line at the bottom, no box, no letterbox */
static void draw_sub(const story_ui_t *U) {
  float a = ds_clamp(U->sub_t * 3, 0, 1) * ds_clamp((U->sub_dur - U->sub_t) * 6, 0, 1);
  cv *c = layer_begin(&L_sub, 1040, 80, hash_str(U->sub_text, hash_str(U->sub_name, U->sub_col)));
  static int lines;
  if (c) {
    char up[64]; int k = 0; for (const char *p = U->sub_name; p && *p && k < 63; p++) up[k++] = (*p >= 'a' && *p <= 'z') ? (char)(*p - 32) : *p; up[k] = 0;
    cv_font(c, F_TYPE, 22); char w[3][120]; lines = wrap(c, U->sub_text, 820, w, 2);
    p_text(c, F_DISPLAY, 16, 2, CV_RIGHT, up, 170, 30, U->sub_col, 1);
    for (int i = 0; i < lines; i++) p_text(c, F_TYPE, 22, 0, CV_LEFT, w[i], 188, 30 + i * 30, PAPER, 1);
    layer_end(&L_sub, c);
  }
  float y = DS_H - 60 - lines * 30 - U->letterbox * 62 - (U->prompt_use ? 100 : U->roam ? gear_rows(U) * 46 : 0);
  hg_grad(640, y + 22 + lines * 15, 1100, lines * 30 + 26, 0, INK, 0, .0f);
  hg_rect(120, y + 2, 1040, lines * 30 + 18, INK, .55f * a);
  hg_tex(&L_sub, 120, y, a);
}
static void draw_toasts(const story_ui_t *U) {
  if (U->toast_t > 0 && U->toast) {
    char s[40]; snprintf(s, sizeof s, "%s %s%d", U->toast, U->toast_n > 0 ? "+" : "", U->toast_n);
    cv *c = layer_begin(&L_toast, 240, 40, hash_str(s, 3));
    if (c) { p_text(c, F_DISPLAY, 20, 2, CV_RIGHT, s, 232, 28, U->toast_n > 0 ? 0x8fe388 : 0xff8a80, 1); layer_end(&L_toast, c); }
    float a = ds_clamp(U->toast_t * 2, 0, 1) * ds_clamp((2.4f - U->toast_t) * 6, 0, 1);
    hg_rect(DS_W - 250, 112, 250, 40, INK, .6f * a);
    hg_tex(&L_toast, DS_W - 250, 112, a);
    if (U->toast2) {
      static layer_t L_toast2;
      char s2[40]; snprintf(s2, sizeof s2, "%s %s%d", U->toast2, U->toast2_n > 0 ? "+" : "", U->toast2_n);
      c = layer_begin(&L_toast2, 240, 40, hash_str(s2, 4));
      if (c) { p_text(c, F_DISPLAY, 20, 2, CV_RIGHT, s2, 232, 28, U->toast2_n > 0 ? 0x8fe388 : 0xff8a80, 1); layer_end(&L_toast2, c); }
      hg_rect(DS_W - 250, 154, 250, 40, INK, .6f * a);
      hg_tex(&L_toast2, DS_W - 250, 154, a);
    }
  }
  if (got_alpha(U) > 0) {
    char s[60]; snprintf(s, sizeof s, "NEW WEAPON  %s", U->got);
    cv *c = layer_begin(&L_got, 620, 56, hash_str(s, 7));
    if (c) { cv_fill_hex(c, ORANGE, 1); cv_fill_rect(c, 0, 0, 620, 52); p_text(c, F_DISPLAY, 25, 2, CV_CENTER, s, 310, 36, INK, 1); layer_end(&L_got, c); }
    /* under a chapter card, not across it; once low, it stays low until it is gone */
    static float last_t; static bool low;
    if (U->got_t > last_t) low = false;
    if (U->chapter) low = true;
    last_t = U->got_t;
    hg_tex(&L_got, 330, low ? 292 : 100, got_alpha(U));
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
  if (U->roam) { draw_roam(U); draw_tasks(U); draw_gear(U); if (U->mg) draw_minigame(U); }
  if (U->sub_text && U->sub_t > 0 && !U->talking) draw_sub(U);
  if (U->chapter) draw_chapter(U);
  if (note_alpha(U) > 0) draw_note(U);   /* after the chapter card, never over it */
  if (U->talking && U->line) draw_dialogue(U);
  if (U->choosing) draw_choice(U);
  draw_toasts(U);
  if (U->skip_hint || U->skip > 0) {
    hg_rect(DS_W - 160, DS_H - 40, 130, 5, 0x000000, .45f);
    if (U->skip > 0) hg_rect(DS_W - 160, DS_H - 40, 130 * U->skip, 5, ORANGE, 1);
    if (U->skip_hint) {
      static layer_t L_skip;
      cv *c = layer_begin(&L_skip, 320, 26, 0x5C1);
      if (c) { g_markup(c, 318, 18, CV_RIGHT, F_MONO, 15, 1, PAPER, 15, true, "HOLD {PAUSE}  SKIP ORIENTATION", true); layer_end(&L_skip, c); }
      hg_tex(&L_skip, DS_W - 30 - 320, DS_H - 68, 1);
    }
  }
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
    /* row 1 is the Battle Zone's entry (hud_battle_title): the picked keyholder keeps an orange name */
    bool picked = i == who, s = picked && row == 0;
    c = layer_begin(&L_cards[i], 330, 112, hash_str(names[i], (s ? 31 : 7) + (picked ? 100 : 0)));
    if (c) {
      cv_fill_hex(c, s ? ORANGE : INK, s ? 1.f : .8f);
      cv_begin(c); cv_move(c, 14, 0); cv_line(c, 330, 0); cv_line(c, 316, 112); cv_line(c, 0, 112); cv_close(c); cv_fill(c);
      p_text(c, F_DISPLAY, 32, 2, CV_LEFT, names[i], 26, 44, s ? INK : picked ? ORANGE : PAPER, 1);
      cv_font(c, F_TYPE, 15); char lines[3][120]; int n = wrap(c, blurbs[i], 280, lines, 2);
      for (int k = 0; k < n; k++) p_text(c, F_TYPE, 15, 0, CV_LEFT, lines[k], 24, 70 + k * 20, s ? INK : DIMT, 1);
      layer_end(&L_cards[i], c);
    }
    hg_tex(&L_cards[i], 90 + i * 370, 500 + (s ? -6 : 0), 1);
  }
  const intent_t *in = input_get();
  char m[120];
  if (row == 2) snprintf(m, sizeof m, "%s  pick a night     %s  play it     %s  back", in->used_pad ? "LEFT RIGHT" : "ARROWS", in->used_pad ? "A" : "ENTER", in->used_pad ? "UP" : "UP ARROW");
  else if (row == 1) snprintf(m, sizeof m, "%s  enter the Battle Zone     %s  back to the day", in->used_pad ? "A" : "ENTER", in->used_pad ? "UP" : "UP ARROW");
  else if (has_save) snprintf(m, sizeof m, "%s  continue     %s  start a new day", in->used_pad ? "A" : "ENTER", in->used_pad ? "X" : "N");
  else snprintf(m, sizeof m, "%s  start the day", in->used_pad ? "A" : "ENTER");
  c = layer_begin(&L_menu, 800, 40, hash_str(m, 1));
  if (c) { p_text(c, F_DISPLAY, 19, 1, CV_CENTER, m, 400, 28, PAPER, 1); layer_end(&L_menu, c); }
  hg_tex(&L_menu, 240, 642, .6f + .4f * sinf(t * 3));
  hg_end();
}

/* v1.3: the night select, once a night has been reached: which night, and its name */
void hud_nights_title(float t, bool sel, int night, int open_mask, int count, const char *title) {
  if (!open_mask) return;
  hg_begin();
  char line[96], num[32];
  snprintf(num, sizeof num, sel ? "<  NIGHT %d OF %d  >" : "NIGHT %d OF %d", night, count);
  snprintf(line, sizeof line, "%s", title ? title : "");
  cv *c = layer_begin(&L_nights, 400, 92, hash_str(line, hash_str(num, (sel ? 10 : 0) + (unsigned)open_mask)));
  if (c) {
    cv_fill_hex(c, sel ? ORANGE : INK, sel ? 1.f : .82f);
    cv_begin(c); cv_move(c, 18, 0); cv_line(c, 392, 0); cv_line(c, 374, 62); cv_line(c, 0, 62); cv_close(c); cv_fill(c);
    p_text(c, F_DISPLAY, 30, 2, CV_LEFT, "NIGHTS", 28, 47, sel ? INK : PAPER, 1);
    p_text(c, F_MONO, 12, 2, CV_RIGHT, num, 356, 17, sel ? INK : ORANGE, 1);
    p_text(c, F_TYPE, 14, 0, CV_RIGHT, line, 352, 50, sel ? INK : PAPER, 1);   /* the night's name, inside the panel */
    layer_end(&L_nights, c);
  }
  float a = ez(t * 1.5f - .5f);
  hg_tex(&L_nights, 846 + (sel ? -14 : 0) + (1 - a) * 60, 300, a);
  hg_end();
}

void hud_pause_draw(int sel, bool music, int save) {
  hg_begin();
  hg_rect(0, 0, DS_W, DS_H, INK, .72f);
  const intent_t *in = input_get();
  cv *c = layer_begin(&L_menu, 1000, 560, 500u + (unsigned)sel * 7 + (music ? 1 : 0) + (unsigned)save * 2 + (in->used_pad ? 50 : 0));
  if (c) {
    p_text(c, F_DISPLAY, 50, 2, CV_LEFT, "PAUSED", 0, 54, PAPER, 1);
    const char *sv = save == 2 ? "Saved. Continue starts here" : save == 1 ? "Save (while walking free, between scenes)" : "Save game";
    const char *rows[4] = {"Back to the day", sv, music ? "Music on" : "Music off", "Leave (your last save stays)"};
    for (int i = 0; i < 4; i++) {
      bool s = i == sel;
      cv_fill_hex(c, s ? ORANGE : 0x24242e, 1); cv_fill_rect(c, 0, 84 + i * 56, 470, 48);
      p_text(c, F_TYPE, 19, 0, CV_LEFT, rows[i], 18, 115 + i * 56, s ? INK : (i == 1 && save == 1) ? DIMT : PAPER, 1);
    }
    static const char *K[][2] = { {"Move", "WASD"}, {"Camera", "Mouse, arrows"}, {"Light attack", "Q (or left click)"}, {"Heavy attack", "F"},
      {"Guard, parry", "C, tap to parry (or right click)"}, {"Dodge, sprint", "Shift tap; hold Shift to sprint"}, {"Jump", "Space"}, {"Special", "X"},
      {"Lock on, switch", "Tab, middle click; wheel switches"}, {"Zoom", "Mouse wheel"}, {"Weapon", "1 to 5"}, {"Call a friend", "R"},
      {"Talk, use", "E"}, {"Draw, sheathe", "G"}, {"Bike (outdoors)", "B"}, {"Skip scene", "Hold Enter"} };
    static const char *P[] = {"Left stick", "Right stick", "X or RT", "Y", "LT (tap to parry)", "B; sprint L3 or full stick", "A", "RB", "LB or R3; flick right stick",
      "LB in roams", "D-pad left, right", "D-pad up", "X", "D-pad up", "D-pad down", "Hold START"};
    enum { NK = sizeof K / sizeof K[0] };
    _Static_assert(sizeof P / sizeof P[0] == NK, "a pad row for every keyboard row");
    for (int i = 0; i < NK; i++) {
      p_text(c, F_MONO, 15, 0, CV_LEFT, K[i][0], 510, 100 + i * 28, DIMT, 1);
      p_text(c, F_MONO, 15, 0, CV_LEFT, in->used_pad ? P[i] : K[i][1], 680, 100 + i * 28, PAPER, 1);
    }
    layer_end(&L_menu, c);
  }
  hg_tex(&L_menu, 140, 90, 1);
  hg_end();
}

void hud_end_draw(float t, const char *title, const char *const *lines, int n, const int *stats) {
  hg_begin();
  hg_rect(0, 0, DS_W, DS_H, INK, ez(t * .5f) * .88f);
  /* two pages: how the day (or the night) went, then the credits. v1.3: the page is keyed by what it says, since
   * each night has its own; the credits roll only after the last night; before that it says what comes next */
  bool more = story_more();
  uint32_t key = hash_str(title, 900u + (unsigned)n * 7 + (more ? 1 : 0));
  for (int i = 0; i < n; i++) key = hash_str(lines[i], key);
  cv *c = layer_begin(&L_end, 1100, 640, key);
  if (c) {
    p_text(c, F_DISPLAY, 66, 3, CV_LEFT, title, 0, 70, PAPER, 1);
    float hr = STORY_UI.hour;
    const char *when = hr >= 23.9f ? "HEATSYNC LABS. MIDNIGHT." : hr >= 21.5f ? "HEATSYNC LABS. TEN O'CLOCK." : "HEATSYNC LABS. CLOSING TIME.";
    p_text(c, F_MONO, 20, 1, CV_LEFT, when, 4, 106, ORANGE, 1);
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
  float swap = more ? 0 : ds_clamp((t - 14) * 1.2f, 0, 1);
  hg_tex(&L_end, 100, 60, ez(t - 1) * (1 - swap));
  hg_tex(&L_cred, 100, 80, swap);
  if (more && t > 6) {
    static layer_t L_next;
    const intent_t *in = input_get();
    char m[96]; snprintf(m, sizeof m, "%s  on to the next night", in->used_pad ? "A" : "ENTER");
    cv *k = layer_begin(&L_next, 600, 40, hash_str(m, 77));
    if (k) { p_text(k, F_DISPLAY, 19, 1, CV_CENTER, m, 300, 28, PAPER, 1); layer_end(&L_next, k); }
    hg_tex(&L_next, 340, 650, .6f + .4f * sinf(t * 3));
  }
  hg_end();
}
