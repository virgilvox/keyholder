/* attic_hud.c: the attic's own HUD between scenes (see attic_int.h): the place tag up top, the objective or
 * the prompt in reach at the bottom (button glyphs follow the device in hand, through g_markup), the memories
 * found, a toast when a memory ends, and the black of a board giving way. Scenes draw the story's HUD instead. */
#include "attic_int.h"
#include "hud_int.h"
#include <stdio.h>
#include <string.h>

#define AT_INK 0x0c0f14u
#define AT_PAPER 0xf4eee2u
#define AT_RED 0xe23b2eu
#define AT_YEL 0xf2c21eu
#define AT_PINK 0xff5aa0u
static layer_t L_tag, L_line, L_count, L_toast;

static uint32_t hash_s(const char *s) { uint32_t h = 2166136261u; while (s && *s) h = (h ^ (uint8_t)*s++) * 16777619u; return h; }

void attic_hud_draw(const attic_hud_t *h) {
  bool pad = input_get()->used_pad;
  hg_begin();
  /* the place tag */
  cv *c = layer_begin(&L_tag, 420, 92, 0xA70u ^ hash_s(h->title) ^ hash_s(h->sub));
  if (c) {
    p_hazard(c, 0, 0, 420, 10, 26, h->deep ? AT_RED : AT_YEL, AT_INK);
    p_text(c, F_DISPLAY, 34, 1, CV_LEFT, h->title, 14, 52, AT_PAPER, 1);
    if (h->sub) p_text(c, F_MONO, 15, 1, CV_LEFT, h->sub, 15, 78, h->deep ? AT_RED : AT_YEL, 1);
    layer_end(&L_tag, c);
  }
  hg_tex(&L_tag, 44, 36, 1);
  /* the line at the bottom: the prompt in reach, else the objective */
  const char *s = h->prompt ? h->prompt : h->objective;
  if (s) {
    cv *b = layer_begin(&L_line, 900, 40, 0xB10u ^ hash_s(s) ^ (pad ? 0x40000u : 0u));
    if (b) { p_plate(b, 0, 0, 900, 40, 10, AT_INK, .82f); g_markup(b, 16, 26, CV_LEFT, F_TYPE, 20, .1f, AT_PAPER, 20, pad, s, true); layer_end(&L_line, b); }
    hg_tex(&L_line, 44, 648, 1);
  }
  /* the memories found, or the falls taken */
  char n[40];
  n[0] = 0;
  if (h->deep) { if (h->falls) snprintf(n, sizeof n, "FALLS %d", h->falls); } else snprintf(n, sizeof n, "MEMORIES %d / %d", h->found, HUB_MEMORIES);
  if (n[0]) {
    cv *k = layer_begin(&L_count, 260, 30, 0xC20u ^ hash_s(n));
    if (k) { p_text(k, F_MONO, 16, 2, CV_RIGHT, n, 254, 22, h->deep ? AT_RED : AT_PINK, 1); layer_end(&L_count, k); }
    hg_tex(&L_count, 1280 - 44 - 260, 44, 1);
  }
  /* a memory just ended: its name, briefly */
  if (h->toast && h->toast_t > 0) {
    float a = h->toast_t > 2.6f ? (3.2f - h->toast_t) / .6f : h->toast_t < .6f ? h->toast_t / .6f : 1;
    cv *t = layer_begin(&L_toast, 760, 92, 0xD30u ^ hash_s(h->toast));
    if (t) { p_text(t, F_MONO, 14, 4, CV_CENTER, h->toast_sub ? h->toast_sub : "", 380, 26, AT_YEL, 1); p_text(t, F_DISPLAY, 36, 1, CV_CENTER, h->toast, 380, 72, 0xf0e2c4, 1); layer_end(&L_toast, t); }
    hg_tex_m(&L_toast, (float[6]){ 1, 0, 0, 1, 640 - 380, 120 }, a, 0xffffff);
  }
  if (h->fade > 0) hg_rect(0, 0, 1280, 720, 0x000000, h->fade);
  hg_end();
}
