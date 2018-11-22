cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

find_package(WPEFramework REQUIRED)

include_directories(${WPEFRAMEWORK_INCLUDE_DIR})

add_subdirectory(MediaSystem)
add_subdirectory(MediaConnect)
