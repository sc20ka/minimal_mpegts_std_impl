#ifndef MPEGTS_PCR_HPP
#define MPEGTS_PCR_HPP

#include <cstdint>
#include <optional>
#include <map>
#include <vector>

namespace mpegts {

// ============================================================================
// PCR (Program Clock Reference) Structures
// ============================================================================

/**
 * @brief PCR value (27 MHz clock reference)
 *
 * PCR consists of two parts:
 * - PCR_base: 33 bits (90 kHz clock)
 * - PCR_extension: 9 bits (27 MHz remainder)
 *
 * PCR(i) = PCR_base(i) × 300 + PCR_ext(i)
 */
struct PCR {
    uint64_t base;          ///< PCR base (33 bits, 90 kHz)
    uint16_t extension;     ///< PCR extension (9 bits, 27 MHz)

    PCR() : base(0), extension(0) {}
    PCR(uint64_t b, uint16_t ext) : base(b), extension(ext) {}

    /**
     * @brief Get full PCR value in 27 MHz ticks
     */
    uint64_t getValue27MHz() const {
        return base * 300 + extension;
    }

    /**
     * @brief Get PCR value in 90 kHz ticks (PTS/DTS compatible)
     */
    uint64_t getValue90kHz() const {
        return base;
    }

    /**
     * @brief Get PCR value in seconds
     */
    double getSeconds() const {
        return getValue27MHz() / 27000000.0;
    }

    /**
     * @brief Check if PCR is valid (not wrapped around max value)
     */
    bool isValid() const {
        return base < (1ULL << 33) && extension < 300;
    }
};

/**
 * @brief PCR statistics for a stream
 */
struct PCRStats {
    uint16_t pid;                       ///< PID of the stream
    size_t pcr_count;                   ///< Number of PCRs received

    std::optional<PCR> first_pcr;       ///< First PCR value
    std::optional<PCR> last_pcr;        ///< Most recent PCR value

    double average_interval_ms;         ///< Average PCR interval in milliseconds
    double max_jitter_ms;               ///< Maximum PCR jitter detected
    bool discontinuity_detected;        ///< PCR discontinuity flag

    PCRStats()
        : pid(0)
        , pcr_count(0)
        , average_interval_ms(0.0)
        , max_jitter_ms(0.0)
        , discontinuity_detected(false)
    {}
};

/**
 * @brief PCR sample (timestamp + packet counter)
 */
struct PCRSample {
    PCR pcr;                    ///< PCR value
    uint64_t packet_number;     ///< Packet number when PCR was extracted
    uint8_t continuity_counter; ///< CC at time of PCR

    PCRSample() : packet_number(0), continuity_counter(0) {}
    PCRSample(const PCR& p, uint64_t pn, uint8_t cc)
        : pcr(p), packet_number(pn), continuity_counter(cc) {}
};

// ============================================================================
// PCR Tracker
// ============================================================================

/**
 * @brief Tracks PCR values for a single PID
 *
 * Maintains history of PCR values and calculates statistics including:
 * - Average PCR interval
 * - PCR jitter detection
 * - Discontinuity detection
 * - PCR interpolation between samples
 */
class PCRTracker {
public:
    PCRTracker(uint16_t pid);
    ~PCRTracker() = default;

    /**
     * @brief Add new PCR sample
     * @param pcr PCR value
     * @param packet_number Global packet number
     * @param cc Continuity counter
     */
    void addPCR(const PCR& pcr, uint64_t packet_number, uint8_t cc);

    /**
     * @brief Get statistics for this stream
     */
    PCRStats getStats() const;

    /**
     * @brief Get most recent PCR
     */
    std::optional<PCR> getLastPCR() const;

    /**
     * @brief Interpolate PCR for a given packet number
     * @param packet_number Target packet number
     * @return Interpolated PCR value (if possible)
     */
    std::optional<PCR> interpolatePCR(uint64_t packet_number) const;

    /**
     * @brief Get all PCR samples
     */
    const std::vector<PCRSample>& getSamples() const { return samples_; }

    /**
     * @brief Clear all data
     */
    void clear();

    /**
     * @brief Check if discontinuity was detected
     */
    bool hasDiscontinuity() const { return discontinuity_detected_; }

private:
    uint16_t pid_;
    std::vector<PCRSample> samples_;

    double average_interval_ms_;
    double max_jitter_ms_;
    bool discontinuity_detected_;

    // Configuration
    static constexpr size_t MAX_SAMPLES = 1000;
    static constexpr double EXPECTED_PCR_INTERVAL_MS = 40.0;  // Typical: 40ms
    static constexpr double JITTER_THRESHOLD_MS = 5.0;         // 5ms jitter threshold
    static constexpr double DISCONTINUITY_THRESHOLD_MS = 100.0; // 100ms = discontinuity

    void updateStatistics();
    double calculateInterval(const PCRSample& s1, const PCRSample& s2) const;
};

// ============================================================================
// PCR Manager
// ============================================================================

/**
 * @brief Manages PCR tracking for all streams
 */
class PCRManager {
public:
    PCRManager() = default;
    ~PCRManager() = default;

    /**
     * @brief Add PCR for a specific PID
     */
    void addPCR(uint16_t pid, const PCR& pcr, uint64_t packet_number, uint8_t cc);

    /**
     * @brief Get tracker for specific PID
     */
    PCRTracker* getTracker(uint16_t pid);
    const PCRTracker* getTracker(uint16_t pid) const;

    /**
     * @brief Get statistics for all streams with PCR
     */
    std::vector<PCRStats> getAllStats() const;

    /**
     * @brief Get PIDs with PCR data
     */
    std::vector<uint16_t> getPIDsWithPCR() const;

    /**
     * @brief Clear all PCR data
     */
    void clear();

private:
    std::map<uint16_t, PCRTracker> trackers_;
};

// ============================================================================
// PCR Utilities
// ============================================================================

/**
 * @brief Extract PCR from adaptation field data
 * @param adaptation_field Pointer to adaptation field (after length byte)
 * @param length Length of adaptation field
 * @return PCR value if present, nullopt otherwise
 */
std::optional<PCR> extractPCR(const uint8_t* adaptation_field, size_t length);

/**
 * @brief Calculate difference between two PCRs (handles wraparound)
 * @return Difference in 27 MHz ticks
 */
int64_t pcrDifference(const PCR& pcr1, const PCR& pcr2);

/**
 * @brief Convert PCR difference to milliseconds
 */
double pcrDifferenceMs(const PCR& pcr1, const PCR& pcr2);

} // namespace mpegts

#endif // MPEGTS_PCR_HPP
