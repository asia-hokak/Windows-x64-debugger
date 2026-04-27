#pragma once

#include <stdint.h>
#include <optional>
#include <vector>
#include <windows.h>

#include "debug_session.h"

class Threads {
public:
    explicit Threads(DebugSession& session);

    bool add_thread(DWORD tid, HANDLE thread_handle, uint64_t teb = 0);
    bool remove_thread(DWORD tid);

    std::optional<DWORD> get_current_thread_id() const;
    HANDLE get_thread_handle(DWORD tid) const;
    HANDLE get_current_thread_handle() const;

    bool refresh_callstack(DWORD tid);
    bool scan_thread_callstack(HANDLE thread_handle, std::vector<uint64_t>& out_callstack) const;

    std::vector<dbgtype::ThreadInfo> list_threads() const;

private:
    DebugSession& session_;
};


