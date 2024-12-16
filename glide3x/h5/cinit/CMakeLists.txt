add_library(cinit
    h3cinit.c
)
target_include_directories(cinit PUBLIC .)
target_link_libraries(cinit PUBLIC
    fxmisc
    h3
)

