project(bin2txt)

add_executable(bin2txt bin2txt.c)
target_link_libraries(bin2txt zlibstatic)
