#ifndef MPEGTS_PES_HPP
#define MPEGTS_PES_HPP

#include <cstdint>
#include <optional>
#include <vector>
#include <map>

namespace mpegts {

// ============================================================================
// PES (Packetized Elementary Stream) Constants
// ============================================================================

constexpr uint32_t PES_START_CODE = 0x000001;  // PES packet start code prefix

// Stream IDs
constexpr uint8_t STREAM_ID_PROGRAM_STREAM_MAP     = 0xBC;
constexpr uint8_t STREAM_ID_PRIVATE_STREAM_1       = 0xBD;
constexpr uint8_t STREAM_ID_PADDING_STREAM         = 0xBE;
constexpr uint8_t STREAM_ID_PRIVATE_STREAM_2       = 0xBF;
constexpr uint8_t STREAM_ID_AUDIO_STREAM_MIN       = 0xC0;
constexpr uint8_t STREAM_ID_AUDIO_STREAM_MAX       = 0xDF;
constexpr uint8_t STREAM_ID_VIDEO_STREAM_MIN       = 0xE0;
constexpr uint8_t STREAM_ID_VIDEO_STREAM_MAX       = 0xEF;
constexpr uint8_t STREAM_ID_ECM_STREAM             = 0xF0;
constexpr uint8_t STREAM_ID_EMM_STREAM             = 0xF1;
constexpr uint8_t STREAM_ID_DSMCC_STREAM           = 0xF2;
constexpr uint8_t STREAM_ID_13522_STREAM           = 0xF3;
constexpr uint8_t STREAM_ID_H222_A_STREAM          = 0xF4;
constexpr uint8_t STREAM_ID_H222_B_STREAM          = 0xF5;
constexpr uint8_t STREAM_ID_H222_C_STREAM          = 0xF6;
constexpr uint8_t STREAM_ID_H222_D_STREAM          = 0xF7;
constexpr uint8_t STREAM_ID_H222_E_STREAM          = 0xF8;
constexpr uint8_t STREAM_ID_ANCILLARY_STREAM       = 0xF9;
constexpr uint8_t STREAM_ID_PROGRAM_STREAM_DIRECTORY = 0xFF;

// ============================================================================
// PTS/DTS (Presentation/Decoding Time Stamp)
// ============================================================================

/**
 * @brief PTS/DTS timestamp (33 bits, 90 kHz clock)
 */
struct Timestamp {
    uint64_t value;  ///< 33-bit timestamp value

    Timestamp() : value(0) {}
    explicit Timestamp(uint64_t v) : value(v) {}

    /**
     * @brief Get timestamp in seconds
     */
    double getSeconds() const {
        return value / 90000.0;
    }

    /**
     * @brief Get timestamp in milliseconds
     */
    double getMilliseconds() const {
        return value / 90.0;
    }

    /**
     * @brief Check if timestamp is valid (within 33-bit range)
     */
    bool isValid() const {
        return value < (1ULL << 33);
    }
};

/**
 * @brief Calculate difference between two timestamps (handles wraparound)
 */
int64_t timestampDifference(const Timestamp& ts1, const Timestamp& ts2);

/**
 * @brief Calculate difference in milliseconds
 */
double timestampDifferenceMs(const Timestamp& ts1, const Timestamp& ts2);

// ============================================================================
// PES Header
// ============================================================================

/**
 * @brief PES packet header structure
 */
struct PESHeader {
    // Fixed fields
    uint32_t start_code;        ///< PES start code (0x000001)
    uint8_t  stream_id;          ///< Stream ID
    uint16_t packet_length;      ///< PES packet length (0 = unbounded)

    // Optional fields (present if stream_id in valid range)
    bool     has_optional_fields;

    // PES header flags
    uint8_t  scrambling_control;      ///< Scrambling control (2 bits)
    bool     priority;                 ///< Priority flag
    bool     data_alignment_indicator; ///< Data alignment
    bool     copyright;                ///< Copyright flag
    bool     original_or_copy;         ///< Original/copy flag

    // PTS/DTS flags
    uint8_t  pts_dts_flags;            ///< PTS/DTS flags (2 bits)
    bool     has_pts;                  ///< PTS present
    bool     has_dts;                  ///< DTS present

    // Other flags
    bool     escr_flag;                ///< ESCR present
    bool     es_rate_flag;             ///< ES rate present
    bool     dsm_trick_mode_flag;      ///< DSM trick mode present
    bool     additional_copy_info_flag; ///< Additional copy info present
    bool     crc_flag;                 ///< CRC present
    bool     extension_flag;           ///< Extension present

    uint8_t  header_data_length;       ///< PES header data length

    // Optional timestamps
    std::optional<Timestamp> pts;      ///< Presentation timestamp
    std::optional<Timestamp> dts;      ///< Decoding timestamp

    PESHeader()
        : start_code(0)
        , stream_id(0)
        , packet_length(0)
        , has_optional_fields(false)
        , scrambling_control(0)
        , priority(false)
        , data_alignment_indicator(false)
        , copyright(false)
        , original_or_copy(false)
        , pts_dts_flags(0)
        , has_pts(false)
        , has_dts(false)
        , escr_flag(false)
        , es_rate_flag(false)
        , dsm_trick_mode_flag(false)
        , additional_copy_info_flag(false)
        , crc_flag(false)
        , extension_flag(false)
        , header_data_length(0)
    {}

    /**
     * @brief Check if this is a video stream
     */
    bool isVideoStream() const {
        return stream_id >= STREAM_ID_VIDEO_STREAM_MIN &&
               stream_id <= STREAM_ID_VIDEO_STREAM_MAX;
    }

    /**
     * @brief Check if this is an audio stream
     */
    bool isAudioStream() const {
        return stream_id >= STREAM_ID_AUDIO_STREAM_MIN &&
               stream_id <= STREAM_ID_AUDIO_STREAM_MAX;
    }

    /**
     * @brief Get total header size in bytes
     */
    size_t getHeaderSize() const {
        if (!has_optional_fields) {
            return 6;  // Basic header only
        }
        return 9 + header_data_length;  // Full header
    }
};

// ============================================================================
// PES Packet
// ============================================================================

/**
 * @brief Complete PES packet
 */
struct PESPacket {
    PESHeader header;                  ///< PES header
    std::vector<uint8_t> payload;      ///< PES payload data
    bool complete;                      ///< Is packet complete?

    PESPacket() : complete(false) {}

    /**
     * @brief Get payload size
     */
    size_t getPayloadSize() const {
        return payload.size();
    }

    /**
     * @brief Get payload data pointer
     */
    const uint8_t* getPayloadData() const {
        if (payload.empty()) {
            return nullptr;
        }
        return payload.data();
    }

    /**
     * @brief Clear packet data
     */
    void clear() {
        header = PESHeader();
        payload.clear();
        complete = false;
    }
};

// ============================================================================
// PES Parser
// ============================================================================

/**
 * @brief PES packet parser
 */
class PESParser {
public:
    /**
     * @brief Parse PES header from data
     * @param data Pointer to PES packet data
     * @param length Length of data
     * @param header Output header structure
     * @return true if parsing successful, false otherwise
     */
    static bool parseHeader(const uint8_t* data, size_t length, PESHeader& header);

    /**
     * @brief Parse complete PES packet from data
     * @param data Pointer to PES packet data
     * @param length Length of data
     * @param packet Output PES packet structure
     * @return true if parsing successful, false otherwise
     */
    static bool parsePacket(const uint8_t* data, size_t length, PESPacket& packet);

    /**
     * @brief Verify PES start code
     * @param data Pointer to start of PES packet
     * @return true if valid start code (0x000001) present
     */
    static bool verifyStartCode(const uint8_t* data);

private:
    /**
     * @brief Extract PTS/DTS from header data
     */
    static std::optional<Timestamp> extractTimestamp(const uint8_t* data);
};

// ============================================================================
// PES Accumulator
// ============================================================================

/**
 * @brief Accumulates PES packet data from multiple TS packets
 */
class PESAccumulator {
public:
    PESAccumulator();
    ~PESAccumulator() = default;

    /**
     * @brief Add TS packet payload data
     * @param data Payload data
     * @param length Data length
     * @param payload_unit_start True if this is start of new PES packet
     * @return true if PES packet is complete
     */
    bool addData(const uint8_t* data, size_t length, bool payload_unit_start);

    /**
     * @brief Get completed PES packet
     * @param packet Output packet
     * @return true if packet available
     */
    bool getPacket(PESPacket& packet);

    /**
     * @brief Reset accumulator
     */
    void reset();

    /**
     * @brief Check if packet is complete
     */
    bool isComplete() const { return complete_; }

private:
    std::vector<uint8_t> buffer_;
    size_t expected_length_;
    bool synced_;
    bool complete_;

    bool parseAndCheckComplete();
};

// ============================================================================
// PES Manager
// ============================================================================

/**
 * @brief Manages PES accumulators for multiple PIDs
 */
class PESManager {
public:
    PESManager() = default;
    ~PESManager() = default;

    /**
     * @brief Get or create accumulator for PID
     */
    PESAccumulator& getAccumulator(uint16_t pid);

    /**
     * @brief Check if PID has accumulator
     */
    bool hasAccumulator(uint16_t pid) const;

    /**
     * @brief Remove accumulator for PID
     */
    void removeAccumulator(uint16_t pid);

    /**
     * @brief Clear all accumulators
     */
    void clear();

    /**
     * @brief Get all PIDs with accumulators
     */
    std::vector<uint16_t> getPIDs() const;

private:
    std::map<uint16_t, PESAccumulator> accumulators_;
};

} // namespace mpegts

#endif // MPEGTS_PES_HPP
