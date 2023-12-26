################################
# Wave writer
################################

add_library(wave_writer STATIC ${THIRDPARTY_DIR}/blip-buf/wave_writer.c)
target_include_directories(wave_writer INTERFACE ${THIRDPARTY_DIR}/blip-buf)
