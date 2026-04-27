from __future__ import annotations

from rich.text import Text
from textual.widgets import RichLog

from dbg.tui.style.colors import CAT_BLUE, CAT_GREEN, CAT_MAUVE, CAT_PEACH, CAT_RED, CAT_SUBTEXT1, CAT_TEAL, CAT_TEXT

TITLE = "LOG"

COLOR_TITLE = CAT_TEAL
COLOR_DEFAULT = CAT_SUBTEXT1
COLOR_SUFFIX = CAT_TEXT

PREFIX_COLORS = {
    "[ui]": CAT_BLUE,
    "[backend]": CAT_MAUVE,
    "[target]": CAT_GREEN,
    "[error]": CAT_RED,
    "[parse-error]": CAT_PEACH,
}


class LogView(RichLog):
    can_focus = True

    def __init__(self, *, id: str | None = None) -> None:
        super().__init__(
            id=id,
            wrap=False,
            markup=False,
            highlight=False,
            max_lines=500,
            auto_scroll=True,
        )
        self.write(Text(TITLE, style=f"bold {COLOR_TITLE}"))

    def append_data(self, lines: list[str]) -> None:
        if not lines:
            return
        for line in lines:
            safe_text = "" if line is None else str(line)
            self.write(self._format_log_text(safe_text))
        self.scroll_end(animate=False)

    def set_data(self, lines: list[str]) -> None:
        self.clear()
        self.write(Text(TITLE, style=f"bold {COLOR_TITLE}"))
        self.append_data(lines)

    def _format_log_text(self, line: str) -> Text:
        for prefix, color in PREFIX_COLORS.items():
            if line.startswith(prefix):
                text = Text()
                text.append(prefix, style=f"bold {color}")
                tail = line[len(prefix):]
                if tail:
                    text.append(tail, style=COLOR_SUFFIX)
                return text
        return Text(line, style=COLOR_DEFAULT)
