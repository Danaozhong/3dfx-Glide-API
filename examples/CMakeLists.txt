# Example programs of Glide3x for Voodoo3/4/5.
#
#   teapot/  The Utah teapot, Gouraud shaded and depth buffered. Runs on a
#            Voodoo3/4/5 or on the simulated board (BUILD_HAL_CSIM), and can
#            save the frames as images. Not built for DRI, where Glide is the
#            backend of Mesa.
#   3dfx/    The test programs of the 3dfx Glide3 SDK, built with
#            GLIDE_BUILD_3DFX_EXAMPLES.

if(NOT USE_DRI)
    add_subdirectory(teapot)
endif()

if(GLIDE_BUILD_3DFX_EXAMPLES)
    add_subdirectory(3dfx)
endif()
