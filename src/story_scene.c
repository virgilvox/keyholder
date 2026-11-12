/* story_scene.c: the attic's scenes (tools/script/attic.mjs, by SC_*): encounters, the boss and the flashbacks,
 * played by the story VM from attic.c. The day is held while one plays (story_day_hold) and given back at its
 * scene_end beat; a flashback is seen through the memory grade (present_memory) and may stand in a memory
 * room (one forty) or set out its pieces (memory_props.c), all of which go when it ends. */
#include "story_int.h"
#include "script.h"
#include "present.h"
#include "memory_props.h"
#include "interior.h"
#include <math.h>

static bool busy, won;
static float grade_amt, grade_to, grade_rate = 1;

bool story_scene_play(int sc) {
  if (sc < 0 || sc >= SC_COUNT || busy) return false;
  busy = true; won = false;
  story_day_hold(SCENES[sc].start);
  return true;
}
bool story_scene_busy(void) { return busy; }
void story_scene_won(void) { if (busy) won = true; }
bool story_scene_fight_won(void) { return won; }

void story_scene_end(void) {
  memory_props_hide();
  if (interior_current() >= 0) interior_exit();
  grade_to = 0; grade_rate = 100;   /* the attic itself is never seen through a memory */
  story_day_release();
  busy = false;
}

/* quitting to the title in the middle of one: the scene's pieces go, the day is not given back (it is over) */
void story_scene_abort(void) {
  if (!busy) return;
  memory_props_hide();
  if (interior_current() >= 0) interior_exit();
  grade_to = grade_amt = 0; present_memory(0, 0);
  busy = false;
}
void story_scene_grade(bool memory, float t) { grade_to = memory ? 1.f : 0.f; grade_rate = t > 0 ? 1.f / t : 100.f; }
void story_scene_tick(float dt) {
  grade_amt = grade_to > grade_amt ? fminf(grade_to, grade_amt + grade_rate * dt) : fmaxf(grade_to, grade_amt - grade_rate * dt);
  present_memory(grade_amt, ds_now());
}

/* the attic between scenes: the town's people stay out of sight up there, and the grade keeps easing */
void story_attic_tick(float dt) { story_extras_tick(dt); story_scene_tick(dt); }
