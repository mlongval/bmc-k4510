# Altered source notice (RunCPM, MIT -- see LICENSE-RunCPM.txt)

`src/` is RunCPM as vendored (VENDORED-FROM.txt), with these [BMC-K4510]
changes for the in-process Tube build (`-DK4510_TUBE -Dmain=tube_cpm_main`,
used by the Pi kernel and by the desktop `make tubetest`; the desktop's
`cpm/runcpm` on a pty is built from the unmodified POSIX abstraction):

- src/main.c: selects abstraction_k4510.h under K4510_TUBE, and lands
  the machine's kill (setjmp) so *stopping the Tube* returns from main.
- src/abstraction_k4510.h: generated from abstraction_posix.h by the
  project's patch_cpm.py -- identical except: no termios/poll/glob/
  system; console on the Tube rings (core/tube_cp.h); files through the
  co-processor's path layer with FILEBASE ""; the directory search walks
  opendir() (sorted, as glob sorted); millis() is the Tube clock.
