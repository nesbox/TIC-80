################################
# naett
################################

if(USE_NAETT)
    add_library(naett STATIC ${THIRDPARTY_DIR}/naett/naett.c)
    target_include_directories(naett PUBLIC ${THIRDPARTY_DIR}/naett)

    if(WIN32)
        target_link_libraries(naett INTERFACE winhttp)
    elseif(LINUX)
        find_package(CURL REQUIRED)
        target_include_directories(naett PRIVATE ${CURL_INCLUDE_DIRS})
        target_link_libraries(naett ${CURL_LIBRARIES})
    endif()
endif()
