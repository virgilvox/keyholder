#ifndef KH_HUD_VN_H
#define KH_HUD_VN_H
#include <stdbool.h>
void hud_vn_draw(void);
void hud_title_draw(float t, int row, int who, bool has_save, const char *const *names, const char *const *blurbs);
void hud_pause_draw(int sel, bool music);
void hud_end_draw(float t, const char *title, const char *const *lines, int n, const int *stats);
#endif
