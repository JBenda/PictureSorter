cmake_minimum_required (VERSION 3.1)
project (PicSorter)

set (CMAKE_CXX_STANDARD 17)
add_compile_options( /std:c++17 )

file(GLOB SOURCE_FILES
	"src/*.cpp"
	"src/*.hpp"
)


add_executable(api ${SOURCE_FILES})
target_link_libraries(api ${OPENCL_cl_LIBRARY})
