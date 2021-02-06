project(wrench_runtime_src)

# All sources that also need to be tested in unit tests go into a static library
add_library(wrench_runtime_lib STATIC mutex.c mutex.h os_call.c os_call.h)
target_include_directories(wrench_runtime_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# The main program
add_executable(wrench main.c)
target_link_libraries(wrench PRIVATE wrench_runtime_lib stb_ds wren_static)