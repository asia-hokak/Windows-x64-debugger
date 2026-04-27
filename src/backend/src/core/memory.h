#pragma once

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

#include "debug_session.h"

class Memory {
public:
    explicit Memory(DebugSession& session);

    bool read(uint64_t address, void* buffer, size_t size, size_t* bytes_read) const;
    bool write(uint64_t address, const void* buffer, size_t size, size_t* bytes_written);
    bool protect(uint64_t address, size_t size, DWORD new_protect, DWORD* old_protect);
    bool flush_instruction_cache(uint64_t address, size_t size);

private:
    DebugSession& session_;
};
