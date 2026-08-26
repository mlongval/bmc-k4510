---
name: Report or request
about: One block, filled in. It goes to a coding session as written.
title: ''
labels: ''
assignees: ''
---

<!-- Generated from doc/guide/chapters/88-issues.tex by doc/guide/mkissue.py.
     Edit the handbook page, not this file: the next build overwrites it. -->

An issue filed here is handed to an AI coding session to work on, so this
template is shaped like a prompt rather than like a form. Fill in what you
can and delete what does not apply. Nothing is required except the last two
lines of the block.

**Three things first**, each of which removes a day:

- Type `DUMP` while it is wrong.
- Try it again with `k4510 --no-startup.bat`.
- Look it up in this book.

```
Machine:  BMC-K4510, desktop  |  Raspberry Pi 3B+
Version:  from this book's cover, or the release you downloaded
          (INFO -v names the K/OS and emulator generation, coarser)
Where:    shell | EhBASIC | BBC BASIC | CP/M | Forth | Pascal |
          VI or EDIT | the F7 menu | this handbook

What I typed, exactly:

    LOAD "DEMO.BAS"
    RUN

What happened:
What I expected:
Why I expected it:  handbook p.NN | a real C64 or Beeb does it |
                    it worked before

Still wrong with  k4510 --no-startup.bat ?   yes | no | did not try
Dump:  dumps/dump-017.txt   (type DUMP, then attach that file)
```

**Before you attach a dump:** it carries the shell log -- every command line
from that session, and file names from your own disk with them. It is plain
text. Open it and have a look first.

The handbook page this came from ("Filing an Issue", near the back) says the
same at greater length, and says why each line is there.
