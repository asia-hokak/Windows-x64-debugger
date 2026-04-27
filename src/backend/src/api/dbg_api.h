#pragma once

#include <stdint.h>

#include "dbg_export.h"
#include "dbg_types.h"

#ifdef __cplusplus
class Debugger;
extern "C" {
#endif

// debugger class create & delete
DBG_EXPORT Debugger* dbg_create_session();
DBG_EXPORT void dbg_destroy_session(Debugger* dbg);

// debugger 操作
DBG_EXPORT int dbg_start_process(Debugger* dbg, const char* target_exe, const char* args_line);
DBG_EXPORT int dbg_attach_process(Debugger* dbg, uint32_t pid);
DBG_EXPORT int dbg_continue(Debugger* dbg);
DBG_EXPORT int dbg_step_into(Debugger* dbg);
DBG_EXPORT int dbg_step_over(Debugger* dbg);
DBG_EXPORT int dbg_finish(Debugger* dbg);
DBG_EXPORT int dbg_quit(Debugger* dbg);
DBG_EXPORT int dbg_set_int3_breakpoint(Debugger* dbg, uint64_t address, uint32_t kind);
DBG_EXPORT int dbg_remove_breakpoint(Debugger* dbg, uint64_t address);
DBG_EXPORT int dbg_set_disassembly_count(Debugger* dbg, uint32_t count);

// debugger 查詢資料
DBG_EXPORT int dbg_get_disassembly(Debugger* dbg, dbgapi::InstructionView* out_items, uint32_t capacity, uint32_t* out_count);
DBG_EXPORT int dbg_get_breakpoints(Debugger* dbg, dbgapi::BreakpointView* out_items, uint32_t capacity, uint32_t* out_count);
DBG_EXPORT int dbg_get_memory_regions(Debugger* dbg, dbgapi::MemoryRegionView* out_items, uint32_t capacity, uint32_t* out_count);
DBG_EXPORT int dbg_get_registers(Debugger* dbg, dbgapi::RegisterView* out_items, uint32_t capacity, uint32_t* out_count);
DBG_EXPORT int dbg_get_threads_info(Debugger* dbg, dbgapi::ThreadInfoView* out_items, uint32_t capacity, uint32_t* out_count);
DBG_EXPORT int dbg_read_memory(Debugger* dbg, uint64_t address, uint8_t* out_bytes, uint32_t capacity, uint32_t* out_read);

// debugger 查詢變數
DBG_EXPORT int dbg_is_active(Debugger* dbg);
DBG_EXPORT char* dbg_get_last_output(Debugger* dbg);
DBG_EXPORT char* dbg_get_last_error(Debugger* dbg);

#ifdef __cplusplus
}
#endif
