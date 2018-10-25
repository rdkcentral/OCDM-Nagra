
project(DRMNagraSystem)

set(DRM_PLUGIN_NAME "NagraSystem")

set(DRM_PLUGIN_SOURCES 
    MediaSessionSystem.cpp 
    MediaSystem.cpp
    OperatorVault.cpp)

# add the library
add_library(${PROJECT_NAME} SHARED ${DRM_PLUGIN_SOURCES})
#target_compile_definitions(${DRM_PLUGIN_NAME} PRIVATE ${PLAYREADY_FLAGS})
target_link_libraries(${PROJECT_NAME} -lccl ${WPEFRAMEWORK_LIBRARIES})
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".drm")
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/share/WPEFramework/OCDM)
