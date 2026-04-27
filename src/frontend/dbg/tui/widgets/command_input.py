from __future__ import annotations

from textual.widgets import Input


class CommandInput(Input):
    # BINDING 屬於
    BINDINGS = [("escape", "unfocus")]

    def __init__(self, *, id: str | None = None) -> None:
        super().__init__(placeholder="Enter debugger command (c/si/ni/fin/b/bc/x/q)", id=id)

    def action_unfocus(self) -> None:
        self.screen.set_focus(None)

