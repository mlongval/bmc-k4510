#!/usr/bin/env python3
"""Free space per memory area of the system ROM.

Reads rom/k4510.cfg for the area sizes and rom/kernal.map for what
ld65 actually put in each, and prints how much room is left.  ROM2
(the resident code) and BSSR (the static variables) are the two that
run out; print this before and after touching the ROM.
"""
import re, sys, os

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
cfg  = open(os.path.join(root, 'rom/k4510.cfg')).read()
mp   = open(os.path.join(root, 'rom/kernal.map')).read()

# MEMORY { NAME: ... start = $x, size = $y ... };  SEGMENTS { SEG: load = NAME [, run = NAME] };
mem, seg2mem = {}, {}
sect = None
for line in cfg.splitlines():
    line = line.split('#')[0]
    if 'MEMORY' in line and '{' in line: sect = 'mem'; continue
    if 'SEGMENTS' in line and '{' in line: sect = 'seg'; continue
    if 'FEATURES' in line and '{' in line: sect = None; continue
    m = re.match(r'\s*(\w+):\s*(.*)', line)
    if not m or not sect: continue
    name, rest = m.group(1), m.group(2)
    if sect == 'mem':
        sz = re.search(r'size\s*=\s*\$([0-9A-Fa-f]+)', rest)
        if sz: mem[name] = int(sz.group(1), 16)
    else:
        ld = re.search(r'load\s*=\s*(\w+)', rest)
        if ld: seg2mem[name] = ld.group(1)

# "Segment list:"  NAME  Start  End  Size  Align
used = {}
for line in mp.split('Segment list:')[1].splitlines():
    f = line.split()
    if len(f) >= 4 and re.fullmatch(r'[0-9A-Fa-f]{6}', f[1]):
        area = seg2mem.get(f[0])
        if area: used[area] = used.get(area, 0) + int(f[3], 16)

print('%-8s %6s %6s %6s' % ('area', 'size', 'used', 'free'))
bad = 0
for name, size in mem.items():
    u = used.get(name, 0)
    if not u and name not in ('ROM2', 'BSSR', 'ZP'):
        continue
    free = size - u
    if free < 0:
        bad = 1
    print('%-8s %6d %6d %6d%s' % (name, size, u, free, '   <-- OVER' if free < 0 else ''))
sys.exit(bad)
