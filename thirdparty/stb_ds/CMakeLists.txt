project (stb_ds)

# Header only library, therefore INTERFACE
add_library(stb_ds INTERFACE)

# INTERFACE targets only have INTERFACE properties
target_include_directories(stb_ds INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)