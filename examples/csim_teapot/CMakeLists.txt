# The teapot scene is a library, so that tests/csim can render it too.
add_library(csim_teapot_scene STATIC
    scene.cpp
    teapot.cpp
)
target_compile_features(csim_teapot_scene PUBLIC cxx_std_17)
target_include_directories(csim_teapot_scene PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(csim_teapot_scene PUBLIC ${CMAKE_PROJECT_NAME})

add_executable(csim_teapot main.cpp)
target_link_libraries(csim_teapot PRIVATE csim_teapot_scene)

# The Glide DLL has to be next to the program on Windows.
if(WIN32)
    add_custom_command(TARGET csim_teapot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:${CMAKE_PROJECT_NAME}> $<TARGET_FILE_DIR:csim_teapot>
        VERBATIM)
endif()
