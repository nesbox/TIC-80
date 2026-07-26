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
    
    if(IOS)
        configure_file(${CMAKE_SOURCE_DIR}/build/macosx/tic80.plist.in ${CMAKE_BINARY_DIR}/tic80.plist)

        # Link native iOS frameworks
        target_link_libraries(${TIC80_TARGET} PRIVATE
            "-framework UIKit"
            "-framework Metal"
            "-framework MetalKit"
            "-framework AVFoundation"
            "-framework GameController"
        )
        
        set_target_properties(${TIC80_TARGET} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_BINARY_DIR}/tic80.plist
        )
    else()
        # Link native macOS frameworks
        target_link_libraries(${TIC80_TARGET} PRIVATE
            "-framework AppKit"
            "-framework Metal"
            "-framework MetalKit"
            "-framework AVFoundation"
            "-framework GameController"
        )
    endif()
endif()
