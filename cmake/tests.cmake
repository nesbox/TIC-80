################################
# Unit tests
################################

# OFF by default, so every job in the matrix and the release path configure
# exactly what they configured before. On, it adds the tests/ targets and the
# ctest wiring: `cmake --build <dir> --target studio_machine_editors` builds
# one of them, `ctest` in the build directory runs them all.
option(TIC80_BUILD_TESTS "Build the unit tests" OFF)

if(TIC80_BUILD_TESTS)
    enable_testing()
    add_subdirectory(${CMAKE_SOURCE_DIR}/tests ${CMAKE_BINARY_DIR}/tests)
endif()
