cmake_minimum_required(VERSION 3.6)

project(wren)

file(GLOB_RECURSE wren_sources ${CMAKE_CURRENT_SOURCE_DIR}/wren/src/*.c)
file(GLOB_RECURSE wren_headers ${CMAKE_CURRENT_SOURCE_DIR}/wren/src/*.h)

add_library(wren_static STATIC ${wren_sources} ${wren_headers})
target_include_directories(wren_static PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/wren/src/vm ${CMAKE_CURRENT_SOURCE_DIR}/wren/src/optional)
target_include_directories(wren_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/wren/src/include)
target_compile_definitions(wren_static PRIVATE DEBUG)

add_library(wren SHARED ${wren_sources} ${wren_headers})
target_include_directories(wren PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/wren/src/vm ${CMAKE_CURRENT_SOURCE_DIR}/wren/src/optional)
target_include_directories(wren PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/wren/src/include)
