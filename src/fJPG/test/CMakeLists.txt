add_executable(TestDecode testDecode.cpp)
add_executable(TestHuff testHuff.cpp ../src/HuffTable.cpp ../src/Util.cpp)
target_include_directories(TestHuff PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_compile_definitions(TestDecode PRIVATE SAMPLE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" DEBUG=ON)
target_compile_definitions(TestHuff PRIVATE SAMPLE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" DEBUG=ON)
target_link_libraries(TestDecode fJPG)

