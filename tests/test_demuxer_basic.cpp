#include "test_framework.hpp"
#include "test_packet_generator.hpp"
#include "mpegts_demuxer.hpp"

using namespace mpegts;
using namespace test;

// ============================================================================
// Basic Functionality Tests
// ============================================================================

TEST(single_valid_packet) {
    PacketGenerator gen;
    GeneratorConfig config;
    config.pid = 0x100;
    config.starting_cc = 0;

    auto packet = gen.generatePacket(config);

    TEST_ASSERT_EQ(packet.size(), MPEGTS_PACKET_SIZE, "Packet size should be 188 bytes");
    TEST_ASSERT_EQ(packet[0], MPEGTS_SYNC_BYTE, "First byte should be sync byte");

    return true;
}

TEST(clean_stream_synchronization) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    GeneratorConfig config;
    config.pid = 0x100;

    // Generate clean stream with 5 packets
    auto data = gen.generateSequence(5, config);

    std::cout << "  DEBUG: Generated " << data.size() << " bytes\n";
    std::cout << "  DEBUG: First bytes: ";
    test::printHex(data.data(), 16);

    // Feed to demuxer
    demuxer.feedData(data.data(), data.size());

    std::cout << "  DEBUG: Synchronized: " << (demuxer.isSynchronized() ? "YES" : "NO") << "\n";
    std::cout << "  DEBUG: Buffer occupancy: " << demuxer.getBufferOccupancy() << "\n";

    // Should be synchronized
    TEST_ASSERT_TRUE(demuxer.isSynchronized(), "Should synchronize with clean stream");

    // Should discover PID
    auto pids = demuxer.getDiscoveredPIDs();
    std::cout << "  DEBUG: Discovered PIDs: " << pids.size() << "\n";

    std::cout << "  DEBUG: Getting iterations for PID 0x100\n";
    auto iterations = demuxer.getIterationsSummary(0x100);
    std::cout << "  DEBUG: Iterations count: " << iterations.size() << "\n";

    std::cout << "  DEBUG: Getting programs\n";
    auto programs = demuxer.getPrograms();
    std::cout << "  DEBUG: Programs count: " << programs.size() << "\n";

    TEST_ASSERT_EQ(pids.size(), 1, "Should discover 1 PID");
    TEST_ASSERT_TRUE(pids.count(0x100) > 0, "Should discover PID 0x100");

    return true;
}

TEST(multiple_pids) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    // Generate packets for different PIDs
    GeneratorConfig config1;
    config1.pid = 0x100;
    auto data1 = gen.generateSequence(3, config1);

    GeneratorConfig config2;
    config2.pid = 0x101;
    auto data2 = gen.generateSequence(3, config2);

    // Interleave packets
    std::vector<uint8_t> data;
    for (size_t i = 0; i < 3; ++i) {
        data.insert(data.end(),
                   data1.begin() + i * MPEGTS_PACKET_SIZE,
                   data1.begin() + (i + 1) * MPEGTS_PACKET_SIZE);
        data.insert(data.end(),
                   data2.begin() + i * MPEGTS_PACKET_SIZE,
                   data2.begin() + (i + 1) * MPEGTS_PACKET_SIZE);
    }

    demuxer.feedData(data.data(), data.size());

    auto pids = demuxer.getDiscoveredPIDs();
    TEST_ASSERT_EQ(pids.size(), 2, "Should discover 2 PIDs");
    TEST_ASSERT_TRUE(pids.count(0x100) > 0, "Should discover PID 0x100");
    TEST_ASSERT_TRUE(pids.count(0x101) > 0, "Should discover PID 0x101");

    return true;
}

TEST(payload_extraction) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    GeneratorConfig config;
    config.pid = 0x100;
    config.payload_pattern = 0xAA;

    auto data = gen.generateSequence(5, config);
    demuxer.feedData(data.data(), data.size());

    // Get iterations
    auto iterations = demuxer.getIterationsSummary(0x100);
    TEST_ASSERT_TRUE(iterations.size() > 0, "Should have iterations");

    // Get payload
    auto payload = demuxer.getPayload(0x100, iterations[0].iteration_id);
    TEST_ASSERT_TRUE(payload.length > 0, "Payload should not be empty");
    TEST_ASSERT_EQ(payload.data[0], 0xAA, "Payload should have pattern 0xAA");

    return true;
}

TEST(continuity_counter_tracking) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    GeneratorConfig config;
    config.pid = 0x100;

    auto data = gen.generateSequence(10, config);
    demuxer.feedData(data.data(), data.size());

    auto iterations = demuxer.getIterationsSummary(0x100);

    // Check CC progression
    if (iterations.size() >= 2) {
        uint8_t expected_cc = iterations[0].cc_end;
        for (size_t i = 1; i < iterations.size(); ++i) {
            expected_cc = (expected_cc + 1) % 16;
            TEST_ASSERT_EQ(iterations[i].cc_start, expected_cc,
                          "CC should progress sequentially");
        }
    }

    return true;
}

TEST(system_pid_filtering) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    // Generate packets for system PID (PAT)
    GeneratorConfig config;
    config.pid = 0x0000; // PAT

    auto data = gen.generateSequence(5, config);
    demuxer.feedData(data.data(), data.size());

    // System PIDs should be filtered out
    auto pids = demuxer.getDiscoveredPIDs();
    TEST_ASSERT_EQ(pids.size(), 0, "System PIDs should be filtered");

    return true;
}

TEST(private_data_extraction) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    GeneratorConfig config;
    config.pid = 0x100;
    config.include_adaptation = true;
    config.include_private_data = true;

    auto data = gen.generateSequence(3, config);
    demuxer.feedData(data.data(), data.size());

    auto iterations = demuxer.getIterationsSummary(0x100);
    TEST_ASSERT_TRUE(iterations.size() > 0, "Should have iterations");

    // Check for private data
    bool has_private = false;
    for (const auto& iter : iterations) {
        if (iter.payload_private_size > 0) {
            has_private = true;
            break;
        }
    }

    TEST_ASSERT_TRUE(has_private, "Should extract private data");

    return true;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    return TestRegistry::instance().runAll();
}
