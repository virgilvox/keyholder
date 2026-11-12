#ifndef KH_HUD_VN_H
#define KH_HUD_VN_H
#include <stdbool.h>
void hud_vn_draw(void);
void hud_title_ask(float t, int sel, const char *save_name, float hour, const char *where, const char *new_name);
void hud_title_draw(float t, int row, int who, bool has_save, const char *const *names, const char *const *blurbs);
/* save: 0 can save here, 1 not here, 2 just saved; leave: the last row's words (NULL: back to the title) */
void hud_pause_draw(int sel, bool music, int save, const char *leave);
/* the interact prompt ({USE} and what it does) centred low on screen, inside hg_begin/hg_end */
void hud_use_prompt(const char *label, float y);
void hud_nights_title(float t, bool sel, int night, int open_mask, int count, const char *title);   /* v1.3 */
void hud_end_draw(float t, const char *title, const char *const *lines, int n, const int *stats);
#endif
