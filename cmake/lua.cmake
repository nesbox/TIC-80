################################
# LUA
################################

set(BUILD_WITH_LUA_DEFAULT TRUE)
set(BUILD_WITH_MOON_DEFAULT TRUE)
set(BUILD_WITH_FENNEL_DEFAULT TRUE)

option(BUILD_WITH_LUA "Lua Enabled" ${BUILD_WITH_LUA_DEFAULT})
message("BUILD_WITH_LUA: ${BUILD_WITH_LUA}")

option(BUILD_WITH_MOON "Moon Enabled" ${BUILD_WITH_MOON_DEFAULT})
message("BUILD_WITH_MOON: ${BUILD_WITH_MOON}")

option(BUILD_WITH_FENNEL "Fennel Enabled" ${BUILD_WITH_FENNEL_DEFAULT})
message("BUILD_WITH_FENNEL: ${BUILD_WITH_FENNEL}")

if(BUILD_WITH_MOON OR BUILD_WITH_FENNEL)
    set(BUILD_WITH_LUA TRUE)
endif()

if(BUILD_WITH_LUA)

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
        ${LUA_DIR}/lbitlib.c
    )

    list(APPEND LUA_SRC ${CMAKE_SOURCE_DIR}/src/api/lua.c)
    list(APPEND LUA_SRC ${CMAKE_SOURCE_DIR}/src/api/parse_note.c)

    if(BUILD_WITH_MOON)
        list(APPEND LUA_SRC ${CMAKE_SOURCE_DIR}/src/api/moonscript.c)
    endif()

    if(BUILD_WITH_FENNEL)
        list(APPEND LUA_SRC ${CMAKE_SOURCE_DIR}/src/api/fennel.c)
    endif()

    add_library(lua ${TIC_RUNTIME} ${LUA_SRC})

    if(NOT BUILD_STATIC)
        set_target_properties(lua PROPERTIES PREFIX "")
    endif()

    target_compile_definitions(lua PRIVATE LUA_COMPAT_5_2)
    target_include_directories(lua 
        PUBLIC ${THIRDPARTY_DIR}/lua
        PRIVATE 
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )

    if(N3DS)
        target_compile_definitions(lua PUBLIC LUA_32BITS)
    endif()

    target_compile_definitions(lua INTERFACE TIC_BUILD_WITH_LUA)

    if(BUILD_WITH_MOON)
        target_include_directories(lua PRIVATE ${THIRDPARTY_DIR}/moonscript)
        target_compile_definitions(lua INTERFACE TIC_BUILD_WITH_MOON)
    endif()

    if(BUILD_WITH_FENNEL)
        target_include_directories(lua PRIVATE ${THIRDPARTY_DIR}/fennel)
        target_compile_definitions(lua INTERFACE TIC_BUILD_WITH_FENNEL)
    endif()

    if(BUILD_DEMO_CARTS)
        list(APPEND DEMO_CARTS ${CMAKE_SOURCE_DIR}/config.lua)

        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/quest.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/car.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/music.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/sfx.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/palette.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/bpp.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/tetris.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/font.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/fire.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/benchmark.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/p3d.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/luademo.lua)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/bunny/luamark.lua)

        if(BUILD_WITH_MOON)
            list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/moondemo.moon)
            list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/bunny/moonmark.moon)
        endif()

        if(BUILD_WITH_FENNEL)
            list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/fenneldemo.fnl)
        endif()
    endif()
endif()

################################
# LPEG
################################

if(BUILD_WITH_MOON)

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
    target_link_libraries(lua PRIVATE lpeg)
endif()
