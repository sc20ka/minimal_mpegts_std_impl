#include "mpegts_muxer.hpp"
#include "mpegts_ts_packet_builder.hpp"
#include "mpegts_psi_generator.hpp"
#include "mpegts_private_data_manager.hpp"
#include <stdexcept>
#include <cstring>

namespace mpegts {

// ============================================================================
// Constructor / Destructor
// ============================================================================

MPEGTSMuxer::MPEGTSMuxer(const MuxerConfig& config)
    : config_(config)
{
    // Initialize PSI Generator
    psi_generator_ = std::make_unique<PSIGenerator>();
    psi_generator_->setTransportStreamID(config_.transport_stream_id);
    psi_generator_->setProgramNumber(config_.program_number, 0x1000);  // PMT PID = 0x1000
    psi_generator_->setPCRPID(config_.program_number, config_.pcr_pid);

    // Initialize Private Data Manager
    private_data_manager_ = std::make_unique<PrivateDataManager>();

    // Other components will be initialized in future weeks
    // scheduler_ will be added in Week 7
    // pcr_injector_ will be added in Week 6
    // bitrate_controller_ will be added in Week 9
}

MPEGTSMuxer::~MPEGTSMuxer() {
    // Unique pointers automatically cleaned up
}

// ============================================================================
// Stream Management
// ============================================================================

uint16_t MPEGTSMuxer::addStream(const StreamConfig& config) {
    // Validate configuration
    validateStreamConfig(config);

    // Check if PID is available
    if (!isPIDAvailable(config.pid)) {
        throw std::invalid_argument("PID already in use");
    }

    // Create stream context
    StreamContext context;
    context.pid = config.pid;
    context.config = config;

    // Create TS packet builder for this stream
    context.ts_builder = std::make_unique<TSPacketBuilder>(config.pid);

    // Add stream to PSI
    psi_generator_->addStream(config_.program_number, config.pid, config.type);

    // Store context
    streams_[config.pid] = std::move(context);

    // If PCR is enabled for this stream, update PCR PID
    if (config.pcr_enabled) {
        config_.pcr_pid = config.pid;
        psi_generator_->setPCRPID(config_.program_number, config.pid);
    }

    return config.pid;
}

void MPEGTSMuxer::removeStream(uint16_t pid) {
    auto it = streams_.find(pid);
    if (it == streams_.end()) {
        return;  // Stream not found, silently ignore
    }

    // Remove from PSI
    psi_generator_->removeStream(config_.program_number, pid);

    // Clear private data for this stream
    if (private_data_manager_) {
        private_data_manager_->clearStream(pid);
    }

    // Remove stream context
    streams_.erase(it);
}

// ============================================================================
// Data Input
// ============================================================================

void MPEGTSMuxer::feedElementaryData(uint16_t pid,
                                     const uint8_t* data, size_t length,
                                     uint64_t pts, uint64_t dts) {
    auto it = streams_.find(pid);
    if (it == streams_.end()) {
        throw std::invalid_argument("Stream PID not found");
    }

    // For Week 4: Simple implementation
    // Just store data with timestamps for now
    // PES packetization will be added in Week 5

    // Build simple TS packets with the data using TSPacketBuilder
    BuildOptions options;
    options.pusi = true;  // First packet has PUSI
    options.has_pcr = false;  // PCR will be added in Week 6

    auto packets = it->second.ts_builder->build(data, length, options);

    // Add packets to stream buffer (store as raw bytes)
    for (const auto& packet : packets) {
        it->second.packet_buffer.push(packet);
    }
}

// ============================================================================
// Output Control
// ============================================================================

std::vector<uint8_t> MPEGTSMuxer::getOutputPackets(size_t max_packets) {
    std::vector<uint8_t> output;

    // Generate PAT and PMT
    processPSI();

    // Process stream packets
    processStreams();

    // Collect output packets
    size_t packet_count = 0;
    while (!output_buffer_.empty()) {
        if (max_packets > 0 && packet_count >= max_packets) {
            break;
        }

        const auto& packet = output_buffer_.front();
        output.insert(output.end(), packet.begin(), packet.end());
        output_buffer_.erase(output_buffer_.begin());
        packet_count++;

        // Call output callback if set
        if (output_callback_) {
            output_callback_(packet.data(), packet.size());
        }
    }

    return output;
}

void MPEGTSMuxer::setOutputCallback(OutputCallback callback) {
    output_callback_ = callback;
}

// ============================================================================
// Configuration
// ============================================================================

void MPEGTSMuxer::setBitrate(uint32_t bitrate_bps) {
    config_.bitrate = bitrate_bps;
}

void MPEGTSMuxer::setMode(MuxMode mode) {
    config_.mode = mode;
}

void MPEGTSMuxer::setPCRPID(uint16_t pid) {
    config_.pcr_pid = pid;
    psi_generator_->setPCRPID(config_.program_number, pid);
}

void MPEGTSMuxer::setPCRInterval(uint32_t interval_ms) {
    config_.pcr_interval_ms = interval_ms;
}

// ============================================================================
// PSI Management
// ============================================================================

void MPEGTSMuxer::setProgramNumber(uint16_t program_num) {
    config_.program_number = program_num;
    // Would need to re-initialize PSI generator
}

void MPEGTSMuxer::setTransportStreamID(uint16_t tsid) {
    config_.transport_stream_id = tsid;
    psi_generator_->setTransportStreamID(tsid);
}

void MPEGTSMuxer::setPATInterval(uint32_t interval_ms) {
    config_.pat_interval_ms = interval_ms;
}

void MPEGTSMuxer::setPMTInterval(uint32_t interval_ms) {
    config_.pmt_interval_ms = interval_ms;
}

// ============================================================================
// Private Data Management
// ============================================================================

void MPEGTSMuxer::addPrivateData(uint16_t pid,
                                 const uint8_t* data, size_t length) {
    if (private_data_manager_) {
        private_data_manager_->addPrivateData(pid, data, length);
    }
}

void MPEGTSMuxer::addPrivateDataWithPTS(uint16_t pid,
                                        const uint8_t* data, size_t length,
                                        uint64_t pts) {
    if (private_data_manager_) {
        private_data_manager_->addPrivateDataWithPTS(pid, data, length, pts);
    }
}

void MPEGTSMuxer::setPrivateDataMode(uint16_t pid,
                                     PrivateDataInsertionMode mode) {
    if (private_data_manager_) {
        private_data_manager_->setInsertionMode(pid, mode);
    }
}

uint16_t MPEGTSMuxer::addPrivateDataStream(uint16_t pid, uint32_t bitrate) {
    StreamConfig config;
    config.type = StreamType::PRIVATE_DATA;
    config.pid = pid;
    config.stream_id = 0xBD;  // Private stream 1
    config.bitrate = bitrate;
    config.private_data_enabled = true;

    return addStream(config);
}

// ============================================================================
// Internal Processing Methods
// ============================================================================

void MPEGTSMuxer::processPSI() {
    // Generate PAT
    auto pat_data = psi_generator_->generatePAT();

    // Build TS packets for PAT (PID 0x0000)
    TSPacketBuilder pat_builder(0x0000);
    BuildOptions pat_options;
    pat_options.pusi = true;
    pat_options.has_pcr = false;

    auto pat_packets = pat_builder.build(pat_data.data(), pat_data.size(), pat_options);

    // Add to output buffer
    for (const auto& packet : pat_packets) {
        output_buffer_.push_back(packet);
    }

    // Generate PMT
    auto pmt_data = psi_generator_->generatePMT(config_.program_number);

    // Build TS packets for PMT (PID 0x1000)
    TSPacketBuilder pmt_builder(0x1000);
    BuildOptions pmt_options;
    pmt_options.pusi = true;
    pmt_options.has_pcr = false;

    auto pmt_packets = pmt_builder.build(pmt_data.data(), pmt_data.size(), pmt_options);

    // Add to output buffer
    for (const auto& packet : pmt_packets) {
        output_buffer_.push_back(packet);
    }
}

void MPEGTSMuxer::processPCR() {
    // PCR injection will be implemented in Week 6
}

void MPEGTSMuxer::processStreams() {
    // For Week 4: Simple round-robin processing
    // More sophisticated scheduling will be added in Week 7

    for (auto& pair : streams_) {
        StreamContext& context = pair.second;

        // Move packets from stream buffer to output buffer
        while (!context.packet_buffer.empty()) {
            output_buffer_.push_back(std::move(context.packet_buffer.front()));
            context.packet_buffer.pop();
        }
    }
}

void MPEGTSMuxer::outputPackets() {
    // Currently handled in getOutputPackets()
}

void MPEGTSMuxer::validateStreamConfig(const StreamConfig& config) {
    // Validate PID range (13-bit value: 0x0000 - 0x1FFF)
    if (config.pid > 0x1FFF) {
        throw std::invalid_argument("PID exceeds maximum value (0x1FFF)");
    }

    // Check reserved PIDs
    if (config.pid == 0x0000) {
        throw std::invalid_argument("PID 0x0000 is reserved for PAT");
    }
    if (config.pid == 0x0001) {
        throw std::invalid_argument("PID 0x0001 is reserved for CAT");
    }
    if (config.pid == 0x1FFF) {
        throw std::invalid_argument("PID 0x1FFF is reserved for null packets");
    }

    // Validate bitrate
    if (config.bitrate == 0) {
        throw std::invalid_argument("Bitrate must be greater than 0");
    }

    // Validate stream-specific parameters
    if (isVideoStream(config.type)) {
        if (config.width == 0 || config.height == 0) {
            throw std::invalid_argument("Video stream must have valid dimensions");
        }
        if (config.frame_rate == 0) {
            throw std::invalid_argument("Video stream must have valid frame rate");
        }
    }

    if (isAudioStream(config.type)) {
        if (config.sample_rate == 0) {
            throw std::invalid_argument("Audio stream must have valid sample rate");
        }
        if (config.channels == 0) {
            throw std::invalid_argument("Audio stream must have valid channel count");
        }
    }
}

bool MPEGTSMuxer::isPIDAvailable(uint16_t pid) const {
    // Check if PID is already used by a stream
    if (streams_.find(pid) != streams_.end()) {
        return false;
    }

    // Check if PID is used by PSI
    if (pid == 0x0000 || pid == 0x1000) {
        return false;
    }

    return true;
}

} // namespace mpegts
