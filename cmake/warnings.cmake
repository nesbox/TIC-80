################################
# Warnings for the code we own
################################

# -Wall -Wextra -Werror is for the files we wrote: the libraries under vendor/
# keep their own habits, and their messages would bury ours in the log. Ours
# all live under src/, whichever target compiles them, so their path picks
# them out. A warning here is a build that stops, so ours stays at zero.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")

    # Xcode turns -Wshorten-64-to-32 on by itself for the Apple backend, and
    # the script bindings narrow 64-bit host values to the 32-bit API on
    # purpose: the flag is not part of -Wall -Wextra and comes off with them.
    set(TIC80_WARNING_FLAGS -Wall -Wextra -Werror -Wno-shorten-64-to-32)

    file(GLOB_RECURSE TIC80_OWN_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_SOURCE_DIR}/src/*.c
        ${CMAKE_SOURCE_DIR}/src/*.cpp)

    set_source_files_properties(${TIC80_OWN_SOURCES}
        PROPERTIES COMPILE_OPTIONS "${TIC80_WARNING_FLAGS}")

endif()
