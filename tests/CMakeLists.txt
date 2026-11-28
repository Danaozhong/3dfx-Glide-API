# Tests of Glide, written with GoogleTest. Enabled with GLIDE_BUILD_TESTS, run
# them with ctest.
#
#   csim/  Integration tests of the Glide3x library against the chip simulator
#          simulator, only built with BUILD_HAL_CSIM.
#   win32/ Unit tests of the Windows specific code of the Glide3x library.

include(GlideGoogleTest)

if(BUILD_HAL_CSIM)
    add_subdirectory(csim)
endif()

if(WIN32)
    add_subdirectory(win32)
endif()
