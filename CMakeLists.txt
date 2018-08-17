cmake_minimum_required(VERSION 3.1)
project(tinyutf8)

set(CMAKE_CXX_STANDARD 11)

option(TINYUTF8_BUILD_STATIC "Build as static library" On)

if (${TINYUTF8_BUILD_STATIC})
   set(LIB_BUILD_TYPE STATIC)
else()
    set(LIB_BUILD_TYPE SHARED)
endif()

add_library(tinyutf8 ${LIB_BUILD_TYPE} lib/tinyutf8.cpp)

target_include_directories(tinyutf8 PUBLIC include)
