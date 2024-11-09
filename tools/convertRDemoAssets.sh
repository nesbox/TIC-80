#!/bin/env bash
# FILE := $1
PRJ2CART() {
  local SRC2TIC="${HOME}/src/c/build/bin/prj2cart"
  command $SRC2TIC $1 ${BLDA_DIR}/$(basename -s .r $1).tic
}

# FILE := $1
BIN2TXT() {
  local TIC2DAT="${HOME}/src/c/build/bin/bin2txt"
  command $TIC2DAT ${1} ${BLDA_DIR}/$(basename $1).dat -z
  rm ${BLDA_DIR}/$(basename $1)
  # mv ${BLDA_DIR}/$(basename $1) -t ~/src/c/build/
}

main() {
  # NOTE: this could be "improved" to take command-line arguments for the language
  # name (prefixing "demo" or "mark") and the file suffix (including the dot, so
  # ".r"). Relative paths only valid from where r.org defined (src/api/r.org)
  local R_DEMO="${1}/demos/rdemo.r"
  local R_MARK="${1}/demos/bunny/rmark.r"
  local BLDA_DIR="${1}/build/assets"
  local FIND_PATH="${1}/demos"

  for SRC in $(find ${FIND_PATH} -maxdepth 1 -type f); do
    PRJ2CART $SRC
    BIN2TXT ${BLDA_DIR}/$(basename -s .r $SRC).tic
  done
}

# convertDemosAndBenchmarks.sh TIC-80
main $1
