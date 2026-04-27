#include "dbg_api.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "../core/debugger.h"


void copy_string(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy_s(dst, dst_size, src, _TRUNCATE);
}

void format_bytes(char *dst, size_t dst_size, const std::vector<uint8_t> &bytes)
{
    if (!dst || dst_size == 0) {
        return;
    }

    dst[0] = '\0';
    for (size_t i = 0; i < bytes.size(); ++i) {
        char chunk[4] = {};
        sprintf_s(chunk, sizeof(chunk), "%02X", (unsigned int)(bytes[i]));
        if (i != 0) {
            strncat_s(dst, dst_size, " ", _TRUNCATE);
        }
        strncat_s(dst, dst_size, chunk, _TRUNCATE);
    }
}


Debugger *dbg_create_session() { return new Debugger(); }
void dbg_destroy_session(Debugger *dbg) { delete dbg; }

int dbg_start_process(Debugger *dbg, const char *target_exe, const char *args_line)
{
    if (!dbg) {
        return -1;
    }
    return dbg->start_process(target_exe ? target_exe : "", args_line ? args_line : "") ? 0 : -1;
}
int dbg_attach_process(Debugger *dbg, uint32_t pid)
{
    if (!dbg) {
        return -1;
    }
    return dbg->attach_to_process((DWORD)(pid)) ? 0 : -1;
}
int dbg_continue(Debugger *dbg)
{
    return dbg ? (dbg->continue_execution() ? 0 : -1) : -1;
}
int dbg_step_into(Debugger *dbg)
{
    return dbg ? (dbg->step_into() ? 0 : -1) : -1;
}
int dbg_step_over(Debugger *dbg)
{
    return dbg ? (dbg->step_over() ? 0 : -1) : -1;
}
int dbg_finish(Debugger *dbg)
{
    return dbg ? (dbg->finish() ? 0 : -1) : -1;
}
int dbg_quit(Debugger *dbg)
{
    return dbg ? dbg->quit() : -1;
}
int dbg_set_int3_breakpoint(Debugger *dbg, uint64_t address, uint32_t kind)
{
    if (!dbg) {
        return -1;
    }

    dbgtype::BreakpointKind breakpoint_kind = dbgtype::BreakpointKind::Software;
    switch (kind) {
    case 0:
        breakpoint_kind = dbgtype::BreakpointKind::Software;
        break;
    case 1:
        breakpoint_kind = dbgtype::BreakpointKind::StepOver;
        break;
    case 2:
        breakpoint_kind = dbgtype::BreakpointKind::Hardware;
        break;
    default:
        return -1;
    }

    return dbg->set_int3_breakpoint(address, breakpoint_kind) ? 0 : -1;
}
int dbg_remove_breakpoint(Debugger *dbg, uint64_t address)
{
    return dbg ? (dbg->remove_breakpoint(address) ? 0 : -1) : -1;
}
int dbg_set_disassembly_count(Debugger *dbg, uint32_t count)
{
    return dbg ? (dbg->set_disassembly_count((size_t)(count)) ? 0 : -1) : -1;
}
int dbg_get_disassembly(Debugger *dbg, dbgapi::InstructionView *out_items, uint32_t capacity, uint32_t *out_count)
{
    if (!dbg || !out_count) {
        return -1;
    }

    const auto items = dbg->get_disassembly();
    const size_t total = items.size();
    *out_count = (total > 0xFFFFFFFFull) ? 0xFFFFFFFFu : (uint32_t)(total);
    if (!out_items || capacity == 0) {
        return 0;
    }

    dbgtype::RegisterState regs = {};
    const bool has_regs = dbg->read_registers(regs);
    const uint64_t current_ip = has_regs ? regs.rip : 0;
    const size_t limit = (total < capacity) ? total : capacity;
    for (size_t i = 0; i < limit; ++i) {
        const auto &src = items[i];
        auto &dst = out_items[i];
        memset(&dst, 0, sizeof(dst));
        dst.address = src.address;
        dst.size = src.size;
        dst.is_current = (has_regs && src.address == current_ip) ? 1 : 0;
        dst.kind = (uint32_t)(src.kind);
        copy_string(dst.text, sizeof(dst.text), src.text.c_str());
        format_bytes(dst.bytes, sizeof(dst.bytes), src.bytes);
    }
    return 0;
}
int dbg_get_breakpoints(Debugger *dbg, dbgapi::BreakpointView *out_items, uint32_t capacity, uint32_t *out_count)
{
    if (!dbg || !out_count) {
        return -1;
    }

    const auto items = dbg->get_breakpoints();
    const size_t total = items.size();
    *out_count = (total > 0xFFFFFFFFull) ? 0xFFFFFFFFu : (uint32_t)(total);
    if (!out_items || capacity == 0) {
        return 0;
    }

    const size_t limit = (total < capacity) ? total : capacity;
    for (size_t i = 0; i < limit; ++i) {
        const auto &src = items[i];
        auto &dst = out_items[i];
        memset(&dst, 0, sizeof(dst));
        dst.address = src.address;
        dst.hit_count = src.hit_count;
        dst.kind = (uint32_t)(src.kind);
        dst.enabled = src.enabled ? 1 : 0;
    }
    return 0;
}
int dbg_get_memory_regions(Debugger *dbg, dbgapi::MemoryRegionView *out_items, uint32_t capacity, uint32_t *out_count)
{
    if (!dbg || !out_count) {
        return -1;
    }

    const auto items = dbg->get_memory_regions();
    const size_t total = items.size();
    *out_count = (total > 0xFFFFFFFFull) ? 0xFFFFFFFFu : (uint32_t)(total);
    if (!out_items || capacity == 0) {
        return 0;
    }

    const size_t limit = (total < capacity) ? total : capacity;
    for (size_t i = 0; i < limit; ++i) {
        const auto &src = items[i];
        auto &dst = out_items[i];
        memset(&dst, 0, sizeof(dst));
        dst.base = src.base;
        dst.size = src.size;
        dst.state = src.state;
        dst.protect = src.protect;
        dst.type = src.type;
        copy_string(dst.info, sizeof(dst.info), src.info.c_str());
    }
    return 0;
}
int dbg_get_registers(Debugger *dbg, dbgapi::RegisterView *out_items, uint32_t capacity, uint32_t *out_count)
{
    if (!dbg || !out_count) {
        return -1;
    }

    const auto items = dbg->get_registers();
    const size_t total = items.size();
    *out_count = (total > 0xFFFFFFFFull) ? 0xFFFFFFFFu : (uint32_t)(total);
    if (!out_items || capacity == 0) {
        return 0;
    }

    const size_t limit = (total < capacity) ? total : capacity;
    for (size_t i = 0; i < limit; ++i) {
        const auto &src = items[i];
        auto &dst = out_items[i];
        memset(&dst, 0, sizeof(dst));
        copy_string(dst.name, sizeof(dst.name), src.name.c_str());
        dst.value = src.value;
    }
    return 0;
}
int dbg_get_threads_info(Debugger *dbg, dbgapi::ThreadInfoView *out_items, uint32_t capacity, uint32_t *out_count)
{
    if (!dbg || !out_count) {
        return -1;
    }

    const auto items = dbg->get_threads_info();
    size_t total = 0;
    for (const auto &item : items) {
        total += item.callstack.empty() ? 1 : item.callstack.size();
    }

    *out_count = (total > 0xFFFFFFFFull) ? 0xFFFFFFFFu : (uint32_t)(total);
    if (!out_items || capacity == 0) {
        return 0;
    }

    size_t out_index = 0;
    for (const auto &item : items) {
        if (out_index >= capacity) {
            break;
        }

        if (item.callstack.empty()) {
            auto &dst = out_items[out_index++];
            memset(&dst, 0, sizeof(dst));
            dst.tid = item.tid;
            dst.teb = item.teb;
            dst.alive = item.alive ? 1 : 0;
            dst.suspended = item.suspended ? 1 : 0;
            dst.is_current = item.is_current ? 1 : 0;
            dst.frame_index = 0xFFFFFFFFu;
            dst.callstack_address = 0;
            continue;
        }

        for (size_t i = 0; i < item.callstack.size(); ++i) {
            if (out_index >= capacity) {
                break;
            }
            auto &dst = out_items[out_index++];
            memset(&dst, 0, sizeof(dst));
            dst.tid = item.tid;
            dst.teb = item.teb;
            dst.alive = item.alive ? 1 : 0;
            dst.suspended = item.suspended ? 1 : 0;
            dst.is_current = item.is_current ? 1 : 0;
            dst.frame_index = (i > 0xFFFFFFFFull) ? 0xFFFFFFFFu : (uint32_t)(i);
            dst.callstack_address = item.callstack[i];
        }
    }

    return 0;
}
int dbg_read_memory(Debugger *dbg, uint64_t address, uint8_t *out_bytes, uint32_t capacity, uint32_t *out_read)
{
    if (!dbg || !out_read) {
        return -1;
    }

    *out_read = 0;
    if (capacity == 0) {
        return 0;
    }
    if (!out_bytes) {
        return -1;
    }

    size_t bytes_read = 0;
    const bool ok = dbg->read_memory(address, out_bytes, (size_t)(capacity), &bytes_read);
    if (bytes_read > 0xFFFFFFFFull) {
        bytes_read = 0xFFFFFFFFull;
    }
    *out_read = (uint32_t)(bytes_read);

    if (!ok && bytes_read == 0) {
        return -1;
    }
    return 0;
}

int dbg_is_active(Debugger *dbg)
{
    return dbg ? dbg->is_active() : 0;
}
char *dbg_get_last_output(Debugger *dbg)
{
    static char k_empty[] = "";
    return dbg ? (char *)(dbg->last_output()) : k_empty;
}
char *dbg_get_last_error(Debugger *dbg)
{
    static char k_invalid[] = "invalid session";
    return dbg ? (char *)(dbg->last_error()) : k_invalid;
}


