/* game.h: the save, the game's entry points and the render hooks main.c provides. */
#ifndef DS_GAME_H
#define DS_GAME_H
#include "ds.h"
#include "three.h"
typedef struct { uint32_t magic; uint8_t data[1020]; } ds_save_t;
#define DS_SAVE_MAGIC 0x314b484bu
extern ds_save_t DS_SAVE;
extern t3_renderer *DS_R;
void render_scene(t3_scene *scene, t3_camera *cam);
void render_scene_split(t3_scene *scene, t3_camera *left, t3_camera *right, void (*before)(int));
void render_target(void);
void render_clear(uint32_t rgb);
void game_init(void);
void game_frame(float dt);
void game_draw(void);
#endif
