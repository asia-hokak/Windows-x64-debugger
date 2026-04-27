from __future__ import annotations

from rich.text import Text
from textual.widgets import RichLog

from dbg.tui.style.colors import (
    CAT_BLUE,
    CAT_GREEN,
    CAT_MAUVE,
    CAT_OVERLAY0,
    CAT_OVERLAY1,
    CAT_PEACH,
    CAT_PINK,
    CAT_SKY,
    CAT_SUBTEXT1,
    CAT_SURFACE1,
    CAT_SURFACE2,
    CAT_TEAL,
    CAT_TEXT,
    CAT_YELLOW,
)

TITLE = "Memory Regions"

COLOR_TITLE = CAT_PEACH
COLOR_HEADER = CAT_SUBTEXT1
COLOR_SEPARATOR = CAT_SURFACE1
COLOR_EMPTY = CAT_OVERLAY0
COLOR_FALLBACK = CAT_TEXT
COLOR_BASE = CAT_BLUE
COLOR_SIZE = CAT_PINK
COLOR_STATE = CAT_TEAL
COLOR_TYPE = CAT_MAUVE
COLOR_INFO_DEFAULT = CAT_SUBTEXT1
COLOR_ROWS = CAT_OVERLAY1
COLOR_GAP_EVEN = CAT_SURFACE2
COLOR_GAP_ODD = CAT_SURFACE1
COLOR_PERM_EXEC = CAT_YELLOW
COLOR_PERM_WRITE = CAT_GREEN
COLOR_PERM_READ = CAT_SKY
COLOR_INFO_TEXT = CAT_BLUE
COLOR_INFO_RDATA = CAT_TEAL
COLOR_INFO_DATA = CAT_GREEN
COLOR_INFO_STACK = CAT_YELLOW
COLOR_INFO_HEAP = CAT_PINK
COLOR_INFO_MODULE = CAT_TEXT

ADDRESS_COL = 18
SIZE_COL = 10
PERM_COL = 4
STATE_COL = 9
TYPE_COL = 8

STATE_MAP = {
    0x00001000: "COMMIT",
    0x00002000: "RESERVE",
    0x00010000: "FREE",
}

TYPE_MAP = {
    0x00000000: "NONE",
    0x00020000: "PRIVATE",
    0x00040000: "MAPPED",
    0x01000000: "IMAGE",
}

HEADER_TEXT = f"{'Base':<{ADDRESS_COL}}  {'Size':<{SIZE_COL}}  {'Perm':<{PERM_COL}}  {'State':<{STATE_COL}}  {'Type':<{TYPE_COL}}  Info"
SEPARATOR_TEXT = "-" * (ADDRESS_COL + SIZE_COL + PERM_COL + STATE_COL + TYPE_COL + 14)

class MemoryRegionsView(RichLog):
    can_focus = True

    def __init__(self, *, id: str | None = None) -> None:
        super().__init__(
            id=id,
            wrap=False,
            markup=False,
            highlight=False,
            max_lines=2000,
            auto_scroll=False,
        )
        self.set_data([])

    def set_data(self, regions: list[dict]) -> None:
        self.clear()
        self.write(Text(TITLE, style=f"bold {COLOR_TITLE}"))
        self.write(Text(HEADER_TEXT, style=COLOR_HEADER))
        self.write(Text(SEPARATOR_TEXT, style=COLOR_SEPARATOR))

        row_index = 0
        for region in regions:
            self.write(self._format_row(region, row_index))

        self.write(Text(SEPARATOR_TEXT, style=COLOR_SEPARATOR))

    def _should_render_region(self, region: dict) -> bool:
        # 不顯示 state 是 FREE 或者 type 是 NONE
        state = int(region["state"])
        region_type = int(region["type"])
        if state == 0x00010000:
            return False
        if region_type == 0:
            return False
        return True

    def _format_row(self, region: dict, row_index: int) -> Text:
        # 單行 format
        base = int(region["base"])
        size = int(region["size"])
        protect = int(region["protect"])
        state = int(region["state"])
        region_type = int(region["type"])
        info = str(region["info"]).strip() or "-"

        perm = self._format_perm_text(protect)
        state_text = self._format_state_text(state)
        type_text = self._format_type_text(region_type)
        base_text = f"0x{base:016X}"
        size_text = f"0x{size:08X}"

        perm_style = self._perm_style(perm)
        info_style = self._info_style(info)
        gap_style = COLOR_GAP_EVEN if (row_index % 2 == 0) else COLOR_GAP_ODD

        text = Text()
        text.append(self._fit(base_text, ADDRESS_COL), style=COLOR_BASE)
        text.append("  ", style=gap_style)
        text.append(self._fit(size_text, SIZE_COL), style=COLOR_SIZE)
        text.append("  ", style=gap_style)
        text.append(self._fit(perm, PERM_COL), style=perm_style)
        text.append("  ", style=gap_style)
        text.append(self._fit(state_text, STATE_COL), style=COLOR_STATE)
        text.append("  ", style=gap_style)
        text.append(self._fit(type_text, TYPE_COL), style=COLOR_TYPE)
        text.append("  ", style=gap_style)
        text.append(info, style=info_style)
        return text

    # === enum 轉回數字 ===
    def _format_perm_text(self, protect: int) -> str:
        base_protect = protect & 0xFF
        '''
        PAGE_NOACCESS          = 0x01
        PAGE_READONLY          = 0x02
        PAGE_READWRITE         = 0x04
        PAGE_WRITECOPY         = 0x08
        PAGE_EXECUTE           = 0x10
        PAGE_EXECUTE_READ      = 0x20
        PAGE_EXECUTE_READWRITE = 0x40
        PAGE_EXECUTE_WRITECOPY = 0x80
        '''
        can_read = base_protect in {0x02, 0x04, 0x08, 0x20, 0x40, 0x80}
        can_write = base_protect in {0x04, 0x08, 0x40, 0x80}
        can_exec = base_protect in {0x10, 0x20, 0x40, 0x80}
        return f"{'r' if can_read else '-'}{'w' if can_write else '-'}{'x' if can_exec else '-'}"

    def _format_state_text(self, state: int) -> str:
        name = STATE_MAP[state]
        return f"{name}"

    def _format_type_text(self, region_type: int) -> str:
        name = TYPE_MAP[region_type]
        return f"{name}"

    # === style ===
    def _perm_style(self, perm: str) -> str:
        value = perm.lower()
        if "x" in value:
            return f"bold {COLOR_PERM_EXEC}"
        if "w" in value:
            return f"bold {COLOR_PERM_WRITE}"
        if "r" in value:
            return f"bold {COLOR_PERM_READ}"
        return COLOR_EMPTY

    def _info_style(self, info: str) -> str:
        value = info.lower()
        if ".text" in value:
            return COLOR_INFO_TEXT
        if ".rdata" in value:
            return COLOR_INFO_RDATA
        if ".data" in value or ".bss" in value:
            return COLOR_INFO_DATA
        if "stack" in value:
            return COLOR_INFO_STACK
        if "heap" in value:
            return COLOR_INFO_HEAP
        if ".dll" in value or ".exe" in value:
            return COLOR_INFO_MODULE
        return COLOR_INFO_DEFAULT

    def _fit(self, value: str, width: int) -> str:
        if len(value) > width:
            if width <= 1:
                return value[:width]
            return value[: width - 1] + "."
        return f"{value:<{width}}"
