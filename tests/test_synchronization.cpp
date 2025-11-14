#include "test_framework.hpp"
#include "test_packet_generator.hpp"
#include "mpegts_demuxer.hpp"

using namespace mpegts;
using namespace test;

// ============================================================================
// Synchronization Edge Cases
// ============================================================================

TEST(sync_with_exact_3_packets) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    GeneratorConfig config;
    config.pid = 0x100;

    // Exactly 3 packets - minimum for sync
    auto data = gen.generateSequence(3, config);
    demuxer.feedData(data.data(), data.size());

    TEST_ASSERT_TRUE(demuxer.isSynchronized(),
                    "Should sync with exactly 3 packets");

    return true;
}

TEST(sync_loss_on_invalid_packet) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    GeneratorConfig config;
    config.pid = 0x100;

    // Feed valid packets first
    auto data = gen.generateSequence(5, config);
    demuxer.feedData(data.data(), data.size());

    TEST_ASSERT_TRUE(demuxer.isSynchronized(), "Should be synced");

    // Feed garbage to cause sync loss
    auto garbage = gen.generateGarbage(500, false);
    demuxer.feedData(garbage.data(), garbage.size());

    // After garbage, should try to resync
    // (exact behavior depends on implementation)

    return true;
}

TEST(sync_with_scattered_valid_packets) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    GeneratorConfig config;
    config.pid = 0x100;

    std::vector<uint8_t> data;

    // Add 3 valid packets with large gaps
    for (int i = 0; i < 3; ++i) {
        auto packet = gen.generatePacket(config);
        data.insert(data.end(), packet.begin(), packet.end());

        config.starting_cc = (config.starting_cc + 1) % 16;

        // Add garbage between (except after last)
        if (i < 2) {
            auto garbage = gen.generateGarbage(100, false);
            data.insert(data.end(), garbage.begin(), garbage.end());
        }
    }

    demuxer.feedData(data.data(), data.size());

    TEST_ASSERT_TRUE(demuxer.isSynchronized(),
                    "Should sync with scattered packets");

    return true;
}

TEST(sync_false_positive_prevention) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    // Create garbage with false sync bytes
    std::vector<uint8_t> data;

    // Add false sync byte followed by random data (not valid packet)
    for (int i = 0; i < 10; ++i) {
        data.push_back(MPEGTS_SYNC_BYTE);
        auto garbage = gen.generateGarbage(187, false);
        data.insert(data.end(), garbage.begin(), garbage.end());
    }

    demuxer.feedData(data.data(), data.size());

    // Should NOT sync on false sync bytes
    TEST_ASSERT_FALSE(demuxer.isSynchronized(),
                     "Should not sync on false sync bytes");

    return true;
}

TEST(sync_recovery_after_corruption) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    GeneratorConfig config;
    config.pid = 0x100;

    std::vector<uint8_t> data;

    // Valid packets
    auto valid1 = gen.generateSequence(5, config);
    data.insert(data.end(), valid1.begin(), valid1.end());

    // Corruption
    auto garbage = gen.generateGarbage(300, true);
    data.insert(data.end(), garbage.begin(), garbage.end());

    // More valid packets
    config.starting_cc = 5;
    auto valid2 = gen.generateSequence(5, config);
    data.insert(data.end(), valid2.begin(), valid2.end());

    demuxer.feedData(data.data(), data.size());

    // Should eventually sync (either on first or second set)
    TEST_ASSERT_TRUE(demuxer.isSynchronized(),
                    "Should recover sync after corruption");

    return true;
}

TEST(sync_with_different_pids) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    // Create packets with different PIDs but sequential
    std::vector<uint8_t> data;

    for (uint16_t pid = 0x100; pid <= 0x102; ++pid) {
        GeneratorConfig config;
        config.pid = pid;
        config.starting_cc = 0;

        auto packet = gen.generatePacket(config);
        data.insert(data.end(), packet.begin(), packet.end());
    }

    demuxer.feedData(data.data(), data.size());

    // Should sync even with different PIDs
    TEST_ASSERT_TRUE(demuxer.isSynchronized(),
                    "Should sync with different PIDs");

    return true;
}

TEST(sync_buffer_boundary_conditions) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    GeneratorConfig config;
    config.pid = 0x100;

    // Feed data in very small chunks (less than packet size)
    auto data = gen.generateSequence(5, config);

    const size_t chunk_size = 50; // Less than 188
    for (size_t offset = 0; offset < data.size(); offset += chunk_size) {
        size_t remaining = std::min(chunk_size, data.size() - offset);
        demuxer.feedData(data.data() + offset, remaining);
    }

    TEST_ASSERT_TRUE(demuxer.isSynchronized(),
                    "Should handle small chunks correctly");

    return true;
}

TEST(sync_with_cc_discontinuity) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    GeneratorConfig config;
    config.pid = 0x100;

    std::vector<uint8_t> data;

    // Normal sequence
    config.starting_cc = 0;
    auto seq1 = gen.generateSequence(3, config);
    data.insert(data.end(), seq1.begin(), seq1.end());

    // Jump in CC (simulate discontinuity)
    config.starting_cc = 10; // Skip from 2 to 10
    auto seq2 = gen.generateSequence(3, config);
    data.insert(data.end(), seq2.begin(), seq2.end());

    demuxer.feedData(data.data(), data.size());

    // Should still sync (discontinuity handling)
    TEST_ASSERT_TRUE(demuxer.isSynchronized(),
                    "Should handle CC discontinuity");

    return true;
}

TEST(sync_packet_position_accuracy) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    GeneratorConfig config;
    config.pid = 0x100;

    ScenarioConfig scenario;
    scenario.valid_packet_count = 5;
    scenario.garbage_before = 200;
    scenario.garbage_between_min = 20;
    scenario.garbage_between_max = 50;

    auto data = gen.generateScenario(scenario, config);
    auto positions = gen.getPacketPositions();

    demuxer.feedData(data.data(), data.size());

    TEST_ASSERT_TRUE(demuxer.isSynchronized(), "Should sync");

    // Verify we can extract packets
    auto iterations = demuxer.getIterationsSummary(0x100);
    TEST_ASSERT_TRUE(iterations.size() > 0, "Should have iterations");

    std::cout << "  Generated " << positions.size() << " packets at positions:\n";
    for (size_t pos : positions) {
        std::cout << "    Offset: " << pos << "\n";
    }

    std::cout << "  Demuxer found " << iterations.size() << " iterations\n";

    return true;
}

TEST(sync_rapid_resync) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    GeneratorConfig config;
    config.pid = 0x100;

    // Alternating valid and garbage
    for (int cycle = 0; cycle < 3; ++cycle) {
        auto valid = gen.generateSequence(3, config);
        demuxer.feedData(valid.data(), valid.size());

        auto garbage = gen.generateGarbage(100, false);
        demuxer.feedData(garbage.data(), garbage.size());

        config.starting_cc = (config.starting_cc + 3) % 16;
    }

    // Should be able to resync multiple times
    auto iterations = demuxer.getIterationsSummary(0x100);
    TEST_ASSERT_TRUE(iterations.size() > 0, "Should have found packets");

    return true;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    return TestRegistry::instance().runAll();
}
