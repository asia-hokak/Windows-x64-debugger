#pragma once

#include <stddef.h>
#include <stdint.h>
#include <optional>
#include <unordered_map>
#include <vector>

#include "dbg_types.h"

class DebugSession {
public:
    void clear()
    {
        if (process_handle && process_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(process_handle);
            process_handle = nullptr;
        }
        for (auto& thread_entry : threads) {
            auto& thread = thread_entry.second;
            if (thread.handle && thread.handle != INVALID_HANDLE_VALUE) {
                CloseHandle(thread.handle);
                thread.handle = nullptr;
            }
        }

        pid = 0;
        attached = false;
        state = dbgtype::DebugState::Inactive;
        target_arch = dbgtype::TargetArch::Unknown;
        is_wow64_target = false;
        current_tid.reset();
        threads.clear();
        modules.clear();
        breakpoints.clear();
        pending_thread_callstacks.clear();
    }

    bool is_active() const
    {
        return attached && process_handle != nullptr && process_handle != INVALID_HANDLE_VALUE;
    }

    bool is_running() const
    {
        return state == dbgtype::DebugState::Running;
    }

    bool is_paused() const
    {
        return state == dbgtype::DebugState::Paused;
    }

    bool has_current_thread() const
    {
        return current_tid.has_value();
    }

public:
    DWORD pid = 0;
    bool attached = false;
    dbgtype::DebugState state = dbgtype::DebugState::Inactive;
    dbgtype::TargetArch target_arch = dbgtype::TargetArch::Unknown;
    bool is_wow64_target = false;

    HANDLE process_handle = nullptr;
    std::optional<DWORD> current_tid;

    std::unordered_map<DWORD, dbgtype::ThreadInfo> threads;
    std::unordered_map<uint64_t, dbgtype::ModuleInfo> modules;
    std::unordered_map<uint64_t, dbgtype::Breakpoint> breakpoints;
    std::unordered_map<DWORD, std::vector<uint64_t>> pending_thread_callstacks;
};
