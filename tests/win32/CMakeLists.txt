# Unit tests of the Windows specific code of the Glide3x library. They reach
# internal functions, so they are linked with the objects of the library
# instead of the DLL.

glide_add_gtest(thread_storage_test
    SOURCES thread_storage_test.cpp
            thread_storage_access.c
            $<TARGET_OBJECTS:glide_objects>
    LIBRARIES $<TARGET_PROPERTY:${CMAKE_PROJECT_NAME},LINK_LIBRARIES>
)
target_include_directories(thread_storage_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_dependencies(thread_storage_test glide_objects)

if(MINGW)
    # Runs without the DLLs of the MinGW runtime, e.g. with Wine.
    target_link_options(thread_storage_test PRIVATE -static)
endif()
