cmake_minimum_required(VERSION 3.1)

set(CMAKE_CXX_STANDARD 11)

add_executable(tinyutf8_test test.cpp)
target_link_libraries(tinyutf8_test tinyutf8)

configure_file(input.txt input.txt COPYONLY)