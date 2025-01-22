################################
# YUESCRIPT
################################

option(BUILD_WITH_YUE "Yue Enabled" ${BUILD_WITH_ALL})
message("BUILD_WITH_YUE: ${BUILD_WITH_YUE}")

if(BUILD_WITH_YUE)
    set(YUESCRIPT_DIR ${THIRDPARTY_DIR}/yuescript)
    set(YUESCRIPT_SRC
        ${YUESCRIPT_DIR}/src/yuescript/ast.cpp
        ${YUESCRIPT_DIR}/src/yuescript/parser.cpp
        ${YUESCRIPT_DIR}/src/yuescript/yue_ast.cpp
        ${YUESCRIPT_DIR}/src/yuescript/yue_parser.cpp
        ${YUESCRIPT_DIR}/src/yuescript/yue_compiler.cpp
        ${YUESCRIPT_DIR}/src/yuescript/yuescript.cpp
    )

    list(APPEND YUESCRIPT_SRC ${CMAKE_SOURCE_DIR}/src/api/yue.c)
    list(APPEND YUESCRIPT_SRC ${CMAKE_SOURCE_DIR}/src/api/parse_note.c)

    add_library(yuescript ${TIC_RUNTIME} ${YUESCRIPT_SRC})

    if(NOT BUILD_STATIC)
        set_target_properties(yuescript PROPERTIES PREFIX "")
    else()
        target_compile_definitions(yuescript INTERFACE TIC_BUILD_WITH_YUE)
    endif()

    target_link_libraries(yuescript
        PRIVATE
        runtime
        luaapi
    )

    set_target_properties(yuescript PROPERTIES
        LINKER_LANGUAGE CXX
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )

    target_include_directories(yuescript
        PRIVATE
        ${YUESCRIPT_DIR}/src
        ${YUESCRIPT_DIR}/src/yuescript
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/api
    )

    target_compile_definitions(yuescript PRIVATE 
        YUE_NO_MACRO 
        YUE_WRAPPER_EXPORTS
        YUE_STATIC_CONFIG
        $<$<BOOL:${MSVC}>:_SCL_SECURE_NO_WARNINGS>
    )

    # Platform specific configurations
    if(BAREMETALPI OR NINTENDO_3DS)
        # Disable thread-safe statics for C++ files
        set_source_files_properties(
            ${YUESCRIPT_SRC}
            ${CMAKE_SOURCE_DIR}/src/api/yue_wrapper/yue_wrapper.cpp
            PROPERTIES COMPILE_FLAGS "-fno-threadsafe-statics"
        )
        
        # Force static linking for baremetal
        set_target_properties(yuescript PROPERTIES 
            STATIC_LIBRARY TRUE
            POSITION_INDEPENDENT_CODE FALSE
        )
        
        # Add baremetal specific compile flags
        target_compile_options(yuescript PRIVATE 
            -fno-rtti
        )
    endif()

    target_sources(yuescript PRIVATE
        ${CMAKE_SOURCE_DIR}/src/api/yue_wrapper/yue_wrapper.cpp
        ${CMAKE_SOURCE_DIR}/src/api/yue_wrapper/yue_wrapper.h
    )

    target_link_libraries(yuescript PRIVATE runtime luaapi)

    if(MSVC)
        target_compile_definitions(yuescript PRIVATE _SCL_SECURE_NO_WARNINGS)
    else()
        target_compile_options(yuescript PRIVATE -Wall -Wno-long-long -fPIC -O3)
    endif()

    if(APPLE)
        target_compile_options(yuescript PRIVATE -Wno-deprecated-declarations)
    endif()

endif()
