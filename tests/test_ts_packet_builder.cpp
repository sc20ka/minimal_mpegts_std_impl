#include "test_framework.hpp"
#include "mpegts_ts_packet_builder.hpp"
#include "mpegts_packet.hpp"
#include <cstring>

using namespace mpegts;
using namespace test;

// ============================================================================
// Helper Functions
// ============================================================================

bool validatePacketStructure(const std::vector<uint8_t>& packet) {
    if (packet.size() != MPEGTS_PACKET_SIZE) return false;
    if (packet[0] != MPEGTS_SYNC_BYTE) return false;
    return true;
}

uint8_t extractCC(const std::vector<uint8_t>& packet) {
    return packet[3] & 0x0F;
}

uint16_t extractPID(const std::vector<uint8_t>& packet) {
    return ((packet[1] & 0x1F) << 8) | packet[2];
}

bool extractPUSI(const std::vector<uint8_t>& packet) {
    return (packet[1] & 0x40) != 0;
}

AdaptationFieldControl extractAdaptationControl(const std::vector<uint8_t>& packet) {
    return static_cast<AdaptationFieldControl>((packet[3] >> 4) & 0x03);
}

// ============================================================================
// Basic Construction Tests
// ============================================================================

TEST(ts_builder_construction) {
    TSPacketBuilder builder(0x100);

    TEST_ASSERT_EQ(builder.getPID(), 0x100, "PID should be 0x100");
    TEST_ASSERT_EQ(builder.getContinuityCounter(), 0, "Initial CC should be 0");

    return true;
}

TEST(ts_builder_pid_mask) {
    // PID is 13-bit, should mask to 0x1FFF
    TSPacketBuilder builder(0xFFFF);

    TEST_ASSERT_EQ(builder.getPID(), 0x1FFF, "PID should be masked to 13 bits");

    return true;
}

// ============================================================================
// NULL Packet Tests
// ============================================================================

TEST(ts_builder_null_packet) {
    auto packet = TSPacketBuilder::createNullPacket();

    TEST_ASSERT_TRUE(validatePacketStructure(packet), "NULL packet should be valid");
    TEST_ASSERT_EQ(extractPID(packet), 0x1FFF, "NULL packet PID should be 0x1FFF");
    TEST_ASSERT_EQ(extractCC(packet), 0, "NULL packet CC should be 0");

    // Payload should be all 0xFF
    bool all_ff = true;
    for (size_t i = 4; i < packet.size(); ++i) {
        if (packet[i] != 0xFF) {
            all_ff = false;
            break;
        }
    }
    TEST_ASSERT_TRUE(all_ff, "NULL packet payload should be all 0xFF");

    return true;
}

// ============================================================================
// Simple Payload Tests
// ============================================================================

TEST(ts_builder_empty_payload) {
    TSPacketBuilder builder(0x100);

    BuildOptions opts;
    auto packets = builder.build(nullptr, 0, opts);

    TEST_ASSERT_EQ(packets.size(), 1, "Should create 1 packet for empty payload");
    TEST_ASSERT_TRUE(validatePacketStructure(packets[0]), "Packet should be valid");

    return true;
}

TEST(ts_builder_small_payload) {
    TSPacketBuilder builder(0x100);

    uint8_t payload[10] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};

    BuildOptions opts;
    auto packets = builder.build(payload, sizeof(payload), opts);

    TEST_ASSERT_EQ(packets.size(), 1, "Should create 1 packet");
    TEST_ASSERT_TRUE(validatePacketStructure(packets[0]), "Packet should be valid");
    TEST_ASSERT_EQ(extractPID(packets[0]), 0x100, "PID should match");

    return true;
}

TEST(ts_builder_full_payload) {
    TSPacketBuilder builder(0x100);

    // 184 bytes = full payload without adaptation field
    uint8_t payload[184];
    for (int i = 0; i < 184; ++i) {
        payload[i] = i & 0xFF;
    }

    BuildOptions opts;
    auto packets = builder.build(payload, sizeof(payload), opts);

    TEST_ASSERT_EQ(packets.size(), 1, "Should create 1 packet for 184 bytes");
    TEST_ASSERT_TRUE(validatePacketStructure(packets[0]), "Packet should be valid");

    // Verify payload
    bool payload_match = std::memcmp(packets[0].data() + 4, payload, 184) == 0;
    TEST_ASSERT_TRUE(payload_match, "Payload should match");

    return true;
}

TEST(ts_builder_multiple_packets) {
    TSPacketBuilder builder(0x100);

    // 400 bytes requires 3 packets (184 + 184 + 32)
    uint8_t payload[400];
    for (int i = 0; i < 400; ++i) {
        payload[i] = i & 0xFF;
    }

    BuildOptions opts;
    auto packets = builder.build(payload, sizeof(payload), opts);

    TEST_ASSERT_EQ(packets.size(), 3, "Should create 3 packets for 400 bytes");

    for (const auto& pkt : packets) {
        TEST_ASSERT_TRUE(validatePacketStructure(pkt), "All packets should be valid");
    }

    return true;
}

// ============================================================================
// PUSI Flag Tests
// ============================================================================

TEST(ts_builder_pusi_flag) {
    TSPacketBuilder builder(0x100);

    uint8_t payload[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    BuildOptions opts;
    opts.pusi = true;
    auto packets = builder.build(payload, sizeof(payload), opts);

    TEST_ASSERT_EQ(packets.size(), 1, "Should create 1 packet");
    TEST_ASSERT_TRUE(extractPUSI(packets[0]), "PUSI should be set");

    return true;
}

TEST(ts_builder_pusi_only_first_packet) {
    TSPacketBuilder builder(0x100);

    uint8_t payload[400];
    for (int i = 0; i < 400; ++i) {
        payload[i] = i & 0xFF;
    }

    BuildOptions opts;
    opts.pusi = true;
    auto packets = builder.build(payload, sizeof(payload), opts);

    TEST_ASSERT_TRUE(packets.size() > 1, "Should create multiple packets");
    TEST_ASSERT_TRUE(extractPUSI(packets[0]), "PUSI should be set on first packet");

    for (size_t i = 1; i < packets.size(); ++i) {
        TEST_ASSERT_FALSE(extractPUSI(packets[i]),
                         "PUSI should NOT be set on subsequent packets");
    }

    return true;
}

// ============================================================================
// Continuity Counter Tests
// ============================================================================

TEST(ts_builder_continuity_counter_increment) {
    TSPacketBuilder builder(0x100);

    uint8_t payload[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Build first packet
    auto packets1 = builder.build(payload, sizeof(payload));
    TEST_ASSERT_EQ(extractCC(packets1[0]), 0, "First packet CC should be 0");
    TEST_ASSERT_EQ(builder.getContinuityCounter(), 1, "Builder CC should be 1");

    // Build second packet
    auto packets2 = builder.build(payload, sizeof(payload));
    TEST_ASSERT_EQ(extractCC(packets2[0]), 1, "Second packet CC should be 1");
    TEST_ASSERT_EQ(builder.getContinuityCounter(), 2, "Builder CC should be 2");

    return true;
}

TEST(ts_builder_continuity_counter_wrap) {
    TSPacketBuilder builder(0x100);

    builder.setContinuityCounter(15);

    uint8_t payload[10] = {1, 2, 3};
    auto packets = builder.build(payload, sizeof(payload));

    TEST_ASSERT_EQ(extractCC(packets[0]), 15, "Packet CC should be 15");
    TEST_ASSERT_EQ(builder.getContinuityCounter(), 0, "CC should wrap to 0");

    return true;
}

TEST(ts_builder_continuity_counter_reset) {
    TSPacketBuilder builder(0x100);

    uint8_t payload[10] = {1, 2, 3};
    builder.build(payload, sizeof(payload));
    builder.build(payload, sizeof(payload));
    builder.build(payload, sizeof(payload));

    TEST_ASSERT_EQ(builder.getContinuityCounter(), 3, "CC should be 3");

    builder.resetContinuityCounter();
    TEST_ASSERT_EQ(builder.getContinuityCounter(), 0, "CC should be reset to 0");

    return true;
}

TEST(ts_builder_cc_multiple_packets) {
    TSPacketBuilder builder(0x100);

    // 400 bytes requires 3 packets
    uint8_t payload[400];
    auto packets = builder.build(payload, sizeof(payload));

    TEST_ASSERT_EQ(packets.size(), 3, "Should create 3 packets");
    TEST_ASSERT_EQ(extractCC(packets[0]), 0, "Packet 1 CC should be 0");
    TEST_ASSERT_EQ(extractCC(packets[1]), 1, "Packet 2 CC should be 1");
    TEST_ASSERT_EQ(extractCC(packets[2]), 2, "Packet 3 CC should be 2");

    return true;
}

// ============================================================================
// Adaptation Field Tests
// ============================================================================

TEST(ts_builder_adaptation_field_pcr) {
    TSPacketBuilder builder(0x100);

    uint8_t payload[10] = {1, 2, 3, 4, 5};

    BuildOptions opts;
    opts.has_pcr = true;
    opts.pcr_value = 123456789;

    auto packets = builder.build(payload, sizeof(payload), opts);

    TEST_ASSERT_EQ(packets.size(), 1, "Should create 1 packet");
    TEST_ASSERT_TRUE(validatePacketStructure(packets[0]), "Packet should be valid");

    auto adapt_ctrl = extractAdaptationControl(packets[0]);
    TEST_ASSERT_TRUE(adapt_ctrl == AdaptationFieldControl::ADAPTATION_PAYLOAD,
                    "Should have adaptation field and payload");

    // Check PCR flag in adaptation field
    uint8_t adaptation_length = packets[0][4];
    TEST_ASSERT_TRUE(adaptation_length > 0, "Adaptation field should have length > 0");

    uint8_t flags = packets[0][5];
    bool pcr_flag = (flags & 0x10) != 0;
    TEST_ASSERT_TRUE(pcr_flag, "PCR flag should be set");

    return true;
}

TEST(ts_builder_adaptation_field_random_access) {
    TSPacketBuilder builder(0x100);

    uint8_t payload[10] = {1, 2, 3};

    BuildOptions opts;
    opts.random_access = true;

    auto packets = builder.build(payload, sizeof(payload), opts);

    uint8_t flags = packets[0][5];
    bool ra_flag = (flags & 0x40) != 0;
    TEST_ASSERT_TRUE(ra_flag, "Random access flag should be set");

    return true;
}

TEST(ts_builder_adaptation_field_discontinuity) {
    TSPacketBuilder builder(0x100);

    uint8_t payload[10] = {1, 2, 3};

    BuildOptions opts;
    opts.discontinuity = true;

    auto packets = builder.build(payload, sizeof(payload), opts);

    uint8_t flags = packets[0][5];
    bool disc_flag = (flags & 0x80) != 0;
    TEST_ASSERT_TRUE(disc_flag, "Discontinuity flag should be set");

    return true;
}

// ============================================================================
// Private Data Tests
// ============================================================================

TEST(ts_builder_private_data_basic) {
    TSPacketBuilder builder(0x100);

    uint8_t payload[10] = {1, 2, 3, 4, 5};
    uint8_t private_data[5] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};

    BuildOptions opts;
    opts.has_private_data = true;
    opts.private_data = private_data;
    opts.private_data_length = sizeof(private_data);

    auto packets = builder.build(payload, sizeof(payload), opts);

    TEST_ASSERT_EQ(packets.size(), 1, "Should create 1 packet");
    TEST_ASSERT_TRUE(validatePacketStructure(packets[0]), "Packet should be valid");

    // Check private data flag
    uint8_t flags = packets[0][5];
    bool private_flag = (flags & 0x02) != 0;
    TEST_ASSERT_TRUE(private_flag, "Private data flag should be set");

    return true;
}

TEST(ts_builder_private_data_large) {
    TSPacketBuilder builder(0x100);

    uint8_t payload[10] = {1, 2, 3};
    uint8_t private_data[100];
    for (int i = 0; i < 100; ++i) {
        private_data[i] = i & 0xFF;
    }

    BuildOptions opts;
    opts.has_private_data = true;
    opts.private_data = private_data;
    opts.private_data_length = sizeof(private_data);

    auto packets = builder.build(payload, sizeof(payload), opts);

    TEST_ASSERT_TRUE(packets.size() >= 1, "Should create at least 1 packet");
    TEST_ASSERT_TRUE(validatePacketStructure(packets[0]), "Packet should be valid");

    return true;
}

TEST(ts_builder_private_data_with_pcr) {
    TSPacketBuilder builder(0x100);

    uint8_t payload[50];
    for (int i = 0; i < 50; ++i) payload[i] = i;

    uint8_t private_data[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    BuildOptions opts;
    opts.has_pcr = true;
    opts.pcr_value = 987654321;
    opts.has_private_data = true;
    opts.private_data = private_data;
    opts.private_data_length = sizeof(private_data);

    auto packets = builder.build(payload, sizeof(payload), opts);

    TEST_ASSERT_TRUE(packets.size() >= 1, "Should create at least 1 packet");

    // Check both flags are set
    uint8_t flags = packets[0][5];
    bool pcr_flag = (flags & 0x10) != 0;
    bool private_flag = (flags & 0x02) != 0;

    TEST_ASSERT_TRUE(pcr_flag, "PCR flag should be set");
    TEST_ASSERT_TRUE(private_flag, "Private data flag should be set");

    return true;
}

TEST(ts_builder_private_data_empty) {
    TSPacketBuilder builder(0x100);

    uint8_t payload[10] = {1, 2, 3};

    BuildOptions opts;
    opts.has_private_data = true;
    opts.private_data = nullptr;
    opts.private_data_length = 0;

    auto packets = builder.build(payload, sizeof(payload), opts);

    // Should handle empty private data gracefully
    TEST_ASSERT_EQ(packets.size(), 1, "Should create 1 packet");
    TEST_ASSERT_TRUE(validatePacketStructure(packets[0]), "Packet should be valid");

    return true;
}

TEST(ts_builder_private_data_only_adaptation) {
    TSPacketBuilder builder(0x100);

    uint8_t private_data[20];
    for (int i = 0; i < 20; ++i) private_data[i] = i * 2;

    BuildOptions opts;
    opts.has_private_data = true;
    opts.private_data = private_data;
    opts.private_data_length = sizeof(private_data);

    // No payload, only adaptation field with private data
    auto packets = builder.build(nullptr, 0, opts);

    TEST_ASSERT_EQ(packets.size(), 1, "Should create 1 packet");
    TEST_ASSERT_TRUE(validatePacketStructure(packets[0]), "Packet should be valid");

    auto adapt_ctrl = extractAdaptationControl(packets[0]);
    TEST_ASSERT_TRUE(adapt_ctrl == AdaptationFieldControl::ADAPTATION_ONLY,
                    "Should have adaptation field only");

    return true;
}

// ============================================================================
// PCR Encoding Tests
// ============================================================================

TEST(ts_builder_pcr_encoding) {
    TSPacketBuilder builder(0x100);

    uint8_t payload[20] = {1, 2, 3};

    BuildOptions opts;
    opts.has_pcr = true;
    opts.pcr_value = 27000000;  // 1 second at 27 MHz

    auto packets = builder.build(payload, sizeof(payload), opts);

    TEST_ASSERT_EQ(packets.size(), 1, "Should create 1 packet");

    // PCR should be at offset 6 (4 header + 1 length + 1 flags)
    // PCR = PCR_base * 300 + PCR_ext
    // 27000000 = 90000 * 300 + 0

    return true;
}

TEST(ts_builder_pcr_zero) {
    TSPacketBuilder builder(0x100);

    uint8_t payload[10] = {1, 2, 3};

    BuildOptions opts;
    opts.has_pcr = true;
    opts.pcr_value = 0;

    auto packets = builder.build(payload, sizeof(payload), opts);

    TEST_ASSERT_EQ(packets.size(), 1, "Should create 1 packet");
    TEST_ASSERT_TRUE(validatePacketStructure(packets[0]), "Packet should be valid");

    return true;
}

TEST(ts_builder_pcr_max_value) {
    TSPacketBuilder builder(0x100);

    uint8_t payload[10] = {1, 2, 3};

    BuildOptions opts;
    opts.has_pcr = true;
    opts.pcr_value = 0x3FFFFFFFFFF;  // Max 42-bit value

    auto packets = builder.build(payload, sizeof(payload), opts);

    TEST_ASSERT_EQ(packets.size(), 1, "Should create 1 packet");
    TEST_ASSERT_TRUE(validatePacketStructure(packets[0]), "Packet should be valid");

    return true;
}

// ============================================================================
// Stuffing Tests
// ============================================================================

TEST(ts_builder_stuffing_small_payload) {
    TSPacketBuilder builder(0x100);

    uint8_t payload[5] = {1, 2, 3, 4, 5};

    auto packets = builder.build(payload, sizeof(payload));

    TEST_ASSERT_EQ(packets.size(), 1, "Should create 1 packet");

    // Should have adaptation field with stuffing
    auto adapt_ctrl = extractAdaptationControl(packets[0]);
    TEST_ASSERT_TRUE(adapt_ctrl == AdaptationFieldControl::ADAPTATION_PAYLOAD,
                    "Should have adaptation field for stuffing");

    return true;
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(ts_builder_full_featured_packet) {
    TSPacketBuilder builder(0x100);

    uint8_t payload[100];
    for (int i = 0; i < 100; ++i) payload[i] = i;

    uint8_t private_data[15] = {0xFF, 0xEE, 0xDD, 0xCC, 0xBB,
                                0xAA, 0x99, 0x88, 0x77, 0x66,
                                0x55, 0x44, 0x33, 0x22, 0x11};

    BuildOptions opts;
    opts.pusi = true;
    opts.has_pcr = true;
    opts.pcr_value = 123456789;
    opts.random_access = true;
    opts.has_private_data = true;
    opts.private_data = private_data;
    opts.private_data_length = sizeof(private_data);

    auto packets = builder.build(payload, sizeof(payload), opts);

    TEST_ASSERT_TRUE(packets.size() >= 1, "Should create at least 1 packet");
    TEST_ASSERT_TRUE(validatePacketStructure(packets[0]), "Packet should be valid");
    TEST_ASSERT_TRUE(extractPUSI(packets[0]), "PUSI should be set");

    uint8_t flags = packets[0][5];
    TEST_ASSERT_TRUE((flags & 0x10) != 0, "PCR flag should be set");
    TEST_ASSERT_TRUE((flags & 0x40) != 0, "Random access flag should be set");
    TEST_ASSERT_TRUE((flags & 0x02) != 0, "Private data flag should be set");

    return true;
}

TEST(ts_builder_parse_built_packet) {
    TSPacketBuilder builder(0x100);

    uint8_t payload[50];
    for (int i = 0; i < 50; ++i) payload[i] = i * 2;

    BuildOptions opts;
    opts.pusi = true;

    auto packets = builder.build(payload, sizeof(payload), opts);

    // Try to parse the built packet
    TSPacket ts_packet;
    bool parsed = ts_packet.parse(packets[0].data());

    TEST_ASSERT_TRUE(parsed, "Built packet should be parseable");
    TEST_ASSERT_TRUE(ts_packet.isValid(), "Parsed packet should be valid");
    TEST_ASSERT_EQ(ts_packet.getHeader().pid, 0x100, "PID should match");
    TEST_ASSERT_TRUE(ts_packet.getHeader().payload_unit_start, "PUSI should match");

    return true;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    return TestRegistry::instance().runAll();
}
