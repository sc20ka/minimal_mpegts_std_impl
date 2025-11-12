#include "test_framework.hpp"
#include "mpegts_pes.hpp"
#include <cmath>

using namespace mpegts;
using namespace test;

// ============================================================================
// Timestamp Tests
// ============================================================================

TEST(timestamp_construction) {
    Timestamp ts(90000);

    TEST_ASSERT_EQ(ts.value, 90000, "Timestamp value should be 90000");
    TEST_ASSERT_TRUE(ts.isValid(), "Timestamp should be valid");

    return true;
}

TEST(timestamp_seconds_conversion) {
    Timestamp ts(90000);  // 90000 ticks at 90 kHz = 1 second

    double seconds = ts.getSeconds();
    TEST_ASSERT_TRUE(std::abs(seconds - 1.0) < 0.001, "Should be 1 second");

    return true;
}

TEST(timestamp_milliseconds_conversion) {
    Timestamp ts(9000);  // 9000 ticks at 90 kHz = 100 ms

    double ms = ts.getMilliseconds();
    TEST_ASSERT_TRUE(std::abs(ms - 100.0) < 0.1, "Should be 100 milliseconds");

    return true;
}

TEST(timestamp_validity) {
    Timestamp valid_ts(1000);
    TEST_ASSERT_TRUE(valid_ts.isValid(), "Valid timestamp should pass");

    // 33-bit wraparound check
    Timestamp invalid_ts(1ULL << 34);  // Too large
    TEST_ASSERT_FALSE(invalid_ts.isValid(), "Timestamp >= 2^33 should be invalid");

    return true;
}

TEST(timestamp_difference) {
    Timestamp ts1(1000);
    Timestamp ts2(2000);

    int64_t diff = timestampDifference(ts1, ts2);
    TEST_ASSERT_EQ(diff, 1000, "Difference should be 1000");

    int64_t diff_rev = timestampDifference(ts2, ts1);
    TEST_ASSERT_EQ(diff_rev, -1000, "Reverse difference should be -1000");

    return true;
}

TEST(timestamp_difference_ms) {
    Timestamp ts1(0);
    Timestamp ts2(90);  // 90 ticks at 90 kHz = 1 ms

    double diff_ms = timestampDifferenceMs(ts1, ts2);
    TEST_ASSERT_TRUE(std::abs(diff_ms - 1.0) < 0.01, "Difference should be ~1 ms");

    return true;
}

// ============================================================================
// PES Header Tests
// ============================================================================

TEST(pes_header_stream_type) {
    PESHeader header;

    // Video stream
    header.stream_id = STREAM_ID_VIDEO_STREAM_MIN;
    TEST_ASSERT_TRUE(header.isVideoStream(), "Should be video stream");
    TEST_ASSERT_FALSE(header.isAudioStream(), "Should not be audio stream");

    // Audio stream
    header.stream_id = STREAM_ID_AUDIO_STREAM_MIN;
    TEST_ASSERT_FALSE(header.isVideoStream(), "Should not be video stream");
    TEST_ASSERT_TRUE(header.isAudioStream(), "Should be audio stream");

    return true;
}

TEST(pes_header_size) {
    PESHeader header1;
    header1.has_optional_fields = false;
    TEST_ASSERT_EQ(header1.getHeaderSize(), 6, "Basic header should be 6 bytes");

    PESHeader header2;
    header2.has_optional_fields = true;
    header2.header_data_length = 5;
    TEST_ASSERT_EQ(header2.getHeaderSize(), 14, "Header with 5 bytes data should be 14 bytes");

    return true;
}

// ============================================================================
// PES Parser Tests
// ============================================================================

TEST(pes_parser_verify_start_code) {
    std::vector<uint8_t> valid_data = {0x00, 0x00, 0x01, 0xE0};
    TEST_ASSERT_TRUE(PESParser::verifyStartCode(valid_data.data()),
                    "Should verify valid start code");

    std::vector<uint8_t> invalid_data = {0x00, 0x00, 0x02, 0xE0};
    TEST_ASSERT_FALSE(PESParser::verifyStartCode(invalid_data.data()),
                     "Should reject invalid start code");

    return true;
}

TEST(pes_parser_basic_header) {
    // Create minimal PES header
    std::vector<uint8_t> pes_data;

    // Start code
    pes_data.push_back(0x00);
    pes_data.push_back(0x00);
    pes_data.push_back(0x01);

    // Stream ID (video)
    pes_data.push_back(STREAM_ID_VIDEO_STREAM_MIN);

    // Packet length (10 bytes)
    pes_data.push_back(0x00);
    pes_data.push_back(0x0A);

    // Optional fields
    pes_data.push_back(0x80);  // '10' + flags
    pes_data.push_back(0x00);  // No PTS/DTS
    pes_data.push_back(0x00);  // Header data length = 0

    PESHeader header;
    bool result = PESParser::parseHeader(pes_data.data(), pes_data.size(), header);

    TEST_ASSERT_TRUE(result, "Should parse header successfully");
    TEST_ASSERT_EQ(header.stream_id, STREAM_ID_VIDEO_STREAM_MIN, "Stream ID should match");
    TEST_ASSERT_EQ(header.packet_length, 10, "Packet length should be 10");
    TEST_ASSERT_TRUE(header.has_optional_fields, "Should have optional fields");

    return true;
}

TEST(pes_parser_with_pts) {
    // Create PES header with PTS
    std::vector<uint8_t> pes_data;

    // Start code
    pes_data.push_back(0x00);
    pes_data.push_back(0x00);
    pes_data.push_back(0x01);

    // Stream ID
    pes_data.push_back(STREAM_ID_VIDEO_STREAM_MIN);

    // Packet length
    pes_data.push_back(0x00);
    pes_data.push_back(0x0F);  // 15 bytes

    // Optional header
    pes_data.push_back(0x80);  // '10' + flags
    pes_data.push_back(0x80);  // PTS flag set (10xxxxxx)
    pes_data.push_back(0x05);  // Header data length = 5 (for PTS)

    // PTS (5 bytes) - simple value: 0x00000001
    // Format: '0010' | PTS[32:30] | marker | PTS[29:15] | marker | PTS[14:0] | marker
    pes_data.push_back(0x21);  // '0010' + PTS[32:30]=000 + marker
    pes_data.push_back(0x00);  // PTS[29:22]
    pes_data.push_back(0x01);  // PTS[21:15] + marker
    pes_data.push_back(0x00);  // PTS[14:7]
    pes_data.push_back(0x01);  // PTS[6:0] + marker

    PESHeader header;
    bool result = PESParser::parseHeader(pes_data.data(), pes_data.size(), header);

    TEST_ASSERT_TRUE(result, "Should parse header with PTS");
    TEST_ASSERT_TRUE(header.has_pts, "Should have PTS");
    TEST_ASSERT_TRUE(header.pts.has_value(), "PTS should have value");

    return true;
}

// ============================================================================
// PES Packet Tests
// ============================================================================

TEST(pes_packet_basic) {
    PESPacket packet;

    TEST_ASSERT_EQ(packet.getPayloadSize(), 0, "New packet should have empty payload");
    TEST_ASSERT_FALSE(packet.complete, "New packet should not be complete");

    packet.payload = {0x01, 0x02, 0x03};
    TEST_ASSERT_EQ(packet.getPayloadSize(), 3, "Payload size should be 3");

    return true;
}

TEST(pes_packet_clear) {
    PESPacket packet;
    packet.payload = {0x01, 0x02};
    packet.complete = true;

    packet.clear();

    TEST_ASSERT_EQ(packet.getPayloadSize(), 0, "Payload should be cleared");
    TEST_ASSERT_FALSE(packet.complete, "Complete flag should be reset");

    return true;
}

// ============================================================================
// PES Accumulator Tests
// ============================================================================

TEST(pes_accumulator_basic) {
    PESAccumulator acc;

    // Create simple PES packet
    std::vector<uint8_t> pes_data;

    // Start code
    pes_data.push_back(0x00);
    pes_data.push_back(0x00);
    pes_data.push_back(0x01);

    // Stream ID
    pes_data.push_back(STREAM_ID_AUDIO_STREAM_MIN);

    // Packet length (5 bytes following)
    pes_data.push_back(0x00);
    pes_data.push_back(0x05);

    // Optional header (3 bytes)
    pes_data.push_back(0x80);
    pes_data.push_back(0x00);
    pes_data.push_back(0x00);

    // Payload (2 bytes)
    pes_data.push_back(0xAA);
    pes_data.push_back(0xBB);

    // Add data with PUSI set
    bool complete = acc.addData(pes_data.data(), pes_data.size(), true);

    TEST_ASSERT_TRUE(complete, "Packet should be complete");
    TEST_ASSERT_TRUE(acc.isComplete(), "Accumulator should report complete");

    PESPacket packet;
    bool got_packet = acc.getPacket(packet);

    TEST_ASSERT_TRUE(got_packet, "Should get packet successfully");
    TEST_ASSERT_TRUE(packet.complete, "Packet should be marked complete");

    return true;
}

TEST(pes_accumulator_multi_packet) {
    PESAccumulator acc;

    // First part of PES packet
    std::vector<uint8_t> part1;
    part1.push_back(0x00);
    part1.push_back(0x00);
    part1.push_back(0x01);
    part1.push_back(STREAM_ID_VIDEO_STREAM_MIN);
    part1.push_back(0x00);
    part1.push_back(0x0A);  // 10 bytes following
    part1.push_back(0x80);
    part1.push_back(0x00);
    part1.push_back(0x00);

    bool complete1 = acc.addData(part1.data(), part1.size(), true);
    TEST_ASSERT_FALSE(complete1, "Should not be complete yet");

    // Second part (payload)
    std::vector<uint8_t> part2 = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};

    bool complete2 = acc.addData(part2.data(), part2.size(), false);
    TEST_ASSERT_TRUE(complete2, "Should be complete after second part");

    return true;
}

// ============================================================================
// PES Manager Tests
// ============================================================================

TEST(pes_manager_basic) {
    PESManager manager;

    PESAccumulator& acc1 = manager.getAccumulator(0x100);
    TEST_ASSERT_TRUE(manager.hasAccumulator(0x100), "Should have accumulator for 0x100");

    PESAccumulator& acc2 = manager.getAccumulator(0x200);
    TEST_ASSERT_TRUE(manager.hasAccumulator(0x200), "Should have accumulator for 0x200");

    auto pids = manager.getPIDs();
    TEST_ASSERT_EQ(pids.size(), 2, "Should have 2 PIDs");

    return true;
}

TEST(pes_manager_remove) {
    PESManager manager;

    manager.getAccumulator(0x100);
    TEST_ASSERT_TRUE(manager.hasAccumulator(0x100), "Should have accumulator");

    manager.removeAccumulator(0x100);
    TEST_ASSERT_FALSE(manager.hasAccumulator(0x100), "Should not have accumulator after remove");

    return true;
}

TEST(pes_manager_clear) {
    PESManager manager;

    manager.getAccumulator(0x100);
    manager.getAccumulator(0x200);

    auto pids_before = manager.getPIDs();
    TEST_ASSERT_EQ(pids_before.size(), 2, "Should have 2 PIDs before clear");

    manager.clear();

    auto pids_after = manager.getPIDs();
    TEST_ASSERT_EQ(pids_after.size(), 0, "Should have 0 PIDs after clear");

    return true;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    return TestRegistry::instance().runAll();
}
