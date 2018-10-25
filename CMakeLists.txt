cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

find_package(Nagra REQUIRED)
find_package(WPEFramework REQUIRED)

include_directories(${NAGRA_INCLUDE_DIRS})
include_directories(${WPEFRAMEWORK_INCLUDE_DIR})

add_subdirectory(MediaConnect)
add_subdirectory(MediaSystem)
