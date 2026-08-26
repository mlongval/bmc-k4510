#!/usr/bin/env python3
"""Generate the repository's issue template from the handbook page.

The handbook has a page -- "Filing an Issue" -- whose argument is that an
issue filed here is going to be handed to a coding session as written, so
it should be shaped like a prompt rather than like a form. GitHub needs
the same block, and two copies of a thing like that drift within a month.

So there is one copy, and it is the book's. This lifts the form and the
three checks off that page and writes .github/ISSUE_TEMPLATE/report.md.
Editing the chapter changes both; editing the generated file changes
nothing, because the next build overwrites it.

Run by make-guide.sh. Output: .github/ISSUE_TEMPLATE/report.md (never edit).
"""
import re, sys, pathlib

CHAPTER = "chapters/88-issues.tex"
OUT = ".github/ISSUE_TEMPLATE/report.md"

FRONT = """---
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

"""

TAIL = """
**Before you attach a dump:** it carries the shell log -- every command line
from that session, and file names from your own disk with them. It is plain
text. Open it and have a look first.

The handbook page this came from ("Filing an Issue", near the back) says the
same at greater length, and says why each line is there.
"""


def balanced(text, start):
    """The contents of a brace group that may hold brace groups of its own."""
    depth, i = 1, start
    while i < len(text) and depth:
        if text[i] == "{":   depth += 1
        elif text[i] == "}": depth -= 1
        i += 1
    return text[start:i - 1]


def unlatex(t):
    """Enough LaTeX to render one sentence of the book's prose as Markdown."""
    t = t.replace("{-}{-}", "--")          # before any {...} is matched
    t = re.sub(r"\\verb\|([^|]*)\|", r"`\1`", t)
    t = re.sub(r"\\pth\{([^}]*)\}", r"`\1`", t)
    t = re.sub(r"\\texttt\{([^}]*)\}", r"`\1`", t)
    t = re.sub(r"\\emph\{([^}]*)\}", r"*\1*", t)
    t = t.replace("---", "--").replace("``", '"').replace("''", '"')
    return re.sub(r"\s+", " ", t).strip()


def main():
    here = pathlib.Path(__file__).parent
    root = here.resolve().parents[1]
    src = (here / CHAPTER).read_text()

    # the three leads of "Three things first" -- the bold sentence of each
    section = src[src.index("\\section{Three things first}"):
                  src.index("\\section{The report}")]
    checks = [balanced(section, m.end()) for m in re.finditer(r"\\textbf\{", section)]
    if len(checks) != 3:
        print("mkissue: expected 3 checks on the page, found %d" % len(checks),
              file=sys.stderr)
        return 1

    m = re.search(r"\\begin\{form\}\n(.*?)\\end\{form\}", src, re.S)
    if not m:
        print("mkissue: no form block on the page", file=sys.stderr)
        return 1

    out = [FRONT]
    out += ["- %s\n" % unlatex(c) for c in checks]
    out += ["\n```\n", m.group(1), "```\n", TAIL]
    dest = root / OUT
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text("".join(out))
    print("issue template: %s" % OUT)
    return 0

sys.exit(main())
