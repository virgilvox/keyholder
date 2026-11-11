/* audio.h: the synth. Effects are fire-and-forget; music follows a mood. */
#ifndef SP_AUDIO_H
#define SP_AUDIO_H
#include <stdint.h>

typedef enum {
  SFX_SWING_ARC,    /* plasma through the air: a crackle and a sizzle */
  SFX_SWING_MAUL,   /* a great weight: a slow low whoosh */
  SFX_SWING_TWIN,   /* a quick thin whip */
  SFX_SWING_SPEAR,  /* a long airy sweep */
  SFX_BOLT, SFX_LANCE,
  SFX_CHARGE,       /* a rising whine (call every frame while charging, gain = charge) */
  SFX_GLINT,        /* a foe's windup: a bright ting so it can be heard coming */
  SFX_HIT_LIGHT, SFX_HIT_HEAVY,
  SFX_CLASH,        /* blocked: steel on steel */
  SFX_PARRY,        /* a ringing bell of a parry */
  SFX_GUARD_BREAK,
  SFX_BODY_FALL,
  SFX_SHOCK,        /* a slam into the ground */
  SFX_ROAR,         /* a boss turning up the heat */
  SFX_DODGE, SFX_PERFECT,
  SFX_SUMMON,       /* a weapon appears */
  SFX_SPECIAL_CHARGE, SFX_SPECIAL_RELEASE,
  SFX_KO,
  SFX_JUMP, SFX_LAND, SFX_STEP,
  SFX_PICKUP, SFX_WEAPON, SFX_ALARM,
  SFX_UI, SFX_UI_OK,
  SFX_COUNT
} sfx_t;

/* v1.1: MOOD_EXPLORE is the street by day; HOSTING the lab by day; TENSION a scene turning; NIGHT the quiet
 * late hours; ZONE the Battle Zone menu and ladder. MOOD_SILENT stays last. */
typedef enum { MOOD_TITLE, MOOD_EXPLORE, MOOD_FIGHT, MOOD_BOSS, MOOD_CLEAR, MOOD_CINE, MOOD_HOSTING, MOOD_TENSION, MOOD_NIGHT, MOOD_ZONE, MOOD_FINAL, MOOD_SILENT } mood_t;

void audio_init(uint32_t rate);
void audio_render(float *ring, uint32_t cap, uint32_t *cursor, uint32_t rate, double ms);
/* pan -1..1, gain 0..1 */
void sfx(sfx_t s, float pan, float gain);
/* a sound at a place in the lab, panned and attenuated from the listener (game.c sets it each frame) */
void sfx_at(sfx_t s, float x, float y, float z, float gain);
void audio_listener(float x, float y, float z, float yaw);
void music_mood(mood_t m);
void audio_set_music(int on);

#endif
