/* test_audio.c: the synth alone (KH_TEST_AUDIO), for rendering with WAV=... tools/shot.mjs.
 * Each mood plays 20 s, then a voice line over HOSTING (the duck), then every sfx 1.2 s
 * apart. The schedule is logged so a WAV can be cut up. Build with game.c left out:
 * KH_SRC="main.c present.c audio.c voice.c input.c test_audio.c" EXTRA_CFLAGS=-DKH_TEST_AUDIO */
#ifdef KH_TEST_AUDIO
#include "game.h"
#include "audio.h"
#include "voice.h"

#define MOOD_F 1200
#define VOICE_F 600
#define SFX_F 72
static int frame;

void game_init(void) {
  ds_log("[audio] schedule: %d moods x %d frames, voice %d frames, %d sfx x %d frames", MOOD_SILENT, MOOD_F, VOICE_F, SFX_COUNT, SFX_F);
  music_mood(MOOD_TITLE);
}
void game_frame(float dt) {
  int f = frame++, end_m = MOOD_SILENT * MOOD_F;
  if (f < end_m) { if (f % MOOD_F == 0) { music_mood((mood_t)(f / MOOD_F)); ds_log("[audio] mood %d at frame %d", f / MOOD_F, f); } return; }
  f -= end_m;
  if (f == 0) music_mood(MOOD_HOSTING);
  if (f == 180) { bool ok = voice_say("dex", "Who else knows you clean the kitchen?"); ds_log("[audio] voice at frame %d %s", frame - 1, ok ? "ok" : "missing"); }
  if (f < VOICE_F) return;
  f -= VOICE_F;
  if (f == 0) music_mood(MOOD_SILENT);
  int s = f / SFX_F - 1;   /* a second of silence for the fade */
  if (s >= 0 && s < SFX_COUNT && f % SFX_F == 0) {
    if (s != SFX_CHARGE) sfx((sfx_t)s, 0, 1);
    ds_log("[audio] sfx %d at frame %d", s, frame - 1);
  }
  if (s == SFX_CHARGE && f % SFX_F < 60) sfx(SFX_CHARGE, 0, (f % SFX_F) / 60.f);
}
void game_draw(void) { render_clear(0x101018); }
#endif
