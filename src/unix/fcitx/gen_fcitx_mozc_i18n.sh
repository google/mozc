#!/bin/sh
objdir="$1"

cd po || exit 1

mkdir -p "$1"

for pofile in *.po
do
    msgfmt "$pofile" -o "$1/`basename ${pofile} .po`.mo"
done
