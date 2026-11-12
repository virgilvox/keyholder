/* input.c: see input.h. Every connected pad, the keyboard and the mouse drive the one player;
 * the strongest stick wins and buttons are or'd. */
#include "input.h"
#include "ds.h"
#include <string.h>

#define KEY_LSHIFT 0xE1
#define KEY_RSHIFT 0xE5
#define KEY_LALT 0xE2
static uint8_t kcur[32], kprev[32];
static wc_pad_t pcur[4], pprev[4];
static intent_t I;
static int nav_ox, nav_oy;
static bool last_pad, seen[4], flagged[4], guard_was;
#define MOUSE_GAIN 1.5f   /* a captured mouse turns the camera faster than a full stick can */

#define DOWN(k, c) (((k)[(c) >> 3] >> ((c) & 7)) & 1)
bool input_key_down(int c) { return c > 0 && c < 256 && DOWN(kcur, c); }
static bool kp(int c) { return DOWN(kcur, c) && !DOWN(kprev, c); }
static float stick(int16_t v) {
  float f = v / 32767.f, dz = 0.2f;
  if (fabsf(f) < dz) return 0;
  return ds_sign(f) * fminf(1, (fabsf(f) - dz) / (1 - dz));
}
static bool pad_live(int p) { return pcur[p].connected || (seen[p] && !flagged[p]); }

void input_frame(const wc_pad_t *pads, const uint8_t *keys) {
  memcpy(kprev, kcur, 32); memcpy(kcur, keys, 32);
  memcpy(pprev, pcur, sizeof pcur); memcpy(pcur, pads, sizeof pcur);
  bool any_key = false;
  for (int i = 0; i < 32; i++) if (kcur[i] & ~kprev[i]) any_key = true;
  if (any_key) last_pad = false;
  for (int p = 0; p < 4; p++) {
    if (pcur[p].connected) { flagged[p] = true; seen[p] = true; }
    else if (flagged[p]) flagged[p] = seen[p] = false;
    if (pcur[p].buttons & ~pprev[p].buttons) seen[p] = true;
  }
  intent_t *o = &I;
  memset(o, 0, sizeof *o);
  o->weapon_pick = -1;
  o->mx = (float)DOWN(kcur, WC_KEY_D) - (float)DOWN(kcur, WC_KEY_A);
  o->my = (float)DOWN(kcur, WC_KEY_W) - (float)DOWN(kcur, WC_KEY_S);
  o->ax = (float)DOWN(kcur, WC_KEY_RIGHT) - (float)DOWN(kcur, WC_KEY_LEFT);
  o->ay = (float)DOWN(kcur, WC_KEY_UP) - (float)DOWN(kcur, WC_KEY_DOWN);
  /* PC action layout (v2.2): the fight sits under the left hand around WASD. E light (a fight has nothing to use;
   * in a walk a drawn weapon only swings when no prompt is up, story.c), Q heavy, F special, C guard and tap to
   * parry, Shift dodge, Space jump. Left click is light and right click guard too; left click fires on the press. */
  bool lmb = DOWN(kcur, KEY_MOUSE_L);
  o->light = kp(WC_KEY_E) || kp(WC_KEY_J) || kp(KEY_MOUSE_L);
  o->light_held = DOWN(kcur, WC_KEY_E) || DOWN(kcur, WC_KEY_J) || lmb;
  o->heavy = kp(WC_KEY_Q) || kp(WC_KEY_K); o->heavy_held = DOWN(kcur, WC_KEY_Q) || DOWN(kcur, WC_KEY_K);
  o->guard = DOWN(kcur, WC_KEY_C) || DOWN(kcur, WC_KEY_L) || DOWN(kcur, KEY_MOUSE_R);
  o->jump = kp(WC_KEY_SPACE); o->jump_held = DOWN(kcur, WC_KEY_SPACE);
  o->dodge = kp(KEY_LSHIFT) || kp(KEY_RSHIFT);
  o->special = kp(WC_KEY_F) || kp(WC_KEY_U);
  o->lock = kp(WC_KEY_TAB) || kp(KEY_MOUSE_M);
  o->wheel = kp(KEY_WHEEL_UP) ? 1 : kp(KEY_WHEEL_DOWN) ? -1 : 0;
  for (int k = 0; k < 5; k++) if (kp(WC_KEY_1 + k)) o->weapon_pick = k;
  o->interact = kp(WC_KEY_E);
  o->assist = kp(WC_KEY_R);
  o->draw = kp(WC_KEY_G);
  o->bike = kp(WC_KEY_B);
  o->sprint = DOWN(kcur, KEY_LALT) || DOWN(kcur, KEY_LSHIFT) || DOWN(kcur, KEY_RSHIFT);
  o->pause = kp(WC_KEY_ESCAPE) || kp(WC_KEY_P);
  o->ok = kp(WC_KEY_ENTER) || kp(WC_KEY_SPACE) || kp(WC_KEY_E) || kp(WC_KEY_J) || kp(KEY_MOUSE_L);
  o->back = kp(WC_KEY_ESCAPE) || kp(WC_KEY_BACKSPACE);
  o->mouse_look = DOWN(kcur, KEY_MOUSE_LOOK);
  int dx = (DOWN(kcur, WC_KEY_D) || DOWN(kcur, WC_KEY_RIGHT)) - (DOWN(kcur, WC_KEY_A) || DOWN(kcur, WC_KEY_LEFT));
  int dy = (DOWN(kcur, WC_KEY_S) || DOWN(kcur, WC_KEY_DOWN)) - (DOWN(kcur, WC_KEY_W) || DOWN(kcur, WC_KEY_UP));
  for (int p = 0; p < 4; p++) {
    const wc_pad_t *n = &pcur[p], *q = &pprev[p];
    if (!pad_live(p)) continue;
    uint32_t b = n->buttons, pr = b & ~q->buttons;
    float lx = stick(n->left_x), ly = -stick(n->left_y), rx = stick(n->right_x), ry = -stick(n->right_y);
    bool lt = n->left_trigger > WC_TRIGGER_PRESSED, rt = n->right_trigger > WC_TRIGGER_PRESSED;
    bool prt = q->right_trigger > WC_TRIGGER_PRESSED;
    bool mouse_stick = p == 0 && o->mouse_look;   /* the web page's mouse look rides on pad 0's right stick */
    if (mouse_stick) { rx *= MOUSE_GAIN; ry *= MOUSE_GAIN; }
    if (pr || lt || rt || fabsf(lx) + fabsf(ly) + (mouse_stick ? 0 : fabsf(rx) + fabsf(ry)) > 0.3f) last_pad = true;
    if (fabsf(lx) > fabsf(o->mx)) o->mx = lx;
    if (fabsf(ly) > fabsf(o->my)) o->my = ly;
    if (fabsf(rx) > fabsf(o->ax)) o->ax = rx;
    if (fabsf(ry) > fabsf(o->ay)) o->ay = ry;
    o->light |= (pr & WC_BTN_X) != 0 || (rt && !prt); o->light_held |= (b & WC_BTN_X) != 0 || rt;
    o->heavy |= (pr & WC_BTN_Y) != 0; o->heavy_held |= (b & WC_BTN_Y) != 0;
    o->guard |= lt;
    o->jump |= (pr & WC_BTN_A) != 0; o->jump_held |= (b & WC_BTN_A) != 0;
    o->dodge |= (pr & WC_BTN_B) != 0;
    o->special |= (pr & WC_BTN_R) != 0;
    o->lock |= (pr & (WC_BTN_L | WC_BTN_R3)) != 0;
    if (pr & WC_BTN_RIGHT) o->weapon_step = 1;
    if (pr & WC_BTN_LEFT) o->weapon_step = -1;
    o->assist |= (pr & WC_BTN_UP) != 0;
    o->draw |= (pr & WC_BTN_UP) != 0;     /* d-pad up: the assist in a fight, draw or sheathe in a roam */
    o->bike |= (pr & WC_BTN_DOWN) != 0;
    o->interact |= (pr & WC_BTN_X) != 0;
    o->sprint |= (b & WC_BTN_L3) != 0;
    o->pause |= (pr & WC_BTN_START) != 0;
    o->ok |= (pr & (WC_BTN_A | WC_BTN_START)) != 0 || (rt && !prt);
    o->back |= (pr & WC_BTN_B) != 0;
    int px = ((b & WC_BTN_RIGHT) || n->left_x > 16000) - ((b & WC_BTN_LEFT) || n->left_x < -16000);
    int py = ((b & WC_BTN_DOWN) || n->left_y > 16000) - ((b & WC_BTN_UP) || n->left_y < -16000);
    if (px) dx = px;
    if (py) dy = py;
  }
  o->guard_pressed = o->guard && !guard_was; guard_was = o->guard;
  o->nav_x = dx != nav_ox ? dx : 0; o->nav_y = dy != nav_oy ? dy : 0;
  nav_ox = dx; nav_oy = dy;
  o->used_pad = last_pad;
}
const intent_t *input_get(void) { return &I; }
bool input_start_held(void) {
  if (DOWN(kcur, WC_KEY_ENTER)) return true;
  for (int p = 0; p < 4; p++) if (pcur[p].connected && (pcur[p].buttons & WC_BTN_START)) return true;
  return false;
}
