################################
# PNG
################################

if(PREFER_SYSTEM_LIBRARIES)
    find_package(PkgConfig)
    pkg_check_modules(libpng IMPORTED_TARGET GLOBAL libpng)
    if (libpng_FOUND)
        add_library(png ALIAS PkgConfig::libpng)
        message(STATUS "Use system library: libpng")
        return()
    else()
        message(WARNING "System library libpng not found")
    endif()
endif()


set(LIBPNG_DIR ${THIRDPARTY_DIR}/libpng)
set(LIBPNG_SRC
    ${LIBPNG_DIR}/png.c
    ${LIBPNG_DIR}/pngerror.c
    ${LIBPNG_DIR}/pngget.c
    ${LIBPNG_DIR}/pngmem.c
    ${LIBPNG_DIR}/pngpread.c
    ${LIBPNG_DIR}/pngread.c
    ${LIBPNG_DIR}/pngrio.c
    ${LIBPNG_DIR}/pngrtran.c
    ${LIBPNG_DIR}/pngrutil.c
    ${LIBPNG_DIR}/pngset.c
    ${LIBPNG_DIR}/pngtrans.c
    ${LIBPNG_DIR}/pngwio.c
    ${LIBPNG_DIR}/pngwrite.c
    ${LIBPNG_DIR}/pngwtran.c
    ${LIBPNG_DIR}/pngwutil.c
)

configure_file(${LIBPNG_DIR}/scripts/pnglibconf.h.prebuilt ${CMAKE_CURRENT_BINARY_DIR}/pnglibconf.h)

add_library(png STATIC ${LIBPNG_SRC})

target_compile_definitions(png PRIVATE PNG_ARM_NEON_OPT=0)

target_include_directories(png
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR}
    PRIVATE ${THIRDPARTY_DIR}/zlib
    INTERFACE ${THIRDPARTY_DIR}/libpng)