#!/bin/sh
# Local-only desktop build. No uploads or release publication.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_dir=${FTE_BUILD_DIR:-"$root/build"}
build_type=${FTE_BUILD_TYPE:-Release}
cmake -S "$root" -B "$build_dir" -DCMAKE_BUILD_TYPE="$build_type" "$@"
cmake --build "$build_dir" --config "$build_type" --parallel
