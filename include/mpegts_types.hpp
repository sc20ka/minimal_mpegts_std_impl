#ifndef MPEGTS_TYPES_HPP
#define MPEGTS_TYPES_HPP

#include <cstdint>
#include <vector>
#include <map>
#include <set>

namespace mpegts {

// ============================================================================
// Constants
// ============================================================================

constexpr size_t MPEGTS_PACKET_SIZE = 188;      // Standard MPEG-TS packet size
constexpr uint8_t MPEGTS_SYNC_BYTE = 0x47;      // Sync byte
constexpr size_t MAX_BUFFER_PACKETS = 100;      // Maximum packets in buffer
constexpr size_t MAX_BUFFER_SIZE = MPEGTS_PACKET_SIZE * MAX_BUFFER_PACKETS;

// System PIDs
constexpr uint16_t PID_PAT = 0x0000;
constexpr uint16_t PID_CAT = 0x0001;
constexpr uint16_t PID_TSDT = 0x0002;
constexpr uint16_t PID_NULL = 0x1FFF;

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief Type of payload data
 */
enum class PayloadType : uint8_t {
    PAYLOAD_NORMAL  = 0,    ///< Main stream data
    PAYLOAD_PRIVATE = 1     ///< Private data from adaptation field
};

/**
 * @brief Adaptation field control values
 */
enum class AdaptationFieldControl : uint8_t {
    RESERVED            = 0x00,     ///< Reserved (invalid)
    PAYLOAD_ONLY        = 0x01,     ///< Payload only, no adaptation field
    ADAPTATION_ONLY     = 0x02,     ///< Adaptation field only, no payload
    ADAPTATION_PAYLOAD  = 0x03      ///< Both adaptation field and payload
};

// ============================================================================
// Structures
// ============================================================================

/**
 * @brief A segment of payload data
 */
struct PayloadSegment {
    PayloadType     type;               ///< Type of payload
    const uint8_t*  data;               ///< Pointer to data
    size_t          length;             ///< Size in bytes
    size_t          offset_in_stream;   ///< Position in the overall stream

    PayloadSegment()
        : type(PayloadType::PAYLOAD_NORMAL)
        , data(nullptr)
        , length(0)
        , offset_in_stream(0)
    {}
};

/**
 * @brief Data for one iteration (group of related packets)
 */
struct IterationData {
    std::vector<PayloadSegment> payloads;           ///< Payload segments

    // Flags
    bool    discontinuity_detected;                 ///< CC discontinuity detected?
    bool    payload_unit_start_seen;                ///< PES frame start seen?
    bool    is_complete;                            ///< Frame complete?

    // Metadata
    uint8_t first_cc;                               ///< First continuity counter
    uint8_t last_cc;                                ///< Last continuity counter
    size_t  packet_count;                           ///< Number of packets
    size_t  buffer_position;                        ///< Position in buffer

    IterationData()
        : discontinuity_detected(false)
        , payload_unit_start_seen(false)
        , is_complete(false)
        , first_cc(0)
        , last_cc(0)
        , packet_count(0)
        , buffer_position(0)
    {}
};

/**
 * @brief Payload buffer returned by API
 */
struct PayloadBuffer {
    const uint8_t*  data;       ///< Pointer to data
    size_t          length;     ///< Size in bytes
    PayloadType     type;       ///< Type of payload

    PayloadBuffer()
        : data(nullptr)
        , length(0)
        , type(PayloadType::PAYLOAD_NORMAL)
    {}
};

/**
 * @brief Information about a single iteration
 */
struct IterationInfo {
    uint32_t    iteration_id;           ///< Unique iteration ID
    size_t      payload_normal_size;    ///< Size of normal payload
    size_t      payload_private_size;   ///< Size of private payload
    bool        has_discontinuity;      ///< Discontinuity flag
    uint8_t     cc_start;               ///< Starting CC
    uint8_t     cc_end;                 ///< Ending CC
    size_t      packet_count;           ///< Number of packets

    IterationInfo()
        : iteration_id(0)
        , payload_normal_size(0)
        , payload_private_size(0)
        , has_discontinuity(false)
        , cc_start(0)
        , cc_end(0)
        , packet_count(0)
    {}
};

/**
 * @brief Information about a program/stream
 */
struct ProgramInfo {
    uint16_t                program_number;     ///< Program number
    std::vector<uint16_t>   stream_pids;        ///< PIDs in this program
    size_t                  total_payload_size; ///< Total payload size
    size_t                  iteration_count;    ///< Number of iterations
    bool                    has_discontinuity;  ///< Any discontinuities?

    ProgramInfo()
        : program_number(0)
        , total_payload_size(0)
        , iteration_count(0)
        , has_discontinuity(false)
    {}
};

/**
 * @brief Program table mapping
 */
struct ProgramTable {
    // program_id → [PIDs in program]
    std::map<uint16_t, std::vector<uint16_t>> programs;
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Check if PID is a system PID
 */
inline bool isSystemPID(uint16_t pid) {
    return pid == PID_PAT || pid == PID_CAT || pid == PID_TSDT || pid == PID_NULL;
}

/**
 * @brief Check if PID is a program stream
 */
inline bool isProgramStream(uint16_t pid) {
    return !isSystemPID(pid);
}

} // namespace mpegts

#endif // MPEGTS_TYPES_HPP
