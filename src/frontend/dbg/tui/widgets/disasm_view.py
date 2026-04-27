from __future__ import annotations

from rich.text import Text
from textual.widgets import RichLog

from dbg.tui.style.colors import (
    CAT_BASE,
    CAT_BLUE,
    CAT_GREEN,
    CAT_MAUVE,
    CAT_OVERLAY0,
    CAT_OVERLAY2,
    CAT_PEACH,
    CAT_RED,
    CAT_SAPPHIRE,
    CAT_SUBTEXT1,
    CAT_TEXT,
    CAT_YELLOW,
)

TITLE = "Disassembly"

COLOR_TITLE = CAT_BLUE
COLOR_EMPTY = CAT_OVERLAY0
COLOR_TEXT = CAT_TEXT
COLOR_MARKER = f"bold {CAT_SUBTEXT1}"
COLOR_CURRENT_MARKER = f"bold {CAT_BASE} on {CAT_YELLOW}"
COLOR_ADDRESS = CAT_SAPPHIRE
COLOR_BYTES = CAT_OVERLAY0

ADDRESS_HEX_WIDTH = 16
BYTES_COLUMN_WIDTH = 47

INSTRUCTION_KIND_COLOR = {
    0: CAT_RED,  # Invalid
    1: CAT_OVERLAY2,  # Unknown
    2: CAT_TEXT,  # Normal
    3: CAT_BLUE,  # Call
    4: CAT_PEACH,  # Jump
    5: CAT_YELLOW,  # ConditionalJump
    6: CAT_GREEN,  # Return
    7: CAT_MAUVE,  # Interrupt
}


class DisasmView(RichLog):
    can_focus = True

    def __init__(self, *, id: str | None = None) -> None:
        super().__init__(
            id=id,
            wrap=False,
            markup=False,
            highlight=False,
            max_lines=1000,
            auto_scroll=False,
        )
        self.write(Text(TITLE, style=f"bold {COLOR_TITLE}"))

    def set_data(self, items: list[dict]) -> None:
        self.clear()
        self.write(Text(TITLE, style=f"bold {COLOR_TITLE}"))
        if not items:
            self.write(Text("(no data)", style=COLOR_EMPTY))
            return

        current_row = -1
        for index, item in enumerate(items):
            if bool(item["is_current"]):
                current_row = index
            self.write(self._format_instruction(item))

        if current_row >= 0:
            self.scroll_to(y=max(0, current_row), animate=False)

    def _format_instruction(self, item: dict) -> Text:
        address = int(item["address"])
        bytes_text = str(item["bytes"])
        instruction_text = str(item["text"])
        is_current = bool(item["is_current"])
        kind = int(item["kind"])

        # 在 rip 處標註箭頭表示下次執行的 insturtion
        marker = "=> " if is_current else "   "
        marker_style = COLOR_CURRENT_MARKER if is_current else COLOR_MARKER

        instruction_color = INSTRUCTION_KIND_COLOR[kind]
        if is_current:
            instruction_color = f"bold {instruction_color}"

        line = Text()
        line.append(marker, style=marker_style)
        line.append(f"0x{address:0{ADDRESS_HEX_WIDTH}x}  ", style=COLOR_ADDRESS)
        line.append(f"{bytes_text:<{BYTES_COLUMN_WIDTH}} ", style=COLOR_BYTES)
        line.append(instruction_text, style=instruction_color)
        return line
