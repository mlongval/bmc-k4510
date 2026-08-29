# The two names

**Decision, 2026-08-29 (Doc).** The project has been using one name for
two different things. From here:

- **K4510** is the machine — the architecture and its software. The
  45GS02, VICKY, SHEILA, the four SIDs, K/OS, the ROM, the handbook,
  every `.prg`. It is what you are using whichever way you run it, and
  on the desktop under SDL2 it is simply *the K4510*.
- **BMC-K4510** is the bare-metal Raspberry Pi appliance. The SD card,
  Circle, the four cores, HDMI and the headphone jack, no operating
  system between the machine and the board.

One is the computer. The other is the computer as a piece of hardware
you switch on.

## This is not a rename

`docs/K4510-Design.md` settled the etymology on 2026-08-21, and it
already said this:

> *BMC* is for Randy Rossi's BMC64 and its family on the Pi 3B and
> earlier boards — the platform this is built on.

*BMC* was never part of the machine's identity. It names the platform,
exactly the way it does in BMC64. `BMC-K4510` has always parsed as "the
K4510 on the bare-metal Pi"; the project just used it for everything
because for a while there was only one way to run the thing. The *K*
(Kawari) and the *4510* (the C65's 4510 by way of the MEGA65's 45GS02,
and the C64's 6510) are the machine's own, and they stay in both names.

So nothing is being renamed. A name that covered too much is being
returned to what it says.

## The rule, and the test

> **Would the sentence still be true if you unplugged the Pi and opened
> the desktop build?** If yes, it is the **K4510**. If it is only true
> of a board with an SD card in it, it is the **BMC-K4510**.

That test decides essentially every case. Some worked examples:

| Statement | Name |
|---|---|
| 256 MB of RAM, banked through the 45GS02's 28-bit space | K4510 |
| `MODE 2` is 320x240, 40x30 text | K4510 |
| K/OS boots to a shell at `/]` | K4510 |
| Four reSID chips, `HUSH` silences them | K4510 |
| Boots in about two seconds from cold, with no OS underneath | BMC-K4510 |
| Core 1 runs the emulator, core 2 presentation, core 3 the Tube | BMC-K4510 |
| Write the card with `pi/make-sd.sh` | BMC-K4510 |
| Holds 15 MHz at 60 fps; the desktop holds 40.5 | both, separately |

The last row is the interesting one. Performance is a property of the
*host*, not the machine — which is why `core/calib.c` measures it rather
than looking it up. Say "the BMC-K4510 holds 15 MHz", never "the K4510
runs at 15 MHz".

## What each name owns

| | K4510 | BMC-K4510 |
|---|---|---|
| Code | `rom/`, `core/`, `sdl/`, `basic/`, `forth/`, `demo/`, `mon/`, `cpm/`, `tube/`, `fs/` | `pi/` |
| Docs | the handbook, `VICKY-SPEC.md`, `K4510-Design.md`, Appendix A | `pi/README-SD.txt`, the SD-card sections |
| Anything the guest can see | banner, status bar, `INFO`, the settings file | — |
| Anything you hold | — | the card, the board, the cables |

The dividing line falls almost exactly on `pi/`. Seven mentions of
`BMC-K4510` live there and belong there; the other ~150 across the tree
are the machine and should read `K4510`.

## What this changed

**Done, 2026-08-29, and shipped as alpha-0.4 'Imprint'.** By area:

- **The guest, and this is the important one.** The ROM banner says
  `BMC-K4510 -- A FANTASY 8/16-bit COMPUTER`, the status bar says
  `BMC-K4510  K/OS`, and `INFO` says `K/OS ... (the BMC-K4510 operating
  system)`. **The same ROM bytes boot on both hosts**, so the guest
  cannot honestly claim to be the Pi appliance. All three become
  `K4510`. Saves four bytes of rodata and costs nothing.
- **Shared host chrome** — the SDL window title, the F7 menu heading,
  the settings-file header, the dump header in `core/io.c`: `K4510`.
- **File-header comments** across `demo/`, `pascal/`, `basic/`,
  `forth/`, `tube/`, `cpm/`, `mon/`, `test/`, `tools/`: `K4510`. This is
  the bulk of the count and the least urgent part of it.
- **`pi/`** keeps `BMC-K4510` throughout: `kernel.cpp`'s boot line,
  `README-SD.txt`, `make-sd.sh`, `Makefile`, `config.txt`.
- **`README.md`** opens on the machine now, with a *Two names, one
  machine* paragraph, and its Pi section is titled for the appliance.
- **The handbook** is *The K4510 User's and Programmer's Guide*; the
  cover reads `K4510`. A new §1.3, *Two names, one machine*, carries
  this file's argument for readers, and the thanks page now says out
  loud that Randy Rossi's initials are in the appliance's name. Every
  figure was recaptured — the banner is in a dozen of them.
- **The issue form** distinguishes the two, at its source
  (`doc/guide/issue-form.txt`; `.github/ISSUE_TEMPLATE/report.md` is
  generated from the book and must never be edited directly).
- **`install-sd.sh` keeps `BMC-K4510`** — it writes cards, so it belongs
  to the appliance. It is the one root-level file that does.

## What does not change

- **The repository stays `github.com/mlongval/bmc-k4510`.** It is the
  project's name and it is in every clone, release URL and bookmark that
  exists. Renaming it would break those to gain tidiness.
- **`K/OS`**, the register names, `$D5xx`, the file layout, the release
  naming (`alpha-N`) — untouched. This is about prose and chrome.
- **The desktop build gets no new name of its own.** It is the K4510.
  There is no silicon for it to be an emulation *of*: for a fantasy
  machine, the emulator is the machine.

## The guest names itself correctly — done

This was written up as an open question, on the assumption that telling
the guest which host it was on would cost a new mechanism. It does not:
**`$D522` already carries it**, and `INFO` has been reading it since the
Pi port (`rom/kernal.c:770`). So the banner branches on the same byte —
it prints `BMC-K4510 -- A FANTASY 8/16-bit COMPUTER` from a card and
`K4510 -- ...` on a desktop, from one ROM image. `INFO`'s system line
says `BMC-K4510: bare metal on a Raspberry Pi 3B+` or `K4510 on a
desktop`.

Seventeen bytes of ROM1A. The machine now tells the truth about which of
its two selves you are looking at, which is the whole point of this file.

The **status bar** deliberately does not branch: it says `K4510  K/OS`
either way. It is eighty columns wide at most and it names the machine,
not the box.
