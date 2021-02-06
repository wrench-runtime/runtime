cmake_minimum_required(VERSION 3.6)
project(wrench_runtime)

# if(EMSCRIPTEN)
#   SET(CMAKE_EXECUTABLE_SUFFIX ".html")
# endif()

add_subdirectory(thirdparty/stb_ds)
add_subdirectory(thirdparty/wren)
add_subdirectory(src)

# if(EMSCRIPTEN)
# set_target_properties(wrench PROPERTIES LINK_FLAGS "-s --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/html/template.html -s MAIN_MODULE=1")
# set_target_properties(wrench PROPERTIES COMPILE_FLAGS "-fPIC -s MAIN_MODULE=1")
# set_target_properties(wren PROPERTIES COMPILE_FLAGS "-fPIC -s MAIN_MODULE=1")
# set_target_properties(wrench_runtime_lib PROPERTIES COMPILE_FLAGS "-fPIC -s MAIN_MODULE=1")
# endif()
