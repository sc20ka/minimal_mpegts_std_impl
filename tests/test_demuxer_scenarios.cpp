#include "test_framework.hpp"
#include "test_packet_generator.hpp"
#include "mpegts_demuxer.hpp"

using namespace mpegts;
using namespace test;
using namespace test::scenarios;

// ============================================================================
// Scenario Tests with Various Garbage Conditions
// ============================================================================

TEST(scenario_clean_stream) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    GeneratorConfig gen_config;
    gen_config.pid = 0x100;

    auto scenario = cleanStream(10);
    auto data = gen.generateScenario(scenario, gen_config);

    demuxer.feedData(data.data(), data.size());

    TEST_ASSERT_TRUE(demuxer.isSynchronized(), "Should sync with clean stream");

    auto pids = demuxer.getDiscoveredPIDs();
    TEST_ASSERT_EQ(pids.size(), 1, "Should discover 1 PID");

    auto iterations = demuxer.getIterationsSummary(0x100);
    TEST_ASSERT_EQ(iterations.size(), 10, "Should have 10 iterations");

    return true;
}

TEST(scenario_garbage_prefix) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    GeneratorConfig gen_config;
    gen_config.pid = 0x100;

    // Add 200 bytes of garbage before valid packets
    auto scenario = garbagePrefix(200, 10);
    auto data = gen.generateScenario(scenario, gen_config);

    demuxer.feedData(data.data(), data.size());

    TEST_ASSERT_TRUE(demuxer.isSynchronized(),
                    "Should sync despite garbage prefix");

    auto iterations = demuxer.getIterationsSummary(0x100);
    TEST_ASSERT_TRUE(iterations.size() > 0, "Should extract packets");

    return true;
}

TEST(scenario_garbage_between_packets) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    GeneratorConfig gen_config;
    gen_config.pid = 0x100;

    // Add 10-50 bytes of garbage between each packet
    auto scenario = garbageBetween(10, 50, 10);
    auto data = gen.generateScenario(scenario, gen_config);

    demuxer.feedData(data.data(), data.size());

    TEST_ASSERT_TRUE(demuxer.isSynchronized(),
                    "Should sync with garbage between packets");

    auto iterations = demuxer.getIterationsSummary(0x100);
    TEST_ASSERT_TRUE(iterations.size() > 0, "Should extract some packets");

    return true;
}

TEST(scenario_false_sync_bytes) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    GeneratorConfig gen_config;
    gen_config.pid = 0x100;

    // Add false sync bytes in garbage (10% probability)
    auto scenario = falseSyncBytes(0.1, 10);
    auto data = gen.generateScenario(scenario, gen_config);

    demuxer.feedData(data.data(), data.size());

    // Should still synchronize despite false sync bytes
    TEST_ASSERT_TRUE(demuxer.isSynchronized(),
                    "Should sync despite false sync bytes");

    return true;
}

TEST(scenario_worst_case) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    GeneratorConfig gen_config;
    gen_config.pid = 0x100;

    // Worst case: lots of garbage everywhere + false sync bytes
    auto scenario = worstCase(10);
    auto data = gen.generateScenario(scenario, gen_config);

    demuxer.feedData(data.data(), data.size());

    // Even in worst case, 3-iteration validation should work
    TEST_ASSERT_TRUE(demuxer.isSynchronized(),
                    "Should sync even in worst case");

    auto iterations = demuxer.getIterationsSummary(0x100);
    TEST_ASSERT_TRUE(iterations.size() > 0, "Should extract packets");

    return true;
}

TEST(scenario_minimal_3_packets) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    GeneratorConfig gen_config;
    gen_config.pid = 0x100;

    // Minimal: exactly 3 packets (minimum for 3-iteration validation)
    auto scenario = minimal();
    auto data = gen.generateScenario(scenario, gen_config);

    demuxer.feedData(data.data(), data.size());

    TEST_ASSERT_TRUE(demuxer.isSynchronized(),
                    "Should sync with exactly 3 packets");

    auto iterations = demuxer.getIterationsSummary(0x100);
    TEST_ASSERT_EQ(iterations.size(), 3, "Should have exactly 3 iterations");

    return true;
}

TEST(scenario_insufficient_data) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    GeneratorConfig gen_config;
    gen_config.pid = 0x100;

    // Only 2 packets - not enough for 3-iteration validation
    auto data = gen.generateSequence(2, gen_config);
    demuxer.feedData(data.data(), data.size());

    TEST_ASSERT_FALSE(demuxer.isSynchronized(),
                     "Should NOT sync with only 2 packets");

    return true;
}

TEST(scenario_heavy_garbage_ratio) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    GeneratorConfig gen_config;
    gen_config.pid = 0x100;

    ScenarioConfig scenario;
    scenario.valid_packet_count = 5;
    scenario.garbage_before = 1000;
    scenario.garbage_between_min = 100;
    scenario.garbage_between_max = 200;
    scenario.garbage_after = 1000;
    scenario.false_sync_probability = 0.2; // 20% false sync bytes

    auto data = gen.generateScenario(scenario, gen_config);

    demuxer.feedData(data.data(), data.size());

    // Even with high garbage ratio, should sync
    TEST_ASSERT_TRUE(demuxer.isSynchronized(),
                    "Should sync with heavy garbage ratio");

    return true;
}

TEST(scenario_multiple_streams_with_garbage) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    // Generate two streams with garbage
    GeneratorConfig config1;
    config1.pid = 0x100;

    GeneratorConfig config2;
    config2.pid = 0x101;

    ScenarioConfig scenario;
    scenario.valid_packet_count = 5;
    scenario.garbage_before = 100;
    scenario.garbage_between_min = 10;
    scenario.garbage_between_max = 30;

    auto data1 = gen.generateScenario(scenario, config1);
    auto data2 = gen.generateScenario(scenario, config2);

    // Feed both streams
    demuxer.feedData(data1.data(), data1.size());
    demuxer.feedData(data2.data(), data2.size());

    auto pids = demuxer.getDiscoveredPIDs();
    TEST_ASSERT_TRUE(pids.size() >= 1, "Should discover at least 1 PID");

    return true;
}

TEST(scenario_progressive_feeding) {
    PacketGenerator gen;
    MPEGTSDemuxer demuxer;

    GeneratorConfig gen_config;
    gen_config.pid = 0x100;

    auto scenario = cleanStream(10);
    auto data = gen.generateScenario(scenario, gen_config);

    // Feed data in small chunks (simulate streaming)
    const size_t chunk_size = 100;
    for (size_t offset = 0; offset < data.size(); offset += chunk_size) {
        size_t remaining = std::min(chunk_size, data.size() - offset);
        demuxer.feedData(data.data() + offset, remaining);
    }

    TEST_ASSERT_TRUE(demuxer.isSynchronized(),
                    "Should sync with progressive feeding");

    auto iterations = demuxer.getIterationsSummary(0x100);
    TEST_ASSERT_EQ(iterations.size(), 10, "Should have all 10 iterations");

    return true;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    return TestRegistry::instance().runAll();
}
