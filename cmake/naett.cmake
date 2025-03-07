################################
# naett
################################

if(NOT RPI AND NOT NINTENDO_3DS AND NOT EMSCRIPTEN AND NOT BAREMETALPI)
    set(USE_NAETT TRUE)
endif()

# On Linux, naett requires curl. Disable naett if curl is not found.
if(LINUX)
    find_package(CURL)
    if(NOT CURL_FOUND)
        set(USE_NAETT FALSE)
    endif()
endif()

if(PREFER_SYSTEM_LIBRARIES)
    find_path(naett_INCLUDE_DIR NAMES naett.h)
    find_library(naett_LIBRARY NAMES naett)
    if(naett_INCLUDE_DIR AND naett_LIBRARY)
        add_library(naett UNKNOWN IMPORTED GLOBAL)
        set_target_properties(naett PROPERTIES
            IMPORTED_LOCATION "${naett_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${naett_INCLUDE_DIR}"
        )
        message(STATUS "Use system library: naett")
        set(USE_NAETT TRUE)
        return()
    else()
        message(WARNING "System library naett not found")
    endif()
endif()

if(USE_NAETT)
    add_library(naett STATIC ${THIRDPARTY_DIR}/naett/naett.c)
    target_include_directories(naett PUBLIC ${THIRDPARTY_DIR}/naett)

    if(WIN32)
        target_link_libraries(naett INTERFACE winhttp)
    elseif(LINUX)
        target_include_directories(naett PRIVATE ${CURL_INCLUDE_DIRS})
        target_link_libraries(naett ${CURL_LIBRARIES} pthread)
    elseif(APPLE)
        target_link_libraries(naett
            "-framework Cocoa")
    endif()
endif()
