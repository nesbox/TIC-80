################################
# ArgParse lib
################################

if(PREFER_SYSTEM_LIBRARIES)
    find_path(argparse_INCLUDE_DIR NAMES argparse.h)
    find_library(argparse_LIBRARY NAMES argparse)
    if(argparse_INCLUDE_DIR AND argparse_LIBRARY)
        add_library(argparse UNKNOWN IMPORTED GLOBAL)
        set_target_properties(argparse PROPERTIES
            IMPORTED_LOCATION "${argparse_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${argparse_INCLUDE_DIR}"
        )
        message(STATUS "Use system library: argparse")
        return()
    else()
        message(WARNING "System library argparse not found")
    endif()
endif()

add_library(argparse STATIC ${THIRDPARTY_DIR}/argparse/argparse.c)
target_include_directories(argparse INTERFACE ${THIRDPARTY_DIR}/argparse)
