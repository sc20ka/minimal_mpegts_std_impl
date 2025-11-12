#ifndef MPEGTS_PSI_HPP
#define MPEGTS_PSI_HPP

#include "mpegts_types.hpp"
#include <vector>
#include <map>
#include <cstdint>

namespace mpegts {

// ============================================================================
// PSI Table Constants
// ============================================================================

constexpr uint8_t TABLE_ID_PAT = 0x00;  // Program Association Table
constexpr uint8_t TABLE_ID_CAT = 0x01;  // Conditional Access Table
constexpr uint8_t TABLE_ID_PMT = 0x02;  // Program Map Table
constexpr uint8_t TABLE_ID_NIT = 0x40;  // Network Information Table

// ============================================================================
// PSI Section Header
// ============================================================================

/**
 * @brief Generic PSI section header
 */
struct PSISectionHeader {
    uint8_t     table_id;                   ///< Table identifier
    bool        section_syntax_indicator;   ///< Section syntax indicator
    uint16_t    section_length;             ///< Section length
    uint16_t    table_id_extension;         ///< Transport stream ID or program number
    uint8_t     version_number;             ///< Version number
    bool        current_next_indicator;     ///< Current/next indicator
    uint8_t     section_number;             ///< Section number
    uint8_t     last_section_number;        ///< Last section number

    PSISectionHeader()
        : table_id(0)
        , section_syntax_indicator(false)
        , section_length(0)
        , table_id_extension(0)
        , version_number(0)
        , current_next_indicator(false)
        , section_number(0)
        , last_section_number(0)
    {}
};

// ============================================================================
// PAT (Program Association Table)
// ============================================================================

/**
 * @brief PAT entry - program number to PMT PID mapping
 */
struct PATEntry {
    uint16_t program_number;    ///< Program number (0 = NIT)
    uint16_t pid;               ///< PMT PID or NIT PID

    PATEntry() : program_number(0), pid(0) {}
    PATEntry(uint16_t prog, uint16_t p) : program_number(prog), pid(p) {}
};

/**
 * @brief Complete PAT structure
 */
struct PAT {
    PSISectionHeader    header;
    uint16_t            transport_stream_id;    ///< Transport stream ID
    std::vector<PATEntry> programs;             ///< Program list
    uint32_t            crc32;                  ///< CRC-32

    PAT() : transport_stream_id(0), crc32(0) {}

    /**
     * @brief Get PMT PID for program number
     * @return PMT PID, or 0 if not found
     */
    uint16_t getPMTPID(uint16_t program_number) const;

    /**
     * @brief Get all program numbers
     */
    std::vector<uint16_t> getProgramNumbers() const;
};

// ============================================================================
// PMT (Program Map Table)
// ============================================================================

/**
 * @brief Stream types
 */
enum class StreamType : uint8_t {
    RESERVED                = 0x00,
    MPEG1_VIDEO            = 0x01,
    MPEG2_VIDEO            = 0x02,
    MPEG1_AUDIO            = 0x03,
    MPEG2_AUDIO            = 0x04,
    PRIVATE_SECTIONS       = 0x05,
    PRIVATE_DATA           = 0x06,
    MHEG                   = 0x07,
    DSM_CC                 = 0x08,
    H222_1                 = 0x09,
    MPEG2_MULTIPROTO       = 0x0A,
    MPEG2_DSM_CC_U_N       = 0x0B,
    MPEG2_DSM_CC_STREAM    = 0x0C,
    MPEG2_DSM_CC_SECTIONS  = 0x0D,
    MPEG2_AUX              = 0x0E,
    AAC_AUDIO              = 0x0F,
    MPEG4_VISUAL           = 0x10,
    MPEG4_AUDIO_LATM       = 0x11,
    MPEG4_FLEXMUX_PES      = 0x12,
    MPEG4_FLEXMUX_SECTIONS = 0x13,
    SYNC_DOWNLOAD          = 0x14,
    METADATA_PES           = 0x15,
    METADATA_SECTIONS      = 0x16,
    METADATA_DATA_CAROUSEL = 0x17,
    METADATA_OBJECT_CAROUSEL = 0x18,
    METADATA_SYNC_DOWNLOAD = 0x19,
    MPEG2_IPMP             = 0x1A,
    H264_VIDEO             = 0x1B,  // AVC
    MPEG4_AUDIO_RAW        = 0x1C,
    MPEG4_TEXT             = 0x1D,
    AUX_VIDEO              = 0x1E,
    H264_SVC_VIDEO         = 0x1F,
    H264_MVC_VIDEO         = 0x20,
    JPEG2000_VIDEO         = 0x21,
    MPEG2_3D_VIDEO         = 0x22,
    H265_VIDEO             = 0x24,  // HEVC
    // ... more types can be added
};

/**
 * @brief Get string name for stream type
 */
const char* getStreamTypeName(StreamType type);

/**
 * @brief PMT elementary stream info
 */
struct PMTStreamInfo {
    StreamType  stream_type;        ///< Stream type
    uint16_t    elementary_pid;     ///< Elementary stream PID
    uint16_t    es_info_length;     ///< ES info length
    std::vector<uint8_t> descriptors; ///< ES descriptors

    PMTStreamInfo()
        : stream_type(StreamType::RESERVED)
        , elementary_pid(0)
        , es_info_length(0)
    {}
};

/**
 * @brief Complete PMT structure
 */
struct PMT {
    PSISectionHeader    header;
    uint16_t            program_number;         ///< Program number
    uint16_t            pcr_pid;                ///< PCR PID
    uint16_t            program_info_length;    ///< Program info length
    std::vector<uint8_t> program_descriptors;   ///< Program descriptors
    std::vector<PMTStreamInfo> streams;         ///< Elementary streams
    uint32_t            crc32;                  ///< CRC-32

    PMT() : program_number(0), pcr_pid(0), program_info_length(0), crc32(0) {}

    /**
     * @brief Get elementary PIDs for a specific stream type
     */
    std::vector<uint16_t> getPIDsByType(StreamType type) const;

    /**
     * @brief Get all elementary PIDs
     */
    std::vector<uint16_t> getAllPIDs() const;

    /**
     * @brief Get stream info by PID
     */
    const PMTStreamInfo* getStreamInfo(uint16_t pid) const;
};

// ============================================================================
// PSI Parser
// ============================================================================

/**
 * @brief PSI table parser
 */
class PSIParser {
public:
    PSIParser() = default;
    ~PSIParser() = default;

    /**
     * @brief Parse PSI section header
     * @param data Pointer to section data
     * @param length Data length
     * @param header Output header
     * @return Number of bytes consumed, or 0 on error
     */
    static size_t parseSectionHeader(const uint8_t* data, size_t length,
                                     PSISectionHeader& header);

    /**
     * @brief Parse PAT from payload data
     * @param data Pointer to PAT payload
     * @param length Payload length
     * @param pat Output PAT structure
     * @return true if parsing succeeded
     */
    static bool parsePAT(const uint8_t* data, size_t length, PAT& pat);

    /**
     * @brief Parse PMT from payload data
     * @param data Pointer to PMT payload
     * @param length Payload length
     * @param pmt Output PMT structure
     * @return true if parsing succeeded
     */
    static bool parsePMT(const uint8_t* data, size_t length, PMT& pmt);

    /**
     * @brief Verify CRC-32 for PSI section
     * @param data Section data
     * @param length Section length (including CRC)
     * @return true if CRC is valid
     */
    static bool verifyCRC32(const uint8_t* data, size_t length);

    /**
     * @brief Calculate CRC-32
     */
    static uint32_t calculateCRC32(const uint8_t* data, size_t length);

private:
    static const uint32_t crc32_table_[256];
    static void initCRC32Table();
};

// ============================================================================
// PSI Accumulator - для сборки секций из пакетов
// ============================================================================

/**
 * @brief Accumulates PSI sections from multiple TS packets
 */
class PSIAccumulator {
public:
    PSIAccumulator();
    ~PSIAccumulator() = default;

    /**
     * @brief Add packet payload to accumulator
     * @param data Payload data
     * @param length Payload length
     * @param payload_unit_start PUSI flag
     * @return true if complete section is ready
     */
    bool addData(const uint8_t* data, size_t length, bool payload_unit_start);

    /**
     * @brief Get complete section
     * @param section Output buffer for section
     * @return Section length, or 0 if not complete
     */
    size_t getSection(std::vector<uint8_t>& section);

    /**
     * @brief Reset accumulator
     */
    void reset();

    /**
     * @brief Check if section is complete
     */
    bool isComplete() const { return complete_; }

private:
    std::vector<uint8_t> buffer_;
    size_t expected_length_;
    bool complete_;
    bool synced_;
};

} // namespace mpegts

#endif // MPEGTS_PSI_HPP
