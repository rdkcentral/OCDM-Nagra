cmake_minimum_required(VERSION 2.8)
#project(DRMNagra)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

#find_package(Nagra REQUIRED)
find_package(WPEFramework REQUIRED)

#include_directories( "usr/include/Nagra" )

message("found include dirs ${WPEFRAMEWORK_INCLUDE_DIR}")
include_directories(${WPEFRAMEWORK_INCLUDE_DIR})

if(NOT NAGRA_FOUND)

add_subdirectory(MediaConnect)
add_subdirectory(MediaSystem)

endif()


