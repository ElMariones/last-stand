#!/usr/bin/env bash
# Enforces GDD 14.2: the simulation layer must not depend on rendering.
# This is what makes headless benchmarking and deterministic replay possible.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
violations=0

for dir in sim math core ai gameplay persist fx; do
    target="$root/src/$dir"
    [ -d "$target" ] || continue
    if grep -rn -E '#include[[:space:]]*[<"](raylib\.h|render/|ui/)' "$target"; then
        echo "LAYERING VIOLATION in src/$dir (see above)" >&2
        violations=1
    fi
done

if [ "$violations" -ne 0 ]; then
    echo "" >&2
    echo "src/{sim,math,core,ai,gameplay,persist,fx} must never include raylib, render/ or ui/." >&2
    echo "See GDD 14.2. Move the rendering concern into src/render/." >&2
    exit 1
fi

echo "layering OK"
