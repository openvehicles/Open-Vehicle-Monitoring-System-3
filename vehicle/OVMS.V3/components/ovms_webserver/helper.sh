#!/usr/bin/env sh

HEADER_FILENAME="$1"
shift

true > "$HEADER_FILENAME"

while [ $# -gt 0 ]
do
  DEFINE="$1"
  FILENAME="$2"
  shift 2
  MTIME=$(perl -e "print +(stat \"$FILENAME\")[9]")
  echo "#define $DEFINE $MTIME" >> "$HEADER_FILENAME"
done
