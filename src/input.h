/* input.h: the player's intent each frame, from every pad and the keyboard and mouse.
 *
 *                       Pad                Keyboard and mouse (web page)
 *   move                left stick         WASD
 *   camera              right stick        mouse (pointer lock), arrows
 *   light attack        X, RT              Q; left click (fires on the press), J
 *   heavy attack        Y                  F, K
 *   guard (held)        LT                 C; right click, L       (a tap as a blow lands parries)
 *   dodge               B                  Shift (tap)
 *   sprint (held)       L3, or stick full  Shift (held), Left Alt
 *   jump                A                  Space
 *   special             RB                 X, U
 *   lock on (toggle)    LB, R3             Tab, middle click       (in roams: a step through three zooms)
 *   switch target       right stick flick  mouse wheel while locked on
 *   zoom                LB, R3 in roams    mouse wheel (when not locked on)
 *   weapon              d-pad left/right   1 to 5
 *   assist (call ally)  d-pad up           R
 *   talk, use           X                  E
 *   draw, sheathe       d-pad up (roam)    G
 *   bike (outdoors)     d-pad down (roam)  B
 *   pause               START              Esc, P
 *   menus: stick, d-pad or arrows; A, START, a left click or Enter picks, B or Esc backs out.
 *
 * The mouse: the web page reports its buttons and wheel as key codes in the HID reserved range
 * (KEY_MOUSE_* below), so they sit in the same bitmap as the keyboard and count as the keyboard for
 * the glyphs. Mouse motion still arrives as pad 0's right stick, and KEY_MOUSE_LOOK is held while the
 * pointer is captured, so that motion does not switch the prompts to pad glyphs. */
#ifndef DS_INPUT_H
#define DS_INPUT_H
#include <stdbool.h>
#include <stdint.h>
#include "wasmcart.h"

#define KEY_MOUSE_L 0xF0      /* left button */
#define KEY_MOUSE_R 0xF1      /* right button */
#define KEY_MOUSE_M 0xF2      /* middle button */
#define KEY_WHEEL_UP 0xF3     /* pulsed a few frames per notch */
#define KEY_WHEEL_DOWN 0xF4
#define KEY_MOUSE_LOOK 0xF5   /* held while the pointer is captured */

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
  bool assist;             /* pressed: call an ally (fight) */
  bool interact, sprint, pause;
  int nav_x, nav_y;        /* menus: edges, y +1 = down */
  bool ok, back;
  bool used_pad;           /* the last thing touched was a pad */
  /* v1.2 */
  int wheel;               /* mouse wheel this frame: +1 up, -1 down (locked on: targets, else the camera zoom) */
  bool mouse_look;         /* the camera rate comes from a captured mouse, not a stick */
  bool draw;               /* pressed: draw or sheathe the weapon (roam) */
  bool bike;               /* pressed: summon or leave the bike (roam, outdoors) */
} intent_t;

void input_frame(const wc_pad_t *pads, const uint8_t *keys);
const intent_t *input_get(void);
bool input_key_down(int code);
bool input_start_held(void);   /* START or Enter is down (cutscene skip hold) */
#endif
