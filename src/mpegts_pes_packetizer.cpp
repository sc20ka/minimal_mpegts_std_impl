#include "mpegts_pes_packetizer.hpp"
#include <stdexcept>
#include <cstring>

namespace mpegts {

// ============================================================================
// Constructor / Destructor
// ============================================================================

PESPacketizer::PESPacketizer(uint8_t stream_id)
    : stream_id_(stream_id)
{
}

PESPacketizer::~PESPacketizer() {
}

// ============================================================================
// Public Methods
// ============================================================================

std::vector<uint8_t> PESPacketizer::createPESPacket(
    const uint8_t* es_data, size_t es_size,
    uint64_t pts, uint64_t dts,
    bool data_alignment)
{
    // Validate input data
    if (es_data == nullptr && es_size > 0) {
        throw std::invalid_argument("es_data is null but es_size is non-zero");
    }

    // Validate timestamps
    if (pts != NO_DTS && !isValidTimestamp(pts)) {
        throw std::invalid_argument("PTS exceeds 33-bit range");
    }
    if (dts != NO_DTS && !isValidTimestamp(dts)) {
        throw std::invalid_argument("DTS exceeds 33-bit range");
    }

    // Validate PTS/DTS relationship: DTS must be <= PTS
    if (pts != NO_DTS && dts != NO_DTS && dts > pts) {
        throw std::invalid_argument("DTS cannot be greater than PTS");
    }

    // For video streams (0xE0-0xEF), packet_length can be 0 (unbounded)
    // For audio streams (0xC0-0xDF), packet_length must be specified
    bool is_video = (stream_id_ >= STREAM_ID_VIDEO_STREAM_MIN &&
                     stream_id_ <= STREAM_ID_VIDEO_STREAM_MAX);

    size_t payload_size = is_video ? 0 : es_size;

    // Build PES header
    auto header = buildPESHeader(payload_size, pts, dts, data_alignment);

    // Combine header and payload
    std::vector<uint8_t> pes_packet;
    pes_packet.reserve(header.size() + es_size);
    pes_packet.insert(pes_packet.end(), header.begin(), header.end());
    pes_packet.insert(pes_packet.end(), es_data, es_data + es_size);

    // Update packet length field for non-video streams
    if (!is_video && payload_size > 0) {
        // packet_length = header_size (after first 6 bytes) + payload_size
        // Check BEFORE casting to uint16_t to detect overflow
        size_t total_length = (header.size() - 6) + es_size;
        if (total_length > 65535) {
            throw std::invalid_argument("PES packet too large for audio stream (max 65535 bytes after header)");
        }
        uint16_t packet_length = static_cast<uint16_t>(total_length);

        // CRITICAL FIX: Update bytes 4-5 directly instead of appending
        // Bytes 4-5 contain PES_packet_length field
        pes_packet[4] = (packet_length >> 8) & 0xFF;
        pes_packet[5] = packet_length & 0xFF;
    }

    return pes_packet;
}

std::vector<std::vector<uint8_t>> PESPacketizer::createPESPackets(
    const uint8_t* es_data, size_t es_size,
    size_t max_packet_size,
    uint64_t pts, uint64_t dts,
    bool data_alignment)
{
    // IMPORTANT NOTE: This method creates MULTIPLE independent PES packets,
    // NOT a single fragmented PES packet. Each packet has its own PES header
    // (except timestamps are only in the first packet).
    //
    // This is NOT standard MPEG-TS fragmentation where one large PES packet
    // is split across multiple TS packets. Instead, this splits elementary
    // stream data into multiple small PES packets.
    //
    // For standard TS packet fragmentation, use createPESPacket() followed
    // by TSPacketBuilder::build() which handles proper TS-level fragmentation.

    // Validate input data
    if (es_data == nullptr && es_size > 0) {
        throw std::invalid_argument("es_data is null but es_size is non-zero");
    }

    std::vector<std::vector<uint8_t>> packets;

    // Calculate header size
    size_t header_size = calculateHeaderSize(pts != NO_DTS, dts != NO_DTS);

    if (max_packet_size < header_size + 1) {
        throw std::invalid_argument("max_packet_size too small for PES header");
    }

    size_t offset = 0;
    bool first_packet = true;

    while (offset < es_size) {
        size_t max_payload = max_packet_size - header_size;
        size_t chunk_size = std::min(max_payload, es_size - offset);

        // Only include PTS/DTS in first packet
        uint64_t packet_pts = first_packet ? pts : NO_DTS;
        uint64_t packet_dts = first_packet ? dts : NO_DTS;
        bool packet_alignment = first_packet ? data_alignment : false;

        auto packet = createPESPacket(
            es_data + offset, chunk_size,
            packet_pts, packet_dts,
            packet_alignment);

        packets.push_back(std::move(packet));

        offset += chunk_size;
        first_packet = false;
    }

    return packets;
}

std::vector<uint8_t> PESPacketizer::buildPESHeader(
    size_t payload_size,
    uint64_t pts, uint64_t dts,
    bool data_alignment)
{
    std::vector<uint8_t> header;

    // Byte 0-2: packet_start_code_prefix (0x000001)
    write24(header, PES_START_CODE);

    // Byte 3: stream_id
    header.push_back(stream_id_);

    // Byte 4-5: PES_packet_length (placeholder, will be updated)
    // For video: 0 (unbounded)
    // For audio: actual length
    bool is_video = (stream_id_ >= STREAM_ID_VIDEO_STREAM_MIN &&
                     stream_id_ <= STREAM_ID_VIDEO_STREAM_MAX);

    if (is_video || payload_size == 0) {
        write16(header, 0);  // Unbounded
    } else {
        // Will be calculated: header_data_after_this + payload
        write16(header, 0);  // Placeholder
    }

    // Check if this stream requires optional PES header fields
    if (!requiresOptionalFields()) {
        // No optional fields for this stream type
        return header;
    }

    // === Optional PES Header Fields ===

    // Byte 6: marker bits + flags
    uint8_t byte6 = 0x80;  // '10' marker bits
    // PES_scrambling_control = 00
    // PES_priority = 0
    // data_alignment_indicator
    if (data_alignment) {
        byte6 |= 0x04;
    }
    // copyright = 0
    // original_or_copy = 0
    header.push_back(byte6);

    // Byte 7: PTS_DTS_flags + other flags
    uint8_t byte7 = 0x00;
    bool has_pts = (pts != NO_DTS);
    bool has_dts = (dts != NO_DTS);

    if (has_pts && has_dts) {
        byte7 |= 0xC0;  // '11' = both PTS and DTS
    } else if (has_pts) {
        byte7 |= 0x80;  // '10' = PTS only
    }
    // ESCR_flag = 0
    // ES_rate_flag = 0
    // DSM_trick_mode_flag = 0
    // additional_copy_info_flag = 0
    // PES_CRC_flag = 0
    // PES_extension_flag = 0
    header.push_back(byte7);

    // Byte 8: PES_header_data_length
    uint8_t header_data_length = 0;
    if (has_pts && has_dts) {
        header_data_length = 10;  // 5 bytes PTS + 5 bytes DTS
    } else if (has_pts) {
        header_data_length = 5;   // 5 bytes PTS
    }
    header.push_back(header_data_length);

    // Bytes 9+: Optional fields
    if (has_pts && has_dts) {
        // Encode PTS with marker '0011'
        auto pts_bytes = encodePTS(pts, 0x3);
        header.insert(header.end(), pts_bytes.begin(), pts_bytes.end());

        // Encode DTS with marker '0001'
        auto dts_bytes = encodeDTS(dts);
        header.insert(header.end(), dts_bytes.begin(), dts_bytes.end());
    } else if (has_pts) {
        // Encode PTS with marker '0010'
        auto pts_bytes = encodePTS(pts, 0x2);
        header.insert(header.end(), pts_bytes.begin(), pts_bytes.end());
    }

    return header;
}

void PESPacketizer::setStreamID(uint8_t stream_id) {
    stream_id_ = stream_id;
}

bool PESPacketizer::requiresOptionalFields() const {
    // Optional PES header fields are NOT present for:
    // - program_stream_map (0xBC)
    // - private_stream_2 (0xBF)
    // - ECM (0xF0)
    // - EMM (0xF1)
    // - program_stream_directory (0xFF)
    // - DSMCC_stream (0xF2)
    // - ITU-T Rec. H.222.1 type E (0xF8)

    if (stream_id_ == STREAM_ID_PROGRAM_STREAM_MAP ||
        stream_id_ == STREAM_ID_PRIVATE_STREAM_2 ||
        stream_id_ == STREAM_ID_ECM_STREAM ||
        stream_id_ == STREAM_ID_EMM_STREAM ||
        stream_id_ == STREAM_ID_PROGRAM_STREAM_DIRECTORY ||
        stream_id_ == STREAM_ID_DSMCC_STREAM ||
        stream_id_ == STREAM_ID_H222_E_STREAM) {
        return false;
    }

    return true;
}

size_t PESPacketizer::calculateHeaderSize(bool has_pts, bool has_dts) {
    // Basic header: 6 bytes (start code + stream_id + length)
    size_t size = 6;

    // For streams with optional fields:
    // + 3 bytes (marker + flags + header_data_length)
    // + 5 bytes per timestamp (PTS and/or DTS)

    // Assume stream requires optional fields (most common case)
    size += 3;  // Bytes 6-8

    if (has_pts && has_dts) {
        size += 10;  // 5 + 5
    } else if (has_pts) {
        size += 5;
    }

    return size;
}

std::vector<uint8_t> PESPacketizer::encodePTS(uint64_t pts, uint8_t marker) {
    std::vector<uint8_t> encoded(5);

    // Validate 33-bit range
    if (pts >= (1ULL << 33)) {
        pts &= ((1ULL << 33) - 1);  // Truncate to 33 bits
    }

    // Format: 'MMMP PTS[32:30] 1 PTS[29:15] 1 PTS[14:0] 1'
    // M = marker bits (4 bits)
    // P = PTS bits
    // 1 = marker bit

    // Byte 0: marker(4) + PTS[32:30](3) + marker_bit(1)
    encoded[0] = (marker << 4) | ((pts >> 29) & 0x0E) | 0x01;

    // Byte 1: PTS[29:22]
    encoded[1] = (pts >> 22) & 0xFF;

    // Byte 2: PTS[21:15] + marker_bit
    encoded[2] = ((pts >> 14) & 0xFE) | 0x01;

    // Byte 3: PTS[14:7]
    encoded[3] = (pts >> 7) & 0xFF;

    // Byte 4: PTS[6:0] + marker_bit
    encoded[4] = ((pts << 1) & 0xFE) | 0x01;

    return encoded;
}

std::vector<uint8_t> PESPacketizer::encodeDTS(uint64_t dts) {
    // DTS uses marker '0001' (0x1)
    return encodePTS(dts, 0x1);
}

bool PESPacketizer::isValidTimestamp(uint64_t timestamp) {
    return timestamp < (1ULL << 33);
}

// ============================================================================
// Private Methods
// ============================================================================

void PESPacketizer::write16(std::vector<uint8_t>& buffer, uint16_t value) {
    buffer.push_back((value >> 8) & 0xFF);
    buffer.push_back(value & 0xFF);
}

void PESPacketizer::write24(std::vector<uint8_t>& buffer, uint32_t value) {
    buffer.push_back((value >> 16) & 0xFF);
    buffer.push_back((value >> 8) & 0xFF);
    buffer.push_back(value & 0xFF);
}

} // namespace mpegts
