#include "process_control.h"

#include <algorithm>
#include <vector>
#include <wow64apiset.h>

#include "threads.h"

ProcessControl::ProcessControl(DebugSession& session)
    : session_(session)
{
}

static bool has_whitespace(const std::string& text)
{
    // 判斷是否有 whitespace
    size_t i = 0;
    while (i < text.size()) {
        const char c = text[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            return true;
        }
        ++i;
    }
    return false;
}

static std::string quote_for_command_line(const std::string& value)
{
    // escape 掉 "，確保 parse command 正確
    if (!has_whitespace(value) && value.find('\"') == std::string::npos) {
        return value;
    }

    std::string out = "\"";
    size_t i = 0;
    while (i < value.size()) {
        const char c = value[i];
        if (c == '\"') {
            out += "\\\"";
        } else {
            out += c;
        }
        ++i;
    }
    out += "\"";
    return out;
}

bool ProcessControl::start_process(const std::string& path, const std::string& cmdline)
{
    session_.clear();
    clear_captured_output_state();

    STARTUPINFOA si = {};
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi = {};

    // 處理 command
    std::string command = quote_for_command_line(path);
    if (!cmdline.empty()) {
        command += " ";
        command += cmdline;
    }
    std::vector<char> mutable_cmd(command.begin(), command.end());
    mutable_cmd.push_back('\0');

    // CreatePipe 要用的東西
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = nullptr;
    sa.bInheritHandle = TRUE;

    HANDLE child_stdout_read = nullptr;
    HANDLE child_stdout_write = nullptr;
    capture_child_output_ = false;

    // 用 CreatePipe 把 process 輸出設定成一個 file handle
    // 太抽象了直接抄
    if (CreatePipe(&child_stdout_read, &child_stdout_write, &sa, 0)) {
        if (SetHandleInformation(child_stdout_read, HANDLE_FLAG_INHERIT, 0)) {
            capture_child_output_ = true;
            si.dwFlags |= STARTF_USESTDHANDLES;
            si.hStdOutput = child_stdout_write;
            si.hStdError = child_stdout_write;
            HANDLE stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
            si.hStdInput = (stdin_handle && stdin_handle != INVALID_HANDLE_VALUE) ? stdin_handle : nullptr;
        } else {
            CloseHandle(child_stdout_read);
            CloseHandle(child_stdout_write);
            child_stdout_read = nullptr;
            child_stdout_write = nullptr;
        }
    }

    const DWORD flags = DEBUG_ONLY_THIS_PROCESS;
    const BOOL ok = CreateProcessA(
        nullptr,
        mutable_cmd.data(),
        nullptr,
        nullptr,
        capture_child_output_ ? TRUE : FALSE,
        flags,
        nullptr,
        nullptr,
        &si,
        &pi);

    if (child_stdout_write) {
        CloseHandle(child_stdout_write);
        child_stdout_write = nullptr;
    }

    if (!ok) {
        if (child_stdout_read) {
            CloseHandle(child_stdout_read);
        }
        capture_child_output_ = false;
        return false;
    }

    if (capture_child_output_ && child_stdout_read) {
        child_stdout_read_ = child_stdout_read;
        child_stdout_pending_.clear();
    } else if (child_stdout_read) {
        CloseHandle(child_stdout_read);
    }

    if (!initialize_session_from_create_process(pi)) {
        if (pi.hThread) {
            CloseHandle(pi.hThread);
            pi.hThread = nullptr;
        }
        if (pi.hProcess) {
            TerminateProcess(pi.hProcess, 1);
            CloseHandle(pi.hProcess);
            pi.hProcess = nullptr;
        }
        clear_captured_output_state();
        return false;
    }

    return true;
}

bool ProcessControl::attach(DWORD pid)
{
    session_.clear();
    clear_captured_output_state();

    if (!DebugActiveProcess(pid)) { // binary
        return false;
    }

    HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!process) {
        DebugActiveProcessStop(pid);
        return false;
    }

    session_.pid = pid;
    session_.process_handle = process;
    session_.attached = true;
    session_.state = dbgtype::DebugState::Running;
    if (!detect_and_store_target_arch()) {
        session_.clear();
        DebugActiveProcessStop(pid);
        return false;
    }
    return true;
}

bool ProcessControl::detach()
{
    if (!session_.attached) {
        return false;
    }

    if (!DebugActiveProcessStop(session_.pid)) {
        return false;
    }

    clear_captured_output_state();
    session_.clear();
    return true;
}

bool ProcessControl::terminate(UINT exit_code)
{
    if (!session_.process_handle) {
        return false;
    }

    const BOOL ok = TerminateProcess(session_.process_handle, exit_code);
    if (ok) {
        session_.state = dbgtype::DebugState::Exited;
        clear_captured_output_state();
    }
    return ok != FALSE;
}

void ProcessControl::drain_captured_output(std::vector<std::string>& out_lines)
{
    
    out_lines.clear();
    if (!capture_child_output_ || !child_stdout_read_) {
        return;
    }

    while (true) {
        DWORD available = 0;
        // 檢查目前 pipe 有多少資料
        if (!PeekNamedPipe(child_stdout_read_, nullptr, 0, nullptr, &available, nullptr)) {
            break;
        }
        if (available == 0) {
            break;
        }

        char buffer[4096] = {};
        const DWORD want = std::min<DWORD>(available, DWORD(sizeof(buffer)));
        DWORD bytes_read = 0;
        if (!ReadFile(child_stdout_read_, buffer, want, &bytes_read, nullptr) || bytes_read == 0) {
            break;
        }

        child_stdout_pending_.append(buffer, buffer + bytes_read);
    }

    while (true) {
        size_t sep = std::string::npos;
        size_t consume = 0;
        for (size_t i = 0; i < child_stdout_pending_.size(); ++i) {
            const char c = child_stdout_pending_[i];
            if (c == '\n' || c == '\r') {
                sep = i;
                consume = 1;
                if (i + 1 < child_stdout_pending_.size()) {
                    const char n = child_stdout_pending_[i + 1];
                    if ((c == '\r' && n == '\n') || (c == '\n' && n == '\r')) {
                        consume = 2;
                    }
                }
                break;
            }
        }

        if (sep == std::string::npos) {
            break;
        }

        std::string line = child_stdout_pending_.substr(0, sep);
        child_stdout_pending_.erase(0, sep + consume);
        if (!line.empty()) {
            out_lines.push_back(std::move(line));
        }
    }

    if (session_.state != dbgtype::DebugState::Running && !child_stdout_pending_.empty()) {
        out_lines.push_back(child_stdout_pending_);
        child_stdout_pending_.clear();
    }
}

bool ProcessControl::initialize_session_from_create_process(PROCESS_INFORMATION& pi)
{

    session_.pid = pi.dwProcessId;
    session_.process_handle = pi.hProcess;
    session_.attached = true;
    session_.state = dbgtype::DebugState::Running;
    session_.current_tid = pi.dwThreadId;

    if (!detect_and_store_target_arch()) {
        session_.clear();
        if (pi.hThread) {
            CloseHandle(pi.hThread);
            pi.hThread = nullptr;
        }
        pi.hProcess = nullptr;
        return false;
    }

    capture_initial_thread_callstack(pi.dwThreadId, pi.hThread);

    CloseHandle(pi.hThread);
    pi.hThread = nullptr;
    return true;
}

bool ProcessControl::detect_and_store_target_arch()
{
    if (!session_.process_handle) {
        return false;
    }

    session_.target_arch = dbgtype::TargetArch::Unknown;
    session_.is_wow64_target = false;

    using IsWow64Process2Fn = BOOL(WINAPI*)(HANDLE, USHORT*, USHORT*);
    auto* const is_wow64_process2 = reinterpret_cast<IsWow64Process2Fn>(
        GetProcAddress(GetModuleHandleA("kernel32.dll"), "IsWow64Process2"));
    

    if (is_wow64_process2) {
        USHORT process_machine = IMAGE_FILE_MACHINE_UNKNOWN;
        USHORT native_machine = IMAGE_FILE_MACHINE_UNKNOWN;
        if (!is_wow64_process2(session_.process_handle, &process_machine, &native_machine)) {
            return false;
        }

        if (process_machine == IMAGE_FILE_MACHINE_UNKNOWN && native_machine == IMAGE_FILE_MACHINE_AMD64) {
            session_.target_arch = dbgtype::TargetArch::X64;
            session_.is_wow64_target = false;
            return true;
        }

        if (process_machine == IMAGE_FILE_MACHINE_I386) {
            session_.target_arch = dbgtype::TargetArch::X86;
            session_.is_wow64_target = true;
            return true;
        }
    }

    session_.target_arch = dbgtype::TargetArch::Unknown;
    session_.is_wow64_target = false;
    return false;
}

void ProcessControl::clear_captured_output_state()
{
    if (child_stdout_read_ && child_stdout_read_ != INVALID_HANDLE_VALUE) {
        CloseHandle(child_stdout_read_);
    }
    child_stdout_read_ = nullptr;
    child_stdout_pending_.clear();
    capture_child_output_ = false;
}

void ProcessControl::capture_initial_thread_callstack(DWORD tid, HANDLE thread_handle)
{
    if (tid == 0 || !thread_handle) {
        return;
    }

    std::vector<uint64_t> callstack;
    Threads threads(session_);
    if (threads.scan_thread_callstack(thread_handle, callstack)) {
        session_.pending_thread_callstacks[tid] = std::move(callstack);
    }
}

