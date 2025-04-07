cmake_minimum_required(VERSION 3.10)

if(DEFINED CMAKE_CROSSCOMPILING)
  return()
endif()

if(CMAKE_TOOLCHAIN_FILE)
  # touch the toolchain variable to suppress "unused variable" warning
endif()

set(SYMBIAN_SUPPORT_ROOT ${CMAKE_SOURCE_DIR}/build/symbian)

find_program(ELF2E32 elf2e32 REQUIRED)

set(CMAKE_C_COMPILER_WORKS    1)
set(CMAKE_CXX_COMPILER_WORKS  1)
set(CMAKE_CXX_STANDARD        14)
set(CMAKE_CXX_STANDARD_REQUIRED 1)

set(CMAKE_STATIC_LIBRARY_PREFIX "")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".lib")
set(CMAKE_SHARED_LIBRARY_PREFIX "")
set(CMAKE_SHARED_LIBRARY_SUFFIX ".dso")
set(CMAKE_IMPORT_LIBRARY_PREFIX "")
set(CMAKE_IMPORT_LIBRARY_SUFFIX ".lib")
set(CMAKE_EXECUTABLE_SUFFIX ".exe")
set(CMAKE_LINK_LIBRARY_SUFFIX ".lib")
set(CMAKE_DL_LIBS "")

set(CMAKE_SYSTEM_NAME "Symbian")

set(CMAKE_FIND_LIBRARY_PREFIXES "")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".dso")

set(SYMBIAN_EPOCROOT	$ENV{EPOCROOT}/epoc32)
set(SYMBIAN_TOOLROOT	"${SYMBIAN_EPOCROOT}/tools" CACHE STRING "Symbian tools root directory")

set(SYMBIAN_GCCE_SUPPORT_HEADER "${SYMBIAN_EPOCROOT}/include/gcce/gcce.h" CACHE STRING "Symbian GCCE support header")

set(CROSS_COMPILER_PREFIX	"arm-none-symbianelf-" CACHE STRING "Cross compiler prefix")

set(TOOL_OS_SUFFIX "")
if(CMAKE_HOST_WIN32)
  set(TOOL_OS_SUFFIX ".exe")
endif()

set(CMAKE_C_COMPILER "${CROSS_COMPILER_PREFIX}gcc${TOOL_OS_SUFFIX}")
set(CMAKE_CXX_COMPILER "${CROSS_COMPILER_PREFIX}g++${TOOL_OS_SUFFIX}")
set(CMAKE_LINKER 		"${CROSS_COMPILER_PREFIX}ld${TOOL_OS_SUFFIX}")

set(SYMBIAN_LIB_DIR ${SYMBIAN_EPOCROOT}/release/armv5/lib)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(SYMBIAN_USER_LIB_DIR ${SYMBIAN_EPOCROOT}/release/armv5/urel)
else()
  set(SYMBIAN_USER_LIB_DIR ${SYMBIAN_EPOCROOT}/release/armv5/udeb)
  set(ELF2E32_EXTRA_FLAGS "--debuggable")
endif()

set(CMAKE_FIND_ROOT_PATH ${SYMBIAN_EPOCROOT}/lib/gcc/arm-none-symbianelf/${ARM_CC_VERSION} ${SYMBIAN_EPOCROOT} ${SYMBIAN_EPOCROOT}/release/armv5 ${SYMBIAN_EPOCROOT}/release/armv5/lib)

set(BASE_INC "")
list(APPEND BASE_INC
  ${SYMBIAN_EPOCROOT}/include
  ${SYMBIAN_EPOCROOT}/include/stdapis/stlport
  ${SYMBIAN_EPOCROOT}/include/stdapis
  ${SYMBIAN_EPOCROOT}/include/mw
  ${SYMBIAN_EPOCROOT}/include/platform
  ${SYMBIAN_EPOCROOT}/include/platform/mw
  ${SYMBIAN_SUPPORT_ROOT}/sympathy
)

set(BASE_INC_FLAGS "${BASE_INC}")
list(TRANSFORM BASE_INC_FLAGS PREPEND "-I")

include_directories(BEFORE SYSTEM
  ${BASE_INC}
)

link_directories(BEFORE
  ${SYMBIAN_USER_LIB_DIR}
  ${SYMBIAN_LIB_DIR}
  ${SYMBIAN_EPOCROOT}/release/armv5/lib
)

link_libraries(
  :eexe.lib
  -Wl,--start-group
  :usrt2_2.lib
  :libcrt0.lib
  -Wl,--end-group
  :libc.dso
  :libm.dso
  :libpthread.dso
  :euser.dso
  :dfpaeabi.dso
  :dfprvct2_2.dso
  :drtaeabi.dso
  :scppnwdl.dso
  :drtrvct2_2.dso
  :libdl.dso
  :bafl.dso
  :estor.dso
  :eikcore.dso
  :apparc.dso
  :avkon.dso
  :cone.dso
  :hal.dso
  :libGLES_CM.dso
  :ws32.dso
  :gdi.dso
  :mediaclientaudiostream.dso
  :inetprotutil.dso
  :etext.dso
  :efsrv.dso
  supc++
  gcc
)

list(APPEND SYMBIAN_DEFINITIONS_LIST
  -D__ARM_ARCH_5__
  -DNDEBUG
  -D_UNICODE
  -D__GCCE__
  -D__SYMBIAN32__
  -D__SERIES60_31__
  -D__SERIES60_3X__
  -D__GCCE__
  -D__EPOC32__
  -D__MARM__
  -D__EABI__
  -D__MARM_ARMV5__
  -D__EXE__
  -D__SUPPORT_CPP_EXCEPTIONS__
  -D__MARM_ARMV5__
)
set(SYMBIAN_PRODUCT	"symbian_os" CACHE STRING "Symbian Product Name")
set(SYMBIAN_PRODUCT_INCLUDE ${SYMBIAN_EPOCROOT}/include/variant/${SYMBIAN_PRODUCT}.hrh)
list(JOIN SYMBIAN_DEFINITIONS_LIST " " SYMBIAN_DEFINITIONS)

add_definitions(
  ${SYMBIAN_DEFINITIONS}
  "-D__PRODUCT_INCLUDE__=\"${SYMBIAN_PRODUCT_INCLUDE}\""
)

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -Wno-error=narrowing -msoft-float -mapcs -mthumb-interwork -march=armv5t -fno-unit-at-a-time -fno-common -include ${SYMBIAN_GCCE_SUPPORT_HEADER}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive -Wno-error=narrowing -msoft-float -mapcs -mthumb-interwork -march=armv5t -fno-unit-at-a-time -fno-threadsafe-statics -include ${SYMBIAN_GCCE_SUPPORT_HEADER}")
set(CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_CXX_COMPILER> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> -Wl,-Map <TARGET>.exe.map <LINK_LIBRARIES>")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--default-symver -Wl,--fatal-warnings -Wl,--no-relax -Wl,--no-undefined -shared -Ttext 0x8000 -Tdata 0x400000 --entry _E32Startup -u _E32Startup -nostdlib -shared")

function(elf2e32 ONAME INAME UID3)
  add_custom_target(${ONAME}-e32
      COMMAND ${ELF2E32} ${ELF2E32_EXTRA_FLAGS}
                         --capability="LocalServices+ReadUserData+UserEnvironment+WriteUserData+NetworkServices"
                         --elfinput="${INAME}" --output="${ONAME}.exe" --libpath="${SYMBIAN_LIB_DIR}" --linkas="${ONAME}"
                         --fpu=softvfp --uid1=0x1000007a --uid2=0x100039ce --uid3="${UID3}" --sid="${UID3}" --targettype=EXE
                         --dlldata --ignorenoncallable --smpsafe --heap=0x800000,0x3000000 --stack=0x10000
    DEPENDS "${INAME}"
  )
endfunction()

function(rcomp TG SRC)
  add_custom_target(${TG}-rsc
    COMMAND ${CMAKE_CXX_COMPILER} ${SYMBIAN_DEFINITIONS} ${BASE_INC_FLAGS} -I"${SYMBIAN_SUPPORT_ROOT}" -Isrc/system/symbian -I"${CMAKE_CURRENT_BINARY_DIR}" -x c++ -E -P -o "${TG}.pprss" "${SRC}.rss"
    COMMAND ${SYMBIAN_TOOLROOT}/rcomp -s"${TG}.pprss" -h"${TG}.rsg" -o"${TG}.rsc" -u
  )
endfunction()

function(mifconv TG SRC)
  add_custom_target(${TG}-mif
    COMMAND ${SYMBIAN_TOOLROOT}/mifconv "${TG}.mif" -c32 "${SRC}"
  )
endfunction()

function(makesis SIS PKG)
  add_custom_target(${SIS}-sis
    COMMAND ${SYMBIAN_TOOLROOT}/makesis ${PKG} ${SIS}.sis
  )
endfunction()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(UNIX TRUE)
set(EPOC TRUE)
set(EPOC32 TRUE)
set(SYMBIAN TRUE)
