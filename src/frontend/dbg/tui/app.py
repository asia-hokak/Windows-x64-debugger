from __future__ import annotations

import argparse
import pathlib
import sys
from collections.abc import Callable

from textual.app import App, ComposeResult
from textual.containers import Horizontal, Vertical
from textual.widgets import Footer, Header, Input

from dbg.client.session import CommandResult, DebugSessionClient, InfoResult
from dbg.ffi.errors import BackendError, DllLoadError
from dbg.tui.widgets.command_input import CommandInput
from dbg.tui.widgets.disasm_view import DisasmView
from dbg.tui.widgets.log_view import LogView
from dbg.tui.widgets.memory_view import MemoryView
from dbg.tui.widgets.breakpoint_table_view import BreakpointTableView
from dbg.tui.widgets.memory_regions_view import MemoryRegionsView
from dbg.tui.widgets.register_view import RegisterView
from dbg.tui.widgets.thread_callstack_view import ThreadCallstackView


MAIN_SCREEN_ID = "#main-screen"
INSPECT_SCREEN_ID = "#inspect-screen"


class DbgApp(App):
    CSS_PATH = "style/app.tcss"
    BINDINGS = [
        ("ctrl+k", "next_screen", "Next Screen"),
    ]

    def __init__(self, debugger: DebugSessionClient, target: str, target_args: str, attach_pid: int | None = None) -> None:
        super().__init__()
        self.debugger = debugger # debugger
        self.target = target # binary path
        self.target_args = target_args # binary args
        self.attach_pid = attach_pid #
        self.last_command = ""
        self._screen_ids = (MAIN_SCREEN_ID, INSPECT_SCREEN_ID)
        self._active_screen_id = MAIN_SCREEN_ID

    def compose(self) -> ComposeResult:
        yield Header(show_clock=True)
        with Vertical(id="root-layout"):
            with Vertical(id="main-screen"):
                with Horizontal(id="top-pane"):
                    with Vertical(id="left-pane"):
                        yield DisasmView(id="disasm-view")
                        yield MemoryView(id="memory-view")
                    yield RegisterView(id="register-view")
                yield CommandInput(id="command-input-main")
            with Vertical(id="inspect-screen"):
                with Horizontal(id="inspect-pane"):
                    with Vertical(id="inspect-left-pane"):
                        yield BreakpointTableView(id="breakpoint-table-view")
                        yield ThreadCallstackView(id="thread-callstack-view")
                    yield MemoryRegionsView(id="memory-regions-view")
            yield LogView(id="log-view")
        yield Footer()

    # lifecycle
    
    def on_mount(self) -> None:
        # 啟動時
        self._apply_screen()
        self._start_debug_session()

    def on_unmount(self) -> None:
        # 關閉時
        try:
            self.debugger.close()
        except Exception:
            pass

    # event
    
    def on_input_submitted(self, message: Input.Submitted) -> None:
        # 處理 input 事件
        if message.input.id != "command-input-main":
            return

        command = message.value.strip() or self.last_command
        message.input.value = "" # 清空 input
        if not command:
            return

        self.last_command = command
        self._run_command(command)

    def action_next_screen(self) -> None:
        # 切到另一個 screen，先改 id 後渲染
        if self._active_screen_id == MAIN_SCREEN_ID:
            self._active_screen_id = INSPECT_SCREEN_ID
        else:
            self._active_screen_id = MAIN_SCREEN_ID
        self._apply_screen()

    # backend 執行
    
    def _start_debug_session(self) -> None:
        # 透過 attach 或 start prcoess 建立 session
        if self.attach_pid is not None:
            self._append_log([f"[ui] attach: pid={self.attach_pid}"])
            result = self._run_backend(lambda: self.debugger.attach(self.attach_pid))
        else:
            launch_text = f"[ui] launch: {self.target} {self.target_args}".rstrip()
            self._append_log([launch_text])
            result = self._run_backend(lambda: self.debugger.start(self.target, self.target_args))
        if result is None:
            return

        self._refresh_main_view(result)

    def _run_command(self, command: str, exit_after: bool = False) -> None:
        # 執行指令
        lowered = command.strip().lower()
        if lowered in {"q", "quit", "exit"}:
            result = self._run_command_backend(lambda: self.debugger.quit())
            exit_after = True
        else:
            result = self._run_command_backend(lambda: self.debugger.execute(command))
        if result is None:
            return

        self._refresh_main_view(result)
        if exit_after:
            self.exit()

    def _run_backend(self, operation: Callable[[], CommandResult | InfoResult]) -> CommandResult | InfoResult | None:
        # 執行 api 時用經過這個 wrapper (統一處理 fallback)
        # 可以攔截 BackendError 和 Command 解析失敗的 Error，並 append 到 log
        try:
            return operation()
        except BackendError as exc:
            self._append_log([f"[error] {exc}"])
            return None

    def _run_command_backend(self, operation: Callable[[], CommandResult]) -> CommandResult | None:
        # 下 command 後有執行 api 時用經過這個 wrapper (統一處理 fallback)
        # 可以攔截 BackendError 和 Command 解析失敗的 Error，並 append 到 log
        try:
            return operation()
        except BackendError as exc:
            self._append_log([f"[error] {exc}"])
            return None
        except (ValueError, IndexError) as exc:
            self._append_log([f"[parse-error] {exc}"])
            return None

    # screen
    
    def _apply_screen(self) -> None:
        # 根據 screen id 渲染
        for screen_id in self._screen_ids:
            container = self.query_one(screen_id, Vertical) # 依照 id 找到 container
            container.styles.display = "block" if screen_id == self._active_screen_id else "none" # block 代表顯示
        if self._active_screen_id == INSPECT_SCREEN_ID:
            self._refresh_info_view() # 重新拿 info view 的資料

    def _append_log(self, lines: list[str]) -> None:
        # 新增訊息到 log
        self.query_one("#log-view", LogView).append_data(lines)

    def _refresh_main_view(self, result: CommandResult) -> None:
        # 拿 main view 資料
        self.query_one("#disasm-view", DisasmView).set_data(result.disasm_items)
        self.query_one("#register-view", RegisterView).set_data(result.registers)
        self.query_one("#memory-view", MemoryView).set_data(result.memory_address, result.memory_bytes)
        if result.message_lines:
            self._append_log(result.message_lines)

    def _refresh_info_view(self) -> None:
        # 拿 info view 資料
        result = self._run_backend(self.debugger.info_result)
        if result is None:
            return
        self.query_one("#breakpoint-table-view", BreakpointTableView).set_data(result.breakpoints)
        self.query_one("#thread-callstack-view", ThreadCallstackView).set_data(result.threads_info)
        self.query_one("#memory-regions-view", MemoryRegionsView).set_data(result.memory_regions)
        if result.message_lines:
            self._append_log(result.message_lines)



def run_app() -> int:
    parser = argparse.ArgumentParser(description="dbg textual TUI")
    parser.add_argument("target", nargs="?", default="", help="Path to target executable")
    parser.add_argument("target_args", nargs="*", help="Arguments for target executable")
    parser.add_argument("--dll", dest="dll_path", default=None, help="Path to dbg_backend.dll")
    parser.add_argument("--attach", dest="attach_pid", type=int, default=None, help="Attach to process ID")
    args = parser.parse_args()

    if args.attach_pid is None and not args.target:
        print("[error] target is required unless --attach is provided")
        return 1
    if args.attach_pid is not None and args.target:
        print("[error] use either target launch or --attach, not both")
        return 1

    try:
        debugger = DebugSessionClient(dll_path=args.dll_path)
    except (BackendError, DllLoadError) as exc:
        print(f"[error] {exc}")
        return 1

    app = DbgApp(
        debugger=debugger,
        target=args.target,
        target_args=" ".join(args.target_args),
        attach_pid=args.attach_pid,
    )
    app.run()
    return 0
