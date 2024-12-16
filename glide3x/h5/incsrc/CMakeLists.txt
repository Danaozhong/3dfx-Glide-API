add_library(h3 INTERFACE)
target_include_directories(h3 INTERFACE .)
target_link_libraries(h3 INTERFACE
    fxmisc
    pcilib
)

