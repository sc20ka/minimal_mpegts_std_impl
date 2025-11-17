#include "mpegts_psi_generator.hpp"
#include <stdexcept>
#include <cstring>

namespace mpegts {

// ============================================================================
// Constructor
// ============================================================================

PSIGenerator::PSIGenerator()
    : transport_stream_id_(1)
    , pat_version_(0)
{
}

// ============================================================================
// Configuration
// ============================================================================

void PSIGenerator::setProgramNumber(uint16_t program_number, uint16_t pmt_pid) {
    if (program_number == 0) {
        throw std::invalid_argument("Program number 0 is reserved for NIT");
    }

    ProgramInfo info;
    info.pmt_pid = pmt_pid;
    info.pcr_pid = 0x1FFF;  // Default: no PCR
    info.version = 0;

    programs_[program_number] = info;
}

void PSIGenerator::removeProgram(uint16_t program_number) {
    programs_.erase(program_number);
}

void PSIGenerator::setPCRPID(uint16_t program_number, uint16_t pcr_pid) {
    auto it = programs_.find(program_number);
    if (it == programs_.end()) {
        throw std::invalid_argument("Program not found");
    }

    it->second.pcr_pid = pcr_pid;
}

void PSIGenerator::addStream(uint16_t program_number, uint16_t elementary_pid,
                             mpegts::StreamType stream_type) {
    auto it = programs_.find(program_number);
    if (it == programs_.end()) {
        throw std::invalid_argument("Program not found");
    }

    it->second.streams[elementary_pid] = stream_type;
}

void PSIGenerator::removeStream(uint16_t program_number, uint16_t elementary_pid) {
    auto it = programs_.find(program_number);
    if (it != programs_.end()) {
        it->second.streams.erase(elementary_pid);
    }
}

void PSIGenerator::incrementPATVersion() {
    pat_version_ = (pat_version_ + 1) & 0x1F;  // 5-bit version
}

void PSIGenerator::incrementPMTVersion(uint16_t program_number) {
    auto it = programs_.find(program_number);
    if (it != programs_.end()) {
        it->second.version = (it->second.version + 1) & 0x1F;  // 5-bit version
    }
}

void PSIGenerator::resetVersions() {
    pat_version_ = 0;
    for (auto& pair : programs_) {
        pair.second.version = 0;
    }
}

uint8_t PSIGenerator::getPMTVersion(uint16_t program_number) const {
    auto it = programs_.find(program_number);
    return (it != programs_.end()) ? it->second.version : 0;
}

// ============================================================================
// PAT Generation
// ============================================================================

std::vector<uint8_t> PSIGenerator::generatePAT() {
    std::vector<uint8_t> section;

    // Calculate section length
    // section_length = from end of section_length field to end of CRC (inclusive)
    // = 5 (header after section_length) + 4*num_programs + 4 (CRC)
    uint16_t section_length = 5 + (programs_.size() * 4) + 4;

    if (section_length > 1021) {
        throw std::runtime_error("PAT section too large");
    }

    // Build section header (8 bytes)
    buildSectionHeader(section, TABLE_ID_PAT, transport_stream_id_,
                      pat_version_, section_length);

    // Add program entries
    for (const auto& pair : programs_) {
        uint16_t program_number = pair.first;
        uint16_t pmt_pid = pair.second.pmt_pid;

        write16(section, program_number);
        write16(section, 0xE000 | (pmt_pid & 0x1FFF));  // reserved (3 bits) + PID (13 bits)
    }

    // Add CRC-32
    appendCRC32(section);

    return section;
}

// ============================================================================
// PMT Generation
// ============================================================================

std::vector<uint8_t> PSIGenerator::generatePMT(uint16_t program_number) {
    auto it = programs_.find(program_number);
    if (it == programs_.end()) {
        throw std::invalid_argument("Program not found");
    }

    const ProgramInfo& info = it->second;
    std::vector<uint8_t> section;

    // Calculate section length
    // = 9 (header after section_length: 5 fixed + 2 PCR_PID + 2 program_info_length)
    // + program_info_length (0 for now, no descriptors)
    // + streams (5 bytes each: 1 type + 2 PID + 2 ES_info_length)
    // + 4 (CRC)

    uint16_t program_info_length = 0;  // No program descriptors
    uint16_t streams_length = info.streams.size() * 5;  // 5 bytes per stream (no ES descriptors)
    uint16_t section_length = 9 + program_info_length + streams_length + 4;

    if (section_length > 1021) {
        throw std::runtime_error("PMT section too large");
    }

    // Build section header
    buildSectionHeader(section, TABLE_ID_PMT, program_number,
                      info.version, section_length);

    // PCR PID (3 reserved bits + 13 PID bits)
    write16(section, 0xE000 | (info.pcr_pid & 0x1FFF));

    // Program info length (4 reserved bits + 12 length bits)
    write16(section, 0xF000 | (program_info_length & 0x0FFF));

    // Program descriptors would go here (currently none)

    // Elementary stream entries
    for (const auto& stream_pair : info.streams) {
        uint16_t elementary_pid = stream_pair.first;
        mpegts::StreamType stream_type = stream_pair.second;

        // Stream type
        section.push_back(static_cast<uint8_t>(stream_type));

        // Elementary PID (3 reserved bits + 13 PID bits)
        write16(section, 0xE000 | (elementary_pid & 0x1FFF));

        // ES info length (4 reserved bits + 12 length bits)
        write16(section, 0xF000);  // No ES descriptors
    }

    // Add CRC-32
    appendCRC32(section);

    return section;
}

// ============================================================================
// Helper Methods
// ============================================================================

void PSIGenerator::buildSectionHeader(std::vector<uint8_t>& buffer,
                                     uint8_t table_id,
                                     uint16_t table_id_extension,
                                     uint8_t version_number,
                                     uint16_t section_length) {
    // Table ID
    buffer.push_back(table_id);

    // section_syntax_indicator (1), '0' (1), reserved (2), section_length (12)
    uint16_t byte1_2 = 0xB000 | (section_length & 0x0FFF);
    write16(buffer, byte1_2);

    // Table ID extension (transport_stream_id or program_number)
    write16(buffer, table_id_extension);

    // reserved (2), version_number (5), current_next_indicator (1)
    uint8_t byte5 = 0xC0 | ((version_number & 0x1F) << 1) | 0x01;
    buffer.push_back(byte5);

    // Section number
    buffer.push_back(0x00);

    // Last section number
    buffer.push_back(0x00);
}

void PSIGenerator::appendCRC32(std::vector<uint8_t>& data) {
    // Calculate CRC-32 over entire section (excluding the CRC itself)
    uint32_t crc = PSIParser::calculateCRC32(data.data(), data.size());

    // Append CRC in big-endian
    write32(data, crc);
}

void PSIGenerator::write16(std::vector<uint8_t>& buffer, uint16_t value) {
    buffer.push_back((value >> 8) & 0xFF);
    buffer.push_back(value & 0xFF);
}

void PSIGenerator::write32(std::vector<uint8_t>& buffer, uint32_t value) {
    buffer.push_back((value >> 24) & 0xFF);
    buffer.push_back((value >> 16) & 0xFF);
    buffer.push_back((value >> 8) & 0xFF);
    buffer.push_back(value & 0xFF);
}

} // namespace mpegts
