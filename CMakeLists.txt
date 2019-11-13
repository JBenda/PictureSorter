cmake_minimum_required (VERSION 3.1)
project (PicSorter)

set (CMAKE_CXX_STANDARD 17)

set (CMAKE_EXPORT_COMPILE_COMMANDS=YES)

# add_compile_options( /std:c++17 )

file(GLOB SOURCE_FILES
	"src/*.cpp"
	"src/*.hpp"
	"src/*.cl"
)


# find_package( OpenCL REQUIRED )

include_directories("/home/bendajul/cuda_65/include/")
link_directories( "/home/bendajul/cuda_65/lib64" )

# find_package( OpenCL REQUIRED )
# message(" ${OpenCL_INCLUDE_DIRS} : ${OpenCL_LIBRARIES}")

# include_directories( "C:/Program Files (x86)/IntelSWTools/OpenCL/sdk/include" )
# link_directories( "C:/Program Files (x86)/IntelSWTools/OpenCL/sdk/lib/x86" )

add_executable(api ${SOURCE_FILES})
target_link_libraries(api stdc++fs  OpenCL)
# target_link_libraries(api OpenCL)
