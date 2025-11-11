#include "mpegts_demuxer.hpp"
#include <algorithm>
#include <cstring>

namespace mpegts {

MPEGTSDemuxer::MPEGTSDemuxer()
    : is_synchronized_(false)
    , sync_offset_(0)
    , sync_validation_depth_(3)
    , programs_table_available_(false)
{
    raw_buffer_.reserve(MAX_BUFFER_SIZE);
}

MPEGTSDemuxer::~MPEGTSDemuxer() {
}

void MPEGTSDemuxer::feedData(const uint8_t* data, size_t length) {
    if (!data || length == 0) {
        return;
    }

    // Add data to buffer
    raw_buffer_.insert(raw_buffer_.end(), data, data + length);

    // Prevent buffer overflow
    if (raw_buffer_.size() > MAX_BUFFER_SIZE) {
        // TODO: Implement proper buffer management
        // For now, keep only the last MAX_BUFFER_SIZE bytes
        size_t overflow = raw_buffer_.size() - MAX_BUFFER_SIZE;
        raw_buffer_.erase(raw_buffer_.begin(), raw_buffer_.begin() + overflow);
    }

    // Process buffer
    processBuffer();
}

void MPEGTSDemuxer::processBuffer() {
    // Try to find valid iteration if not synchronized
    if (!is_synchronized_) {
        if (tryFindValidIteration()) {
            is_synchronized_ = true;
        }
    }

    // TODO: Process synchronized packets
}

bool MPEGTSDemuxer::tryFindValidIteration() {
    // TODO: Implement 3-iteration validation algorithm
    // This is a placeholder implementation

    // Scan for sync bytes
    for (size_t i = 0; i + MPEGTS_PACKET_SIZE <= raw_buffer_.size(); ++i) {
        if (raw_buffer_[i] == MPEGTS_SYNC_BYTE) {
            // Try to validate packet
            TSPacket packet;
            if (packet.parse(&raw_buffer_[i])) {
                // Found potential sync position
                sync_offset_ = i;
                return true;
            }
        }
    }

    return false;
}

bool MPEGTSDemuxer::validatePacket(const uint8_t* data) {
    if (!data) {
        return false;
    }

    TSPacket packet;
    return packet.parse(data) && packet.isValid();
}

bool MPEGTSDemuxer::belongsToSameIteration(const TSPacket& p1, const TSPacket& p2) {
    const auto& h1 = p1.getHeader();
    const auto& h2 = p2.getHeader();

    // Check continuity counter
    uint8_t expected_cc = (h1.continuity_counter + 1) % 16;
    uint8_t actual_cc = h2.continuity_counter;

    if (actual_cc != expected_cc) {
        // Check for discontinuity indicator
        const auto* adapt = p2.getAdaptationField();
        if (!adapt || !adapt->discontinuity_indicator) {
            return false; // Unexpected CC jump
        }
    }

    // Check PID consistency (if payload exists)
    if (p1.hasPayload() && h1.pid != h2.pid) {
        return false;
    }

    return true;
}

void MPEGTSDemuxer::addPacketToStorage(const TSPacket& packet, uint32_t iter_id) {
    const auto& header = packet.getHeader();

    // Filter system PIDs
    if (isSystemPID(header.pid)) {
        return;
    }

    // Filter by program table if available
    if (programs_table_available_) {
        if (known_program_pids_.find(header.pid) == known_program_pids_.end()) {
            return; // Unknown PID, skip
        }
    }

    // TODO: Add packet data to storage
    // This is a placeholder
}

void MPEGTSDemuxer::handleDiscontinuity(uint16_t pid) {
    // TODO: Handle discontinuity detection
}

std::vector<ProgramInfo> MPEGTSDemuxer::getPrograms() const {
    std::vector<ProgramInfo> programs;

    for (const auto& [pid, stream] : storage_.getAllStreams()) {
        if (isProgramStream(pid)) {
            ProgramInfo info;
            info.program_number = 0; // Unknown without PAT/PMT
            info.stream_pids.push_back(pid);
            info.iteration_count = stream.getIterationCount();
            info.has_discontinuity = stream.hasDiscontinuity();

            // Calculate total payload size
            for (const auto& [iter_id, iter_data] : stream.getIterations()) {
                for (const auto& payload : iter_data.payloads) {
                    info.total_payload_size += payload.length;
                }
            }

            programs.push_back(info);
        }
    }

    return programs;
}

std::set<uint16_t> MPEGTSDemuxer::getDiscoveredPIDs() const {
    return storage_.getDiscoveredPIDs();
}

std::vector<IterationInfo> MPEGTSDemuxer::getIterationsSummary(uint16_t pid) const {
    std::vector<IterationInfo> result;

    const auto* stream = storage_.getStream(pid);
    if (!stream) {
        return result;
    }

    for (const auto& [iter_id, iter_data] : stream->getIterations()) {
        IterationInfo info;
        info.iteration_id = iter_id;
        info.has_discontinuity = iter_data.discontinuity_detected;
        info.cc_start = iter_data.first_cc;
        info.cc_end = iter_data.last_cc;
        info.packet_count = iter_data.packet_count;

        // Calculate payload sizes
        for (const auto& payload : iter_data.payloads) {
            if (payload.type == PayloadType::PAYLOAD_NORMAL) {
                info.payload_normal_size += payload.length;
            } else {
                info.payload_private_size += payload.length;
            }
        }

        result.push_back(info);
    }

    return result;
}

PayloadBuffer MPEGTSDemuxer::getPayload(uint16_t pid, uint32_t iter_id, PayloadType type) const {
    PayloadBuffer buffer;

    const auto* stream = storage_.getStream(pid);
    if (!stream) {
        return buffer;
    }

    const auto* iter_data = stream->getIteration(iter_id);
    if (!iter_data) {
        return buffer;
    }

    // Find first payload of requested type
    for (const auto& payload : iter_data->payloads) {
        if (payload.type == type) {
            buffer.data = payload.data;
            buffer.length = payload.length;
            buffer.type = payload.type;
            break;
        }
    }

    return buffer;
}

std::vector<PayloadBuffer> MPEGTSDemuxer::getAllPayloads(uint16_t pid, uint32_t iter_id) const {
    std::vector<PayloadBuffer> result;

    const auto* stream = storage_.getStream(pid);
    if (!stream) {
        return result;
    }

    const auto* iter_data = stream->getIteration(iter_id);
    if (!iter_data) {
        return result;
    }

    // Convert all payloads
    for (const auto& payload : iter_data->payloads) {
        PayloadBuffer buffer;
        buffer.data = payload.data;
        buffer.length = payload.length;
        buffer.type = payload.type;
        result.push_back(buffer);
    }

    return result;
}

void MPEGTSDemuxer::clearIteration(uint16_t pid, uint32_t iter_id) {
    auto& stream = storage_.getOrCreateStream(pid);
    stream.removeIteration(iter_id);
}

void MPEGTSDemuxer::clearStream(uint16_t pid) {
    storage_.clearStream(pid);
}

void MPEGTSDemuxer::clearAll() {
    storage_.clear();
    raw_buffer_.clear();
    is_synchronized_ = false;
}

size_t MPEGTSDemuxer::getBufferOccupancy() const {
    return raw_buffer_.size();
}

size_t MPEGTSDemuxer::getPacketCount() const {
    return raw_buffer_.size() / MPEGTS_PACKET_SIZE;
}

void MPEGTSDemuxer::setProgramsTable(const ProgramTable& table) {
    programs_table_available_ = true;
    known_program_pids_.clear();

    for (const auto& [prog_num, pids] : table.programs) {
        for (uint16_t pid : pids) {
            known_program_pids_.insert(pid);
        }
    }

    // Clear existing data
    storage_.clear();
}

} // namespace mpegts
