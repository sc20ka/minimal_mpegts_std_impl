#ifndef MPEGTS_PACKET_HPP
#define MPEGTS_PACKET_HPP

#include "mpegts_types.hpp"
#include <cstdint>
#include <vector>

namespace mpegts {

/**
 * @brief MPEG-TS packet header structure
 */
struct TSPacketHeader {
    uint8_t     sync_byte;                  ///< Should be 0x47
    bool        transport_error_indicator;  ///< Error in packet
    bool        payload_unit_start;         ///< Start of PES/PSI
    bool        transport_priority;         ///< Priority flag
    uint16_t    pid;                        ///< Packet ID (13 bits)
    uint8_t     scrambling_control;         ///< Scrambling (2 bits)
    AdaptationFieldControl adaptation_control; ///< Adaptation field control
    uint8_t     continuity_counter;         ///< CC (4 bits)

    TSPacketHeader()
        : sync_byte(0)
        , transport_error_indicator(false)
        , payload_unit_start(false)
        , transport_priority(false)
        , pid(0)
        , scrambling_control(0)
        , adaptation_control(AdaptationFieldControl::RESERVED)
        , continuity_counter(0)
    {}
};

/**
 * @brief MPEG-TS adaptation field structure
 */
struct TSAdaptationField {
    uint8_t     length;                         ///< Adaptation field length
    bool        discontinuity_indicator;        ///< Discontinuity flag
    bool        random_access_indicator;        ///< Random access flag
    bool        es_priority_indicator;          ///< ES priority flag
    bool        pcr_flag;                       ///< PCR present
    bool        opcr_flag;                      ///< OPCR present
    bool        splicing_point_flag;            ///< Splicing point flag
    bool        transport_private_data_flag;    ///< Private data present
    bool        adaptation_extension_flag;      ///< Extension present

    // Optional fields
    uint64_t    pcr_base;                       ///< Program clock reference base
    uint16_t    pcr_extension;                  ///< PCR extension

    // Private data
    uint8_t     private_data_length;            ///< Length of private data
    const uint8_t* private_data;                ///< Pointer to private data

    TSAdaptationField()
        : length(0)
        , discontinuity_indicator(false)
        , random_access_indicator(false)
        , es_priority_indicator(false)
        , pcr_flag(false)
        , opcr_flag(false)
        , splicing_point_flag(false)
        , transport_private_data_flag(false)
        , adaptation_extension_flag(false)
        , pcr_base(0)
        , pcr_extension(0)
        , private_data_length(0)
        , private_data(nullptr)
    {}
};

/**
 * @brief Complete MPEG-TS packet representation
 */
class TSPacket {
public:
    TSPacket();
    ~TSPacket() = default;

    /**
     * @brief Parse packet from raw data
     * @param data Pointer to 188-byte packet
     * @return true if packet is valid
     */
    bool parse(const uint8_t* data);

    /**
     * @brief Validate packet structure
     * @return true if packet is valid
     */
    bool isValid() const;

    /**
     * @brief Get packet header
     */
    const TSPacketHeader& getHeader() const { return header_; }

    /**
     * @brief Get adaptation field (if present)
     */
    const TSAdaptationField* getAdaptationField() const {
        return has_adaptation_ ? &adaptation_field_ : nullptr;
    }

    /**
     * @brief Get payload data (if present)
     */
    const uint8_t* getPayload() const { return payload_data_; }

    /**
     * @brief Get payload size
     */
    size_t getPayloadSize() const { return payload_size_; }

    /**
     * @brief Check if packet has adaptation field
     */
    bool hasAdaptationField() const { return has_adaptation_; }

    /**
     * @brief Check if packet has payload
     */
    bool hasPayload() const { return has_payload_; }

    /**
     * @brief Get private data from adaptation field
     */
    const uint8_t* getPrivateData() const {
        return (has_adaptation_ && adaptation_field_.transport_private_data_flag)
            ? adaptation_field_.private_data
            : nullptr;
    }

    /**
     * @brief Get private data length
     */
    size_t getPrivateDataLength() const {
        return (has_adaptation_ && adaptation_field_.transport_private_data_flag)
            ? adaptation_field_.private_data_length
            : 0;
    }

private:
    TSPacketHeader      header_;
    TSAdaptationField   adaptation_field_;
    const uint8_t*      payload_data_;
    size_t              payload_size_;
    bool                has_adaptation_;
    bool                has_payload_;
    bool                valid_;

    /**
     * @brief Parse packet header
     */
    bool parseHeader(const uint8_t* data);

    /**
     * @brief Parse adaptation field
     */
    bool parseAdaptationField(const uint8_t* data, size_t offset);
};

} // namespace mpegts

#endif // MPEGTS_PACKET_HPP
