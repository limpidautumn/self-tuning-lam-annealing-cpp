#!/usr/bin/env python3
"""Doxygen input filter: preserve Markdown math for KaTeX."""

import pathlib
import re
import sys


def escape_tex(tex: str) -> str:
    # Doxygen consumes one backslash. Double it so HTML keeps '\'.
    return tex.replace("\\", "\\\\")


def convert(text: str) -> str:
    saved: list[str] = []

    def keep(match: re.Match[str]) -> str:
        saved.append(match.group(0))
        return f"\x00{len(saved) - 1}\x00"

    # Fenced code blocks (``` or ~~~) are not math.
    text = re.sub(
        r"(?ms)^[ \t]*(`{3,}|~{3,}).*?^[ \t]*\1[ \t]*$",
        keep,
        text,
    )
    # Inline code spans are not math.
    text = re.sub(r"(`+)(.*?)\1", keep, text, flags=re.S)

    # $$...$$ -> \[...\] in the final HTML.
    text = re.sub(
        r"\$\$(.+?)\$\$",
        lambda m: "\\\\[" + escape_tex(m.group(1)) + "\\\\]",
        text,
        flags=re.S,
    )
    # $...$ -> \(...\) in the final HTML.
    text = re.sub(
        r"(?<!\\)\$(?!\$)([^$\n]+?)(?<!\\)\$",
        lambda m: "\\\\(" + escape_tex(m.group(1)) + "\\\\)",
        text,
    )

    for i, value in enumerate(saved):
        text = text.replace(f"\x00{i}\x00", value)
    return text


def main() -> None:
    text = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
    print(convert(text), end="")


if __name__ == "__main__":
    main()
