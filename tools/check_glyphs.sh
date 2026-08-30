#!/usr/bin/env bash
# raylib's default font covers Latin-1 and nothing else, so any character
# above U+00FF in a user-facing string renders as a hollow box. That is
# invisible in code review and obvious on screen, which is the worst
# combination — so it fails the build instead.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"

if ! python3 - "$root" <<'PY'
import glob, os, re, sys

root = sys.argv[1]
violations = []
for pattern in ("src/**/*.cpp", "src/**/*.h"):
    for path in glob.glob(os.path.join(root, pattern), recursive=True):
        with open(path, encoding="utf-8") as handle:
            for number, line in enumerate(handle, 1):
                for literal in re.findall(r'"((?:[^"\\]|\\.)*)"', line):
                    for char in literal:
                        if ord(char) > 0xFF:
                            violations.append(
                                (os.path.relpath(path, root), number, char, literal))

for path, number, char, literal in violations:
    print(f"{path}:{number}: U+{ord(char):04X} {char!r} in \"{literal}\"")
sys.exit(1 if violations else 0)
PY
then
    echo "" >&2
    echo "Characters above U+00FF do not exist in raylib's default font." >&2
    echo "Use ASCII, or the Latin-1 middot, in anything drawn with DrawText." >&2
    exit 1
fi

echo "glyphs OK"
