################################
# SQUIRREL
################################

set(BUILD_WITH_SQUIRREL_DEFAULT TRUE)

option(BUILD_WITH_SQUIRREL "Squirrel Enabled" ${BUILD_WITH_SQUIRREL_DEFAULT})
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

    if(BUILD_STATIC)
        add_library(squirrel STATIC ${SQUIRREL_SRC})
        target_compile_definitions(squirrel PUBLIC TIC_BUILD_STATIC)
        target_link_libraries(squirrel PRIVATE tic80core)
    else()
        add_library(squirrel SHARED ${SQUIRREL_SRC})
        set_target_properties(squirrel PROPERTIES PREFIX "")
        if(MINGW)
            target_link_options(squirrel PRIVATE -static)
        endif()
    endif()

    set_target_properties(squirrel PROPERTIES LINKER_LANGUAGE CXX)

    target_include_directories(squirrel 
        PUBLIC ${SQUIRREL_DIR}/include
        PRIVATE 
            ${SQUIRREL_DIR}/squirrel
            ${SQUIRREL_DIR}/sqstdlib
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )

    target_compile_definitions(squirrel INTERFACE TIC_BUILD_WITH_SQUIRREL=1)

    if(BUILD_DEMO_CARTS)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/squirreldemo.nut)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/bunny/squirrelmark.nut)
    endif()

endif()
