#ifndef MPEGTS_MUXER_HPP
#define MPEGTS_MUXER_HPP

#include "mpegts_muxer_types.hpp"
#include "mpegts_types.hpp"
#include "mpegts_packet.hpp"
#include <memory>
#include <map>
#include <queue>
#include <vector>
#include <cstdint>

namespace mpegts {

// Forward declarations for component classes
class TSPacketBuilder;
class PESPacketizer;
class PSIGenerator;
class PCRInjector;
class StreamScheduler;
class BitrateController;
class PrivateDataManager;

// ============================================================================
// MPEGTSMuxer - Main Muxer Class
// ============================================================================

/**
 * @brief Main MPEG-TS Muxer class
 *
 * Coordinates all muxing operations including stream management, PSI generation,
 * PCR injection, bitrate control, and private data handling.
 *
 * Architecture:
 * - TSPacketBuilder: Assembles 188-byte TS packets
 * - PESPacketizer: Converts ES to PES packets
 * - PSIGenerator: Generates PAT/PMT tables
 * - PCRInjector: Injects Program Clock Reference
 * - StreamScheduler: Manages multi-stream scheduling
 * - BitrateController: Controls CBR/VBR output
 * - PrivateDataManager: Handles private data insertion
 *
 * Usage:
 * @code
 * MuxerConfig config;
 * config.bitrate = 5000000;  // 5 Mbps
 * config.mode = MuxMode::CBR;
 *
 * MPEGTSMuxer muxer(config);
 *
 * StreamConfig video;
 * video.type = StreamType::VIDEO_H264;
 * video.pid = 0x100;
 * video.bitrate = 4000000;
 * uint16_t video_pid = muxer.addStream(video);
 *
 * StreamConfig audio;
 * audio.type = StreamType::AUDIO_AAC;
 * audio.pid = 0x101;
 * audio.bitrate = 128000;
 * uint16_t audio_pid = muxer.addStream(audio);
 *
 * muxer.feedElementaryData(video_pid, h264_data, h264_size, pts, dts);
 * muxer.feedElementaryData(audio_pid, aac_data, aac_size, pts);
 *
 * auto packets = muxer.getOutputPackets();
 * // Process output packets...
 * @endcode
 */
class MPEGTSMuxer {
public:
    /**
     * @brief Construct muxer with configuration
     * @param config Muxer configuration
     */
    explicit MPEGTSMuxer(const MuxerConfig& config);

    /**
     * @brief Destructor
     */
    ~MPEGTSMuxer();

    // Disable copy/move (for now)
    MPEGTSMuxer(const MPEGTSMuxer&) = delete;
    MPEGTSMuxer& operator=(const MPEGTSMuxer&) = delete;
    MPEGTSMuxer(MPEGTSMuxer&&) = delete;
    MPEGTSMuxer& operator=(MPEGTSMuxer&&) = delete;

    // ========================================================================
    // Stream Management
    // ========================================================================

    /**
     * @brief Add elementary stream to muxer
     * @param config Stream configuration
     * @return PID assigned to stream
     * @throws std::invalid_argument if PID already in use
     */
    uint16_t addStream(const StreamConfig& config);

    /**
     * @brief Remove stream from muxer
     * @param pid PID of stream to remove
     */
    void removeStream(uint16_t pid);

    // ========================================================================
    // Data Input
    // ========================================================================

    /**
     * @brief Feed elementary stream data
     * @param pid Stream PID
     * @param data Elementary stream data
     * @param length Data size in bytes
     * @param pts Presentation timestamp (90kHz)
     * @param dts Decode timestamp (90kHz), or NO_DTS if not present
     * @throws std::invalid_argument if PID not found
     */
    void feedElementaryData(uint16_t pid,
                           const uint8_t* data, size_t length,
                           uint64_t pts, uint64_t dts = NO_DTS);

    // ========================================================================
    // Output Control
    // ========================================================================

    /**
     * @brief Get output TS packets
     * @param max_packets Maximum packets to return (0 = all available)
     * @return Vector of TS packets (188 bytes each)
     */
    std::vector<uint8_t> getOutputPackets(size_t max_packets = 0);

    /**
     * @brief Set callback for packet output
     * @param callback Function called for each output packet
     */
    void setOutputCallback(OutputCallback callback);

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * @brief Set output bitrate
     * @param bitrate_bps Bitrate in bits per second
     */
    void setBitrate(uint32_t bitrate_bps);

    /**
     * @brief Set muxing mode
     * @param mode CBR or VBR
     */
    void setMode(MuxMode mode);

    /**
     * @brief Set PID for PCR insertion
     * @param pid PID to carry PCR
     */
    void setPCRPID(uint16_t pid);

    /**
     * @brief Set PCR insertion interval
     * @param interval_ms Interval in milliseconds (default: 40ms)
     */
    void setPCRInterval(uint32_t interval_ms);

    // ========================================================================
    // PSI Management
    // ========================================================================

    /**
     * @brief Set program number
     * @param program_num Program number (default: 1)
     */
    void setProgramNumber(uint16_t program_num);

    /**
     * @brief Set transport stream ID
     * @param tsid Transport stream ID (default: 1)
     */
    void setTransportStreamID(uint16_t tsid);

    /**
     * @brief Set PAT insertion interval
     * @param interval_ms Interval in milliseconds (default: 100ms)
     */
    void setPATInterval(uint32_t interval_ms);

    /**
     * @brief Set PMT insertion interval
     * @param interval_ms Interval in milliseconds (default: 100ms)
     */
    void setPMTInterval(uint32_t interval_ms);

    // ========================================================================
    // Private Data Management
    // ========================================================================

    /**
     * @brief Add private data for insertion
     * @param pid Stream PID
     * @param data Private data bytes
     * @param length Data size (max: 182 bytes per packet)
     */
    void addPrivateData(uint16_t pid,
                       const uint8_t* data, size_t length);

    /**
     * @brief Add private data with PTS synchronization
     * @param pid Stream PID
     * @param data Private data bytes
     * @param length Data size
     * @param pts PTS for synchronization
     */
    void addPrivateDataWithPTS(uint16_t pid,
                              const uint8_t* data, size_t length,
                              uint64_t pts);

    /**
     * @brief Set private data insertion mode
     * @param pid Stream PID
     * @param mode Insertion strategy
     */
    void setPrivateDataMode(uint16_t pid,
                           PrivateDataInsertionMode mode);

    /**
     * @brief Add dedicated private data stream
     * @param pid PID for private stream (stream_type = 0x06)
     * @param bitrate Bitrate for private stream (default: 64kbps)
     * @return PID of private stream
     */
    uint16_t addPrivateDataStream(uint16_t pid,
                                  uint32_t bitrate = 64000);

private:
    // ========================================================================
    // Configuration
    // ========================================================================

    MuxerConfig config_;

    // ========================================================================
    // Components
    // ========================================================================

    // Week 4: Only PSI Generator and Private Data Manager are implemented
    std::unique_ptr<PSIGenerator> psi_generator_;
    std::unique_ptr<PrivateDataManager> private_data_manager_;

    // These will be added in future weeks:
    // std::unique_ptr<StreamScheduler> scheduler_;       // Week 7
    // std::unique_ptr<PCRInjector> pcr_injector_;        // Week 6
    // std::unique_ptr<BitrateController> bitrate_controller_;  // Week 9

    // ========================================================================
    // Stream Context
    // ========================================================================

    /**
     * @brief Per-stream context
     */
    struct StreamContext {
        uint16_t pid;                                      ///< Stream PID
        StreamConfig config;                               ///< Stream configuration
        std::unique_ptr<TSPacketBuilder> ts_builder;       ///< TS packet builder
        std::unique_ptr<PESPacketizer> pes_packetizer;     ///< PES packetizer
        std::queue<std::vector<uint8_t>> packet_buffer;    ///< Buffered packets (188 bytes each)
    };

    std::map<uint16_t, StreamContext> streams_;

    // ========================================================================
    // Output Buffer
    // ========================================================================

    std::vector<std::vector<uint8_t>> output_buffer_;    ///< Output packets (188 bytes each)
    OutputCallback output_callback_;

    // ========================================================================
    // Internal Processing Methods
    // ========================================================================

    /**
     * @brief Process PSI table generation
     */
    void processPSI();

    /**
     * @brief Process PCR injection
     */
    void processPCR();

    /**
     * @brief Process stream data
     */
    void processStreams();

    /**
     * @brief Output packets to buffer or callback
     */
    void outputPackets();

    /**
     * @brief Validate stream configuration
     * @throws std::invalid_argument if config is invalid
     */
    void validateStreamConfig(const StreamConfig& config);

    /**
     * @brief Check if PID is available
     * @return true if PID not in use
     */
    bool isPIDAvailable(uint16_t pid) const;
};

} // namespace mpegts

#endif // MPEGTS_MUXER_HPP
