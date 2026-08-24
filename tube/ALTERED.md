# Altered source notice (licence.txt, condition 2)

This directory vendors a subset of Richard T. Russell's BBCSDL
(https://github.com/rtrussell/BBCSDL), specifically the Console Mode
edition ("BBCTTY") interpreter, for use as the BMC-K4510's Tube
co-processor. The name "BBC BASIC" is used by permission of the BBC and
is not transferable to derived works; the K4510 documentation therefore
describes this as "Richard Russell's BBC BASIC interpreter, running on
the Tube co-processor", and claims no naming rights.

Alterations, all marked with [BMC-K4510] comments:
- include/bbccon.h: MAXIMUM_RAM reduced from 4 GB to 256 MB, to match
  the machine the Tube is fitted to.

- src/bbccos.c: OSCLI's unknown-command / *RUN path no longer calls the
  host's system(); it emits `ESC ] K4510 ; <cmd> BEL`, which the K4510
  console executes in the machine's own shell.

Everything else is unmodified from BBCSDL commit as cloned 2026-08-24.
