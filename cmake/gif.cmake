################################
# GIFLIB
################################

if(PREFER_SYSTEM_LIBRARIES)
    find_path(giflib_INCLUDE_DIR NAMES gif_lib.h)
    find_library(giflib_LIBRARY NAMES gif)
    if(giflib_INCLUDE_DIR AND giflib_LIBRARY)
        add_library(giflib UNKNOWN IMPORTED GLOBAL)
        set_target_properties(giflib PROPERTIES
            IMPORTED_LOCATION "${giflib_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${giflib_INCLUDE_DIR};${THIRDPARTY_DIR}/msf_gif"
        )
        message(STATUS "Use system library: giflib")
        return()
    else()
        message(WARNING "System library giflib not found")
    endif()
endif()

set(GIFLIB_DIR ${THIRDPARTY_DIR}/giflib)
set(GIFLIB_SRC
    ${GIFLIB_DIR}/dgif_lib.c
    ${GIFLIB_DIR}/egif_lib.c
    ${GIFLIB_DIR}/gif_err.c
    ${GIFLIB_DIR}/gif_font.c
    ${GIFLIB_DIR}/gif_hash.c
    ${GIFLIB_DIR}/gifalloc.c
    ${GIFLIB_DIR}/openbsd-reallocarray.c
)
add_library(giflib STATIC ${GIFLIB_SRC})
target_include_directories(giflib
    PRIVATE ${GIFLIB_DIR}
    INTERFACE
        ${THIRDPARTY_DIR}/giflib
        ${THIRDPARTY_DIR}/msf_gif)