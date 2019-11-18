add_executable(Test test.cpp)
target_compile_definitions(Test PRIVATE SAMPLE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(Test fJPG)

