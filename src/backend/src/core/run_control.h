#pragma once

#include <stdint.h>
#include <windows.h>

#include "debug_session.h"

class DebugLoop;
class Registers;
class Memory;
class BreakpointManager;
class Threads;

class RunControl {
public:
    RunControl(DebugSession& session,
               DebugLoop& debug_loop,
               Registers& registers,
               Memory& memory,
               BreakpointManager& breakpoints,
               Threads& threads);

    bool continue_execution();
    bool step_into();
    bool step_over();
    bool finish();

    const DEBUG_EVENT& last_stop_event() const;
    bool last_breakpoint_address(uint64_t& address) const;
    bool last_breakpoint_kind(dbgtype::BreakpointKind& kind) const;

private:
    bool run_until_stop(DEBUG_EVENT& out_event);

private:
    DebugSession& session_;
    DebugLoop& debug_loop_;
    Registers& registers_;
    Memory& memory_;
    BreakpointManager& breakpoints_;
    Threads& threads_;
    DEBUG_EVENT last_stop_event_{};
};


