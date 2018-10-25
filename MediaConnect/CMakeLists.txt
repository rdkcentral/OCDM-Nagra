project(DRMNagraConnect)


set(DRM_PLUGIN_NAME "NagraConnect")



set(DRM_PLUGIN_SOURCES 
    MediaSessionConnect.cpp 
    MediaConnect.cpp)


# add the library
add_library(${PROJECT_NAME} SHARED ${DRM_PLUGIN_SOURCES})
#target_compile_definitions(${DRM_PLUGIN_NAME} PRIVATE ${PLAYREADY_FLAGS})
#target_include_directories(${DRM_PLUGIN_NAME} PRIVATE ${PLAYREADY_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} -lccl ${WPEFRAMEWORK_LIBRARIES})
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".drm")
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/share/WPEFramework/OCDM)
