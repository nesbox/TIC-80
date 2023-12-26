################################
# MRUBY
################################

find_program(RUBY ruby)
find_program(RAKE rake)
if(NOT RAKE)
    set(BUILD_WITH_MRUBY_DEFAULT FALSE)
else()
    set(BUILD_WITH_MRUBY_DEFAULT TRUE)
endif()
option(BUILD_WITH_MRUBY "mruby Enabled" ${BUILD_WITH_MRUBY_DEFAULT})
message("BUILD_WITH_MRUBY: ${BUILD_WITH_MRUBY}")

if(BUILD_WITH_MRUBY)

    if(CMAKE_BUILD_TYPE)
        string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE_UC)
    endif()

    set(MRUBY_BUILDDIR ${CMAKE_SOURCE_DIR}/build/mruby)
    set(MRUBY_DIR ${THIRDPARTY_DIR}/mruby)
    if(ANDROID)
        set(MRUBY_CONFIG ${MRUBY_BUILDDIR}/tic_android.rb)
    else()
        set(MRUBY_CONFIG ${MRUBY_BUILDDIR}/tic_default.rb)
    endif()
    set(MRUBY_LIB ${MRUBY_DIR}/build/target/lib/libmruby.a)

    if(MSVC)
        set(MRUBY_TOOLCHAIN visualcpp)
    elseif(APPLE OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(MRUBY_TOOLCHAIN clang)
    else()
        set(MRUBY_TOOLCHAIN gcc)
    endif()

    if(APPLE)
        exec_program(xcrun ARGS --sdk macosx --show-sdk-path OUTPUT_VARIABLE MRUBY_SYSROOT)
    endif()

    if(ANDROID_NDK_HOME)
        set(MRUBY_RAKE_EXTRA_OPTS "${MRUBY_RAKE_EXTRA_OPTS} ANDROID_NDK_HOME=${ANDROID_NDK_HOME}")
    endif()

    include(ExternalProject)

    ExternalProject_Add(mruby_vendor
        SOURCE_DIR         ${MRUBY_DIR}
        CONFIGURE_COMMAND  ""
        BUILD_IN_SOURCE    TRUE
        BUILD_COMMAND
            ${RAKE} clean all "MRUBY_CONFIG=${MRUBY_CONFIG}"
                "TARGET_CC=\"${CMAKE_C_COMPILER}\""
                "TARGET_AR=\"${CMAKE_AR}\""
                "TARGET_CFLAGS=${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${BUILD_TYPE_UC}}"
                "TARGET_LDFLAGS=${CMAKE_EXE_LINKER_FLAGS} ${CMAKE_LINKER_FLAGS_${BUILD_TYPE_UC}}"
                "BUILD_TYPE=${BUILD_TYPE_UC}"
                "MRUBY_SYSROOT=${MRUBY_SYSROOT}"
                "MRUBY_TOOLCHAIN=${MRUBY_TOOLCHAIN}"
                "ANDROID_ARCH=${CMAKE_ANDROID_ARCH_ABI}"
                "ANDROID_PLATFORM=android-${CMAKE_SYSTEM_VERSION}"
                ${MRUBY_RAKE_EXTRA_OPTS}
        INSTALL_COMMAND    ""
        BUILD_BYPRODUCTS   ${MRUBY_LIB}
    )

    add_library(mruby STATIC IMPORTED GLOBAL)
    set_property(TARGET mruby APPEND
        PROPERTY IMPORTED_LOCATION              ${MRUBY_LIB}
    )
    set_property(TARGET mruby APPEND
        PROPERTY INTERFACE_INCLUDE_DIRECTORIES  ${MRUBY_DIR}/include
    )
    add_dependencies(mruby mruby_vendor)

    target_compile_definitions(mruby INTERFACE TIC_BUILD_WITH_MRUBY=1)

endif()