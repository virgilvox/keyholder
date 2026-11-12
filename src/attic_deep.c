/* attic_deep.c: walking the nightmare attic (see attic_int.h). The route, its soft boards, landings,
 * clearings and boss chamber are attic_env's (tools/prep-attic-deep.mjs); this decides what happens on them.
 * A board a step ahead creaks; standing on one, it gives: a short drop to black and back at the last landing
 * you reached. Walking into a clearing that is not yet cleared asks attic.c for its encounter scene; the boss
 * chamber asks for SYSOP. The player walks with the ordinary player_update. */
#include "attic_int.h"
#include "attic_env.h"
#include "player.h"
#include "audio.h"
#include "fx.h"
#include "script.h"
#include <math.h>
#include <string.h>

#define NCLEAR 8
static int last_landing, falls;
static unsigned cleared;
static float fall_t = -1, creak_t;
static int fall_to;
static const char *zone_now = "THE ATTIC";

int deep_scene_for(int clearing) {
  float r; const char *z = NULL; attic_env_clearing(clearing, &r, &z);
  if (!z) return -1;
  if (!strcmp(z, "holiday")) return SC_ENC_HOLIDAY;
  if (!strcmp(z, "members")) return SC_ENC_MEMBERS;
  if (!strcmp(z, "robotics")) return SC_ENC_ROBOTS;
  return SC_ENC_PARTS;
}
static const char *zone_title(const char *z) {
  if (!z) return "THE ATTIC";
  if (!strcmp(z, "holiday")) return "HOLIDAY SUPPLIES";
  if (!strcmp(z, "members")) return "OLD MEMBER STORAGE";
  if (!strcmp(z, "robotics")) return "ROBOTICS";
  return "METAL AND PARTS";
}

void deep_begin(void) {
  float yaw; v3 e = attic_env_entry(&yaw);
  last_landing = 0; fall_t = -1; creak_t = 0; zone_now = "THE ATTIC";
  player_teleport(e, yaw); player_camera_snap();
  attic_env_focus(e);
}
void deep_cleared(int c) { if (c >= 0 && c < NCLEAR) cleared |= 1u << c; }
bool deep_all_cleared(void) { int n = attic_env_clearing_count(); for (int i = 0; i < n && i < NCLEAR; i++) if (!(cleared & (1u << i))) return false; return true; }
void deep_retreat(void) { float yaw; v3 p = attic_env_landing(last_landing, &yaw); player_teleport(p, yaw); player_camera_snap(); }
float deep_fade(void) { if (fall_t < 0) return 0; return fall_t < .35f ? fall_t / .35f : fall_t < .9f ? 1 : fmaxf(0, 1 - (fall_t - .9f) / .4f); }
int deep_falls(void) { return falls; }
const char *deep_zone(void) { return zone_now; }

int deep_update(float dt, const intent_t *in, int *arg) {
  /* a board giving way: the drop, black, then up at the landing */
  if (fall_t >= 0) {
    fall_t += dt;
    if (fall_t >= .9f && fall_t - dt < .9f) { float yaw; v3 p = attic_env_landing(fall_to, &yaw); player_teleport(p, yaw); player_camera_snap(); }
    if (fall_t > 1.3f) fall_t = -1;
    intent_t still; memset(&still, 0, sizeof still); still.weapon_pick = -1;
    player_update(dt, &still);
    return DEEP_NONE;
  }
  player_update(dt, in);
  v3 p = player_pos();
  attic_env_focus(p);

  /* the landings you reach become where a fall sends you */
  int nl = attic_env_landing_count();
  for (int i = last_landing + 1; i < nl; i++) { float y; v3 l = attic_env_landing(i, &y); if (hypotf(p.x - l.x, p.z - l.z) < 1.3f) last_landing = i; }

  /* a soft board a step ahead creaks; on one, it gives */
  creak_t -= dt;
  float yaw = player_yaw();
  v3 ahead = v3_(p.x + sinf(yaw) * .55f, 0, p.z + cosf(yaw) * .55f);
  if (creak_t <= 0 && attic_env_soft_at(ahead) >= 0) { sfx(SFX_TELL, 0, .35f); creak_t = .8f; }
  int to = attic_env_soft_at(p);
  if (to >= 0) {
    fall_t = 0; fall_to = to < last_landing ? to : last_landing; falls++;
    sfx(SFX_SMASH, 0, .7f); sfx(SFX_BODY_FALL, 0, .8f); fx_dust(p, 1.2f, 18);
    return DEEP_NONE;
  }

  /* the zone you are in, and a clearing not yet cleared starts its fight */
  int nc = attic_env_clearing_count();
  for (int i = 0; i < nc && i < NCLEAR; i++) {
    float r; const char *z; v3 c = attic_env_clearing(i, &r, &z);
    float d = hypotf(p.x - c.x, p.z - c.z);
    if (d < r + 4) zone_now = zone_title(z);
    if (!(cleared & (1u << i)) && d < r - 1.2f) { attic_env_set_arena(c, r); *arg = i; return DEEP_ENCOUNTER; }
  }
  float br; v3 b = attic_env_boss(&br);
  if (hypotf(p.x - b.x, p.z - b.z) < br + 4) zone_now = "THE RACK";
  if (hypotf(p.x - b.x, p.z - b.z) < br - 1.5f) { attic_env_set_arena(b, br); return DEEP_BOSS; }
  return DEEP_NONE;
}
