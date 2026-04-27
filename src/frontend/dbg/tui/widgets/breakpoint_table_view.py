from __future__ import annotations

from rich.text import Text
from textual.widgets import RichLog

from dbg.tui.style.colors import CAT_OVERLAY0, CAT_RED, CAT_SUBTEXT1, CAT_TEXT

TITLE = "Breakpoints"

COLOR_TITLE = CAT_RED
COLOR_HEADER = CAT_SUBTEXT1
COLOR_ROW = CAT_TEXT
COLOR_EMPTY = CAT_OVERLAY0

ADDRESS_COL = 18
HIT_COL = 6


class BreakpointTableView(RichLog):
    can_focus = True

    def __init__(self, *, id: str | None = None) -> None:
        super().__init__(
            id=id,
            wrap=False,
            markup=False,
            highlight=False,
            max_lines=600,
            auto_scroll=False,
        )
        self.write(Text(TITLE, style=f"bold {COLOR_TITLE}"))

    def set_data(self, breakpoints: list[dict]) -> None:
        self.clear()
        self.write(Text(TITLE, style=f"bold {COLOR_TITLE}"))
        self.write(Text(f"{'Address':<{ADDRESS_COL}}  {'Hit':<{HIT_COL}}", style=COLOR_HEADER))
        for item in breakpoints:
            if int(item["kind"]) != 0:
                continue
            address = int(item["address"])
            hit = int(item["hit_count"])
            self.write(Text(f"0x{address:016X}  {hit:<{HIT_COL}}", style=COLOR_ROW))