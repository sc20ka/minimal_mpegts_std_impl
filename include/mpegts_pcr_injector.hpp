#ifndef MPEGTS_PCR_INJECTOR_HPP
#define MPEGTS_PCR_INJECTOR_HPP

#include "mpegts_pcr.hpp"
#include "mpegts_types.hpp"
#include <cstdint>
#include <chrono>
#include <optional>

namespace mpegts {

// ============================================================================
// PCR Injector
// ============================================================================

/**
 * @brief Manages PCR (Program Clock Reference) injection into transport stream
 *
 * PCRInjector calculates and manages PCR values for clock synchronization.
 * PCR consists of:
 * - PCR_base: 33 bits at 90 kHz
 * - PCR_extension: 9 bits (27 MHz remainder, 0-299)
 * - Full PCR = base * 300 + extension (in 27 MHz ticks)
 *
 * Features:
 * - PCR value calculation from timestamps
 * - Configurable injection interval (default 40ms)
 * - Timing accuracy tracking
 * - Discontinuity handling
 */
class PCRInjector {
public:
    /**
     * @brief Construct PCR injector
     * @param interval_ms PCR injection interval in milliseconds (default: 40ms)
     */
    explicit PCRInjector(uint32_t interval_ms = 40);

    /**
     * @brief Destructor
     */
    ~PCRInjector();

    /**
     * @brief Set PCR injection interval
     * @param interval_ms Interval in milliseconds (10-100ms recommended)
     */
    void setInterval(uint32_t interval_ms);

    /**
     * @brief Get current injection interval
     * @return Interval in milliseconds
     */
    uint32_t getInterval() const { return interval_ms_; }

    /**
     * @brief Check if PCR should be injected at current timestamp
     * @param current_pcr Current PCR value (90kHz base)
     * @return true if PCR should be injected
     */
    bool shouldInjectPCR(uint64_t current_pcr);

    /**
     * @brief Record PCR injection at given value
     * @param pcr_value PCR value that was injected (90kHz base)
     *
     * Should be called after PCR has been successfully injected into stream
     * to update internal state and injection count.
     */
    void recordInjection(uint64_t pcr_value);

    /**
     * @brief Calculate PCR from PTS/DTS timestamp
     * @param pts_dts PTS or DTS value (90kHz)
     * @return PCR structure with base and extension
     */
    static PCR calculatePCR(uint64_t pts_dts);

    /**
     * @brief Calculate PCR from 27MHz timestamp
     * @param timestamp_27mhz Timestamp in 27MHz ticks
     * @return PCR structure
     */
    static PCR calculatePCRFrom27MHz(uint64_t timestamp_27mhz);

    /**
     * @brief Encode PCR into 6-byte format for adaptation field
     * @param pcr PCR value
     * @return 6 bytes containing encoded PCR
     */
    static std::vector<uint8_t> encodePCR(const PCR& pcr);

    /**
     * @brief Get last injected PCR value
     * @return Last PCR, or nullopt if none injected yet
     */
    std::optional<PCR> getLastPCR() const { return last_pcr_; }

    /**
     * @brief Reset PCR state (for discontinuities)
     */
    void reset();

    /**
     * @brief Get PCR injection count
     * @return Number of PCRs injected
     */
    size_t getInjectionCount() const { return injection_count_; }

    /**
     * @brief Calculate PCR difference in milliseconds
     * @param pcr1 First PCR
     * @param pcr2 Second PCR
     * @return Difference in milliseconds
     */
    static double calculatePCRDifferenceMs(const PCR& pcr1, const PCR& pcr2);

    /**
     * @brief Validate PCR value
     * @param pcr PCR to validate
     * @return true if valid
     */
    static bool isValidPCR(const PCR& pcr);

private:
    uint32_t interval_ms_;              ///< PCR injection interval in ms
    uint64_t interval_90khz_;           ///< Interval in 90kHz ticks
    std::optional<PCR> last_pcr_;       ///< Last injected PCR
    size_t injection_count_;            ///< Number of PCRs injected

    /**
     * @brief Update internal state after PCR injection
     * @param pcr Injected PCR value
     */
    void updateState(const PCR& pcr);
};

} // namespace mpegts

#endif // MPEGTS_PCR_INJECTOR_HPP
