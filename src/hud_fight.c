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

/* the intro: speed lines through the whip, the foe's name on the hero shot, then a smash cut to a stark card */
static layer_t L_hero, L_obj, L_num, L_warn, L_bark, L_ally, L_fin, L_tut, L_mark[3], L_rank, L_check;
static void speed_lines(float k, uint32_t c, bool horiz) {
  if (k <= 0) return;
  int seed = (int)(FG.clock * 30);
  for (int i = 0; i < 38; i++) {
    uint32_t h = (uint32_t)(seed * 131 + i * 977) * 2654435761u;
    float y = (h & 1023) / 1023.f * DS_H, x = ((h >> 10) & 1023) / 1023.f * DS_W, len = 200 + ((h >> 20) & 511);
    float w = 1.5f + ((h >> 28) & 3);
    if (horiz) hg_rect_m(x, y, len, w, 0, i % 5 == 0 ? c : 0xffffff, .55f * k);
    else { float a = (h & 1023) / 1023.f * 6.283f, off = 320 + ((h >> 10) & 255); hg_rect_m(640 + cosf(a) * (off + len * .5f), 360 + sinf(a) * (off + len * .5f), len, w, a, i % 4 == 0 ? c : 0xffffff, .5f * k); }
  }
}
static void draw_card(void) {
  float t = FG.phase_t;
  if (FG.quick) {
    /* a retry from the checkpoint: a strip, no ceremony */
    if (t < .8f) {
      text_layer(&L_title, F_DISPLAY, 40, 2, "FROM THE CHECKPOINT", C_CREAM, 0, 0x76);
      float w = text_w(F_DISPLAY, 40, 2, "FROM THE CHECKPOINT"), a = ds_clamp((.8f - t) * 4, 0, 1);
      hg_rect(0, 300, DS_W, 70, INK, .9f * a); hg_rect(0, 368, DS_W, 4, C_HIVIS, a);
      tex_skew(&L_title, 640 - w / 2 - 8, 300, .16f, 1, a, 0xffffff);
    }
    return;
  }
  if (t < INTRO_HERO) {
    float k = sinf(DS_PI * t / INTRO_HERO);
    speed_lines(k, C_HIVIS, true);
    hg_vignette(INK, .5f * k, .5f);
  } else if (t < INTRO_CARD) {
    /* the hero shot: the name low, letterboxed */
    const fighter_t *H = NULL;
    for (int i = 0; i < FG.nfoe; i++) if (!H || FG.foe[i].d.ai == AI_BOSS) H = &FG.foe[i];
    hg_rect(0, 0, DS_W, 64, 0, 1); hg_rect(0, DS_H - 64, DS_W, 64, 0, 1);
    if (H && H->d.name) {
      float k = hud_ease_out((t - INTRO_HERO) / .18f);
      text_layer(&L_hero, F_DISPLAY, 46, 2, H->d.name, C_CREAM, INK, 0x77);
      float w = text_w(F_DISPLAY, 46, 2, H->d.name);
      float x = 80 + (1 - k) * -400;
      hg_para(x - 30, 548, w + 70, 62, SKEW * .55f, INK, .9f);
      hg_para(x - 30, 548, 8, 62, SKEW * .55f, H->d.ai == AI_BOSS ? C_ALERT : C_HIVIS, 1);
      tex_skew(&L_hero, x, 546, .16f, 1, 1, 0xffffff);
    }
  } else if (t < INTRO_BACK) {
    /* the smash card: a hard cut to ink, the title huge, a hard cut out */
    float k = t - INTRO_CARD;
    hg_rect(0, 0, DS_W, DS_H, k < .05f ? 0xffffff : INK, 1);
    hg_stripes(0, 0, DS_W, DS_H, 0xffffff, .025f, 0, 0, 40, 0);
    hg_rect(0, 228, DS_W, 10, C_HIVIS, 1); hg_rect(0, 492, DS_W, 10, C_HIVIS, 1);
    const char *title = FG.def.title ? FG.def.title : "FIGHT";
    const char *sub = FG.def.sub ? FG.def.sub : "";
    float px = 96;
    float tw = text_w(F_DISPLAY, px, 1, title);
    if (tw > 1180) { px *= 1180 / tw; tw = text_w(F_DISPLAY, px, 1, title); }
    text_layer(&L_title, F_DISPLAY, px, 1, title, C_CREAM, 0, 0x71);
    text_layer(&L_sub, F_DISPLAY, 26, 3, sub, C_HIVIS, 0, 0x72);
    float sw = text_w(F_DISPLAY, 26, 3, sub);
    float sc = 1 + fmaxf(0, .08f - k) * 2.5f;
    tex_skew(&L_title, 640 - tw * .5f * sc - 8, 360 - px * 1.05f * sc, .16f, sc, 1, 0xffffff);
    tex_skew(&L_sub, 640 - sw * .5f - 8, 392, .16f, 1, 1, 0xffffff);
    const char *lb = fight_twist_label();
    if (lb) {
      text_layer(&L_obj, F_DISPLAY, 20, 3, lb, INK, 0, 0x78);
      float lw = text_w(F_DISPLAY, 20, 3, lb);
      hg_para(640 - lw / 2 - 30, 440, lw + 60, 34, SKEW * .55f, C_HIVIS, 1);
      tex_skew(&L_obj, 640 - lw / 2 - 8, 438, .16f, 1, 1, 0xffffff);
    }
  } else {
    float k = 1 - ds_clamp((t - INTRO_BACK) / .25f, 0, 1);
    if (k > 0) hg_rect(0, 0, DS_W, DS_H, 0xffffff, .35f * k * k);
  }
}

/* ---- v1.1 HUD: the objective, markers, barks, assists, the finisher's prompt ---- */
static void obj_bar(float x, float y, float w, float h, float f, uint32_t c, uint32_t back) {
  hg_para(x, y, w, h, SKEW, back, 1);
  hg_para(x, y, w * ds_clamp(f, 0, 1), h, SKEW, c, 1);
  hg_para(x, y, w * ds_clamp(f, 0, 1), h * .3f, SKEW, 0xffffff, .25f);
}
static void world_tag(int slot, v3 p, const char *s, uint32_t col, float t) {
  float x, y;
  if (!fight_project(p, &x, &y)) return;
  x = ds_clamp(x, 60, DS_W - 60); y = ds_clamp(y, 90, DS_H - 60);
  text_layer(&L_mark[slot], F_DISPLAY, 15, 2, s, INK, 0, 0x90 + slot);
  float w = text_w(F_DISPLAY, 15, 2, s), bob = sinf(t * 4 + slot) * 3;
  hg_para(x - w / 2 - 14, y - 36 + bob, w + 26, 24, SKEW * .55f, col, .95f);
  hg_rect_m(x, y - 8 + bob, 12, 12, .785f, col, .95f);
  tex_skew(&L_mark[slot], x - w / 2 - 8, y - 40 + bob, .16f, 1, 1, 0xffffff);
}
static void draw_twist(float t) {
  const twist_t *T = &FG.tw;
  const char *lb = fight_twist_label();
  float cx = 652, y = 20, w = 250;
  if (lb && T->kind != FT_TUTORIAL) {
    text_layer(&L_obj, F_DISPLAY, 15, 2, lb, C_CREAM, 0, 0x79);
    float lw = text_w(F_DISPLAY, 15, 2, lb);
    hg_para(cx - w / 2 - 20, y - 6, w + 40, 64, SKEW * .55f, INK, .88f);
    hg_para(cx - w / 2 - 20, y - 6, 6, 64, SKEW * .55f, C_HIVIS, 1);
    tex_skew(&L_obj, cx - lw / 2 - 8, y - 6, .16f, 1, 1, 0xffffff);
  }
  float by = y + 26, bx = cx - w / 2;
  char num[48]; num[0] = 0;
  bool hot = false;
  switch (T->kind) {
    case FT_TIMER: {
      float k = T->meter / fmaxf(T->meter_max, 1);
      hot = k > .7f && !T->obj_done;
      obj_bar(bx, by, w, 14, k, hot && fmodf(t * 4, 1) < .5f ? 0xc8ff60 : 0x7ac040, 0x22242e);
      if (T->obj_done) snprintf(num, sizeof num, "LASER STOPPED");
      else snprintf(num, sizeof num, "GAS %d%%   STOP %d/%d", (int)(k * 100), (int)(T->obj_hpmax - T->obj_hp), (int)T->obj_hpmax);
      if (!T->obj_done) world_tag(0, v3_add(T->obj, v3_(0, 1.7f, 0)), "STOP", 0xffd040, t);
      break; }
    case FT_RETRIEVE: {
      float k = T->meter / fmaxf(T->meter_max, 1);
      hot = k > .65f;
      obj_bar(bx, by, w, 14, k, hot && fmodf(t * 4, 1) < .5f ? C_ALERT : 0xff8a40, 0x22242e);
      snprintf(num, sizeof num, T->carrier >= 0 ? "THEY HAVE IT" : T->carrier == -1 ? "IT IS LOOSE: STAND ON IT" : "GOT IT");
      if (T->carrier >= 0) world_tag(0, v3_add(FG.foe[T->carrier].pos, v3_(0, 2.2f, 0)), "BIKE", C_HIVIS, t);
      else if (T->carrier == -1) {
        world_tag(0, v3_add(T->obj, v3_(0, 1.2f, 0)), "BIKE", C_HIVIS, t);
        float x, yy;
        if (T->take_t > 0 && fight_project(v3_add(T->obj, v3_(0, 1.f, 0)), &x, &yy)) { hg_rect(x - 40, yy + 4, 80, 8, INK, .9f); hg_rect(x - 40, yy + 4, 80 * T->take_t / 1.5f, 8, C_HIVIS, 1); }
      }
      if (T->train_st == 1 && fmodf(t * 3, 1) < .6f) {
        hg_rect(0, 470, DS_W, 56, INK, .8f); hg_stripes(0, 470, DS_W, 56, 0xffc020, .9f, INK, .9f, 40, t * 60);
        text_layer(&L_warn, F_DISPLAY, 34, 3, "TRAIN", INK, 0, 0x7a);
        float ww = text_w(F_DISPLAY, 34, 3, "TRAIN");
        hg_rect(640 - ww / 2 - 30, 474, ww + 60, 48, 0xffc020, 1);
        tex_skew(&L_warn, 640 - ww / 2 - 8, 470, .16f, 1, 1, 0xffffff);
      }
      break; }
    case FT_SCORE: {
      int left = (int)ceilf(fmaxf(0, T->meter_max - T->meter));
      obj_bar(bx, by, w, 14, T->shown / fmaxf(T->target, 1), 0xfff0a0, 0x22242e);
      snprintf(num, sizeof num, "%07d / %07d    %02d", (int)T->shown, (int)T->target, left);
      hot = left <= 15;
      /* the style rank, big at the right */
      static const char *R[5] = { "D", "C", "B", "A", "S" }; static const uint32_t RC[5] = { 0x9aa0ac, 0x58c8ff, 0x40f0d0, 0xfff0a0, 0xff3a78 };
      int r = T->ranks;
      text_layer(&L_rank, F_DISPLAY, 96, 0, R[r], RC[r], INK, 0x7b + r);
      float pop = 1 + FG.combo_pop * .3f;
      float m[6]; mat_place(m, 1150, 420, -.1f, 40, 70, pop, pop); m[2] += .16f * pop;
      hg_tex_m(&L_rank, m, 1, 0xffffff);
      hg_para(1080, 470, 150 * T->style / 100, 6, SKEW, RC[r], 1);
      break; }
    case FT_HOLD: {
      hot = T->meter > .7f;
      obj_bar(bx, by, w, 14, T->meter, hot && fmodf(t * 4, 1) < .5f ? C_ALERT : 0x80c0ff, 0x22242e);
      snprintf(num, sizeof num, "DOOR OPEN %d%%", (int)(T->meter * 100));
      world_tag(0, v3_add(T->obj, v3_(0, 2.4f, 0)), T->meter > .4f ? "SLAM IT" : "FRIDGE", hot ? C_ALERT : 0x80c0ff, t);
      break; }
    case FT_DEFEND: case FT_PROTECT: {
      float k = T->obj_hp / fmaxf(T->obj_hpmax, 1);
      hot = k < .4f;
      uint32_t c = T->obj_flash > 0 ? 0xffffff : hot && fmodf(t * 4, 1) < .5f ? C_ALERT : 0x60e090;
      obj_bar(bx, by, w, 14, k, c, 0x22242e);
      snprintf(num, sizeof num, "%s %d", T->kind == FT_DEFEND ? fight_obj_name() : "SAM", (int)ceilf(T->obj_hp));
      v3 p = T->kind == FT_PROTECT && FG.def.protect_actor ? actor_pos(FG.def.protect_actor) : T->obj;
      world_tag(0, v3_add(p, v3_(0, T->kind == FT_DEFEND ? 2.3f : 2.f, 0)), T->kind == FT_DEFEND ? fight_obj_name() : "SAM", hot ? C_ALERT : 0x60e090, t);
      break; }
    default: break;
  }
  if (num[0]) {
    text_layer(&L_num, F_MONO, 13, 0, num, hot ? 0xffc0b0 : C_CREAM, 0, 0x7f);
    float nw = text_w(F_MONO, 13, 0, num);
    hg_tex(&L_num, cx - nw / 2 - 8, by + 12, 1);
  }
  /* a warning line */
  if (T->warn_t > 0 && T->warn && T->train_st != 1) {
    float a = ds_clamp(T->warn_t * 3, 0, 1) * (fmodf(t * 3, 1) < .65f ? 1 : .5f);
    text_layer(&L_warn, F_DISPLAY, 26, 3, T->warn, 0xffffff, INK, 0x80);
    float ww = text_w(F_DISPLAY, 26, 3, T->warn);
    hg_para(640 - ww / 2 - 34, 118, ww + 60, 40, SKEW * .55f, C_ALERT, .92f * a);
    tex_skew(&L_warn, 640 - ww / 2 - 8, 114, .16f, 1, a, 0xffffff);
  }
  /* the checkpoint, once reached */
  if (FG.checkpoint && FG.phase == PH_FIGHT) {
    text_layer(&L_check, F_MONO, 13, 1, "CHECKPOINT", C_HIVIS, 0, 0x81);
    hg_tex(&L_check, 1150, 160 + (FG.nfoe - 1) * 66, .8f);
  }
}
/* the tutorial: the lesson in hand */
static void draw_lesson(void) {
  static const struct { const char *label, *act; } L[TUT_FREE] = {
    {"MOVE AROUND", "MOVE"}, {"LIGHT ATTACK: LAND THREE", "LIGHT"}, {"HEAVY ATTACK: LAND TWO", "HEAVY"}, {"DODGE THROUGH A SWING", "DODGE"},
    {"GUARD: HOLD AND TAKE ONE", "GUARD"}, {"PARRY: TAP GUARD AS IT LANDS", "GUARD"}, {"SPECIAL: THE METER IS FULL", "SPECIAL"} };
  const twist_t *T = &FG.tw;
  bool pad = input_get()->used_pad;
  int st = T->step;
  uint32_t key = 0x4c00 | (uint32_t)st << 2 | (pad ? 1 : 0);
  cv *c = layer_begin(&L_tut, 560, 92, key);
  if (c) {
    p_plate(c, 0, 0, 560, 92, 14, INK, .86f);
    cv_fill_hex(c, C_HIVIS, 1); cv_fill_rect(c, 0, 14, 5, 64);
    char h[40];
    if (st < TUT_FREE) snprintf(h, sizeof h, "LESSON %d OF %d", st + 1, TUT_FREE); else snprintf(h, sizeof h, "LESSONS DONE");
    p_text(c, F_MONO, 14, 1, CV_LEFT, h, 20, 26, C_HIVIS, 1);
    if (st < TUT_FREE) {
      char m[64]; snprintf(m, sizeof m, "{%s}  %s", L[st].act, L[st].label);
      g_markup(c, 20, 66, CV_LEFT, F_DISPLAY, 21, 1, C_CREAM, 26, pad, m, true);
    } else p_text(c, F_DISPLAY, 21, 1, CV_LEFT, "SPAR UNTIL ONE OF YOU YIELDS", 20, 66, C_CREAM, 1);
    for (int i = 0; i < TUT_FREE; i++) { cv_fill_hex(c, i < st ? C_HIVIS : 0x3a3e4a, 1); cv_fill_rect(c, 380 + i * 24, 16, 16, 8); }
    layer_end(&L_tut, c);
  }
  hg_tex(&L_tut, 24, DS_H - 92 - 24, FG.phase == PH_INTRO ? 0 : 1);
}
/* the bark: a small plate low in the middle, the name in their colour */
static void draw_bark(void) {
  if (FG.bark_cur < 0 || FG.bark_t <= 0 || FG.bark_cur >= FG.def.nbarks) return;
  const fight_bark *b = &FG.def.barks[FG.bark_cur];
  uint32_t col = b->color ? b->color : C_HIVIS;
  uint32_t key = hash_str(b->text ? b->text : "", 0x6200u + (uint32_t)FG.bark_cur);
  cv *c = layer_begin(&L_bark, 900, 64, key);
  if (c) {
    const char *nm = b->name ? b->name : b->speaker ? b->speaker : "";
    cv_font(c, F_DISPLAY, 16); cv_letter_spacing(c, 1.5f);
    float nw = cv_measure(c, nm);
    cv_font(c, F_TYPE, 21); cv_letter_spacing(c, 0);
    float tw = cv_measure(c, b->text ? b->text : "");
    float W = fminf(900, nw + tw + 60), x0 = (900 - W) / 2;
    p_plate(c, x0, 6, W, 52, 10, INK, .82f);
    p_text(c, F_DISPLAY, 16, 1.5f, CV_LEFT, nm, x0 + 18, 38, col, 1);
    p_text(c, F_TYPE, 21, 0, CV_LEFT, b->text ? b->text : "", x0 + 34 + nw, 39, C_CREAM, 1);
    layer_end(&L_bark, c);
  }
  float a = ds_clamp(FG.bark_t * 4, 0, 1) * ds_clamp((FG.bark_len - FG.bark_t) * 6, 0, 1);
  hg_tex(&L_bark, 640 - 450, 600, a);
}
/* who you can call, bottom left */
static void draw_assist(float t) {
  if (FG.def.nassist <= 0) return;
  bool pad = input_get()->used_pad;
  int n = FG.def.nassist, next = FG.ally_next % n;
  const fighter_def *d = &FG.def.assist[next];
  bool ready = !FG.ally_on && FG.ally_cd <= 0;
  const char *nm = FG.ally_on ? FG.def.assist[FG.ally_i].name : d->name;
  uint32_t key = hash_str(nm ? nm : "", 0x4100u | (pad ? 1 : 0) | (ready ? 2 : 0) | (FG.ally_on ? 4 : 0));
  cv *c = layer_begin(&L_ally, 380, 44, key);
  if (c) {
    p_plate(c, 0, 0, 380, 44, 10, INK, .84f);
    char m[80]; snprintf(m, sizeof m, FG.ally_on ? "WITH YOU  %s" : ready ? "{=%s}  CALL %s" : "RECHARGING  %s", FG.ally_on ? (nm ? nm : "") : ready ? g_act("ASSIST", pad) : (nm ? nm : ""), ready ? (nm ? nm : "") : "");
    g_markup(c, 14, 30, CV_LEFT, F_DISPLAY, 16, 1, ready || FG.ally_on ? C_CREAM : 0x8a90a0, 22, pad, m, true);
    layer_end(&L_ally, c);
  }
  float y = DS_H - 44 - (FG.tw.kind == FT_TUTORIAL || FG.def.tutorial ? 140 : 24);
  hg_tex(&L_ally, 24, y, ready ? .85f + .15f * sinf(t * 5) : 1);
  float k = FG.ally_on ? 1 - FG.ally_t / 8.f : ready ? 1 : 1 - FG.ally_cd / 22.f;
  hg_rect(24, y + 40, 380 * ds_clamp(k, 0, 1), 4, FG.ally_on ? 0x60e090 : C_HIVIS, 1);
}
/* the finisher: the button, a ring closing on it */
static void draw_fin(float t) {
  if (FG.fin_t < 0) return;
  float k = ds_clamp(FG.fin_t / .3f, 0, 1), left = ds_clamp(1 - (FG.fin_t - .3f) / 1.6f, 0, 1);
  hg_vignette(INK, .7f * k, .4f);
  hg_rect(0, 0, DS_W, 80 * k, 0, 1); hg_rect(0, DS_H - 80 * k, DS_W, 80 * k, 0, 1);
  if (FG.fin_t < .3f) return;
  bool pad = input_get()->used_pad;
  cv *c = layer_begin(&L_fin, 420, 70, 0x5100u | (pad ? 1 : 0));
  if (c) { g_markup(c, 210, 52, CV_CENTER, F_DISPLAY, 36, 2, 0xffffff, 48, pad, "{STRIKE}  FINISH IT", true); layer_end(&L_fin, c); }
  float cx = 640, cy = 470;
  hg_circle(cx, cy - 20, 60 + 140 * left, C_HIVIS, .25f + .2f * sinf(t * 20));
  hg_tex(&L_fin, cx - 210, cy - 52, 1);
}
/* a boss's big move: a "!" over the head */
static void draw_tells(float t) {
  for (int i = 0; i < FG.nfoe; i++) {
    const fighter_t *f = &FG.foe[i];
    float x, y;
    if (f->tell <= 0 || !fight_project(v3_add(f->pos, v3_(0, 2.25f, 0)), &x, &y)) continue;
    float s = 1 + .2f * sinf(t * 30);
    hg_rect_m(x, y - 14, 14 * s, 34 * s, .12f, INK, .9f); hg_rect_m(x, y - 14, 9 * s, 28 * s, .12f, C_ALERT, 1);
    hg_rect_m(x - 2, y + 12, 12 * s, 12 * s, .12f, INK, .9f); hg_rect_m(x - 2, y + 12, 8 * s, 8 * s, .12f, C_ALERT, 1);
  }
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
  if (FG.tw.fail) s = FG.tw.fail;
  float px = 128; if (text_w(F_DISPLAY, px, 2, s) > 1180) px *= 1180 / text_w(F_DISPLAY, px, 2, s);
  text_layer(&L_ko, F_DISPLAY, px, 2, s, won ? C_CREAM : 0xffc0b0, INK, 0x75);
  float w = text_w(F_DISPLAY, px, 2, s);
  float k = hud_ease_out(pt / .14f), sc = ds_lerp(2.1f, 1, k) + fmaxf(0, pt - .14f) * .03f, a = pt < 2.1f ? 1 : ds_clamp(1 - (pt - 2.1f) / .35f, 0, 1);
  float x = 640 - w * .5f * sc - 8 * sc, y = 360 - px * .75f * sc;
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
  if (!FG.def.tutorial || FG.tw.kind == FT_TUTORIAL || FG.phase == PH_WON || FG.phase == PH_LOST || FG.cine_t >= 0) return;
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
  /* the laser's chlorine: a green haze that thickens */
  if (FG.tw.kind == FT_TIMER) { float k = ds_clamp(FG.tw.meter / fmaxf(FG.tw.meter_max, 1), 0, 1); if (k > .02f) { hg_rect(0, 0, DS_W, DS_H, 0x6a9a30, .38f * k * k + .04f * k); hg_vignette(0x3a6a10, .5f * k, .5f); } }
  /* the fridge's cold at the screen's edge as the door opens */
  if (FG.tw.kind == FT_HOLD && FG.tw.meter > .3f) hg_vignette(0x9fd8ff, .4f * (FG.tw.meter - .3f), .55f);
  bool cine = FG.cine_t >= 0;
  bool intro_hide = FG.phase == PH_INTRO && FG.phase_t < (FG.quick ? 0 : INTRO_BACK);
  if (!cine && FG.fin_t < 0 && FG.phase != PH_WON && FG.phase != PH_LOST && !intro_hide) {
    draw_player(t); draw_foes(t); draw_lock(t);
    draw_twist(t);
    draw_tells(t);
    draw_combo();
    draw_prompts();
    if (FG.tw.kind == FT_TUTORIAL) draw_lesson();
    draw_assist(t);
    draw_banner();
  }
  draw_fin(t);
  if (!(FG.phase == PH_INTRO && FG.phase_t >= INTRO_CARD && FG.phase_t < INTRO_BACK)) draw_bark();
  if (FG.phase == PH_INTRO) draw_card();
  (void)L_check;
  draw_special(t);
  draw_end(t);
  if (FG.screen_flash > 0) hg_rect(0, 0, DS_W, DS_H, FG.screen_col, ds_clamp(FG.screen_flash, 0, 1) * .85f);
  hg_end();
}
