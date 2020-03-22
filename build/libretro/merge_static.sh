#!/bin/sh

AR="$1"
shift
OUTPUT="$1"
shift

(echo create "$OUTPUT";
 for x in "$@"; do
     echo addlib "$x"
 done
 echo save
 echo end
) | "$AR" -M
