# Minimal MPEG-TS Standard Implementation

## 📖 Project Description

A minimalistic implementation of MPEG-TS (ISO/IEC 13818-1) **Demultiplexer** and **Multiplexer** in C++17, created for educational purposes and practical application in transport stream processing tasks.

## 🎯 Purpose

This project provides a clean, profile-agnostic implementation of MPEG-TS tools focused on:

### Demuxer (COMPLETE)
- **Stream processing** of MPEG-TS data with high reliability
- **Adaptive synchronization** in conditions with noise and garbage data
- **Recovery** of valid packets from arbitrary data
- **Separation** of main payload and private data
- **Management** of multiple programs and streams simultaneously

### Muxer (IN PROGRESS - 37.5% Complete)
- ✅ **TS packet building** - construct valid 188-byte MPEG-TS packets
- ✅ **PSI generation** (PAT/PMT) - program structure with CRC-32 validation
- ✅ **PCR injection** - timing synchronization with configurable intervals
- ✅ **PES packetization** - convert elementary streams to PES packets with PTS/DTS
- ✅ **Private data support** - metadata and auxiliary information insertion
- ⏳ **Stream scheduling** - multi-stream coordination (NEXT)
- ⏳ **Bitrate control** - CBR/VBR modes (PLANNED)
- ⏳ **Multi-stream multiplexing** - complete integration (PLANNED)

## ⚠️ Current Development Stage

**BETA v0.3.0 - Demuxer Complete + Muxer Core Components**

The project has completed **Phase 1 & 2: Full Demuxer** with comprehensive MPEG-TS support, and is progressing through **Phase 4: MPEG-TS Muxer** with core building blocks complete!

### ✅ Implemented (Phase 1 - COMPLETE)

- ✅ **3-iteration validation algorithm** - robust sync with garbage tolerance
- ✅ **Adaptive synchronization** - recovers from noise and interference
- ✅ **Payload extraction** - separates normal and private data
- ✅ **Multi-PID support** - handles multiple streams simultaneously
- ✅ **Continuity counter tracking** - detects packet loss
- ✅ **System PID filtering** - automatic PAT/CAT exclusion
- ✅ **Comprehensive test suite** - 7/7 basic tests passing
- ✅ **Synthetic packet generator** - for testing various scenarios

### ✅ Implemented (Phase 2 - COMPLETE)

- ✅ **PAT/PMT parsing** - full program table analysis with CRC-32 validation
- ✅ **PCR processing** - Program Clock Reference extraction, tracking, and interpolation
- ✅ **PES decoding** - Packetized Elementary Stream parsing with PTS/DTS timestamps
- ✅ **Enhanced test coverage** - 44/44 core tests passing (Basic: 7, PSI: 6, PCR: 13, PES: 18)
- ✅ **Stream type detection** - automatic video/audio stream identification
- ✅ **Multi-packet accumulation** - handles sections/packets spanning multiple TS packets

### 📋 Planned (Phase 3 - Demuxer Optimization)

- ⏳ **Performance optimizations** - SIMD, zero-copy operations
- ⏳ **Multi-threading support** - parallel stream processing
- ⏳ **DVB-specific functions** (optional) - service descriptors, EIT
- ⏳ **Real-time statistics** - bitrate, jitter, continuity errors
- ⏳ **Advanced error handling** - enhanced recovery strategies

### 🚀 In Progress (Phase 4 - MPEG-TS Muxer)

**Target: 16-week development cycle**
**Current Progress: Week 6/16 (37.5%)**
**Last Updated:** November 18, 2025

#### Phase 4.1: Foundation (Weeks 1-4) - ✅ COMPLETE

##### ✅ Week 1: Core Infrastructure (COMPLETED)
- ✅ **Core infrastructure** - muxer types and skeleton
  - `mpegts_muxer_types.hpp` (172 lines): StreamConfig, MuxerConfig, enums
  - `mpegts_muxer.hpp` (322 lines): MPEGTSMuxer class skeleton with full API
  - **12/12 unit tests passing**
  - **Commit:** c27b5b8

##### ✅ Week 2: TS Packet Builder (COMPLETED)
- ✅ **TS Packet Builder** - construct valid 188-byte packets
  - `mpegts_ts_packet_builder.hpp/cpp` (450 lines total)
  - Continuity counter management (0-15 auto-increment with wrap)
  - Adaptation field with PCR, private data, flags
  - Multi-packet splitting, stuffing bytes, NULL packets
  - **27/27 unit tests passing** (target: 25+, achieved 108%)
  - **Commit:** 9df6ae7

##### ✅ Week 3: PSI Generator (COMPLETED)
- ✅ **PSI Generator** - create PAT/PMT tables with CRC-32
  - `mpegts_psi_generator.hpp/cpp` (217 lines)
  - PAT/PMT generation with CRC-32 validation
  - Section wrapping, version numbering
  - Multi-stream PMT support
  - **27/27 unit tests passing** (target: 30+, achieved 90%)

##### ✅ Week 4: Basic Stream Management (COMPLETED)
- ✅ **Basic stream management** - stream registration and control
  - Stream registration, PID management
  - Configuration methods (bitrate, intervals)
  - Private data management integration
  - **23/23 unit tests passing**

#### Phase 4.2: Multi-Stream & PES (Weeks 5-8) - IN PROGRESS

##### ✅ Week 5: PES Packetizer (COMPLETED)
- ✅ **PES Packetizer** - elementary stream → PES packets
  - `mpegts_pes_packetizer.hpp/cpp` (152 lines)
  - PTS/DTS encoding (33-bit, 90kHz)
  - Stream ID management per stream type
  - Unbounded packets for video (packet_length = 0)
  - Data alignment indicator support
  - **21/21 unit tests passing**

##### ✅ Week 6: PCR Injection (COMPLETED)
- ✅ **PCR Injector** - Program Clock Reference generation
  - `mpegts_pcr_injector.hpp/cpp` (141 lines)
  - PCR calculation from timestamps (27 MHz)
  - Configurable injection interval (default 40ms)
  - Timing accuracy tracking
  - Discontinuity handling
  - **24/24 unit tests passing**

##### ⏳ Week 7: Stream Scheduler (NEXT)
- ⏳ **Stream Scheduler** - priority-based multi-stream scheduling
  - Round-robin with priority
  - Buffer level tracking
  - PSI/PCR priority handling
  - Target: 25+ unit tests

##### ⏳ Week 8: Integration Testing
- ⏳ **Integration tests** - validate complete muxing pipeline
  - Multi-stream muxing validation
  - Demuxer round-trip testing
  - Target: 15+ integration tests

#### Phase 4.3: Bitrate Control (Weeks 9-12)
- ⏳ **BitrateController** - CBR/VBR modes
- ⏳ **NULL packet insertion** - maintain constant bitrate
- ⏳ **Stream synchronization** - A/V sync within ±40ms
- ⏳ **Performance optimization** - meet 50 Mbps target

#### Phase 4.4: Polish (Weeks 13-16)
- ⏳ **Additional codecs** - H.265, MP3, AC-3
- ⏳ **Examples & tools** - command-line muxer utility
- ⏳ **Comprehensive testing** - compliance & stress tests
- ⏳ **Documentation** - API reference and user guide

**See detailed plans in:**
- `ideas/muxer_design.md` - Architecture and design
- `ideas/muxer_roadmap.md` - Week-by-week development plan
- `ideas/todo.md` - Technical specifications and tasks

## 🏗️ Architecture

### Demuxer Architecture (Implemented)

```
┌─────────────────────────────────────────┐
│  MPEG-TS Stream Input (raw bytes)      │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│  Adaptive Buffer (18.8 KB circular)    │
│  - Accumulates packets                  │
│  - Handles garbage filtering            │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│  Synchronization Engine                 │
│  - 0x47 sync byte detection             │
│  - 3-iteration validation               │
│  - Continuity counter check             │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│  Stream Storage                         │
│  - PID-based organization               │
│  - Iteration tracking                   │
│  - Payload separation (normal/private)  │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│  User API                               │
│  - feedData()                           │
│  - getPayload()                         │
│  - getIterations()                      │
└─────────────────────────────────────────┘
```

### Muxer Architecture (Planned)

```
┌─────────────────────────────────────────┐
│  Elementary Streams (H.264, AAC, etc.)  │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│  PES Packetizer                         │
│  - PTS/DTS generation                   │
│  - Stream-specific headers              │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│  TS Packet Builder                      │
│  - PES → 188-byte packets               │
│  - Continuity counter                   │
│  - Adaptation field                     │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│  Multiplexer Core                       │
│  - Stream scheduling                    │
│  - PSI injection (PAT/PMT)              │
│  - PCR injection (40ms)                 │
│  - Bitrate control                      │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│  Output Buffer                          │
│  - CBR/VBR modes                        │
│  - NULL packet padding                  │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│  MPEG-TS Stream Output                  │
└─────────────────────────────────────────┘
```

## 🛠️ Technical Specifications

### Demuxer
| Parameter                 | Value                               |
| ------------------------- | ----------------------------------- |
| Language                  | C++17                               |
| Standard                  | ISO/IEC 13818-1 (MPEG-TS)           |
| Packet size               | 188 bytes (no 192-byte mode)        |
| Buffer                    | 100 packets (18.8 KB)               |
| Sync validation           | 3-iteration (trinary)               |
| Video codec support       | H.264, H.265 (any MPEG-TS types)    |
| Private data              | Full support                        |
| Scrambled content         | NOT supported                       |
| Profiles                  | Profile-agnostic                    |

### Muxer (Planned)
| Parameter                 | Value                               |
| ------------------------- | ----------------------------------- |
| Language                  | C++17                               |
| Standard                  | ISO/IEC 13818-1 (MPEG-TS)           |
| Packet size               | 188 bytes                           |
| Max bitrate               | 50 Mbps                             |
| Concurrent streams        | 8 streams                           |
| PCR interval              | 40ms (configurable)                 |
| PAT/PMT interval          | 100ms (configurable)                |
| Modes                     | CBR, VBR                            |
| Latency target            | < 100ms                             |
| Video codecs              | H.264, H.265                        |
| Audio codecs              | AAC, MP3, AC-3                      |

## 📦 Building the Project

### Requirements

- CMake 3.15+
- Compiler with C++17 support (GCC 7+, Clang 5+, MSVC 2017+)

### Build

```bash
# Clone the repository
git clone <repository-url>
cd minimal_mpegts_std_impl

# Create build directory
mkdir build && cd build

# Configure with options
cmake -DBUILD_EXAMPLES=ON -DBUILD_TESTS=ON ..

# Build
cmake --build .

# Run tests
./tests/test_demuxer_basic
./tests/test_demuxer_scenarios

# Or use CTest
ctest

# Run example
./bin/basic_example input.ts
```

## 📚 Usage

### Basic Example

```cpp
#include "mpegts_demuxer.hpp"

int main() {
    using namespace mpegts;
    MPEGTSDemuxer demuxer;

    // Feed data from file or stream
    uint8_t buffer[4096];
    size_t bytes_read = read_stream(buffer, sizeof(buffer));
    demuxer.feedData(buffer, bytes_read);

    // Check synchronization
    if (demuxer.isSynchronized()) {
        // Get discovered programs
        auto programs = demuxer.getPrograms();

        for (const auto& prog : programs) {
            std::cout << "Found " << prog.stream_pids.size() << " streams\n";

            for (uint16_t pid : prog.stream_pids) {
                // Get iterations for this PID
                auto iterations = demuxer.getIterationsSummary(pid);

                for (const auto& iter : iterations) {
                    // Extract payload
                    auto payload = demuxer.getPayload(pid, iter.iteration_id);

                    // Process payload data
                    process_data(payload.data, payload.length);

                    // Clean up when done
                    demuxer.clearIteration(pid, iter.iteration_id);
                }
            }
        }
    }

    return 0;
}
```

Detailed examples are available in the `examples/` directory.

## 📋 Roadmap

### Phase 1: Core Demuxing ✅ (COMPLETED)
- [x] Basic project structure
- [x] Adaptive buffer implementation
- [x] 3-iteration packet synchronization and validation
- [x] Stream storage with iteration tracking
- [x] Complete API (feedData, getPrograms, getPayload, etc.)
- [x] Comprehensive test framework
- [x] Synthetic packet generator

### Phase 2: Advanced Features ✅ (COMPLETED)
- [x] PAT/PMT parsing with CRC-32 validation
- [x] PCR processing (extraction, tracking, interpolation, jitter detection)
- [x] PES decoding (header parsing, PTS/DTS extraction, multi-packet accumulation)
- [x] Stream type detection (video/audio identification)
- [x] Comprehensive test coverage (44/44 tests passing)

### Phase 3: Optimization & Extensions ⏳ (PLANNED)
- [ ] Performance optimizations (SIMD, zero-copy)
- [ ] Multi-threading support
- [ ] Real-time statistics (bitrate, jitter, errors)
- [ ] Enhanced error handling
- [ ] DVB-specific functions (optional)
- [ ] Fuzz testing and hardening

## 🧪 Testing

The project includes a comprehensive test suite:

```bash
# Build with tests
cmake -DBUILD_TESTS=ON ..
cmake --build .

# Run demuxer test suites
./tests/test_demuxer_basic       # 7/7 basic functionality tests
./tests/test_psi_tables          # 6/6 PSI table parsing tests
./tests/test_pcr                 # 13/13 PCR processing tests
./tests/test_pes                 # 18/18 PES decoding tests
./tests/test_demuxer_scenarios   # Scenario tests with garbage
./tests/test_synchronization     # Sync algorithm edge cases

# Run muxer test suites
./tests/test_muxer_basic              # 12/12 core infrastructure tests
./tests/test_ts_packet_builder        # 27/27 packet builder tests
./tests/test_psi_generator            # 27/27 PSI generation tests
./tests/test_pes_packetizer           # 21/21 PES packetizer tests
./tests/test_pcr_injector             # 24/24 PCR injector tests
./tests/test_muxer_stream_management  # 23/23 stream management tests

# Or run all with CTest
ctest
```

**Test Coverage (178/178 core tests passing - 100%):**

**Demuxer Tests (44/44):**
- ✅ Core Demuxing (7 tests) - packet validation, synchronization, multi-PID, payload extraction
- ✅ PSI Tables (6 tests) - PAT/PMT parsing, CRC-32 validation, section accumulation
- ✅ PCR Processing (13 tests) - extraction, tracking, interpolation, jitter detection
- ✅ PES Decoding (18 tests) - header parsing, PTS/DTS extraction, packet accumulation

**Muxer Tests (134/134):**
- ✅ Core Infrastructure (12 tests) - muxer types, configuration, basic API
- ✅ TS Packet Builder (27 tests) - packet construction, continuity counter, adaptation field
- ✅ PSI Generator (27 tests) - PAT/PMT generation, CRC-32, section wrapping
- ✅ PES Packetizer (21 tests) - PES header, PTS/DTS encoding, stream ID management
- ✅ PCR Injector (24 tests) - PCR calculation, timing accuracy, interval control
- ✅ Stream Management (23 tests) - stream registration, configuration, private data

## 📄 Documentation

Full technical specification is available in [todo.md](todo.md).

Russian documentation: [README_RU.md](README_RU.md)

## 📜 License

See [LICENSE](LICENSE) file.

## 🤝 Contributing

The project is in early development stage. Contributions are welcome after core functionality is completed.

## ⚡ Status

- **Version:** 0.3.0-beta
- **Demuxer Status:** Phase 1 Complete ✅ | Phase 2 Complete ✅ | Phase 3 Planned ⏳
- **Muxer Status:** Phase 4.1 Complete ✅ | Phase 4.2 In Progress (Week 6/16 - 37.5%)
- **Test Coverage:** 178/178 core tests passing (100%)
- **Last updated:** November 18, 2025

### Recent Updates

- ✅ **Muxer Week 6 COMPLETE** - PCR Injector with 24/24 tests passing
- ✅ **Muxer Week 5 COMPLETE** - PES Packetizer with 21/21 tests passing
- ✅ **Muxer Week 4 COMPLETE** - Basic Stream Management with 23/23 tests passing
- ✅ **Muxer Week 3 COMPLETE** - PSI Generator (PAT/PMT) with 27/27 tests passing
- ✅ **Phase 4.1 Foundation COMPLETE** - All core muxer building blocks implemented
- ✅ **134 muxer tests passing** - comprehensive coverage of all components

---

**Note:** This is an educational project focused on clean implementation of the MPEG-TS standard. For production use, libavformat (FFmpeg) or similar mature libraries are recommended.
