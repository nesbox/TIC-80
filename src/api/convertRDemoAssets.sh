#!/bin/env bash
# FILE := $1
SRC2TIC() {
  local SRC2TIC="${HOME}/src/c/TIC-80/fakeroot/bin/prj2cart"
  command $SRC2TIC $1 ${BLDA_DIR}/$(basename -s .r $1).tic
}

# FILE := $1
TIC2DAT() {
  local TIC2DAT="${HOME}/src/c/TIC-80/fakeroot/bin/bin2txt"
  command $TIC2DAT ${1} ${BLDA_DIR}/$(basename $1).dat -z
  rm ${BLDA_DIR}/$(basename $1)
  # mv ${BLDA_DIR}/$(basename $1) -t ~/src/c/build/
}

main() {
  set --
  set +x

  # NOTE: this could be "improved" to take command-line arguments for the language
  # name (prefixing "demo" or "mark") and the file suffix (including the dot, so
  # ".r"). Relative paths only valid from where r.org defined (src/api/r.org)
  local T80_DIR="${HOME}/src/c/TIC-80"
  local R_DEMO="${T80_DIR}/demos/rdemo.r"
  local R_MARK="${T80_DIR}/demos/bunny/rmark.r"
  local BLDA_DIR="${T80_DIR}/build/assets"

  for SRC in $R_DEMO $R_MARK; do
    SRC2TIC $SRC
    TIC2DAT ${BLDA_DIR}/$(basename -s .r $SRC).tic
  done
}

main
