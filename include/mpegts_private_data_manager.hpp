#ifndef MPEGTS_PRIVATE_DATA_MANAGER_HPP
#define MPEGTS_PRIVATE_DATA_MANAGER_HPP

#include "mpegts_muxer_types.hpp"
#include <cstdint>
#include <vector>
#include <queue>
#include <map>

namespace mpegts {

// ============================================================================
// Private Data Manager
// ============================================================================

/**
 * @brief Manages private data insertion into transport stream
 *
 * Handles queuing and insertion of private data according to various
 * insertion modes (with PCR, standalone, with payload).
 *
 * This is a skeleton implementation for Week 4.
 * Full functionality will be added in later weeks.
 */
class PrivateDataManager {
public:
    /**
     * @brief Construct manager
     */
    PrivateDataManager();

    /**
     * @brief Destructor
     */
    ~PrivateDataManager();

    /**
     * @brief Add private data for a stream
     * @param pid Stream PID
     * @param data Data bytes
     * @param length Data length (max MAX_PRIVATE_DATA_SIZE)
     * @return true if added successfully
     */
    bool addPrivateData(uint16_t pid, const uint8_t* data, size_t length);

    /**
     * @brief Add private data with PTS synchronization
     * @param pid Stream PID
     * @param data Data bytes
     * @param length Data length
     * @param pts PTS for synchronization
     * @return true if added successfully
     */
    bool addPrivateDataWithPTS(uint16_t pid,
                               const uint8_t* data, size_t length,
                               uint64_t pts);

    /**
     * @brief Set insertion mode for stream
     * @param pid Stream PID
     * @param mode Insertion mode
     */
    void setInsertionMode(uint16_t pid, PrivateDataInsertionMode mode);

    /**
     * @brief Get insertion mode for stream
     * @param pid Stream PID
     * @return Insertion mode
     */
    PrivateDataInsertionMode getInsertionMode(uint16_t pid) const;

    /**
     * @brief Check if stream has pending private data
     * @param pid Stream PID
     * @return true if data is pending
     */
    bool hasPendingData(uint16_t pid) const;

    /**
     * @brief Get next private data chunk for stream
     * @param pid Stream PID
     * @param[out] data Output buffer
     * @param[out] length Data length
     * @return true if data retrieved
     */
    bool getNextData(uint16_t pid, std::vector<uint8_t>& data);

    /**
     * @brief Clear all private data for stream
     * @param pid Stream PID
     */
    void clearStream(uint16_t pid);

    /**
     * @brief Clear all private data
     */
    void clearAll();

private:
    /**
     * @brief Private data entry
     */
    struct PrivateDataEntry {
        std::vector<uint8_t> data;  ///< Data bytes
        uint64_t pts;               ///< Associated PTS (or NO_DTS if none)
        bool has_pts;               ///< Whether PTS is valid

        PrivateDataEntry()
            : pts(NO_DTS), has_pts(false) {}

        PrivateDataEntry(const uint8_t* d, size_t len, uint64_t p, bool hp)
            : data(d, d + len), pts(p), has_pts(hp) {}
    };

    /**
     * @brief Per-stream private data context
     */
    struct StreamPrivateData {
        PrivateDataInsertionMode mode;
        std::queue<PrivateDataEntry> queue;

        StreamPrivateData()
            : mode(PrivateDataInsertionMode::INSERT_WITH_PAYLOAD) {}
    };

    std::map<uint16_t, StreamPrivateData> streams_;
};

} // namespace mpegts

#endif // MPEGTS_PRIVATE_DATA_MANAGER_HPP
