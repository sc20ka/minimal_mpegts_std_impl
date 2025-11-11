#include "mpegts_storage.hpp"
#include <algorithm>

namespace mpegts {

// ============================================================================
// StreamIterations Implementation
// ============================================================================

StreamIterations::StreamIterations(uint16_t pid)
    : pid_(pid)
{
}

void StreamIterations::addIteration(uint32_t iter_id, const IterationData& data) {
    iterations_.emplace_back(iter_id, data);

    // Track observed CC values
    observed_cc_values_.insert(data.first_cc);
    observed_cc_values_.insert(data.last_cc);
}

const IterationData* StreamIterations::getIteration(uint32_t iter_id) const {
    auto it = std::find_if(iterations_.begin(), iterations_.end(),
        [iter_id](const auto& pair) { return pair.first == iter_id; });

    return (it != iterations_.end()) ? &it->second : nullptr;
}

void StreamIterations::removeIteration(uint32_t iter_id) {
    auto it = std::find_if(iterations_.begin(), iterations_.end(),
        [iter_id](const auto& pair) { return pair.first == iter_id; });

    if (it != iterations_.end()) {
        iterations_.erase(it);
    }
}

void StreamIterations::clear() {
    iterations_.clear();
    observed_cc_values_.clear();
}

bool StreamIterations::hasDiscontinuity() const {
    for (const auto& [iter_id, data] : iterations_) {
        if (data.discontinuity_detected) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// DemuxerStreamStorage Implementation
// ============================================================================

DemuxerStreamStorage::DemuxerStreamStorage()
    : next_iteration_id_(1)
{
}

StreamIterations& DemuxerStreamStorage::getOrCreateStream(uint16_t pid) {
    auto it = streams_.find(pid);
    if (it == streams_.end()) {
        it = streams_.emplace(pid, StreamIterations(pid)).first;
    }
    return it->second;
}

const StreamIterations* DemuxerStreamStorage::getStream(uint16_t pid) const {
    auto it = streams_.find(pid);
    return (it != streams_.end()) ? &it->second : nullptr;
}

uint32_t DemuxerStreamStorage::generateIterationID() {
    return next_iteration_id_++;
}

void DemuxerStreamStorage::clearStream(uint16_t pid) {
    auto it = streams_.find(pid);
    if (it != streams_.end()) {
        it->second.clear();
    }
}

void DemuxerStreamStorage::clear() {
    streams_.clear();
    next_iteration_id_ = 1;
}

std::set<uint16_t> DemuxerStreamStorage::getDiscoveredPIDs() const {
    std::set<uint16_t> pids;
    for (const auto& [pid, stream] : streams_) {
        pids.insert(pid);
    }
    return pids;
}

bool DemuxerStreamStorage::hasStream(uint16_t pid) const {
    return streams_.find(pid) != streams_.end();
}

} // namespace mpegts
