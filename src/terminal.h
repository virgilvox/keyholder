/* terminal.h: the attic server's "pizza box" terminal. A green-CRT screen with a boot log and a menu
 * of hacking minigames (Johnny-Mnemonic node-trace runs), drawn full screen over the frame. It is a
 * sub-phase of the attic (attic.c's A_TERMINAL drives it), not its own game.c mode. */
#ifndef KH_TERMINAL_H
#define KH_TERMINAL_H
#include "ds.h"
#include "input.h"

void term_init(void);                              /* once (attic_init calls it) */
void term_open(unsigned unlocked);                 /* reset to the boot/menu; unlocked = bitmask of hacks available */
bool term_update(float dt, const intent_t *in);    /* false when the player logs off (backs out of the top menu) */
void term_draw(void);                              /* the full-screen CRT, after render_scene/render_target */
unsigned term_unlocked(void);                      /* the unlock bitmask after play (clearing a hack may open the next) */
#endif
