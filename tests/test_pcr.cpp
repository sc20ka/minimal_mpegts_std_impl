#include "test_framework.hpp"
#include "mpegts_pcr.hpp"
#include <cmath>

using namespace mpegts;
using namespace test;

// ============================================================================
// PCR Structure Tests
// ============================================================================

TEST(pcr_value_calculation) {
    // Test PCR value calculation
    PCR pcr(100, 50);  // base = 100, extension = 50

    uint64_t value_27mhz = pcr.getValue27MHz();
    uint64_t expected_27mhz = 100 * 300 + 50;

    TEST_ASSERT_EQ(value_27mhz, expected_27mhz, "27MHz value should be base*300 + ext");

    uint64_t value_90khz = pcr.getValue90kHz();
    TEST_ASSERT_EQ(value_90khz, 100, "90kHz value should equal base");

    return true;
}

TEST(pcr_seconds_conversion) {
    // Test PCR to seconds conversion
    PCR pcr(90000, 0);  // 90000 ticks at 90kHz = 1 second

    double seconds = pcr.getSeconds();
    double expected = 90000.0 * 300.0 / 27000000.0;  // Should be 1.0 second

    TEST_ASSERT_TRUE(std::abs(seconds - expected) < 0.001, "Should convert to seconds correctly");

    return true;
}

TEST(pcr_validity) {
    // Test PCR validation
    PCR valid_pcr(1000, 50);
    TEST_ASSERT_TRUE(valid_pcr.isValid(), "Valid PCR should pass validation");

    // PCR_base must be < 2^33
    PCR invalid_base(1ULL << 34, 50);
    TEST_ASSERT_FALSE(invalid_base.isValid(), "PCR with base >= 2^33 should be invalid");

    // PCR_extension must be < 300
    PCR invalid_ext(1000, 300);
    TEST_ASSERT_FALSE(invalid_ext.isValid(), "PCR with extension >= 300 should be invalid");

    return true;
}

// ============================================================================
// PCR Utilities Tests
// ============================================================================

TEST(pcr_difference) {
    // Test PCR difference calculation
    PCR pcr1(1000, 0);
    PCR pcr2(2000, 0);

    int64_t diff = pcrDifference(pcr1, pcr2);
    int64_t expected = (2000 - 1000) * 300;

    TEST_ASSERT_EQ(diff, expected, "PCR difference should be correct");

    // Test reverse
    int64_t diff_rev = pcrDifference(pcr2, pcr1);
    TEST_ASSERT_EQ(diff_rev, -expected, "Reverse PCR difference should be negative");

    return true;
}

TEST(pcr_difference_milliseconds) {
    // Test PCR difference in milliseconds
    PCR pcr1(0, 0);
    PCR pcr2(90000, 0);  // 90000 ticks at 90kHz = 1 second

    double diff_ms = pcrDifferenceMs(pcr1, pcr2);
    double expected_ms = 1000.0;  // 1 second = 1000 ms

    TEST_ASSERT_TRUE(std::abs(diff_ms - expected_ms) < 1.0,
                    "PCR difference should be ~1000ms");

    return true;
}

TEST(pcr_extract_from_adaptation) {
    // Test PCR extraction from adaptation field data

    // Create adaptation field with PCR flag set
    std::vector<uint8_t> adapt_field;

    // Flags byte: PCR flag = bit 4 (0x10)
    adapt_field.push_back(0x10);  // PCR flag set

    // PCR value: base = 0x123456789 (33 bits), extension = 0x1AA (9 bits)
    // For simplicity, use base = 0x000000001 (lower 33 bits), ext = 0x100

    // PCR encoding (6 bytes total):
    // base[32..25] | base[24..17] | base[16..9] | base[8..1] | base[0] reserved[5:0] ext[8] | ext[7:0]

    // Simple example: base = 0x1, ext = 0x100
    // base = 0x00000001 (33 bits)
    adapt_field.push_back(0x00);  // base[32:25]
    adapt_field.push_back(0x00);  // base[24:17]
    adapt_field.push_back(0x00);  // base[16:9]
    adapt_field.push_back(0x00);  // base[8:1]
    adapt_field.push_back(0x81);  // base[0] + reserved + ext[8] = 1000 0001
    adapt_field.push_back(0x00);  // ext[7:0]

    auto pcr_opt = extractPCR(adapt_field.data(), adapt_field.size());

    TEST_ASSERT_TRUE(pcr_opt.has_value(), "Should extract PCR successfully");

    PCR pcr = pcr_opt.value();
    TEST_ASSERT_EQ(pcr.base, 1, "PCR base should be 1");
    TEST_ASSERT_EQ(pcr.extension, 256, "PCR extension should be 256");

    return true;
}

TEST(pcr_extract_no_flag) {
    // Test extraction when PCR flag is not set
    std::vector<uint8_t> adapt_field;
    adapt_field.push_back(0x00);  // No PCR flag

    for (int i = 0; i < 6; ++i) {
        adapt_field.push_back(0x00);
    }

    auto pcr_opt = extractPCR(adapt_field.data(), adapt_field.size());

    TEST_ASSERT_FALSE(pcr_opt.has_value(), "Should not extract PCR when flag not set");

    return true;
}

// ============================================================================
// PCR Tracker Tests
// ============================================================================

TEST(pcr_tracker_basic) {
    PCRTracker tracker(0x100);

    // Add some PCR samples
    PCR pcr1(90000, 0);   // 1 second
    PCR pcr2(180000, 0);  // 2 seconds
    PCR pcr3(270000, 0);  // 3 seconds

    tracker.addPCR(pcr1, 0, 0);
    tracker.addPCR(pcr2, 100, 1);
    tracker.addPCR(pcr3, 200, 2);

    // Get stats
    auto stats = tracker.getStats();

    TEST_ASSERT_EQ(stats.pid, 0x100, "PID should be 0x100");
    TEST_ASSERT_EQ(stats.pcr_count, 3, "Should have 3 PCR samples");
    TEST_ASSERT_TRUE(stats.first_pcr.has_value(), "Should have first PCR");
    TEST_ASSERT_TRUE(stats.last_pcr.has_value(), "Should have last PCR");

    return true;
}

TEST(pcr_tracker_last_pcr) {
    PCRTracker tracker(0x200);

    PCR pcr1(1000, 0);
    PCR pcr2(2000, 0);

    tracker.addPCR(pcr1, 0, 0);
    tracker.addPCR(pcr2, 1, 1);

    auto last = tracker.getLastPCR();

    TEST_ASSERT_TRUE(last.has_value(), "Should have last PCR");
    TEST_ASSERT_EQ(last->base, 2000, "Last PCR base should be 2000");

    return true;
}

TEST(pcr_tracker_samples) {
    PCRTracker tracker(0x300);

    for (int i = 0; i < 10; ++i) {
        PCR pcr(i * 1000, 0);
        tracker.addPCR(pcr, i, i);
    }

    const auto& samples = tracker.getSamples();

    TEST_ASSERT_EQ(samples.size(), 10, "Should have 10 samples");
    TEST_ASSERT_EQ(samples[0].pcr.base, 0, "First sample should have base 0");
    TEST_ASSERT_EQ(samples[9].pcr.base, 9000, "Last sample should have base 9000");

    return true;
}

// ============================================================================
// PCR Manager Tests
// ============================================================================

TEST(pcr_manager_add_and_retrieve) {
    PCRManager manager;

    PCR pcr1(1000, 0);
    PCR pcr2(2000, 0);

    manager.addPCR(0x100, pcr1, 0, 0);
    manager.addPCR(0x200, pcr2, 1, 1);

    auto pids = manager.getPIDsWithPCR();
    TEST_ASSERT_EQ(pids.size(), 2, "Should have 2 PIDs with PCR");

    auto tracker1 = manager.getTracker(0x100);
    TEST_ASSERT_TRUE(tracker1 != nullptr, "Should find tracker for PID 0x100");

    auto tracker2 = manager.getTracker(0x200);
    TEST_ASSERT_TRUE(tracker2 != nullptr, "Should find tracker for PID 0x200");

    return true;
}

TEST(pcr_manager_all_stats) {
    PCRManager manager;

    for (uint16_t pid = 0x100; pid < 0x103; ++pid) {
        PCR pcr(pid * 100, 0);
        manager.addPCR(pid, pcr, 0, 0);
    }

    auto all_stats = manager.getAllStats();

    TEST_ASSERT_EQ(all_stats.size(), 3, "Should have stats for 3 PIDs");

    return true;
}

TEST(pcr_manager_clear) {
    PCRManager manager;

    PCR pcr(1000, 0);
    manager.addPCR(0x100, pcr, 0, 0);

    auto pids_before = manager.getPIDsWithPCR();
    TEST_ASSERT_EQ(pids_before.size(), 1, "Should have 1 PID before clear");

    manager.clear();

    auto pids_after = manager.getPIDsWithPCR();
    TEST_ASSERT_EQ(pids_after.size(), 0, "Should have 0 PIDs after clear");

    return true;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    return TestRegistry::instance().runAll();
}
