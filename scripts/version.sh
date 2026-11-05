#!/bin/sh
# Single source of the version: build = 10000 + commit count, name = git describe.
cd "$(dirname "$0")/.." || exit 1
if [ "$(git rev-parse --is-shallow-repository 2>/dev/null)" = "true" ]; then
    echo "version.sh: shallow clone, build number would be wrong" >&2
    code=0
else
    n=$(git rev-list --count HEAD 2>/dev/null) && code=$((10000 + n)) || code=0
fi
name=$(git describe --tags --always --dirty --match 'v[0-9]*' 2>/dev/null) || name=unknown
case "${1:-both}" in
    code) echo "$code" ;;
    name) echo "$name" ;;
    *) echo "$code $name" ;;
esac
