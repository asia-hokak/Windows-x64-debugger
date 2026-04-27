#pragma once

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <string>
#include <vector>
#include <windows.h>

#include "debug_session.h"

class ProcessControl;
class DebugLoop;
class Registers;
class Threads;
class Memory;
class MemoryRegionManager;
class Modules;
class BreakpointManager;
class RunControl;

namespace dbg::analysis {
class Disassembler;
struct Instruction;
}

class Debugger {
public:
    Debugger();
    ~Debugger();

    Debugger(const Debugger&) = delete;
    Debugger& operator=(const Debugger&) = delete;

    bool start_process(const std::string& path, const std::string& cmdline = "");
    bool attach_to_process(DWORD pid);
    bool detach();
    bool terminate(UINT exit_code = 0);

    bool continue_execution();
    bool step_into();
    bool step_over();
    bool finish();

    bool set_int3_breakpoint(uint64_t address, dbgtype::BreakpointKind kind = dbgtype::BreakpointKind::Software);
    bool remove_breakpoint(uint64_t address);

    bool read_registers(dbgtype::RegisterState& out) const;
    bool read_memory(uint64_t address, void* buffer, size_t size, size_t* bytes_read) const;
    bool write_memory(uint64_t address, const void* buffer, size_t size, size_t* bytes_written);

    std::vector<dbgtype::ThreadInfo> get_threads_info() const;
    std::vector<dbgtype::ThreadInfo> list_threads() const;
    std::vector<dbgtype::ModuleInfo> list_modules() const;
    std::vector<dbgtype::Breakpoint> get_breakpoints() const;
    std::vector<dbgtype::MemoryRegion> get_memory_regions() const;
    std::vector<dbgtype::RegisterValue> get_registers() const;
    std::vector<dbg::analysis::Instruction> get_disassembly() const;

    bool get_thread_ip(HANDLE thread, uint64_t& ip) const;
    bool set_thread_ip(HANDLE thread, uint64_t ip);

    DebugSession& session();
    const DebugSession& session() const;

    void append_line(const std::string& line);
    const std::vector<std::string>& get_log_lines() const;

    // C API compatibility helpers
    int continue_exec();
    int quit();
    int is_active() const;
    const char* last_output() const;
    const char* last_error() const;
    bool set_disassembly_count(size_t count);

private:
    void begin_command_output();
    void set_error(const std::string& message);
    void append_stop_event_summary();
    void append_captured_process_output();

private:
    DebugSession session_;
    std::vector<std::string> log_lines_;
    std::string last_output_;
    std::string last_error_;
    size_t disasm_count_ = 8;

    std::unique_ptr<ProcessControl> process_control_;
    std::unique_ptr<DebugLoop> debug_loop_;
    std::unique_ptr<Threads> threads_;
    std::unique_ptr<Modules> modules_;
    std::unique_ptr<Memory> memory_;
    std::unique_ptr<MemoryRegionManager> memory_region_manager_;
    std::unique_ptr<Registers> registers_;
    std::unique_ptr<BreakpointManager> breakpoint_manager_;
    std::unique_ptr<RunControl> run_control_;
    std::unique_ptr<dbg::analysis::Disassembler> disassembler_;
};

