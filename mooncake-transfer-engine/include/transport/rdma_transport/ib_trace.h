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

#ifndef MOONCAKE_TRANSPORT_RDMA_TRANSPORT_IB_TRACE_H_
#define MOONCAKE_TRANSPORT_RDMA_TRANSPORT_IB_TRACE_H_

#include <cstddef>
#include <cstdint>

namespace mooncake {

// 1M records -> 32 MB buffer.
constexpr size_t kIbTraceCapacity = 1ull << 20;

struct IbTraceRecord {
    uint64_t t_ns;
    uint64_t wr_id;
    uint32_t size;
    uint16_t dev;
    uint16_t qp;
    uint8_t opcode;
    uint8_t is_send;
    uint8_t phase;
    uint8_t status;
    uint32_t extra;
};

#ifdef IB_TRACE_ENABLE

void IbTraceLog(uint64_t wr_id, uint32_t size, uint16_t dev, uint16_t qp,
                uint8_t opcode, uint8_t is_send, uint8_t phase, uint8_t status,
                uint32_t extra);

const IbTraceRecord *IbTraceRecords();
size_t IbTraceRecordCapacity();
uint64_t IbTraceRecordWriteIndex();
void IbTraceReset();
void IbTraceDumpToFile(const char *path);
void IbTraceDumpFromEnv(const char *env_var);

#define IB_TRACE_POST_SEND(wr_id, size, dev, qp, opcode, extra) \
    ::mooncake::IbTraceLog((wr_id), (uint32_t)(size), (uint16_t)(dev),         \
                           (uint16_t)(qp), (uint8_t)(opcode), 1, 0, 0xff,      \
                           (uint32_t)(extra))

#define IB_TRACE_POST_RECV(wr_id, size, dev, qp, opcode, extra) \
    ::mooncake::IbTraceLog((wr_id), (uint32_t)(size), (uint16_t)(dev),         \
                           (uint16_t)(qp), (uint8_t)(opcode), 0, 0, 0xff,      \
                           (uint32_t)(extra))

#define IB_TRACE_COMPLETE(wr_id, size, dev, qp, opcode, status, is_send, extra) \
    ::mooncake::IbTraceLog((wr_id), (uint32_t)(size), (uint16_t)(dev),          \
                           (uint16_t)(qp), (uint8_t)(opcode),                   \
                           (uint8_t)(is_send), 1, (uint8_t)(status),            \
                           (uint32_t)(extra))

#else

inline void IbTraceLog(uint64_t, uint32_t, uint16_t, uint16_t, uint8_t,
                       uint8_t, uint8_t, uint8_t, uint32_t) {}

inline const IbTraceRecord *IbTraceRecords() { return nullptr; }
inline size_t IbTraceRecordCapacity() { return 0; }
inline uint64_t IbTraceRecordWriteIndex() { return 0; }
inline void IbTraceReset() {}
inline void IbTraceDumpToFile(const char *) {}
inline void IbTraceDumpFromEnv(const char *) {}

#define IB_TRACE_POST_SEND(...)
#define IB_TRACE_POST_RECV(...)
#define IB_TRACE_COMPLETE(...)

#endif  // IB_TRACE_ENABLE

}  // namespace mooncake

#endif  // MOONCAKE_TRANSPORT_RDMA_TRANSPORT_IB_TRACE_H_
