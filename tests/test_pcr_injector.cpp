#include "test_framework.hpp"
#include "mpegts_pcr_injector.hpp"
#include "mpegts_pcr.hpp"

using namespace mpegts;

// Helper function to decode PCR from encoded bytes
PCR decodePCR(const uint8_t* data) {
    // Decode PCR_base (33 bits)
    uint64_t base = 0;
    base |= ((uint64_t)data[0]) << 25;
    base |= ((uint64_t)data[1]) << 17;
    base |= ((uint64_t)data[2]) << 9;
    base |= ((uint64_t)data[3]) << 1;
    base |= ((uint64_t)(data[4] >> 7)) & 0x01;

    // Decode PCR_extension (9 bits)
    uint16_t ext = 0;
    ext |= ((uint16_t)(data[4] & 0x01)) << 8;
    ext |= (uint16_t)data[5];

    return PCR(base, ext);
}

// ============================================================================
// Constructor and Configuration Tests
// ============================================================================

TEST(pcr_injector_construction_default) {
    PCRInjector injector;

    TEST_ASSERT_EQ(injector.getInterval(), 40, "Default interval should be 40ms");
    TEST_ASSERT_EQ(injector.getInjectionCount(), 0, "Initial count should be 0");

    return true;
}

TEST(pcr_injector_construction_custom) {
    PCRInjector injector(20);

    TEST_ASSERT_EQ(injector.getInterval(), 20, "Custom interval should be 20ms");

    return true;
}

TEST(pcr_injector_set_interval) {
    PCRInjector injector;

    injector.setInterval(50);
    TEST_ASSERT_EQ(injector.getInterval(), 50, "Interval should be updated");

    return true;
}

TEST(pcr_injector_invalid_interval_low) {
    PCRInjector injector;

    bool threw = false;
    try {
        injector.setInterval(5);  // Too low
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    TEST_ASSERT_TRUE(threw, "Should throw for interval < 10ms");

    return true;
}

TEST(pcr_injector_invalid_interval_high) {
    PCRInjector injector;

    bool threw = false;
    try {
        injector.setInterval(150);  // Too high
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    TEST_ASSERT_TRUE(threw, "Should throw for interval > 100ms");

    return true;
}

// ============================================================================
// PCR Calculation Tests
// ============================================================================

TEST(pcr_injector_calculate_pcr_from_pts) {
    uint64_t pts = 90000;  // 1 second at 90kHz

    PCR pcr = PCRInjector::calculatePCR(pts);

    TEST_ASSERT_EQ(pcr.base, 90000, "PCR base should equal PTS");
    TEST_ASSERT_EQ(pcr.extension, 0, "Extension should be 0 for 90kHz input");

    return true;
}

TEST(pcr_injector_calculate_pcr_from_27mhz) {
    uint64_t timestamp_27mhz = 27000000;  // 1 second at 27MHz

    PCR pcr = PCRInjector::calculatePCRFrom27MHz(timestamp_27mhz);

    // 27000000 / 300 = 90000
    // 27000000 % 300 = 0
    TEST_ASSERT_EQ(pcr.base, 90000, "PCR base should be 90000");
    TEST_ASSERT_EQ(pcr.extension, 0, "PCR extension should be 0");

    return true;
}

TEST(pcr_injector_calculate_pcr_with_extension) {
    // 27MHz timestamp that doesn't divide evenly by 300
    uint64_t timestamp_27mhz = 27000150;  // 1 second + 150 ticks

    PCR pcr = PCRInjector::calculatePCRFrom27MHz(timestamp_27mhz);

    // 27000150 / 300 = 90000
    // 27000150 % 300 = 150
    TEST_ASSERT_EQ(pcr.base, 90000, "PCR base should be 90000");
    TEST_ASSERT_EQ(pcr.extension, 150, "PCR extension should be 150");

    return true;
}

TEST(pcr_injector_calculate_pcr_large_value) {
    // Large value close to 33-bit limit
    uint64_t pts = (1ULL << 32);  // 2^32

    PCR pcr = PCRInjector::calculatePCR(pts);

    TEST_ASSERT_TRUE(pcr.base < (1ULL << 33), "PCR base should be within 33 bits");

    return true;
}

// ============================================================================
// PCR Encoding Tests
// ============================================================================

TEST(pcr_injector_encode_simple) {
    PCR pcr(90000, 0);  // 1 second

    auto encoded = PCRInjector::encodePCR(pcr);

    TEST_ASSERT_EQ(encoded.size(), 6, "Encoded PCR should be 6 bytes");

    // Decode and verify
    PCR decoded = decodePCR(encoded.data());
    TEST_ASSERT_EQ(decoded.base, pcr.base, "Decoded base should match");
    TEST_ASSERT_EQ(decoded.extension, pcr.extension, "Decoded extension should match");

    return true;
}

TEST(pcr_injector_encode_with_extension) {
    PCR pcr(90000, 150);

    auto encoded = PCRInjector::encodePCR(pcr);

    // Decode and verify
    PCR decoded = decodePCR(encoded.data());
    TEST_ASSERT_EQ(decoded.base, 90000, "Decoded base should be 90000");
    TEST_ASSERT_EQ(decoded.extension, 150, "Decoded extension should be 150");

    return true;
}

TEST(pcr_injector_encode_zero) {
    PCR pcr(0, 0);

    auto encoded = PCRInjector::encodePCR(pcr);

    // Check reserved bits are set correctly (0x7E = 01111110)
    TEST_ASSERT_EQ((encoded[4] & 0x7E), 0x7E, "Reserved bits should be 0x7E");

    PCR decoded = decodePCR(encoded.data());
    TEST_ASSERT_EQ(decoded.base, 0, "Decoded base should be 0");
    TEST_ASSERT_EQ(decoded.extension, 0, "Decoded extension should be 0");

    return true;
}

TEST(pcr_injector_encode_max_extension) {
    PCR pcr(90000, 299);  // Max extension value

    auto encoded = PCRInjector::encodePCR(pcr);

    PCR decoded = decodePCR(encoded.data());
    TEST_ASSERT_EQ(decoded.base, 90000, "Decoded base should match");
    TEST_ASSERT_EQ(decoded.extension, 299, "Decoded extension should be 299");

    return true;
}

// ============================================================================
// PCR Injection Timing Tests
// ============================================================================

TEST(pcr_injector_should_inject_first) {
    PCRInjector injector(40);

    // First PCR should always be injected
    bool should_inject = injector.shouldInjectPCR(0);

    TEST_ASSERT_TRUE(should_inject, "First PCR should always be injected");

    // Record the injection
    injector.recordInjection(0);
    TEST_ASSERT_EQ(injector.getInjectionCount(), 1, "Count should be 1 after injection");

    return true;
}

TEST(pcr_injector_should_not_inject_too_soon) {
    PCRInjector injector(40);  // 40ms = 3600 ticks at 90kHz

    // Inject first PCR
    TEST_ASSERT_TRUE(injector.shouldInjectPCR(0), "First should inject");
    injector.recordInjection(0);

    // Try to inject after only 1ms (90 ticks)
    bool should_inject = injector.shouldInjectPCR(90);

    TEST_ASSERT_FALSE(should_inject, "Should not inject before interval");

    return true;
}

TEST(pcr_injector_should_inject_after_interval) {
    PCRInjector injector(40);  // 40ms = 3600 ticks at 90kHz

    // Inject first PCR at time 0
    TEST_ASSERT_TRUE(injector.shouldInjectPCR(0), "First should inject");
    injector.recordInjection(0);

    // Try to inject after exactly 40ms (3600 ticks)
    bool should_inject = injector.shouldInjectPCR(3600);

    TEST_ASSERT_TRUE(should_inject, "Should inject after interval");

    return true;
}

// ============================================================================
// PCR Difference Calculation Tests
// ============================================================================

TEST(pcr_injector_pcr_difference_simple) {
    PCR pcr1(90000, 0);    // 1 second
    PCR pcr2(180000, 0);   // 2 seconds

    double diff_ms = PCRInjector::calculatePCRDifferenceMs(pcr1, pcr2);

    // Difference = 90000 * 300 = 27000000 ticks at 27MHz
    // 27000000 / 27000 = 1000 ms
    TEST_ASSERT_TRUE(diff_ms >= 999.9 && diff_ms <= 1000.1,
                     "Difference should be approximately 1000ms");

    return true;
}

TEST(pcr_injector_pcr_difference_with_extension) {
    PCR pcr1(90000, 100);
    PCR pcr2(90000, 200);

    double diff_ms = PCRInjector::calculatePCRDifferenceMs(pcr1, pcr2);

    // Difference = 100 ticks at 27MHz
    // 100 / 27000 ≈ 0.0037 ms
    TEST_ASSERT_TRUE(diff_ms >= 0.003 && diff_ms <= 0.004,
                     "Difference should be small");

    return true;
}

TEST(pcr_injector_pcr_difference_40ms) {
    PCR pcr1(0, 0);
    PCR pcr2(3600, 0);  // 40ms at 90kHz = 3600 ticks

    double diff_ms = PCRInjector::calculatePCRDifferenceMs(pcr1, pcr2);

    TEST_ASSERT_TRUE(diff_ms >= 39.9 && diff_ms <= 40.1,
                     "Difference should be approximately 40ms");

    return true;
}

// ============================================================================
// PCR Validation Tests
// ============================================================================

TEST(pcr_injector_validate_pcr_valid) {
    PCR pcr(90000, 150);

    TEST_ASSERT_TRUE(PCRInjector::isValidPCR(pcr), "Valid PCR should pass");

    return true;
}

TEST(pcr_injector_validate_pcr_invalid_base) {
    PCR pcr(1ULL << 33, 0);  // Base too large (33-bit max)

    TEST_ASSERT_FALSE(PCRInjector::isValidPCR(pcr), "Invalid base should fail");

    return true;
}

TEST(pcr_injector_validate_pcr_invalid_extension) {
    PCR pcr(90000, 300);  // Extension too large (max 299)

    TEST_ASSERT_FALSE(PCRInjector::isValidPCR(pcr), "Invalid extension should fail");

    return true;
}

// ============================================================================
// PCR State Management Tests
// ============================================================================

TEST(pcr_injector_reset) {
    PCRInjector injector;

    // Simulate some injections
    TEST_ASSERT_TRUE(injector.shouldInjectPCR(0), "First should inject");
    injector.recordInjection(0);
    TEST_ASSERT_EQ(injector.getInjectionCount(), 1, "Count should be 1");

    // Reset
    injector.reset();

    TEST_ASSERT_EQ(injector.getInjectionCount(), 0, "Count should be reset");
    TEST_ASSERT_FALSE(injector.getLastPCR().has_value(), "Last PCR should be cleared");

    return true;
}

TEST(pcr_injector_get_last_pcr_none) {
    PCRInjector injector;

    auto last_pcr = injector.getLastPCR();

    TEST_ASSERT_FALSE(last_pcr.has_value(), "Should have no last PCR initially");

    return true;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    return test::TestRegistry::instance().runAll();
}
