# necessary as the -rdynamic parameter is not supported
# see for example https://github.com/golang/go/issues/36633

# these must be placed before "project" i think
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

if(MINGW OR CYGWIN OR WIN32)
    set(UTIL_SEARCH_CMD where)
elseif(UNIX OR APPLE)
    set(UTIL_SEARCH_CMD which)
endif()

set(TOOLCHAIN_PREFIX arm-none-eabi-)

execute_process(
  COMMAND ${UTIL_SEARCH_CMD} ${TOOLCHAIN_PREFIX}gcc
  OUTPUT_VARIABLE BINUTILS_PATH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
message("Crosscompiler path is ${BINUTILS_PATH}")
get_filename_component(ARM_TOOLCHAIN_DIR ${BINUTILS_PATH} DIRECTORY)

# removed squirrel language as it doesn't seem to compile under arm. Needs investigation.
#    More investigation: Squirrel builds correctly but the linked give these kinds of error:
#    arm-none-eabi-ld: error: kernel8-32.elf uses VFP register arguments, ../../build/lib/libsquirrel.a(sqapi.cpp.obj) does not
#    Even more investigation: turns out i was only setting CMAKE_C_FLAGS and not CMAKE_CXX_FLAGS. So C++ stuff was building without arm/rpi proper flags, VFP-related-stuff in particular.
#    Now squirrel works.
# Ideally should use the CFLAGS defined in circle build files, not hardwire them here too.
# For RPI2
#set(CMAKE_C_FLAGS " -DMINIZ_NO_TIME -DTIC_BUILD_WITH_FENNEL -DTIC_BUILD_WITH_MOON -DTIC_BUILD_WITH_JS -DTIC_BUILD_WITH_WREN -DTIC_BUILD_WITH_LUA -DLUA_32BITS -std=c99 -march=armv7-a+neon-vfpv4  -D AARCH=32 -D __circle__ -D BAREMETALPI  --specs=nosys.specs -O3 -mabi=aapcs -marm  -mfloat-abi=hard -mfpu=neon-vfpv4  -D__DYNAMIC_REENT__")
# For RPI3
# investigate -funsafe-math-optimizations and -march=armv8-a+crc -mcpu=cortex-a53
set(CMAKE_C_FLAGS " -DMINIZ_NO_TIME -DLUA_32BITS -std=c99 -march=armv8-a  -D AARCH=32 -mtune=cortex-a53  -D __circle__ -D BAREMETALPI  --specs=nosys.specs -O3 -marm -mfloat-abi=hard -mfpu=neon-fp-armv8 -funsafe-math-optimizations -D__DYNAMIC_REENT__")
set(CMAKE_CXX_FLAGS " -DMINIZ_NO_TIME -DLUA_32BITS -march=armv8-a  -D AARCH=32 -mtune=cortex-a53  -D __circle__ -D BAREMETALPI  --specs=nosys.specs -O3 -marm -mfloat-abi=hard -mfpu=neon-fp-armv8 -funsafe-math-optimizations -D__DYNAMIC_REENT__")

set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)

set(CMAKE_OBJCOPY ${ARM_TOOLCHAIN_DIR}/${TOOLCHAIN_PREFIX}objcopy CACHE INTERNAL "objcopy tool")
set(CMAKE_SIZE_UTIL ${ARM_TOOLCHAIN_DIR}/${TOOLCHAIN_PREFIX}size CACHE INTERNAL "size tool")

set(CMAKE_SYSROOT ${ARM_TOOLCHAIN_DIR}/../arm-none-eabi)
set(CMAKE_FIND_ROOT_PATH ${BINUTILS_PATH})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(BAREMETALPI TRUE)

