#!/usr/bin/env bash
# Cross-compiles Achievements with llvm-mingw and stages dist/, laid out like the Azimuth folder
# (the release zip is dist/'s contents, for the FH2ML Manager's Install mod or to extract there):
#   dist/mods/achievements/mod.json           what FH2ML reads (its version is CMakeLists.txt's)
#   dist/mods/achievements/Achievements.dll   the mod
#   dist/mods/achievements/README.txt, LICENSE.txt
#
#   $ scripts/build.sh
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
for d in /opt/llvm-mingw/bin /usr/local/llvm-mingw/bin; do [ -d "$d" ] && PATH=$d:$PATH; done
cmake -S "$root" -B "$root/build" -DCMAKE_TOOLCHAIN_FILE="$root/cmake/mingw-w64-x86_64.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build "$root/build" -j "${JOBS:-$(nproc)}"
rm -rf "$root/dist"
pkg="$root/dist/mods/achievements"
mkdir -p "$pkg"
cp "$root/build/mod.json" "$root/build/Achievements.dll" "$pkg/"
cp "$root/scripts/dist-readme.txt" "$pkg/README.txt"
cp "$root/LICENSE" "$pkg/LICENSE.txt"
(cd "$root/dist" && find . -type f -printf '  %P\n')
