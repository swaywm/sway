#!/bin/sh -eu

var=${1:-data}
hex="$(od -A n -t x1 -v)"

echo "static const char $var[] = {"
for byte in $hex; do
    echo "	0x$byte,"
done
echo "	0x00,"
echo "};"
