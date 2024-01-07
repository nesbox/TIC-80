################################
# naett
################################

if(NOT RPI AND NOT N3DS AND NOT EMSCRIPTEN AND NOT BAREMETALPI)
    set(USE_NAETT TRUE)
endif()

# On Linux, naett requires curl. Disable naett if curl is not found.
if(LINUX)
    find_package(CURL)
    if(NOT CURL_FOUND)
        set(USE_NAETT FALSE)
    endif()
endif()

if(USE_NAETT)
    add_library(naett STATIC ${THIRDPARTY_DIR}/naett/naett.c)
    target_include_directories(naett PUBLIC ${THIRDPARTY_DIR}/naett)

    if(WIN32)
        target_link_libraries(naett INTERFACE winhttp)
    elseif(LINUX)
        target_include_directories(naett PRIVATE ${CURL_INCLUDE_DIRS})
        target_link_libraries(naett ${CURL_LIBRARIES})
    endif()
endif()
