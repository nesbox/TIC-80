################################
# Warnings for the code we own
################################

# -Wall -Wextra -Werror is for the files we wrote: the libraries under vendor/
# keep their own habits, and their messages would bury ours in the log. Ours
# all live under src/, whichever target compiles them, so their path picks
# them out. A warning here is a build that stops, so ours stays at zero.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")

    set(TIC80_WARNING_FLAGS -Wall -Wextra -Werror)

    file(GLOB_RECURSE TIC80_OWN_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_SOURCE_DIR}/src/*.c
        ${CMAKE_SOURCE_DIR}/src/*.cpp)

    set_source_files_properties(${TIC80_OWN_SOURCES}
        PROPERTIES COMPILE_OPTIONS "${TIC80_WARNING_FLAGS}")

endif()
