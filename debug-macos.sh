#!/bin/bash

FRESH=false
ASAN=false
PRO=false
DEBUG=false
MACPORTS=false
HOMEBREW=false
STATIC=false
WARNINGS=false
BUILD_TYPE="MinSizeRel"
COMPILER_FLAGS=""
DEBUG_FLAGS=""
PRO_VERSION_FLAG=""
STATIC_FLAG=""
WARNING_FLAGS="-Wall -Wextra"

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
    -m|--macports)
      MACPORTS=true
      shift
      ;;
    -h|--homebrew)
      HOMEBREW=true
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

if [ "$MACPORTS" = true ] && [ "$HOMEBREW" = true ]; then
  echo "Error: -m/--macports and -h/--homebrew options are mutually exclusive."
  exit 1
fi

# remove MacPorts from PATH if we're not using it or we're using Homebrew
if [ "$MACPORTS" = false ] || [ "$HOMEBREW" = true ]; then
  PATH=${PATH/\/opt\/local\/bin:/}
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

if [ "$MACPORTS" = true ]; then
  CLANG=$(which clang)
  CLANGPP=$(which clang++)
  if [ -z "$CLANG" ] || [ -z "$CLANGPP" ]; then
    echo "clang or clang++ not found. Please ensure they are installed and in your PATH."
    exit 1
  fi
  COMPILER_FLAGS="-DCMAKE_C_COMPILER=$CLANG -DCMAKE_CXX_COMPILER=$CLANGPP"
  export CPPFLAGS='-isystem/opt/local/include'
  export LDFLAGS='-L/opt/local/lib'
  echo
  echo "Using clang at $CLANG and clang++ at $CLANGPP from MacPorts"
fi

if [ "$HOMEBREW" = true ]; then
  export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
  export LDFLAGS="-L/opt/homebrew/opt/llvm/lib/c++ -Wl,-rpath,/opt/homebrew/opt/llvm/lib/c++ -L/opt/homebrew/opt/llvm/lib"
  export CPPFLAGS="-I/opt/homebrew/opt/llvm/include"
  
  # Add explicit compiler flags like MacPorts does
  CLANG="/opt/homebrew/opt/llvm/bin/clang"
  CLANGPP="/opt/homebrew/opt/llvm/bin/clang++"
  if [ -z "$CLANG" ] || [ -z "$CLANGPP" ]; then
    echo "Homebrew clang or clang++ not found. Please ensure llvm is installed via Homebrew."
    exit 1
  fi
  COMPILER_FLAGS="$COMPILER_FLAGS -DCMAKE_C_COMPILER=$CLANG -DCMAKE_CXX_COMPILER=$CLANGPP"
  
  echo
  echo "Using clang at $CLANG and clang++ at $CLANGPP from Homebrew"
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
echo "| MacPorts (-m, --macports)      | $(printf "%-5s" $([ "$MACPORTS" = true ] && echo "Yes" || echo "No")) |"
echo "| Homebrew (-h, --homebrew)      | $(printf "%-5s" $([ "$HOMEBREW" = true ] && echo "Yes" || echo "No")) |"
echo "| Static build (-s, --static)    | $(printf "%-5s" $([ "$STATIC" = true ] && echo "Yes" || echo "No")) |"
echo "| Warnings (-w, --warnings)      | $(printf "%-5s" $([ "$WARNINGS" = true ] && echo "Yes" || echo "No")) |"
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
cmake --build . --config "$BUILD_TYPE" --parallel
