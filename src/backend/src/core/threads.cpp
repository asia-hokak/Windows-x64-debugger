#include "threads.h"

#include <algorithm>
#include <wow64apiset.h>

static bool read_u64(HANDLE process, uint64_t address, uint64_t& out_value)
{
    out_value = 0;
    if (!process) {
        return false;
    }

    SIZE_T bytes_read = 0;
    if (!ReadProcessMemory(process, (LPCVOID)(uintptr_t)(address), &out_value, sizeof(out_value), &bytes_read)) {
        return false;
    }
    return bytes_read == sizeof(out_value);
}

static bool read_u32(HANDLE process, uint64_t address, uint32_t& out_value)
{
    out_value = 0;
    if (!process) {
        return false;
    }

    SIZE_T bytes_read = 0;
    if (!ReadProcessMemory(process, (LPCVOID)(uintptr_t)(address), &out_value, sizeof(out_value), &bytes_read)) {
        return false;
    }
    return bytes_read == sizeof(out_value);
}

Threads::Threads(DebugSession& session)
    : session_(session)
{
}

bool Threads::add_thread(DWORD tid, HANDLE thread_handle, uint64_t teb)
{
    if (tid == 0 || !thread_handle) {
        return false;
    }

    dbgtype::ThreadInfo info;
    info.tid = tid;
    info.teb = teb;
    info.alive = true;
    info.suspended = false;
    info.is_current = !session_.current_tid.has_value();
    info.handle = thread_handle;

    // 初始化先掃一次 callstack
    auto pending_callstack_it = session_.pending_thread_callstacks.find(tid);
    if (pending_callstack_it != session_.pending_thread_callstacks.end()) {
        info.callstack = std::move(pending_callstack_it->second);
        session_.pending_thread_callstacks.erase(pending_callstack_it);
    } else {
        scan_thread_callstack(thread_handle, info.callstack);
    }

    // 如果 thread 存在就不新增
    auto existing = session_.threads.find(tid);
    if (existing != session_.threads.end()) {
        if (existing->second.handle && existing->second.handle != INVALID_HANDLE_VALUE) {
            CloseHandle(existing->second.handle);
        }
    }

    // 如果目前沒有 current thread 的話就把這個 thread 設成 current thread
    session_.threads[tid] = std::move(info);
    if (!session_.current_tid.has_value()) {
        session_.current_tid = tid;
    }
    return true;
}

bool Threads::remove_thread(DWORD tid)
{
    auto it = session_.threads.find(tid);
    const bool erased = it != session_.threads.end();
    if (erased) {
        if (it->second.handle && it->second.handle != INVALID_HANDLE_VALUE) {
            CloseHandle(it->second.handle);
        }
        session_.threads.erase(it);
    }
    if (erased && session_.current_tid.has_value() && *session_.current_tid == tid) {
        if (session_.threads.empty()) {
            session_.current_tid.reset();
        } else {
            session_.current_tid = session_.threads.begin()->first;
        }
    }

    return erased;
}

std::optional<DWORD> Threads::get_current_thread_id() const
{
    return session_.current_tid;
}

HANDLE Threads::get_thread_handle(DWORD tid) const
{
    auto it = session_.threads.find(tid);
    return it == session_.threads.end() ? nullptr : it->second.handle;
}

HANDLE Threads::get_current_thread_handle() const
{
    return session_.current_tid.has_value() ? get_thread_handle(*session_.current_tid) : nullptr;
}

bool Threads::refresh_callstack(DWORD tid)
{
    auto it = session_.threads.find(tid);
    if (it == session_.threads.end()) {
        return false;
    }

    HANDLE thread = it->second.handle;
    if (!thread || thread == INVALID_HANDLE_VALUE) {
        return false;
    }

    return scan_thread_callstack(thread, it->second.callstack);
}

bool Threads::scan_thread_callstack(HANDLE thread_handle, std::vector<uint64_t>& out_callstack) const
{
    out_callstack.clear();
    if (!thread_handle || thread_handle == INVALID_HANDLE_VALUE || !session_.process_handle) {
        return false;
    }
    // 以 stack frame 來獲得所有 ret address
    if (session_.target_arch == dbgtype::TargetArch::X86) {
        WOW64_CONTEXT ctx = {};
        ctx.ContextFlags = WOW64_CONTEXT_CONTROL;
        if (!Wow64GetThreadContext(thread_handle, &ctx)) {
            return false;
        }

        uint32_t frame = ctx.Ebp;
        if (frame == 0) {
            out_callstack.push_back(0);
            return true;
        }

        size_t depth = 0;
        while (frame != 0 && depth < 128) {
            uint32_t next = 0;
            uint32_t ret = 0;
            if (!read_u32(session_.process_handle, (uint64_t)(frame), next)) {
                break;
            }
            if (!read_u32(session_.process_handle, (uint64_t)(frame) + 4, ret)) {
                break;
            }
            if (ret == 0) {
                break;
            }
            out_callstack.push_back((uint64_t)(ret));
            if (next <= frame) {
                break;
            }
            frame = next;
            ++depth;
        }
    } else {
        CONTEXT ctx = {};
        ctx.ContextFlags = CONTEXT_CONTROL;
        if (!GetThreadContext(thread_handle, &ctx)) {
            return false;
        }

        uint64_t frame = ctx.Rbp;
        uint64_t stack = ctx.Rsp;
        if (frame == 0) {
            uint64_t ret = 0;
            if (read_u64(session_.process_handle, stack, ret) && ret != 0) {
                out_callstack.push_back(ret);
                return true;
            }
            return false;
        }

        size_t depth = 0;
        while (frame != 0 && depth < 128) {
            uint64_t next = 0;
            uint64_t ret = 0;
            if (!read_u64(session_.process_handle, frame, next)) {
                break;
            }
            if (!read_u64(session_.process_handle, frame + 8, ret)) {
                break;
            }
            if (ret == 0) {
                break;
            }
            out_callstack.push_back(ret);
            if (next <= frame) {
                break;
            }
            frame = next;
            ++depth;
        }
    }

    if (out_callstack.empty()) {
        return false;
    }

    std::reverse(out_callstack.begin(), out_callstack.end());
    return true;
}

std::vector<dbgtype::ThreadInfo> Threads::list_threads() const
{
    std::vector<dbgtype::ThreadInfo> out;
    out.reserve(session_.threads.size());
    for (const auto& [tid, info] : session_.threads) {
        dbgtype::ThreadInfo view = info;
        view.is_current = session_.current_tid.has_value() && *session_.current_tid == tid;
        out.push_back(std::move(view));
    }

    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        return a.tid < b.tid;
    });
    return out;
}
