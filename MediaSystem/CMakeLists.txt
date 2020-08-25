# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2020 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the License);
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an AS IS BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

project(DRMNagraSystem)

find_package(Nagra REQUIRED)

set(MODULE_NAME NagraSystem)

# add the library
add_library(${MODULE_NAME} SHARED
    MediaSessionSystem.cpp
    MediaSystem.cpp
    OperatorVault.cpp
    ../ParsePSSHHeader.cpp)

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

target_include_directories(${MODULE_NAME}
    PRIVATE
    "${CMAKE_SYSROOT}/usr/include"
    "${CMAKE_SYSROOT}/usr/include/${NAMESPACE}"
    ${NAGRA_INCLUDE_DIRS})

target_link_libraries(${MODULE_NAME}
    ${NAGRA_LIBRARIES}
    ${NAMESPACE}Core::${NAMESPACE}Core)

add_compiler_flags(${MODULE_NAME} "${CORE_DEFINITIONS}")
set_target_properties(${MODULE_NAME} PROPERTIES SUFFIX ".drm")
set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "")

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/share/${NAMESPACE}/OCDM)
