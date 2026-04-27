#pragma once

#include <stdint.h>
#include <windows.h>

#include "debug_session.h"

class Memory;
class Registers;
class Threads;

class BreakpointManager {
public:
    BreakpointManager(DebugSession& session, Memory& memory, Registers& registers, Threads& threads);

    bool set_int3_breakpoint(uint64_t address, dbgtype::BreakpointKind kind);
    bool remove_breakpoint(uint64_t address);

    bool handle_breakpoint_hit(DWORD tid,
                               uint64_t* hit_address = nullptr,
                               dbgtype::BreakpointKind* hit_kind = nullptr);

private:
    DebugSession& session_;
    Memory& memory_;
    Registers& registers_;
    Threads& threads_;
};


