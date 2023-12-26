################################
# LUA
################################

set(LUA_DIR ${THIRDPARTY_DIR}/lua)
set(LUA_SRC
    ${LUA_DIR}/lapi.c
    ${LUA_DIR}/lcode.c
    ${LUA_DIR}/lctype.c
    ${LUA_DIR}/ldebug.c
    ${LUA_DIR}/ldo.c
    ${LUA_DIR}/ldump.c
    ${LUA_DIR}/lfunc.c
    ${LUA_DIR}/lgc.c
    ${LUA_DIR}/llex.c
    ${LUA_DIR}/lmem.c
    ${LUA_DIR}/lobject.c
    ${LUA_DIR}/lopcodes.c
    ${LUA_DIR}/lparser.c
    ${LUA_DIR}/lstate.c
    ${LUA_DIR}/lstring.c
    ${LUA_DIR}/ltable.c
    ${LUA_DIR}/ltm.c
    ${LUA_DIR}/lundump.c
    ${LUA_DIR}/lvm.c
    ${LUA_DIR}/lzio.c
    ${LUA_DIR}/lauxlib.c
    ${LUA_DIR}/lbaselib.c
    ${LUA_DIR}/lcorolib.c
    ${LUA_DIR}/ldblib.c
    ${LUA_DIR}/liolib.c
    ${LUA_DIR}/lmathlib.c
    ${LUA_DIR}/loslib.c
    ${LUA_DIR}/lstrlib.c
    ${LUA_DIR}/ltablib.c
    ${LUA_DIR}/lutf8lib.c
    ${LUA_DIR}/loadlib.c
    ${LUA_DIR}/linit.c
)

add_library(lua STATIC ${LUA_SRC})

target_compile_definitions(lua PRIVATE LUA_COMPAT_5_2)
target_include_directories(lua INTERFACE ${THIRDPARTY_DIR}/lua)

if(N3DS)
    target_compile_definitions(lua PUBLIC LUA_32BITS)
endif()

################################
# LPEG
################################

set(LPEG_DIR ${THIRDPARTY_DIR}/lpeg)
set(LPEG_SRC
    ${LPEG_DIR}/lpcap.c
    ${LPEG_DIR}/lpcode.c
    ${LPEG_DIR}/lpprint.c
    ${LPEG_DIR}/lptree.c
    ${LPEG_DIR}/lpvm.c
)

add_library(lpeg STATIC ${LPEG_SRC})
target_include_directories(lpeg PRIVATE ${LUA_DIR})