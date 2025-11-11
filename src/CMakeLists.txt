# Source files for MPEG-TS Demuxer library

set(MPEGTS_SOURCES
    mpegts_demuxer.cpp
    mpegts_storage.cpp
    mpegts_packet.cpp
)

set(MPEGTS_HEADERS
    ${PROJECT_SOURCE_DIR}/include/mpegts_demuxer.hpp
    ${PROJECT_SOURCE_DIR}/include/mpegts_storage.hpp
    ${PROJECT_SOURCE_DIR}/include/mpegts_packet.hpp
    ${PROJECT_SOURCE_DIR}/include/mpegts_types.hpp
)

# Create static library
add_library(mpegts_demuxer STATIC
    ${MPEGTS_SOURCES}
    ${MPEGTS_HEADERS}
)

target_include_directories(mpegts_demuxer PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# Set library properties
set_target_properties(mpegts_demuxer PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
