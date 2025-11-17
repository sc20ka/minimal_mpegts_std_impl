#ifndef MPEGTS_PSI_GENERATOR_HPP
#define MPEGTS_PSI_GENERATOR_HPP

#include "mpegts_psi.hpp"
#include "mpegts_muxer_types.hpp"
#include <vector>
#include <map>
#include <cstdint>

namespace mpegts {

/**
 * @brief PSI table generator for MPEG-TS muxer
 *
 * Generates PAT and PMT tables with proper formatting and CRC-32 validation.
 *
 * Usage:
 * @code
 * PSIGenerator generator;
 * generator.setTransportStreamID(1);
 * generator.setProgramNumber(1, 0x1000);  // Program 1 -> PMT PID 0x1000
 *
 * // Generate PAT
 * auto pat_data = generator.generatePAT();
 *
 * // Add streams to PMT
 * generator.addStream(1, 0x100, StreamType::H264_VIDEO);  // Program 1
 * generator.addStream(1, 0x101, StreamType::AAC_AUDIO);
 * generator.setPCRPID(1, 0x100);
 *
 * // Generate PMT
 * auto pmt_data = generator.generatePMT(1);
 * @endcode
 */
class PSIGenerator {
public:
    /**
     * @brief Construct PSI generator
     */
    PSIGenerator();

    /**
     * @brief Destructor
     */
    ~PSIGenerator() = default;

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * @brief Set transport stream ID
     * @param tsid Transport stream ID (default: 1)
     */
    void setTransportStreamID(uint16_t tsid) { transport_stream_id_ = tsid; }

    /**
     * @brief Get transport stream ID
     */
    uint16_t getTransportStreamID() const { return transport_stream_id_; }

    /**
     * @brief Set program number to PMT PID mapping
     * @param program_number Program number
     * @param pmt_pid PMT PID for this program
     */
    void setProgramNumber(uint16_t program_number, uint16_t pmt_pid);

    /**
     * @brief Remove program
     * @param program_number Program number to remove
     */
    void removeProgram(uint16_t program_number);

    /**
     * @brief Set PCR PID for a program
     * @param program_number Program number
     * @param pcr_pid PCR PID
     */
    void setPCRPID(uint16_t program_number, uint16_t pcr_pid);

    /**
     * @brief Add elementary stream to program
     * @param program_number Program number
     * @param elementary_pid Elementary stream PID
     * @param stream_type Stream type
     */
    void addStream(uint16_t program_number, uint16_t elementary_pid,
                  mpegts::StreamType stream_type);

    /**
     * @brief Remove elementary stream
     * @param program_number Program number
     * @param elementary_pid Elementary stream PID
     */
    void removeStream(uint16_t program_number, uint16_t elementary_pid);

    /**
     * @brief Increment version number for PAT
     */
    void incrementPATVersion();

    /**
     * @brief Increment version number for PMT
     * @param program_number Program number
     */
    void incrementPMTVersion(uint16_t program_number);

    /**
     * @brief Reset all version numbers to 0
     */
    void resetVersions();

    // ========================================================================
    // Generation
    // ========================================================================

    /**
     * @brief Generate PAT section data
     *
     * Generates complete PAT section with:
     * - Section header (table_id = 0x00)
     * - Transport stream ID
     * - Program number -> PMT PID mappings
     * - CRC-32
     *
     * @return PAT section data (ready to be inserted into TS packets)
     */
    std::vector<uint8_t> generatePAT();

    /**
     * @brief Generate PMT section data
     *
     * Generates complete PMT section with:
     * - Section header (table_id = 0x02)
     * - Program number
     * - PCR PID
     * - Elementary stream list with types and PIDs
     * - CRC-32
     *
     * @param program_number Program number
     * @return PMT section data (ready to be inserted into TS packets)
     * @throws std::invalid_argument if program not found
     */
    std::vector<uint8_t> generatePMT(uint16_t program_number);

    /**
     * @brief Get PAT version number
     */
    uint8_t getPATVersion() const { return pat_version_; }

    /**
     * @brief Get PMT version number
     * @param program_number Program number
     * @return Version number, or 0 if program not found
     */
    uint8_t getPMTVersion(uint16_t program_number) const;

private:
    // ========================================================================
    // Internal Structures
    // ========================================================================

    struct ProgramInfo {
        uint16_t pmt_pid;                           ///< PMT PID
        uint16_t pcr_pid;                           ///< PCR PID
        std::map<uint16_t, mpegts::StreamType> streams; ///< PID -> StreamType
        uint8_t version;                            ///< PMT version number
    };

    // ========================================================================
    // State
    // ========================================================================

    uint16_t transport_stream_id_;                  ///< Transport stream ID
    std::map<uint16_t, ProgramInfo> programs_;      ///< Program number -> info
    uint8_t pat_version_;                           ///< PAT version number

    // ========================================================================
    // Helper Methods
    // ========================================================================

    /**
     * @brief Build PSI section header
     * @param buffer Output buffer
     * @param table_id Table ID
     * @param table_id_extension TS ID or program number
     * @param version_number Version number
     * @param section_length Section length (after this header)
     */
    void buildSectionHeader(std::vector<uint8_t>& buffer,
                           uint8_t table_id,
                           uint16_t table_id_extension,
                           uint8_t version_number,
                           uint16_t section_length);

    /**
     * @brief Calculate and append CRC-32
     * @param data Section data (header + body)
     */
    void appendCRC32(std::vector<uint8_t>& data);

    /**
     * @brief Write 16-bit value in big-endian
     */
    void write16(std::vector<uint8_t>& buffer, uint16_t value);

    /**
     * @brief Write 32-bit value in big-endian
     */
    void write32(std::vector<uint8_t>& buffer, uint32_t value);
};

} // namespace mpegts

#endif // MPEGTS_PSI_GENERATOR_HPP
