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

    add_library(squirrel STATIC ${SQUIRREL_SRC})
    set_target_properties(squirrel PROPERTIES LINKER_LANGUAGE CXX)
    target_include_directories(squirrel PUBLIC ${SQUIRREL_DIR}/include)
    target_include_directories(squirrel PRIVATE ${SQUIRREL_DIR}/squirrel)
    target_include_directories(squirrel PRIVATE ${SQUIRREL_DIR}/sqstdlib)
    target_compile_definitions(squirrel INTERFACE TIC_BUILD_WITH_SQUIRREL=1)
endif()
