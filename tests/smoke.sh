#!/usr/bin/env sh
set -eu

known_50='3.14159265358979323846264338327950288419716939937510'

actual_50="$(./picalc 50)"
if [ "$actual_50" != "$known_50" ]; then
    printf '50 digit mismatch\nexpected: %s\nactual:   %s\n' "$known_50" "$actual_50" >&2
    exit 1
fi

tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT

./picalc 25 -o "$tmp"
actual_file="$(cat "$tmp")"
if [ "$actual_file" != '3.1415926535897932384626433' ]; then
    printf 'file output mismatch\nactual: %s\n' "$actual_file" >&2
    exit 1
fi

printf 'ok\n'
