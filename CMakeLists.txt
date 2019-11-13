cmake_minimum_required (VERSION 3.1)
project (PicSorter LANGUAGES CXX VERSION 0.0 DESCRIPTION "api for image simualarity check")

set (CMAKE_CXX_STANDARD 17)

set (CMAKE_EXPORT_COMPILE_COMMANDS=YES)

# add_compile_options( /std:c++17 )

file(GLOB SOURCE_FILES
	"src/*.cpp"
	"src/*.cl"
)
file(GLOB PUBLIC_HEADER_FILES
	"include/*.hpp"
)

include(GNUInstallDirs)
set (PACKAGE_DIR  ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig CACHE PATH "path to install pakcage configuration")
set (INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR} CACHE STRING "library directory name")
set (INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR} CACHE STRING "include directory name")
set (INSTALL_BINDIR ${CMAKE_INSTALL_BINDIR} CACHE STRING "binary directory name")

set (BUILD_SUB YES CACHE BOOL "build submodules or just link them")

if (BUILD_SUB)
	add_subdirectory(src/fJPG fJPG )
else()
	set(fJPG_DIR . CACHE PATH "path to fJPG package")
	find_package(fJPG REQUIRED)
endif()


# find_package( OpenCL REQUIRED )

include_directories("/home/bendajul/cuda_65/include/")
link_directories( "/home/bendajul/cuda_65/lib64" )

# find_package( OpenCL REQUIRED )
# message(" ${OpenCL_INCLUDE_DIRS} : ${OpenCL_LIBRARIES}")

# include_directories( "C:/Program Files (x86)/IntelSWTools/OpenCL/sdk/include" )
# link_directories( "C:/Program Files (x86)/IntelSWTools/OpenCL/sdk/lib/x86" )

add_executable(api ${SOURCE_FILES})
set_target_properties( api PROPERTIES
	VERSION ${PROJECT_VERSION}
	)


if (NOT BUILD_SUB)
	target_include_directories(api PUBLIC ${fJPG_INCLUDE_DIRS})
	target_link_libraries(api ${fJPG_LIBRARIES})
else()
	target_link_libraries(api fJPG)
endif()


target_link_libraries(api stdc++fs  OpenCL)
# target_link_libraries(api OpenCL)

install(TARGETS api 
	RUNTIME DESTINATION ${INSTALL_BINDIR})
