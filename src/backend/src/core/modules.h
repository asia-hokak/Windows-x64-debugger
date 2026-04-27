#pragma once

#include <stdint.h>
#include <string>
#include <vector>

#include "debug_session.h"

class Modules {
public:
    explicit Modules(DebugSession& session);

    bool add_module(uint64_t base,
                   uint64_t size,
                   std::string path,
                   bool is_main_module = false);
    bool remove_module(uint64_t base);

    bool get_proccess_module();
    std::vector<dbgtype::ModuleInfo> query_module() const;

private:
    static std::string extract_module_name(const std::string& path);

private:
    DebugSession& session_;
};



