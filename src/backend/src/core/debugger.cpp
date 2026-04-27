#include "debugger.h"

#include <algorithm>
#include <stddef.h>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "breakpoint.h"
#include "debug_loop.h"
#include "memory.h"
#include "memory_region.h"
#include "modules.h"
#include "process_control.h"
#include "registers.h"
#include "run_control.h"
#include "threads.h"
#include "../analysis/disassembler.h"

using std::make_unique;
using std::min;
using std::dec;
using std::hex;
using std::ostringstream;
using std::string;
using std::vector;

static constexpr size_t kMaxDisassemblyCount = 128;
static constexpr DWORD kStatusWx86SingleStep = 0x4000001E;
static constexpr DWORD kStatusWx86Breakpoint = 0x4000001F;

static string FormatExceptionCode(uint32_t code)
{
    ostringstream oss;
    oss << "0x" << hex << code;
    return oss.str();
}

static bool IsTargetX64(const DebugSession& session)
{
    return session.target_arch == dbgtype::TargetArch::X64;
}

static const char* TargetArchName(const DebugSession& session)
{
    switch (session.target_arch) {
    case dbgtype::TargetArch::X86:
        return "x86";
    case dbgtype::TargetArch::X64:
        return "x64";
    default:
        return "unknown";
    }
}

Debugger::Debugger()
    : process_control_(make_unique<ProcessControl>(session_)),
      debug_loop_(make_unique<DebugLoop>(session_)),
      threads_(make_unique<Threads>(session_)),
      modules_(make_unique<Modules>(session_)),
      memory_(make_unique<Memory>(session_)),
      memory_region_manager_(make_unique<MemoryRegionManager>(session_)),
      registers_(make_unique<Registers>(session_)),
      breakpoint_manager_(make_unique<BreakpointManager>(session_, *memory_, *registers_, *threads_)),
      run_control_(make_unique<RunControl>(session_, *debug_loop_, *registers_, *memory_, *breakpoint_manager_, *threads_)),
      disassembler_(make_unique<dbg::analysis::Disassembler>(session_))
{
    debug_loop_->set_threads(threads_.get());
    debug_loop_->set_modules(modules_.get());
    debug_loop_->set_breakpoints(breakpoint_manager_.get());
}

Debugger::~Debugger() = default;

bool Debugger::start_process(const string& path, const string& cmdline)
{
    begin_command_output();

    if (!process_control_->start_process(path, cmdline)) {
        ostringstream oss;
        oss << "CreateProcess failed, error=" << GetLastError();
        set_error(oss.str());
        return false;
    }

    append_line("[backend] session started");
    {
        ostringstream oss;
        oss << "[backend] target arch: " << TargetArchName(session_);
        append_line(oss.str());
    }

    if (!run_control_->continue_execution()) {
        set_error("failed to run to initial stop");
        return false;
    }

    append_captured_process_output();
    append_stop_event_summary();
    return true;
}

bool Debugger::attach_to_process(DWORD pid)
{
    begin_command_output();
    if (!process_control_->attach(pid)) {
        ostringstream oss;
        oss << "DebugActiveProcess failed, error=" << GetLastError();
        set_error(oss.str());
        return false;
    }

    append_line("[backend] attached");
    {
        ostringstream oss;
        oss << "[backend] target arch: " << TargetArchName(session_);
        append_line(oss.str());
    }

    if (!run_control_->continue_execution()) {
        set_error("failed to run to initial stop after attach");
        return false;
    }

    append_captured_process_output();
    append_stop_event_summary();
    return true;
}

bool Debugger::detach()
{
    begin_command_output();
    if (!process_control_->detach()) {
        ostringstream oss;
        oss << "detach failed, error=" << GetLastError();
        set_error(oss.str());
        return false;
    }
    append_line("[backend] detached");
    return true;
}

bool Debugger::terminate(UINT exit_code)
{
    begin_command_output();
    if (!process_control_->terminate(exit_code)) {
        ostringstream oss;
        oss << "terminate failed, error=" << GetLastError();
        set_error(oss.str());
        return false;
    }

    session_.clear();
    append_line("[backend] session terminated");
    return true;
}

bool Debugger::continue_execution()
{
    begin_command_output();
    if (!run_control_->continue_execution()) {
        set_error("continue failed");
        return false;
    }

    append_captured_process_output();
    append_stop_event_summary();
    return true;
}

bool Debugger::step_into()
{
    begin_command_output();
    if (!run_control_->step_into()) {
        set_error("step into failed");
        return false;
    }

    append_captured_process_output();
    append_stop_event_summary();
    return true;
}

bool Debugger::step_over()
{
    begin_command_output();
    if (!run_control_->step_over()) {
        set_error("step over failed");
        return false;
    }

    append_captured_process_output();
    append_stop_event_summary();
    return true;
}

bool Debugger::finish()
{
    begin_command_output();
    if (!run_control_->finish()) {
        set_error("finish failed");
        return false;
    }

    append_captured_process_output();
    append_stop_event_summary();
    return true;
}

bool Debugger::set_int3_breakpoint(uint64_t address, dbgtype::BreakpointKind kind)
{
    begin_command_output();
    if (!breakpoint_manager_->set_int3_breakpoint(address, kind)) {
        ostringstream oss;
        oss << "failed to set breakpoint at 0x" << hex << address;
        set_error(oss.str());
        return false;
    }

    ostringstream oss;
    if (kind == dbgtype::BreakpointKind::StepOver) {
        oss << "[backend] step-over breakpoint set at 0x" << hex << address;
    } else {
        oss << "[backend] breakpoint set at 0x" << hex << address;
    }
    append_line(oss.str());
    return true;
}

bool Debugger::remove_breakpoint(uint64_t address)
{
    begin_command_output();
    if (!breakpoint_manager_->remove_breakpoint(address)) {
        ostringstream oss;
        oss << "failed to clear breakpoint at 0x" << hex << address;
        set_error(oss.str());
        return false;
    }

    ostringstream oss;
    oss << "[backend] breakpoint cleared at 0x" << hex << address;
    append_line(oss.str());
    return true;
}

bool Debugger::read_registers(dbgtype::RegisterState& out) const
{
    return registers_->read_current_thread_registers(out);
}

bool Debugger::read_memory(uint64_t address, void* buffer, size_t size, size_t* bytes_read) const
{
    return memory_->read(address, buffer, size, bytes_read);
}

bool Debugger::write_memory(uint64_t address, const void* buffer, size_t size, size_t* bytes_written)
{
    return memory_->write(address, buffer, size, bytes_written);
}

vector<dbgtype::ThreadInfo> Debugger::get_threads_info() const
{
    return threads_->list_threads();
}

vector<dbgtype::ThreadInfo> Debugger::list_threads() const
{
    return get_threads_info();
}

vector<dbgtype::ModuleInfo> Debugger::list_modules() const
{
    return modules_->query_module();
}

vector<dbgtype::Breakpoint> Debugger::get_breakpoints() const
{
    vector<dbgtype::Breakpoint> out;
    out.reserve(session_.breakpoints.size());
    for (const auto& breakpoint_entry : session_.breakpoints) {
        out.push_back(breakpoint_entry.second);
    }
    std::sort(out.begin(), out.end(), [](const dbgtype::Breakpoint& a, const dbgtype::Breakpoint& b) {
        return a.address < b.address;
    });
    return out;
}

vector<dbgtype::MemoryRegion> Debugger::get_memory_regions() const
{
    if (!memory_region_manager_) {
        return {};
    }
    return memory_region_manager_->query_regions();
}

vector<dbgtype::RegisterValue> Debugger::get_registers() const
{
    dbgtype::RegisterState regs = {};
    if (!read_registers(regs)) {
        return {};
    }

    vector<dbgtype::RegisterValue> out;
    out.reserve(18);

    if (session_.target_arch == dbgtype::TargetArch::X86) {
        out.push_back({"eax", regs.rax});
        out.push_back({"ebx", regs.rbx});
        out.push_back({"ecx", regs.rcx});
        out.push_back({"edx", regs.rdx});
        out.push_back({"esi", regs.rsi});
        out.push_back({"edi", regs.rdi});
        out.push_back({"ebp", regs.rbp});
        out.push_back({"esp", regs.rsp});
        out.push_back({"eip", regs.rip});
        out.push_back({"eflags", regs.eflags});
    } else {
        out.push_back({"rax", regs.rax});
        out.push_back({"rbx", regs.rbx});
        out.push_back({"rcx", regs.rcx});
        out.push_back({"rdx", regs.rdx});
        out.push_back({"rsi", regs.rsi});
        out.push_back({"rdi", regs.rdi});
        out.push_back({"rbp", regs.rbp});
        out.push_back({"rsp", regs.rsp});
        out.push_back({"rip", regs.rip});
        out.push_back({"r8", regs.r8});
        out.push_back({"r9", regs.r9});
        out.push_back({"r10", regs.r10});
        out.push_back({"r11", regs.r11});
        out.push_back({"r12", regs.r12});
        out.push_back({"r13", regs.r13});
        out.push_back({"r14", regs.r14});
        out.push_back({"r15", regs.r15});
        out.push_back({"rflags", regs.eflags});
    }

    return out;
}

vector<dbg::analysis::Instruction> Debugger::get_disassembly() const
{
    if (!disassembler_ || !memory_ || !registers_ || !threads_) {
        return {};
    }

    HANDLE thread = threads_->get_current_thread_handle();
    if (!thread) {
        return {};
    }

    uint64_t ip = 0;
    if (!registers_->get_thread_ip(thread, ip)) {
        return {};
    }

    const bool is_x64 = IsTargetX64(session_);
    return disassembler_->disassembly_at(ip, disasm_count_, is_x64, *memory_);
}

bool Debugger::get_thread_ip(HANDLE thread, uint64_t& ip) const
{
    return registers_->get_thread_ip(thread, ip);
}

bool Debugger::set_thread_ip(HANDLE thread, uint64_t ip)
{
    return registers_->set_thread_ip(thread, ip);
}

DebugSession& Debugger::session()
{
    return session_;
}

const DebugSession& Debugger::session() const
{
    return session_;
}

void Debugger::append_line(const string& line)
{
    log_lines_.push_back(line);
    last_output_ += line;
    last_output_ += "\n";
}

const vector<string>& Debugger::get_log_lines() const
{
    return log_lines_;
}

int Debugger::continue_exec()
{
    return continue_execution() ? 0 : -1;
}

int Debugger::quit()
{
    begin_command_output();

    if (!session_.is_active()) {
        append_line("[backend] session not active");
        return 2;
    }

    if (!process_control_->terminate(0)) {
        ostringstream oss;
        oss << "terminate failed, error=" << GetLastError();
        set_error(oss.str());
        return -1;
    }

    session_.clear();
    append_line("[backend] session terminated");
    return 2;
}

int Debugger::is_active() const
{
    return session_.is_active() ? 1 : 0;
}

const char* Debugger::last_output() const
{
    return last_output_.c_str();
}

const char* Debugger::last_error() const
{
    return last_error_.c_str();
}

bool Debugger::set_disassembly_count(size_t count)
{
    begin_command_output();
    if (count > kMaxDisassemblyCount) {
        ostringstream oss;
        oss << "invalid disassembly count " << count << " (max " << kMaxDisassemblyCount << ")";
        set_error(oss.str());
        return false;
    }

    disasm_count_ = count;
    ostringstream oss;
    oss << "[backend] disassembly count set to " << count;
    append_line(oss.str());
    return true;
}

void Debugger::begin_command_output()
{
    last_output_.clear();
    last_error_.clear();
}

void Debugger::set_error(const string& message)
{
    last_error_ = message;
    append_line(string("[backend] error: ") + message);
}

void Debugger::append_stop_event_summary()
{
    // debugger 在處理每次 event 的時候會記錄東西到 last_stop_event
    const auto& ev = run_control_->last_stop_event();
    if (ev.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
        const DWORD code = ev.u.Exception.ExceptionRecord.ExceptionCode;
        if (code == EXCEPTION_BREAKPOINT || code == kStatusWx86Breakpoint) {
            bool skip_hit_log = false;
            dbgtype::BreakpointKind hit_kind = dbgtype::BreakpointKind::Software;
            if (run_control_->last_breakpoint_kind(hit_kind) && hit_kind == dbgtype::BreakpointKind::StepOver) {
                skip_hit_log = true;
            }
            uint64_t hit_address = 0;
            if (run_control_->last_breakpoint_address(hit_address)) {
                if (!skip_hit_log) {
                    ostringstream oss;
                    oss << "[backend] Hit breakpoint at 0x" << hex << hit_address;
                    append_line(oss.str());
                }
            } else {
                const uint64_t event_address = (uint64_t)(uintptr_t)(ev.u.Exception.ExceptionRecord.ExceptionAddress);
                if (!skip_hit_log) {
                    ostringstream oss;
                    oss << "[backend] Hit breakpoint at 0x" << hex << event_address;
                    append_line(oss.str());
                }
            }
            return;
        }

        if (code == EXCEPTION_SINGLE_STEP || code == kStatusWx86SingleStep) {
            return;
        }

        append_line("[backend] Exception " + FormatExceptionCode(code) +
                    (ev.u.Exception.dwFirstChance != FALSE ? " (first-chance)" : " (second-chance)"));
        return;
    }

    if (ev.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) {
        ostringstream oss;
        oss << "[backend] Process exited, code=" << ev.u.ExitProcess.dwExitCode;
        append_line(oss.str());
        return;
    }
}

void Debugger::append_captured_process_output()
{
    if (!process_control_) {
        return;
    }

    vector<string> lines;
    process_control_->drain_captured_output(lines);
    for (const auto& line : lines) {
        if (!line.empty()) {
            append_line("[target] " + line);
        }
    }
}






