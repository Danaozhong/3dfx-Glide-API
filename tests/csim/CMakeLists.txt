# Integration tests of the Glide3x library against the chip simulator
# (swlibs/csim). They use the public Glide API of the built library, and read
# the statistics and the swapped images of the simulated board.
#
# The timeout of the tests matters here: a register missing in the simulated board
# typically shows up as Glide waiting forever.

glide_add_gtest(csim_smoke_test
    SOURCES csim_smoke_test.cpp
    LIBRARIES glide3x_voodoo345 csim_teapot_scene
)
