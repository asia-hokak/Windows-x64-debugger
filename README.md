# Windows x64 debugger（C++ backend + Python Textual frontend)

## 關於此專案

此專案是因為我在玩逆向工程時遇到踩過 anti-debug 的坑，於是決定寫一款小型的 debugger 來玩玩，並作為台南一中高三資訊專題的期末專案。

## 環境需求

- Windows 10/11 x64
- Visual Studio 2022（含 MSVC x64 toolchain）
- CMake >= 3.20
- Python 3.10+（建議 3.13）
- 已存在 `cloned_repos/zydis`（`CMakeLists.txt` 會直接用這份原始碼）

## python 依賴

```powershell
pip install -r requirements.txt
```

## build

在專案根目錄執行：

```powershell
.\build.bat
```

建置完成後：

- backend DLL :`src/frontend/dbg/lib/dbg_backend.dll`
- protable excutable :`dist/dbg.exe`

## 啟動方式

### 1) 直接跑 protable excutable

```powershell
.\dbg.exe <target_exe> [args...]
```

例：

```powershell
.\dbg.exe C:\Windows\System32\calc.exe
```

Attach 模式：

```powershell
.\dbg.exe --attach <pid>
```

### 2) 開發模式

在 `src\frontend`：

```powershell
python main.py --dll .\dbg\lib\dbg_backend.dll <target_exe> [args...]
```

Attach pid：

```powershell
python \main.py --dll .\dbg\lib\dbg_backend.dll --attach <pid>
```

## 基礎操作

- `Ctrl + K`：切換主畫面 / info 畫面
- Command Input 按 `Esc`：取消 focus
- Command Input 直接按 Enter（空字串）：重複上一個指令
- `shift + 左鍵`: 可以選取

## 內建指令

- `c` / `continue`：繼續執行
- `si` / `s` / `step`：step into
- `ni` / `n` / `next`：step over
- `fin` / `finish`：finish
- `b <addr>` / `bp <addr>`：breakpoint
- `bc <addr>`：移除 breakpoint
- `x <addr>`：memory view 跳到指定位址
- `ctx <count>` / `win <count>` / `window <count>`：調整 disassembly 視窗數量
- `q` / `quit` / `exit`：離開

位址可用 `0x...` 或十進位。

## 常見問題

`backend DLL not found`：

- 確認已先跑 `build.bat`
- 開發模式請加上 `--dll .\src\frontend\dbg\lib\dbg_backend.dll`