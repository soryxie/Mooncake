// Copyright 2024 KVCache.AI
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "transport/rdma_transport/ib_trace.h"

#ifdef IB_TRACE_ENABLE

#include <glog/logging.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <time.h>

namespace mooncake {
namespace {

struct IbTraceBuffer {
    IbTraceRecord records[kIbTraceCapacity];
    std::atomic<uint64_t> write_index;
};

struct IbTraceFileHeader {
    char magic[8];
    uint32_t version;
    uint32_t record_size;
    uint64_t count;
};

alignas(64) IbTraceBuffer g_ib_trace_buffer{};
constexpr uint32_t kIbTraceFileVersion = 1;

inline uint64_t IbTraceNowNs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ull +
           static_cast<uint64_t>(ts.tv_nsec);
}

inline uint64_t NextIndex() {
    return g_ib_trace_buffer.write_index.fetch_add(1,
                                                   std::memory_order_relaxed);
}

}  // namespace

void IbTraceLog(uint64_t wr_id, uint32_t size, uint16_t dev, uint16_t qp,
                uint8_t opcode, uint8_t is_send, uint8_t phase, uint8_t status,
                uint32_t extra) {
    const uint64_t idx = NextIndex();
    IbTraceRecord &rec =
        g_ib_trace_buffer.records[idx & (kIbTraceCapacity - 1)];

    rec.t_ns = IbTraceNowNs();
    rec.wr_id = wr_id;
    rec.size = size;
    rec.dev = dev;
    rec.qp = qp;
    rec.opcode = opcode;
    rec.is_send = is_send;
    rec.phase = phase;
    rec.status = status;
    rec.extra = extra;
}

const IbTraceRecord *IbTraceRecords() { return g_ib_trace_buffer.records; }

size_t IbTraceRecordCapacity() { return kIbTraceCapacity; }

uint64_t IbTraceRecordWriteIndex() {
    return g_ib_trace_buffer.write_index.load(std::memory_order_relaxed);
}

void IbTraceReset() { g_ib_trace_buffer.write_index.store(0); }

void IbTraceDumpToFile(const char *path) {
    if (!path || !path[0]) return;

    const uint64_t write_index =
        g_ib_trace_buffer.write_index.load(std::memory_order_relaxed);
    const uint64_t count =
        std::min<uint64_t>(write_index, static_cast<uint64_t>(kIbTraceCapacity));
    const uint64_t start = write_index - count;
    VLOG(1) << "MC_IB_TRACE: dumping " << count << " records to \"" << path << "\".";

    std::FILE *fp = std::fopen(path, "wb");
    if (!fp) {
        LOG(ERROR) << "MC_IB_TRACE: failed to open \"" << path
                   << "\" for writing: " << std::strerror(errno);
        return;
    }

    IbTraceFileHeader header{};
    std::memcpy(header.magic, "IBTRACE", 7);
    header.version = kIbTraceFileVersion;
    header.record_size = sizeof(IbTraceRecord);
    header.count = count;

    if (std::fwrite(&header, sizeof(header), 1, fp) != 1) {
        std::fclose(fp);
        return;
    }

    for (uint64_t i = 0; i < count; ++i) {
        const uint64_t idx = (start + i) & (kIbTraceCapacity - 1);
        const IbTraceRecord &rec = g_ib_trace_buffer.records[idx];
        if (std::fwrite(&rec, sizeof(IbTraceRecord), 1, fp) != 1) {
            break;
        }
    }

    std::fclose(fp);
    VLOG(1) << "MC_IB_TRACE: dump to \"" << path << "\" completed (header + "
            << count << " records).";
}

void IbTraceDumpFromEnv(const char *env_var) {
    if (!env_var || !env_var[0]) return;
    const char *path = std::getenv(env_var);
    if (!path || !path[0]) return;
    IbTraceDumpToFile(path);
}

namespace {

constexpr char kIbTraceEnvVar[] = "MC_IB_TRACE_FILE";

void IbTraceLogConfigOnce() {
    static bool logged = false;
    if (logged) return;
    logged = true;

    const char *path = std::getenv(kIbTraceEnvVar);
    if (path && path[0]) {
        VLOG(1) << "MC_IB_TRACE: enabled (IB_TRACE_ENABLE=1). Ring buffer "
                   "capacity=" << kIbTraceCapacity << " records. "
                << kIbTraceEnvVar << "=\"" << path << "\"";
    } else {
        VLOG(1) << "MC_IB_TRACE: compiled in (IB_TRACE_ENABLE=1) but "
                << kIbTraceEnvVar << " not set; no trace dump will be written.";
    }
}

struct IbTraceAutoDumper {
    IbTraceAutoDumper() { IbTraceLogConfigOnce(); }
    ~IbTraceAutoDumper() { IbTraceDumpFromEnv(kIbTraceEnvVar); }
};

// Dump traces (if requested) when the process exits or the shared
// object unloads.
const IbTraceAutoDumper kIbTraceAutoDumper{};

}  // namespace

}  // namespace mooncake

#endif  // IB_TRACE_ENABLE
