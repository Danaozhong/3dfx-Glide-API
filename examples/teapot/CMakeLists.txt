# The teapot scene is a library, so that tests/csim can render it too.
add_library(teapot_scene STATIC
    scene.cpp
    teapot.cpp
)
target_compile_features(teapot_scene PUBLIC cxx_std_17)
target_include_directories(teapot_scene PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(teapot_scene PUBLIC ${CMAKE_PROJECT_NAME})

add_executable(teapot main.cpp)
target_link_libraries(teapot PRIVATE teapot_scene)
# On the simulator, the program shows the frames in its window.
if(BUILD_HAL_CSIM)
    target_compile_definitions(teapot PRIVATE TEAPOT_CSIM)
endif()

# The Glide DLL has to be next to the program on Windows.
if(WIN32)
    add_custom_command(TARGET teapot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:${CMAKE_PROJECT_NAME}> $<TARGET_FILE_DIR:teapot>
        VERBATIM)
endif()
