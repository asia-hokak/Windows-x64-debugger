#pragma once

#include <vector>

#include "debug_session.h"

class MemoryRegionManager {
public:
    explicit MemoryRegionManager(DebugSession& session);

    std::vector<dbgtype::MemoryRegion> query_regions() const;

private:
    DebugSession& session_;
};
