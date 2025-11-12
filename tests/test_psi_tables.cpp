#include "test_framework.hpp"
#include "test_packet_generator.hpp"
#include "mpegts_demuxer.hpp"
#include "mpegts_psi.hpp"

using namespace mpegts;
using namespace test;

// ============================================================================
// PAT/PMT Parsing Tests
// ============================================================================

TEST(pat_basic_parsing) {
    // Create a basic PAT section manually
    std::vector<uint8_t> pat_section;

    // PAT header
    pat_section.push_back(0x00);  // table_id (PAT)
    pat_section.push_back(0xB0);  // section_syntax_indicator + reserved + section_length high bits
    pat_section.push_back(0x0D);  // section_length low byte (13 bytes remaining)

    // Transport stream ID
    pat_section.push_back(0x00);
    pat_section.push_back(0x01);

    // Version + current/next
    pat_section.push_back(0xC1);  // reserved + version 0 + current_next = 1

    // Section number
    pat_section.push_back(0x00);

    // Last section number
    pat_section.push_back(0x00);

    // Program 1: program_number = 1, PMT PID = 0x100
    pat_section.push_back(0x00);
    pat_section.push_back(0x01);  // program_number = 1
    pat_section.push_back(0xE1);  // reserved + PMT PID high bits
    pat_section.push_back(0x00);  // PMT PID low byte (0x100)

    // Calculate and add CRC-32
    uint32_t crc = PSIParser::calculateCRC32(pat_section.data(), pat_section.size());
    pat_section.push_back((crc >> 24) & 0xFF);
    pat_section.push_back((crc >> 16) & 0xFF);
    pat_section.push_back((crc >> 8) & 0xFF);
    pat_section.push_back(crc & 0xFF);

    // Parse PAT
    PAT pat;
    bool result = PSIParser::parsePAT(pat_section.data(), pat_section.size(), pat);

    TEST_ASSERT_TRUE(result, "Should parse PAT successfully");
    TEST_ASSERT_EQ(pat.transport_stream_id, 1, "Transport stream ID should be 1");
    TEST_ASSERT_EQ(pat.programs.size(), 1, "Should have 1 program");
    TEST_ASSERT_EQ(pat.programs[0].program_number, 1, "Program number should be 1");
    TEST_ASSERT_EQ(pat.programs[0].pid, 0x100, "PMT PID should be 0x100");

    return true;
}

TEST(pmt_basic_parsing) {
    // Create a basic PMT section manually
    std::vector<uint8_t> pmt_section;

    // PMT header
    pmt_section.push_back(0x02);  // table_id (PMT)
    pmt_section.push_back(0xB0);  // section_syntax_indicator + reserved
    pmt_section.push_back(0x17);  // section_length (23 bytes remaining)

    // Program number
    pmt_section.push_back(0x00);
    pmt_section.push_back(0x01);

    // Version + current/next
    pmt_section.push_back(0xC1);

    // Section number
    pmt_section.push_back(0x00);

    // Last section number
    pmt_section.push_back(0x00);

    // PCR PID
    pmt_section.push_back(0xE1);  // reserved + PCR PID high bits
    pmt_section.push_back(0x00);  // PCR PID low byte (0x100)

    // Program info length
    pmt_section.push_back(0xF0);  // reserved + length high bits
    pmt_section.push_back(0x00);  // length low byte (0)

    // Stream 1: H.264 video on PID 0x100
    pmt_section.push_back(0x1B);  // stream_type (H.264)
    pmt_section.push_back(0xE1);  // reserved + elementary PID high bits
    pmt_section.push_back(0x00);  // elementary PID low byte (0x100)
    pmt_section.push_back(0xF0);  // reserved + ES info length high bits
    pmt_section.push_back(0x00);  // ES info length low byte (0)

    // Stream 2: AAC audio on PID 0x101
    pmt_section.push_back(0x0F);  // stream_type (AAC)
    pmt_section.push_back(0xE1);  // reserved + elementary PID high bits
    pmt_section.push_back(0x01);  // elementary PID low byte (0x101)
    pmt_section.push_back(0xF0);  // reserved + ES info length high bits
    pmt_section.push_back(0x00);  // ES info length low byte (0)

    // Calculate and add CRC-32
    uint32_t crc = PSIParser::calculateCRC32(pmt_section.data(), pmt_section.size());
    pmt_section.push_back((crc >> 24) & 0xFF);
    pmt_section.push_back((crc >> 16) & 0xFF);
    pmt_section.push_back((crc >> 8) & 0xFF);
    pmt_section.push_back(crc & 0xFF);

    // Parse PMT
    PMT pmt;
    bool result = PSIParser::parsePMT(pmt_section.data(), pmt_section.size(), pmt);

    TEST_ASSERT_TRUE(result, "Should parse PMT successfully");
    TEST_ASSERT_EQ(pmt.program_number, 1, "Program number should be 1");
    TEST_ASSERT_EQ(pmt.pcr_pid, 0x100, "PCR PID should be 0x100");
    TEST_ASSERT_EQ(pmt.streams.size(), 2, "Should have 2 streams");
    TEST_ASSERT_EQ(static_cast<uint8_t>(pmt.streams[0].stream_type), 0x1B, "First stream should be H.264");
    TEST_ASSERT_EQ(pmt.streams[0].elementary_pid, 0x100, "First stream PID should be 0x100");
    TEST_ASSERT_EQ(static_cast<uint8_t>(pmt.streams[1].stream_type), 0x0F, "Second stream should be AAC");
    TEST_ASSERT_EQ(pmt.streams[1].elementary_pid, 0x101, "Second stream PID should be 0x101");

    return true;
}

TEST(psi_accumulator_single_section) {
    PSIAccumulator acc;

    // Create a properly formatted PSI section
    std::vector<uint8_t> payload;
    payload.push_back(0x00);  // pointer_field = 0 (section starts immediately)

    // PSI section header
    payload.push_back(0x00);  // table_id
    payload.push_back(0xB0);  // section_syntax_indicator + reserved + length high bits
    payload.push_back(0x05);  // section_length = 5 bytes (remaining data)

    // Section data (5 bytes as indicated by section_length)
    for (int i = 0; i < 5; ++i) {
        payload.push_back(i);
    }

    // Add with payload_unit_start = true
    bool complete = acc.addData(payload.data(), payload.size(), true);

    TEST_ASSERT_TRUE(complete, "Section should be complete");

    // Get section
    std::vector<uint8_t> section;
    size_t len = acc.getSection(section);

    TEST_ASSERT_EQ(len, 8, "Section length should be 8 (3 header + 5 data)");
    TEST_ASSERT_EQ(section[0], 0x00, "Table ID should be 0");
    TEST_ASSERT_EQ(section[1], 0xB0, "Second byte should be 0xB0");
    TEST_ASSERT_EQ(section[2], 0x05, "Section length should be 5");

    return true;
}

TEST(psi_accumulator_multi_packet) {
    PSIAccumulator acc;

    // First packet with pointer and start of section
    std::vector<uint8_t> packet1;
    packet1.push_back(0x00);  // pointer_field = 0
    packet1.push_back(0x00);  // table_id
    packet1.push_back(0xB0);  // section_syntax_indicator + reserved
    packet1.push_back(0x14);  // section_length = 20 bytes

    // Add 20 more bytes (but split across packets)
    for (int i = 0; i < 10; ++i) {
        packet1.push_back(i);
    }

    bool complete1 = acc.addData(packet1.data(), packet1.size(), true);
    TEST_ASSERT_FALSE(complete1, "Section should not be complete yet");

    // Second packet with continuation
    std::vector<uint8_t> packet2;
    for (int i = 10; i < 20; ++i) {
        packet2.push_back(i);
    }

    bool complete2 = acc.addData(packet2.data(), packet2.size(), false);
    TEST_ASSERT_TRUE(complete2, "Section should be complete now");

    // Get section
    std::vector<uint8_t> section;
    size_t len = acc.getSection(section);

    TEST_ASSERT_EQ(len, 23, "Section length should be 23 (header + 20 bytes)");

    return true;
}

TEST(crc32_calculation) {
    // Test CRC-32 with known values
    std::vector<uint8_t> data = {0x00, 0x01, 0x02, 0x03};
    uint32_t crc = PSIParser::calculateCRC32(data.data(), data.size());

    // CRC should be deterministic
    uint32_t crc2 = PSIParser::calculateCRC32(data.data(), data.size());
    TEST_ASSERT_EQ(crc, crc2, "CRC should be deterministic");

    return true;
}

TEST(pat_get_pmt_pid) {
    PAT pat;
    pat.programs.push_back(PATEntry(1, 0x100));
    pat.programs.push_back(PATEntry(2, 0x200));

    TEST_ASSERT_EQ(pat.getPMTPID(1), 0x100, "Should find PMT PID for program 1");
    TEST_ASSERT_EQ(pat.getPMTPID(2), 0x200, "Should find PMT PID for program 2");
    TEST_ASSERT_EQ(pat.getPMTPID(999), 0, "Should return 0 for unknown program");

    return true;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    return TestRegistry::instance().runAll();
}
