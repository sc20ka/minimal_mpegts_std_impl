#include "mpegts_ts_packet_builder.hpp"
#include <cstring>
#include <algorithm>

namespace mpegts {

// ============================================================================
// Constructor
// ============================================================================

TSPacketBuilder::TSPacketBuilder(uint16_t pid)
    : pid_(pid & 0x1FFF)  // 13-bit PID
    , continuity_counter_(0)
{
}

// ============================================================================
// Main Build Method
// ============================================================================

std::vector<std::vector<uint8_t>> TSPacketBuilder::build(
    const uint8_t* payload, size_t length,
    const BuildOptions& options)
{
    std::vector<std::vector<uint8_t>> packets;

    if (length == 0) {
        // No payload - create single packet with adaptation field only
        std::vector<uint8_t> packet(MPEGTS_PACKET_SIZE, 0xFF);
        buildHeader(packet.data(), options.pusi,
                   AdaptationFieldControl::ADAPTATION_ONLY);

        size_t adaptation_size = buildAdaptationField(
            packet.data() + 4, options, MPEGTS_PACKET_SIZE - 4);

        packets.push_back(std::move(packet));
        return packets;
    }

    // Calculate adaptation field size for first packet
    size_t adaptation_size = 0;
    bool needs_adaptation = options.has_pcr || options.has_private_data ||
                           options.random_access || options.discontinuity;

    if (needs_adaptation) {
        adaptation_size = calculateAdaptationFieldSize(options);
    }

    // Available payload space in first packet
    size_t header_size = 4;
    size_t first_payload_capacity = MPEGTS_PACKET_SIZE - header_size - adaptation_size;

    // If payload doesn't fit, we need stuffing in adaptation field
    size_t stuffing_needed = 0;
    if (length < first_payload_capacity && !needs_adaptation) {
        // Need adaptation field for stuffing
        stuffing_needed = first_payload_capacity - length;
        adaptation_size = 1 + 1 + stuffing_needed;  // length + flags + stuffing
        first_payload_capacity = length;
    } else if (length < first_payload_capacity) {
        // Already have adaptation field, add stuffing to it
        stuffing_needed = first_payload_capacity - length;
        adaptation_size += stuffing_needed;
        first_payload_capacity = length;
    }

    size_t payload_offset = 0;
    bool is_first_packet = true;

    while (payload_offset < length || is_first_packet) {
        std::vector<uint8_t> packet(MPEGTS_PACKET_SIZE, 0xFF);

        size_t packet_payload_capacity;
        AdaptationFieldControl adapt_ctrl;
        bool use_pusi = is_first_packet && options.pusi;

        if (is_first_packet && adaptation_size > 0) {
            // First packet with adaptation field
            adapt_ctrl = AdaptationFieldControl::ADAPTATION_PAYLOAD;
            packet_payload_capacity = first_payload_capacity;

            // Build header
            buildHeader(packet.data(), use_pusi, adapt_ctrl);

            // Build adaptation field
            buildAdaptationField(packet.data() + 4, options, stuffing_needed);

            // Copy payload
            size_t to_copy = std::min(length - payload_offset, packet_payload_capacity);
            std::memcpy(packet.data() + 4 + adaptation_size,
                       payload + payload_offset, to_copy);
            payload_offset += to_copy;

            is_first_packet = false;
        } else {
            // Subsequent packets OR first packet without adaptation field
            adapt_ctrl = AdaptationFieldControl::PAYLOAD_ONLY;
            packet_payload_capacity = MPEGTS_PACKET_SIZE - 4;

            // Copy payload
            size_t to_copy = std::min(length - payload_offset, packet_payload_capacity);
            std::memcpy(packet.data() + 4, payload + payload_offset, to_copy);
            payload_offset += to_copy;

            // If last packet has remaining space, add adaptation field with stuffing
            if (to_copy < packet_payload_capacity) {
                size_t remaining = packet_payload_capacity - to_copy;

                // Update adaptation control
                adapt_ctrl = AdaptationFieldControl::ADAPTATION_PAYLOAD;

                // Create adaptation field with stuffing
                uint8_t* adapt_ptr = packet.data() + 4;
                adapt_ptr[0] = remaining - 1;  // adaptation_field_length
                if (remaining > 1) {
                    adapt_ptr[1] = 0x00;  // flags = 0
                    // Rest is already 0xFF (stuffing)
                }

                // Move payload after adaptation field
                std::memmove(packet.data() + 4 + remaining,
                           packet.data() + 4, to_copy);
            }

            // Build header AFTER determining adaptation control
            buildHeader(packet.data(), use_pusi, adapt_ctrl);

            is_first_packet = false;
        }

        packets.push_back(std::move(packet));

        if (payload_offset >= length) {
            break;
        }
    }

    return packets;
}

// ============================================================================
// NULL Packet
// ============================================================================

std::vector<uint8_t> TSPacketBuilder::createNullPacket() {
    std::vector<uint8_t> packet(MPEGTS_PACKET_SIZE, 0xFF);

    // Header for NULL packet (PID = 0x1FFF)
    packet[0] = MPEGTS_SYNC_BYTE;
    packet[1] = 0x1F;  // PID high bits
    packet[2] = 0xFF;  // PID low bits
    packet[3] = 0x10;  // adaptation_field_control = 01 (payload only), CC = 0

    // Payload is all 0xFF (already set)

    return packet;
}

// ============================================================================
// Continuity Counter Management
// ============================================================================

void TSPacketBuilder::resetContinuityCounter() {
    continuity_counter_ = 0;
}

void TSPacketBuilder::incrementCC() {
    continuity_counter_ = (continuity_counter_ + 1) & 0x0F;
}

// ============================================================================
// Header Building
// ============================================================================

void TSPacketBuilder::buildHeader(uint8_t* buffer, bool pusi,
                                  AdaptationFieldControl adaptation_control) {
    // Byte 0: sync_byte
    buffer[0] = MPEGTS_SYNC_BYTE;

    // Byte 1: TEI (0), PUSI (1 bit), Priority (0), PID high (5 bits)
    buffer[1] = (pusi ? 0x40 : 0x00) | ((pid_ >> 8) & 0x1F);

    // Byte 2: PID low (8 bits)
    buffer[2] = pid_ & 0xFF;

    // Byte 3: Scrambling (00), Adaptation (2 bits), CC (4 bits)
    buffer[3] = (static_cast<uint8_t>(adaptation_control) << 4) | continuity_counter_;

    // Increment CC if packet has payload
    if (adaptation_control == AdaptationFieldControl::PAYLOAD_ONLY ||
        adaptation_control == AdaptationFieldControl::ADAPTATION_PAYLOAD) {
        incrementCC();
    }
}

// ============================================================================
// Adaptation Field Building
// ============================================================================

size_t TSPacketBuilder::buildAdaptationField(uint8_t* buffer,
                                             const BuildOptions& options,
                                             size_t stuffing_needed) {
    size_t base_size = calculateAdaptationFieldSize(options);
    size_t total_size = base_size + stuffing_needed;

    if (total_size == 0) {
        return 0;
    }

    uint8_t* ptr = buffer;

    // Adaptation field length (excluding this byte)
    *ptr++ = total_size - 1;

    if (total_size == 1) {
        // Only length byte, no flags
        return 1;
    }

    // Flags byte
    uint8_t flags = 0;
    if (options.discontinuity) flags |= 0x80;
    if (options.random_access) flags |= 0x40;
    if (options.has_pcr) flags |= 0x10;
    if (options.has_private_data) flags |= 0x02;

    *ptr++ = flags;

    // PCR (6 bytes)
    if (options.has_pcr) {
        encodePCR(ptr, options.pcr_value);
        ptr += 6;
    }

    // Private data
    if (options.has_private_data && options.private_data_length > 0) {
        *ptr++ = options.private_data_length;
        std::memcpy(ptr, options.private_data, options.private_data_length);
        ptr += options.private_data_length;
    }

    // Stuffing bytes (0xFF)
    if (stuffing_needed > 0) {
        std::memset(ptr, 0xFF, stuffing_needed);
        ptr += stuffing_needed;
    }

    return total_size;
}

// ============================================================================
// Adaptation Field Size Calculation
// ============================================================================

size_t TSPacketBuilder::calculateAdaptationFieldSize(const BuildOptions& options) const {
    if (!options.has_pcr && !options.has_private_data &&
        !options.random_access && !options.discontinuity) {
        return 0;
    }

    size_t size = 2;  // length byte + flags byte

    if (options.has_pcr) {
        size += 6;  // PCR is 6 bytes
    }

    if (options.has_private_data && options.private_data_length > 0) {
        size += 1 + options.private_data_length;  // length + data
    }

    return size;
}

// ============================================================================
// PCR Encoding
// ============================================================================

void TSPacketBuilder::encodePCR(uint8_t* buffer, uint64_t pcr_value) {
    // PCR = PCR_base * 300 + PCR_ext
    // PCR_base: 33 bits @ 90 kHz
    // PCR_ext: 9 bits @ 27 MHz

    uint64_t pcr_base = pcr_value / 300;
    uint16_t pcr_ext = pcr_value % 300;

    // PCR encoding (6 bytes):
    // 33 bits PCR_base + 6 reserved bits + 9 bits PCR_ext

    buffer[0] = (pcr_base >> 25) & 0xFF;
    buffer[1] = (pcr_base >> 17) & 0xFF;
    buffer[2] = (pcr_base >> 9) & 0xFF;
    buffer[3] = (pcr_base >> 1) & 0xFF;
    buffer[4] = ((pcr_base & 0x01) << 7) | 0x7E | ((pcr_ext >> 8) & 0x01);
    buffer[5] = pcr_ext & 0xFF;
}

} // namespace mpegts
