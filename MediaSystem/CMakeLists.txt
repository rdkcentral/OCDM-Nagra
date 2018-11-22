project(DRMNagraSystem)

set(DRM_PLUGIN_NAME "NagraSystem")

set(DRM_PLUGIN_SOURCES 
    MediaSessionSystem.cpp 
    MediaSystem.cpp
    OperatorVault.cpp)

find_package(Nagra REQUIRED)

include_directories(${NAGRA_INCLUDE_DIRS})

# add the library
add_library(${PROJECT_NAME} SHARED ${DRM_PLUGIN_SOURCES})
target_link_libraries(${PROJECT_NAME} ${NAGRA_LIBRARIES} ${WPEFRAMEWORK_LIBRARIES})
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".drm")
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/share/WPEFramework/OCDM)
