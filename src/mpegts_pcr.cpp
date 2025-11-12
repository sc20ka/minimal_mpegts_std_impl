#include "mpegts_pcr.hpp"
#include <algorithm>
#include <cmath>

namespace mpegts {

// ============================================================================
// PCR Utilities
// ============================================================================

std::optional<PCR> extractPCR(const uint8_t* adaptation_field, size_t length) {
    if (!adaptation_field || length < 6) {
        return std::nullopt;
    }

    // Check PCR flag (bit 4 of first byte)
    uint8_t flags = adaptation_field[0];
    bool pcr_flag = (flags & 0x10) != 0;

    if (!pcr_flag) {
        return std::nullopt;
    }

    // PCR starts at byte 1 of adaptation field (after flags)
    // PCR format (6 bytes total):
    // - 33 bits: PCR_base
    // - 6 bits: reserved
    // - 9 bits: PCR_extension

    const uint8_t* pcr_bytes = adaptation_field + 1;

    // Extract PCR_base (33 bits from first 4 bytes + 1 bit from 5th byte)
    uint64_t pcr_base = 0;
    pcr_base |= static_cast<uint64_t>(pcr_bytes[0]) << 25;
    pcr_base |= static_cast<uint64_t>(pcr_bytes[1]) << 17;
    pcr_base |= static_cast<uint64_t>(pcr_bytes[2]) << 9;
    pcr_base |= static_cast<uint64_t>(pcr_bytes[3]) << 1;
    pcr_base |= static_cast<uint64_t>(pcr_bytes[4] >> 7) & 0x01;

    // Extract PCR_extension (9 bits from bytes 4-5)
    uint16_t pcr_ext = 0;
    pcr_ext |= static_cast<uint16_t>(pcr_bytes[4] & 0x01) << 8;
    pcr_ext |= static_cast<uint16_t>(pcr_bytes[5]);

    PCR pcr(pcr_base, pcr_ext);

    // Validate
    if (!pcr.isValid()) {
        return std::nullopt;
    }

    return pcr;
}

int64_t pcrDifference(const PCR& pcr1, const PCR& pcr2) {
    // Convert to 27 MHz ticks
    int64_t v1 = static_cast<int64_t>(pcr1.getValue27MHz());
    int64_t v2 = static_cast<int64_t>(pcr2.getValue27MHz());

    int64_t diff = v2 - v1;

    // Handle wraparound (PCR wraps at 2^33 * 300)
    const int64_t PCR_MAX = (1LL << 33) * 300;

    if (diff > PCR_MAX / 2) {
        diff -= PCR_MAX;
    } else if (diff < -PCR_MAX / 2) {
        diff += PCR_MAX;
    }

    return diff;
}

double pcrDifferenceMs(const PCR& pcr1, const PCR& pcr2) {
    int64_t diff_27mhz = pcrDifference(pcr1, pcr2);
    return (diff_27mhz / 27000.0);  // Convert to milliseconds
}

// ============================================================================
// PCR Tracker
// ============================================================================

PCRTracker::PCRTracker(uint16_t pid)
    : pid_(pid)
    , average_interval_ms_(0.0)
    , max_jitter_ms_(0.0)
    , discontinuity_detected_(false)
{
    samples_.reserve(MAX_SAMPLES);
}

void PCRTracker::addPCR(const PCR& pcr, uint64_t packet_number, uint8_t cc) {
    PCRSample sample(pcr, packet_number, cc);

    // Check for discontinuity
    if (!samples_.empty()) {
        const PCRSample& last = samples_.back();
        double interval = calculateInterval(last, sample);

        if (interval < 0 || interval > DISCONTINUITY_THRESHOLD_MS) {
            discontinuity_detected_ = true;
        }

        // Track jitter
        if (interval > 0 && interval < DISCONTINUITY_THRESHOLD_MS) {
            double jitter = std::abs(interval - EXPECTED_PCR_INTERVAL_MS);
            max_jitter_ms_ = std::max(max_jitter_ms_, jitter);
        }
    }

    samples_.push_back(sample);

    // Limit sample history
    if (samples_.size() > MAX_SAMPLES) {
        samples_.erase(samples_.begin());
    }

    updateStatistics();
}

PCRStats PCRTracker::getStats() const {
    PCRStats stats;
    stats.pid = pid_;
    stats.pcr_count = samples_.size();
    stats.average_interval_ms = average_interval_ms_;
    stats.max_jitter_ms = max_jitter_ms_;
    stats.discontinuity_detected = discontinuity_detected_;

    if (!samples_.empty()) {
        stats.first_pcr = samples_.front().pcr;
        stats.last_pcr = samples_.back().pcr;
    }

    return stats;
}

std::optional<PCR> PCRTracker::getLastPCR() const {
    if (samples_.empty()) {
        return std::nullopt;
    }
    return samples_.back().pcr;
}

std::optional<PCR> PCRTracker::interpolatePCR(uint64_t packet_number) const {
    if (samples_.size() < 2) {
        return std::nullopt;  // Need at least 2 samples for interpolation
    }

    // Find the two closest samples around the target packet
    const PCRSample* before = nullptr;
    const PCRSample* after = nullptr;

    for (size_t i = 0; i < samples_.size(); ++i) {
        if (samples_[i].packet_number <= packet_number) {
            before = &samples_[i];
        } else if (samples_[i].packet_number > packet_number) {
            after = &samples_[i];
            break;
        }
    }

    // If we have both samples, interpolate
    if (before && after) {
        // Linear interpolation
        uint64_t total_packets = after->packet_number - before->packet_number;
        uint64_t target_offset = packet_number - before->packet_number;

        if (total_packets == 0) {
            return before->pcr;
        }

        double ratio = static_cast<double>(target_offset) / total_packets;

        int64_t pcr_diff = pcrDifference(before->pcr, after->pcr);
        uint64_t interpolated_27mhz = before->pcr.getValue27MHz() +
                                      static_cast<uint64_t>(pcr_diff * ratio);

        // Convert back to PCR
        uint64_t base = interpolated_27mhz / 300;
        uint16_t ext = interpolated_27mhz % 300;

        return PCR(base, ext);
    }

    // Extrapolation: if only one side available
    if (before && samples_.size() >= 2) {
        // Use the last two samples to extrapolate forward
        size_t idx = 0;
        for (size_t i = 0; i < samples_.size(); ++i) {
            if (&samples_[i] == before) {
                idx = i;
                break;
            }
        }

        if (idx > 0) {
            const PCRSample& s1 = samples_[idx - 1];
            const PCRSample& s2 = samples_[idx];

            double interval = calculateInterval(s1, s2);
            uint64_t packet_diff = s2.packet_number - s1.packet_number;

            if (packet_diff > 0 && interval > 0) {
                double ms_per_packet = interval / packet_diff;
                double extrapolation_ms = ms_per_packet * (packet_number - s2.packet_number);

                uint64_t extrapolated_27mhz = s2.pcr.getValue27MHz() +
                                              static_cast<uint64_t>(extrapolation_ms * 27000.0);

                uint64_t base = extrapolated_27mhz / 300;
                uint16_t ext = extrapolated_27mhz % 300;

                return PCR(base, ext);
            }
        }
    }

    return std::nullopt;
}

void PCRTracker::clear() {
    samples_.clear();
    average_interval_ms_ = 0.0;
    max_jitter_ms_ = 0.0;
    discontinuity_detected_ = false;
}

void PCRTracker::updateStatistics() {
    if (samples_.size() < 2) {
        return;
    }

    // Calculate average interval from recent samples
    size_t sample_count = std::min<size_t>(samples_.size(), 100);
    double total_interval = 0.0;
    size_t valid_intervals = 0;

    for (size_t i = samples_.size() - sample_count; i < samples_.size() - 1; ++i) {
        double interval = calculateInterval(samples_[i], samples_[i + 1]);
        if (interval > 0 && interval < DISCONTINUITY_THRESHOLD_MS) {
            total_interval += interval;
            valid_intervals++;
        }
    }

    if (valid_intervals > 0) {
        average_interval_ms_ = total_interval / valid_intervals;
    }
}

double PCRTracker::calculateInterval(const PCRSample& s1, const PCRSample& s2) const {
    return pcrDifferenceMs(s1.pcr, s2.pcr);
}

// ============================================================================
// PCR Manager
// ============================================================================

void PCRManager::addPCR(uint16_t pid, const PCR& pcr, uint64_t packet_number, uint8_t cc) {
    // Get or create tracker
    auto it = trackers_.find(pid);
    if (it == trackers_.end()) {
        trackers_.emplace(pid, PCRTracker(pid));
        it = trackers_.find(pid);
    }

    it->second.addPCR(pcr, packet_number, cc);
}

PCRTracker* PCRManager::getTracker(uint16_t pid) {
    auto it = trackers_.find(pid);
    if (it != trackers_.end()) {
        return &it->second;
    }
    return nullptr;
}

const PCRTracker* PCRManager::getTracker(uint16_t pid) const {
    auto it = trackers_.find(pid);
    if (it != trackers_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<PCRStats> PCRManager::getAllStats() const {
    std::vector<PCRStats> result;
    result.reserve(trackers_.size());

    for (const auto& [pid, tracker] : trackers_) {
        result.push_back(tracker.getStats());
    }

    return result;
}

std::vector<uint16_t> PCRManager::getPIDsWithPCR() const {
    std::vector<uint16_t> result;
    result.reserve(trackers_.size());

    for (const auto& [pid, _] : trackers_) {
        result.push_back(pid);
    }

    return result;
}

void PCRManager::clear() {
    trackers_.clear();
}

} // namespace mpegts
