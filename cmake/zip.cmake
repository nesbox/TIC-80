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
