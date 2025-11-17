#include "mpegts_private_data_manager.hpp"

namespace mpegts {

// ============================================================================
// Constructor / Destructor
// ============================================================================

PrivateDataManager::PrivateDataManager() {
}

PrivateDataManager::~PrivateDataManager() {
}

// ============================================================================
// Public Methods
// ============================================================================

bool PrivateDataManager::addPrivateData(uint16_t pid,
                                        const uint8_t* data, size_t length) {
    if (length == 0 || length > MAX_PRIVATE_DATA_SIZE) {
        return false;
    }

    PrivateDataEntry entry(data, length, NO_DTS, false);
    streams_[pid].queue.push(entry);
    return true;
}

bool PrivateDataManager::addPrivateDataWithPTS(uint16_t pid,
                                               const uint8_t* data, size_t length,
                                               uint64_t pts) {
    if (length == 0 || length > MAX_PRIVATE_DATA_SIZE) {
        return false;
    }

    PrivateDataEntry entry(data, length, pts, true);
    streams_[pid].queue.push(entry);
    return true;
}

void PrivateDataManager::setInsertionMode(uint16_t pid,
                                          PrivateDataInsertionMode mode) {
    streams_[pid].mode = mode;
}

PrivateDataInsertionMode PrivateDataManager::getInsertionMode(uint16_t pid) const {
    auto it = streams_.find(pid);
    if (it != streams_.end()) {
        return it->second.mode;
    }
    return PrivateDataInsertionMode::INSERT_WITH_PAYLOAD;
}

bool PrivateDataManager::hasPendingData(uint16_t pid) const {
    auto it = streams_.find(pid);
    return (it != streams_.end() && !it->second.queue.empty());
}

bool PrivateDataManager::getNextData(uint16_t pid, std::vector<uint8_t>& data) {
    auto it = streams_.find(pid);
    if (it == streams_.end() || it->second.queue.empty()) {
        return false;
    }

    const PrivateDataEntry& entry = it->second.queue.front();
    data = entry.data;
    it->second.queue.pop();
    return true;
}

void PrivateDataManager::clearStream(uint16_t pid) {
    auto it = streams_.find(pid);
    if (it != streams_.end()) {
        // Clear the queue
        while (!it->second.queue.empty()) {
            it->second.queue.pop();
        }
    }
}

void PrivateDataManager::clearAll() {
    for (auto& pair : streams_) {
        while (!pair.second.queue.empty()) {
            pair.second.queue.pop();
        }
    }
    streams_.clear();
}

} // namespace mpegts
