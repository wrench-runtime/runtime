project(wrench_runtime_src)

# All sources that also need to be tested in unit tests go into a static library
add_library(wren_runtime SHARED wren_runtime.c mutex.c mutex.h os_call.c os_call.h)
target_include_directories(wren_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(wren_runtime PUBLIC wren_static stb_ds)

add_library(readfile INTERFACE)
target_include_directories(readfile INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
