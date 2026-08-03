#!/bin/sh
# tests/build/test_public_symbols.sh
#
# TDD gate for T-HE01 (symbol visibility). After building the shared
# library, compare nm -D output against the allow-list in
# public_symbols.txt. Any symbol exported by the .so that is NOT in
# the allow-list fails the test.
#
# Run from the project root after a Debug build:
#   sh tests/build/test_public_symbols.sh build-c23/libmsgbroker.so

set -u

SO="${1:-build/libmsgbroker.so}"
ALLOW="$(dirname "$0")/public_symbols.txt"

if [ ! -f "$SO" ]; then
    echo "test_public_symbols: SKIPPED ($SO not found, not a shared build)"
    exit 0
fi

# Symbols defined and exported by the shared object, demangled.
exposed=$(nm -D --defined-only "$SO" | awk '$2 ~ /[Tt]/ {print $3}' | sort -u)

# Allow-list as one regex of alternations.
pattern=$(awk 'NR>0 {printf "%s|", $0}' "$ALLOW" | sed 's/|$//')

violations=$(echo "$exposed" | grep -Ev "^($pattern)$" || true)

if [ -n "$violations" ]; then
    echo "test_public_symbols: FAILED"
    echo "Unexpected exported symbols:"
    echo "$violations"
    exit 1
fi

echo "test_public_symbols: PASSED"
