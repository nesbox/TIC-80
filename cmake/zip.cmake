################################
# ZIP
################################

if(PREFER_SYSTEM_LIBRARIES)
    find_path(zip_INCLUDE_DIR NAMES zip.h PATH_SUFFIXES zip)
    find_library(zip_LIBRARY NAMES zip)
    if(zip_INCLUDE_DIR AND zip_LIBRARY)
        add_library(zip UNKNOWN IMPORTED GLOBAL)
        set_target_properties(zip PROPERTIES
            IMPORTED_LOCATION "${zip_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${zip_INCLUDE_DIR}"
        )
        message(STATUS "Use system library: kubazip")
        return()
    else()
        message(WARNING "System library kubazip not found")
    endif()
endif()

set(CMAKE_DISABLE_TESTING ON CACHE BOOL "" FORCE)
add_subdirectory(${THIRDPARTY_DIR}/zip)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(zip PRIVATE -Wno-type-limits)
#  -Wno-type-limits due to:
#     zip/src/miniz.h:8503:30: error: comparison is always false due to limited range of data type [-Werror=type-limits]
#  8503 |     if (((mz_uint64)buf_size > 0xFFFFFFFF) || (uncomp_size > 0xFFFFFFFF)) {
endif()
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR BAREMETALPI)
    target_compile_options(zip PRIVATE -Wno-stringop-truncation)
#  -Wno-stringop-truncation due to:
#     /home/runner/work/TIC-80/TIC-80/vendor/zip/src/zip.c: In function 'zip_archive_extract':
#     /home/runner/work/TIC-80/TIC-80/vendor/zip/src/zip.c:191:3: error: 'strncpy' output may be truncated copying between 0 and 512 bytes from a string of length 512 [-Werror=stringop-truncation]
#  191 |   strncpy(npath, path, len);
endif()
if(BAREMETALPI)
    # lstat is not available in circle-stdlib (bare-metal environment)
    # Use stat instead, symlink handling isn't relevant on bare-metal anyway
    target_compile_definitions(zip PRIVATE lstat=stat)
endif()
