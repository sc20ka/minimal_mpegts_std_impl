#include "mpegts_psi.hpp"
#include <cstring>
#include <algorithm>

namespace mpegts {

// ============================================================================
// CRC-32 Table
// ============================================================================

const uint32_t PSIParser::crc32_table_[256] = {
    0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
    0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
    0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
    0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
    0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
    0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
    0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
    0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
    0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
    0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
    0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
    0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
    0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
    0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
    0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
    0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
    0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
    0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
    0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
    0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
    0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
    0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
    0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
    0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
    0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
    0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
    0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
    0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
    0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
    0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
    0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
    0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
    0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
    0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
    0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
    0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
    0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
    0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
    0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
    0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
    0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
    0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
    0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

uint32_t PSIParser::calculateCRC32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < length; ++i) {
        crc = (crc << 8) ^ crc32_table_[((crc >> 24) ^ data[i]) & 0xFF];
    }

    return crc;
}

bool PSIParser::verifyCRC32(const uint8_t* data, size_t length) {
    if (length < 4) {
        return false;
    }

    // CRC is last 4 bytes
    uint32_t expected_crc = (data[length - 4] << 24) |
                           (data[length - 3] << 16) |
                           (data[length - 2] << 8) |
                           data[length - 1];

    uint32_t calculated_crc = calculateCRC32(data, length - 4);

    return calculated_crc == expected_crc;
}

// ============================================================================
// PSI Section Header Parsing
// ============================================================================

size_t PSIParser::parseSectionHeader(const uint8_t* data, size_t length,
                                     PSISectionHeader& header) {
    if (length < 3) {
        return 0; // Not enough data
    }

    // Byte 0: table_id
    header.table_id = data[0];

    // Byte 1: section_syntax_indicator, section_length[11:8]
    header.section_syntax_indicator = (data[1] >> 7) & 0x01;
    header.section_length = ((data[1] & 0x0F) << 8) | data[2];

    if (!header.section_syntax_indicator) {
        // Short form, no more header
        return 3;
    }

    if (length < 8) {
        return 0; // Not enough data for long form
    }

    // Bytes 3-4: table_id_extension
    header.table_id_extension = (data[3] << 8) | data[4];

    // Byte 5: version_number, current_next_indicator
    header.version_number = (data[5] >> 1) & 0x1F;
    header.current_next_indicator = data[5] & 0x01;

    // Byte 6: section_number
    header.section_number = data[6];

    // Byte 7: last_section_number
    header.last_section_number = data[7];

    return 8; // Long form header size
}

// ============================================================================
// PAT Parsing
// ============================================================================

bool PSIParser::parsePAT(const uint8_t* data, size_t length, PAT& pat) {
    if (length < 8) {
        return false;
    }

    // Parse section header
    size_t header_size = parseSectionHeader(data, length, pat.header);
    if (header_size == 0) {
        return false;
    }

    // Verify table ID
    if (pat.header.table_id != TABLE_ID_PAT) {
        return false;
    }

    // Verify CRC
    size_t total_section_length = 3 + pat.header.section_length;
    if (total_section_length > length) {
        return false;
    }

    if (!verifyCRC32(data, total_section_length)) {
        return false;
    }

    // Transport stream ID is table_id_extension
    pat.transport_stream_id = pat.header.table_id_extension;

    // Parse program entries
    size_t offset = header_size;
    size_t entries_end = 3 + pat.header.section_length - 4; // -4 for CRC

    while (offset + 4 <= entries_end) {
        uint16_t program_number = (data[offset] << 8) | data[offset + 1];
        uint16_t pid = ((data[offset + 2] & 0x1F) << 8) | data[offset + 3];

        pat.programs.emplace_back(program_number, pid);
        offset += 4;
    }

    // Extract CRC
    pat.crc32 = (data[entries_end] << 24) |
                (data[entries_end + 1] << 16) |
                (data[entries_end + 2] << 8) |
                data[entries_end + 3];

    return true;
}

uint16_t PAT::getPMTPID(uint16_t program_number) const {
    for (const auto& entry : programs) {
        if (entry.program_number == program_number && entry.program_number != 0) {
            return entry.pid;
        }
    }
    return 0;
}

std::vector<uint16_t> PAT::getProgramNumbers() const {
    std::vector<uint16_t> result;
    for (const auto& entry : programs) {
        if (entry.program_number != 0) { // Skip NIT
            result.push_back(entry.program_number);
        }
    }
    return result;
}

// ============================================================================
// PMT Parsing
// ============================================================================

bool PSIParser::parsePMT(const uint8_t* data, size_t length, PMT& pmt) {
    if (length < 12) {
        return false;
    }

    // Parse section header
    size_t header_size = parseSectionHeader(data, length, pmt.header);
    if (header_size == 0) {
        return false;
    }

    // Verify table ID
    if (pmt.header.table_id != TABLE_ID_PMT) {
        return false;
    }

    // Verify CRC
    size_t total_section_length = 3 + pmt.header.section_length;
    if (total_section_length > length) {
        return false;
    }

    if (!verifyCRC32(data, total_section_length)) {
        return false;
    }

    // Program number is table_id_extension
    pmt.program_number = pmt.header.table_id_extension;

    size_t offset = header_size;

    // PCR PID (bytes 8-9)
    pmt.pcr_pid = ((data[offset] & 0x1F) << 8) | data[offset + 1];
    offset += 2;

    // Program info length (bytes 10-11)
    pmt.program_info_length = ((data[offset] & 0x0F) << 8) | data[offset + 1];
    offset += 2;

    // Program descriptors
    if (pmt.program_info_length > 0) {
        if (offset + pmt.program_info_length > length) {
            return false;
        }
        pmt.program_descriptors.assign(data + offset,
                                       data + offset + pmt.program_info_length);
        offset += pmt.program_info_length;
    }

    // Parse elementary streams
    size_t streams_end = 3 + pmt.header.section_length - 4; // -4 for CRC

    while (offset + 5 <= streams_end) {
        PMTStreamInfo stream;

        // Stream type
        stream.stream_type = static_cast<StreamType>(data[offset]);
        offset++;

        // Elementary PID
        stream.elementary_pid = ((data[offset] & 0x1F) << 8) | data[offset + 1];
        offset += 2;

        // ES info length
        stream.es_info_length = ((data[offset] & 0x0F) << 8) | data[offset + 1];
        offset += 2;

        // ES descriptors
        if (stream.es_info_length > 0) {
            if (offset + stream.es_info_length > streams_end) {
                return false;
            }
            stream.descriptors.assign(data + offset,
                                     data + offset + stream.es_info_length);
            offset += stream.es_info_length;
        }

        pmt.streams.push_back(stream);
    }

    // Extract CRC
    pmt.crc32 = (data[streams_end] << 24) |
                (data[streams_end + 1] << 16) |
                (data[streams_end + 2] << 8) |
                data[streams_end + 3];

    return true;
}

std::vector<uint16_t> PMT::getPIDsByType(StreamType type) const {
    std::vector<uint16_t> result;
    for (const auto& stream : streams) {
        if (stream.stream_type == type) {
            result.push_back(stream.elementary_pid);
        }
    }
    return result;
}

std::vector<uint16_t> PMT::getAllPIDs() const {
    std::vector<uint16_t> result;
    for (const auto& stream : streams) {
        result.push_back(stream.elementary_pid);
    }
    return result;
}

const PMTStreamInfo* PMT::getStreamInfo(uint16_t pid) const {
    for (const auto& stream : streams) {
        if (stream.elementary_pid == pid) {
            return &stream;
        }
    }
    return nullptr;
}

const char* getStreamTypeName(StreamType type) {
    switch (type) {
        case StreamType::MPEG1_VIDEO: return "MPEG-1 Video";
        case StreamType::MPEG2_VIDEO: return "MPEG-2 Video";
        case StreamType::MPEG1_AUDIO: return "MPEG-1 Audio";
        case StreamType::MPEG2_AUDIO: return "MPEG-2 Audio";
        case StreamType::PRIVATE_DATA: return "Private Data";
        case StreamType::AAC_AUDIO: return "AAC Audio";
        case StreamType::MPEG4_VISUAL: return "MPEG-4 Visual";
        case StreamType::H264_VIDEO: return "H.264/AVC Video";
        case StreamType::H265_VIDEO: return "H.265/HEVC Video";
        default: return "Unknown";
    }
}

// ============================================================================
// PSI Accumulator
// ============================================================================

PSIAccumulator::PSIAccumulator()
    : expected_length_(0)
    , complete_(false)
    , synced_(false)
{
}

bool PSIAccumulator::addData(const uint8_t* data, size_t length,
                             bool payload_unit_start) {
    if (payload_unit_start) {
        // New section starts
        reset();

        // Skip pointer field if present
        if (length > 0) {
            uint8_t pointer = data[0];
            data++;
            length--;

            // Skip to start of section
            if (pointer < length) {
                data += pointer;
                length -= pointer;
            } else {
                return false;
            }
        }

        synced_ = true;
    }

    if (!synced_) {
        return false; // Wait for PUSI
    }

    // Add data to buffer
    buffer_.insert(buffer_.end(), data, data + length);

    // Check if we have enough data to determine section length
    if (expected_length_ == 0 && buffer_.size() >= 3) {
        // Parse section length
        uint16_t section_length = ((buffer_[1] & 0x0F) << 8) | buffer_[2];
        expected_length_ = 3 + section_length;
    }

    // Check if section is complete
    if (expected_length_ > 0 && buffer_.size() >= expected_length_) {
        complete_ = true;
        return true;
    }

    return false;
}

size_t PSIAccumulator::getSection(std::vector<uint8_t>& section) {
    if (!complete_) {
        return 0;
    }

    section.assign(buffer_.begin(), buffer_.begin() + expected_length_);
    reset();

    return section.size();
}

void PSIAccumulator::reset() {
    buffer_.clear();
    expected_length_ = 0;
    complete_ = false;
    synced_ = false;
}

} // namespace mpegts
