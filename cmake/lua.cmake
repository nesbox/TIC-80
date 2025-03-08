################################
# LUA
################################

option(BUILD_WITH_LUA "Lua Enabled" ON)
message("BUILD_WITH_LUA: ${BUILD_WITH_LUA}")

if(BUILD_WITH_LUA AND PREFER_SYSTEM_LIBRARIES)
    find_path(lua_INCLUDE_DIR NAMES lua.h)
    find_library(lua_LIBRARY NAMES lua)
    if(lua_INCLUDE_DIR AND lua_LIBRARY)
        add_library(luaapi STATIC
            ${CMAKE_SOURCE_DIR}/src/api/luaapi.c
            ${CMAKE_SOURCE_DIR}/src/api/parse_note.c
        )
        target_link_libraries(luaapi PRIVATE runtime ${lua_LIBRARY})
        target_include_directories(luaapi PUBLIC
            ${lua_INCLUDE_DIR}
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
        )
        add_library(lua STATIC ${CMAKE_SOURCE_DIR}/src/api/lua.c)
        target_compile_definitions(lua INTERFACE TIC_BUILD_WITH_LUA)
        target_link_libraries(lua PRIVATE runtime luaapi)
        message(STATUS "Use system library: lua")
        return()
    else()
        message(WARNING "System library lua not found")
    endif()
endif()

if(BUILD_WITH_LUA OR BUILD_WITH_MOON OR BUILD_WITH_FENNEL)
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

    add_library(luaapi STATIC
        ${LUA_SRC}
        ${CMAKE_SOURCE_DIR}/src/api/luaapi.c
        ${CMAKE_SOURCE_DIR}/src/api/parse_note.c
    )
    target_link_libraries(luaapi PRIVATE runtime)

    target_compile_definitions(luaapi PRIVATE LUA_COMPAT_5_2)

    target_include_directories(luaapi
        PUBLIC ${THIRDPARTY_DIR}/lua
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
        )

endif()

if(BUILD_WITH_LUA)

    add_library(lua ${TIC_RUNTIME} ${CMAKE_SOURCE_DIR}/src/api/lua.c)

    if(NOT BUILD_STATIC)
        set_target_properties(lua PROPERTIES PREFIX "")
    else()
        target_compile_definitions(lua INTERFACE TIC_BUILD_WITH_LUA)
    endif()

    target_link_libraries(lua PRIVATE runtime luaapi)

    target_include_directories(lua
        PUBLIC ${THIRDPARTY_DIR}/lua
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )

    if(NINTENDO_3DS)
        target_compile_definitions(luaapi PUBLIC LUA_32BITS)
    endif()

endif()
