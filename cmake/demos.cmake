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
  cmake_path(APPEND ${CMAKE_BINARY_DIR} ${cartridge} OUTPUT_VARIABLE cartridge)
  LIST(APPEND cartridges ${cartridge})
  message(DEBUG "${project}")
  message(DEBUG "${cartridge}")
endforeach()

# <<generate the tic.dat files>>
<<generate the tic.dat files using the original tools>>

target_sources(GenerateTicDatFiles PRIVATE ${PROJECT_PATHS})
