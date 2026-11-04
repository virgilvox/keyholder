/* test_stub.c: flat stand-ins for world, fight and player, so the story can run alone. */
#ifdef KH_STUB
#include "world.h"
#include "fight.h"
#include "player.h"
#include <string.h>
static t3_light *H, *D;
void world_init(t3_scene *s) {
  s->has_background = true; s->background = t3_color_hex(0x7fb0e0); s->fog = t3_fog_linear(0x7fb0e0, 30, 300);
  H = t3_hemisphere_light_new(0xffffff, 0xb0a8b8, 2.4f); t3_object_add(&s->base, H);
  D = t3_directional_light_new(0xfff4e0, 1.6f); t3_object_set_position(D, 2, 5, 4); t3_object_add(&s->base, D);
  t3_mesh *g = t3_mesh_new(t3_plane_geometry_new(400, 400, 1, 1), t3_mesh_lambert_material_new(0xa09888)); t3_object_set_rotation(&g->base, -DS_PI / 2, 0, 0); t3_object_add(&s->base, &g->base);
}
void world_set_stage(world_stage s) {} world_stage world_get_stage(void) { return STAGE_TOWN; } void world_update(float dt, v3 f) {} void world_set_time(float h) {}
void world_stage_floor(world_stage s, v3 *c, float *r) { *c = v3_(0, 0, 0); *r = 11; }
void world_move(v3 *p, v3 *v, float r, float h, float dt, bool *g) { p->x += v->x * dt; p->z += v->z * dt; *g = true; }
bool world_raycast(v3 o, v3 d, float m, float *t, v3 *n) { return false; } bool world_visible(v3 a, v3 b) { return true; } float world_floor_y(v3 p) { return 0; } bool world_in_lab(v3 p) { return false; }
v3 world_lab(float x, float z) { return v3_(z, 0, -x); } float world_lab_yaw(float d) { return d * DS_PI / 180; }
bool world_place(const char *n, v3 *p, float *y) { *p = v3_(0, 0, 3); *y = 0; return true; } void world_door(float o) {}
static actor_t *PA; static t3_camera *C;
void player_init(t3_camera *c) { C = c; } void player_set_actor(actor_t *a) { PA = a; } actor_t *player_actor(void) { return PA; } void player_teleport(v3 p, float y) {} void player_update(float dt, const intent_t *i) {} void player_camera_snap(void) {} v3 player_pos(void) { return actor_pos(PA); } float player_yaw(void) { return 0; }
void fight_init(t3_scene *s, t3_camera *c) {} void fight_start(const fight_def *d, actor_t *p) {} fight_state fight_update(float dt, const intent_t *i) { return FIGHT_WON; } void fight_draw_hud(void) {} float fight_player_hp(void) { return 1; } void fight_end(void) {} actor_t *fight_foe_actor(int i) { return NULL; }
void weapon_equip(actor_t *a, int w) {} void weapon_draw_flash(actor_t *a) {}
const char *weapon_name(weapon_id w) { return ""; }
void fx_init(t3_scene *s) {} void fx_update(float dt, const t3_camera *c) {}
#endif
