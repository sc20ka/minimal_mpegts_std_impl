#include "test_framework.hpp"
#include "mpegts_muxer.hpp"
#include "mpegts_muxer_types.hpp"

using namespace mpegts;

// ============================================================================
// Basic Type Tests
// ============================================================================

TEST(muxer_types_stream_type) {
    // Test StreamType enum values
    TEST_ASSERT_EQ(static_cast<uint8_t>(StreamType::VIDEO_H264), 0x1B,
                   "H.264 stream type should be 0x1B");
    TEST_ASSERT_EQ(static_cast<uint8_t>(StreamType::VIDEO_H265), 0x24,
                   "H.265 stream type should be 0x24");
    TEST_ASSERT_EQ(static_cast<uint8_t>(StreamType::AUDIO_AAC), 0x0F,
                   "AAC stream type should be 0x0F");
    TEST_ASSERT_EQ(static_cast<uint8_t>(StreamType::PRIVATE_DATA), 0x06,
                   "Private data stream type should be 0x06");

    return true;
}

TEST(muxer_types_mux_mode) {
    // Test MuxMode enum
    MuxMode cbr = MuxMode::CBR;
    MuxMode vbr = MuxMode::VBR;

    TEST_ASSERT_TRUE(cbr != vbr, "CBR and VBR should be different");

    return true;
}

TEST(muxer_types_private_data_mode) {
    // Test PrivateDataInsertionMode enum
    PrivateDataInsertionMode mode1 = PrivateDataInsertionMode::INSERT_WITH_PCR;
    PrivateDataInsertionMode mode2 = PrivateDataInsertionMode::INSERT_STANDALONE;
    PrivateDataInsertionMode mode3 = PrivateDataInsertionMode::INSERT_WITH_PAYLOAD;

    TEST_ASSERT_TRUE(mode1 != mode2, "Insertion modes should be different");
    TEST_ASSERT_TRUE(mode2 != mode3, "Insertion modes should be different");

    return true;
}

TEST(muxer_types_constants) {
    // Test constants
    TEST_ASSERT_EQ(DEFAULT_PCR_INTERVAL_MS, 40, "Default PCR interval should be 40ms");
    TEST_ASSERT_EQ(DEFAULT_PAT_INTERVAL_MS, 100, "Default PAT interval should be 100ms");
    TEST_ASSERT_EQ(DEFAULT_PMT_INTERVAL_MS, 100, "Default PMT interval should be 100ms");
    TEST_ASSERT_EQ(MAX_PRIVATE_DATA_SIZE, 182, "Max private data size should be 182 bytes");

    return true;
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST(muxer_stream_config_default) {
    StreamConfig config;

    // Test default values
    TEST_ASSERT_TRUE(config.type == StreamType::VIDEO_H264, "Default type should be H.264");
    TEST_ASSERT_EQ(config.pid, 0x100, "Default PID should be 0x100");
    TEST_ASSERT_EQ(config.stream_id, 0xE0, "Default stream_id should be 0xE0 (video)");
    TEST_ASSERT_EQ(config.bitrate, 4000000, "Default bitrate should be 4 Mbps");
    TEST_ASSERT_FALSE(config.pcr_enabled, "PCR should be disabled by default");
    TEST_ASSERT_FALSE(config.private_data_enabled, "Private data should be disabled");

    return true;
}

TEST(muxer_stream_config_video) {
    StreamConfig config;
    config.type = StreamType::VIDEO_H265;
    config.pid = 0x200;
    config.width = 3840;
    config.height = 2160;
    config.frame_rate = 60;

    TEST_ASSERT_TRUE(config.type == StreamType::VIDEO_H265, "Stream type should be H.265");
    TEST_ASSERT_EQ(config.width, 3840, "Width should be 3840 (4K)");
    TEST_ASSERT_EQ(config.height, 2160, "Height should be 2160 (4K)");
    TEST_ASSERT_EQ(config.frame_rate, 60, "Frame rate should be 60 fps");

    return true;
}

TEST(muxer_stream_config_audio) {
    StreamConfig config;
    config.type = StreamType::AUDIO_AAC;
    config.pid = 0x201;
    config.stream_id = 0xC0;
    config.sample_rate = 48000;
    config.channels = 2;

    TEST_ASSERT_TRUE(config.type == StreamType::AUDIO_AAC, "Stream type should be AAC");
    TEST_ASSERT_EQ(config.stream_id, 0xC0, "Stream ID should be 0xC0 (audio)");
    TEST_ASSERT_EQ(config.sample_rate, 48000, "Sample rate should be 48kHz");
    TEST_ASSERT_EQ(config.channels, 2, "Should have 2 channels");

    return true;
}

TEST(muxer_config_default) {
    MuxerConfig config;

    // Test default values
    TEST_ASSERT_EQ(config.bitrate, 5000000, "Default bitrate should be 5 Mbps");
    TEST_ASSERT_TRUE(config.mode == MuxMode::CBR, "Default mode should be CBR");
    TEST_ASSERT_EQ(config.transport_stream_id, 1, "Default TS ID should be 1");
    TEST_ASSERT_EQ(config.program_number, 1, "Default program number should be 1");
    TEST_ASSERT_EQ(config.pcr_pid, 0x100, "Default PCR PID should be 0x100");
    TEST_ASSERT_EQ(config.pcr_interval_ms, DEFAULT_PCR_INTERVAL_MS,
                   "PCR interval should match default");
    TEST_ASSERT_EQ(config.pat_interval_ms, DEFAULT_PAT_INTERVAL_MS,
                   "PAT interval should match default");
    TEST_ASSERT_EQ(config.pmt_interval_ms, DEFAULT_PMT_INTERVAL_MS,
                   "PMT interval should match default");

    return true;
}

TEST(muxer_config_custom) {
    MuxerConfig config;
    config.bitrate = 10000000;  // 10 Mbps
    config.mode = MuxMode::VBR;
    config.transport_stream_id = 42;
    config.program_number = 5;
    config.pcr_pid = 0x500;
    config.pcr_interval_ms = 20;

    TEST_ASSERT_EQ(config.bitrate, 10000000, "Custom bitrate should be set");
    TEST_ASSERT_TRUE(config.mode == MuxMode::VBR, "Mode should be VBR");
    TEST_ASSERT_EQ(config.transport_stream_id, 42, "Custom TS ID should be set");
    TEST_ASSERT_EQ(config.program_number, 5, "Custom program number should be set");
    TEST_ASSERT_EQ(config.pcr_pid, 0x500, "Custom PCR PID should be set");
    TEST_ASSERT_EQ(config.pcr_interval_ms, 20, "Custom PCR interval should be set");

    return true;
}

// ============================================================================
// Helper Function Tests
// ============================================================================

TEST(muxer_helpers_is_video_stream) {
    TEST_ASSERT_TRUE(isVideoStream(StreamType::VIDEO_MPEG1), "MPEG-1 is video");
    TEST_ASSERT_TRUE(isVideoStream(StreamType::VIDEO_MPEG2), "MPEG-2 is video");
    TEST_ASSERT_TRUE(isVideoStream(StreamType::VIDEO_H264), "H.264 is video");
    TEST_ASSERT_TRUE(isVideoStream(StreamType::VIDEO_H265), "H.265 is video");

    TEST_ASSERT_FALSE(isVideoStream(StreamType::AUDIO_AAC), "AAC is not video");
    TEST_ASSERT_FALSE(isVideoStream(StreamType::AUDIO_AC3), "AC-3 is not video");
    TEST_ASSERT_FALSE(isVideoStream(StreamType::PRIVATE_DATA), "Private is not video");

    return true;
}

TEST(muxer_helpers_is_audio_stream) {
    TEST_ASSERT_TRUE(isAudioStream(StreamType::AUDIO_MPEG1), "MPEG-1 Audio is audio");
    TEST_ASSERT_TRUE(isAudioStream(StreamType::AUDIO_MPEG2), "MPEG-2 Audio is audio");
    TEST_ASSERT_TRUE(isAudioStream(StreamType::AUDIO_AAC), "AAC is audio");
    TEST_ASSERT_TRUE(isAudioStream(StreamType::AUDIO_AAC_LATM), "AAC LATM is audio");
    TEST_ASSERT_TRUE(isAudioStream(StreamType::AUDIO_AC3), "AC-3 is audio");

    TEST_ASSERT_FALSE(isAudioStream(StreamType::VIDEO_H264), "H.264 is not audio");
    TEST_ASSERT_FALSE(isAudioStream(StreamType::PRIVATE_DATA), "Private is not audio");

    return true;
}

TEST(muxer_helpers_get_default_stream_id) {
    // Video streams should get 0xE0
    TEST_ASSERT_EQ(getDefaultStreamID(StreamType::VIDEO_H264), 0xE0,
                   "Video should get stream ID 0xE0");
    TEST_ASSERT_EQ(getDefaultStreamID(StreamType::VIDEO_H265), 0xE0,
                   "Video should get stream ID 0xE0");

    // Audio streams should get 0xC0
    TEST_ASSERT_EQ(getDefaultStreamID(StreamType::AUDIO_AAC), 0xC0,
                   "Audio should get stream ID 0xC0");
    TEST_ASSERT_EQ(getDefaultStreamID(StreamType::AUDIO_AC3), 0xC0,
                   "Audio should get stream ID 0xC0");

    // Private streams should get 0xBD
    TEST_ASSERT_EQ(getDefaultStreamID(StreamType::PRIVATE_DATA), 0xBD,
                   "Private should get stream ID 0xBD");

    return true;
}

// ============================================================================
// NOTE: MPEGTSMuxer class tests will be added when implementation is ready
// ============================================================================

// TODO: Add tests for MPEGTSMuxer class once we have implementation:
// - test_muxer_construction
// - test_muxer_add_stream
// - test_muxer_remove_stream
// - test_muxer_feed_data
// - test_muxer_get_output
// - test_muxer_configuration
// - test_muxer_private_data

// ============================================================================
// Main
// ============================================================================

int main() {
    return test::TestRegistry::instance().runAll();
}
