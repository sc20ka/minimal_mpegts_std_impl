#include "mpegts_pcr_injector.hpp"
#include <stdexcept>

namespace mpegts {

// ============================================================================
// Constructor / Destructor
// ============================================================================

PCRInjector::PCRInjector(uint32_t interval_ms)
    : interval_ms_(interval_ms)
    , interval_90khz_(0)
    , injection_count_(0)
{
    // Convert interval from ms to 90kHz ticks
    // 90kHz = 90 ticks per millisecond
    interval_90khz_ = static_cast<uint64_t>(interval_ms_) * 90;
}

PCRInjector::~PCRInjector() {
}

// ============================================================================
// Public Methods
// ============================================================================

void PCRInjector::setInterval(uint32_t interval_ms) {
    if (interval_ms < 10 || interval_ms > 100) {
        throw std::invalid_argument("PCR interval must be between 10-100ms");
    }
    interval_ms_ = interval_ms;
    interval_90khz_ = static_cast<uint64_t>(interval_ms_) * 90;
}

bool PCRInjector::shouldInjectPCR(uint64_t current_pcr) {
    if (!last_pcr_.has_value()) {
        // First PCR should always be injected
        return true;
    }

    // Calculate difference from last PCR
    uint64_t last_pcr_90khz = last_pcr_->getValue90kHz();

    // Handle wraparound (33-bit PCR base)
    uint64_t diff;
    if (current_pcr >= last_pcr_90khz) {
        diff = current_pcr - last_pcr_90khz;
    } else {
        // Wraparound occurred
        diff = ((1ULL << 33) - last_pcr_90khz) + current_pcr;
    }

    // Inject if interval has passed
    return (diff >= interval_90khz_);
}

void PCRInjector::recordInjection(uint64_t pcr_value) {
    // Convert to PCR structure and update state
    PCR pcr = calculatePCR(pcr_value);
    updateState(pcr);
}

PCR PCRInjector::calculatePCR(uint64_t pts_dts) {
    // PTS/DTS are at 90kHz
    // PCR base is also at 90kHz, so we can use it directly
    // PCR extension represents the 27MHz remainder

    // Mask to 33 bits for PCR base
    uint64_t base = pts_dts & ((1ULL << 33) - 1);

    // Extension is always 0 when converting from 90kHz
    // (no sub-90kHz precision available)
    uint16_t extension = 0;

    return PCR(base, extension);
}

PCR PCRInjector::calculatePCRFrom27MHz(uint64_t timestamp_27mhz) {
    // PCR(i) = PCR_base(i) × 300 + PCR_ext(i)
    // Therefore:
    // PCR_base = timestamp_27mhz / 300
    // PCR_ext = timestamp_27mhz % 300

    uint64_t base = timestamp_27mhz / 300;
    uint16_t extension = timestamp_27mhz % 300;

    // Mask base to 33 bits
    base &= ((1ULL << 33) - 1);

    return PCR(base, extension);
}

std::vector<uint8_t> PCRInjector::encodePCR(const PCR& pcr) {
    std::vector<uint8_t> encoded(6);

    // PCR encoding format (6 bytes = 48 bits):
    // PCR_base (33 bits) + reserved (6 bits) + PCR_extension (9 bits)
    //
    // Byte layout:
    // Byte 0: PCR_base[32:25]
    // Byte 1: PCR_base[24:17]
    // Byte 2: PCR_base[16:9]
    // Byte 3: PCR_base[8:1]
    // Byte 4: PCR_base[0] + reserved(6 bits '111111') + PCR_ext[8]
    // Byte 5: PCR_ext[7:0]

    uint64_t base = pcr.base;
    uint16_t ext = pcr.extension;

    // Encode PCR_base (33 bits)
    encoded[0] = (base >> 25) & 0xFF;
    encoded[1] = (base >> 17) & 0xFF;
    encoded[2] = (base >> 9) & 0xFF;
    encoded[3] = (base >> 1) & 0xFF;

    // Byte 4: LSB of base + reserved bits + MSB of extension
    encoded[4] = ((base & 0x01) << 7) |  // PCR_base[0]
                 0x7E |                   // Reserved bits (6 bits = 111111)
                 ((ext >> 8) & 0x01);    // PCR_ext[8]

    // Byte 5: Lower 8 bits of extension
    encoded[5] = ext & 0xFF;

    return encoded;
}

void PCRInjector::reset() {
    last_pcr_.reset();
    injection_count_ = 0;
}

double PCRInjector::calculatePCRDifferenceMs(const PCR& pcr1, const PCR& pcr2) {
    // Calculate difference in 27MHz ticks
    uint64_t value1 = pcr1.getValue27MHz();
    uint64_t value2 = pcr2.getValue27MHz();

    int64_t diff;
    if (value2 >= value1) {
        diff = value2 - value1;
    } else {
        // Handle wraparound
        // Max PCR value = (2^33 - 1) * 300 + 299
        uint64_t max_pcr = ((1ULL << 33) - 1) * 300 + 299;
        diff = (max_pcr - value1) + value2;
    }

    // Convert to milliseconds: 27MHz / 1000 = 27000 ticks per ms
    return diff / 27000.0;
}

bool PCRInjector::isValidPCR(const PCR& pcr) {
    return pcr.base < (1ULL << 33) && pcr.extension < 300;
}

// ============================================================================
// Private Methods
// ============================================================================

void PCRInjector::updateState(const PCR& pcr) {
    last_pcr_ = pcr;
    injection_count_++;
}

} // namespace mpegts
