# Integration tests of the Glide3x library against the fake chip simulator
# (swlibs/csim). They use the public Glide API of the built library, and read
# the statistics of the fake board.
#
# The timeout of the tests matters here: a register missing in the fake board
# typically shows up as Glide waiting forever.

glide_add_gtest(csim_smoke_test
    SOURCES csim_smoke_test.cpp
    LIBRARIES ${CMAKE_PROJECT_NAME}
)
