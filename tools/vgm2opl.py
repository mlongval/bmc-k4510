#!/usr/bin/env python3
"""vgm2opl -- turn VGM/VGZ logs into the K4510's own OPL2 stream (.OPL).

The machine has a YM3812 at $D480 running at 3579545 Hz, the AdLib's crystal,
which is exactly what a VGM file assumes -- so an OPL2 log plays here at the
right pitch with no resampling.  What it cannot do is parse gzip, or seek
around a VGM's dozens of command types, on a 6502.  So the host does the
tedious half and leaves a stream the guest can walk with a switch statement.

  K4OP, the output format
  -----------------------
    0..3    "K4OP"
    4       version (1)
    5       flags: bit0 the tune loops
    6..7    frames per second the stream was quantised at (60)
    8..11   loop point, a byte offset into the data (0 if it does not loop)
    12..43  title, NUL padded, from the VGM's GD3 tag
    44..    the data:
              01 rr dd   write OPL2 register rr = dd
              02 nn      wait nn frames (1..255)
              00         end

The 60 Hz quantisation is not a compromise for this machine, it is the truth
about it: the guest can only act once a frame.  Tunes logged at the AdLib's
usual ~70 Hz are resampled onto frames, which is inaudible for register
writes but does mean the stream is not sample-accurate.

WHAT IT REFUSES.  OPL3 (YMF262) files are skipped, not downmixed.  OPL3 has a
second register bank and four-operator modes that a YM3812 has nowhere to put;
taking bank 0 alone would play something, but it would be a different piece of
music, quietly wrong.  Better to say so.  OPL1 (YM3526) is accepted: an OPL2
is an OPL1 with waveform select added.

  tools/vgm2opl.py IN.vgz [OUT.OPL]
  tools/vgm2opl.py --dir SRCDIR OUTDIR      convert a tree, report what it skipped
"""
import gzip, os, struct, sys

FPS = 60
SAMPLES_PER_FRAME = 44100.0 / FPS      # VGM counts time in 44.1 kHz samples


def read_vgm(path):
    op = gzip.open if path.lower().endswith(".vgz") else open
    with op(path, "rb") as f:
        return f.read()


def gd3_title(b, off):
    """The GD3 tag's first field is the track name, UTF-16LE."""
    if off == 0 or off + 12 > len(b):
        return ""
    if b[off:off + 4] != b"Gd3 ":
        return ""
    n = struct.unpack_from("<I", b, off + 8)[0]
    data = b[off + 12: off + 12 + n]
    try:
        fields = data.decode("utf-16-le", "replace").split("\x00")
    except Exception:
        return ""
    return fields[0] if fields else ""


def convert(b):
    """-> (bytes, title, nframes, looped) or raises ValueError."""
    if b[:4] != b"Vgm ":
        raise ValueError("not a VGM file")
    g = lambda o: struct.unpack_from("<I", b, o)[0] if o + 4 <= len(b) else 0
    ver = g(0x08)
    ym3812, ym3526, ymf262 = g(0x50), g(0x54), g(0x5C)
    if ymf262:
        raise ValueError("OPL3 (YMF262): this machine has a YM3812")
    if not ym3812 and not ym3526:
        raise ValueError("no OPL2/OPL1 data")

    data_off = 0x40
    if ver >= 0x150:
        rel = g(0x34)
        if rel:
            data_off = 0x34 + rel
    loop_off_vgm = g(0x1C)
    loop_abs = (0x1C + loop_off_vgm) if loop_off_vgm else 0
    title = gd3_title(b, 0x14 + g(0x14) if g(0x14) else 0)

    out = bytearray()
    pos = data_off
    pending = 0.0            # samples waited but not yet turned into frames
    loop_out = 0
    n_frames = 0

    def flush_wait():
        nonlocal pending, n_frames
        frames = int(pending // SAMPLES_PER_FRAME)
        if frames <= 0:
            return
        pending -= frames * SAMPLES_PER_FRAME
        n_frames += frames
        while frames > 0:
            n = min(frames, 255)
            out.append(0x02); out.append(n)
            frames -= n

    while pos < len(b):
        if loop_abs and pos >= loop_abs and loop_out == 0:
            flush_wait()
            loop_out = len(out)
        c = b[pos]
        if c == 0x5A or c == 0x5B:            # YM3812 / YM3526 port 0
            if pos + 2 >= len(b): break
            flush_wait()
            out.append(0x01); out.append(b[pos + 1]); out.append(b[pos + 2])
            pos += 3
        elif c == 0x61:
            if pos + 2 >= len(b): break
            pending += struct.unpack_from("<H", b, pos + 1)[0]; pos += 3
        elif c == 0x62:
            pending += 735; pos += 1
        elif c == 0x63:
            pending += 882; pos += 1
        elif c == 0x66:
            break
        elif 0x70 <= c <= 0x7F:
            pending += (c & 0x0F) + 1; pos += 1
        # everything else is another chip's command: step over it by its length
        elif c in (0x4F, 0x50):
            pos += 2
        elif 0x51 <= c <= 0x5F:
            pos += 3
        elif c == 0x67:                        # data block
            if pos + 6 >= len(b): break
            sz = struct.unpack_from("<I", b, pos + 3)[0]
            pos += 7 + sz
        elif c == 0x68:
            pos += 12
        elif 0x80 <= c <= 0x8F:
            pos += 1
        elif c in (0xE0,):
            pos += 5
        elif c in (0x90, 0x91, 0x92, 0x93, 0x94, 0x95):
            pos += {0x90: 5, 0x91: 5, 0x92: 6, 0x93: 11, 0x94: 2, 0x95: 5}[c]
        elif 0xA0 <= c <= 0xBF:
            pos += 3
        elif 0xC0 <= c <= 0xDF:
            pos += 4
        else:
            pos += 1
    flush_wait()
    out.append(0x00)

    hdr = bytearray(44)
    hdr[0:4] = b"K4OP"
    hdr[4] = 1
    hdr[5] = 1 if loop_out else 0
    struct.pack_into("<H", hdr, 6, FPS)
    struct.pack_into("<I", hdr, 8, loop_out)
    t = title.encode("ascii", "replace")[:31]
    hdr[12:12 + len(t)] = t
    return bytes(hdr) + bytes(out), title, n_frames, bool(loop_out)


def one(src, dst):
    body, title, frames, looped = convert(read_vgm(src))
    with open(dst, "wb") as f:
        f.write(body)
    return len(body), title, frames, looped


def main():
    a = sys.argv[1:]
    if not a:
        print(__doc__); return 2
    if a[0] == "--dir":
        srcdir, outdir = a[1], a[2]
        os.makedirs(outdir, exist_ok=True)
        ok = skipped = 0; total = 0; reasons = {}
        for dp, _, fs in os.walk(srcdir):
            for fn in sorted(fs):
                if not fn.lower().endswith((".vgz", ".vgm")):
                    continue
                src = os.path.join(dp, fn)
                # 8.3-ish, upper case: the machine's filesystem and the card's FAT
                base = "".join(ch for ch in os.path.splitext(fn)[0].upper()
                               if ch.isalnum())[:8] or "TUNE"
                dst = os.path.join(outdir, base + ".OPL")
                n = 1
                while os.path.exists(dst):
                    dst = os.path.join(outdir, (base[:6] + "%02d" % n) + ".OPL"); n += 1
                try:
                    sz, title, frames, looped = one(src, dst)
                    ok += 1; total += sz
                except Exception as e:
                    skipped += 1
                    reasons[str(e)] = reasons.get(str(e), 0) + 1
        print("converted %d, skipped %d, %.1f MB" % (ok, skipped, total / 1048576.0))
        for r, n in sorted(reasons.items(), key=lambda kv: -kv[1]):
            print("  %5d  %s" % (n, r))
        return 0
    src = a[0]
    dst = a[1] if len(a) > 1 else os.path.splitext(src)[0] + ".OPL"
    sz, title, frames, looped = one(src, dst)
    print("%s  %d bytes  %d frames (%.1fs)  %s  %s"
          % (dst, sz, frames, frames / float(FPS), "loops" if looped else "one-shot", title))
    return 0


if __name__ == "__main__":
    sys.exit(main())
