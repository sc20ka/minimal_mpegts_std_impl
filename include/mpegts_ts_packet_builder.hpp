#ifndef MPEGTS_TS_PACKET_BUILDER_HPP
#define MPEGTS_TS_PACKET_BUILDER_HPP

#include "mpegts_types.hpp"
#include "mpegts_packet.hpp"
#include <cstdint>
#include <vector>

namespace mpegts {

/**
 * @brief Options for building TS packets with adaptation field
 */
struct BuildOptions {
    bool pusi = false;                          ///< Payload unit start indicator
    bool has_pcr = false;                       ///< Include PCR in adaptation field
    uint64_t pcr_value = 0;                     ///< PCR value (42-bit: 33-bit base + 9-bit ext)
    bool random_access = false;                 ///< Random access indicator
    bool discontinuity = false;                 ///< Discontinuity indicator
    bool has_private_data = false;              ///< Include private data
    const uint8_t* private_data = nullptr;      ///< Private data bytes
    uint8_t private_data_length = 0;            ///< Private data length (max 182)
};

/**
 * @brief Builder for creating MPEG-TS packets
 *
 * TSPacketBuilder constructs valid 188-byte MPEG-TS packets from payload data.
 * It manages:
 * - Continuity counter (automatically increments)
 * - Adaptation field insertion (PCR, private data, stuffing)
 * - Packet header construction
 * - NULL packet generation
 *
 * Usage:
 * @code
 * TSPacketBuilder builder(0x100);  // PID = 0x100
 *
 * BuildOptions opts;
 * opts.pusi = true;
 * opts.has_pcr = true;
 * opts.pcr_value = calculate_pcr();
 *
 * std::vector<std::vector<uint8_t>> packets = builder.build(data, size, opts);
 * @endcode
 */
class TSPacketBuilder {
public:
    /**
     * @brief Construct builder for specific PID
     * @param pid Packet ID (13-bit)
     */
    explicit TSPacketBuilder(uint16_t pid);

    /**
     * @brief Build TS packets from payload data
     *
     * Splits payload into multiple 188-byte packets if necessary.
     * PUSI flag is only set on the first packet.
     * Continuity counter increments for each packet with payload.
     *
     * @param payload Payload data
     * @param length Payload length
     * @param options Build options (PCR, private data, flags)
     * @return Vector of 188-byte packets
     */
    std::vector<std::vector<uint8_t>> build(
        const uint8_t* payload, size_t length,
        const BuildOptions& options = BuildOptions()
    );

    /**
     * @brief Create a single NULL packet (PID 0x1FFF)
     *
     * Used for bitrate padding in CBR mode.
     *
     * @return 188-byte NULL packet
     */
    static std::vector<uint8_t> createNullPacket();

    /**
     * @brief Reset continuity counter to 0
     */
    void resetContinuityCounter();

    /**
     * @brief Get current continuity counter value
     */
    uint8_t getContinuityCounter() const { return continuity_counter_; }

    /**
     * @brief Set continuity counter (for synchronization)
     */
    void setContinuityCounter(uint8_t cc) { continuity_counter_ = cc & 0x0F; }

    /**
     * @brief Get PID
     */
    uint16_t getPID() const { return pid_; }

private:
    uint16_t pid_;                              ///< Packet ID
    uint8_t continuity_counter_;                ///< Continuity counter (4-bit, 0-15)

    /**
     * @brief Build packet header (4 bytes)
     * @param buffer Output buffer (must be at least 4 bytes)
     * @param pusi Payload unit start indicator
     * @param adaptation_control Adaptation field control
     */
    void buildHeader(uint8_t* buffer, bool pusi,
                    AdaptationFieldControl adaptation_control);

    /**
     * @brief Build adaptation field
     * @param buffer Output buffer
     * @param options Build options
     * @param stuffing_needed Bytes of stuffing needed
     * @return Adaptation field length (including length byte)
     */
    size_t buildAdaptationField(uint8_t* buffer,
                               const BuildOptions& options,
                               size_t stuffing_needed);

    /**
     * @brief Calculate adaptation field size needed
     * @param options Build options
     * @return Size in bytes
     */
    size_t calculateAdaptationFieldSize(const BuildOptions& options) const;

    /**
     * @brief Encode PCR value into buffer (6 bytes)
     * @param buffer Output buffer
     * @param pcr_value PCR value (42-bit)
     */
    void encodePCR(uint8_t* buffer, uint64_t pcr_value);

    /**
     * @brief Increment continuity counter
     */
    void incrementCC();
};

} // namespace mpegts

#endif // MPEGTS_TS_PACKET_BUILDER_HPP
