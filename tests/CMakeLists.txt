# The unit tests. The mode machine depends on neither SDL, nor the core, nor
# anything under vendor/, so every binary here is two files and compiles in a
# second — but each is built once per shipped configuration, because the build
# variants are exactly what the machinery has to get right (cmake/studio.cmake).
#
# Wired through the root project with -DBUILD_TESTS=ON. This file also works on
# its own, for a loop that does not pay for the rest of the tree:
#   cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests --output-on-failure
cmake_minimum_required(VERSION 3.14)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(tic80-tests C)
    enable_testing()
    set(TIC80_TESTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
else()
    set(TIC80_TESTS_ROOT ${CMAKE_SOURCE_DIR})
endif()

# The flags are the point of a test target: the client itself is compiled
# without -Wall, so a switch that stops covering every enum value would go
# unnoticed. -Wswitch-enum is what keeps sm_dispatch honest as modes are added.
function(studio_machine_test name)
    add_executable(${name} studio_machine.c ${TIC80_TESTS_ROOT}/src/studio/machine.c)
    target_include_directories(${name} PRIVATE ${TIC80_TESTS_ROOT}/src ${TIC80_TESTS_ROOT}/include)
    target_compile_definitions(${name} PRIVATE ${ARGN})

    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${name} PRIVATE -Wall -Wextra -Werror -Wswitch-enum)
    endif()

    add_test(NAME ${name} COMMAND ${name})
endfunction()

# The three configurations cmake/studio.cmake ships: an editors build always
# defines BUILD_SURF, and SURF_MENU is the SURF row of the main menu.
studio_machine_test(studio_machine_editors BUILD_EDITORS BUILD_SURF SURF_MENU TEST_VARIANT_EDITORS)
studio_machine_test(studio_machine_surf    BUILD_SURF SURF_MENU TEST_VARIANT_SURF)
studio_machine_test(studio_machine_plain   TEST_VARIANT_PLAIN)
