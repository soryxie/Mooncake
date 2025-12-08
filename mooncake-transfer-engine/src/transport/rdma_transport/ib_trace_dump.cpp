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

#include <cstdio>
#include <cstdlib>

namespace mooncake {
namespace {

constexpr char kIbTraceEnvVar[] = "MC_IB_TRACE_FILE";

void IbTraceLogConfigOnce() {
    static bool logged = false;
    if (logged) return;
    logged = true;

    const char *path = std::getenv(kIbTraceEnvVar);
    if (path && path[0]) {
        std::fprintf(stderr,
                     "MC_IB_TRACE: enabled (IB_TRACE_ENABLE=1). Ring buffer "
                     "capacity=%llu records. %s=\"%s\"\n",
                     static_cast<unsigned long long>(kIbTraceCapacity),
                     kIbTraceEnvVar, path);
    } else {
        std::fprintf(stderr,
                     "MC_IB_TRACE: compiled in (IB_TRACE_ENABLE=1) but %s not "
                     "set; no trace dump will be written.\n",
                     kIbTraceEnvVar);
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
