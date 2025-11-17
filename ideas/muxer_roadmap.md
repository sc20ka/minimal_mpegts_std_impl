# MPEG-TS Muxer Development Roadmap

## Project Timeline: 12-16 weeks

---

## Phase 1: Foundation (Weeks 1-4)

### Week 1: Core Infrastructure
**Goal:** Set up basic muxer structure and types

#### Tasks:
- [ ] Create `mpegts_muxer.hpp` header file
- [ ] Define `MPEGTSMuxer` class skeleton
- [ ] Create muxer-specific types in `mpegts_muxer_types.hpp`:
  - `StreamConfig`
  - `MuxerConfig`
  - `MuxMode` enum (CBR/VBR)
  - `OutputCallback` type
- [ ] Set up CMake build integration
- [ ] Create test framework for muxer

**Deliverables:**
- Compilable muxer skeleton
- Basic types defined
- Test infrastructure ready

---

### Week 2: TS Packet Builder
**Goal:** Implement TS packet construction

#### Tasks:
- [ ] Create `TSPacketBuilder` class
- [ ] Implement basic 188-byte packet assembly
- [ ] Add continuity counter management
- [ ] Implement adaptation field insertion
- [ ] Add stuffing byte handling
- [ ] **Add private data support in adaptation field**
- [ ] **Implement transport_private_data_flag handling**
- [ ] **Validate max private data size (182 bytes)**
- [ ] Write unit tests for packet builder

**Test Coverage:**
- Packet structure validation
- Continuity counter increment
- Adaptation field with PCR
- **Adaptation field with private data**
- **Private data + PCR combined**
- PUSI flag handling
- Maximum payload size

**Deliverables:**
- Working `TSPacketBuilder`
- 10+ unit tests passing
- Documentation

---

### Week 3: PSI Generator
**Goal:** Generate valid PAT and PMT tables

#### Tasks:
- [ ] Create `PSIGenerator` class
- [ ] Implement PAT generation
- [ ] Implement PMT generation
- [ ] Add CRC-32 calculation (reuse from demuxer)
- [ ] Implement section wrapping
- [ ] Handle version numbering
- [ ] Write unit tests

**Test Coverage:**
- PAT structure and CRC validation
- PMT structure and CRC validation
- Multiple streams in PMT
- Version number management
- Section length validation

**Deliverables:**
- Working PSI generator
- Valid PAT/PMT output
- 15+ unit tests passing

---

### Week 4: Basic Stream Management
**Goal:** Handle single elementary stream

#### Tasks:
- [ ] Implement `addStream()` method
- [ ] Create stream registration system
- [ ] Implement basic PID management
- [ ] Add stream metadata storage
- [ ] Create simple buffer management
- [ ] **Create `PrivateDataManager` class skeleton**
- [ ] **Implement basic private data queue management**
- [ ] **Add private data configuration to StreamConfig**
- [ ] Write integration tests

**Test Coverage:**
- Add/remove streams
- PID uniqueness validation
- Stream type validation
- Basic data flow
- **Simple private data insertion**
- **Private data queue management**

**Deliverables:**
- Single stream muxing works
- Stream lifecycle management
- 10+ tests passing

---

## Phase 2: PES and Multi-Stream (Weeks 5-8)

### Week 5: PES Packetizer
**Goal:** Convert elementary streams to PES

#### Tasks:
- [ ] Create `PESPacketizer` class
- [ ] Implement PES header generation
- [ ] Add PTS encoding (33-bit timestamp)
- [ ] Add DTS encoding (optional)
- [ ] Handle stream IDs per stream type
- [ ] Implement packet size management
- [ ] Write unit tests

**Test Coverage:**
- PES header structure
- PTS/DTS encoding accuracy
- Stream ID correctness
- Packet size limits
- Data alignment

**Deliverables:**
- Working PES packetizer
- PTS/DTS support
- 12+ unit tests passing

---

### Week 6: PCR Injection
**Goal:** Implement Program Clock Reference

#### Tasks:
- [ ] Create `PCRInjector` class
- [ ] Implement PCR value calculation (27MHz base)
- [ ] Add timing control (default 40ms interval)
- [ ] Create adaptation field with PCR
- [ ] Integrate with packet builder
- [ ] Write timing accuracy tests

**Test Coverage:**
- PCR calculation accuracy
- 40ms interval timing
- PCR discontinuity detection
- Adaptation field format

**Deliverables:**
- Working PCR injection
- < 100ns timing accuracy
- 8+ unit tests passing

---

### Week 7: Stream Scheduler
**Goal:** Manage multiple concurrent streams

#### Tasks:
- [ ] Create `StreamScheduler` class
- [ ] Implement priority-based scheduling
- [ ] Add buffer level tracking
- [ ] Implement round-robin with priority
- [ ] Add PSI/PCR priority handling
- [ ] Write scheduling tests

**Test Coverage:**
- Fair scheduling among equal priority
- Priority enforcement
- Buffer overflow prevention
- PSI insertion timing

**Deliverables:**
- Working multi-stream scheduler
- Fair stream interleaving
- 15+ unit tests passing

---

### Week 8: Integration Testing
**Goal:** Validate multi-stream muxing

#### Tasks:
- [ ] Create end-to-end test scenarios
- [ ] Test 2-stream muxing (video + audio)
- [ ] Test 4-stream muxing
- [ ] Validate output with demuxer
- [ ] Test PSI periodicity
- [ ] Test PCR accuracy
- [ ] Performance profiling

**Test Scenarios:**
- H.264 + AAC muxing
- Multiple audio tracks
- Varying bitrates
- Stream addition/removal during mux

**Deliverables:**
- Multi-stream muxing working
- Demuxer can parse output
- Performance baseline

---

## Phase 3: Bitrate Control and Advanced Features (Weeks 9-12)

### Week 9: Bitrate Controller
**Goal:** Implement CBR and VBR modes

#### Tasks:
- [ ] Create `BitrateController` class
- [ ] Implement CBR mode with NULL padding
- [ ] Implement VBR mode
- [ ] Add packet timing calculation
- [ ] Implement NULL packet insertion
- [ ] Write bitrate accuracy tests

**Test Coverage:**
- CBR bitrate accuracy (±1%)
- NULL packet insertion
- VBR burst handling
- Long-term bitrate average

**Deliverables:**
- Working CBR/VBR modes
- Bitrate accuracy within spec
- 10+ unit tests passing

---

### Week 10: Stream Synchronization & Advanced Private Data
**Goal:** Ensure proper A/V sync and complete private data support

#### Tasks:
- [ ] Implement PTS alignment checking
- [ ] Add buffer management for sync
- [ ] Handle variable frame rates
- [ ] Add latency compensation
- [ ] **Complete PrivateDataManager implementation**
- [ ] **Add PTS-synchronized private data insertion**
- [ ] **Implement insertion strategies (WITH_PCR, STANDALONE, WITH_PAYLOAD)**
- [ ] **Add dedicated private data stream support (stream_type = 0x06)**
- [ ] **Implement private data overflow handling**
- [ ] Write A/V sync tests
- [ ] **Write comprehensive private data tests**

**Test Coverage:**
- Audio-video sync accuracy
- Variable bitrate handling
- Buffering strategies
- Drift correction
- **Private data synchronization with PTS**
- **Private data insertion modes**
- **Dedicated private stream muxing**
- **Cross-validation with demuxer private data extraction**

**Deliverables:**
- A/V sync within ±40ms
- Stable long-duration muxing
- **Full private data support**
- **Demuxer can extract all private data correctly**
- 15+ unit tests passing (including 7+ private data tests)

---

### Week 11: Error Handling and Recovery
**Goal:** Robust error management

#### Tasks:
- [ ] Add input validation
- [ ] Implement buffer overflow handling
- [ ] Add underflow recovery
- [ ] Implement error callbacks
- [ ] Add logging system
- [ ] Write error scenario tests

**Test Coverage:**
- Buffer overflow recovery
- Invalid input handling
- Timestamp discontinuity
- Stream failure recovery

**Deliverables:**
- Robust error handling
- No crashes on bad input
- Clear error reporting

---

### Week 12: Performance Optimization
**Goal:** Meet performance targets

#### Tasks:
- [ ] Profile critical paths
- [ ] Optimize packet assembly
- [ ] Reduce memory allocations
- [ ] Optimize buffer management
- [ ] Add memory pooling
- [ ] Benchmark against targets

**Performance Targets:**
- 50 Mbps sustained output
- < 30% CPU usage
- < 50 MB memory
- < 100ms latency

**Deliverables:**
- Optimized implementation
- Performance targets met
- Benchmark report

---

## Phase 4: Polish and Documentation (Weeks 13-16)

### Week 13: Additional Stream Types
**Goal:** Support more codecs

#### Tasks:
- [ ] Add H.265/HEVC support
- [ ] Add MP3 audio support
- [ ] Add AC-3 audio support (optional)
- [ ] Add private data streams
- [ ] Update PSI generator for new types
- [ ] Write codec-specific tests

**Deliverables:**
- Support for 5+ stream types
- Codec-specific tests passing

---

### Week 14: Examples and Tools
**Goal:** Provide usage examples

#### Tasks:
- [ ] Create basic muxing example
- [ ] Create multi-stream example
- [ ] Create file muxer tool
- [ ] Create streaming tool (optional)
- [ ] Add example media files
- [ ] Write example documentation

**Examples:**
- `examples/basic_muxer.cpp` - Simple file mux
- `examples/live_muxer.cpp` - Simulated live muxing
- `examples/remux.cpp` - Demux and remux
- `tools/ts_muxer` - Command-line tool

**Deliverables:**
- 4+ working examples
- Command-line tool
- Example documentation

---

### Week 15: Comprehensive Testing
**Goal:** Ensure reliability

#### Tasks:
- [ ] Compliance testing (ISO/IEC 13818-1)
- [ ] Cross-validation with FFmpeg
- [ ] Stress testing (24+ hour runs)
- [ ] Edge case testing
- [ ] Memory leak detection
- [ ] Thread safety validation (future)

**Test Scenarios:**
- Long-duration muxing (24h+)
- High bitrate (50 Mbps+)
- Stream switching
- Error injection
- Fuzzing

**Deliverables:**
- Comprehensive test suite
- Compliance validation
- Stability report

---

### Week 16: Documentation and Release
**Goal:** Prepare for release

#### Tasks:
- [ ] Complete API documentation
- [ ] Write user guide
- [ ] Create architecture diagrams
- [ ] Write performance guide
- [ ] Update main README
- [ ] Create CHANGELOG
- [ ] Prepare release notes

**Documentation:**
- API reference (Doxygen)
- User guide with examples
- Architecture documentation
- Performance tuning guide
- Migration guide (if needed)

**Deliverables:**
- Complete documentation
- Release-ready code
- Version 1.0 tag

---

## Testing Strategy

### Unit Tests (Target: 165+ tests)
- **TSPacketBuilder**: 30 tests (includes 5 private data tests)
- **PESPacketizer**: 20 tests
- **PSIGenerator**: 30 tests
- **PCRInjector**: 15 tests
- **StreamScheduler**: 25 tests
- **BitrateController**: 20 tests
- **PrivateDataManager**: 10 tests
- **MPEGTSMuxer**: 15 tests

### Integration Tests (Target: 60+ tests)
- Single stream scenarios: 10 tests
- Multi-stream scenarios: 15 tests
- PSI/PCR timing: 10 tests
- Bitrate conformance: 10 tests
- **Private data scenarios**: 10 tests
  - Private data in adaptation field
  - PTS-synchronized private data
  - Dedicated private stream
  - Cross-validation with demuxer
- Error scenarios: 5 tests

### Compliance Tests (Target: 20+ tests)
- ISO/IEC 13818-1 validation
- PAT/PMT structure
- PCR accuracy
- PTS/DTS monotonicity
- Bitrate conformance

### Performance Tests (Target: 10+ tests)
- Throughput benchmarks
- Latency measurements
- CPU usage profiling
- Memory usage tracking
- Long-duration stability

---

## Milestones

### M1: Basic Muxing (End of Week 4)
- Single stream muxing works
- Valid TS packets generated
- Basic PSI tables

### M2: Multi-Stream Support (End of Week 8)
- Multiple streams muxing
- PCR injection working
- Stream scheduling functional

### M3: Production Ready (End of Week 12)
- CBR/VBR support
- Error handling complete
- Performance targets met

### M4: Release (End of Week 16)
- Full documentation
- Comprehensive testing
- Examples and tools
- Version 1.0 release

---

## Risk Management

### Technical Risks:

**Risk:** PCR timing accuracy
- **Mitigation:** Early implementation, extensive testing
- **Fallback:** Use high-resolution timers, OS-specific code

**Risk:** Performance bottlenecks
- **Mitigation:** Early profiling, incremental optimization
- **Fallback:** Reduce feature scope, focus on core

**Risk:** Complex scheduling bugs
- **Mitigation:** Comprehensive unit tests, formal verification
- **Fallback:** Simplify scheduler, use simpler algorithms

### Schedule Risks:

**Risk:** Implementation delays
- **Mitigation:** Buffer time in schedule, prioritize features
- **Fallback:** Delay non-critical features to phase 5

**Risk:** Testing takes longer
- **Mitigation:** Test-driven development, continuous testing
- **Fallback:** Focus on critical tests, defer edge cases

---

## Success Criteria

### Functional:
- ✅ Mux 2+ streams simultaneously
- ✅ Generate valid PAT/PMT
- ✅ Inject PCR accurately
- ✅ Support H.264 and AAC at minimum
- ✅ CBR mode functional
- ✅ Output parseable by standard demuxers

### Performance:
- ✅ Support 50 Mbps output
- ✅ < 100ms latency
- ✅ < 30% CPU on reference hardware
- ✅ Stable for 24+ hour runs

### Quality:
- ✅ 200+ tests passing
- ✅ Zero known crashes
- ✅ ISO/IEC 13818-1 compliant
- ✅ Complete documentation

---

## Future Enhancements (Post 1.0)

### Phase 5: Advanced Features
- Multi-program support
- Service Information (SI) tables
- Teletext and subtitles
- Data carousel
- Conditional access hooks

### Phase 6: Optimization
- SIMD packet assembly
- Multi-threaded muxing
- GPU acceleration hooks
- Zero-copy operations

### Phase 7: Integration
- FFmpeg integration
- GStreamer plugin
- Network streaming (RTP/UDP)
- HLS/DASH packaging

---

## Resources Required

### Development:
- 1 senior developer (full-time)
- Code review support
- Testing infrastructure

### Testing:
- Reference TS streams
- Compliance tools
- Performance testing hardware

### Documentation:
- Technical writer (part-time)
- Diagram tools
- Documentation generator

---

## Conclusion

This roadmap provides a structured 16-week plan to develop a fully-functional MPEG-TS muxer. The phased approach ensures:

1. **Early validation** - Basic muxing working by week 4
2. **Incremental complexity** - Features added systematically
3. **Continuous testing** - Quality maintained throughout
4. **Clear milestones** - Progress easily tracked

**Estimated completion:** 16 weeks for version 1.0
**Recommended start:** After demuxer issues are resolved
