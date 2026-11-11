/* street.h: the people you can talk to in a roam, the doors you can walk through, and Moheeb and Luis
 * on the Chupacabra patio.
 *
 * Who says what is data: tools/script/street.mjs, compiled by tools/script/street_compile.mjs into
 * src/gen/street_data.c (and source/street_lines.json for the voices). The people are story.c's extras
 * (its extras region casts a stranger onto a background body and tells street.c who is in reach); a
 * talk runs here, over the roam, with the dialogue box and the choice menu of STORY_UI and the director.
 * Nothing here is needed to finish the day: strangers are optional and only in roams.
 *
 * story.c calls (each hook marked street there): street_init from boot (it resets the day's memory),
 * street_roam after the roam's frame (which it skips while a talk or a door holds the player),
 * street_after once a frame after the HUD state is set, street_task_point for a task's spot and
 * street_end_lines on the epilogue's last page; its extras region calls street_stop from extras_reset
 * (story_stop) and the casting functions below. story.c defines story_flag for talks that react to the day. */
#ifndef KH_STREET_H
#define KH_STREET_H
#include "ds.h"
#include "input.h"
#include "three.h"
#include "actor.h"
#include "interior.h"

void street_init(t3_scene *scene, t3_camera *cam, void (*occluders)(void));
void street_reset(void);                     /* a new day, or a loaded one: nobody remembers you yet */
void street_stop(void);                      /* back to the title: the cameo hidden, a talk dropped */
bool street_busy(void);                      /* a talk or a door holds the player: the roam's frame waits */
void street_roam(float dt, const intent_t *in);   /* every roam frame, after the roam's own (prompts, talks, doors) */
void street_after(float dt);                 /* every frame, after story_update set the HUD state */
v3 street_task_point(v3 p);                  /* a task's spot, moved to the counter when you are inside its venue */
int street_end_lines(const char **out, int n, int cap);   /* the epilogue: a line about the people you met */

/* story.c: a story flag by name (FLAG_NAMES), false if there is none */
bool story_flag(const char *name);

/* ---- the extras (story.c's extras region) ---- */
enum { SW_STREET = 1, SW_INSIDE = 2, SW_LAB = 4 };
/* cast a stranger onto background body `body` in slot `slot` for `where` (SW_*): a type, or -1 for a
 * passer by. *alt says which of the body's two looks to wear. seed varies the pick. */
int street_cast(int slot, actor_model body, int where, unsigned seed, bool *alt);
void street_uncast(int slot);
void street_people_begin(void);              /* each frame, before the extras say who is about */
void street_person(int slot, actor_t *a, const char *clip);   /* a cast extra, visible and near, and the clip they rest in */
bool street_holds(int slot);                 /* they are in a talk: stand still, face you */
#ifdef KH_STREET_TEST
#define STREET_NEAR 90.f                     /* how near an extra must be to be told about (a test finds them from afar) */
#else
#define STREET_NEAR 6.f
#endif
/* inside a venue: where its people go (its named spots, else spots spread from the door). pos y is the floor. */
int street_inside_spots(v3 *pos, float *yaw, bool *behind, int cap);

/* ---- the data (gen/street_data.c) ---- */
enum { SB_SAY, SB_CHOICE, SB_OPT, SB_JUMP, SB_ADD, SB_MARK, SB_NOTE, SB_END };
enum { SP_THEM, SP_ME, SP_MOHEEB, SP_LUIS };
/* a beat. SAY: who says text (by who is playing: Vee, Dex, Ruth), clip a gesture or NULL. CHOICE: text[0]
 * the prompt, a the option count, the options follow. OPT: text the menu line, say the spoken words (NULL:
 * an action, never spoken), a its first beat. JUMP: to a. ADD: stat a by b. MARK: text[0] remembered.
 * NOTE: text[0] on screen. END. */
typedef struct { uint8_t op, who; int16_t a, b; const char *text[3], *say[3]; const char *clip; } st_beat;
/* when a talk can happen. met: -1 either, 0 a first meeting, 1 talked before. flag and mark: a story flag
 * and a street memory that must be set (or must not, with _not). stat at least stat_v (at most, with
 * stat_low; stat -1 none).
 * venue: the place key of the room you are in. again: it can be had more than once a day. */
typedef struct {
  float h0, h1; int8_t met; const char *flag; bool flag_not; const char *mark; bool mark_not;
  int8_t stat; int8_t stat_v; bool stat_low; const char *venue; bool again; int start;
} st_talk;
typedef struct {
  const char *id, *name, *speaker; uint32_t color;
  uint64_t bodies;              /* bit per actor_model it can wear */
  uint8_t where, look;          /* SW_* bits; look 0 or 1: which of the body's looks */
  float h0, h1;                 /* when they are about */
  int t0, nt;                   /* talks: ST_TALKS[t0, t0 + nt) */
} st_type;
typedef struct { const char *name, *speaker; uint32_t color; } st_person;
extern const st_beat ST_BEATS[];
extern const st_talk ST_TALKS[];
extern const st_type ST_TYPES[];
extern const int ST_NTYPE, ST_NTALK;
/* the cameo: Moheeb and Luis, their hours, chatter between them (talks of SAY beats) and talks with you */
extern const st_person ST_CAMEO[2];
extern const float ST_CAMEO_H0, ST_CAMEO_H1;
extern const int ST_CHAT0, ST_NCHAT, ST_CTALK0, ST_NCTALK;
/* the downtown place keys (docs/PLACES.md), to tell which venue a room belongs to */
extern const char *const ST_VENUES[];
extern const int ST_NVENUE;
#endif
