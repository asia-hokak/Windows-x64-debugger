#pragma once

#include <string>
#include <vector>
#include <windows.h>

#include "debug_session.h"

class ProcessControl {
public:
    explicit ProcessControl(DebugSession& session);

    bool start_process(const std::string& path, const std::string& cmdline = "");
    bool attach(DWORD pid);
    bool detach();
    bool terminate(UINT exit_code = 0);

    void drain_captured_output(std::vector<std::string>& out_lines);

private:
    bool initialize_session_from_create_process(PROCESS_INFORMATION& pi);
    bool detect_and_store_target_arch();
    void capture_initial_thread_callstack(DWORD tid, HANDLE thread_handle);
    void clear_captured_output_state();

private:
    DebugSession& session_;
    HANDLE child_stdout_read_ = nullptr;
    std::string child_stdout_pending_;
    bool capture_child_output_ = false;
};

