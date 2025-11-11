#ifndef MPEGTS_STORAGE_HPP
#define MPEGTS_STORAGE_HPP

#include "mpegts_types.hpp"
#include <map>
#include <vector>
#include <memory>

namespace mpegts {

/**
 * @brief Container for iterations of a single stream (PID)
 */
class StreamIterations {
public:
    StreamIterations(uint16_t pid);
    ~StreamIterations() = default;

    /**
     * @brief Get stream PID
     */
    uint16_t getPID() const { return pid_; }

    /**
     * @brief Add new iteration
     */
    void addIteration(uint32_t iter_id, const IterationData& data);

    /**
     * @brief Get iteration by ID
     */
    const IterationData* getIteration(uint32_t iter_id) const;

    /**
     * @brief Get all iterations
     */
    const std::vector<std::pair<uint32_t, IterationData>>& getIterations() const {
        return iterations_;
    }

    /**
     * @brief Remove iteration by ID
     */
    void removeIteration(uint32_t iter_id);

    /**
     * @brief Clear all iterations
     */
    void clear();

    /**
     * @brief Get iteration count
     */
    size_t getIterationCount() const { return iterations_.size(); }

    /**
     * @brief Check if stream has discontinuities
     */
    bool hasDiscontinuity() const;

private:
    uint16_t pid_;
    std::vector<std::pair<uint32_t, IterationData>> iterations_;
    std::set<uint8_t> observed_cc_values_;
};

/**
 * @brief Main storage for all demuxed streams
 */
class DemuxerStreamStorage {
public:
    DemuxerStreamStorage();
    ~DemuxerStreamStorage() = default;

    /**
     * @brief Get or create stream for PID
     */
    StreamIterations& getOrCreateStream(uint16_t pid);

    /**
     * @brief Get existing stream (const)
     */
    const StreamIterations* getStream(uint16_t pid) const;

    /**
     * @brief Get all streams
     */
    const std::map<uint16_t, StreamIterations>& getAllStreams() const {
        return streams_;
    }

    /**
     * @brief Generate unique iteration ID
     */
    uint32_t generateIterationID();

    /**
     * @brief Clear specific stream
     */
    void clearStream(uint16_t pid);

    /**
     * @brief Clear all streams
     */
    void clear();

    /**
     * @brief Get discovered PIDs
     */
    std::set<uint16_t> getDiscoveredPIDs() const;

    /**
     * @brief Check if stream exists
     */
    bool hasStream(uint16_t pid) const;

private:
    std::map<uint16_t, StreamIterations> streams_;
    uint32_t next_iteration_id_;
};

} // namespace mpegts

#endif // MPEGTS_STORAGE_HPP
