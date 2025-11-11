# Examples for MPEG-TS Demuxer

# Basic example
add_executable(basic_example
    basic_example.cpp
)

target_link_libraries(basic_example PRIVATE
    mpegts_demuxer
)

# Set output directory
set_target_properties(basic_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
