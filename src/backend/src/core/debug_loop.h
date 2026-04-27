#pragma once

#include <stdint.h>
#include <windows.h>

#include "debug_session.h"

class Threads;
class Modules;
class BreakpointManager;

class DebugLoop {
public:
    explicit DebugLoop(DebugSession& session);

    void set_threads(Threads* threads) { threads_ = threads; }
    void set_modules(Modules* modules) { modules_ = modules; }
    void set_breakpoints(BreakpointManager* breakpoints) { breakpoints_ = breakpoints; }

    bool wait_for_event(DEBUG_EVENT& out_event, DWORD timeout_ms);
    bool continue_last_event(DWORD continue_status = DBG_CONTINUE);

    const DEBUG_EVENT& last_event() const { return last_event_; }
    bool last_breakpoint_address(uint64_t& address) const;
    bool last_breakpoint_kind(dbgtype::BreakpointKind& kind) const;

private:
    bool handle_create_process(const DEBUG_EVENT& raw);
    bool handle_create_thread(const DEBUG_EVENT& raw);
    bool handle_exit_thread(const DEBUG_EVENT& raw);
    bool handle_load_dll(const DEBUG_EVENT& raw);
    bool handle_unload_dll(const DEBUG_EVENT& raw);
    bool handle_exception(const DEBUG_EVENT& raw);

private:
    DebugSession& session_;
    Threads* threads_ = nullptr;
    Modules* modules_ = nullptr;
    BreakpointManager* breakpoints_ = nullptr;

    DWORD last_event_pid_ = 0;
    DWORD last_event_tid_ = 0;
    bool has_last_event_ = false;
    DEBUG_EVENT last_event_{};
    bool has_last_breakpoint_address_ = false;
    uint64_t last_breakpoint_address_ = 0;
    bool has_last_breakpoint_kind_ = false;
    dbgtype::BreakpointKind last_breakpoint_kind_ = dbgtype::BreakpointKind::Software;
};

