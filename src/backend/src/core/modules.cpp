#include "modules.h"

#include <algorithm>
#include <string>
#include <tlhelp32.h>
#include <unordered_map>
#include <utility>

using std::string;
using std::vector;

static string normalize_module_path(string path)
{
    // GetFinalPathNameByHandle* 這類 API 可能回 \\?\C:\...
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfinalpathnamebyhandlea
    constexpr const char* kNtPrefix1 = R"(\\?\)";
    constexpr const char* kNtPrefix2 = R"(\??\)";
    if (path.rfind(kNtPrefix1, 0) == 0) {
        return path.substr(4);
    }
    if (path.rfind(kNtPrefix2, 0) == 0) {
        return path.substr(4);
    }
    return path;
}

static string WideToUtf8(const wchar_t* wide)
{
    if (!wide || *wide == L'\0') {
        return {};
    }

    const int needed = WideCharToMultiByte(
        CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1) {
        return {};
    }

    string out(size_t(needed - 1), '\0');
    if (WideCharToMultiByte(
            CP_UTF8, 0, wide, -1, out.data(), needed, nullptr, nullptr) <= 0) {
        return {};
    }
    return out;
}

Modules::Modules(DebugSession& session)
    : session_(session)
{
}

bool Modules::add_module(uint64_t base,
                        uint64_t size,
                        string path,
                        bool is_main_module)
{
    if (base == 0) {
        return false;
    }

    dbgtype::ModuleInfo info;
    info.base = base;
    info.size = size;
    info.path = normalize_module_path(std::move(path));
    info.name = extract_module_name(info.path);
    info.is_main_module = is_main_module;

    session_.modules[base] = std::move(info);
    return true;
}

bool Modules::remove_module(uint64_t base)
{
    return session_.modules.erase(base) > 0;
}

bool Modules::get_proccess_module()
{
    if (session_.pid == 0) {
        return false;
    }

    // 這個 winapi 可以找到目前 process 的 module
    HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, session_.pid);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    MODULEENTRY32W entry = {};
    entry.dwSize = sizeof(entry);

    std::unordered_map<uint64_t, dbgtype::ModuleInfo> scanned;

    bool is_main_module = true;
    if (Module32FirstW(snapshot, &entry)) {
        do {
            const uint64_t base = (uint64_t)(uintptr_t)(entry.modBaseAddr);
            const uint64_t size = (uint64_t)(entry.modBaseSize);

            string path = WideToUtf8(entry.szExePath);
            if (path.empty()) {
                path = WideToUtf8(entry.szModule);
            }

            dbgtype::ModuleInfo info;
            info.base = base;
            info.size = size;
            info.path = normalize_module_path(std::move(path));
            info.name = extract_module_name(info.path);
            info.is_main_module = is_main_module;

            scanned[base] = std::move(info);
            is_main_module = false;
        } while (Module32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);

    if (scanned.empty()) {
        return false;
    }

    session_.modules = std::move(scanned);
    return true;
}

vector<dbgtype::ModuleInfo> Modules::query_module() const
{
    vector<dbgtype::ModuleInfo> out;
    out.reserve(session_.modules.size());
    for (const auto& module_entry : session_.modules) {
        out.push_back(module_entry.second);
    }

    std::sort(out.begin(), out.end(), [](const dbgtype::ModuleInfo& a, const dbgtype::ModuleInfo& b) {
        return a.base < b.base;
    });
    return out;
}

string Modules::extract_module_name(const string& path)
{
    if (path.empty()) {
        return {};
    }

    const auto last_separator = path.find_last_of("\\/");
    if (last_separator == string::npos) {
        return path;
    }

    if (last_separator + 1 >= path.size()) {
        return path;
    }

    return path.substr(last_separator + 1);
}
