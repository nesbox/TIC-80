cmake_minimum_required(VERSION 3.16)

# The TIC-80 component lives one directory up, next to the backend sources.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/..")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tic80)
