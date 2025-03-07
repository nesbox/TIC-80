################################
# ZLIB
################################

if(PREFER_SYSTEM_LIBRARIES)
    find_package(PkgConfig)
    pkg_check_modules(zlib IMPORTED_TARGET GLOBAL zlib)
    if (zlib_FOUND)
        add_library(zlib ALIAS PkgConfig::zlib)
        message(STATUS "Use system library: zlib")
        return()
    else()
        message(WARNING "System library zlib not found")
    endif()
endif()

if (NOT NINTENDO_3DS)

set(ZLIB_DIR ${THIRDPARTY_DIR}/zlib)
set(ZLIB_SRC
    ${ZLIB_DIR}/adler32.c
    ${ZLIB_DIR}/compress.c
    ${ZLIB_DIR}/crc32.c
    ${ZLIB_DIR}/deflate.c
    ${ZLIB_DIR}/inflate.c
    ${ZLIB_DIR}/infback.c
    ${ZLIB_DIR}/inftrees.c
    ${ZLIB_DIR}/inffast.c
    ${ZLIB_DIR}/trees.c
    ${ZLIB_DIR}/uncompr.c
    ${ZLIB_DIR}/zutil.c
)

add_library(zlib STATIC ${ZLIB_SRC})
target_include_directories(zlib INTERFACE ${THIRDPARTY_DIR}/zlib)

else ()

add_library(zlib STATIC IMPORTED)
set_target_properties( zlib PROPERTIES IMPORTED_LOCATION ${DEVKITPRO}/portlibs/3ds/lib/libz.a )
target_include_directories(zlib INTERFACE ${DEVKITPRO}/portlibs/3ds/include)

endif ()