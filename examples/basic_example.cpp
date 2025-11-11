/**
 * @file basic_example.cpp
 * @brief Basic usage example of MPEG-TS demuxer
 *
 * This example demonstrates how to:
 * - Create a demuxer instance
 * - Feed data from a file
 * - Check synchronization status
 * - Retrieve discovered streams
 * - Access payload data
 */

#include "mpegts_demuxer.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>

using namespace mpegts;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input.ts>\n";
        return 1;
    }

    const char* filename = argv[1];

    // Open MPEG-TS file
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot open file " << filename << "\n";
        return 1;
    }

    // Create demuxer
    MPEGTSDemuxer demuxer;

    // Feed data in chunks
    const size_t CHUNK_SIZE = 4096;
    uint8_t buffer[CHUNK_SIZE];

    std::cout << "Processing file: " << filename << "\n";
    std::cout << "----------------------------------------\n";

    size_t total_bytes = 0;

    while (file.read(reinterpret_cast<char*>(buffer), CHUNK_SIZE) || file.gcount() > 0) {
        size_t bytes_read = file.gcount();
        total_bytes += bytes_read;

        demuxer.feedData(buffer, bytes_read);

        // Check synchronization status
        if (demuxer.isSynchronized()) {
            std::cout << "\r✓ Synchronized | ";
            std::cout << "Buffer: " << demuxer.getPacketCount() << " packets | ";
            std::cout << "Bytes: " << total_bytes;
            std::cout.flush();
        }
    }

    std::cout << "\n----------------------------------------\n";

    // Get discovered programs/streams
    auto programs = demuxer.getPrograms();

    std::cout << "\nDiscovered " << programs.size() << " stream(s):\n";
    std::cout << "----------------------------------------\n";

    for (const auto& prog : programs) {
        std::cout << "\nStream PIDs: ";
        for (size_t i = 0; i < prog.stream_pids.size(); ++i) {
            std::cout << "0x" << std::hex << std::setw(4) << std::setfill('0')
                      << prog.stream_pids[i];
            if (i + 1 < prog.stream_pids.size()) {
                std::cout << ", ";
            }
        }
        std::cout << std::dec << "\n";

        std::cout << "  Total payload: " << prog.total_payload_size << " bytes\n";
        std::cout << "  Iterations: " << prog.iteration_count << "\n";
        std::cout << "  Discontinuities: " << (prog.has_discontinuity ? "YES" : "NO") << "\n";

        // Get detailed iteration info for each PID
        for (uint16_t pid : prog.stream_pids) {
            auto iterations = demuxer.getIterationsSummary(pid);

            std::cout << "\n  PID 0x" << std::hex << std::setw(4) << std::setfill('0')
                      << pid << std::dec << " iterations:\n";

            for (const auto& iter : iterations) {
                std::cout << "    Iteration #" << iter.iteration_id << ":\n";
                std::cout << "      Normal payload: " << iter.payload_normal_size << " bytes\n";
                std::cout << "      Private payload: " << iter.payload_private_size << " bytes\n";
                std::cout << "      CC: " << int(iter.cc_start) << " -> " << int(iter.cc_end) << "\n";
                std::cout << "      Packets: " << iter.packet_count << "\n";

                if (iter.has_discontinuity) {
                    std::cout << "      ⚠️  DISCONTINUITY DETECTED\n";
                }
            }
        }
    }

    std::cout << "\n----------------------------------------\n";
    std::cout << "Processing complete!\n";

    return 0;
}
