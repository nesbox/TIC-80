set(DEMODIR ${CMAKE_SOURCE_DIR}/demos)
set(DEMOS benchmark.lua bpp.lua car.lua fenneldemo.fnl fire.lua font.lua janetdemo.janet jsdemo.js luademo.lua moondemo.moon music.lua p3d.lua palette.lua pythondemo.py quest.lua rdemo.r rubydemo.rb schemedemo.scm sfx.lua squirreldemo.nut tetris.lua wrendemo.wren)

foreach(DEMO ${DEMOS})
  file(REAL_PATH ${DEMO} DEMOABSPATH BASE_DIRECTORY ${DEMODIR})
  LIST(APPEND DEMOABSPATHS ${DEMOABSPATH})
endforeach()

set(BUNNYDIR ${DEMODIR}/bunny)
set(BUNNYS janetmark.janet jsmark.js luamark.lua moonmark.moon pythonmark.py rubymark.rb schememark.scm squirrelmark.nut wasmmark wrenmark.wren)

foreach(BUNNY ${BUNNYS})
  file(REAL_PATH ${BUNNY} BUNNYABSPATH BASE_DIRECTORY ${BUNNYDIR})
  LIST(APPEND DEMOABSPATHS ${DEMOABSPATH})
endforeach()

add_custom_target(GenerateTicDatFiles
  ALL
  COMMAND prj2cart -i ${DEMOABSPATHS} ${BUNNYABSPATHS}
  COMMAND bin2txt -zi "$<LIST:TRANSFORM,${DEMOABSPATHS},REPLACE,\"\..*$$\",\".tic\">" "$<LIST:TRANSFORM,${BUNNYABSPATHS},REPLACE,\"\..*$$\",\".tic\">"
  COMMENT "[DEMOS_AND_BUNNYS] Generating .tic files, then .tic.dat files those, from source files in the demos and demos/bunny directories (WASM not included)."
  COMMAND_EXPAND_LISTS)

target_sources(GenerateTicDatFiles PRIVATE ${DEMOABSPATHS} ${BUNNYABSPATHS})
