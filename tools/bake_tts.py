#!/usr/bin/env python3
"""Voices every line in source/lines.json with Kokoro 82M and packs assets/voice/voices.bin.

  KOKORO=/dir/with/kokoro-v1.0.onnx,voices-v1.0.bin CACHE=/dir python3 tools/bake_tts.py

Takes are cached by voice, speed and text, so a rerun only speaks what changed. The bank is IMA ADPCM
at 16 kHz: "SPVB", u32 version, u32 count, then count entries of (u32 key, u32 rate, u32 samples, u32 offset)
sorted by key, then the data. key is FNV-1a of "speaker|text". voice.c reads it.
Model files: thewh1teagle/kokoro-onnx releases, model-files-v1.0. Kokoro 82M is Apache 2.0.
"""
import hashlib, json, os, struct, sys
import numpy as np, soundfile as sf
from kokoro_onnx import Kokoro

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
RATE = int(os.environ.get('VOICE_RATE', '16000'))
KDIR = os.environ['KOKORO']
CACHE = os.environ.get('CACHE', os.path.join(ROOT, 'source/wav'))
os.makedirs(CACHE, exist_ok=True)
lines = json.load(open(os.path.join(ROOT, 'source/lines.json')))
kokoro = None

STEP = [7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767]
IDX = [-1,-1,-1,-1,2,4,6,8]

def fnv(s):
    h = 2166136261
    for b in s.encode('utf-8'):
        h ^= b
        h = (h * 16777619) & 0xffffffff
    return h

def adpcm(pcm):
    out = bytearray((len(pcm) + 1) // 2)
    pred, idx = 0, 0
    for i, v in enumerate(pcm):
        step = STEP[idx]
        diff = int(v) - pred
        code = 0
        if diff < 0:
            code = 8; diff = -diff
        d = step >> 3
        if diff >= step: code |= 4; diff -= step; d += step
        if diff >= step >> 1: code |= 2; diff -= step >> 1; d += step >> 1
        if diff >= step >> 2: code |= 1; d += step >> 2
        pred += -d if code & 8 else d
        pred = max(-32768, min(32767, pred))
        idx = max(0, min(88, idx + IDX[code & 7]))
        out[i >> 1] |= code << 4 if i & 1 else code
    return bytes(out)

entries, total = [], 0.0
for n, L in enumerate(lines):
    tag = hashlib.sha1(f"{L['voice']}|{L['speed']}|{L['text']}".encode()).hexdigest()[:20]
    path = os.path.join(CACHE, tag + '.wav')
    if not os.path.exists(path):
        if kokoro is None:
            kokoro = Kokoro(os.path.join(KDIR, 'kokoro-v1.0.onnx'), os.path.join(KDIR, 'voices-v1.0.bin'))
        lang = 'en-gb' if L['voice'].startswith('b') else 'en-us'
        samples, sr = kokoro.create(L['text'], voice=L['voice'], speed=L['speed'], lang=lang)
        a = np.asarray(samples, dtype=np.float32)
        nz = np.where(np.abs(a) > 0.01)[0]
        if len(nz):
            a = a[max(0, nz[0] - int(.03 * sr)): min(len(a), nz[-1] + int(.08 * sr))]
        sf.write(path, a, sr)
        print(f"{n + 1}/{len(lines)} {L['speaker']}: {L['text'][:60]}", flush=True)
    a, sr = sf.read(path, dtype='float32')
    m = int(len(a) * RATE / sr)
    x = np.interp(np.arange(m) * sr / RATE, np.arange(len(a)), a)
    peak = max(1e-4, float(np.max(np.abs(x))))
    x = np.clip(x * min(3.0, .85 / peak), -1, 1)
    total += m / RATE
    entries.append((fnv(L['speaker'] + '|' + L['text']), RATE, m, adpcm((x * 32767).astype(np.int16))))

entries.sort(key=lambda e: e[0])
if len({e[0] for e in entries}) != len(entries):
    sys.exit('hash collision in the bank')
os.makedirs(os.path.join(ROOT, 'assets/voice'), exist_ok=True)
with open(os.path.join(ROOT, 'assets/voice/voices.bin'), 'wb') as f:
    f.write(b'SPVB'); f.write(struct.pack('<II', 1, len(entries)))
    off = 0
    for k, r, m, d in entries:
        f.write(struct.pack('<IIII', k, r, m, off)); off += len(d)
    for e in entries:
        f.write(e[3])
print(f"{len(entries)} lines, {total:.0f} s of speech, {os.path.getsize(os.path.join(ROOT, 'assets/voice/voices.bin')) / 1048576:.1f} MB")
