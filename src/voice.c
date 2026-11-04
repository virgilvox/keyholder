/* voice.c: see voice.h. The bank is IMA ADPCM at one fixed rate per line. */
#include "voice.h"
#include "ds.h"
#include <stdlib.h>
#include <string.h>

typedef struct { uint32_t key, rate, n, off; } bank_ent;
static uint8_t *bank; static size_t bank_len;
static const bank_ent *tab; static uint32_t nent;
static const uint8_t *data;
static uint32_t out_rate = 48000;
static bool enabled = true;
static int16_t *pcm; static uint32_t pcm_n, pcm_rate;
static double pos;
static float fade = 1; static bool stopping;
static float m_open, m_shape, duck;

static uint32_t line_key(const char *sp, const char *text) {
  uint32_t h = 2166136261u;
  for (const char *p = sp; *p; p++) { h ^= (uint8_t)*p; h *= 16777619u; }
  h ^= '|'; h *= 16777619u;
  for (const char *p = text; *p; p++) { h ^= (uint8_t)*p; h *= 16777619u; }
  return h;
}
static const bank_ent *find(uint32_t key) {
  uint32_t lo = 0, hi = nent;
  while (lo < hi) { uint32_t m = (lo + hi) / 2; if (tab[m].key < key) lo = m + 1; else hi = m; }
  return lo < nent && tab[lo].key == key ? &tab[lo] : NULL;
}
static int16_t *decode(const bank_ent *e) {
  static const int16_t STEP[89] = {7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767};
  static const int8_t IDX[8] = {-1, -1, -1, -1, 2, 4, 6, 8};
  if ((size_t)(data - bank) + e->off + (e->n + 1) / 2 > bank_len) return NULL;
  int16_t *o = malloc((size_t)e->n * 2);
  if (!o) return NULL;
  int pred = 0, idx = 0;
  const uint8_t *d = data + e->off;
  for (uint32_t i = 0; i < e->n; i++) {
    int code = (i & 1) ? d[i >> 1] >> 4 : d[i >> 1] & 15, step = STEP[idx], diff = step >> 3;
    if (code & 4) diff += step;
    if (code & 2) diff += step >> 1;
    if (code & 1) diff += step >> 2;
    pred += code & 8 ? -diff : diff;
    pred = pred < -32768 ? -32768 : pred > 32767 ? 32767 : pred;
    idx += IDX[code & 7]; idx = idx < 0 ? 0 : idx > 88 ? 88 : idx;
    o[i] = (int16_t)pred;
  }
  return o;
}

void voice_init(uint32_t rate) {
  out_rate = rate;
  bank = ds_asset("voices.bin", &bank_len);
  if (!bank || bank_len < 12 || memcmp(bank, "SPVB", 4)) { free(bank); bank = NULL; return; }
  uint32_t n; memcpy(&n, bank + 8, 4);
  if (12 + (size_t)n * 16 > bank_len) { free(bank); bank = NULL; return; }
  nent = n; tab = (const bank_ent *)(bank + 12); data = bank + 12 + (size_t)n * 16;
}
bool voice_say(const char *sp, const char *text) {
  voice_stop();
  free(pcm); pcm = NULL; pcm_n = 0; stopping = false; fade = 1; pos = 0;
  if (!enabled || !bank) return false;
  const bank_ent *e = find(line_key(sp, text));
  if (!e) { ds_log("[vo] not in the bank: %s|%s", sp, text); return false; }
  pcm = decode(e);
  if (!pcm) return false;
  pcm_n = e->n; pcm_rate = e->rate;
  return true;
}
void voice_stop(void) { if (pcm && pos < pcm_n) stopping = true; }
bool voice_busy(void) { return pcm && pos < pcm_n; }
float voice_seconds(const char *sp, const char *text) {
  if (!bank) return 0;
  const bank_ent *e = find(line_key(sp, text));
  return e ? (float)e->n / (float)e->rate : 0;
}
void voice_mouth(float *open, float *shape) { *open = m_open; *shape = m_shape; }
void voice_set_enabled(bool on) { enabled = on; if (!on) voice_stop(); }
float voice_mix(float *lr, int n) {
  float rms = 0; int zc = 0, cnt = 0, last = 0;
  if (pcm) {
    double step = (double)pcm_rate / out_rate;
    for (int i = 0; i < n && pos < pcm_n - 1; i++) {
      uint32_t k = (uint32_t)pos; float f = (float)(pos - k);
      float s = (pcm[k] + (pcm[k + 1] - pcm[k]) * f) / 32768.f;
      rms += s * s; cnt++;
      int sg = s > 0; if (sg != last) zc++; last = sg;
      if (stopping) { fade -= 1.f / (out_rate * .04f); if (fade <= 0) { pos = pcm_n; break; } }
      float o = s * fade * 1.15f;
      lr[i * 2] += o; lr[i * 2 + 1] += o;
      pos += step;
    }
  }
  float lv = cnt ? sqrtf(rms / cnt) : 0;
  float target = ds_clamp(lv * 9.f, 0, 1);
  m_open += (target - m_open) * .55f;
  if (cnt > 16) { float z = (float)zc / cnt; m_shape += (ds_clamp((z - .02f) * 8.f, 0, 1) - m_shape) * .3f; }
  duck += ((cnt ? 1.f : 0.f) - duck) * .2f;
  return duck;
}
