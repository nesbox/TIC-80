################################
# ArgParse lib
################################

add_library(argparse STATIC ${THIRDPARTY_DIR}/argparse/argparse.c)
target_include_directories(argparse INTERFACE ${THIRDPARTY_DIR}/argparse)
