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

set(DRM_PLUGIN_SOURCES 
    MediaSessionSystem.cpp 
    MediaSystem.cpp
    OperatorVault.cpp
    ../ParsePSSHHeader.cpp)

find_package(Nagra REQUIRED)

include_directories(${NAGRA_INCLUDE_DIRS})

# add the library
add_library(${PROJECT_NAME} SHARED ${DRM_PLUGIN_SOURCES})
target_link_libraries(${PROJECT_NAME} ${NAGRA_LIBRARIES} ${CORE_LIBRARIES})
add_compiler_flags(${PROJECT_NAME} "${CORE_DEFINITIONS}")
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".drm")
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/share/${NAMESPACE}/OCDM)
