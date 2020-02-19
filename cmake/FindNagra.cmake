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

# - Try to find Nagra
# Once done this will define
#  NAGRA_FOUND - System has Nagra
#  NAGRA_INCLUDE_DIRS - The Nagra include directories
#  NAGRA_LIBRARIES - The libraries needed to use Nagra
#  NAGRA_FLAGS - The flags needed to use Nagra
#

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(NAGRA_LIB_NAME libnagra_cma_dbg)
else()
    set(NAGRA_LIB_NAME libnagra_cma_rel)
endif()

find_package(PkgConfig)
pkg_check_modules(PC_NAGRA REQUIRED ${NAGRA_LIB_NAME})

if(PC_NAGRA_FOUND)
    if(NAGRA_FIND_VERSION AND PC_NAGRA_VERSION)
        if ("${NAGRA_FIND_VERSION}" VERSION_GREATER "${PC_NAGRA_VERSION}")
            message(WARNING "Incorrect version, found ${PC_NAGRA_VERSION}, need at least ${NAGRA_FIND_VERSION}, please install correct version ${NAGRA_FIND_VERSION}")
            set(NAGRA_FOUND_TEXT "Found incorrect version")
            unset(PC_NAGRA_FOUND)
        endif()
    endif()

    if(PC_NAGRA_FOUND)

        # CPFLAGS += -DDRM_BUILD_PROFILE=DRM_BUILD_PROFILE_OEM  
        set(NAGRA_FLAGS ${PC_NAGRA_CFLAGS_OTHER} -DTARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS=0 -DTARGET_LITTLE_ENDIAN=1)
        set(NAGRA_INCLUDE_DIRS ${PC_NAGRA_INCLUDE_DIRS})
        set(NAGRA_LIBRARIES ${PC_NAGRA_LIBRARIES})
        set(NAGRA_LIBRARY_DIRS ${PC_NAGRA_LIBRARY_DIRS})
    endif()
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(NAGRA DEFAULT_MSG NAGRA_LIBRARIES)

mark_as_advanced(
    NAGRA_FOUND
    NAGRA_INCLUDE_DIRS
    NAGRA_LIBRARIES
    NAGRA_LIBRARY_DIRS
    NAGRA_FLAGS)
