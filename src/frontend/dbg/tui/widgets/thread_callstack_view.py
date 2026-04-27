from __future__ import annotations

from rich.text import Text
from textual.widgets import RichLog

from dbg.tui.style.colors import CAT_BLUE, CAT_GREEN, CAT_TEXT, CAT_YELLOW

TITLE = "Threads / Callstack"

COLOR_TITLE = CAT_GREEN
COLOR_CURRENT_THREAD = CAT_BLUE
COLOR_THREAD = CAT_TEXT
COLOR_FRAME = CAT_YELLOW


class ThreadCallstackView(RichLog):
    can_focus = True

    def __init__(self, *, id: str | None = None) -> None:
        super().__init__(
            id=id,
            wrap=False,
            markup=False,
            highlight=False,
            max_lines=1200,
            auto_scroll=False,
        )
        self.write(Text(TITLE, style=f"bold {COLOR_TITLE}"))

    def set_data(self, threads_info: list[dict]) -> None:
        self.clear()
        self.write(Text(TITLE, style=f"bold {COLOR_TITLE}"))

        for thread in threads_info:
            tid = int(thread["tid"])
            is_current = bool(thread["is_current"])
            marker = "*" if is_current else " "
            self.write(Text(f"{marker} tid={tid}", style=COLOR_CURRENT_THREAD if is_current else COLOR_THREAD))

            callstack = thread["callstack"]
            for depth, address in enumerate(callstack):
                self.write(Text(f"  #{depth:<2} 0x{int(address):016X}", style=COLOR_FRAME))