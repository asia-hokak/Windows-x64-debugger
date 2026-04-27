from __future__ import annotations

from dataclasses import dataclass

from dbg.ffi.api import BackendApi
from dbg.ffi.errors import BackendError


@dataclass
class CommandResult:
    disasm_items: list[dict]
    memory_bytes: bytes
    memory_address: int
    registers: list[dict]
    message_lines: list[str]


@dataclass
class InfoResult:
    breakpoints: list[dict]
    threads_info: list[dict]
    memory_regions: list[dict]
    message_lines: list[str]


class DebugSessionClient:
    def __init__(self, api: BackendApi | None = None, dll_path: str | None = None) -> None:
        self.api = api if api is not None else BackendApi(dll_path=dll_path)
        self.handle = self.api.create()
        self._memory_address: int | None = None # memory view 的位置
        self._memory_rows = 16 # memory view 的長度

    def close(self) -> None:
        if self.handle:
            self.api.destroy(self.handle)
            self.handle = 0
    

    # 簡化 api 呼叫流程
    
    def execute(self, command: str) -> CommandResult:
        parts = command.strip().split()
        if not parts:
            return self.main_result("")

        cmd = parts[0].lower()
        if cmd in {"attach", "att"} and len(parts) >= 2:
            return self.attach(int(parts[1], 0))
        if cmd in {"c", "continue"}:
            return self.cont()
        if cmd in {"si", "s", "step"}:
            return self.step_into()
        if cmd in {"ni", "n", "next"}:
            return self.step_over()
        if cmd in {"fin", "finish"}:
            return self.finish()
        if cmd in {"q", "quit", "exit"}:
            return self.quit()
        if cmd in {"b", "bp"} and len(parts) >= 2:
            return self.set_bp(int(parts[1], 0))
        if cmd in {"bc"} and len(parts) >= 2:
            return self.clear_bp(int(parts[1], 0))
        if cmd in {"ctx", "win", "window"} and len(parts) >= 2:
            count = int(parts[1], 0)
            if count < 0:
                return self.main_result("count must be >= 0")
            return self.set_disasm_count(count)
        if cmd == "x" and len(parts) >= 2:
            return self.set_memory_address(int(parts[1], 0))
        return self.main_result(f"unknown command: {command}")
     
    @staticmethod
    def _get_register_value(registers: list[dict], *names: str) -> int | None:
        wanted = {name.lower() for name in names}
        for item in registers:
            if str(item["name"]).lower() in wanted:
                return int(item["value"])
        return None

    @staticmethod
    def _build_message_lines(output: str) -> list[str]:
        return [line.rstrip() for line in output.splitlines() if line.strip()]

    def _default_memory_address(self, registers: list[dict]) -> int:
        eip = self._get_register_value(registers, "eip")
        if eip is not None:
            return eip
        rip = self._get_register_value(registers, "rip")
        if rip is not None:
            return rip
        return 0

    def _read_memory_data(self, registers: list[dict]) -> tuple[int, bytes]:
        base = self._memory_address if self._memory_address is not None else self._default_memory_address(registers)
        if base < 0:
            base = 0
        self._memory_address = base

        line_size = 16
        total_size = self._memory_rows * line_size
        try:
            data = self.api.read_memory(self.handle, base, total_size)
        except BackendError:
            data = b""

        return base, data

    def main_result(self, message: str) -> CommandResult:
        disasm_items = self.get_disassembly()
        registers = self.get_registers()
        memory_address, memory_bytes = self._read_memory_data(registers)
        message_lines = self._build_message_lines(message)

        return CommandResult(
            disasm_items=disasm_items,
            memory_bytes=memory_bytes,
            memory_address=memory_address,
            registers=registers,
            message_lines=message_lines,
        )

    def info_result(self, message: str = "") -> InfoResult:
        memory_regions = self.get_memory_regions()
        threads_info = self.get_threads_info()
        breakpoints = self.get_breakpoints()
        message_lines = self._build_message_lines(message)

        return InfoResult(
            breakpoints=breakpoints,
            threads_info=threads_info,
            memory_regions=memory_regions,
            message_lines=message_lines,
        )

    # wrapper
    
    def _run(self, rc: int) -> CommandResult:
        output = self.api.output(self.handle).strip()
        if rc == -1:
            err = self.api.error(self.handle).strip() or output or "backend call failed"
            raise BackendError(err)
        return self.main_result(output)

    # 操作 api

    def start(self, target: str, args_line: str = "") -> CommandResult:
        rc = self.api.start_process(self.handle, target, args_line)
        return self._run(rc)

    def attach(self, pid: int) -> CommandResult:
        rc = self.api.attach_process(self.handle, pid)
        return self._run(rc)

    def cont(self) -> CommandResult:
        return self._run(self.api.continue_exec(self.handle))

    def step_into(self) -> CommandResult:
        return self._run(self.api.step_into(self.handle))

    def step_over(self) -> CommandResult:
        return self._run(self.api.step_over(self.handle))

    def finish(self) -> CommandResult:
        return self._run(self.api.finish(self.handle))

    def quit(self) -> CommandResult:
        return self._run(self.api.quit(self.handle))

    def set_bp(self, addr: int) -> CommandResult:
        return self._run(self.api.set_int3_breakpoint(self.handle, addr, 0))

    def clear_bp(self, addr: int) -> CommandResult:
        return self._run(self.api.remove_breakpoint(self.handle, addr))

    def set_disasm_count(self, count: int) -> CommandResult:
        return self._run(self.api.set_disassembly_count(self.handle, count))

    def set_memory_address(self, address: int) -> CommandResult:
        if address < 0:
            raise ValueError("address must be >= 0")
        self._memory_address = address
        return self.main_result(f"memory view @ 0x{address:X}")

    
    # 查詢 api
    
    def get_disassembly(self) -> list[dict]:
        return self.api.get_disassembly(self.handle)

    def get_breakpoints(self) -> list[dict]:
        return self.api.get_breakpoints(self.handle)

    def get_memory_regions(self) -> list[dict]:
        return self.api.get_memory_regions(self.handle)

    def get_registers(self) -> list[dict]:
        return self.api.get_registers(self.handle)

    def get_threads_info(self) -> list[dict]:
        return self.api.get_threads_info(self.handle)

    def is_active(self) -> bool:
        return self.api.is_active(self.handle)
