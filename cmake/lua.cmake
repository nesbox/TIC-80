################################
# LUA
################################

option(BUILD_WITH_LUA "Lua Enabled" ON)

if(BUILD_WITH_LUA)
    set(BUILD_WITH_MOON OFF CACHE BOOL "" FORCE)
    set(BUILD_WITH_YUE OFF CACHE BOOL "" FORCE)
    set(BUILD_WITH_FENNEL OFF CACHE BOOL "" FORCE)
    set(BUILD_WITH_JS OFF CACHE BOOL "" FORCE)
    set(BUILD_WITH_SCHEME OFF CACHE BOOL "" FORCE)
    set(BUILD_WITH_SQUIRREL OFF CACHE BOOL "" FORCE)
    set(BUILD_WITH_PYTHON OFF CACHE BOOL "" FORCE)
    set(BUILD_WITH_WREN OFF CACHE BOOL "" FORCE)
    set(BUILD_WITH_RUBY OFF CACHE BOOL "" FORCE)
    set(BUILD_WITH_JANET OFF CACHE BOOL "" FORCE)
    set(BUILD_WITH_WASM OFF CACHE BOOL "" FORCE)
endif()

message("BUILD_WITH_LUA: ${BUILD_WITH_LUA}")

set(LUAJIT_DIR ${THIRDPARTY_DIR}/LuaJIT CACHE PATH "Path to LuaJIT")
add_subdirectory(${THIRDPARTY_DIR}/luajit-cmake)

add_library(luaapi STATIC
    ${CMAKE_SOURCE_DIR}/src/api/luaapi.c
    ${CMAKE_SOURCE_DIR}/src/api/parse_note.c
)
target_link_libraries(luaapi PRIVATE runtime luajit::lib)
target_include_directories(luaapi PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    luajit::header
)
target_compile_definitions(luaapi PRIVATE LUA_COMPAT_5_2)

add_library(lua ${TIC_RUNTIME} ${CMAKE_SOURCE_DIR}/src/api/lua.c)

if(NOT BUILD_STATIC)
    set_target_properties(lua PROPERTIES PREFIX "")
else()
    target_compile_definitions(lua INTERFACE TIC_BUILD_WITH_LUA)
endif()

target_link_libraries(lua PRIVATE runtime luaapi luajit::lib)
target_include_directories(lua
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    PRIVATE
        luajit::header
)

