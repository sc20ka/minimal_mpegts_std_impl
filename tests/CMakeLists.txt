# Test suite for MPEG-TS Demuxer

# Test utilities library
add_library(test_utils STATIC
    test_packet_generator.cpp
    test_framework.cpp
)

target_link_libraries(test_utils PUBLIC
    mpegts_demuxer
)

target_include_directories(test_utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Test executables
add_executable(test_demuxer_basic
    test_demuxer_basic.cpp
)

add_executable(test_demuxer_scenarios
    test_demuxer_scenarios.cpp
)

add_executable(test_synchronization
    test_synchronization.cpp
)

add_executable(test_psi_tables
    test_psi_tables.cpp
)

add_executable(test_pcr
    test_pcr.cpp
)

add_executable(test_pes
    test_pes.cpp
)

# Link tests with library
target_link_libraries(test_demuxer_basic PRIVATE
    mpegts_demuxer
    test_utils
)

target_link_libraries(test_demuxer_scenarios PRIVATE
    mpegts_demuxer
    test_utils
)

target_link_libraries(test_synchronization PRIVATE
    mpegts_demuxer
    test_utils
)

target_link_libraries(test_psi_tables PRIVATE
    mpegts_demuxer
    test_utils
)

target_link_libraries(test_pcr PRIVATE
    mpegts_demuxer
    test_utils
)

target_link_libraries(test_pes PRIVATE
    mpegts_demuxer
    test_utils
)

# Set output directory
set_target_properties(
    test_demuxer_basic
    test_demuxer_scenarios
    test_synchronization
    test_psi_tables
    test_pcr
    test_pes
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

# Add tests to CTest
enable_testing()
add_test(NAME BasicDemuxerTests COMMAND test_demuxer_basic)
add_test(NAME ScenarioTests COMMAND test_demuxer_scenarios)
add_test(NAME SynchronizationTests COMMAND test_synchronization)
add_test(NAME PSITableTests COMMAND test_psi_tables)
add_test(NAME PCRTests COMMAND test_pcr)
add_test(NAME PESTests COMMAND test_pes)
