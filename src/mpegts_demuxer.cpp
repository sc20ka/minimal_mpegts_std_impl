#include "mpegts_demuxer.hpp"
#include <algorithm>
#include <cstring>

namespace mpegts {

MPEGTSDemuxer::MPEGTSDemuxer()
    : is_synchronized_(false)
    , sync_offset_(0)
    , sync_validation_depth_(3)
    , programs_table_available_(false)
    , total_packets_processed_(0)
{
    raw_buffer_.reserve(MAX_BUFFER_SIZE);
}

MPEGTSDemuxer::~MPEGTSDemuxer() {
    // Finalize all pending iterations using safe method
    finalizeAllIterations();
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
            // Continue processing after finding sync
        } else {
            return; // Wait for synchronization
        }
    }

    // Process synchronized packets
    while (sync_offset_ + MPEGTS_PACKET_SIZE <= raw_buffer_.size()) {
        const uint8_t* packet_data = &raw_buffer_[sync_offset_];

        // Validate sync byte
        if (packet_data[0] != MPEGTS_SYNC_BYTE) {
            // Lost synchronization
            is_synchronized_ = false;
            sync_offset_ = 0;
            return;
        }

        // Parse packet
        TSPacket packet;
        if (!packet.parse(packet_data) || !packet.isValid()) {
            // Invalid packet, try to resync
            is_synchronized_ = false;
            sync_offset_ = 0;
            return;
        }

        // Process PSI packets (PAT/PMT)
        processPSIPacket(packet);

        // Process PCR if present
        processPCR(packet);

        // Add packet to storage (accumulates in current iteration)
        addPacketToStorage(packet);

        // Move to next packet
        sync_offset_ += MPEGTS_PACKET_SIZE;
        total_packets_processed_++;
    }

    // Clean up processed data from buffer
    if (sync_offset_ > 0) {
        raw_buffer_.erase(raw_buffer_.begin(), raw_buffer_.begin() + sync_offset_);
        sync_offset_ = 0;
    }
}

bool MPEGTSDemuxer::tryFindValidIteration() {
    // 3-iteration validation algorithm
    // We need to find at least 3 valid packets to confirm synchronization

    const size_t min_buffer_for_sync = MPEGTS_PACKET_SIZE * 3;
    if (raw_buffer_.size() < min_buffer_for_sync) {
        return false; // Not enough data
    }

    // Scan for potential sync positions
    for (size_t start_pos = 0;
         start_pos + min_buffer_for_sync <= raw_buffer_.size();
         ++start_pos) {

        // Check for sync byte at this position
        if (raw_buffer_[start_pos] != MPEGTS_SYNC_BYTE) {
            continue;
        }

        // Try to parse first packet
        TSPacket packet1;
        if (!packet1.parse(&raw_buffer_[start_pos]) || !packet1.isValid()) {
            continue;
        }

        // Search for second valid packet (adaptive search)
        std::vector<TSPacket> candidates;
        candidates.push_back(packet1);

        size_t search_pos = start_pos + 1;
        size_t max_search = std::min(start_pos + MPEGTS_PACKET_SIZE * 10,
                                     raw_buffer_.size());

        while (candidates.size() < sync_validation_depth_ &&
               search_pos + MPEGTS_PACKET_SIZE <= max_search) {

            if (raw_buffer_[search_pos] == MPEGTS_SYNC_BYTE) {
                TSPacket packet_candidate;

                if (packet_candidate.parse(&raw_buffer_[search_pos]) &&
                    packet_candidate.isValid()) {

                    // Check if belongs to same iteration
                    if (candidates.empty() ||
                        belongsToSameIteration(candidates.back(), packet_candidate)) {

                        candidates.push_back(packet_candidate);

                        // After finding valid packet, assume next is 188 bytes away
                        search_pos += MPEGTS_PACKET_SIZE;
                        continue;
                    }
                }
            }

            // Adaptive skip: move one byte forward
            search_pos++;
        }

        // Check if we found 3 valid packets
        if (candidates.size() >= sync_validation_depth_) {
            // Verify they form a consistent sequence
            bool valid_sequence = true;

            for (size_t i = 1; i < candidates.size(); ++i) {
                if (!belongsToSameIteration(candidates[i-1], candidates[i])) {
                    valid_sequence = false;
                    break;
                }
            }

            if (valid_sequence) {
                // Found valid synchronization point!
                sync_offset_ = start_pos;
                return true;
            }
        }
    }

    return false; // No valid sync position found
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

void MPEGTSDemuxer::addPacketToStorage(const TSPacket& packet) {
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

    uint16_t pid = header.pid;

    // Check if we need to start a new iteration
    bool start_new_iteration = false;

    if (current_iterations_.find(pid) == current_iterations_.end()) {
        // First packet for this PID
        start_new_iteration = true;
    } else if (header.payload_unit_start) {
        // PUSI flag indicates start of new PES/section
        finalizeIteration(pid);
        start_new_iteration = true;
    }

    // Start new iteration if needed
    if (start_new_iteration) {
        current_iteration_ids_[pid] = storage_.generateIterationID();
        current_iterations_[pid] = IterationData();
        current_iterations_[pid].first_cc = header.continuity_counter;
        current_iterations_[pid].payload_unit_start_seen = header.payload_unit_start;
    }

    // Get current iteration for this PID
    auto& iter_data = current_iterations_[pid];

    // Update metadata
    iter_data.last_cc = header.continuity_counter;
    iter_data.packet_count++;

    // Check for discontinuity
    if (last_cc_.find(pid) != last_cc_.end()) {
        uint8_t expected_cc = (last_cc_[pid] + 1) % 16;
        if (header.continuity_counter != expected_cc) {
            const auto* adapt = packet.getAdaptationField();
            if (adapt && adapt->discontinuity_indicator) {
                iter_data.discontinuity_detected = true;
            }
        }
    }
    last_cc_[pid] = header.continuity_counter;

    // Extract private data from adaptation field
    if (packet.getPrivateDataLength() > 0) {
        const uint8_t* private_data = packet.getPrivateData();
        size_t private_len = packet.getPrivateDataLength();

        // Store private data in buffer
        size_t offset = iter_data.payload_data.size();
        iter_data.payload_data.insert(iter_data.payload_data.end(),
                                      private_data,
                                      private_data + private_len);

        // Create segment pointing to stored data
        PayloadSegment private_segment;
        private_segment.type = PayloadType::PAYLOAD_PRIVATE;
        private_segment.data = nullptr; // Will be set after adding to storage
        private_segment.length = private_len;
        private_segment.offset_in_stream = offset;

        iter_data.payloads.push_back(private_segment);
    }

    // Extract normal payload
    if (packet.hasPayload() && packet.getPayloadSize() > 0) {
        const uint8_t* payload = packet.getPayload();
        size_t payload_len = packet.getPayloadSize();

        // Store payload data in buffer
        size_t offset = iter_data.payload_data.size();
        iter_data.payload_data.insert(iter_data.payload_data.end(),
                                      payload,
                                      payload + payload_len);

        // Create segment pointing to stored data
        PayloadSegment normal_segment;
        normal_segment.type = PayloadType::PAYLOAD_NORMAL;
        normal_segment.data = nullptr; // Will be set after adding to storage
        normal_segment.length = payload_len;
        normal_segment.offset_in_stream = offset;

        iter_data.payloads.push_back(normal_segment);
    }
}

void MPEGTSDemuxer::finalizeIteration(uint16_t pid) {
    auto it = current_iterations_.find(pid);
    if (it == current_iterations_.end()) {
        return; // No current iteration
    }

    // Add iteration to storage
    auto& stream = storage_.getOrCreateStream(pid);
    uint32_t iter_id = current_iteration_ids_[pid];
    stream.addIteration(iter_id, it->second);

    // Clear current iteration
    current_iterations_.erase(pid);
    current_iteration_ids_.erase(pid);
}

void MPEGTSDemuxer::finalizeAllIterations() {
    // Create a copy of PIDs to avoid iterator invalidation
    std::vector<uint16_t> pids;
    for (const auto& [pid, _] : current_iterations_) {
        pids.push_back(pid);
    }

    // Finalize each iteration
    for (uint16_t pid : pids) {
        finalizeIteration(pid);
    }
}

void MPEGTSDemuxer::handleDiscontinuity(uint16_t pid) {
    // TODO: Handle discontinuity detection
}

std::vector<ProgramInfo> MPEGTSDemuxer::getPrograms() const {
    // Finalize pending iterations first
    const_cast<MPEGTSDemuxer*>(this)->finalizeAllIterations();

    std::vector<ProgramInfo> programs;

    // If we have parsed PMTs, use them to build program information
    if (!parsed_pmts_.empty()) {
        for (const auto& [prog_num, pmt] : parsed_pmts_) {
            ProgramInfo info;
            info.program_number = prog_num;

            // Add all elementary stream PIDs from PMT
            for (const auto& stream_info : pmt.streams) {
                info.stream_pids.push_back(stream_info.elementary_pid);

                // Collect statistics from storage if available
                const auto* stream = storage_.getStream(stream_info.elementary_pid);
                if (stream) {
                    info.iteration_count += stream->getIterationCount();
                    if (stream->hasDiscontinuity()) {
                        info.has_discontinuity = true;
                    }

                    // Calculate total payload size
                    for (const auto& [iter_id, iter_data] : stream->getIterations()) {
                        for (const auto& payload : iter_data.payloads) {
                            info.total_payload_size += payload.length;
                        }
                    }
                }
            }

            programs.push_back(info);
        }
    }
    // Fallback: if no PMT parsed, return discovered PIDs as separate programs
    else {
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
    }

    return programs;
}

std::set<uint16_t> MPEGTSDemuxer::getDiscoveredPIDs() const {
    // Finalize pending iterations first
    const_cast<MPEGTSDemuxer*>(this)->finalizeAllIterations();

    return storage_.getDiscoveredPIDs();
}

std::vector<IterationInfo> MPEGTSDemuxer::getIterationsSummary(uint16_t pid) const {
    // Finalize pending iterations first
    const_cast<MPEGTSDemuxer*>(this)->finalizeAllIterations();

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
    // Finalize all pending iterations
    for (const auto& [pid, _] : current_iterations_) {
        finalizeIteration(pid);
    }

    storage_.clear();
    raw_buffer_.clear();
    is_synchronized_ = false;
    current_iterations_.clear();
    current_iteration_ids_.clear();
    last_cc_.clear();
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

void MPEGTSDemuxer::processPSIPacket(const TSPacket& packet) {
    const auto& header = packet.getHeader();

    // Check if this is a PAT packet (PID 0x0000)
    if (header.pid == 0x0000 && packet.hasPayload()) {
        const uint8_t* payload = packet.getPayload();
        size_t payload_len = packet.getPayloadSize();

        // Add data to PAT accumulator
        if (pat_accumulator_.addData(payload, payload_len, header.payload_unit_start)) {
            // Section complete, try to parse PAT
            std::vector<uint8_t> section;
            if (pat_accumulator_.getSection(section) > 0) {
                PAT pat;
                if (PSIParser::parsePAT(section.data(), section.size(), pat)) {
                    // Successfully parsed PAT
                    parsed_pat_ = pat;

                    // Create accumulators for discovered PMT PIDs
                    for (const auto& entry : pat.programs) {
                        if (entry.program_number != 0) {  // Skip NIT
                            pmt_accumulators_[entry.pid] = PSIAccumulator();
                        }
                    }
                }
            }
        }
    }
    // Check if this is a PMT packet (discovered from PAT)
    else if (pmt_accumulators_.find(header.pid) != pmt_accumulators_.end() &&
             packet.hasPayload()) {
        const uint8_t* payload = packet.getPayload();
        size_t payload_len = packet.getPayloadSize();

        // Add data to PMT accumulator
        auto& pmt_acc = pmt_accumulators_[header.pid];
        if (pmt_acc.addData(payload, payload_len, header.payload_unit_start)) {
            // Section complete, try to parse PMT
            std::vector<uint8_t> section;
            if (pmt_acc.getSection(section) > 0) {
                PMT pmt;
                if (PSIParser::parsePMT(section.data(), section.size(), pmt)) {
                    // Successfully parsed PMT
                    parsed_pmts_[pmt.program_number] = pmt;
                }
            }
        }
    }
}

void MPEGTSDemuxer::processPCR(const TSPacket& packet) {
    const auto& header = packet.getHeader();

    // Check if packet has adaptation field with PCR
    const auto* adaptation = packet.getAdaptationField();
    if (!adaptation || !adaptation->pcr_flag) {
        return;
    }

    // Create PCR from parsed adaptation field data
    PCR pcr(adaptation->pcr_base, adaptation->pcr_extension);

    // Validate and add to manager
    if (pcr.isValid()) {
        pcr_manager_.addPCR(header.pid, pcr,
                           total_packets_processed_, header.continuity_counter);
    }
}

// ============================================================================
// PCR API Methods
// ============================================================================

std::optional<PCRStats> MPEGTSDemuxer::getPCRStats(uint16_t pid) const {
    const PCRTracker* tracker = pcr_manager_.getTracker(pid);
    if (tracker) {
        return tracker->getStats();
    }
    return std::nullopt;
}

std::vector<PCRStats> MPEGTSDemuxer::getAllPCRStats() const {
    return pcr_manager_.getAllStats();
}

std::vector<uint16_t> MPEGTSDemuxer::getPIDsWithPCR() const {
    return pcr_manager_.getPIDsWithPCR();
}

std::optional<PCR> MPEGTSDemuxer::getLastPCR(uint16_t pid) const {
    const PCRTracker* tracker = pcr_manager_.getTracker(pid);
    if (tracker) {
        return tracker->getLastPCR();
    }
    return std::nullopt;
}

} // namespace mpegts
