cmake_minimum_required (VERSION 3.1)
project (fJPG LANGUAGES CXX VERSION 0.0 DESCRIPTION "fast jpg converter (lose of data)")

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

file(GLOB SOURCE_FILES
	"src/*.cpp"
)

file(GLOB PUBLIC_HEADER_FILES
	"include/*.hpp"
)

if(LIB_TYPE STREQUAL "STATIC")
	set(LIB_SUFFIX "a")
	add_library(fJPG STATIC ${SOURCE_FILES})
elseif(LIB_TYPE STREQUAL "SHARED")
	set(LIB_SUFFIX "so")
	add_library(fJPG SHARED ${SOURCE_FILES})
else()
	message(FATAL_ERROR "invalid library type '${LIB_TYPE}', only STATIC and SHARED supported")
endif()
target_include_directories(fJPG PUBLIC include)

set_target_properties(fJPG PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(fJPG PROPERTIES PUBLIC_HEADER ${PUBLIC_HEADER_FILES} )


if (CMAKE_CXX_COMPILER_ID STREQUAL GNU) 
	target_link_libraries(fJPG stdc++fs)
endif()

configure_file(fJPG.pc.in fJPGConfig.cmake @ONLY)
install(TARGETS fJPG
	LIBRARY DESTINATION ${INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${INSTALL_LIBDIR}
	PUBLIC_HEADER DESTINATION ${INSTALL_INCLUDEDIR}	
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/fJPGConfig.cmake DESTINATION ${PACKAGE_DIR})
