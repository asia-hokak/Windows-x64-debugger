from __future__ import annotations

from rich.text import Text
from textual.widgets import RichLog

from dbg.tui.style.colors import CAT_BLUE, CAT_MAUVE, CAT_TEAL, CAT_GREEN, CAT_YELLOW, CAT_OVERLAY0

TITLE = "Memory"

COLOR_TITLE = CAT_YELLOW
COLOR_ADDRESS = CAT_BLUE
COLOR_GROUP_0 = CAT_MAUVE
COLOR_GROUP_1 = CAT_TEAL
COLOR_GROUP_2 = CAT_GREEN
COLOR_GROUP_3 = CAT_YELLOW
COLOR_SEPARATOR = CAT_OVERLAY0

GROUP_COLORS = (COLOR_GROUP_0, COLOR_GROUP_1, COLOR_GROUP_2, COLOR_GROUP_3)


class MemoryView(RichLog):
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
        self._row_size = 16
        self._rows = 16
        self.write(Text(TITLE, style=f"bold {COLOR_TITLE}"))

    def set_data(self, address: int, data: bytes) -> None:
        self.clear()
        self.write(Text(f"{TITLE} @ 0x{address:X}", style=f"bold {COLOR_TITLE}"))
        for row in range(self._rows):
            row_address = address + row * self._row_size # 每行的 address
            start = row * self._row_size # data 取值的起始點
            valid_size = 0
            if start < len(data):
                valid_size = min(self._row_size, len(data) - start)
            self.write(self._format_memory_row(row_address, data[start:start + self._row_size], valid_size))

    def _format_memory_row(self, address: int, row_data: bytes, valid_size: int) -> Text:
        if len(row_data) < self._row_size:
            row_data = row_data + (b"\x00" * (self._row_size - len(row_data)))

        text = Text()
        text.append(f"0x{address:016X}  ", style=COLOR_ADDRESS)
        # 一行 Memory 切成 4 * 4
        for index in range(4):
            start = index * 4
            take = 0
            if valid_size > start:
                take = min(4, valid_size - start)
            group = self._format_group(row_data[start:start + 4], take)
            text.append(group, style=GROUP_COLORS[index])
            if index < 3:
                text.append(" ", style=COLOR_SEPARATOR)
        return text

    @staticmethod
    def _format_group(chunk: bytes, valid_count: int) -> str:
        if valid_count <= 0: # 越界的讀取直接回傳 ????????
            return "????????"
        hex_part = "".join(f"{value:02X}" for value in chunk[:valid_count])
        if valid_count < 4:
            hex_part = hex_part + ("??" * (4 - valid_count))
        return hex_part
