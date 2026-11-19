# Tests of Glide, written with GoogleTest. Enabled with GLIDE_BUILD_TESTS, run
# them with ctest.
#
#   csim/  Integration tests of the Glide3x library against the fake chip
#          simulator, only built with BUILD_HAL_CSIM.

include(GlideGoogleTest)

if(BUILD_HAL_CSIM)
    add_subdirectory(csim)
endif()
