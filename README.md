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

**ALPHA v0.1.0 - Initial Demuxing Stage**

Currently, the project is in the **initial development stage** and implements **exclusively demuxing functionality**:

- ✅ Basic MPEG-TS packet synchronization
- ✅ 3-iteration stream validation
- ✅ Payload data extraction (normal + private)
- ✅ Continuity counter processing
- ✅ Adaptive recovery after interference

### Not Implemented (Yet)

- ❌ PAT/PMT parsing (working without program tables)
- ❌ PCR-based synchronization
- ❌ PES decoding
- ❌ DVB/ATSC specific functions
- ❌ Descrambling
- ❌ Advanced metadata processing

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

# Configure
cmake ..

# Build
cmake --build .

# Optional: build with examples
cmake -DBUILD_EXAMPLES=ON ..
cmake --build .
```

## 📚 Usage

```cpp
#include "mpegts_demuxer.hpp"

int main() {
    MPEGTSDemuxer demuxer;

    // Feed data
    uint8_t buffer[4096];
    size_t bytes_read = read_stream(buffer, sizeof(buffer));
    demuxer.feedData(buffer, bytes_read);

    // Check synchronization
    if (demuxer.isSynchronized()) {
        auto programs = demuxer.getPrograms();
        // Process discovered programs
    }

    return 0;
}
```

Detailed examples are available in the `examples/` directory.

## 📋 Roadmap

### Phase 1: Core Demuxing (current phase)
- [x] Basic project structure
- [ ] Adaptive buffer implementation
- [ ] Packet synchronization and validation
- [ ] Stream storage
- [ ] Basic API

### Phase 2: Advanced Features
- [ ] PAT/PMT parsing
- [ ] PCR processing
- [ ] PES decoding
- [ ] Enhanced error handling

### Phase 3: Optimization & Extensions
- [ ] Performance optimizations
- [ ] Multi-threading support
- [ ] DVB-specific functions (optional)

## 📄 Documentation

Full technical specification is available in [todo.md](todo.md).

Russian documentation: [README_RU.md](README_RU.md)

## 📜 License

See [LICENSE](LICENSE) file.

## 🤝 Contributing

The project is in early development stage. Contributions are welcome after core functionality is completed.

## ⚡ Status

- **Version:** 0.1.0-alpha
- **Status:** In Development (Core Demuxing)
- **Last updated:** November 2025

---

**Note:** This is an educational project focused on clean implementation of the MPEG-TS standard. For production use, libavformat (FFmpeg) or similar mature libraries are recommended.
