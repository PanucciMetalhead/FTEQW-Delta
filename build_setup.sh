#!/bin/sh
# Dependency discovery; installation instructions are in documentation/Dependencies.md.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cmake -S "$root" -B "${FTE_BUILD_DIR:-$root/build}" "$@"
