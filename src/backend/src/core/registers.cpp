#include "registers.h"

#include <wow64apiset.h>

Registers::Registers(DebugSession& session)
    : session_(session)
{
}

bool Registers::read_current_thread_registers(dbgtype::RegisterState& out) const
{
    if (!session_.current_tid.has_value()) {
        return false;
    }

    HANDLE thread = nullptr;
    bool opened_local = false;

    auto it = session_.threads.find(*session_.current_tid);
    if (it != session_.threads.end()) {
        thread = it->second.handle;
    }
    if (!thread) {
        thread = OpenThread(THREAD_GET_CONTEXT, FALSE, *session_.current_tid);
        if (!thread) {
            return false;
        }
        opened_local = true;
    }

    out = {};
    bool ok = false;

    if (session_.target_arch == dbgtype::TargetArch::X86) { // x86
        WOW64_CONTEXT ctx = {};
        ctx.ContextFlags = WOW64_CONTEXT_ALL;
        if (Wow64GetThreadContext(thread, &ctx)) {
            out.rax = ctx.Eax;
            out.rbx = ctx.Ebx;
            out.rcx = ctx.Ecx;
            out.rdx = ctx.Edx;
            out.rsi = ctx.Esi;
            out.rdi = ctx.Edi;
            out.rbp = ctx.Ebp;
            out.rsp = ctx.Esp;
            out.rip = ctx.Eip;
            out.eflags = ctx.EFlags;
            out.dr0 = ctx.Dr0;
            out.dr1 = ctx.Dr1;
            out.dr2 = ctx.Dr2;
            out.dr3 = ctx.Dr3;
            out.dr6 = ctx.Dr6;
            out.dr7 = ctx.Dr7;
            ok = true;
        }
    } else {
        CONTEXT ctx = {};
        ctx.ContextFlags = CONTEXT_ALL; // x64
        if (GetThreadContext(thread, &ctx)) {
            out.rax = ctx.Rax;
            out.rbx = ctx.Rbx;
            out.rcx = ctx.Rcx;
            out.rdx = ctx.Rdx;
            out.rsi = ctx.Rsi;
            out.rdi = ctx.Rdi;
            out.rbp = ctx.Rbp;
            out.rsp = ctx.Rsp;
            out.rip = ctx.Rip;
            out.r8 = ctx.R8;
            out.r9 = ctx.R9;
            out.r10 = ctx.R10;
            out.r11 = ctx.R11;
            out.r12 = ctx.R12;
            out.r13 = ctx.R13;
            out.r14 = ctx.R14;
            out.r15 = ctx.R15;
            out.eflags = ctx.EFlags;
            out.dr0 = ctx.Dr0;
            out.dr1 = ctx.Dr1;
            out.dr2 = ctx.Dr2;
            out.dr3 = ctx.Dr3;
            out.dr6 = ctx.Dr6;
            out.dr7 = ctx.Dr7;
            ok = true;
        }
    }

    if (opened_local) {
        CloseHandle(thread);
    }
    return ok;
}

bool Registers::get_thread_ip(HANDLE thread, uint64_t& ip) const
{
    if (session_.target_arch == dbgtype::TargetArch::X86) {
        WOW64_CONTEXT ctx = {};
        ctx.ContextFlags = WOW64_CONTEXT_CONTROL;
        if (!Wow64GetThreadContext(thread, &ctx)) {
            return false;
        }
        ip = ctx.Eip;
        return true;
    }

    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_CONTROL;
    if (!GetThreadContext(thread, &ctx)) {
        return false;
    }
    ip = ctx.Rip;
    return true;
}

bool Registers::set_thread_ip(HANDLE thread, uint64_t ip)
{
    if (session_.target_arch == dbgtype::TargetArch::X86) {
        WOW64_CONTEXT ctx = {};
        ctx.ContextFlags = WOW64_CONTEXT_CONTROL;
        if (!Wow64GetThreadContext(thread, &ctx)) {
            return false;
        }
        ctx.Eip = DWORD(ip);
        return Wow64SetThreadContext(thread, &ctx) != FALSE;
    }

    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_CONTROL;
    if (!GetThreadContext(thread, &ctx)) {
        return false;
    }
    ctx.Rip = ip;
    return SetThreadContext(thread, &ctx) != FALSE;
}

bool Registers::set_trap_flag(DWORD tid, bool enable_single_step) const
{
    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, tid);
    if (!thread) {
        return false;
    }

    constexpr DWORD kTrapFlag = 0x100;
    bool ok = false;

    if (session_.target_arch == dbgtype::TargetArch::X86) {
        WOW64_CONTEXT ctx = {};
        ctx.ContextFlags = WOW64_CONTEXT_CONTROL;
        if (Wow64GetThreadContext(thread, &ctx)) {
            if (enable_single_step) {
                ctx.EFlags |= kTrapFlag;
            } else {
                ctx.EFlags &= ~kTrapFlag;
            }
            ok = Wow64SetThreadContext(thread, &ctx) != FALSE;
        }
    } else {
        CONTEXT ctx = {};
        ctx.ContextFlags = CONTEXT_CONTROL;
        if (GetThreadContext(thread, &ctx)) {
            if (enable_single_step) {
                ctx.EFlags |= kTrapFlag;
            } else {
                ctx.EFlags &= ~kTrapFlag;
            }
            ok = SetThreadContext(thread, &ctx) != FALSE;
        }
    }

    CloseHandle(thread);
    return ok;
}