cmake_minimum_required (VERSION 3.12)
project (Sim LANGUAGES CXX VERSION 0.0 DESCRIPTION "Calculate simularity between images")

if(NOT CMAKE_CXX_STANDARD)
	set (CMAKE_CXX_STANDARD 17)
endif()

if(NOT CMAKE_EXPORT_COMPILE_COMMANDS)
	set (CMAKE_EXPORT_COMPILE_COMMANDS YES)
endif()

include(GNUInstallDirs)
set (PACKAGE_DIR  ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig CACHE PATH "path to install pakcage configuration")
set (INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR} CACHE STRING "library directory name")
set (INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR} CACHE STRING "include directory name")
set (LIB_TYPE "STATIC" CACHE STRING "STATIC or SHARED libs")
set (BUILD_TEST YES CACHE BOOL "build tests")
set (ALL_TEST NO CACHE BOOL "force test for each level")

file(GLOB SOURCE_FILES
	"src/*.cpp"
)

file(GLOB PUBLIC_HEADER_FILES
	"include/*.hpp"
)

if(LIB_TYPE STREQUAL "STATIC")
	set(LIB_SUFFIX "a")
	add_library(Sim STATIC ${SOURCE_FILES})
elseif(LIB_TYPE STREQUAL "SHARED")
	set(LIB_SUFFIX "so")
	add_library(Sim SHARED ${SOURCE_FILES})
else()
	message(FATAL_ERROR "invalid library type '${LIB_TYPE}', only STATIC and SHARED supported")
endif()
target_include_directories(Sim PUBLIC include)

target_compile_definitions(Sim PRIVATE DEBUG=ON)
set_target_properties(Sim PROPERTIES VERSION ${PROJECT_VERSION})
message("${PUBLIC_HEADER_FILES}")
set_target_properties(Sim PROPERTIES PUBLIC_HEADER "${PUBLIC_HEADER_FILES}" )


if (CMAKE_CXX_COMPILER_ID STREQUAL GNU) 
	target_link_libraries(Sim stdc++fs)
endif()

configure_file(Sim.pc.in SimConfig.cmake @ONLY)
install(TARGETS Sim 
	LIBRARY DESTINATION ${INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${INSTALL_LIBDIR}
	PUBLIC_HEADER DESTINATION ${INSTALL_INCLUDEDIR}	
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/SimConfig.cmake DESTINATION ${PACKAGE_DIR})

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TEST OR ALL_TEST)
	add_subdirectory (test)
	enable_testing()
endif()
