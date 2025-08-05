#!/bin/bash

FRESH=false
ASAN=false
PRO=false
DEBUG=false
STATIC=false
WARNINGS=false
USE_CLANG=false
BUILD_TYPE="MinSizeRel"
COMPILER_FLAGS=""
DEBUG_FLAGS=""
PRO_VERSION_FLAG=""
STATIC_FLAG=""
WARNING_FLAGS="-Wall -Wextra"

THREADS=$(nproc)

while (( "$#" )); do
  case "$1" in
    -f|--fresh)
      FRESH=true
      shift
      ;;
    -a|--asan)
      ASAN=true
      shift
      ;;
    -p|--pro)
      PRO=true
      shift
      ;;
    -d|--debug)
      DEBUG=true
      shift
      ;;
    -s|--static)
      STATIC=true
      shift
      ;;
    -w|--warnings)
      WARNINGS=true
      shift
      ;;
    -c|--clang)
      USE_CLANG=true
      shift
      ;;
    -j|--jobs)
      THREADS="$2"
      shift 2
      ;;
    --)
      shift
      break
      ;;
    -*)
      echo "Unknown option: $1" >&2
      exit 1
      ;;
    *)
      echo "Invalid argument: $1" >&2
      exit 1
      ;;
  esac
done

if [ "$USE_CLANG" = true ]; then
  CLANG=$(which clang)
  CLANGPP=$(which clang++)
  if [ -z "$CLANG" ] || [ -z "$CLANGPP" ]; then
    echo "clang or clang++ not found. Please ensure they are installed and in your PATH."
    exit 1
  fi
  COMPILER_FLAGS="-DCMAKE_C_COMPILER=$CLANG -DCMAKE_CXX_COMPILER=$CLANGPP"
  echo "Using clang at $CLANG and clang++ at $CLANGPP"
else
  GCC=$(which gcc)
  GPP=$(which g++)
  if [ -z "$GCC" ] || [ -z "$GPP" ]; then
    echo "gcc or g++ not found. Please ensure they are installed and in your PATH."
    exit 1
  fi
  COMPILER_FLAGS="-DCMAKE_C_COMPILER=$GCC -DCMAKE_CXX_COMPILER=$GPP"
  echo "Using gcc at $GCC and g++ at $GPP"
fi

if [ "$FRESH" = true ]; then
  rm -rf .cache CMakeCache.txt CMakeFiles/ cmake_install.cmake
  rm -rf build && git restore 'build/*'
fi

if [ "$ASAN" = true ]; then
  DEBUG=true
  DEBUG_FLAGS="-DBUILD_NO_OPTIMIZATION=On -DBUILD_ASAN_DEBUG=On"
fi

if [ "$PRO" = true ]; then
  PRO_VERSION_FLAG="-DBUILD_PRO=On"
fi

if [ "$DEBUG" = true ]; then
  BUILD_TYPE="Debug"
fi

if [ "$STATIC" = true ]; then
  STATIC_FLAG="-DBUILD_STATIC=On"
fi

if [ "$WARNINGS" = true ]; then
  COMPILER_FLAGS="$COMPILER_FLAGS -DCMAKE_C_FLAGS=$WARNING_FLAGS -DCMAKE_CXX_FLAGS=$WARNING_FLAGS"
fi

echo
echo "+--------------------------------+-------+"
echo "|         Build Options          | Value |"
echo "+--------------------------------+-------+"
echo "| Fresh build (-f, --fresh)      | $(printf "%-5s" $([ "$FRESH" = true ] && echo "Yes" || echo "No")) |"
echo "| Address Sanitizer (-a, --asan) | $(printf "%-5s" $([ "$ASAN" = true ] && echo "Yes" || echo "No")) |"
echo "| Pro version (-p, --pro)        | $(printf "%-5s" $([ "$PRO" = true ] && echo "Yes" || echo "No")) |"
echo "| Debug build (-d, --debug)      | $(printf "%-5s" $([ "$DEBUG" = true ] && echo "Yes" || echo "No")) |"
echo "| Static build (-s, --static)    | $(printf "%-5s" $([ "$STATIC" = true ] && echo "Yes" || echo "No")) |"
echo "| Warnings (-w, --warnings)      | $(printf "%-5s" $([ "$WARNINGS" = true ] && echo "Yes" || echo "No")) |"
echo "| Use Clang (-c, --clang)        | $(printf "%-5s" $([ "$USE_CLANG" = true ] && echo "Yes" || echo "No")) |"
echo "| Parallel jobs (-j, --jobs)     | $(printf "%-5s" "$THREADS") |"
echo "+--------------------------------+-------+"
echo

cd ./build || exit

cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      $PRO_VERSION_FLAG \
      -DBUILD_SDLGPU=On \
      $DEBUG_FLAGS \
      $COMPILER_FLAGS \
      $STATIC_FLAG \
      -DBUILD_WITH_ALL=On \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=On \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
      .. && \
cmake --build . --config "$BUILD_TYPE" --parallel "$THREADS"
