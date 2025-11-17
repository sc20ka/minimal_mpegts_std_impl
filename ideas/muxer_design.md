# MPEG-TS Muxer: Architecture and Design

## 1. Overview

The MPEG-TS Muxer is the complement to the existing Demuxer, responsible for:
- **Multiplexing** multiple elementary streams into a single MPEG-TS transport stream
- **Generating** valid PAT/PMT tables for program structure
- **Managing** PCR injection for timing synchronization
- **Handling** PES packetization from raw elementary streams
- **Controlling** bitrate and packet scheduling

---

## 2. High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Elementary Streams Input                  │
│  (Video H.264/H.265, Audio AAC/MP3, Subtitles, etc.)       │
└────────────────┬────────────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────────────┐
│              PES Packetizer (per stream)                     │
│  - PTS/DTS generation                                        │
│  - Elementary stream → PES packets                           │
│  - Stream-specific headers                                   │
└────────────────┬────────────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────────────┐
│              TS Packet Generator                             │
│  - PES → TS packets (188 bytes)                             │
│  - Continuity counter management                             │
│  - Adaptation field insertion                                │
│  - Payload unit start indicator                              │
└────────────────┬────────────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────────────┐
│              PSI Generator                                   │
│  - PAT (Program Association Table)                           │
│  - PMT (Program Map Table)                                   │
│  - CRC-32 calculation                                        │
│  - Section formatting                                        │
└────────────────┬────────────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────────────┐
│              PCR Manager                                     │
│  - PCR base/extension calculation                            │
│  - PCR injection scheduling (every ~40ms)                    │
│  - System clock reference                                    │
└────────────────┬────────────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────────────┐
│              Multiplexer Core                                │
│  - Stream interleaving                                       │
│  - Priority scheduling                                       │
│  - Bitrate control                                           │
│  - NULL packet insertion                                     │
└────────────────┬────────────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────────────┐
│              Output Buffer                                   │
│  - CBR/VBR mode support                                      │
│  - Packet alignment                                          │
│  - Write callbacks                                           │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Core Components

### 3.1 MPEGTSMuxer (Main Class)

**Responsibilities:**
- Coordinate all muxing operations
- Manage multiple input streams
- Control output bitrate
- Generate PSI tables periodically

**Key Methods:**
```cpp
class MPEGTSMuxer {
public:
    // Stream management
    uint16_t addStream(StreamType type, const StreamConfig& config);
    void removeStream(uint16_t pid);

    // Data input
    void feedElementaryData(uint16_t pid, const uint8_t* data,
                           size_t length, uint64_t pts, uint64_t dts);

    // Output control
    std::vector<uint8_t> getOutputPackets(size_t max_packets = 0);
    void setOutputCallback(OutputCallback callback);

    // Configuration
    void setBitrate(uint32_t bitrate_bps);
    void setMode(MuxMode mode); // CBR/VBR
    void setPCRPID(uint16_t pid);
    void setPCRInterval(uint32_t interval_ms);

    // PSI management
    void setProgramNumber(uint16_t program_num);
    void setTransportStreamID(uint16_t tsid);
    void setPATInterval(uint32_t interval_ms);
    void setPMTInterval(uint32_t interval_ms);
};
```

---

### 3.2 PESPacketizer

**Responsibilities:**
- Convert elementary stream data to PES packets
- Add PTS/DTS timestamps
- Handle stream-specific formatting

**Key Features:**
```cpp
class PESPacketizer {
public:
    PESPacketizer(uint8_t stream_id, StreamType type);

    // Create PES packet from ES data
    std::vector<uint8_t> packetize(const uint8_t* data, size_t length,
                                   uint64_t pts, uint64_t dts = NO_DTS);

    // Configure
    void setStreamID(uint8_t stream_id);
    void enableDataAlignment(bool enable);
    void enableCopyright(bool enable);

private:
    uint8_t stream_id_;
    StreamType type_;
    bool data_alignment_;
    bool copyright_;
};
```

---

### 3.3 TSPacketBuilder

**Responsibilities:**
- Build 188-byte TS packets from PES packets
- Manage continuity counters
- Insert adaptation fields
- Handle stuffing bytes

**Key Features:**
```cpp
class TSPacketBuilder {
public:
    TSPacketBuilder(uint16_t pid);

    // Build TS packets from payload
    std::vector<TSPacket> build(const uint8_t* payload, size_t length,
                                bool pusi, bool has_pcr = false,
                                uint64_t pcr_value = 0);

    // Single packet with custom adaptation
    TSPacket buildPacket(const uint8_t* payload, size_t payload_len,
                        const AdaptationField* adaptation);

    // Null packet for padding
    static TSPacket createNullPacket();

private:
    uint16_t pid_;
    uint8_t continuity_counter_;

    void insertAdaptationField(TSPacket& packet,
                              const AdaptationField& adapt);
};
```

---

### 3.4 PSIGenerator

**Responsibilities:**
- Generate PAT (Program Association Table)
- Generate PMT (Program Map Table)
- Calculate CRC-32 checksums
- Format PSI sections

**Key Features:**
```cpp
class PSIGenerator {
public:
    // PAT generation
    std::vector<uint8_t> generatePAT(uint16_t transport_stream_id,
                                    const PATEntry& programs);

    // PMT generation
    std::vector<uint8_t> generatePMT(uint16_t program_number,
                                    uint16_t pcr_pid,
                                    const std::vector<PMTEntry>& streams);

    // Section wrapping
    std::vector<uint8_t> wrapInSection(uint8_t table_id,
                                      const uint8_t* data, size_t length,
                                      uint16_t table_id_extension);

private:
    uint32_t calculateCRC32(const uint8_t* data, size_t length);
    uint8_t version_number_;
};
```

---

### 3.5 PCRInjector

**Responsibilities:**
- Calculate PCR values based on system clock
- Inject PCR into adaptation fields
- Maintain timing accuracy

**Key Features:**
```cpp
class PCRInjector {
public:
    PCRInjector(uint16_t pcr_pid);

    // Check if PCR should be injected
    bool shouldInjectPCR(uint64_t current_time_us);

    // Generate PCR value
    uint64_t getCurrentPCR();

    // Create adaptation field with PCR
    AdaptationField createPCRAdaptation(uint64_t pcr_value);

    // Configure
    void setPCRInterval(uint32_t interval_ms);
    void setBaseClock(uint64_t base_time_us);

private:
    uint16_t pcr_pid_;
    uint32_t pcr_interval_us_;
    uint64_t last_pcr_time_;
    uint64_t base_time_;
};
```

---

### 3.6 StreamScheduler

**Responsibilities:**
- Determine which stream to service next
- Manage buffer levels
- Ensure smooth interleaving
- Handle priority streams (PSI, PCR)

**Key Features:**
```cpp
class StreamScheduler {
public:
    enum Priority {
        PRIORITY_PSI,      // Highest (PAT/PMT)
        PRIORITY_PCR,      // High (PCR packets)
        PRIORITY_VIDEO,    // Medium-High
        PRIORITY_AUDIO,    // Medium
        PRIORITY_DATA      // Low
    };

    // Add stream to schedule
    void addStream(uint16_t pid, Priority priority, uint32_t bitrate);

    // Get next PID to service
    uint16_t getNextPID();

    // Update buffer status
    void updateBufferLevel(uint16_t pid, size_t bytes_available);

    // Bitrate management
    void setBitrate(uint32_t total_bitrate_bps);
    void setStreamBitrate(uint16_t pid, uint32_t bitrate_bps);

private:
    struct StreamInfo {
        uint16_t pid;
        Priority priority;
        uint32_t bitrate;
        size_t buffer_level;
        uint64_t last_service_time;
    };

    std::map<uint16_t, StreamInfo> streams_;
    uint32_t total_bitrate_;
};
```

---

### 3.7 BitrateController

**Responsibilities:**
- Maintain constant bitrate (CBR mode)
- Insert NULL packets for padding
- Calculate packet timing

**Key Features:**
```cpp
class BitrateController {
public:
    enum Mode {
        CBR,  // Constant Bitrate
        VBR   // Variable Bitrate
    };

    BitrateController(Mode mode, uint32_t bitrate_bps);

    // Check if output is ready
    bool canOutputPacket(uint64_t current_time_us);

    // Calculate packets needed for time period
    size_t getPacketsForDuration(uint32_t duration_us);

    // Insert NULL packets if needed
    void padToTime(std::vector<TSPacket>& output,
                   uint64_t target_time_us);

private:
    Mode mode_;
    uint32_t bitrate_bps_;
    uint64_t packet_interval_us_;
    uint64_t last_output_time_;
};
```

---

### 3.8 PrivateDataManager

**Responsibilities:**
- Manage private data insertion in adaptation fields
- Handle transport_private_data_flag
- Support custom metadata and auxiliary data
- Coordinate with TSPacketBuilder

**Key Features:**
```cpp
class PrivateDataManager {
public:
    PrivateDataManager();

    // Add private data to be inserted
    void addPrivateData(uint16_t pid, const uint8_t* data,
                       size_t length, uint64_t pts = 0);

    // Check if private data available for PID
    bool hasPrivateData(uint16_t pid) const;

    // Get next private data for PID
    std::vector<uint8_t> getPrivateData(uint16_t pid);

    // Configure private data insertion strategy
    enum InsertionMode {
        INSERT_WITH_PCR,        // Insert with PCR packets
        INSERT_STANDALONE,      // Create dedicated packets
        INSERT_WITH_PAYLOAD     // Insert with payload packets
    };

    void setInsertionMode(uint16_t pid, InsertionMode mode);

    // Maximum private data size per packet
    void setMaxSize(size_t max_bytes); // Default: 182 bytes

private:
    struct PrivateDataEntry {
        std::vector<uint8_t> data;
        uint64_t pts;
        InsertionMode mode;
    };

    std::map<uint16_t, std::queue<PrivateDataEntry>> private_data_queues_;
    InsertionMode default_mode_;
    size_t max_private_data_size_;
};
```

---

## 4. Private Data Support

### 4.1 Overview

Private data support allows embedding custom metadata and auxiliary information
within the MPEG-TS stream through the adaptation field's
`transport_private_data` mechanism.

### 4.2 Use Cases

**Typical Applications:**
- **Timecode/SMPTE metadata** - Frame-accurate timestamps
- **Closed captions/subtitles** - Text overlay data
- **Analytics metadata** - Scene detection, object tracking
- **Custom signaling** - Application-specific commands
- **Encryption keys** - Conditional access metadata
- **Audio metadata** - Dolby/DTS descriptors

### 4.3 Private Data Insertion Methods

#### Method 1: In Adaptation Field (Recommended)
```
TS Packet Header (4 bytes)
    ↓
Adaptation Field
    adaptation_field_length
    flags
    [PCR if present]
    [OPCR if present]
    → transport_private_data_flag = 1
    → transport_private_data_length
    → private_data (up to 182 bytes)
    ↓
Payload (if present)
```

**Advantages:**
- Standard-compliant
- Co-located with payload
- Efficient bandwidth usage

#### Method 2: Dedicated Private Stream
```
Create separate PID for private data:
- stream_type = 0x06 (PES_private_data)
- Encapsulate in PES packets
- Full control over data structure
```

**Advantages:**
- Unlimited data size
- Independent bitrate control
- Easy filtering by receivers

### 4.4 Integration with TSPacketBuilder

```cpp
// Example: Adding private data to packet
TSPacketBuilder builder(pid);

// Prepare private data
uint8_t private_data[] = {0x01, 0x02, 0x03, 0x04};

// Create adaptation field with private data
AdaptationField adapt;
adapt.transport_private_data_flag = true;
adapt.private_data_length = sizeof(private_data);
adapt.private_data = private_data;

// Build packet with adaptation field
auto packets = builder.build(payload, payload_len,
                             true, false, 0, &adapt);
```

### 4.5 Private Data API in MPEGTSMuxer

```cpp
class MPEGTSMuxer {
public:
    // ... existing methods ...

    // ===== Private Data Management =====

    // Add private data for specific PID
    void addPrivateData(uint16_t pid,
                       const uint8_t* data, size_t length);

    // Add timestamped private data
    void addPrivateDataWithPTS(uint16_t pid,
                              const uint8_t* data, size_t length,
                              uint64_t pts);

    // Set private data insertion mode
    void setPrivateDataMode(uint16_t pid,
                           PrivateDataInsertionMode mode);

    // Create dedicated private data stream
    uint16_t addPrivateDataStream(uint16_t pid,
                                  uint32_t bitrate = 64000);

private:
    std::unique_ptr<PrivateDataManager> private_data_manager_;
};
```

### 4.6 Private Data Timing

**Synchronization with main stream:**
- Private data can be associated with PTS timestamps
- Muxer ensures private data appears close to associated video frame
- Configurable insertion strategy (every packet, PCR packets only, etc.)

**Buffer management:**
- Private data queue per PID
- FIFO ordering
- Overflow handling (drop oldest or newest)

### 4.7 Implementation Plan

**Week 2 (TSPacketBuilder):**
- [ ] Add adaptation field with private data support
- [ ] Test private data insertion
- [ ] Validate max size constraints (182 bytes)

**Week 4 (Basic Stream Management):**
- [ ] Add PrivateDataManager skeleton
- [ ] Implement queue management
- [ ] Test with simple private data

**Week 10 (Advanced Features):**
- [ ] Add PTS-synchronized private data
- [ ] Implement insertion strategies
- [ ] Add dedicated private stream support
- [ ] Comprehensive testing with demuxer

---

## 5. Data Flow

### 5.1 Input Processing
```
Elementary Stream Data
         ↓
    [PESPacketizer]
         ↓
    PES Packets
         ↓
    [TSPacketBuilder]
         ↓
    TS Packets (in buffer)
```

### 5.2 Multiplexing Loop
```
1. Check PCR injection needed → inject if necessary
2. Check PSI table needed → generate PAT/PMT if necessary
3. StreamScheduler.getNextPID() → select stream
4. Get TS packet from stream buffer
5. BitrateController.canOutputPacket() → check timing
6. Output packet
7. Insert NULL packets if CBR mode requires padding
8. Repeat
```

---

## 6. Configuration Structures

### 6.1 StreamConfig
```cpp
struct StreamConfig {
    StreamType type;           // VIDEO_H264, AUDIO_AAC, etc.
    uint16_t pid;             // Stream PID
    uint8_t stream_id;        // PES stream ID
    uint32_t bitrate;         // Stream bitrate (bps)
    bool pcr_enabled;         // Generate PCR for this stream

    // Private data support
    bool private_data_enabled;        // Enable private data insertion
    PrivateDataInsertionMode mode;    // Insertion strategy
    size_t max_private_data_size;     // Max bytes per packet (default: 182)

    // Video-specific
    uint16_t width;
    uint16_t height;
    uint8_t frame_rate;

    // Audio-specific
    uint32_t sample_rate;
    uint8_t channels;
};
```

### 6.2 MuxerConfig
```cpp
struct MuxerConfig {
    uint32_t bitrate;              // Total bitrate (bps)
    MuxMode mode;                  // CBR/VBR
    uint16_t transport_stream_id;
    uint16_t program_number;
    uint16_t pcr_pid;
    uint32_t pcr_interval_ms;      // Default: 40ms
    uint32_t pat_interval_ms;      // Default: 100ms
    uint32_t pmt_interval_ms;      // Default: 100ms
};
```

---

## 7. Stream Types Support

### Phase 1 (Core):
- ✅ H.264 Video (stream_type = 0x1B)
- ✅ AAC Audio (stream_type = 0x0F)
- ✅ Private data streams

### Phase 2 (Extended):
- H.265/HEVC Video (stream_type = 0x24)
- MP3 Audio (stream_type = 0x03/0x04)
- AC-3 Audio (stream_type = 0x81)

### Phase 3 (Advanced):
- DVB Subtitles
- Teletext
- Data carousels

---

## 8. Performance Targets

| Metric | Target | Notes |
|--------|--------|-------|
| Max bitrate | 50 Mbps | Single stream output |
| Streams | 8 concurrent | Typical broadcast scenario |
| Latency | < 100ms | Input to output delay |
| PCR accuracy | ± 100ns | ISO/IEC 13818-1 compliant |
| CPU usage | < 30% | Single core @ 2GHz |
| Memory | < 50 MB | For typical muxing session |

---

## 9. Error Handling

### Input Validation:
- Check PTS/DTS monotonicity
- Validate elementary stream syntax
- Detect buffer overflows

### Output Protection:
- Ensure valid TS packet structure
- Maintain continuity counter integrity
- Prevent bitrate violations

### Recovery Strategies:
- Drop frames on buffer overflow
- Insert NULL packets on underflow
- Reset on fatal errors

---

## 10. Testing Strategy

### Unit Tests:
- PESPacketizer: PTS/DTS encoding
- TSPacketBuilder: Packet structure
- PSIGenerator: PAT/PMT validity, CRC
- PCRInjector: Timing accuracy
- BitrateController: CBR accuracy

### Integration Tests:
- Multi-stream muxing
- PSI insertion timing
- PCR injection timing
- Bitrate conformance

### Compliance Tests:
- ISO/IEC 13818-1 validation
- DVB compliance (optional)
- TR 101 290 checks

---

## 11. API Usage Example

```cpp
#include "mpegts_muxer.hpp"

int main() {
    using namespace mpegts;

    // Create muxer
    MuxerConfig config;
    config.bitrate = 5'000'000;  // 5 Mbps
    config.mode = MuxMode::CBR;
    config.program_number = 1;
    config.transport_stream_id = 1;

    MPEGTSMuxer muxer(config);

    // Add video stream
    StreamConfig video;
    video.type = StreamType::VIDEO_H264;
    video.pid = 0x100;
    video.bitrate = 4'000'000;  // 4 Mbps
    video.pcr_enabled = true;
    uint16_t video_pid = muxer.addStream(video);

    // Add audio stream
    StreamConfig audio;
    audio.type = StreamType::AUDIO_AAC;
    audio.pid = 0x101;
    audio.bitrate = 192'000;  // 192 kbps
    uint16_t audio_pid = muxer.addStream(audio);

    // Set PCR stream
    muxer.setPCRPID(video_pid);

    // Feed data
    uint8_t video_frame[100000];
    uint64_t pts = 0;
    muxer.feedElementaryData(video_pid, video_frame,
                            sizeof(video_frame), pts, pts);

    // Get output
    auto output = muxer.getOutputPackets();
    write_to_file(output.data(), output.size());

    return 0;
}
```

---

## 12. Implementation Phases

### Phase 1: Core Muxing (4-6 weeks)
- TSPacketBuilder
- Basic stream management
- Simple PSI generation (PAT/PMT)
- Single stream support

### Phase 2: Multi-Stream (3-4 weeks)
- PESPacketizer
- StreamScheduler
- Multiple concurrent streams
- PCR injection

### Phase 3: Advanced Features (3-4 weeks)
- BitrateController (CBR/VBR)
- Advanced PSI (multiple programs)
- Stream synchronization
- Performance optimization

### Phase 4: Polish (2-3 weeks)
- Comprehensive testing
- Documentation
- Examples
- Bug fixes

---

## 13. Dependencies

### Internal (from Demuxer):
- `mpegts_types.hpp` - Reuse structures
- `mpegts_packet.hpp` - TSPacket class (read-only use)
- `mpegts_psi.hpp` - CRC-32 calculation

### External:
- C++17 standard library
- `<chrono>` for timing
- `<queue>` for buffering

---

## 14. Future Extensions

### Advanced Features:
- Multi-program transport streams
- Service Information (SI) tables
- Conditional Access (CA) integration
- Splicing and advertisement insertion

### Optimization:
- SIMD for packet assembly
- Zero-copy buffers
- GPU acceleration for encoding integration

### Interoperability:
- FFmpeg integration
- GStreamer plugin
- Network streaming (RTP, UDP)
