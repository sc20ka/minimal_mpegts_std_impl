# Minimal MPEG-TS Standard Implementation

## 📖 Project Description

A minimalistic implementation of an MPEG-TS (ISO/IEC 13818-1) demultiplexer in C++17, created for educational purposes and practical application in transport stream processing tasks.

## 🎯 Purpose

This project provides a clean, profile-agnostic implementation of an MPEG-TS demultiplexer focused on:

- **Stream processing** of MPEG-TS data with high reliability
- **Adaptive synchronization** in conditions with noise and garbage data
- **Recovery** of valid packets from arbitrary data
- **Separation** of main payload and private data
- **Management** of multiple programs and streams simultaneously

## ⚠️ Current Development Stage

**ALPHA v0.1.0 - Core Demuxing Complete**

The project has completed **Phase 1: Core Demuxing** with full functionality:

### ✅ Implemented (Phase 1 - COMPLETE)

- ✅ **3-iteration validation algorithm** - robust sync with garbage tolerance
- ✅ **Adaptive synchronization** - recovers from noise and interference
- ✅ **Payload extraction** - separates normal and private data
- ✅ **Multi-PID support** - handles multiple streams simultaneously
- ✅ **Continuity counter tracking** - detects packet loss
- ✅ **System PID filtering** - automatic PAT/CAT exclusion
- ✅ **Comprehensive test suite** - 7/7 basic tests passing
- ✅ **Synthetic packet generator** - for testing various scenarios

### 🚧 In Progress (Phase 2)

- 🔨 PAT/PMT parsing - program table analysis
- 🔨 Advanced scenario handling - edge cases refinement

### 📋 Planned (Phase 2-3)

- ⏳ PCR processing - clock reference handling
- ⏳ PES decoding - elementary stream packets
- ⏳ Performance optimizations - SIMD, zero-copy
- ⏳ Multi-threading support
- ⏳ DVB-specific functions (optional)

## 🏗️ Architecture

### Key Components

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

## 🛠️ Technical Specifications

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

### Phase 2: Advanced Features 🚧 (IN PROGRESS)
- [ ] PAT/PMT parsing
- [ ] PCR processing
- [ ] PES decoding
- [ ] Enhanced error handling
- [ ] Real-time statistics

### Phase 3: Optimization & Extensions ⏳ (PLANNED)
- [ ] Performance optimizations (SIMD, zero-copy)
- [ ] Multi-threading support
- [ ] DVB-specific functions (optional)
- [ ] Fuzz testing and hardening

## 🧪 Testing

The project includes a comprehensive test suite:

```bash
# Build with tests
cmake -DBUILD_TESTS=ON ..
cmake --build .

# Run test suites
./tests/test_demuxer_basic       # 7/7 basic functionality tests
./tests/test_demuxer_scenarios   # Scenario tests with garbage
./tests/test_synchronization     # Sync algorithm edge cases
```

**Test Coverage:**
- ✅ Single packet validation
- ✅ Clean stream synchronization
- ✅ Multiple PID handling
- ✅ Payload extraction (normal + private)
- ✅ Continuity counter tracking
- ✅ System PID filtering
- ✅ Synthetic packet generation with controlled garbage

## 📄 Documentation

Full technical specification is available in [todo.md](todo.md).

Russian documentation: [README_RU.md](README_RU.md)

## 📜 License

See [LICENSE](LICENSE) file.

## 🤝 Contributing

The project is in early development stage. Contributions are welcome after core functionality is completed.

## ⚡ Status

- **Version:** 0.1.0-alpha
- **Status:** Phase 1 Complete ✅ | Phase 2 In Progress 🚧
- **Test Coverage:** 7/7 basic tests passing
- **Last updated:** November 2025

### Recent Updates

- ✅ **Core synchronization complete** - 3-iteration validation working
- ✅ **Test framework added** - comprehensive testing infrastructure
- ✅ **Payload extraction complete** - normal + private data support
- 🔨 **Working on:** PAT/PMT parsing and advanced scenarios

---

**Note:** This is an educational project focused on clean implementation of the MPEG-TS standard. For production use, libavformat (FFmpeg) or similar mature libraries are recommended.
