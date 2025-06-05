add_library(runtime INTERFACE)

target_sources(runtime INTERFACE ${ASSETS_DEPS})

if(BUILD_STATIC)
    set(TIC_RUNTIME STATIC)
    target_compile_definitions(runtime INTERFACE TIC_RUNTIME_STATIC)
else()
    set(TIC_RUNTIME SHARED)
endif()

target_compile_definitions(runtime INTERFACE BUILD_DEPRECATED)
target_include_directories(runtime INTERFACE ${CMAKE_BINARY_DIR})
