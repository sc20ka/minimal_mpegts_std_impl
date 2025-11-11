#include "mpegts_packet.hpp"
#include <cstring>

namespace mpegts {

TSPacket::TSPacket()
    : payload_data_(nullptr)
    , payload_size_(0)
    , has_adaptation_(false)
    , has_payload_(false)
    , valid_(false)
{
}

bool TSPacket::parse(const uint8_t* data) {
    if (!data) {
        valid_ = false;
        return false;
    }

    // Parse header
    if (!parseHeader(data)) {
        valid_ = false;
        return false;
    }

    size_t offset = 4; // After 4-byte header

    // Parse adaptation field if present
    if (header_.adaptation_control == AdaptationFieldControl::ADAPTATION_ONLY ||
        header_.adaptation_control == AdaptationFieldControl::ADAPTATION_PAYLOAD) {

        has_adaptation_ = true;
        if (!parseAdaptationField(data, offset)) {
            valid_ = false;
            return false;
        }
        offset += 1 + adaptation_field_.length; // 1 byte for length + field data
    }

    // Parse payload if present
    if (header_.adaptation_control == AdaptationFieldControl::PAYLOAD_ONLY ||
        header_.adaptation_control == AdaptationFieldControl::ADAPTATION_PAYLOAD) {

        has_payload_ = true;
        payload_data_ = data + offset;
        payload_size_ = MPEGTS_PACKET_SIZE - offset;
    }

    valid_ = true;
    return true;
}

bool TSPacket::parseHeader(const uint8_t* data) {
    // Byte 0: sync byte
    header_.sync_byte = data[0];
    if (header_.sync_byte != MPEGTS_SYNC_BYTE) {
        return false;
    }

    // Byte 1: TEI, PUSI, priority, PID[12:8]
    header_.transport_error_indicator = (data[1] >> 7) & 0x01;
    header_.payload_unit_start = (data[1] >> 6) & 0x01;
    header_.transport_priority = (data[1] >> 5) & 0x01;
    uint16_t pid_high = (data[1] & 0x1F);

    // Byte 2: PID[7:0]
    uint16_t pid_low = data[2];
    header_.pid = (pid_high << 8) | pid_low;

    // Byte 3: scrambling, adaptation control, CC
    header_.scrambling_control = (data[3] >> 6) & 0x03;
    uint8_t adapt_ctrl = (data[3] >> 4) & 0x03;
    header_.adaptation_control = static_cast<AdaptationFieldControl>(adapt_ctrl);
    header_.continuity_counter = data[3] & 0x0F;

    // Validate
    if (header_.transport_error_indicator) {
        return false;
    }

    if (header_.adaptation_control == AdaptationFieldControl::RESERVED) {
        return false;
    }

    return true;
}

bool TSPacket::parseAdaptationField(const uint8_t* data, size_t offset) {
    if (offset >= MPEGTS_PACKET_SIZE) {
        return false;
    }

    // Adaptation field length
    adaptation_field_.length = data[offset];

    if (adaptation_field_.length == 0) {
        return true; // Empty adaptation field is valid
    }

    if (offset + 1 + adaptation_field_.length > MPEGTS_PACKET_SIZE) {
        return false; // Adaptation field exceeds packet size
    }

    size_t field_offset = offset + 1;

    // Flags byte
    uint8_t flags = data[field_offset++];
    adaptation_field_.discontinuity_indicator = (flags >> 7) & 0x01;
    adaptation_field_.random_access_indicator = (flags >> 6) & 0x01;
    adaptation_field_.es_priority_indicator = (flags >> 5) & 0x01;
    adaptation_field_.pcr_flag = (flags >> 4) & 0x01;
    adaptation_field_.opcr_flag = (flags >> 3) & 0x01;
    adaptation_field_.splicing_point_flag = (flags >> 2) & 0x01;
    adaptation_field_.transport_private_data_flag = (flags >> 1) & 0x01;
    adaptation_field_.adaptation_extension_flag = flags & 0x01;

    // PCR (6 bytes)
    if (adaptation_field_.pcr_flag) {
        if (field_offset + 6 > offset + 1 + adaptation_field_.length) {
            return false;
        }
        // PCR parsing (simplified - not fully implemented yet)
        field_offset += 6;
    }

    // OPCR (6 bytes)
    if (adaptation_field_.opcr_flag) {
        if (field_offset + 6 > offset + 1 + adaptation_field_.length) {
            return false;
        }
        field_offset += 6;
    }

    // Splicing countdown (1 byte)
    if (adaptation_field_.splicing_point_flag) {
        if (field_offset + 1 > offset + 1 + adaptation_field_.length) {
            return false;
        }
        field_offset += 1;
    }

    // Transport private data
    if (adaptation_field_.transport_private_data_flag) {
        if (field_offset + 1 > offset + 1 + adaptation_field_.length) {
            return false;
        }

        adaptation_field_.private_data_length = data[field_offset++];

        if (field_offset + adaptation_field_.private_data_length >
            offset + 1 + adaptation_field_.length) {
            return false;
        }

        adaptation_field_.private_data = &data[field_offset];
        field_offset += adaptation_field_.private_data_length;
    }

    return true;
}

bool TSPacket::isValid() const {
    return valid_;
}

} // namespace mpegts
