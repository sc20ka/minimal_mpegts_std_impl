#include "mpegts_pes.hpp"
#include <cstring>
#include <algorithm>

namespace mpegts {

// ============================================================================
// Timestamp Utilities
// ============================================================================

int64_t timestampDifference(const Timestamp& ts1, const Timestamp& ts2) {
    int64_t diff = static_cast<int64_t>(ts2.value) - static_cast<int64_t>(ts1.value);

    // Handle wraparound (33-bit timestamp)
    const int64_t TS_MAX = (1LL << 33);

    if (diff > TS_MAX / 2) {
        diff -= TS_MAX;
    } else if (diff < -TS_MAX / 2) {
        diff += TS_MAX;
    }

    return diff;
}

double timestampDifferenceMs(const Timestamp& ts1, const Timestamp& ts2) {
    int64_t diff = timestampDifference(ts1, ts2);
    return diff / 90.0;  // Convert from 90 kHz to milliseconds
}

// ============================================================================
// PES Parser
// ============================================================================

bool PESParser::verifyStartCode(const uint8_t* data) {
    if (!data) return false;
    return (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01);
}

std::optional<Timestamp> PESParser::extractTimestamp(const uint8_t* data) {
    if (!data) return std::nullopt;

    // PTS/DTS format (5 bytes):
    // '0010' or '0011' (4 bits) | PTS[32..30] | marker_bit | PTS[29..15] | marker_bit | PTS[14..0] | marker_bit

    uint64_t ts = 0;

    // Extract bits from 5-byte timestamp
    ts |= (static_cast<uint64_t>(data[0] & 0x0E) >> 1) << 30;  // PTS[32:30]
    ts |= static_cast<uint64_t>(data[1]) << 22;                // PTS[29:22]
    ts |= (static_cast<uint64_t>(data[2] & 0xFE) >> 1) << 15;  // PTS[21:15]
    ts |= static_cast<uint64_t>(data[3]) << 7;                 // PTS[14:7]
    ts |= (static_cast<uint64_t>(data[4] & 0xFE) >> 1);        // PTS[6:0]

    return Timestamp(ts);
}

bool PESParser::parseHeader(const uint8_t* data, size_t length, PESHeader& header) {
    if (!data || length < 6) {
        return false;
    }

    // Verify PES start code
    if (!verifyStartCode(data)) {
        return false;
    }

    header.start_code = PES_START_CODE;
    header.stream_id = data[3];
    header.packet_length = (static_cast<uint16_t>(data[4]) << 8) | data[5];

    // Check if optional fields are present
    // Optional fields are present for most stream IDs except:
    // - program_stream_map, private_stream_2, ECM, EMM, program_stream_directory,
    //   DSMCC_stream, H.222.1 type E stream

    bool has_optional = true;
    if (header.stream_id == STREAM_ID_PROGRAM_STREAM_MAP ||
        header.stream_id == STREAM_ID_PRIVATE_STREAM_2 ||
        header.stream_id == STREAM_ID_ECM_STREAM ||
        header.stream_id == STREAM_ID_EMM_STREAM ||
        header.stream_id == STREAM_ID_PROGRAM_STREAM_DIRECTORY ||
        header.stream_id == STREAM_ID_DSMCC_STREAM ||
        header.stream_id == STREAM_ID_H222_E_STREAM) {
        has_optional = false;
    }

    header.has_optional_fields = has_optional;

    if (!has_optional) {
        // No optional fields, header is complete
        return true;
    }

    // Parse optional fields (need at least 9 bytes total)
    if (length < 9) {
        return false;
    }

    // Byte 6: flags
    uint8_t flags1 = data[6];
    header.scrambling_control = (flags1 >> 4) & 0x03;
    header.priority = (flags1 & 0x08) != 0;
    header.data_alignment_indicator = (flags1 & 0x04) != 0;
    header.copyright = (flags1 & 0x02) != 0;
    header.original_or_copy = (flags1 & 0x01) != 0;

    // Byte 7: flags
    uint8_t flags2 = data[7];
    header.pts_dts_flags = (flags2 >> 6) & 0x03;
    header.has_pts = (header.pts_dts_flags == 0x02) || (header.pts_dts_flags == 0x03);
    header.has_dts = (header.pts_dts_flags == 0x03);

    header.escr_flag = (flags2 & 0x20) != 0;
    header.es_rate_flag = (flags2 & 0x10) != 0;
    header.dsm_trick_mode_flag = (flags2 & 0x08) != 0;
    header.additional_copy_info_flag = (flags2 & 0x04) != 0;
    header.crc_flag = (flags2 & 0x02) != 0;
    header.extension_flag = (flags2 & 0x01) != 0;

    // Byte 8: header data length
    header.header_data_length = data[8];

    // Check if we have enough data for the full header
    size_t total_header_size = 9 + header.header_data_length;
    if (length < total_header_size) {
        return false;
    }

    // Parse PTS/DTS if present
    size_t pos = 9;

    if (header.has_pts) {
        if (pos + 5 > length) return false;
        header.pts = extractTimestamp(&data[pos]);
        pos += 5;
    }

    if (header.has_dts) {
        if (pos + 5 > length) return false;
        header.dts = extractTimestamp(&data[pos]);
        pos += 5;
    }

    return true;
}

bool PESParser::parsePacket(const uint8_t* data, size_t length, PESPacket& packet) {
    // Parse header first
    if (!parseHeader(data, length, packet.header)) {
        return false;
    }

    // Calculate payload start position
    size_t header_size = packet.header.getHeaderSize();

    if (header_size > length) {
        return false;
    }

    // Extract payload
    size_t payload_size = length - header_size;

    packet.payload.resize(payload_size);
    if (payload_size > 0) {
        std::memcpy(packet.payload.data(), data + header_size, payload_size);
    }

    packet.complete = true;

    return true;
}

// ============================================================================
// PES Accumulator
// ============================================================================

PESAccumulator::PESAccumulator()
    : expected_length_(0)
    , synced_(false)
    , complete_(false)
{
    buffer_.reserve(65536);  // Reserve 64 KB
}

bool PESAccumulator::addData(const uint8_t* data, size_t length, bool payload_unit_start) {
    if (!data || length == 0) {
        return false;
    }

    if (payload_unit_start) {
        // New PES packet starts
        reset();
        synced_ = true;
    }

    if (!synced_) {
        return false;  // Wait for PUSI
    }

    // Add data to buffer
    buffer_.insert(buffer_.end(), data, data + length);

    // Check if packet is complete
    return parseAndCheckComplete();
}

bool PESAccumulator::getPacket(PESPacket& packet) {
    if (!complete_) {
        return false;
    }

    // Parse packet from buffer
    bool result = PESParser::parsePacket(buffer_.data(), buffer_.size(), packet);

    if (result) {
        reset();  // Clear buffer after successful extraction
    }

    return result;
}

void PESAccumulator::reset() {
    buffer_.clear();
    expected_length_ = 0;
    synced_ = false;
    complete_ = false;
}

bool PESAccumulator::parseAndCheckComplete() {
    // Need at least 6 bytes to determine packet length
    if (buffer_.size() < 6) {
        return false;
    }

    // Verify start code
    if (!PESParser::verifyStartCode(buffer_.data())) {
        reset();  // Invalid data, reset
        return false;
    }

    // Get packet length from header
    uint16_t pes_packet_length = (static_cast<uint16_t>(buffer_[4]) << 8) | buffer_[5];

    // Calculate expected total length
    // PES packet length field specifies the number of bytes following the packet_length field
    // So total = 6 + pes_packet_length

    if (pes_packet_length == 0) {
        // Unbounded packet (common for video)
        // For unbounded packets, we consider it complete when we have a valid header
        // The actual payload will continue until the next PUSI

        // Try to parse header to see if we have enough data
        PESHeader temp_header;
        if (PESParser::parseHeader(buffer_.data(), buffer_.size(), temp_header)) {
            size_t header_size = temp_header.getHeaderSize();
            if (buffer_.size() >= header_size) {
                // We have at least the header, mark as complete
                // (for unbounded packets, we'll get payload in next call with PUSI)
                expected_length_ = 0;
                complete_ = true;
                return true;
            }
        }
        return false;
    }

    expected_length_ = 6 + pes_packet_length;

    // Check if we have all data
    if (buffer_.size() >= expected_length_) {
        complete_ = true;
        return true;
    }

    return false;
}

// ============================================================================
// PES Manager
// ============================================================================

PESAccumulator& PESManager::getAccumulator(uint16_t pid) {
    auto it = accumulators_.find(pid);
    if (it == accumulators_.end()) {
        accumulators_[pid] = PESAccumulator();
        it = accumulators_.find(pid);
    }
    return it->second;
}

bool PESManager::hasAccumulator(uint16_t pid) const {
    return accumulators_.find(pid) != accumulators_.end();
}

void PESManager::removeAccumulator(uint16_t pid) {
    accumulators_.erase(pid);
}

void PESManager::clear() {
    accumulators_.clear();
}

std::vector<uint16_t> PESManager::getPIDs() const {
    std::vector<uint16_t> result;
    result.reserve(accumulators_.size());

    for (const auto& [pid, _] : accumulators_) {
        result.push_back(pid);
    }

    return result;
}

} // namespace mpegts
