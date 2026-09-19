################################
# Native Apple Backend (Metal/AVFoundation/Swift)
################################

if(BUILD_APPLE)
    enable_language(Swift)
    
    set(TIC80_SRC
        src/system/apple/bridging.h
        src/system/apple/MetalRenderer.swift
        src/system/apple/AudioEngine.swift
        src/system/apple/GamepadManager.swift
        src/system/apple/main.swift
    )
    
    add_executable(${TIC80_TARGET} ${TIC80_SRC})
    
    target_link_libraries(${TIC80_TARGET} PRIVATE tic80studio)
    
    # Configure Swift Bridging Header
    set_target_properties(${TIC80_TARGET} PROPERTIES
        Swift_BRIDGING_HEADER "${CMAKE_SOURCE_DIR}/src/system/apple/bridging.h"
        XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "${CMAKE_SOURCE_DIR}/src/system/apple/bridging.h"
        SWIFT_MODULE_NAME "tic80"
    )
    
    target_compile_options(${TIC80_TARGET} PRIVATE
        $<$<COMPILE_LANGUAGE:Swift>:-import-objc-header>
        $<$<COMPILE_LANGUAGE:Swift>:${CMAKE_SOURCE_DIR}/src/system/apple/bridging.h>
    )
    
    target_link_options(${TIC80_TARGET} PRIVATE
        "LINKER:-rpath,${CMAKE_BINARY_DIR}/bin"
        "LINKER:-rpath,@executable_path"
    )
    
    # Link native macOS frameworks
    target_link_libraries(${TIC80_TARGET} PRIVATE
        "-framework AppKit"
        "-framework Metal"
        "-framework MetalKit"
        "-framework AVFoundation"
        "-framework GameController"
    )

    if(BUILD_RENDER_CACHE)
        target_compile_definitions(${TIC80_TARGET} PRIVATE BUILD_RENDER_CACHE=1)
        target_compile_options(${TIC80_TARGET} PRIVATE
            $<$<COMPILE_LANGUAGE:Swift>:-Xcc>
            $<$<COMPILE_LANGUAGE:Swift>:-DBUILD_RENDER_CACHE=1>
        )
    endif()
endif()
