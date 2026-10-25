#!/usr/bin/env python3
"""Generate the GitHub Pages index for the published documentation."""

from pathlib import Path
import re
import sys


def main() -> None:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    versions = []
    for path in root.iterdir():
        match = re.fullmatch(r"v(\d+)\.(\d+)\.(\d+)", path.name)
        if path.is_dir() and match:
            versions.append((tuple(map(int, match.groups())), path.name))
    versions.sort(reverse=True)

    items = "\n".join(
        f'      <li><a href="{name}/">{name}</a></li>' for _, name in versions
    )
    (root / "index.html").write_text(
        f"""<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>self-tuning-lam-annealing-cpp documentation</title>
  </head>
  <body>
    <h1>self-tuning-lam-annealing-cpp</h1>
    <p><a href="latest/">latest</a></p>
    <ul>
{items}
    </ul>
  </body>
</html>
""",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
