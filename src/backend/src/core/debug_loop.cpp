#include "debug_loop.h"

#include "breakpoint.h"
#include "modules.h"
#include "threads.h"

#include <stdint.h>

static constexpr DWORD k_status_wx86_single_step = 0x4000001E;
static constexpr DWORD k_status_wx86_breakpoint = 0x4000001F;

static bool read_u32(HANDLE process, uint64_t address, uint32_t& out_value)
{
    out_value = 0;
    if (!process) {
        return false;
    }

    SIZE_T bytes_read = 0;
    if (!ReadProcessMemory(process, (LPCVOID)(uintptr_t)address, &out_value, sizeof(out_value), &bytes_read)) {
        return false;
    }
    return bytes_read == sizeof(out_value);
}

static bool read_bytes(HANDLE process, uint64_t address, void* out_buffer, size_t bytes)
{
    if (!process || !out_buffer || bytes == 0) {
        return false;
    }

    SIZE_T bytes_read = 0;
    if (!ReadProcessMemory(process, (LPCVOID)(uintptr_t)address, out_buffer, bytes, &bytes_read)) {
        return false;
    }
    return bytes_read == bytes;
}

static bool query_entry_point_address(HANDLE process, uint64_t image_base, uint64_t& out_entry_point)
{
    // 從 optional header 拿 entrypoint
    out_entry_point = 0;
    if (!process || image_base == 0) {
        return false;
    }

    IMAGE_DOS_HEADER dos = {};
    if (!read_bytes(process, image_base, &dos, sizeof(dos))) {
        return false;
    }
    if (dos.e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }

    const uint64_t nt_address = image_base + (uint32_t)(dos.e_lfanew);
    uint32_t signature = 0;
    if (!read_u32(process, nt_address, signature) || signature != IMAGE_NT_SIGNATURE) {
        return false;
    }

    const uint64_t optional_header_address = nt_address + 4 + sizeof(IMAGE_FILE_HEADER);
    uint32_t entry_rva = 0;
    const uint64_t entry_rva_address = optional_header_address + offsetof(IMAGE_OPTIONAL_HEADER64, AddressOfEntryPoint);
    if (!read_u32(process, entry_rva_address, entry_rva) || entry_rva == 0) {
        return false;
    }

    out_entry_point = image_base + entry_rva;
    return true;
}

DebugLoop::DebugLoop(DebugSession& session)
    : session_(session)
{
}

bool DebugLoop::wait_for_event(DEBUG_EVENT& out_event, DWORD timeout_ms)
{
    has_last_breakpoint_address_ = false;
    last_breakpoint_address_ = 0;
    has_last_breakpoint_kind_ = false;
    last_breakpoint_kind_ = dbgtype::BreakpointKind::Software;

    DEBUG_EVENT raw = {}; 
    if (!WaitForDebugEvent(&raw, timeout_ms)) {
        return false;
    }

    last_event_pid_ = raw.dwProcessId;
    last_event_tid_ = raw.dwThreadId;
    has_last_event_ = true;

    switch (raw.dwDebugEventCode) {
    case CREATE_PROCESS_DEBUG_EVENT:
        if (!handle_create_process(raw)) {
            return false;
        }
        break;
    case CREATE_THREAD_DEBUG_EVENT:
        if (!handle_create_thread(raw)) {
            return false;
        }
        break;
    case EXIT_THREAD_DEBUG_EVENT:
        if (!handle_exit_thread(raw)) {
            return false;
        }
        break;
    case LOAD_DLL_DEBUG_EVENT:
        if (!handle_load_dll(raw)) {
            return false;
        }
        break;
    case UNLOAD_DLL_DEBUG_EVENT:
        if (!handle_unload_dll(raw)) {
            return false;
        }
        break;
    case EXCEPTION_DEBUG_EVENT:
        if (!handle_exception(raw)) {
            return false;
        }
        break;
    case EXIT_PROCESS_DEBUG_EVENT:
        session_.state = dbgtype::DebugState::Exited;
        session_.attached = false;
        session_.current_tid.reset();
        break;
    default:
        break;
    }

    out_event = raw;
    last_event_ = raw;
    return true;
}

bool DebugLoop::continue_last_event(DWORD continue_status)
{
    if (!has_last_event_) {
        return false;
    }
    return ContinueDebugEvent(last_event_pid_, last_event_tid_, continue_status) != FALSE;
}

bool DebugLoop::handle_create_process(const DEBUG_EVENT& raw)
{
    session_.attached = true;
    session_.state = dbgtype::DebugState::Paused;
    session_.current_tid = raw.dwThreadId;
    if (session_.pid == 0) {
        session_.pid = raw.dwProcessId;
    }

    bool took_thread = false;
    if (threads_ && raw.u.CreateProcessInfo.hThread) {
        took_thread = threads_->add_thread(
            raw.dwThreadId,
            raw.u.CreateProcessInfo.hThread,
            (uint64_t)(uintptr_t)(raw.u.CreateProcessInfo.lpThreadLocalBase));
    }
    if (!took_thread && raw.u.CreateProcessInfo.hThread) {
        CloseHandle(raw.u.CreateProcessInfo.hThread);
    }

    if (modules_) {
        const auto base = (uint64_t)(uintptr_t)(raw.u.CreateProcessInfo.lpBaseOfImage);
        modules_->add_module(base, 0, "", true);
        modules_->get_proccess_module();

        if (breakpoints_ && session_.process_handle) {
            uint64_t entry_point = 0;
            if (query_entry_point_address(session_.process_handle, base, entry_point) && entry_point != 0) {
                breakpoints_->set_int3_breakpoint(entry_point, dbgtype::BreakpointKind::Software);
            }
        }
    }
    if (raw.u.CreateProcessInfo.hFile) {
        CloseHandle(raw.u.CreateProcessInfo.hFile);
    }

    return true;
}

bool DebugLoop::handle_create_thread(const DEBUG_EVENT& raw)
{
    if (threads_ && raw.u.CreateThread.hThread) {
        const bool took = threads_->add_thread(
            raw.dwThreadId,
            raw.u.CreateThread.hThread,
            (uint64_t)(uintptr_t)(raw.u.CreateThread.lpThreadLocalBase));
        if (!took) {
            CloseHandle(raw.u.CreateThread.hThread);
        }
    } else if (raw.u.CreateThread.hThread) {
        CloseHandle(raw.u.CreateThread.hThread);
    }

    return true;
}

bool DebugLoop::handle_exit_thread(const DEBUG_EVENT& raw)
{
    if (threads_) {
        threads_->remove_thread(raw.dwThreadId);
    }
    return true;
}

bool DebugLoop::handle_load_dll(const DEBUG_EVENT& raw)
{
    if (modules_) {
        const auto base = (uint64_t)(uintptr_t)(raw.u.LoadDll.lpBaseOfDll);
        modules_->add_module(base, 0, "", false);
        modules_->get_proccess_module();
    }
    if (raw.u.LoadDll.hFile) {
        CloseHandle(raw.u.LoadDll.hFile);
    }

    return true;
}

bool DebugLoop::handle_unload_dll(const DEBUG_EVENT& raw)
{
    if (modules_) {
        modules_->remove_module((uint64_t)(uintptr_t)(raw.u.UnloadDll.lpBaseOfDll));
    }
    return true;
}

bool DebugLoop::handle_exception(const DEBUG_EVENT& raw)
{
    const DWORD code = raw.u.Exception.ExceptionRecord.ExceptionCode;

    session_.state = dbgtype::DebugState::Paused;
    session_.current_tid = raw.dwThreadId;

    if (code == EXCEPTION_BREAKPOINT || code == k_status_wx86_breakpoint) {
        uint64_t hit = 0;
        dbgtype::BreakpointKind hit_kind = dbgtype::BreakpointKind::Software;
        if (breakpoints_ && breakpoints_->handle_breakpoint_hit(raw.dwThreadId, &hit, &hit_kind)) {
            has_last_breakpoint_address_ = true;
            last_breakpoint_address_ = hit;
            has_last_breakpoint_kind_ = true;
            last_breakpoint_kind_ = hit_kind;
        }
        return true;
    }

    if (code == EXCEPTION_SINGLE_STEP || code == k_status_wx86_single_step) {
        return true;
    }

    return true;
}

bool DebugLoop::last_breakpoint_address(uint64_t& address) const
{
    if (!has_last_breakpoint_address_) {
        return false;
    }
    address = last_breakpoint_address_;
    return true;
}

bool DebugLoop::last_breakpoint_kind(dbgtype::BreakpointKind& kind) const
{
    if (!has_last_breakpoint_kind_) {
        return false;
    }
    kind = last_breakpoint_kind_;
    return true;
}
