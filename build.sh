#!/bin/sh
# Build AgeFix.dylib with Theos. Output: ./AgeFix.dylib
set -e
cd "$(dirname "$0")"

export THEOS="${THEOS:-$HOME/theos}"
[ -d "$THEOS" ] || { echo "Theos not found at $THEOS (set THEOS=...)" >&2; exit 1; }

make FINALPACKAGE=1
cp .theos/obj/AgeFix.dylib .
echo "==> $(pwd)/AgeFix.dylib"
