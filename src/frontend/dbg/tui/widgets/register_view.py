from __future__ import annotations

from rich.text import Text
from textual.widgets import RichLog

from dbg.tui.style.colors import (
    CAT_BLUE,
    CAT_FLAMINGO,
    CAT_LAVENDER,
    CAT_OVERLAY0,
    CAT_OVERLAY1,
    CAT_PEACH,
    CAT_PINK,
    CAT_RED,
    CAT_SKY,
    CAT_SURFACE2,
    CAT_TEXT,
    CAT_YELLOW,
    CAT_GREEN,
)

TITLE = "Registers"

COLOR_TITLE = CAT_RED
COLOR_SECTION_TEXT = CAT_LAVENDER
COLOR_SECTION_LINE = CAT_SURFACE2
COLOR_NAME_GENERAL = CAT_BLUE
COLOR_NAME_POINTER = CAT_PINK
COLOR_NAME_FLAGS = CAT_YELLOW
COLOR_VALUE_DEFAULT = CAT_TEXT
COLOR_VALUE_IP = CAT_PEACH
COLOR_VALUE_SP_BP = CAT_FLAMINGO
COLOR_VALUE_FLAGS = CAT_YELLOW
COLOR_FLAG_LABEL = CAT_SKY
COLOR_FLAG_ON = CAT_GREEN
COLOR_FLAG_OFF = CAT_OVERLAY0
COLOR_GAP = CAT_OVERLAY1
COLOR_EMPTY = CAT_OVERLAY0

X64_GENERAL_REGS = (
    "rax",
    "rbx",
    "rcx",
    "rdx",
    "rsi",
    "rdi",
    "r8",
    "r9",
    "r10",
    "r11",
    "r12",
    "r13",
    "r14",
    "r15",
)
X64_POINTER_REGS = ("rbp", "rsp", "rip")

X86_GENERAL_REGS = ("eax", "ebx", "ecx", "edx", "esi", "edi")
X86_POINTER_REGS = ("ebp", "esp", "eip")

POINTER_REGS = {"rip", "eip", "rsp", "esp", "rbp", "ebp"}
FLAGS_REGS = {"rflags", "eflags"}
IP_REGS = {"rip", "eip"}
SP_BP_REGS = {"rsp", "esp", "rbp", "ebp"}

FLAG_ROWS = (
    (("CF", 0), ("PF", 2), ("AF", 4), ("ZF", 6), ("SF", 7)),
    (("TF", 8), ("IF", 9), ("DF", 10), ("OF", 11)),
)


class RegisterView(RichLog):
    can_focus = True

    def __init__(self, *, id: str | None = None) -> None:
        super().__init__(
            id=id,
            wrap=False,
            markup=False,
            highlight=False,
            max_lines=300,
            auto_scroll=False,
        )
        self.write(Text(TITLE, style=f"bold {COLOR_TITLE}"))

    def set_data(self, registers: list[dict]) -> None:
        self.clear()
        self.write(Text(TITLE, style=f"bold {COLOR_TITLE}"))
        if not registers:
            self.write(Text("(no data)", style=COLOR_EMPTY))
            return

        reg_map = self._to_map(registers)
        is_x86 = "eip" in reg_map or "esp" in reg_map
        if is_x86:
            self._render_arch(reg_map, X86_GENERAL_REGS, X86_POINTER_REGS, "eflags", 8)
            return
        self._render_arch(reg_map, X64_GENERAL_REGS, X64_POINTER_REGS, "rflags", 16)

    def set_registers(self, registers: list[dict]) -> None:
        self.set_data(registers)

    @staticmethod
    def _to_map(registers: list[dict]) -> dict[str, int]:
        out: dict[str, int] = {}
        for item in registers:
            name = str(item["name"]).lower()
            if not name:
                continue
            out[name] = int(item["value"])
        return out

    def _render_arch(
        self,
        reg_map: dict[str, int],
        general_regs: tuple[str, ...],
        pointer_regs: tuple[str, ...],
        flags_name: str,
        width_hex: int,
    ) -> None:
        self._write_section("General")
        for name in general_regs:
            value = reg_map[name]
            self.write(self._render_register_line(name, value, width_hex))

        self.write(Text(""))
        self._write_section("Pointers")
        for name in pointer_regs:
            value = reg_map[name]
            self.write(self._render_register_line(name, value, width_hex))

        self.write(Text(""))
        self._write_section("Flags")
        if flags_name == "rflags":
            flags = int(reg_map["rflags"])
        else:
            flags = int(reg_map["eflags"])
        self.write(self._render_register_line(flags_name, flags, width_hex))
        self._render_flags(flags)

    def _render_register_line(self, name: str, value: int, width_hex: int) -> Text:
        if name in POINTER_REGS:
            name_style = f"bold {COLOR_NAME_POINTER}"
        elif name in FLAGS_REGS:
            name_style = f"bold {COLOR_NAME_FLAGS}"
        else:
            name_style = f"bold {COLOR_NAME_GENERAL}"

        if name in IP_REGS:
            value_style = f"bold {COLOR_VALUE_IP}"
        elif name in SP_BP_REGS:
            value_style = COLOR_VALUE_SP_BP
        elif name in FLAGS_REGS:
            value_style = COLOR_VALUE_FLAGS
        else:
            value_style = COLOR_VALUE_DEFAULT

        line = Text()
        line.append(f"{name.upper():<7}", style=name_style)
        line.append(" ", style=COLOR_GAP)
        line.append(f"0x{value:0{width_hex}X}", style=value_style)
        return line

    def _write_section(self, title: str) -> None:
        line = Text()
        line.append("-- ", style=COLOR_SECTION_LINE)
        line.append(title, style=f"bold {COLOR_SECTION_TEXT}")
        line.append(" ", style=COLOR_SECTION_LINE)
        line.append("-" * 18, style=COLOR_SECTION_LINE)
        self.write(line)

    def _render_flags(self, flags: int) -> None:
        for row in FLAG_ROWS:
            line = Text()
            for name, bit in row:
                enabled = 1 if ((flags >> bit) & 1) else 0
                line.append(f"{name} ", style=f"underline {COLOR_FLAG_LABEL}")
                style = f"bold {COLOR_FLAG_ON}" if enabled else COLOR_FLAG_OFF
                line.append(f"{enabled}  ", style=style)
            self.write(line)
