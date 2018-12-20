cmake_minimum_required(VERSION 2.8)

include(${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/include/cmake/WPEFramework.cmake)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

find_package(Core REQUIRED)
find_package(Interfaces REQUIRED)

include_directories(${CORE_INCLUDE_DIR} ${INTERFACES_INCLUDE_DIR})

add_subdirectory(MediaSystem)
add_subdirectory(MediaConnect)
