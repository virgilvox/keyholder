/* input.h: the player's intent each frame, from every pad and the keyboard.
 *
 *                       Pad                Keyboard            Mouse (web page)
 *   move                left stick         WASD
 *   camera              right stick        arrows              move (pointer lock)
 *   light attack        X, RT              J, F, Left Ctrl     left button
 *   heavy attack        Y                  K
 *   guard (held)        LT                 L                   right button
 *   jump                A                  Space
 *   dodge               B                  Shift
 *   special             RB                 U                   wheel
 *   lock on (toggle)    LB, R3             Tab                 wheel
 *   weapon              d-pad left/right   1 to 5, Q
 *   talk, use           X                  E
 *   sprint (held)       L3, or stick full  Left Alt
 *   pause               START              Esc, P
 *   menus: stick, d-pad or arrows; A, RT, a click or Enter picks, B or Esc backs out. */
#ifndef DS_INPUT_H
#define DS_INPUT_H
#include <stdbool.h>
#include <stdint.h>
#include "wasmcart.h"

typedef struct {
  float mx, my;            /* move, -1..1 (y +1 = forward) */
  float ax, ay;            /* camera rate, -1..1 (y +1 = up) */
  bool light, heavy, special, dodge, jump;   /* pressed this frame */
  bool light_held, heavy_held, jump_held;
  bool guard;              /* held */
  bool guard_pressed;      /* this frame: the parry window opens */
  bool lock;               /* pressed: toggle lock on */
  int weapon_step;         /* -1, +1: weapon cycle this frame */
  int weapon_pick;         /* -1, or a slot from the number keys */
  bool interact, sprint, pause;
  int nav_x, nav_y;        /* menus: edges, y +1 = down */
  bool ok, back;
  bool used_pad;           /* the last thing touched was a pad */
} intent_t;

void input_frame(const wc_pad_t *pads, const uint8_t *keys);
const intent_t *input_get(void);
bool input_key_down(int code);
bool input_start_held(void);   /* START or Enter is down (cutscene skip hold) */
#endif
