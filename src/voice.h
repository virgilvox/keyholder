/* voice.h: every spoken line is a recording in assets/voice/voices.bin, baked with
 * Kokoro 82M by tools/bake_tts.py. A line is found by the FNV-1a hash of "speaker|text".
 * audio.c mixes the voice in and ducks the music; actors lip sync off voice_mouth. */
#ifndef KH_VOICE_H
#define KH_VOICE_H
#include <stdbool.h>
#include <stdint.h>

void voice_init(uint32_t rate);
/* speak now, replacing whatever is playing; false if the line is not in the bank */
bool voice_say(const char *speaker, const char *text);
void voice_stop(void);
bool voice_busy(void);
float voice_seconds(const char *speaker, const char *text);   /* a line's length, 0 if not in the bank */
/* the current line's mouth: open 0..1 and a vowel shape 0..1 */
void voice_mouth(float *open, float *shape);
void voice_set_enabled(bool on);
/* audio.c: add the voice to n stereo frames; returns the ducking amount 0..1 */
float voice_mix(float *lr, int n);
#endif
