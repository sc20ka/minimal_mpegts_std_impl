#ifndef MPEGTS_PES_PACKETIZER_HPP
#define MPEGTS_PES_PACKETIZER_HPP

#include "mpegts_pes.hpp"
#include "mpegts_types.hpp"
#include "mpegts_muxer_types.hpp"
#include <cstdint>
#include <vector>

namespace mpegts {

// ============================================================================
// PES Packetizer
// ============================================================================

/**
 * @brief Packetizes elementary stream data into PES packets
 *
 * Converts raw elementary stream data (video/audio) into PES packets with
 * proper headers, PTS/DTS timestamps, and size management.
 *
 * Features:
 * - PES header generation with PTS/DTS encoding
 * - Stream ID management per stream type
 * - Packet size control and fragmentation
 * - Data alignment support
 */
class PESPacketizer {
public:
    /**
     * @brief Construct packetizer for specific stream
     * @param stream_id PES stream ID (e.g., 0xE0 for video, 0xC0 for audio)
     */
    explicit PESPacketizer(uint8_t stream_id);

    /**
     * @brief Destructor
     */
    ~PESPacketizer();

    /**
     * @brief Create PES packet from elementary stream data
     * @param es_data Elementary stream data
     * @param es_size Size of elementary stream data
     * @param pts Presentation timestamp (90kHz), or NO_DTS if not present
     * @param dts Decode timestamp (90kHz), or NO_DTS if not present
     * @param data_alignment Set data alignment indicator
     * @return PES packet data (header + payload)
     */
    std::vector<uint8_t> createPESPacket(
        const uint8_t* es_data, size_t es_size,
        uint64_t pts = NO_DTS,
        uint64_t dts = NO_DTS,
        bool data_alignment = false);

    /**
     * @brief Create PES packet with maximum size limit
     * @param es_data Elementary stream data
     * @param es_size Size of elementary stream data
     * @param max_packet_size Maximum PES packet size (including header)
     * @param pts Presentation timestamp (90kHz), or NO_DTS if not present
     * @param dts Decode timestamp (90kHz), or NO_DTS if not present
     * @param data_alignment Set data alignment indicator
     * @return Vector of PES packets (may be fragmented)
     */
    std::vector<std::vector<uint8_t>> createPESPackets(
        const uint8_t* es_data, size_t es_size,
        size_t max_packet_size,
        uint64_t pts = NO_DTS,
        uint64_t dts = NO_DTS,
        bool data_alignment = false);

    /**
     * @brief Build PES header
     * @param payload_size Size of PES payload (0 = unbounded for video)
     * @param pts Presentation timestamp, or NO_DTS if not present
     * @param dts Decode timestamp, or NO_DTS if not present
     * @param data_alignment Set data alignment indicator
     * @return PES header bytes
     */
    std::vector<uint8_t> buildPESHeader(
        size_t payload_size,
        uint64_t pts = NO_DTS,
        uint64_t dts = NO_DTS,
        bool data_alignment = false);

    /**
     * @brief Set stream ID
     * @param stream_id New stream ID
     */
    void setStreamID(uint8_t stream_id);

    /**
     * @brief Get current stream ID
     * @return Stream ID
     */
    uint8_t getStreamID() const { return stream_id_; }

    /**
     * @brief Check if stream ID requires optional PES header fields
     * @return true if optional fields should be included
     */
    bool requiresOptionalFields() const;

    /**
     * @brief Calculate PES header size for given timestamps
     * @param has_pts Whether PTS is present
     * @param has_dts Whether DTS is present
     * @return Header size in bytes
     */
    static size_t calculateHeaderSize(bool has_pts, bool has_dts);

    /**
     * @brief Encode PTS into 5-byte format
     * @param pts 33-bit timestamp value
     * @param marker Marker bits (0010 for PTS only, 0011 for PTS with DTS)
     * @return 5 bytes containing encoded PTS
     */
    static std::vector<uint8_t> encodePTS(uint64_t pts, uint8_t marker = 0x2);

    /**
     * @brief Encode DTS into 5-byte format
     * @param dts 33-bit timestamp value
     * @return 5 bytes containing encoded DTS
     */
    static std::vector<uint8_t> encodeDTS(uint64_t dts);

    /**
     * @brief Validate timestamp (must be 33-bit value)
     * @param timestamp Timestamp to validate
     * @return true if valid
     */
    static bool isValidTimestamp(uint64_t timestamp);

private:
    uint8_t stream_id_;  ///< PES stream ID

    /**
     * @brief Write 16-bit value in big-endian
     */
    static void write16(std::vector<uint8_t>& buffer, uint16_t value);

    /**
     * @brief Write 32-bit value in big-endian (24-bit)
     */
    static void write24(std::vector<uint8_t>& buffer, uint32_t value);
};

} // namespace mpegts

#endif // MPEGTS_PES_PACKETIZER_HPP
