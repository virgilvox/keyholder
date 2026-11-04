#!/usr/bin/env python3
"""Contact sheets of screenshots: sheet.py out_prefix cols rows w h img..."""
import subprocess, sys
pre, cols, rows, w, h = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
imgs = sys.argv[6:]
n = cols * rows
for k in range(0, len(imgs), n):
    part = imgs[k:k + n]
    args = ['ffmpeg', '-loglevel', 'error', '-y']
    for p in part: args += ['-i', p]
    layout = '|'.join(f'{(i % cols) * w}_{(i // cols) * h}' for i in range(len(part)))
    f = ''.join(f'[{i}]scale={w}:{h}[v{i}];' for i in range(len(part))) + ''.join(f'[v{i}]' for i in range(len(part)))
    f += f'xstack=inputs={len(part)}:layout={layout}:fill=black'
    subprocess.run(args + ['-filter_complex', f, f'{pre}{k // n}.png'], check=True)
