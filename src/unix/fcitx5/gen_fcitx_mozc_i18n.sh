#!/bin/sh

objdir="$1"

mkdir -p "$1"

for pofile in po/*.po
do
    msgfmt "$pofile" -o "$1/`basename ${pofile} .po`.mo"
done
