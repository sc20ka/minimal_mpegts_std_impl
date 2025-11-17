# Source files for MPEG-TS Demuxer library

set(MPEGTS_SOURCES
    # Demuxer sources
    mpegts_demuxer.cpp
    mpegts_storage.cpp
    mpegts_packet.cpp
    mpegts_psi.cpp
    mpegts_pcr.cpp
    mpegts_pes.cpp
    # Muxer sources
    mpegts_ts_packet_builder.cpp
)

set(MPEGTS_HEADERS
    # Demuxer headers
    ${PROJECT_SOURCE_DIR}/include/mpegts_demuxer.hpp
    ${PROJECT_SOURCE_DIR}/include/mpegts_storage.hpp
    ${PROJECT_SOURCE_DIR}/include/mpegts_packet.hpp
    ${PROJECT_SOURCE_DIR}/include/mpegts_types.hpp
    ${PROJECT_SOURCE_DIR}/include/mpegts_psi.hpp
    ${PROJECT_SOURCE_DIR}/include/mpegts_pcr.hpp
    ${PROJECT_SOURCE_DIR}/include/mpegts_pes.hpp
    # Muxer headers
    ${PROJECT_SOURCE_DIR}/include/mpegts_muxer.hpp
    ${PROJECT_SOURCE_DIR}/include/mpegts_muxer_types.hpp
    ${PROJECT_SOURCE_DIR}/include/mpegts_ts_packet_builder.hpp
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
