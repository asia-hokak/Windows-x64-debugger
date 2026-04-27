from __future__ import annotations

import ctypes

from .errors import BackendError
from .loader import load_backend_dll
from .types import DbgBreakpointView, DbgInstructionView, DbgMemoryRegionView, DbgRegisterView, DbgThreadInfoView


MAX_RESULT_ITEMS = 1024


class BackendApi:
    def __init__(self, dll_path: str | None = None) -> None:
        self.dll = load_backend_dll(dll_path)
        self.dll.dbg_create_session.restype = ctypes.c_void_p
        self.dll.dbg_destroy_session.argtypes = [ctypes.c_void_p]

        self.dll.dbg_start_process.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]
        self.dll.dbg_start_process.restype = ctypes.c_int

        self.dll.dbg_attach_process.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
        self.dll.dbg_attach_process.restype = ctypes.c_int

        self.dll.dbg_continue.argtypes = [ctypes.c_void_p]
        self.dll.dbg_continue.restype = ctypes.c_int

        self.dll.dbg_step_into.argtypes = [ctypes.c_void_p]
        self.dll.dbg_step_into.restype = ctypes.c_int

        self.dll.dbg_step_over.argtypes = [ctypes.c_void_p]
        self.dll.dbg_step_over.restype = ctypes.c_int

        self.dll.dbg_finish.argtypes = [ctypes.c_void_p]
        self.dll.dbg_finish.restype = ctypes.c_int

        self.dll.dbg_quit.argtypes = [ctypes.c_void_p]
        self.dll.dbg_quit.restype = ctypes.c_int

        self.dll.dbg_set_int3_breakpoint.argtypes = [ctypes.c_void_p, ctypes.c_uint64, ctypes.c_uint32]
        self.dll.dbg_set_int3_breakpoint.restype = ctypes.c_int

        self.dll.dbg_remove_breakpoint.argtypes = [ctypes.c_void_p, ctypes.c_uint64]
        self.dll.dbg_remove_breakpoint.restype = ctypes.c_int

        self.dll.dbg_set_disassembly_count.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
        self.dll.dbg_set_disassembly_count.restype = ctypes.c_int

        self.dll.dbg_get_disassembly.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(DbgInstructionView),
            ctypes.c_uint32,
            ctypes.POINTER(ctypes.c_uint32),
        ]
        self.dll.dbg_get_disassembly.restype = ctypes.c_int

        self.dll.dbg_get_breakpoints.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(DbgBreakpointView),
            ctypes.c_uint32,
            ctypes.POINTER(ctypes.c_uint32),
        ]
        self.dll.dbg_get_breakpoints.restype = ctypes.c_int

        self.dll.dbg_get_memory_regions.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(DbgMemoryRegionView),
            ctypes.c_uint32,
            ctypes.POINTER(ctypes.c_uint32),
        ]
        self.dll.dbg_get_memory_regions.restype = ctypes.c_int

        self.dll.dbg_get_registers.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(DbgRegisterView),
            ctypes.c_uint32,
            ctypes.POINTER(ctypes.c_uint32),
        ]
        self.dll.dbg_get_registers.restype = ctypes.c_int

        self.dll.dbg_get_threads_info.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(DbgThreadInfoView),
            ctypes.c_uint32,
            ctypes.POINTER(ctypes.c_uint32),
        ]
        self.dll.dbg_get_threads_info.restype = ctypes.c_int

        self.dll.dbg_read_memory.argtypes = [
            ctypes.c_void_p,
            ctypes.c_uint64,
            ctypes.POINTER(ctypes.c_uint8),
            ctypes.c_uint32,
            ctypes.POINTER(ctypes.c_uint32),
        ]
        self.dll.dbg_read_memory.restype = ctypes.c_int

        self.dll.dbg_is_active.argtypes = [ctypes.c_void_p]
        self.dll.dbg_is_active.restype = ctypes.c_int

        self.dll.dbg_get_last_output.argtypes = [ctypes.c_void_p]
        self.dll.dbg_get_last_output.restype = ctypes.c_char_p

        self.dll.dbg_get_last_error.argtypes = [ctypes.c_void_p]
        self.dll.dbg_get_last_error.restype = ctypes.c_char_p

    # debugger session

    def create(self) -> int:
        handle = self.dll.dbg_create_session()
        if not handle:
            raise BackendError("dbg_create_session failed")
        return handle

    def destroy(self, handle: int) -> None:
        self.dll.dbg_destroy_session(handle)

    # debugger 操作
    
    def start_process(self, handle: int, target_exe: str, args_line: str = "") -> int:
        return self.dll.dbg_start_process(
            handle,
            target_exe.encode("utf-8"),
            args_line.encode("utf-8"),
        )

    def attach_process(self, handle: int, pid: int) -> int:
        return self.dll.dbg_attach_process(handle, ctypes.c_uint32(pid))

    def continue_exec(self, handle: int) -> int:
        return self.dll.dbg_continue(handle)

    def step_into(self, handle: int) -> int:
        return self.dll.dbg_step_into(handle)

    def step_over(self, handle: int) -> int:
        return self.dll.dbg_step_over(handle)

    def finish(self, handle: int) -> int:
        return self.dll.dbg_finish(handle)

    def quit(self, handle: int) -> int:
        return self.dll.dbg_quit(handle)

    def set_int3_breakpoint(self, handle: int, address: int, kind: int = 0) -> int:
        return self.dll.dbg_set_int3_breakpoint(
            handle,
            ctypes.c_uint64(address),
            ctypes.c_uint32(kind),
        )

    def remove_breakpoint(self, handle: int, address: int) -> int:
        return self.dll.dbg_remove_breakpoint(handle, ctypes.c_uint64(address))

    def set_disassembly_count(self, handle: int, count: int) -> int:
        return self.dll.dbg_set_disassembly_count(handle, ctypes.c_uint32(count))

    # debugger 查詢

    def get_disassembly(self, handle: int) -> list[dict]:
        buf = (DbgInstructionView * MAX_RESULT_ITEMS)() # DbgInstructionView buf[MAX_RESULT_ITEMS] = {}
        count = ctypes.c_uint32(0)
        # dbg_get_disassembly(Debugger *dbg, dbgapi::InstructionView *out_items, uint32_t capacity, uint32_t *out_count)
        rc = self.dll.dbg_get_disassembly(handle, buf, ctypes.c_uint32(MAX_RESULT_ITEMS), ctypes.byref(count))
        if rc == -1:
            raise BackendError("dbg_get_disassembly failed")

        # out 不能超過 buf 大小
        available_count = min(int(count.value), MAX_RESULT_ITEMS)
        if available_count == 0:
            return []

        # 轉回 dict list
        out: list[dict] = []
        for i in range(available_count):
            item = buf[i]
            out.append(
                {
                    "address": int(item.address),
                    "size": int(item.size),
                    "text": self._decode_cstr(bytes(item.text)),
                    "bytes": self._decode_cstr(bytes(item.bytes)),
                    "is_current": bool(item.is_current),
                    "kind": int(item.kind),
                }
            )
        return out

    def get_breakpoints(self, handle: int) -> list[dict]:
        buf = (DbgBreakpointView * MAX_RESULT_ITEMS)()
        count = ctypes.c_uint32(0)
        rc = self.dll.dbg_get_breakpoints(handle, buf, ctypes.c_uint32(MAX_RESULT_ITEMS), ctypes.byref(count))
        if rc == -1:
            raise BackendError("dbg_get_breakpoints failed")

        available_count = min(int(count.value), MAX_RESULT_ITEMS)
        if available_count == 0:
            return []

        out: list[dict] = []
        for i in range(available_count):
            item = buf[i]
            out.append(
                {
                    "address": int(item.address),
                    "hit_count": int(item.hit_count),
                    "kind": int(item.kind),
                    "enabled": bool(item.enabled),
                }
            )
        return out

    def get_memory_regions(self, handle: int) -> list[dict]:
        buf = (DbgMemoryRegionView * MAX_RESULT_ITEMS)()
        count = ctypes.c_uint32(0)
        rc = self.dll.dbg_get_memory_regions(handle, buf, ctypes.c_uint32(MAX_RESULT_ITEMS), ctypes.byref(count))
        if rc == -1:
            raise BackendError("dbg_get_memory_regions failed")

        available_count = min(int(count.value), MAX_RESULT_ITEMS)
        if available_count == 0:
            return []

        out: list[dict] = []
        for i in range(available_count):
            item = buf[i]
            out.append(
                {
                    "base": int(item.base),
                    "size": int(item.size),
                    "state": int(item.state),
                    "protect": int(item.protect),
                    "type": int(item.type),
                    "info": self._decode_cstr(bytes(item.info)),
                }
            )
        return out

    def get_registers(self, handle: int) -> list[dict]:
        buf = (DbgRegisterView * MAX_RESULT_ITEMS)()
        count = ctypes.c_uint32(0)
        rc = self.dll.dbg_get_registers(handle, buf, ctypes.c_uint32(MAX_RESULT_ITEMS), ctypes.byref(count))
        if rc == -1:
            raise BackendError("dbg_get_registers failed")

        available_count = min(int(count.value), MAX_RESULT_ITEMS)
        if available_count == 0:
            return []

        out: list[dict] = []
        for i in range(available_count):
            item = buf[i]
            out.append(
                {
                    "name": self._decode_cstr(bytes(item.name)),
                    "value": int(item.value),
                }
            )
        return out

    def get_threads_info(self, handle: int) -> list[dict]:
        buf = (DbgThreadInfoView * MAX_RESULT_ITEMS)()
        count = ctypes.c_uint32(0)
        rc = self.dll.dbg_get_threads_info(handle, buf, ctypes.c_uint32(MAX_RESULT_ITEMS), ctypes.byref(count))
        if rc == -1:
            raise BackendError("dbg_get_threads_info failed")

        available_count = min(int(count.value), MAX_RESULT_ITEMS)
        if available_count == 0:
            return []

        # 同個 tid 會有多筆資料
        grouped: dict[int, dict] = {}
        for i in range(available_count):
            item = buf[i]
            tid = int(item.tid)
            thread = grouped.setdefault(
                tid,
                {
                    "tid": tid,
                    "teb": int(item.teb),
                    "alive": bool(item.alive),
                    "suspended": bool(item.suspended),
                    "is_current": bool(item.is_current),
                    "callstack": [],
                },
            )

            frame_index = int(item.frame_index)
            callstack_address = int(item.callstack_address)
            if frame_index != 0xFFFFFFFF and callstack_address != 0:
                thread["callstack"].append((frame_index, callstack_address))

        out: list[dict] = []
        # 以 tid 順序排 callstack
        for tid in sorted(grouped.keys()):
            thread = grouped[tid]
            # 以 frame_index 順序排 callstack
            thread["callstack"].sort(key=lambda x: x[0])
            thread["callstack"] = [addr for _, addr in thread["callstack"]] 
            out.append(thread)
        return out

    def read_memory(self, handle: int, address: int, size: int) -> bytes:
        if size <= 0:
            return b""
        out_read = ctypes.c_uint32(0)
        buf = (ctypes.c_uint8 * size)()
        rc = self.dll.dbg_read_memory(
            handle,
            ctypes.c_uint64(address),
            buf,
            ctypes.c_uint32(size),
            ctypes.byref(out_read),
        )
        if rc == -1:
            raise BackendError("dbg_read_memory failed")
        return bytes(buf[: out_read.value])

    def is_active(self, handle: int) -> bool:
        return bool(self.dll.dbg_is_active(handle))

    def output(self, handle: int) -> str:
        raw = self.dll.dbg_get_last_output(handle)
        return raw.decode("utf-8", errors="replace") if raw else ""

    def error(self, handle: int) -> str:
        raw = self.dll.dbg_get_last_error(handle)
        return raw.decode("utf-8", errors="replace") if raw else ""

    # helper 
    
    def _decode_cstr(self, raw: bytes) -> str:
        return raw.split(b"\x00", 1)[0].decode("utf-8", errors="replace")
