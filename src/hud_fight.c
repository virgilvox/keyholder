/* hud_fight.c: fight_draw_hud. Bars and banners are skewed quads that cost no paint; names,
 * numbers and cards are cv layers repainted only when they change. 1280x720, y down. */
#include "fight_int.h"
#include "hud_int.h"
#include "input.h"
#include "weapon.h"
#include <stdio.h>

#define INK 0x0b0c12u
#define SKEW -.42f

static layer_t L_name, L_foe[4], L_combo, L_title, L_sub, L_banner, L_special, L_ko, L_prompts, L_wp, L_ready;
static char last_banner[32];

void fight_hud_init(void) { hg_init(); }
void fight_hud_reset(void) { for (int i = 0; i < 4; i++) L_foe[i].key = 0; L_title.ok = L_sub.ok = false; L_title.key = 0; last_banner[0] = 0; }

/* one layer of a single line of display type, sized to fit; shadow optional */
static void text_layer(layer_t *L, int font, float px, float ls, const char *s, uint32_t col, uint32_t edge, uint32_t salt) {
  uint32_t key = hash_str(s, 2166136261u ^ col ^ salt ^ (uint32_t)(px * 8));
  if (L->ok && L->key == key) return;
  int w = 1200, h = (int)(px * 1.5f) + 8;
  cv *c = layer_begin(L, w, h, key);
  if (!c) return;
  cv_font(c, font, px); cv_letter_spacing(c, ls);
  float base = px * 1.1f + 2;
  if (edge) {
    cv_line_join(c, CV_JOIN_ROUND); cv_line_width(c, px * .14f + 2); cv_stroke_hex(c, edge, 1);
    cv_text_align(c, CV_LEFT); cv_text_baseline(c, CV_ALPHABETIC);
    cv_stroke_text(c, s, 8, base);
  }
  p_text(c, font, px, ls, CV_LEFT, s, 8, base, col, 1);
  layer_end(L, c);
}
static float text_w(int font, float px, float ls, const char *s) {
  /* widths come from the shared scratch through a zero sized layer: no paint, no upload */
  static cv *m; if (!m) m = cv_new(16, 16);
  cv_font(m, font, px); cv_letter_spacing(m, ls);
  return cv_measure(m, s);
}
/* a layer sheared so its letters lean forward, placed by its top left */
static void tex_skew(const layer_t *L, float x, float y, float sk, float sc, float alpha, uint32_t tint) {
  float m[6] = { sc, 0, -sk * sc, sc, x, y };
  hg_tex_m(L, m, alpha, tint);
}

static uint32_t mix_col(uint32_t a, uint32_t b, float t) {
  int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255, br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
  return ((uint32_t)(ar + (br - ar) * t) << 16) | ((uint32_t)(ag + (bg - ag) * t) << 8) | (uint32_t)(ab + (bb - ab) * t);
}

/* ---- the player's plate ---- */
static void draw_player(float t) {
  const fighter_t *P = &FG.P;
  float x = 40, y = 34, w = 430, h = 24;
  /* the back plate */
  hg_para(x - 14, y - 12, w + 62, 74, SKEW * .55f, INK, .9f);
  hg_para(x - 14, y - 12, 7, 74, SKEW * .55f, C_HIVIS, 1);
  /* health */
  float f = ds_clamp(P->hp / P->hpmax, 0, 1), g = ds_clamp(P->ghost / P->hpmax, 0, 1);
  hg_para(x, y, w, h, SKEW, 0x22242e, 1);
  hg_para(x, y, w * g, h, SKEW, 0xfff0d8, .9f);
  uint32_t c = f < .28f ? (fmodf(t * 5, 1) < .5f ? C_ALERT : 0xff8870) : C_HIVIS;
  hg_para(x, y, w * f, h, SKEW, c, 1);
  hg_para(x, y, w * f, h * .32f, SKEW, 0xffffff, .28f);
  /* the meter: five cells */
  float my = y + h + 7, mh = 12, cell = (w * .74f) / 5;
  float m = FG.meter / 100;
  bool ready = FG.meter >= 100;
  for (int i = 0; i < 5; i++) {
    float cx = x + i * cell - 4;
    hg_para(cx, my, cell - 5, mh, SKEW, 0x22242e, 1);
    float k = ds_clamp(m * 5 - i, 0, 1);
    uint32_t mc = ready ? (fmodf(t * 4, 1) < .5f ? 0xfff0a0 : 0xffc840) : 0x58c8ff;
    hg_para(cx, my, (cell - 5) * k, mh, SKEW, mc, 1);
  }
  text_layer(&L_name, F_DISPLAY, 15, 2, "KEYHOLDER", C_CREAM, 0, 0);
  hg_tex(&L_name, x + 330, y + h + 2 - 6, 1);
  /* the weapon's name under the plate, a colour flash when it changes */
  char nm[40]; snprintf(nm, sizeof nm, "%s", weapon_name((weapon_id)P->wp));
  text_layer(&L_wp, F_DISPLAY, 16, 1.5f, nm, weapon_color(P->wp), 0, 1);
  float k = FG.weapon_card_t > 0 ? hud_ease_out(1.6f - FG.weapon_card_t) : 1;
  float nw = text_w(F_DISPLAY, 16, 1.5f, nm);
  hg_para(x - 30 + (1 - k) * -40, y + 63, nw + 40, 24, SKEW * .55f, INK, .9f);
  hg_tex(&L_wp, x - 16 + (1 - k) * -40, y + 60, 1);
  if (ready && FG.phase == PH_FIGHT && FG.cine_t < 0) {
    bool pad = input_get()->used_pad;
    uint32_t key = 0x5a00 | (pad ? 1 : 0);
    cv *ca = layer_begin(&L_ready, 300, 34, key);
    if (ca) {
      g_markup(ca, 2, 24, CV_LEFT, F_DISPLAY, 17, 1.5f, 0xfff0a0, 26, pad, "SPECIAL READY {SPECIAL}", true);
      layer_end(&L_ready, ca);
    }
    hg_tex(&L_ready, x + 240, y + 60, .8f + .2f * sinf(t * 8));
  }
}

/* ---- the foes' banner: slanted, top right ---- */
static void draw_foes(float t) {
  float rx = 1258;
  float y = 28;
  for (int i = 0; i < FG.nfoe; i++) {
    const fighter_t *f = &FG.foe[i];
    bool boss = f->d.ai == AI_BOSS;
    bool lock = FG.lock == i;
    float w = boss ? 520 : FG.nfoe > 2 ? 300 : 360, bh = boss ? 20 : 13;
    float x = rx - w + i * -8;
    bool out = f->st == F_KO || f->st == F_KNEEL;
    float al = out ? .5f : 1;
    float plate = 26 + bh + 8;
    /* the banner: a hot slash behind the name */
    hg_para(x - 22, y - 2, w + 30, plate, SKEW * .55f, INK, .88f * al);
    hg_para(x + w + 2, y - 2, 6, plate, SKEW * .55f, lock ? C_HIVIS : 0x5a5e6c, al);
    /* the name */
    text_layer(&L_foe[i], F_DISPLAY, boss ? 21 : 17, 1.5f, f->d.name ? f->d.name : "", out ? 0x9aa0ac : C_CREAM, 0, 40 + i);
    float tw = text_w(F_DISPLAY, boss ? 21 : 17, 1.5f, f->d.name ? f->d.name : "");
    tex_skew(&L_foe[i], x + w - tw - 22, y - 5 - (boss ? 2 : 0), .18f, 1, al, 0xffffff);
    /* bar: drains to the right edge */
    float by = y + 24;
    float g = ds_clamp(f->ghost / f->hpmax, 0, 1), k = ds_clamp(f->hp / f->hpmax, 0, 1);
    hg_para(x, by, w, bh, SKEW, 0x22242e, al);
    hg_para(x + w * (1 - g), by, w * g, bh, SKEW, 0xfff0d8, .9f * al);
    uint32_t c = boss ? (f->phase >= 2 ? C_ALERT : f->phase == 1 ? 0xff5a30 : 0xd8402a) : 0xe8433a;
    hg_para(x + w * (1 - k), by, w * k, bh, SKEW, c, al);
    hg_para(x + w * (1 - k), by, w * k, bh * .3f, SKEW, 0xffffff, .22f * al);
    if (boss) for (int p = 1; p <= 2; p++) { float px = x + w * (1 - (p == 1 ? .66f : .33f)); hg_para(px, by, 3, bh, SKEW, INK, 1); }
    y += plate + 6;
  }
  (void)t;
}

/* the lock marker: a turning diamond over the head */
static void draw_lock(float t) {
  if (FG.lock < 0 || FG.phase != PH_FIGHT || FG.cine_t >= 0) return;
  const fighter_t *f = &FG.foe[FG.lock];
  float x, y;
  if (!fight_project(v3_add(f->pos, v3_(0, 1.95f, 0)), &x, &y)) return;
  float bob = sinf(t * 5) * 3;
  hg_rect_m(x, y + bob, 15, 15, .785f, INK, .8f);
  hg_rect_m(x, y + bob, 10, 10, .785f, C_HIVIS, 1);
  hg_rect_m(x, y + bob - 14, 2.5f, 8, 0, C_HIVIS, .9f);
}

/* the combo, right of centre */
static void draw_combo(void) {
  if (FG.combo < 2) return;
  char b[16]; snprintf(b, sizeof b, "%d", FG.combo);
  uint32_t c = FG.combo >= 20 ? 0xff3a78 : FG.combo >= 10 ? 0xfff0a0 : C_HIVIS;
  uint32_t key = hash_str(b, c);
  cv *ca = layer_begin(&L_combo, 280, 110, key);
  if (ca) {
    cv_font(ca, F_DISPLAY, 78); cv_letter_spacing(ca, -2); cv_text_align(ca, CV_RIGHT); cv_text_baseline(ca, CV_ALPHABETIC);
    cv_line_join(ca, CV_JOIN_ROUND); cv_line_width(ca, 8); cv_stroke_hex(ca, INK, 1);
    cv_stroke_text(ca, b, 176, 84);
    p_text(ca, F_DISPLAY, 78, -2, CV_RIGHT, b, 176, 84, c, 1);
    cv_line_width(ca, 5); cv_font(ca, F_DISPLAY, 26); cv_letter_spacing(ca, 2); cv_text_align(ca, CV_LEFT); cv_stroke_text(ca, "HITS", 184, 84);
    p_text(ca, F_DISPLAY, 26, 2, CV_LEFT, "HITS", 184, 84, C_CREAM, 1);
    layer_end(&L_combo, ca);
  }
  float s = 1 + FG.combo_pop * FG.combo_pop * .35f, a = ds_clamp(FG.combo_t * 3, 0, 1);
  float m[6]; mat_place(m, 1110, 300, -.07f, 176, 84, s, s);
  m[2] += .14f * s;
  hg_tex_m(&L_combo, m, a, 0xffffff);
  /* its time left */
  hg_para(1030, 314, 170 * ds_clamp(FG.combo_t / 2.2f, 0, 1), 5, SKEW, c, a);
}

/* the opening card: a slash across the screen opens a band, the title drives in, it all shuts */
static void draw_card(void) {
  float t = FG.phase_t, D = 1.6f;
  if (t > D) return;
  /* the slash */
  float cy = 330;
  float open = t < .1f ? 0 : hud_ease_out((t - .1f) / .2f), shut = t > D - .22f ? (t - (D - .22f)) / .22f : 0;
  float hh = 96 * open * (1 - shut * shut);
  hg_rect(0, 0, DS_W, DS_H, INK, .3f * (1 - shut));
  if (t < .2f) {
    float k = hud_ease_out(t / .12f);
    hg_para(-120, cy - 3, (DS_W + 240) * k, 6, 0, 0xffffff, 1);
    hg_para(-120, cy - 1, (DS_W + 240) * k, 2, 0, C_HIVIS, 1);
    hg_rect(0, 0, DS_W, DS_H, 0xffffff, .22f * (1 - t / .2f));
  }
  if (hh > 1) {
    hg_rect(0, cy - hh, DS_W, hh * 2, INK, .94f);
    hg_rect(0, cy - hh - 5, DS_W, 5, C_HIVIS, 1);
    hg_rect(0, cy + hh, DS_W, 5, C_HIVIS, 1);
    hg_stripes(0, cy - hh, DS_W, hh * 2, 0xffffff, .018f, 0, 0, 34, t * 1.4f);
    hg_clip(0, cy - hh, DS_W, hh * 2);
    const char *title = FG.def.title ? FG.def.title : "FIGHT";
    const char *sub = FG.def.sub ? FG.def.sub : "";
    text_layer(&L_title, F_DISPLAY, 70, 1, title, C_CREAM, 0, 0x71);
    text_layer(&L_sub, F_DISPLAY, 24, 3, sub, C_HIVIS, 0, 0x72);
    float tw = text_w(F_DISPLAY, 70, 1, title), sw = text_w(F_DISPLAY, 24, 3, sub);
    float in = hud_ease_out((t - .12f) / .2f), drift = (t - .3f) * 26;
    float tx = (DS_W - tw) * .5f + (1 - in) * 900 - drift;
    float sx = (DS_W - sw) * .5f + (1 - in) * -900 + drift;
    tex_skew(&L_title, tx - 8, cy - 86, .16f, 1, 1, 0xffffff);
    tex_skew(&L_sub, sx - 8, cy + 18, .16f, 1, 1, 0xffffff);
    hg_noclip();
  }
  /* the second slash that shuts it */
  if (shut > 0) hg_para(-100, cy - 2, (DS_W + 200) * (1 - shut), 4, 0, 0xffffff, 1);
}

/* PERFECT DODGE, PARRY, GUARD BREAK... */
static void draw_banner(void) {
  if (FG.banner_t <= 0 || !FG.banner) return;
  text_layer(&L_banner, F_DISPLAY, 44, 2, FG.banner, 0xffffff, INK, 0x73);
  float w = text_w(F_DISPLAY, 44, 2, FG.banner);
  float a = ds_clamp(FG.banner_t * 5, 0, 1);
  float age = fmaxf(0, .9f - FG.banner_t);
  float s = 1 + fmaxf(0, .12f - age) * 2.2f;
  float x = 640 - w * .5f * s - 8, y = 186;
  hg_para(0, y + 12, DS_W, 44, 0, INK, .32f * a);
  hg_para(x - 60, y + 58, w * s + 120, 3, 0, C_HIVIS, a);
  tex_skew(&L_banner, x, y, .16f, s, a, 0xffffff);
}

/* the special: letterbox, speed lines, a name called out */
static void draw_special(float t) {
  float ct = FG.cine_t;
  if (ct < 0) return;
  float bar = 70 * hud_ease_out(ct / .16f) * (ct > 1.75f ? 1 - (ct - 1.75f) / .25f : 1);
  /* speed lines from the edges in, thrown new each sixtieth */
  uint32_t c = weapon_color(FG.P.wp);
  float cx = 640, cy = 340;
  if (FG.cine_target >= 0) { float x, y; if (fight_project(v3_add(FG.foe[FG.cine_target].pos, v3_(0, 1, 0)), &x, &y)) { cx = ds_clamp(x, 200, 1080); cy = ds_clamp(y, 150, 570); } }
  int seed = (int)(FG.clock * 30);
  float vis = ct < .5f ? .5f : ct < 1.5f ? 1.f : ds_clamp((2.f - ct) * 2, 0, 1);
  for (int i = 0; i < 44; i++) {
    uint32_t h = (uint32_t)(seed * 131 + i * 977) * 2654435761u;
    float a = (h & 1023) / 1023.f * 6.283f, off = 300 + ((h >> 10) & 255), len = 180 + ((h >> 18) & 255);
    float w = 1.5f + ((h >> 26) & 3);
    float mx = cx + cosf(a) * (off + len * .5f), my = cy + sinf(a) * (off + len * .5f);
    hg_rect_m(mx, my, len, w, a, i % 4 == 0 ? c : 0xffffff, .5f * vis);
  }
  hg_rect(0, 0, DS_W, bar, 0, 1); hg_rect(0, DS_H - bar, DS_W, bar, 0, 1);
  /* the name: a slanted plate driving in from the right, low */
  float inn = hud_ease_out((ct - .06f) / .2f), outt = ct > 1.72f ? (ct - 1.72f) / .28f : 0;
  if (ct > .06f && outt < 1) {
    const char *name = FG.cine_name ? FG.cine_name : "";
    text_layer(&L_special, F_DISPLAY, 58, 1, name, 0xffffff, INK, 0x74);
    float w = text_w(F_DISPLAY, 58, 1, name);
    float x = 1240 - w - (1 - inn) * -800 + outt * 900 - fmaxf(0, ct - .3f) * 16, y = 520;
    hg_para(x - 56, y + 2, w + 110, 76, SKEW * .55f, INK, .9f * (1 - outt));
    hg_para(x - 56, y + 2, 9, 76, SKEW * .55f, c, 1 - outt);
    hg_para(x - 44, y + 74, w + 90, 5, SKEW * .55f, c, 1 - outt);
    tex_skew(&L_special, x - 8, y - 2, .18f, 1, 1 - outt, 0xffffff);
  }
  (void)t;
}

/* the end cards: K.O. in a white hot flash */
static void draw_end(float t) {
  if (FG.phase != PH_WON && FG.phase != PH_LOST) return;
  bool won = FG.phase == PH_WON;
  float pt = FG.phase_t;
  const char *s = won ? (FG.def.tutorial && FG.P.hp <= 1 ? "THAT WILL DO" : "K.O.") : "TAPPED OUT";
  text_layer(&L_ko, F_DISPLAY, 128, 2, s, won ? C_CREAM : 0xffc0b0, INK, 0x75);
  float w = text_w(F_DISPLAY, 128, 2, s);
  float k = hud_ease_out(pt / .14f), sc = ds_lerp(2.1f, 1, k) + fmaxf(0, pt - .14f) * .03f, a = pt < 2.1f ? 1 : ds_clamp(1 - (pt - 2.1f) / .35f, 0, 1);
  float x = 640 - w * .5f * sc - 8 * sc, y = 360 - 96 * sc;
  float band = 90 * k * a;
  hg_rect(0, 0, DS_W, DS_H, INK, .24f * a);
  hg_rect(0, 360 - band, DS_W, band * 2, won ? INK : 0x300a08, .86f * a);
  hg_rect(0, 360 - band - 5, DS_W, 5, won ? C_HIVIS : C_ALERT, a);
  hg_rect(0, 360 + band, DS_W, 5, won ? C_HIVIS : C_ALERT, a);
  tex_skew(&L_ko, x, y, .16f, sc, a, 0xffffff);
  if (pt < .3f) hg_para(-100, 358, DS_W + 200, 4, 0, 0xffffff, 1 - pt / .3f);
  (void)t;
}

/* the controls helper */
static void draw_prompts(void) {
  if (!FG.def.tutorial || FG.phase == PH_WON || FG.phase == PH_LOST || FG.cine_t >= 0) return;
  bool pad = input_get()->used_pad;
  unsigned many = FG.def.weapons & (FG.def.weapons - 1);
  uint32_t key = 0xc0 | (pad ? 1u : 0) | (many ? 2u : 0);
  static const struct { const char *label, *act; } R[] = { {"MOVE", "MOVE"}, {"LOCK ON", "LOCK"}, {"LIGHT", "LIGHT"}, {"HEAVY", "HEAVY"}, {"GUARD, TAP TO PARRY", "GUARD"}, {"DODGE", "DODGE"}, {"JUMP", "JUMP"}, {"SPECIAL, METER FULL", "SPECIAL"}, {"WEAPON", "WEAPON"} };
  int n = many ? 9 : 8;
  float rh = 29;
  cv *c = layer_begin(&L_prompts, 400, (int)(rh * n + 16), key);
  if (c) {
    p_plate(c, 0, 0, 400, rh * n + 16, 12, INK, .78f);
    cv_fill_hex(c, C_HIVIS, 1); cv_fill_rect(c, 0, 12, 4, rh * n - 8);
    for (int i = 0; i < n; i++) {
      float cy = 8 + rh * i + rh * .5f;
      g_glyphs(c, 14, cy, 21, g_act(R[i].act, pad), true);
      p_text(c, F_MONO, 14, 0, CV_LEFT, R[i].label, 172, cy + 5, C_CREAM, 1);
    }
    layer_end(&L_prompts, c);
  }
  hg_tex(&L_prompts, 24, DS_H - (rh * n + 16) - 22, FG.phase == PH_INTRO ? ds_clamp((FG.phase_t - 1.2f) * 3, 0, 1) : 1);
}

void fight_draw_hud(void) {
  float t = FG.clock;
  hg_begin();
  /* the screen's mood */
  const fighter_t *P = &FG.P;
  if (P->hpmax > 0 && P->hp / P->hpmax < .28f && FG.phase == PH_FIGHT) hg_vignette(C_ALERT, .34f + .1f * sinf(t * 5), .55f);
  if (FG.witch_t > 0) { float k = ds_clamp(FG.witch_t * 3, 0, 1); hg_vignette(0x4ab0ff, .5f * k, .45f); hg_rect(0, 0, DS_W, DS_H, 0x1a3060, .12f * k); }
  bool cine = FG.cine_t >= 0;
  if (!cine && FG.phase != PH_WON && FG.phase != PH_LOST) {
    float slide = FG.phase == PH_INTRO ? 1 - ds_clamp((FG.phase_t - 1.15f) * 3, 0, 1) : 0;
    if (slide < 1) { (void)slide; draw_player(t); draw_foes(t); draw_lock(t); }
    draw_combo();
    draw_prompts();
    draw_banner();
  }
  if (FG.phase == PH_INTRO) draw_card();
  draw_special(t);
  draw_end(t);
  if (FG.screen_flash > 0) hg_rect(0, 0, DS_W, DS_H, FG.screen_col, ds_clamp(FG.screen_flash, 0, 1) * .85f);
  hg_end();
}
