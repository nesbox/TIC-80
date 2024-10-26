file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/demos" demoDir)
file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/build/assets" buildAssetDir)
file(GLOB_RECURSE projects
  "demos/*.lua"
  "demos/*.fnl"
  "demos/*.janet"
  "demos/*.js"
  "demos/*.moon"
  "demos/*.py"
  "demos/*.rb"
  "demos/*.scm"
  "demos/*.nut"
  "demos/*.wren"
  "demos/*.r")

## Create the list of project files with the tic extension rather than the
## source project file extension.
foreach(project ${projects})
  cmake_path(REPLACE_EXTENSION project LAST_ONLY "tic" OUTPUT_VARIABLE cartridge)
  cmake_path(GET cartridge FILENAME cartridge)
  cmake_path(APPEND CMAKE_BINARY_DIR ${cartridge} OUTPUT_VARIABLE cartridge)
  LIST(APPEND cartridges ${cartridge})
  message(DEBUG "${project}")
  message(DEBUG "${cartridge}")
endforeach()

add_custom_target(GenerateTicDatFiles
  ALL
  ## Generate .tic files in the CMAKE_BINARY_DIR directory using source files
  ## from the demos/ source directory.
  COMMAND prj2cart -i ${projects} -o ${CMAKE_BINARY_DIR}
  ## Convert the .tic files in the CMAKE_BINARY_DIR to .tic.dat and place the
  ## resulting file in the build/assets source directory.
  COMMAND bin2txt -zi ${cartridges} -o ${buildAssetDir}
  COMMENT "[DEMOS_AND_BUNNYS] Generating build assets (.tic.dat files)"
  COMMAND_EXPAND_LISTS)

target_sources(GenerateTicDatFiles PRIVATE ${PROJECT_PATHS})
