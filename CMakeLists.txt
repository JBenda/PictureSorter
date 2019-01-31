cmake_minimum_required (VERSION 3.1)
project (netscare)

set (CMAKE_CXX_STANDARD 17)
add_compile_options( /std:c++17 )

file(GLOB SOURCE_FILES
	"src/*.cpp"
	"src/*.hpp"
)

find_package(OpenCL REQUIRED)

if (OPENCL_FOUND)
    message("opencl found")
    message("include dir: ${OPENCL_INCLUDE_DIR}")
    message("link libraries: ${OPENCL_cl_LIBRARY}")
else (OPENCL_FOUND)
    message("opencl not found")
endif()

add_executable(api ${SOURCE_FILES})
target_link_libraries(api ${OPENCL_cl_LIBRARY})