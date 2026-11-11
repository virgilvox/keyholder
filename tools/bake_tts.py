#!/usr/bin/env python3
"""Voices every line in source/lines.json (the day) and source/street_lines.json (the strangers and the
Chupacabra cameo) and packs assets/voice/voices.bin.

  KOKORO=/dir/with/kokoro-v1.0.onnx,voices-v1.0.bin CACHE=/dir python3 tools/bake_tts.py

Most lines are Kokoro 82M. Moheeb and Luis (the cameo) are Kyutai Pocket TTS clones of their own voices,
the same references SMALL PARTS uses: per speaker overrides in KYUTAI below. Those are made in one batch
by tools/voices_kyutai.py in its own Python (pocket-tts needs torch):

  KYUTAI_PY="uv run --python 3.12 --with pocket-tts python"   (the default)
  MOHEEB_WAV=~/Downloads/moheeb-voice.wav LUIS_WAV=~/Downloads/luis-voice.wav   (the defaults; not in the repo)

If the references or the generator are missing, a Kyutai line falls back to its Kokoro stand in voice
(street.mjs CAMEO.people) and says so; a later run with Kyutai set up replaces it.

Takes are cached by engine, voice, speed and text, so a rerun only speaks what changed. The bank is IMA
ADPCM at 16 kHz: "SPVB", u32 version, u32 count, then count entries of (u32 key, u32 rate, u32 samples,
u32 offset) sorted by key, then the data. key is FNV-1a of "speaker|text". voice.c reads it.
Model files: thewh1teagle/kokoro-onnx releases, model-files-v1.0. Kokoro 82M is Apache 2.0.
"""
import hashlib, json, os, re, shlex, struct, subprocess, sys, tempfile
import numpy as np, soundfile as sf

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
RATE = int(os.environ.get('VOICE_RATE', '16000'))
KDIR = os.environ['KOKORO']
CACHE = os.environ.get('CACHE', os.path.join(ROOT, 'source/wav'))
os.makedirs(CACHE, exist_ok=True)
# the per speaker overrides: these speakers are Kyutai clones of a reference recording
KYUTAI = {
    'moheeb': os.path.expanduser(os.environ.get('MOHEEB_WAV', '~/Downloads/moheeb-voice.wav')),
    'luis': os.path.expanduser(os.environ.get('LUIS_WAV', '~/Downloads/luis-voice.wav')),
}
KYUTAI_PY = shlex.split(os.environ.get('KYUTAI_PY', 'uv run --python 3.12 --with pocket-tts python'))

lines, seen = [], set()
for src in ('source/lines.json', 'source/street_lines.json'):
    path = os.path.join(ROOT, src)
    if not os.path.exists(path):
        continue
    for L in json.load(open(path)):
        k = L['speaker'] + '|' + L['text']
        if k not in seen:   # the same words by the same speaker in both files: one take
            seen.add(k); lines.append(L)
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

def trim(a, sr):
    nz = np.where(np.abs(a) > 0.01)[0]
    return a[max(0, nz[0] - int(.03 * sr)): min(len(a), nz[-1] + int(.08 * sr))] if len(nz) else a

def kokoro_tag(L):
    return hashlib.sha1(f"{L['voice']}|{L['speed']}|{L['text']}".encode()).hexdigest()[:20]

# ---- the Kyutai takes, in one batch: the model loads once
# The references go in as their first 20 s, mono at 24 kHz (SMALL PARTS' format). Given a whole 40 s
# stereo recording, the model stops most of Luis's lines a second in.
REF_SECONDS, REF_RATE = 20, 24000
def file_sig(path):
    return hashlib.sha1(open(path, 'rb').read()).hexdigest()[:12] + f'-{REF_SECONDS}s' if os.path.exists(path) else None
def ref_clip(path, out):
    a, sr = sf.read(path, dtype='float32', always_2d=True)
    a = a.mean(axis=1)[: int(REF_SECONDS * sr)]
    m = int(len(a) * REF_RATE / sr)
    sf.write(out, np.interp(np.arange(m) * sr / REF_RATE, np.arange(len(a)), a).astype(np.float32), REF_RATE)
    return out
SIG = {k: file_sig(p) for k, p in KYUTAI.items()}
def kyutai_tag(L):
    return 'ky' + hashlib.sha1(f"kyutai|{L['speaker']}|{SIG[L['speaker']]}|{L['text']}".encode()).hexdigest()[:18]
def speakable(t):
    # what the clone reads: a few lab words spelled out (as SMALL PARTS' tools/voices.mjs does)
    s = re.sub(r'\bLED\b', 'L E D', t)
    s = re.sub(r'\bLEDs\b', 'L E Ds', s)
    s = re.sub(r'\bCNC\b', 'C N C', s)
    return s

def kyutai_line(L):
    return L.get('kyutai') or (L['speaker'] if L['speaker'] in KYUTAI else None)

want = [L for L in lines if kyutai_line(L) and SIG.get(kyutai_line(L)) and not os.path.exists(os.path.join(CACHE, kyutai_tag(L) + '.wav'))]
kyutai_ok = all(SIG.get(k) for k in KYUTAI)
if want:
    work = tempfile.mkdtemp(prefix='kh-kyutai-')
    spec = {'voices': {k: ref_clip(p, os.path.join(work, f'ref_{k}.wav')) for k, p in KYUTAI.items() if SIG.get(k)},
            'jobs': [{'voice': kyutai_line(L), 'text': speakable(L['text']), 'out': os.path.join(work, f'{i}.wav')} for i, L in enumerate(want)]}
    with open(os.path.join(work, 'jobs.json'), 'w') as f:
        json.dump(spec, f)
    print(f"kyutai: {len(want)} takes for {', '.join(sorted({kyutai_line(L) for L in want}))}", flush=True)
    try:
        subprocess.run(KYUTAI_PY + [os.path.join(ROOT, 'tools/voices_kyutai.py'), os.path.join(work, 'jobs.json')], check=True)
        for i, L in enumerate(want):
            a, sr = sf.read(os.path.join(work, f'{i}.wav'), dtype='float32')
            sf.write(os.path.join(CACHE, kyutai_tag(L) + '.wav'), trim(a, sr), sr)
    except (subprocess.CalledProcessError, FileNotFoundError, RuntimeError) as e:
        print(f"kyutai: the generator failed ({e}); those lines use their Kokoro stand ins this time", flush=True)
        kyutai_ok = False
elif not kyutai_ok:
    print(f"kyutai: no reference for {', '.join(k for k in KYUTAI if not SIG.get(k))}; those lines use their Kokoro stand ins", flush=True)

entries, total, stand_ins = [], 0.0, 0
for n, L in enumerate(lines):
    ky = kyutai_line(L)
    path = os.path.join(CACHE, kyutai_tag(L) + '.wav') if ky and SIG.get(ky) else None
    if not path or not os.path.exists(path):
        if ky:
            stand_ins += 1
        path = os.path.join(CACHE, kokoro_tag(L) + '.wav')
        if not os.path.exists(path):
            if kokoro is None:
                from kokoro_onnx import Kokoro
                kokoro = Kokoro(os.path.join(KDIR, 'kokoro-v1.0.onnx'), os.path.join(KDIR, 'voices-v1.0.bin'))
            lang = 'en-gb' if L['voice'].startswith('b') else 'en-us'
            samples, sr = kokoro.create(L['text'], voice=L['voice'], speed=L['speed'], lang=lang)
            sf.write(path, trim(np.asarray(samples, dtype=np.float32), sr), sr)
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
print(f"{len(entries)} lines, {total:.0f} s of speech, {os.path.getsize(os.path.join(ROOT, 'assets/voice/voices.bin')) / 1048576:.1f} MB"
      + (f", {stand_ins} Kyutai lines on their Kokoro stand ins" if stand_ins else ""))
