################################
# SQUIRREL
################################

option(BUILD_WITH_SQUIRREL "Squirrel Enabled" ${BUILD_WITH_ALL})
message("BUILD_WITH_SQUIRREL: ${BUILD_WITH_SQUIRREL}")

if(BUILD_WITH_SQUIRREL)

    set(SQUIRREL_DIR ${THIRDPARTY_DIR}/squirrel)
    set(SQUIRREL_SRC
        ${SQUIRREL_DIR}/squirrel/sqapi.cpp
        ${SQUIRREL_DIR}/squirrel/sqbaselib.cpp
        ${SQUIRREL_DIR}/squirrel/sqclass.cpp
        ${SQUIRREL_DIR}/squirrel/sqcompiler.cpp
        ${SQUIRREL_DIR}/squirrel/sqdebug.cpp
        ${SQUIRREL_DIR}/squirrel/sqfuncstate.cpp
        ${SQUIRREL_DIR}/squirrel/sqlexer.cpp
        ${SQUIRREL_DIR}/squirrel/sqmem.cpp
        ${SQUIRREL_DIR}/squirrel/sqobject.cpp
        ${SQUIRREL_DIR}/squirrel/sqstate.cpp
        ${SQUIRREL_DIR}/squirrel/sqtable.cpp
        ${SQUIRREL_DIR}/squirrel/sqvm.cpp
        ${SQUIRREL_DIR}/sqstdlib/sqstdaux.cpp
        ${SQUIRREL_DIR}/sqstdlib/sqstdblob.cpp
        ${SQUIRREL_DIR}/sqstdlib/sqstdio.cpp
        ${SQUIRREL_DIR}/sqstdlib/sqstdmath.cpp
        ${SQUIRREL_DIR}/sqstdlib/sqstdrex.cpp
        ${SQUIRREL_DIR}/sqstdlib/sqstdstream.cpp
        ${SQUIRREL_DIR}/sqstdlib/sqstdstring.cpp
        ${SQUIRREL_DIR}/sqstdlib/sqstdsystem.cpp

    )

    list(APPEND SQUIRREL_SRC ${CMAKE_SOURCE_DIR}/src/api/squirrel.c)
    list(APPEND SQUIRREL_SRC ${CMAKE_SOURCE_DIR}/src/api/parse_note.c)

    add_library(squirrel ${TIC_RUNTIME} ${SQUIRREL_SRC})

    if(NOT BUILD_STATIC)
        set_target_properties(squirrel PROPERTIES PREFIX "")
    else()
        target_compile_definitions(squirrel INTERFACE TIC_BUILD_WITH_SQUIRREL=1)
    endif()

    target_link_libraries(squirrel PRIVATE runtime)

    set_target_properties(squirrel PROPERTIES LINKER_LANGUAGE CXX)

    target_include_directories(squirrel
        PUBLIC ${SQUIRREL_DIR}/include
        PRIVATE
            ${SQUIRREL_DIR}/squirrel
            ${SQUIRREL_DIR}/sqstdlib
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )


endif()
