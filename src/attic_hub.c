/* attic_hub.c: the attic come back to itself (see attic_int.h). You walk the blueprint room; the shelves'
 * examine spots (tools/prep-attic.mjs, src/gen/attic_data.h) each open a flashback, the pizza box on the rack
 * opens the terminal, and the stair head takes you back down. Prompts carry {USE} for g_markup, so the HUD
 * shows the button of the device in hand. */
#include "attic_int.h"
#include "attic_env.h"
#include "gen/attic_data.h"
#include "player.h"
#include "script.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define NEXAM ((int)(sizeof ATTIC_EXAM / sizeof ATTIC_EXAM[0]))

/* which flashback each shelf holds, and its name for the toast */
typedef struct { const char *zone; int scene; const char *title; } memory_t;
static const memory_t MEMORIES[HUB_MEMORIES] = {
  { "elec_balloon", SC_MEM_BALLOON, "THE BALLOON, 2010" },
  { "metal",        SC_MEM_LASER, "THE LASER CRATE, 2011" },
  { "crafts",       SC_MEM_GLITTER, "THE PINK COAT, 2011" },
  { "robots",       SC_MEM_ROBOTS, "THE THEATRE ROBOTS, 2012" },
  { "parts",        SC_MEM_TEARAPART, "TEAR APART NIGHT, 2012" },
  { "elec_arduino", SC_MEM_ARDUINO, "ARDUINO NIGHT, 2013" },
  { "members",      SC_MEM_MOVE, "MOVING DAY, 2018" },
  { "holiday",      SC_MEM_HAXMAS, "HAXMAS, 2019" },
};
static int memory_of_zone(const char *z) { for (int i = 0; i < HUB_MEMORIES; i++) if (!strcmp(MEMORIES[i].zone, z)) return i; return -1; }
int hub_scene_bit(int scene) { for (int i = 0; i < HUB_MEMORIES; i++) if (MEMORIES[i].scene == scene) return i; return -1; }
const char *hub_found_title(int scene) { int i = hub_scene_bit(scene); return i >= 0 ? MEMORIES[i].title : NULL; }
int hub_found_count(unsigned mask) { int n = 0; for (int i = 0; i < HUB_MEMORIES; i++) n += (mask >> i) & 1; return n; }

static char prompt[96];
static bool have_prompt;
static v3 back_pos; static float back_yaw;

void hub_begin(void) { float yaw; v3 e = attic_env_entry(&yaw); player_teleport(e, yaw); player_camera_snap(); }
void hub_return(void) { player_teleport(back_pos, back_yaw); player_camera_snap(); }
const char *hub_prompt(void) { return have_prompt ? prompt : NULL; }
/* "THE PARTS SHELF" -> "Look at the parts shelf", in the lab prompts' own case */
static void look_at(const char *label) {
  int n = snprintf(prompt, sizeof prompt, "Look at ");
  for (const char *c = label; *c && n < (int)sizeof prompt - 1; c++) prompt[n++] = (*c >= 'A' && *c <= 'Z') ? (char)(*c - 'A' + 'a') : *c;
  prompt[n] = 0;
}

int hub_update(float dt, const intent_t *in, int *arg) {
  player_update(dt, in);
  v3 p = player_pos();
  have_prompt = false;
  /* the nearest thing in reach wins: an examine spot, the terminal, the stairs */
  int best = -1; float bd = 1e9f;
  for (int i = 0; i < NEXAM; i++) { float d = hypotf(p.x - ATTIC_EXAM[i].x, p.z - ATTIC_EXAM[i].z); if (d < ATTIC_EXAM[i].r && d < bd) { bd = d; best = i; } }
  float ty, ey; v3 t = attic_env_term(&ty), e = attic_env_entry(&ey);
  float dt_term = hypotf(p.x - t.x, p.z - t.z), dt_stair = hypotf(p.x - e.x, p.z - e.z);
  if (dt_term < 1.0f && dt_term < bd) {
    snprintf(prompt, sizeof prompt, "Use the terminal"); have_prompt = true;
    if (in->interact) return HUB_TERMINAL;
  } else if (best >= 0) {
    int m = memory_of_zone(ATTIC_EXAM[best].zone);
    look_at(ATTIC_EXAM[best].label); have_prompt = true;
    if (in->interact && m >= 0) {
      back_pos = p; back_yaw = atan2f(ATTIC_EXAM[best].x - p.x, ATTIC_EXAM[best].z - p.z);
      *arg = MEMORIES[m].scene; return HUB_MEMORY;
    }
  } else if (dt_stair < 0.9f) {
    snprintf(prompt, sizeof prompt, "Go back down"); have_prompt = true;
    if (in->interact) return HUB_LEAVE;
  }
  return HUB_NONE;
}

#ifdef KH_ATTIC_PATH
/* test build only: stand at the k-th examine spot, facing its shelf */
void hub_test_jump(int k) {
  const attic_exam_t *e = &ATTIC_EXAM[k % NEXAM];
  player_teleport(v3_(e->x, 0, e->z), 0); player_camera_snap();
}
#endif
