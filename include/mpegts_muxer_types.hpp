#ifndef MPEGTS_MUXER_TYPES_HPP
#define MPEGTS_MUXER_TYPES_HPP

#include "mpegts_types.hpp"
#include <functional>
#include <cstdint>

namespace mpegts {

// ============================================================================
// Muxer-specific Constants
// ============================================================================

constexpr uint64_t NO_DTS = 0xFFFFFFFFFFFFFFFF;  // Indicates no DTS present
constexpr uint32_t DEFAULT_PCR_INTERVAL_MS = 40;  // 40ms PCR interval
constexpr uint32_t DEFAULT_PAT_INTERVAL_MS = 100; // 100ms PAT interval
constexpr uint32_t DEFAULT_PMT_INTERVAL_MS = 100; // 100ms PMT interval
constexpr size_t MAX_PRIVATE_DATA_SIZE = 182;     // Max private data per packet

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief Stream type identifiers (ISO/IEC 13818-1)
 */
enum class StreamType : uint8_t {
    // Video
    VIDEO_MPEG1     = 0x01,  ///< MPEG-1 Video
    VIDEO_MPEG2     = 0x02,  ///< MPEG-2 Video
    VIDEO_H264      = 0x1B,  ///< H.264/AVC Video
    VIDEO_H265      = 0x24,  ///< H.265/HEVC Video

    // Audio
    AUDIO_MPEG1     = 0x03,  ///< MPEG-1 Audio (Layer I, II)
    AUDIO_MPEG2     = 0x04,  ///< MPEG-2 Audio
    AUDIO_AAC       = 0x0F,  ///< AAC Audio (ADTS)
    AUDIO_AAC_LATM  = 0x11,  ///< AAC Audio (LATM)
    AUDIO_AC3       = 0x81,  ///< AC-3 Audio (Dolby Digital)

    // Private/Data
    PRIVATE_DATA    = 0x06,  ///< Private data streams
    PRIVATE_PES     = 0x05   ///< Private PES packets
};

/**
 * @brief Muxing mode
 */
enum class MuxMode : uint8_t {
    CBR,  ///< Constant Bitrate
    VBR   ///< Variable Bitrate
};

/**
 * @brief Private data insertion strategy
 */
enum class PrivateDataInsertionMode : uint8_t {
    INSERT_WITH_PCR,      ///< Insert with PCR packets
    INSERT_STANDALONE,    ///< Create dedicated packets
    INSERT_WITH_PAYLOAD   ///< Insert with payload packets
};

// ============================================================================
// Configuration Structures
// ============================================================================

/**
 * @brief Configuration for a single stream
 */
struct StreamConfig {
    StreamType type;           ///< Stream type (H.264, AAC, etc.)
    uint16_t pid;             ///< Stream PID
    uint8_t stream_id;        ///< PES stream ID
    uint32_t bitrate;         ///< Stream bitrate (bps)
    bool pcr_enabled;         ///< Generate PCR for this stream

    // Private data support
    bool private_data_enabled;              ///< Enable private data insertion
    PrivateDataInsertionMode private_mode;  ///< Insertion strategy
    size_t max_private_data_size;           ///< Max bytes per packet

    // Video-specific
    uint16_t width;           ///< Video width (pixels)
    uint16_t height;          ///< Video height (pixels)
    uint8_t frame_rate;       ///< Frame rate (fps)

    // Audio-specific
    uint32_t sample_rate;     ///< Sample rate (Hz)
    uint8_t channels;         ///< Number of audio channels

    /**
     * @brief Default constructor
     */
    StreamConfig()
        : type(StreamType::VIDEO_H264)
        , pid(0x100)
        , stream_id(0xE0)
        , bitrate(4000000)
        , pcr_enabled(false)
        , private_data_enabled(false)
        , private_mode(PrivateDataInsertionMode::INSERT_WITH_PAYLOAD)
        , max_private_data_size(MAX_PRIVATE_DATA_SIZE)
        , width(1920)
        , height(1080)
        , frame_rate(30)
        , sample_rate(48000)
        , channels(2)
    {}
};

/**
 * @brief Configuration for the muxer
 */
struct MuxerConfig {
    uint32_t bitrate;              ///< Total bitrate (bps)
    MuxMode mode;                  ///< CBR or VBR
    uint16_t transport_stream_id;  ///< Transport stream ID
    uint16_t program_number;       ///< Program number
    uint16_t pcr_pid;              ///< PID for PCR insertion
    uint32_t pcr_interval_ms;      ///< PCR interval (milliseconds)
    uint32_t pat_interval_ms;      ///< PAT interval (milliseconds)
    uint32_t pmt_interval_ms;      ///< PMT interval (milliseconds)

    /**
     * @brief Default constructor
     */
    MuxerConfig()
        : bitrate(5000000)  // 5 Mbps
        , mode(MuxMode::CBR)
        , transport_stream_id(1)
        , program_number(1)
        , pcr_pid(0x100)
        , pcr_interval_ms(DEFAULT_PCR_INTERVAL_MS)
        , pat_interval_ms(DEFAULT_PAT_INTERVAL_MS)
        , pmt_interval_ms(DEFAULT_PMT_INTERVAL_MS)
    {}
};

// ============================================================================
// Callback Types
// ============================================================================

/**
 * @brief Callback for output packets
 *
 * @param data Pointer to packet data
 * @param length Size of data in bytes
 */
using OutputCallback = std::function<void(const uint8_t* data, size_t length)>;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Check if stream type is video
 */
inline bool isVideoStream(StreamType type) {
    return type == StreamType::VIDEO_MPEG1 ||
           type == StreamType::VIDEO_MPEG2 ||
           type == StreamType::VIDEO_H264 ||
           type == StreamType::VIDEO_H265;
}

/**
 * @brief Check if stream type is audio
 */
inline bool isAudioStream(StreamType type) {
    return type == StreamType::AUDIO_MPEG1 ||
           type == StreamType::AUDIO_MPEG2 ||
           type == StreamType::AUDIO_AAC ||
           type == StreamType::AUDIO_AAC_LATM ||
           type == StreamType::AUDIO_AC3;
}

/**
 * @brief Get default PES stream ID for stream type
 */
inline uint8_t getDefaultStreamID(StreamType type) {
    if (isVideoStream(type)) {
        return 0xE0;  // Video stream 0
    } else if (isAudioStream(type)) {
        return 0xC0;  // Audio stream 0
    } else {
        return 0xBD;  // Private stream 1
    }
}

} // namespace mpegts

#endif // MPEGTS_MUXER_TYPES_HPP
