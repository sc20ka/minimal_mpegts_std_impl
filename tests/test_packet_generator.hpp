#ifndef TEST_PACKET_GENERATOR_HPP
#define TEST_PACKET_GENERATOR_HPP

#include "mpegts_types.hpp"
#include <vector>
#include <random>
#include <cstdint>

namespace test {

// ============================================================================
// Packet Generator Configuration
// ============================================================================

struct GeneratorConfig {
    uint16_t pid = 0x100;                   // PID to use
    uint8_t  starting_cc = 0;               // Starting continuity counter
    bool     include_adaptation = false;    // Include adaptation field
    bool     include_private_data = false;  // Include private data
    size_t   payload_size = 184;            // Payload size (if no adaptation)
    bool     set_pusi = false;              // Payload unit start indicator
    uint8_t  payload_pattern = 0xAA;        // Pattern for payload data
};

// ============================================================================
// Scenario Configuration
// ============================================================================

struct ScenarioConfig {
    size_t   valid_packet_count = 10;      // Number of valid packets
    size_t   garbage_before = 0;            // Bytes of garbage before
    size_t   garbage_after = 0;             // Bytes of garbage after
    size_t   garbage_between_min = 0;       // Min garbage between packets
    size_t   garbage_between_max = 0;       // Max garbage between packets
    double   false_sync_probability = 0.0;  // Probability of false sync byte
    uint32_t random_seed = 12345;           // Random seed for reproducibility
};

// ============================================================================
// MPEG-TS Packet Generator
// ============================================================================

class PacketGenerator {
public:
    PacketGenerator();
    ~PacketGenerator() = default;

    /**
     * @brief Generate a single valid MPEG-TS packet
     * @param config Packet configuration
     * @return 188-byte packet
     */
    std::vector<uint8_t> generatePacket(const GeneratorConfig& config);

    /**
     * @brief Generate sequence of valid packets
     * @param count Number of packets
     * @param config Base configuration (CC will be incremented)
     * @return Vector of packets
     */
    std::vector<uint8_t> generateSequence(size_t count,
                                          const GeneratorConfig& config);

    /**
     * @brief Generate random garbage data
     * @param size Number of bytes
     * @param allow_false_sync Allow 0x47 bytes in garbage
     * @return Garbage data
     */
    std::vector<uint8_t> generateGarbage(size_t size,
                                         bool allow_false_sync = false);

    /**
     * @brief Generate complex scenario with mixed valid/garbage data
     * @param config Scenario configuration
     * @param gen_config Base packet configuration
     * @return Mixed data stream
     */
    std::vector<uint8_t> generateScenario(const ScenarioConfig& scenario,
                                          const GeneratorConfig& gen_config);

    /**
     * @brief Set random seed
     */
    void setSeed(uint32_t seed);

    /**
     * @brief Get expected packet positions in last generated scenario
     * @return Vector of byte offsets where valid packets start
     */
    const std::vector<size_t>& getPacketPositions() const {
        return packet_positions_;
    }

private:
    std::mt19937 rng_;
    std::vector<size_t> packet_positions_;

    /**
     * @brief Generate adaptation field
     */
    std::vector<uint8_t> generateAdaptationField(
        bool include_private_data,
        const uint8_t* private_data = nullptr,
        size_t private_len = 0);

    /**
     * @brief Generate packet header
     */
    void generateHeader(uint8_t* packet, const GeneratorConfig& config);
};

// ============================================================================
// Predefined Scenarios
// ============================================================================

namespace scenarios {

// Clean stream with no garbage
inline ScenarioConfig cleanStream(size_t packet_count = 10) {
    ScenarioConfig cfg;
    cfg.valid_packet_count = packet_count;
    cfg.garbage_before = 0;
    cfg.garbage_after = 0;
    cfg.garbage_between_min = 0;
    cfg.garbage_between_max = 0;
    return cfg;
}

// Stream with garbage at the beginning
inline ScenarioConfig garbagePrefix(size_t garbage_bytes = 100,
                                   size_t packet_count = 10) {
    ScenarioConfig cfg;
    cfg.valid_packet_count = packet_count;
    cfg.garbage_before = garbage_bytes;
    cfg.garbage_after = 0;
    cfg.garbage_between_min = 0;
    cfg.garbage_between_max = 0;
    return cfg;
}

// Stream with garbage between packets
inline ScenarioConfig garbageBetween(size_t min_garbage = 1,
                                    size_t max_garbage = 50,
                                    size_t packet_count = 10) {
    ScenarioConfig cfg;
    cfg.valid_packet_count = packet_count;
    cfg.garbage_before = 0;
    cfg.garbage_after = 0;
    cfg.garbage_between_min = min_garbage;
    cfg.garbage_between_max = max_garbage;
    return cfg;
}

// Stream with false sync bytes in garbage
inline ScenarioConfig falseSyncBytes(double probability = 0.1,
                                    size_t packet_count = 10) {
    ScenarioConfig cfg;
    cfg.valid_packet_count = packet_count;
    cfg.garbage_before = 200;
    cfg.garbage_between_min = 10;
    cfg.garbage_between_max = 50;
    cfg.false_sync_probability = probability;
    return cfg;
}

// Worst case: lots of garbage and false sync bytes
inline ScenarioConfig worstCase(size_t packet_count = 10) {
    ScenarioConfig cfg;
    cfg.valid_packet_count = packet_count;
    cfg.garbage_before = 500;
    cfg.garbage_after = 500;
    cfg.garbage_between_min = 20;
    cfg.garbage_between_max = 100;
    cfg.false_sync_probability = 0.15;
    return cfg;
}

// Minimal case: just enough for 3-iteration validation
inline ScenarioConfig minimal() {
    ScenarioConfig cfg;
    cfg.valid_packet_count = 3;
    cfg.garbage_before = 0;
    cfg.garbage_after = 0;
    cfg.garbage_between_min = 0;
    cfg.garbage_between_max = 0;
    return cfg;
}

} // namespace scenarios

} // namespace test

#endif // TEST_PACKET_GENERATOR_HPP
