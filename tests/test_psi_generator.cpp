#include "test_framework.hpp"
#include "mpegts_psi_generator.hpp"
#include "mpegts_psi.hpp"
#include <cstring>

using namespace mpegts;
using namespace test;

// ============================================================================
// Helper Functions
// ============================================================================

bool verifySectionLength(const std::vector<uint8_t>& data) {
    if (data.size() < 3) return false;
    uint16_t section_length = ((data[1] & 0x0F) << 8) | data[2];
    // section_length counts from after its own field to end
    return (data.size() == section_length + 3);
}

uint8_t extractVersion(const std::vector<uint8_t>& data) {
    if (data.size() < 6) return 0xFF;
    return (data[5] >> 1) & 0x1F;
}

// ============================================================================
// Constructor and Basic Tests
// ============================================================================

TEST(psi_gen_construction) {
    PSIGenerator gen;

    TEST_ASSERT_EQ(gen.getTransportStreamID(), 1, "Default TS ID should be 1");
    TEST_ASSERT_EQ(gen.getPATVersion(), 0, "Initial PAT version should be 0");

    return true;
}

TEST(psi_gen_set_tsid) {
    PSIGenerator gen;

    gen.setTransportStreamID(42);
    TEST_ASSERT_EQ(gen.getTransportStreamID(), 42, "TS ID should be 42");

    gen.setTransportStreamID(0xFFFF);
    TEST_ASSERT_EQ(gen.getTransportStreamID(), 0xFFFF, "TS ID should be 0xFFFF");

    return true;
}

TEST(psi_gen_add_program) {
    PSIGenerator gen;

    gen.setProgramNumber(1, 0x1000);
    gen.setProgramNumber(2, 0x2000);

    // No direct way to verify, but should not throw
    return true;
}

TEST(psi_gen_add_program_zero) {
    PSIGenerator gen;

    bool threw = false;
    try {
        gen.setProgramNumber(0, 0x1000);  // Program 0 is reserved
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    TEST_ASSERT_TRUE(threw, "Should throw for program number 0");

    return true;
}

TEST(psi_gen_remove_program) {
    PSIGenerator gen;

    gen.setProgramNumber(1, 0x1000);
    gen.removeProgram(1);

    // Should not throw
    gen.removeProgram(999);  // Non-existent program

    return true;
}

// ============================================================================
// PAT Generation Tests
// ============================================================================

TEST(psi_gen_pat_empty) {
    PSIGenerator gen;

    auto pat_data = gen.generatePAT();

    // Verify basic structure
    TEST_ASSERT_TRUE(pat_data.size() > 0, "PAT should not be empty");
    TEST_ASSERT_EQ(pat_data[0], TABLE_ID_PAT, "Table ID should be 0x00");
    TEST_ASSERT_TRUE(verifySectionLength(pat_data), "Section length should match");

    // Verify CRC
    bool crc_valid = PSIParser::verifyCRC32(pat_data.data(), pat_data.size());
    TEST_ASSERT_TRUE(crc_valid, "PAT CRC should be valid");

    return true;
}

TEST(psi_gen_pat_single_program) {
    PSIGenerator gen;
    gen.setTransportStreamID(1);
    gen.setProgramNumber(1, 0x1000);

    auto pat_data = gen.generatePAT();

    TEST_ASSERT_EQ(pat_data[0], TABLE_ID_PAT, "Table ID should be 0x00");

    // Verify TS ID
    uint16_t tsid = (pat_data[3] << 8) | pat_data[4];
    TEST_ASSERT_EQ(tsid, 1, "TS ID should be 1");

    // Expected size: 3 (header) + 5 (section header) + 4 (1 program) + 4 (CRC) = 16 bytes
    TEST_ASSERT_EQ(pat_data.size(), 16, "PAT with 1 program should be 16 bytes");

    // Verify CRC
    bool crc_valid = PSIParser::verifyCRC32(pat_data.data(), pat_data.size());
    TEST_ASSERT_TRUE(crc_valid, "PAT CRC should be valid");

    return true;
}

TEST(psi_gen_pat_multiple_programs) {
    PSIGenerator gen;
    gen.setProgramNumber(1, 0x1000);
    gen.setProgramNumber(2, 0x2000);
    gen.setProgramNumber(3, 0x3000);

    auto pat_data = gen.generatePAT();

    // Expected size: 3 + 5 + 12 (3 programs) + 4 = 24 bytes
    TEST_ASSERT_EQ(pat_data.size(), 24, "PAT with 3 programs should be 24 bytes");

    // Verify CRC
    bool crc_valid = PSIParser::verifyCRC32(pat_data.data(), pat_data.size());
    TEST_ASSERT_TRUE(crc_valid, "PAT CRC should be valid");

    return true;
}

TEST(psi_gen_pat_parse_back) {
    PSIGenerator gen;
    gen.setTransportStreamID(42);
    gen.setProgramNumber(1, 0x1000);
    gen.setProgramNumber(5, 0x1500);  // Fixed: use valid 13-bit PID (0x0000-0x1FFF)

    auto pat_data = gen.generatePAT();

    // Parse it back
    PAT pat;
    bool parsed = PSIParser::parsePAT(pat_data.data(), pat_data.size(), pat);

    TEST_ASSERT_TRUE(parsed, "Should parse generated PAT");
    TEST_ASSERT_EQ(pat.transport_stream_id, 42, "TS ID should match");
    TEST_ASSERT_EQ(pat.programs.size(), 2, "Should have 2 programs");

    // Verify program mappings
    TEST_ASSERT_EQ(pat.getPMTPID(1), 0x1000, "Program 1 PMT PID should be 0x1000");
    TEST_ASSERT_EQ(pat.getPMTPID(5), 0x1500, "Program 5 PMT PID should be 0x1500");

    return true;
}

TEST(psi_gen_pat_version) {
    PSIGenerator gen;
    gen.setProgramNumber(1, 0x1000);

    auto pat1 = gen.generatePAT();
    uint8_t version1 = extractVersion(pat1);
    TEST_ASSERT_EQ(version1, 0, "Initial version should be 0");

    gen.incrementPATVersion();
    auto pat2 = gen.generatePAT();
    uint8_t version2 = extractVersion(pat2);
    TEST_ASSERT_EQ(version2, 1, "Version should be 1 after increment");

    // Increment 31 times to test wrap-around
    for (int i = 0; i < 31; ++i) {
        gen.incrementPATVersion();
    }
    auto pat3 = gen.generatePAT();
    uint8_t version3 = extractVersion(pat3);
    TEST_ASSERT_EQ(version3, 0, "Version should wrap to 0");

    return true;
}

TEST(psi_gen_pat_section_syntax) {
    PSIGenerator gen;
    gen.setProgramNumber(1, 0x1000);

    auto pat = gen.generatePAT();

    // Byte 1: section_syntax_indicator should be 1
    uint8_t byte1 = pat[1];
    bool syntax_indicator = (byte1 & 0x80) != 0;
    TEST_ASSERT_TRUE(syntax_indicator, "Section syntax indicator should be 1");

    // Current/next indicator should be 1
    uint8_t byte5 = pat[5];
    bool current_next = (byte5 & 0x01) != 0;
    TEST_ASSERT_TRUE(current_next, "Current/next indicator should be 1");

    return true;
}

// ============================================================================
// PMT Generation Tests
// ============================================================================

TEST(psi_gen_pmt_not_found) {
    PSIGenerator gen;

    bool threw = false;
    try {
        gen.generatePMT(1);  // Program doesn't exist
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    TEST_ASSERT_TRUE(threw, "Should throw for non-existent program");

    return true;
}

TEST(psi_gen_pmt_no_streams) {
    PSIGenerator gen;
    gen.setProgramNumber(1, 0x1000);
    gen.setPCRPID(1, 0x1FFF);

    auto pmt_data = gen.generatePMT(1);

    TEST_ASSERT_EQ(pmt_data[0], TABLE_ID_PMT, "Table ID should be 0x02");

    // Verify CRC
    bool crc_valid = PSIParser::verifyCRC32(pmt_data.data(), pmt_data.size());
    TEST_ASSERT_TRUE(crc_valid, "PMT CRC should be valid");

    return true;
}

TEST(psi_gen_pmt_single_stream) {
    PSIGenerator gen;
    gen.setProgramNumber(1, 0x1000);
    gen.setPCRPID(1, 0x100);
    gen.addStream(1, 0x100, mpegts::StreamType::H264_VIDEO);

    auto pmt_data = gen.generatePMT(1);

    TEST_ASSERT_EQ(pmt_data[0], TABLE_ID_PMT, "Table ID should be 0x02");

    // Verify program number
    uint16_t program_num = (pmt_data[3] << 8) | pmt_data[4];
    TEST_ASSERT_EQ(program_num, 1, "Program number should be 1");

    // Verify PCR PID
    uint16_t pcr_pid = ((pmt_data[8] & 0x1F) << 8) | pmt_data[9];
    TEST_ASSERT_EQ(pcr_pid, 0x100, "PCR PID should be 0x100");

    // Verify CRC
    bool crc_valid = PSIParser::verifyCRC32(pmt_data.data(), pmt_data.size());
    TEST_ASSERT_TRUE(crc_valid, "PMT CRC should be valid");

    return true;
}

TEST(psi_gen_pmt_multiple_streams) {
    PSIGenerator gen;
    gen.setProgramNumber(1, 0x1000);
    gen.setPCRPID(1, 0x100);
    gen.addStream(1, 0x100, mpegts::StreamType::H264_VIDEO);
    gen.addStream(1, 0x101, mpegts::StreamType::AAC_AUDIO);
    gen.addStream(1, 0x102, mpegts::StreamType::PRIVATE_DATA);

    auto pmt_data = gen.generatePMT(1);

    // Verify CRC
    bool crc_valid = PSIParser::verifyCRC32(pmt_data.data(), pmt_data.size());
    TEST_ASSERT_TRUE(crc_valid, "PMT CRC should be valid");

    return true;
}

TEST(psi_gen_pmt_parse_back) {
    PSIGenerator gen;
    gen.setProgramNumber(1, 0x1000);
    gen.setPCRPID(1, 0x100);
    gen.addStream(1, 0x100, mpegts::StreamType::H264_VIDEO);
    gen.addStream(1, 0x101, mpegts::StreamType::AAC_AUDIO);

    auto pmt_data = gen.generatePMT(1);

    // Parse it back
    PMT pmt;
    bool parsed = PSIParser::parsePMT(pmt_data.data(), pmt_data.size(), pmt);

    TEST_ASSERT_TRUE(parsed, "Should parse generated PMT");
    TEST_ASSERT_EQ(pmt.program_number, 1, "Program number should match");
    TEST_ASSERT_EQ(pmt.pcr_pid, 0x100, "PCR PID should match");
    TEST_ASSERT_EQ(pmt.streams.size(), 2, "Should have 2 streams");

    // Verify stream types
    const PMTStreamInfo* stream1 = pmt.getStreamInfo(0x100);
    TEST_ASSERT_TRUE(stream1 != nullptr, "Stream 0x100 should exist");
    TEST_ASSERT_TRUE(stream1->stream_type == mpegts::StreamType::H264_VIDEO,
                    "Stream 0x100 should be H.264");

    const PMTStreamInfo* stream2 = pmt.getStreamInfo(0x101);
    TEST_ASSERT_TRUE(stream2 != nullptr, "Stream 0x101 should exist");
    TEST_ASSERT_TRUE(stream2->stream_type == mpegts::StreamType::AAC_AUDIO,
                    "Stream 0x101 should be AAC");

    return true;
}

TEST(psi_gen_pmt_version) {
    PSIGenerator gen;
    gen.setProgramNumber(1, 0x1000);

    auto pmt1 = gen.generatePMT(1);
    uint8_t version1 = extractVersion(pmt1);
    TEST_ASSERT_EQ(version1, 0, "Initial PMT version should be 0");

    gen.incrementPMTVersion(1);
    auto pmt2 = gen.generatePMT(1);
    uint8_t version2 = extractVersion(pmt2);
    TEST_ASSERT_EQ(version2, 1, "PMT version should be 1 after increment");

    return true;
}

TEST(psi_gen_pmt_remove_stream) {
    PSIGenerator gen;
    gen.setProgramNumber(1, 0x1000);
    gen.addStream(1, 0x100, mpegts::StreamType::H264_VIDEO);
    gen.addStream(1, 0x101, mpegts::StreamType::AAC_AUDIO);

    auto pmt1 = gen.generatePMT(1);
    PMT pmt_parsed1;
    PSIParser::parsePMT(pmt1.data(), pmt1.size(), pmt_parsed1);
    TEST_ASSERT_EQ(pmt_parsed1.streams.size(), 2, "Should have 2 streams initially");

    gen.removeStream(1, 0x101);
    auto pmt2 = gen.generatePMT(1);
    PMT pmt_parsed2;
    PSIParser::parsePMT(pmt2.data(), pmt2.size(), pmt_parsed2);
    TEST_ASSERT_EQ(pmt_parsed2.streams.size(), 1, "Should have 1 stream after removal");

    return true;
}

// ============================================================================
// Version Management Tests
// ============================================================================

TEST(psi_gen_reset_versions) {
    PSIGenerator gen;
    gen.setProgramNumber(1, 0x1000);

    gen.incrementPATVersion();
    gen.incrementPATVersion();
    gen.incrementPMTVersion(1);

    TEST_ASSERT_EQ(gen.getPATVersion(), 2, "PAT version should be 2");
    TEST_ASSERT_EQ(gen.getPMTVersion(1), 1, "PMT version should be 1");

    gen.resetVersions();

    TEST_ASSERT_EQ(gen.getPATVersion(), 0, "PAT version should reset to 0");
    TEST_ASSERT_EQ(gen.getPMTVersion(1), 0, "PMT version should reset to 0");

    return true;
}

TEST(psi_gen_independent_pmt_versions) {
    PSIGenerator gen;
    gen.setProgramNumber(1, 0x1000);
    gen.setProgramNumber(2, 0x2000);

    gen.incrementPMTVersion(1);
    gen.incrementPMTVersion(1);
    gen.incrementPMTVersion(2);

    TEST_ASSERT_EQ(gen.getPMTVersion(1), 2, "Program 1 PMT version should be 2");
    TEST_ASSERT_EQ(gen.getPMTVersion(2), 1, "Program 2 PMT version should be 1");

    return true;
}

// ============================================================================
// Multi-Program Tests
// ============================================================================

TEST(psi_gen_multi_program_pat_pmt) {
    PSIGenerator gen;

    // Program 1: H.264 video + AAC audio
    gen.setProgramNumber(1, 0x1000);
    gen.setPCRPID(1, 0x100);
    gen.addStream(1, 0x100, mpegts::StreamType::H264_VIDEO);
    gen.addStream(1, 0x101, mpegts::StreamType::AAC_AUDIO);

    // Program 2: H.265 video + AAC audio
    gen.setProgramNumber(2, 0x2000);
    gen.setPCRPID(2, 0x200);
    gen.addStream(2, 0x200, mpegts::StreamType::H265_VIDEO);
    gen.addStream(2, 0x201, mpegts::StreamType::AAC_AUDIO);

    // Generate and verify PAT
    auto pat_data = gen.generatePAT();
    PAT pat;
    bool pat_parsed = PSIParser::parsePAT(pat_data.data(), pat_data.size(), pat);

    TEST_ASSERT_TRUE(pat_parsed, "Should parse PAT");
    TEST_ASSERT_EQ(pat.programs.size(), 2, "PAT should have 2 programs");

    // Generate and verify PMT for program 1
    auto pmt1_data = gen.generatePMT(1);
    PMT pmt1;
    bool pmt1_parsed = PSIParser::parsePMT(pmt1_data.data(), pmt1_data.size(), pmt1);

    TEST_ASSERT_TRUE(pmt1_parsed, "Should parse PMT1");
    TEST_ASSERT_EQ(pmt1.streams.size(), 2, "PMT1 should have 2 streams");

    // Generate and verify PMT for program 2
    auto pmt2_data = gen.generatePMT(2);
    PMT pmt2;
    bool pmt2_parsed = PSIParser::parsePMT(pmt2_data.data(), pmt2_data.size(), pmt2);

    TEST_ASSERT_TRUE(pmt2_parsed, "Should parse PMT2");
    TEST_ASSERT_EQ(pmt2.streams.size(), 2, "PMT2 should have 2 streams");

    return true;
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(psi_gen_max_version) {
    PSIGenerator gen;

    for (int i = 0; i < 32; ++i) {
        gen.incrementPATVersion();
    }

    TEST_ASSERT_EQ(gen.getPATVersion(), 0, "Version should wrap at 32 (5-bit)");

    return true;
}

TEST(psi_gen_pcr_pid_all_ones) {
    PSIGenerator gen;
    gen.setProgramNumber(1, 0x1000);
    gen.setPCRPID(1, 0x1FFF);  // All 1s = no PCR

    auto pmt = gen.generatePMT(1);

    bool crc_valid = PSIParser::verifyCRC32(pmt.data(), pmt.size());
    TEST_ASSERT_TRUE(crc_valid, "PMT with PCR PID 0x1FFF should be valid");

    return true;
}

TEST(psi_gen_stream_type_h265) {
    PSIGenerator gen;
    gen.setProgramNumber(1, 0x1000);
    gen.addStream(1, 0x100, mpegts::StreamType::H265_VIDEO);

    auto pmt_data = gen.generatePMT(1);
    PMT pmt;
    PSIParser::parsePMT(pmt_data.data(), pmt_data.size(), pmt);

    const PMTStreamInfo* stream = pmt.getStreamInfo(0x100);
    TEST_ASSERT_TRUE(stream != nullptr, "Stream should exist");
    TEST_ASSERT_TRUE(stream->stream_type == mpegts::StreamType::H265_VIDEO,
                    "Stream type should be H.265");

    return true;
}

TEST(psi_gen_many_streams) {
    PSIGenerator gen;
    gen.setProgramNumber(1, 0x1000);
    gen.setPCRPID(1, 0x100);

    // Add 10 streams
    for (int i = 0; i < 10; ++i) {
        gen.addStream(1, 0x100 + i, mpegts::StreamType::PRIVATE_DATA);
    }

    auto pmt_data = gen.generatePMT(1);

    // Verify CRC
    bool crc_valid = PSIParser::verifyCRC32(pmt_data.data(), pmt_data.size());
    TEST_ASSERT_TRUE(crc_valid, "PMT with 10 streams should have valid CRC");

    // Parse back
    PMT pmt;
    bool parsed = PSIParser::parsePMT(pmt_data.data(), pmt_data.size(), pmt);
    TEST_ASSERT_TRUE(parsed, "Should parse PMT with 10 streams");
    TEST_ASSERT_EQ(pmt.streams.size(), 10, "Should have 10 streams");

    return true;
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(psi_gen_typical_broadcast) {
    // Simulate typical broadcast: 1 video, 2 audio streams (stereo + 5.1)
    PSIGenerator gen;
    gen.setTransportStreamID(1);
    gen.setProgramNumber(1, 0x1000);
    gen.setPCRPID(1, 0x100);

    gen.addStream(1, 0x100, mpegts::StreamType::H264_VIDEO);
    gen.addStream(1, 0x101, mpegts::StreamType::AAC_AUDIO);  // Stereo
    gen.addStream(1, 0x102, mpegts::StreamType::AAC_AUDIO);  // 5.1

    // Generate tables
    auto pat = gen.generatePAT();
    auto pmt = gen.generatePMT(1);

    // Verify both are valid
    TEST_ASSERT_TRUE(PSIParser::verifyCRC32(pat.data(), pat.size()),
                    "PAT CRC should be valid");
    TEST_ASSERT_TRUE(PSIParser::verifyCRC32(pmt.data(), pmt.size()),
                    "PMT CRC should be valid");

    // Parse and verify
    PAT pat_parsed;
    PMT pmt_parsed;
    PSIParser::parsePAT(pat.data(), pat.size(), pat_parsed);
    PSIParser::parsePMT(pmt.data(), pmt.size(), pmt_parsed);

    TEST_ASSERT_EQ(pmt_parsed.streams.size(), 3, "Should have 3 streams");

    return true;
}

TEST(psi_gen_update_and_regenerate) {
    PSIGenerator gen;
    gen.setProgramNumber(1, 0x1000);
    gen.addStream(1, 0x100, mpegts::StreamType::H264_VIDEO);

    auto pmt1 = gen.generatePMT(1);

    // Add another stream
    gen.addStream(1, 0x101, mpegts::StreamType::AAC_AUDIO);
    gen.incrementPMTVersion(1);

    auto pmt2 = gen.generatePMT(1);

    // Versions should differ
    uint8_t v1 = extractVersion(pmt1);
    uint8_t v2 = extractVersion(pmt2);
    TEST_ASSERT_NE(v1, v2, "Versions should be different");

    // Both should be valid
    TEST_ASSERT_TRUE(PSIParser::verifyCRC32(pmt1.data(), pmt1.size()),
                    "PMT1 should be valid");
    TEST_ASSERT_TRUE(PSIParser::verifyCRC32(pmt2.data(), pmt2.size()),
                    "PMT2 should be valid");

    return true;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    return test::TestRegistry::instance().runAll();
}
