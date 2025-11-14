#include "test_packet_generator.hpp"
#include <cstring>
#include <algorithm>

namespace test {

PacketGenerator::PacketGenerator()
    : rng_(std::random_device{}())
{
}

void PacketGenerator::setSeed(uint32_t seed) {
    rng_.seed(seed);
}

void PacketGenerator::generateHeader(uint8_t* packet, const GeneratorConfig& config) {
    // Byte 0: Sync byte
    packet[0] = mpegts::MPEGTS_SYNC_BYTE;

    // Byte 1: TEI=0, PUSI, Priority=0, PID[12:8]
    packet[1] = 0x00;
    if (config.set_pusi) {
        packet[1] |= 0x40;
    }
    packet[1] |= (config.pid >> 8) & 0x1F;

    // Byte 2: PID[7:0]
    packet[2] = config.pid & 0xFF;

    // Byte 3: Scrambling=00, Adaptation control, CC
    uint8_t adaptation_control = 0x01; // Payload only
    if (config.include_adaptation) {
        adaptation_control = 0x03; // Both adaptation and payload
    }

    packet[3] = 0x00;
    packet[3] |= (adaptation_control << 4);
    packet[3] |= (config.starting_cc & 0x0F);
}

std::vector<uint8_t> PacketGenerator::generateAdaptationField(
    bool include_private_data,
    const uint8_t* private_data,
    size_t private_len)
{
    std::vector<uint8_t> adaptation;

    if (!include_private_data) {
        // Minimal adaptation field
        adaptation.push_back(0x01); // Length
        adaptation.push_back(0x00); // Flags (all off)
        return adaptation;
    }

    // Adaptation field with private data
    uint8_t total_length = 1 + 1 + private_len; // flags + private_len + data
    adaptation.push_back(total_length);
    adaptation.push_back(0x02); // transport_private_data_flag set

    // Private data length
    adaptation.push_back(static_cast<uint8_t>(private_len));

    // Private data
    if (private_data && private_len > 0) {
        adaptation.insert(adaptation.end(), private_data, private_data + private_len);
    }

    return adaptation;
}

std::vector<uint8_t> PacketGenerator::generatePacket(const GeneratorConfig& config) {
    std::vector<uint8_t> packet(mpegts::MPEGTS_PACKET_SIZE, 0xFF);

    // Generate header
    generateHeader(packet.data(), config);

    size_t offset = 4;

    // Add adaptation field if requested
    if (config.include_adaptation) {
        std::vector<uint8_t> private_data;
        if (config.include_private_data) {
            // Generate some test private data
            private_data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
        }

        auto adaptation = generateAdaptationField(
            config.include_private_data,
            private_data.empty() ? nullptr : private_data.data(),
            private_data.size()
        );

        // Copy adaptation field
        std::copy(adaptation.begin(), adaptation.end(), packet.begin() + offset);
        offset += adaptation.size();
    }

    // Fill payload with pattern
    size_t payload_remaining = mpegts::MPEGTS_PACKET_SIZE - offset;
    for (size_t i = 0; i < payload_remaining; ++i) {
        packet[offset + i] = config.payload_pattern;
    }

    return packet;
}

std::vector<uint8_t> PacketGenerator::generateSequence(
    size_t count,
    const GeneratorConfig& base_config)
{
    std::vector<uint8_t> sequence;
    sequence.reserve(count * mpegts::MPEGTS_PACKET_SIZE);

    GeneratorConfig config = base_config;

    for (size_t i = 0; i < count; ++i) {
        auto packet = generatePacket(config);
        sequence.insert(sequence.end(), packet.begin(), packet.end());

        // Increment continuity counter
        config.starting_cc = (config.starting_cc + 1) % 16;
    }

    return sequence;
}

std::vector<uint8_t> PacketGenerator::generateGarbage(
    size_t size,
    bool allow_false_sync)
{
    std::vector<uint8_t> garbage(size);
    std::uniform_int_distribution<int> dist(0, 255);

    for (size_t i = 0; i < size; ++i) {
        uint8_t byte = dist(rng_);

        // Avoid sync byte if not allowed
        if (!allow_false_sync && byte == mpegts::MPEGTS_SYNC_BYTE) {
            byte = 0x46; // Use different value
        }

        garbage[i] = byte;
    }

    // Add false sync bytes with configured probability
    if (allow_false_sync) {
        std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
        for (size_t i = 0; i < size; ++i) {
            if (prob_dist(rng_) < 0.01) { // 1% chance for sync byte
                garbage[i] = mpegts::MPEGTS_SYNC_BYTE;
            }
        }
    }

    return garbage;
}

std::vector<uint8_t> PacketGenerator::generateScenario(
    const ScenarioConfig& scenario,
    const GeneratorConfig& gen_config)
{
    packet_positions_.clear();

    // Set random seed for reproducibility
    setSeed(scenario.random_seed);

    std::vector<uint8_t> data;
    GeneratorConfig config = gen_config;

    // Add garbage before
    if (scenario.garbage_before > 0) {
        auto garbage = generateGarbage(
            scenario.garbage_before,
            scenario.false_sync_probability > 0.0
        );
        data.insert(data.end(), garbage.begin(), garbage.end());
    }

    // Generate packets with garbage between
    std::uniform_int_distribution<size_t> garbage_dist(
        scenario.garbage_between_min,
        scenario.garbage_between_max
    );

    for (size_t i = 0; i < scenario.valid_packet_count; ++i) {
        // Record packet position
        packet_positions_.push_back(data.size());

        // Generate and add packet
        auto packet = generatePacket(config);
        data.insert(data.end(), packet.begin(), packet.end());

        // Increment CC
        config.starting_cc = (config.starting_cc + 1) % 16;

        // Add garbage between packets (except after last)
        if (i + 1 < scenario.valid_packet_count &&
            scenario.garbage_between_max > 0) {

            size_t garbage_size = garbage_dist(rng_);
            if (garbage_size > 0) {
                auto garbage = generateGarbage(
                    garbage_size,
                    scenario.false_sync_probability > 0.0
                );
                data.insert(data.end(), garbage.begin(), garbage.end());
            }
        }
    }

    // Add garbage after
    if (scenario.garbage_after > 0) {
        auto garbage = generateGarbage(
            scenario.garbage_after,
            scenario.false_sync_probability > 0.0
        );
        data.insert(data.end(), garbage.begin(), garbage.end());
    }

    return data;
}

} // namespace test
