#pragma once

#include <stdint.h>
#include <windows.h>

#include "debug_session.h"

class Registers {
public:
    explicit Registers(DebugSession& session);

    bool read_current_thread_registers(dbgtype::RegisterState& out) const;
    bool get_thread_ip(HANDLE thread, uint64_t& ip) const;
    bool set_thread_ip(HANDLE thread, uint64_t ip);
    bool set_trap_flag(DWORD tid, bool enable_single_step) const;

private:
    DebugSession& session_;
};


