/* terminal.h: the attic server `doorman`. An old pizza-box node that ran the lab wiki and a daemon
 * `lockd` that locks the doors at 22:00. Its console is a green-phosphor CRT: a boot log, a program
 * menu, and four terminal games unlocked one after another:
 *
 *     GLITCH  ->  WIRETAP  ->  CORE  ->  DAEMON WAR
 *
 * Each game lives in its own file behind the small term_game interface (term_game.h); terminal.c is the
 * shell (boot, menu, unlock chain). The terminal is a sub-phase of attic.c (A_TERMINAL drives it), not a
 * game.c mode. Drawn full screen over the frame after render_scene/render_target.
 *
 * The unlock mask is a uint16 the story persists (save_t.attic_games) and hands to term_open; term_unlocked
 * returns it again after play so the story can save any new progress. Bit meanings (low nibble: available
 * in the menu, high nibble: cleared):
 *
 *   bit 0  TERM_AV_GLITCH    GLITCH available in the menu   (always set on open)
 *   bit 1  TERM_AV_WIRETAP   WIRETAP available              (set when GLITCH is cleared)
 *   bit 2  TERM_AV_CORE      CORE available                 (set when WIRETAP is cleared)
 *   bit 3  TERM_AV_DAEMON    DAEMON WAR available           (set when CORE is cleared)
 *   bit 4  TERM_CL_GLITCH    GLITCH cleared
 *   bit 5  TERM_CL_WIRETAP   WIRETAP cleared
 *   bit 6  TERM_CL_CORE      CORE cleared
 *   bit 7  TERM_CL_DAEMON    DAEMON WAR cleared
 */
#ifndef KH_TERMINAL_H
#define KH_TERMINAL_H
#include "ds.h"
#include "input.h"

#define TERM_AV_GLITCH  (1u << 0)
#define TERM_AV_WIRETAP (1u << 1)
#define TERM_AV_CORE    (1u << 2)
#define TERM_AV_DAEMON  (1u << 3)
#define TERM_CL_GLITCH  (1u << 4)
#define TERM_CL_WIRETAP (1u << 5)
#define TERM_CL_CORE    (1u << 6)
#define TERM_CL_DAEMON  (1u << 7)

void term_init(void);                              /* once (attic_init calls it) */
void term_open(unsigned unlocked);                 /* reset to boot/menu; unlocked = the saved mask */
bool term_update(float dt, const intent_t *in);    /* false when the player logs off (backs out of the menu) */
void term_draw(void);                              /* the full-screen CRT, after render_scene/render_target */
unsigned term_unlocked(void);                      /* the mask after play (clearing a game opens the next) */
#endif
