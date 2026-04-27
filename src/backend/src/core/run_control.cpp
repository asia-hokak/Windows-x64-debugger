#include "run_control.h"

#include "breakpoint.h"
#include "debug_loop.h"
#include "registers.h"
#include "threads.h"
#include "../analysis/disassembler.h"

static bool is_target_x64(const DebugSession& session)
{
    return session.target_arch == dbgtype::TargetArch::X64;
}

static bool address_in_instruction(const dbg::analysis::Instruction& insn, uint64_t address)
{
    uint64_t size = insn.size == 0 ? 1 : uint64_t(insn.size);
    uint64_t end = insn.address + size;
    if (end < insn.address) {
        return address >= insn.address;
    }
    return address >= insn.address && address < end;
}

static bool find_next_instruction_address(const std::vector<dbg::analysis::Instruction>& items,
                                          uint64_t address,
                                          uint64_t& next_address)
{
    size_t i = 0;
    while (i < items.size()) {
        if (address_in_instruction(items[i], address)) {
            if (i + 1 >= items.size()) {
                return false;
            }
            next_address = items[i + 1].address;
            return true;
        }
        ++i;
    }
    return false;
}

static bool is_call_instruction_at_address(const std::vector<dbg::analysis::Instruction>& items, uint64_t address)
{
    size_t i = 0;
    while (i < items.size()) {
        if (address_in_instruction(items[i], address)) {
            const auto kind = items[i].kind;
            return kind == dbg::analysis::InstructionKind::Call;
        }
        ++i;
    }
    return false;
}

static bool is_return_instruction_at_address(const std::vector<dbg::analysis::Instruction>& items, uint64_t address)
{
    size_t i = 0;
    while (i < items.size()) {
        if (address_in_instruction(items[i], address)) {
            return items[i].kind == dbg::analysis::InstructionKind::Return;
        }
        ++i;
    }
    return false;
}

static bool analyze_instruction_at_address(const std::vector<dbg::analysis::Instruction>& items,
                                           uint64_t address,
                                           dbg::analysis::InstructionKind& out_kind,
                                           uint64_t& out_next_address)
{
    out_kind = dbg::analysis::InstructionKind::Invalid;
    out_next_address = 0;

    size_t i = 0;
    while (i < items.size()) {
        if (address_in_instruction(items[i], address)) {
            out_kind = items[i].kind;
            if (i + 1 < items.size()) {
                out_next_address = items[i + 1].address;
            }
            return true;
        }
        ++i;
    }
    return false;
}

static dbgtype::ThreadInfo* get_current_thread_info(DebugSession& session, Threads& threads)
{
    // 從 tid 找 thread
    const auto tid = threads.get_current_thread_id();
    if (!tid.has_value()) {
        return nullptr;
    }

    auto it = session.threads.find(*tid);
    if (it == session.threads.end()) {
        return nullptr;
    }
    return &it->second;
}

RunControl::RunControl(DebugSession& session,
                       DebugLoop& debug_loop,
                       Registers& registers,
                       Memory& memory,
                       BreakpointManager& breakpoints,
                       Threads& threads)
    : session_(session),
      debug_loop_(debug_loop),
      registers_(registers),
      memory_(memory),
      breakpoints_(breakpoints),
      threads_(threads)
{
}

bool RunControl::continue_execution()
{
    DEBUG_EVENT ev = {};
    return run_until_stop(ev);
}

bool RunControl::step_into()
{
    const auto tid = threads_.get_current_thread_id();
    if (!tid) {
        return false;
    }

    if (!registers_.set_trap_flag(*tid, true)) {
        return false;
    }


    dbg::analysis::InstructionKind current_kind = dbg::analysis::InstructionKind::Invalid;
    uint64_t next_address = 0;
    HANDLE thread = threads_.get_current_thread_handle();
    if (thread) {
        uint64_t rip = 0;
        if (registers_.get_thread_ip(thread, rip)) {
            dbg::analysis::Disassembler disassembler(session_);
            const bool is_x64 = is_target_x64(session_);
            const auto down = disassembler.disassembly_window_down(rip, 1, is_x64, memory_);
            analyze_instruction_at_address(down, rip, current_kind, next_address); // 找 next_instruciton 和 current_kind
        }
    }

    DEBUG_EVENT ev = {};
    const bool ok = run_until_stop(ev);
    if (!ok) {
        return false;
    }

    // 這邊 current_kind 代表執行過的一條 instruction，如果是 call 或 return 的話 callstack 會有變化
    dbgtype::ThreadInfo* thread_info = get_current_thread_info(session_, threads_);
    if (!thread_info) {
        return true;
    }

    if (current_kind == dbg::analysis::InstructionKind::Call && next_address != 0) {
        thread_info->callstack.push_back(next_address); // return address 要是 next_address
    } else if (current_kind == dbg::analysis::InstructionKind::Return) {
        if (!thread_info->callstack.empty()) {
            thread_info->callstack.pop_back();
        }
    }

    return true;
}

bool RunControl::step_over()
{
    HANDLE thread = threads_.get_current_thread_handle();
    if (!thread) {
        return false;
    }

    uint64_t rip = 0;
    if (!registers_.get_thread_ip(thread, rip)) {
        return false;
    }

    // 只有 call 才走 step over，其他指令直接 step into
    dbg::analysis::Disassembler disassembler(session_);
    const bool is_x64 = is_target_x64(session_);
    const auto down = disassembler.disassembly_window_down(rip, 1, is_x64, memory_);
    if (!is_call_instruction_at_address(down, rip)) {
        return step_into();
    }

    uint64_t next_address = 0;
    if (!find_next_instruction_address(down, rip, next_address)) {
        return false;
    }

    // 先檢查目前有沒有 breakpoint 了
    const bool had_existing_breakpoint = session_.breakpoints.find(next_address) != session_.breakpoints.end();
    bool inserted_step_over_breakpoint = false;
    if (!had_existing_breakpoint) {
        if (!breakpoints_.set_int3_breakpoint(next_address, dbgtype::BreakpointKind::StepOver)) {
            return false;
        }
        inserted_step_over_breakpoint = true;
    }

    const bool ok = continue_execution();

    if (inserted_step_over_breakpoint) {
        breakpoints_.remove_breakpoint(next_address);
    }

    return ok;
}

bool RunControl::finish()
{
    dbg::analysis::Disassembler disassembler(session_);
    const bool is_x64 = is_target_x64(session_);
    const uint32_t k_max_finish_steps = 100000;
    uint32_t step_count = 0;

    while (step_count < k_max_finish_steps) {
        HANDLE thread = threads_.get_current_thread_handle();
        if (!thread) {
            return false;
        }

        uint64_t rip = 0;
        if (!registers_.get_thread_ip(thread, rip)) {
            return false;
        }

        const auto down = disassembler.disassembly_window_down(rip, 1, is_x64, memory_);
        if (is_return_instruction_at_address(down, rip)) {
            return step_over(); // 走到 ret instruction 時 step_over，剛好跳出 function
        }

        if (!step_over()) {
            return false;
        }
        ++step_count;

        if (session_.state == dbgtype::DebugState::Exited) {
            return true;
        }
    }

    return false;
}

const DEBUG_EVENT& RunControl::last_stop_event() const
{
    return last_stop_event_;
}

bool RunControl::last_breakpoint_address(uint64_t& address) const
{
    return debug_loop_.last_breakpoint_address(address);
}

bool RunControl::last_breakpoint_kind(dbgtype::BreakpointKind& kind) const
{
    return debug_loop_.last_breakpoint_kind(kind);
}

bool RunControl::run_until_stop(DEBUG_EVENT& out_event)
{
    session_.state = dbgtype::DebugState::Running;

    if (!debug_loop_.continue_last_event(DBG_CONTINUE)) {
        // 因為 first launch 通常沒有事件要處理，所以這邊可容許回傳 false
    }

    while (true) {
        DEBUG_EVENT ev = {};
        if (!debug_loop_.wait_for_event(ev, INFINITE)) {
            return false;
        }

        if (ev.dwDebugEventCode == EXCEPTION_DEBUG_EVENT || ev.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) {
            out_event = ev;
            last_stop_event_ = ev;
            return true;
        }

        if (!debug_loop_.continue_last_event(DBG_CONTINUE)) { // 有些 event 是不需要停下來的，直接繼續
            return false;
        }
    }
}
