#!/bin/bash
# Compiles the plugin's real Alembic writer twice - 1.0 (from git history, commit 9eaf714)
# and the current one (Source/) - against the Alembic library, exports the same synthetic
# grooms with both, compares the files byte for byte and prints the separate-objects layout.
#
# Usage: ALEMBIC_ROOT=/path/to/install Tests/AlembicHarness/run.sh
#   ALEMBIC_ROOT is an install prefix holding include/Alembic, include/Imath, lib/libAlembic.a
#   and lib/libImath*.a (see README.md for the two cmake builds).
set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
SRC="$REPO/Source/GroomExportEditor"
: "${ALEMBIC_ROOT:?set ALEMBIC_ROOT to an Alembic + Imath install prefix}"
BUILD="$HERE/build"

rm -rf "$BUILD"
mkdir -p "$BUILD/v10src" "$BUILD/out"
for f in GroomAlembicWriter.cpp GroomAlembicWriter.h GroomExportSettings.h; do
	git -C "$REPO" show "9eaf714:Source/GroomExportEditor/Private/$f" > "$BUILD/v10src/$f"
done

IMATH_LIB=$(ls "$ALEMBIC_ROOT"/lib/libImath*.a | head -1)
# Alembic headers as system headers: their own warnings are not ours. Every warning in the
# plugin code (shadowed variables included, an error in Unreal) fails the build.
CXX="${CXX:-g++} -std=c++17 -O1 -Wall -Wextra -Wshadow -Wno-unused-parameter -Werror -isystem $ALEMBIC_ROOT/include -isystem $ALEMBIC_ROOT/include/Imath"
LIBS="$ALEMBIC_ROOT/lib/libAlembic.a $IMATH_LIB -lpthread"

$CXX -DHARNESS_V11=0 -I"$HERE/shim" -I"$BUILD/v10src" "$HERE/main.cpp" "$BUILD/v10src/GroomAlembicWriter.cpp" $LIBS -o "$BUILD/harness_v10"
$CXX -DHARNESS_V11=1 -I"$HERE/shim" -I"$SRC/Private" -I"$SRC/Public" "$HERE/main.cpp" "$SRC/Private/GroomAlembicWriter.cpp" "$HERE/physics_stub.cpp" $LIBS -o "$BUILD/harness_current"
$CXX "$HERE/dump.cpp" $LIBS -o "$BUILD/dump"

cd "$BUILD"
status=0
echo "==== 1.0 writer"
./harness_v10 out 2>/dev/null || status=1
echo "==== current writer"
./harness_current out 2>/dev/null || status=1

echo "==== 1.0 vs current, same settings"
for f in mixed exclude mixed_nosplit exclude_noattrs mixed_yup exclude_yup strandwidth_mixed_m noguideattr_mixed; do
	if cmp -s "out/v10_$f.abc" "out/v11_$f.abc"; then
		echo "IDENTICAL  $f"
	else
		echo "DIFFERENT  $f"
		status=1
	fi
done

echo "==== layouts"
./dump out/v11_separate.abc out/v11_exclude.abc out/v11_separate_noattrs.abc

if [ "$status" -ne 0 ]; then echo "FAILED"; else echo "ALL OK"; fi
exit "$status"
