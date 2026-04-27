#include "breakpoint.h"

#include "memory.h"
#include "registers.h"
#include "threads.h"

BreakpointManager::BreakpointManager(DebugSession &session, Memory &memory, Registers &registers, Threads &threads)
    : session_(session), memory_(memory), registers_(registers), threads_(threads)
{
}

bool BreakpointManager::set_int3_breakpoint(uint64_t address, dbgtype::BreakpointKind kind)
{
    if (session_.breakpoints.find(address) != session_.breakpoints.end())
    {
        return true;
    }

    // 保存原本的 instruction byte
    uint8_t original = 0;
    size_t read = 0;
    if (!memory_.read(address, &original, sizeof(original), &read) || read != sizeof(original))
    {
        return false;
    }

    // 更改權限
    DWORD old_protect = 0;
    if (!memory_.protect(address, 1, PAGE_EXECUTE_READWRITE, &old_protect))
    {
        return false;
    }

    // patch
    const uint8_t int3 = 0xCC;
    size_t written = 0;
    const bool ok = memory_.write(address, &int3, 1, &written) && written == 1;
    DWORD ignored = 0;

    // 還原權限並刷更新 instruciton
    memory_.protect(address, 1, old_protect, &ignored);
    memory_.flush_instruction_cache(address, 1);
    if (!ok)
    {
        return false;
    }

    dbgtype::Breakpoint bp;
    bp.id = int(session_.breakpoints.size()) + 1;
    bp.kind = kind;
    bp.address = address;
    bp.hit_count = 0;
    bp.enabled = true;
    bp.original_byte = original;
    session_.breakpoints[address] = bp;
    return true;
}

bool BreakpointManager::remove_breakpoint(uint64_t address)
{
    auto it = session_.breakpoints.find(address);
    if (it == session_.breakpoints.end())
    {
        return false;
    }

    DWORD old_protect = 0;
    if (!memory_.protect(address, 1, PAGE_EXECUTE_READWRITE, &old_protect))
    {
        return false;
    }

    // ??0xCC 寫�??�本??byte
    size_t written = 0;
    const bool ok = memory_.write(address, &it->second.original_byte, 1, &written) && written == 1;
    DWORD ignored = 0;
    memory_.protect(address, 1, old_protect, &ignored);
    memory_.flush_instruction_cache(address, 1);
    if (!ok)
    {
        return false;
    }

    session_.breakpoints.erase(it);
    return true;
}

bool BreakpointManager::handle_breakpoint_hit(DWORD tid,
                                              uint64_t *hit_address,
                                              dbgtype::BreakpointKind *hit_kind)
{
    HANDLE thread = threads_.get_thread_handle(tid);
    bool opened_local = false;
    if (!thread)
    {
        thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, tid);
        if (!thread)
        {
            return false;
        }
        opened_local = true;
    }

    uint64_t ip = 0;
    if (!registers_.get_thread_ip(thread, ip) || ip == 0)
    {
        if (opened_local)
        {
            CloseHandle(thread);
        }
        return false;
    }

    const uint64_t bp_addr = ip - 1;
    auto it = session_.breakpoints.find(bp_addr);
    if (it == session_.breakpoints.end())
    {
        if (opened_local)
        {
            CloseHandle(thread);
        }
        return false;
    }

    ++it->second.hit_count;
    const dbgtype::BreakpointKind breakpoint_kind = it->second.kind;

    if (!remove_breakpoint(bp_addr))
    {
        if (opened_local)
        {
            CloseHandle(thread);
        }
        return false;
    }

    // restore IP to the original instruction address
    if (!registers_.set_thread_ip(thread, bp_addr))
    {
        if (opened_local)
        {
            CloseHandle(thread);
        }
        return false;
    }

    if (hit_address)
    {
        *hit_address = bp_addr;
    }
    if (hit_kind)
    {
        *hit_kind = breakpoint_kind;
    }

    threads_.refresh_callstack(tid);

    if (opened_local)
    {
        CloseHandle(thread);
    }
    return true;
}
