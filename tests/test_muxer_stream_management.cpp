#include "test_framework.hpp"
#include "mpegts_muxer.hpp"
#include "mpegts_muxer_types.hpp"
#include "mpegts_psi.hpp"

using namespace mpegts;

// ============================================================================
// Constructor and Configuration Tests
// ============================================================================

TEST(muxer_construction_default) {
    MuxerConfig config;

    MPEGTSMuxer muxer(config);

    // Should construct without throwing
    return true;
}

TEST(muxer_construction_custom) {
    MuxerConfig config;
    config.bitrate = 10000000;
    config.mode = MuxMode::VBR;
    config.transport_stream_id = 42;
    config.program_number = 5;
    config.pcr_pid = 0x200;

    MPEGTSMuxer muxer(config);

    return true;
}

// ============================================================================
// Stream Addition Tests
// ============================================================================

TEST(muxer_add_single_video_stream) {
    MuxerConfig config;
    MPEGTSMuxer muxer(config);

    StreamConfig stream;
    stream.type = StreamType::H264_VIDEO;
    stream.pid = 0x100;
    stream.stream_id = 0xE0;
    stream.bitrate = 4000000;
    stream.width = 1920;
    stream.height = 1080;
    stream.frame_rate = 30;

    uint16_t pid = muxer.addStream(stream);

    TEST_ASSERT_EQ(pid, 0x100, "Should return correct PID");

    return true;
}

TEST(muxer_add_single_audio_stream) {
    MuxerConfig config;
    MPEGTSMuxer muxer(config);

    StreamConfig stream;
    stream.type = StreamType::AAC_AUDIO;
    stream.pid = 0x101;
    stream.stream_id = 0xC0;
    stream.bitrate = 128000;
    stream.sample_rate = 48000;
    stream.channels = 2;

    uint16_t pid = muxer.addStream(stream);

    TEST_ASSERT_EQ(pid, 0x101, "Should return correct PID");

    return true;
}

TEST(muxer_add_multiple_streams) {
    MuxerConfig config;
    MPEGTSMuxer muxer(config);

    // Add video stream
    StreamConfig video;
    video.type = StreamType::H264_VIDEO;
    video.pid = 0x100;
    video.bitrate = 4000000;
    video.width = 1920;
    video.height = 1080;
    video.frame_rate = 30;

    uint16_t video_pid = muxer.addStream(video);
    TEST_ASSERT_EQ(video_pid, 0x100, "Video PID should be correct");

    // Add audio stream
    StreamConfig audio;
    audio.type = StreamType::AAC_AUDIO;
    audio.pid = 0x101;
    audio.bitrate = 128000;
    audio.sample_rate = 48000;
    audio.channels = 2;

    uint16_t audio_pid = muxer.addStream(audio);
    TEST_ASSERT_EQ(audio_pid, 0x101, "Audio PID should be correct");

    return true;
}

TEST(muxer_add_private_data_stream) {
    MuxerConfig config;
    MPEGTSMuxer muxer(config);

    StreamConfig stream;
    stream.type = StreamType::PRIVATE_DATA;
    stream.pid = 0x102;
    stream.stream_id = 0xBD;
    stream.bitrate = 64000;
    stream.private_data_enabled = true;

    uint16_t pid = muxer.addStream(stream);

    TEST_ASSERT_EQ(pid, 0x102, "Should return correct PID");

    return true;
}

// ============================================================================
// PID Validation Tests
// ============================================================================

TEST(muxer_reject_duplicate_pid) {
    MuxerConfig config;
    MPEGTSMuxer muxer(config);

    StreamConfig stream1;
    stream1.type = StreamType::H264_VIDEO;
    stream1.pid = 0x100;
    stream1.width = 1920;
    stream1.height = 1080;
    stream1.frame_rate = 30;

    muxer.addStream(stream1);

    // Try to add another stream with same PID
    StreamConfig stream2;
    stream2.type = StreamType::AAC_AUDIO;
    stream2.pid = 0x100;  // Same PID!
    stream2.sample_rate = 48000;
    stream2.channels = 2;

    bool threw_exception = false;
    try {
        muxer.addStream(stream2);
    } catch (const std::invalid_argument&) {
        threw_exception = true;
    }

    TEST_ASSERT_TRUE(threw_exception, "Should throw for duplicate PID");

    return true;
}

TEST(muxer_reject_reserved_pid_pat) {
    MuxerConfig config;
    MPEGTSMuxer muxer(config);

    StreamConfig stream;
    stream.type = StreamType::H264_VIDEO;
    stream.pid = 0x0000;  // PAT PID
    stream.width = 1920;
    stream.height = 1080;
    stream.frame_rate = 30;

    bool threw_exception = false;
    try {
        muxer.addStream(stream);
    } catch (const std::invalid_argument&) {
        threw_exception = true;
    }

    TEST_ASSERT_TRUE(threw_exception, "Should throw for PAT PID");

    return true;
}

TEST(muxer_reject_reserved_pid_cat) {
    MuxerConfig config;
    MPEGTSMuxer muxer(config);

    StreamConfig stream;
    stream.type = StreamType::H264_VIDEO;
    stream.pid = 0x0001;  // CAT PID
    stream.width = 1920;
    stream.height = 1080;
    stream.frame_rate = 30;

    bool threw_exception = false;
    try {
        muxer.addStream(stream);
    } catch (const std::invalid_argument&) {
        threw_exception = true;
    }

    TEST_ASSERT_TRUE(threw_exception, "Should throw for CAT PID");

    return true;
}

TEST(muxer_reject_reserved_pid_null) {
    MuxerConfig config;
    MPEGTSMuxer muxer(config);

    StreamConfig stream;
    stream.type = StreamType::H264_VIDEO;
    stream.pid = 0x1FFF;  // Null packet PID
    stream.width = 1920;
    stream.height = 1080;
    stream.frame_rate = 30;

    bool threw_exception = false;
    try {
        muxer.addStream(stream);
    } catch (const std::invalid_argument&) {
        threw_exception = true;
    }

    TEST_ASSERT_TRUE(threw_exception, "Should throw for null packet PID");

    return true;
}

TEST(muxer_reject_invalid_pid) {
    MuxerConfig config;
    MPEGTSMuxer muxer(config);

    StreamConfig stream;
    stream.type = StreamType::H264_VIDEO;
    stream.pid = 0x2000;  // Beyond 13-bit range
    stream.width = 1920;
    stream.height = 1080;
    stream.frame_rate = 30;

    bool threw_exception = false;
    try {
        muxer.addStream(stream);
    } catch (const std::invalid_argument&) {
        threw_exception = true;
    }

    TEST_ASSERT_TRUE(threw_exception, "Should throw for invalid PID");

    return true;
}

// ============================================================================
// Stream Validation Tests
// ============================================================================

TEST(muxer_reject_zero_bitrate) {
    MuxerConfig config;
    MPEGTSMuxer muxer(config);

    StreamConfig stream;
    stream.type = StreamType::H264_VIDEO;
    stream.pid = 0x100;
    stream.bitrate = 0;  // Invalid
    stream.width = 1920;
    stream.height = 1080;
    stream.frame_rate = 30;

    bool threw_exception = false;
    try {
        muxer.addStream(stream);
    } catch (const std::invalid_argument&) {
        threw_exception = true;
    }

    TEST_ASSERT_TRUE(threw_exception, "Should throw for zero bitrate");

    return true;
}

TEST(muxer_reject_invalid_video_dimensions) {
    MuxerConfig config;
    MPEGTSMuxer muxer(config);

    StreamConfig stream;
    stream.type = StreamType::H264_VIDEO;
    stream.pid = 0x100;
    stream.bitrate = 4000000;
    stream.width = 0;  // Invalid
    stream.height = 1080;
    stream.frame_rate = 30;

    bool threw_exception = false;
    try {
        muxer.addStream(stream);
    } catch (const std::invalid_argument&) {
        threw_exception = true;
    }

    TEST_ASSERT_TRUE(threw_exception, "Should throw for invalid dimensions");

    return true;
}

TEST(muxer_reject_invalid_audio_sample_rate) {
    MuxerConfig config;
    MPEGTSMuxer muxer(config);

    StreamConfig stream;
    stream.type = StreamType::AAC_AUDIO;
    stream.pid = 0x101;
    stream.bitrate = 128000;
    stream.sample_rate = 0;  // Invalid
    stream.channels = 2;

    bool threw_exception = false;
    try {
        muxer.addStream(stream);
    } catch (const std::invalid_argument&) {
        threw_exception = true;
    }

    TEST_ASSERT_TRUE(threw_exception, "Should throw for invalid sample rate");

    return true;
}

// ============================================================================
// Stream Removal Tests
// ============================================================================

TEST(muxer_remove_stream) {
    MuxerConfig config;
    MPEGTSMuxer muxer(config);

    StreamConfig stream;
    stream.type = StreamType::H264_VIDEO;
    stream.pid = 0x100;
    stream.bitrate = 4000000;
    stream.width = 1920;
    stream.height = 1080;
    stream.frame_rate = 30;

    muxer.addStream(stream);
    muxer.removeStream(0x100);

    // Should be able to add stream with same PID again
    uint16_t pid = muxer.addStream(stream);
    TEST_ASSERT_EQ(pid, 0x100, "Should reuse PID after removal");

    return true;
}

TEST(muxer_remove_nonexistent_stream) {
    MuxerConfig config;
    MPEGTSMuxer muxer(config);

    // Should not throw when removing non-existent stream
    muxer.removeStream(0x999);

    return true;
}

// ============================================================================
// Data Feed Tests
// ============================================================================

TEST(muxer_feed_data_valid_stream) {
    MuxerConfig config;
    MPEGTSMuxer muxer(config);

    StreamConfig stream;
    stream.type = StreamType::H264_VIDEO;
    stream.pid = 0x100;
    stream.bitrate = 4000000;
    stream.width = 1920;
    stream.height = 1080;
    stream.frame_rate = 30;

    muxer.addStream(stream);

    // Feed some data
    uint8_t data[100] = {0};
    muxer.feedElementaryData(0x100, data, 100, 90000);

    return true;
}

TEST(muxer_feed_data_invalid_stream) {
    MuxerConfig config;
    MPEGTSMuxer muxer(config);

    uint8_t data[100] = {0};

    bool threw_exception = false;
    try {
        muxer.feedElementaryData(0x999, data, 100, 90000);
    } catch (const std::invalid_argument&) {
        threw_exception = true;
    }

    TEST_ASSERT_TRUE(threw_exception, "Should throw for invalid PID");

    return true;
}

// ============================================================================
// Output Tests
// ============================================================================

TEST(muxer_get_output_packets) {
    MuxerConfig config;
    MPEGTSMuxer muxer(config);

    StreamConfig stream;
    stream.type = StreamType::H264_VIDEO;
    stream.pid = 0x100;
    stream.bitrate = 4000000;
    stream.width = 1920;
    stream.height = 1080;
    stream.frame_rate = 30;

    muxer.addStream(stream);

    // Get output (should include PAT and PMT)
    auto output = muxer.getOutputPackets();

    TEST_ASSERT_TRUE(output.size() >= 188, "Should have at least one packet (PAT)");
    TEST_ASSERT_EQ(output.size() % 188, 0, "Output should be multiple of 188 bytes");

    return true;
}

TEST(muxer_output_contains_pat) {
    MuxerConfig config;
    MPEGTSMuxer muxer(config);

    StreamConfig stream;
    stream.type = StreamType::H264_VIDEO;
    stream.pid = 0x100;
    stream.bitrate = 4000000;
    stream.width = 1920;
    stream.height = 1080;
    stream.frame_rate = 30;

    muxer.addStream(stream);

    auto output = muxer.getOutputPackets();

    // Check for PAT (PID 0x0000)
    bool found_pat = false;
    for (size_t i = 0; i < output.size(); i += 188) {
        if (output[i] == 0x47) {  // Sync byte
            uint16_t pid = ((output[i+1] & 0x1F) << 8) | output[i+2];
            if (pid == 0x0000) {
                found_pat = true;
                break;
            }
        }
    }

    TEST_ASSERT_TRUE(found_pat, "Output should contain PAT");

    return true;
}

TEST(muxer_output_contains_pmt) {
    MuxerConfig config;
    MPEGTSMuxer muxer(config);

    StreamConfig stream;
    stream.type = StreamType::H264_VIDEO;
    stream.pid = 0x100;
    stream.bitrate = 4000000;
    stream.width = 1920;
    stream.height = 1080;
    stream.frame_rate = 30;

    muxer.addStream(stream);

    auto output = muxer.getOutputPackets();

    // Check for PMT (PID 0x1000)
    bool found_pmt = false;
    for (size_t i = 0; i < output.size(); i += 188) {
        if (output[i] == 0x47) {  // Sync byte
            uint16_t pid = ((output[i+1] & 0x1F) << 8) | output[i+2];
            if (pid == 0x1000) {
                found_pmt = true;
                break;
            }
        }
    }

    TEST_ASSERT_TRUE(found_pmt, "Output should contain PMT");

    return true;
}

// ============================================================================
// Private Data Manager Tests
// ============================================================================

TEST(muxer_add_private_data) {
    MuxerConfig config;
    MPEGTSMuxer muxer(config);

    StreamConfig stream;
    stream.type = StreamType::H264_VIDEO;
    stream.pid = 0x100;
    stream.bitrate = 4000000;
    stream.width = 1920;
    stream.height = 1080;
    stream.frame_rate = 30;

    muxer.addStream(stream);

    // Add private data
    uint8_t private_data[] = {0x01, 0x02, 0x03, 0x04};
    muxer.addPrivateData(0x100, private_data, sizeof(private_data));

    // Should not throw
    return true;
}

TEST(muxer_add_private_data_with_pts) {
    MuxerConfig config;
    MPEGTSMuxer muxer(config);

    StreamConfig stream;
    stream.type = StreamType::H264_VIDEO;
    stream.pid = 0x100;
    stream.bitrate = 4000000;
    stream.width = 1920;
    stream.height = 1080;
    stream.frame_rate = 30;

    muxer.addStream(stream);

    // Add private data with PTS
    uint8_t private_data[] = {0x01, 0x02, 0x03, 0x04};
    muxer.addPrivateDataWithPTS(0x100, private_data, sizeof(private_data), 90000);

    // Should not throw
    return true;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    return test::TestRegistry::instance().runAll();
}
