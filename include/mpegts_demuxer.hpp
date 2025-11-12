#ifndef MPEGTS_DEMUXER_HPP
#define MPEGTS_DEMUXER_HPP

#include "mpegts_types.hpp"
#include "mpegts_storage.hpp"
#include "mpegts_packet.hpp"
#include "mpegts_psi.hpp"
#include <vector>
#include <memory>
#include <optional>
#include <map>

namespace mpegts {

/**
 * @brief Main MPEG-TS Demuxer class
 *
 * This class implements an adaptive-restorative MPEG-TS demultiplexer
 * with the following features:
 * - Adaptive synchronization in noisy conditions
 * - 3-iteration validation for reliability
 * - Separation of normal and private payload data
 * - Support for multiple programs and streams
 * - Profile-agnostic implementation
 */
class MPEGTSDemuxer {
public:
    MPEGTSDemuxer();
    ~MPEGTSDemuxer();

    // ========================================================================
    // Main API - Data Input
    // ========================================================================

    /**
     * @brief Feed data to demuxer
     * @param data Pointer to raw data
     * @param length Size of data in bytes
     */
    void feedData(const uint8_t* data, size_t length);

    // ========================================================================
    // Program Information
    // ========================================================================

    /**
     * @brief Get all discovered programs/streams
     * @return Vector of program information
     */
    std::vector<ProgramInfo> getPrograms() const;

    /**
     * @brief Get all discovered PIDs
     * @return Set of PIDs
     */
    std::set<uint16_t> getDiscoveredPIDs() const;

    // ========================================================================
    // Iteration Information
    // ========================================================================

    /**
     * @brief Get summary of iterations for a stream
     * @param pid Stream PID
     * @return Vector of iteration information
     */
    std::vector<IterationInfo> getIterationsSummary(uint16_t pid) const;

    // ========================================================================
    // Payload Access
    // ========================================================================

    /**
     * @brief Get payload of specific type
     * @param pid Stream PID
     * @param iter_id Iteration ID
     * @param type Payload type (default: NORMAL)
     * @return Payload buffer
     */
    PayloadBuffer getPayload(uint16_t pid, uint32_t iter_id,
                            PayloadType type = PayloadType::PAYLOAD_NORMAL) const;

    /**
     * @brief Get all payloads for an iteration
     * @param pid Stream PID
     * @param iter_id Iteration ID
     * @return Vector of all payloads
     */
    std::vector<PayloadBuffer> getAllPayloads(uint16_t pid, uint32_t iter_id) const;

    // ========================================================================
    // Data Management
    // ========================================================================

    /**
     * @brief Clear specific iteration
     * @param pid Stream PID
     * @param iter_id Iteration ID
     */
    void clearIteration(uint16_t pid, uint32_t iter_id);

    /**
     * @brief Clear entire stream
     * @param pid Stream PID
     */
    void clearStream(uint16_t pid);

    /**
     * @brief Clear all data
     */
    void clearAll();

    // ========================================================================
    // State Information
    // ========================================================================

    /**
     * @brief Check if demuxer is synchronized
     */
    bool isSynchronized() const { return is_synchronized_; }

    /**
     * @brief Get buffer occupancy
     * @return Number of bytes in buffer
     */
    size_t getBufferOccupancy() const;

    /**
     * @brief Get packet count in buffer
     * @return Number of packets
     */
    size_t getPacketCount() const;

    // ========================================================================
    // Program Table Management
    // ========================================================================

    /**
     * @brief Set program table
     * @param table Program table mapping
     */
    void setProgramsTable(const ProgramTable& table);

    /**
     * @brief Check if program table is set
     */
    bool hasProgramsTable() const { return programs_table_available_; }

private:
    // Internal state
    DemuxerStreamStorage    storage_;
    std::vector<uint8_t>    raw_buffer_;

    bool                    is_synchronized_;
    size_t                  sync_offset_;
    uint8_t                 sync_validation_depth_;

    bool                    programs_table_available_;
    std::set<uint16_t>      known_program_pids_;

    // Current iterations being built per PID
    std::map<uint16_t, uint32_t>        current_iteration_ids_;
    std::map<uint16_t, IterationData>   current_iterations_;
    std::map<uint16_t, uint8_t>         last_cc_;

    // PSI (Program Specific Information) support
    PSIAccumulator                      pat_accumulator_;
    std::map<uint16_t, PSIAccumulator>  pmt_accumulators_;
    std::optional<PAT>                  parsed_pat_;
    std::map<uint16_t, PMT>             parsed_pmts_;  // key: program_number

    // Internal methods
    bool validatePacket(const uint8_t* data);
    bool belongsToSameIteration(const TSPacket& p1, const TSPacket& p2);
    void addPacketToStorage(const TSPacket& packet);
    void finalizeIteration(uint16_t pid);
    void finalizeAllIterations();
    void handleDiscontinuity(uint16_t pid);
    bool tryFindValidIteration();
    void processBuffer();
    void processPSIPacket(const TSPacket& packet);
};

} // namespace mpegts

#endif // MPEGTS_DEMUXER_HPP
