#!/usr/bin/env python3
"""A small TNFS server for the tests (and for serving a folder to the
machine): MOUNT, UMOUNT, OPENDIR, READDIR, CLOSEDIR, OPEN, READ, CLOSE,
STAT, LSEEK, read-only. Usage: tnfsd.py PORT DIRECTORY"""
import os, socket, struct, sys, stat
port, root = int(sys.argv[1]), os.path.abspath(sys.argv[2])
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.bind(("127.0.0.1", port))
sid = 0x1234; files = {}; dirs = {}; nfd = 1
def host(p):
    p = os.path.normpath("/" + p.decode(errors="replace")).lstrip("/")
    return os.path.join(root, p) if p else root
while True:
    d, a = s.recvfrom(2048)
    hdr, cmd, data = d[:3], d[3], d[4:]
    rep = None
    if cmd == 0x00:
        rep = struct.pack("<BHH", 0, 0x0100, 500); hdr = struct.pack("<H", sid) + hdr[2:3]
    elif cmd == 0x01: rep = b"\x00"
    elif cmd == 0x10:
        p = host(data.split(b"\0")[0])
        if os.path.isdir(p): dirs[nfd] = sorted(os.listdir(p)); rep = bytes([0, nfd]); nfd = (nfd % 250) + 1
        else: rep = b"\x02"
    elif cmd == 0x11:
        h = data[0]; l = dirs.get(h)
        if l is None: rep = b"\x09"
        elif l: rep = b"\x00" + l.pop(0).encode() + b"\0"
        else: rep = b"\x21"
    elif cmd == 0x12: dirs.pop(data[0], None); rep = b"\x00"
    elif cmd == 0x29:
        flags, mode = struct.unpack("<HH", data[:4]); p = host(data[4:].split(b"\0")[0])
        if os.path.isfile(p) and flags & 3 == 1: files[nfd] = open(p, "rb"); rep = bytes([0, nfd]); nfd = (nfd % 250) + 1
        else: rep = b"\x02"
    elif cmd == 0x21:
        h, n = data[0], struct.unpack("<H", data[1:3])[0]; f = files.get(h)
        if f is None: rep = b"\x09"
        else:
            b = f.read(min(n, 512)); rep = (b"\x00" + struct.pack("<H", len(b)) + b) if b else b"\x21"
    elif cmd == 0x23:
        f = files.pop(data[0], None); f and f.close(); rep = b"\x00"
    elif cmd == 0x24:
        p = host(data.split(b"\0")[0])
        try:
            st = os.stat(p); m = (0x4000 if stat.S_ISDIR(st.st_mode) else 0x8000) | 0o644
            rep = struct.pack("<BHHHIIII", 0, m, 0, 0, st.st_size & 0xFFFFFFFF, int(st.st_atime), int(st.st_mtime), int(st.st_ctime)) + b"\0\0"
        except OSError: rep = b"\x02"
    elif cmd == 0x25:
        h, t, off = data[0], data[1], struct.unpack("<i", data[2:6])[0]; f = files.get(h)
        if f is None: rep = b"\x09"
        else: f.seek(off, t); rep = b"\x00" + struct.pack("<I", f.tell())
    else: rep = b"\x16"
    s.sendto(hdr + bytes([cmd]) + rep, a)
