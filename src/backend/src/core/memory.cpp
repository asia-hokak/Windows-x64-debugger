#include "memory.h"

#include <stdint.h>

Memory::Memory(DebugSession& session)
    : session_(session)
{
}

bool Memory::read(uint64_t address, void* buffer, size_t size, size_t* bytes_read) const
{
    if (!session_.process_handle || !buffer || size == 0) {
        return false;
    }

    SIZE_T read = 0;
    const BOOL ok = ReadProcessMemory(
        session_.process_handle,
        (LPCVOID)(address),
        buffer,
        (SIZE_T)(size),
        &read);

    if (bytes_read) {
        *bytes_read = (size_t)(read);
    }
    return ok != FALSE;
}

bool Memory::write(uint64_t address, const void* buffer, size_t size, size_t* bytes_written)
{
    if (!session_.process_handle || !buffer || size == 0) {
        return false;
    }

    SIZE_T written = 0;
    const BOOL ok = WriteProcessMemory(
        session_.process_handle,
        (LPVOID)(address),
        buffer,
        (SIZE_T)(size),
        &written);

    if (bytes_written) {
        *bytes_written = (size_t)(written);
    }
    return ok != FALSE;
}

bool Memory::protect(uint64_t address, size_t size, DWORD new_protect, DWORD* old_protect)
{
    if (!session_.process_handle || size == 0) {
        return false;
    }

    return VirtualProtectEx(
        session_.process_handle,
        (LPVOID)(address),
        (SIZE_T)(size),
        new_protect,
        old_protect) != FALSE;
}

bool Memory::flush_instruction_cache(uint64_t address, size_t size)
{ 
    /*
    從官方文檔可以得知 
    Applications should call FlushInstructionCache if they generate or modify code in memory.
    The CPU cannot detect the change, and may execute the old code it cached.
    https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-flushinstructioncache
    */
    if (!session_.process_handle) {
        return false;
    }

    return FlushInstructionCache(
        session_.process_handle,
        (LPCVOID)(address),
        (SIZE_T)(size)) != FALSE;
}
