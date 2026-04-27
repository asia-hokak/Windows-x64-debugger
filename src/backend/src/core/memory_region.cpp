#include "memory_region.h"

#include <algorithm>
#include <cstring>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <tlhelp32.h>
#include <vector>

using std::max;
using std::min;
using std::string;
using std::vector;

struct HeapEntry
{
    uint64_t base = 0;
    uint32_t id = 0;
};

struct StackEntry
{
    uint64_t stack_base = 0;
    uint64_t stack_limit = 0;
    uint32_t tid = 0;
};

static constexpr ULONG process_basic_information_class = 0;
static constexpr ULONG process_wow64_information_class = 26;
static constexpr size_t max_heaps_to_read = 1024;


struct ProcessInformation
{
    PVOID exit_status;
    PVOID peb_base_address;
    PVOID affinity_mask;
    PVOID base_priority;
    ULONG_PTR unique_process_id;
    PVOID inherited_from_unique_process_id;
};

struct CLIENT_ID32
{
    uint32_t unique_process = 0; // HANDLE
    uint32_t unique_thread  = 0; // HANDLE
};

struct CLIENT_ID64
{
    uint64_t unique_process = 0; // HANDLE
    uint64_t unique_thread  = 0; // HANDLE
};

struct DBG_NT_TIB32
{
    uint32_t ExceptionList;          // 0x00
    uint32_t StackBase;              // 0x04
    uint32_t StackLimit;             // 0x08
    uint32_t SubSystemTib;           // 0x0C

    union
    {
        uint32_t FiberData;          // 0x10
        uint32_t Version;            // 0x10
    };

    uint32_t ArbitraryUserPointer;   // 0x14
    uint32_t Self;                   // 0x18
};

struct DBG_NT_TIB64
{
    uint64_t ExceptionList;          // 0x00
    uint64_t StackBase;              // 0x08
    uint64_t StackLimit;             // 0x10
    uint64_t SubSystemTib;           // 0x18

    union
    {
        uint64_t FiberData;          // 0x20
        uint32_t Version;            // 0x20
    };

    uint64_t ArbitraryUserPointer;   // 0x28
    uint64_t Self;                   // 0x30
};

struct TEB32
{
    DBG_NT_TIB32 NtTib;                                  // 0x000
    uint32_t environment_pointer = 0;                    // 0x01C
    CLIENT_ID32 client_id = {};                          // 0x020
    uint32_t active_rpc_handle = 0;                      // 0x028
    uint32_t thread_local_storage_pointer = 0;           // 0x02C
    uint32_t process_environment_block = 0;              // 0x030
    uint32_t last_error_value = 0;                       // 0x034
    uint32_t count_of_owned_critical_sections = 0;       // 0x038
    uint32_t csr_client_thread = 0;                      // 0x03C
    uint32_t win32_thread_info = 0;                      // 0x040
    uint32_t user32_reserved[26] = {};                   // 0x044
    uint32_t user_reserved[5] = {};                      // 0x0AC
    uint32_t wow32_reserved = 0;                         // 0x0C0
    uint32_t current_locale = 0;                         // 0x0C4
    uint32_t fp_software_status_register = 0;            // 0x0C8
    uint32_t reserved_for_debugger_instrumentation[16] = {}; // 0x0CC
    uint32_t system_reserved1[38] = {};                  // 0x10C
    uint8_t placeholder_compatibility_mode = 0;          // 0x1A4
    uint8_t placeholder_hydration_always_explicit = 0;   // 0x1A5
    uint8_t placeholder_reserved[10] = {};               // 0x1A6
    uint32_t proxied_process_id = 0;                     // 0x1B0
};

struct TEB64
{
    DBG_NT_TIB64 NtTib;                                  // 0x000
    uint64_t environment_pointer = 0;                    // 0x038
    CLIENT_ID64 client_id = {};                          // 0x040
    uint64_t active_rpc_handle = 0;                      // 0x050
    uint64_t thread_local_storage_pointer = 0;           // 0x058
    uint64_t process_environment_block = 0;              // 0x060
    uint32_t last_error_value = 0;                       // 0x068
    uint32_t count_of_owned_critical_sections = 0;       // 0x06C
    uint64_t csr_client_thread = 0;                      // 0x070
    uint64_t win32_thread_info = 0;                      // 0x078
    uint32_t user32_reserved[26] = {};                   // 0x080
    uint32_t user_reserved[5] = {};                      // 0x0E8

    uint32_t padding0 = 0;                               // 0x0FC
    uint64_t wow32_reserved = 0;                         // 0x100

    uint32_t current_locale = 0;                         // 0x108
    uint32_t fp_software_status_register = 0;            // 0x10C
    uint64_t reserved_for_debugger_instrumentation[16] = {}; // 0x110
    uint64_t system_reserved1[30] = {};                  // 0x190
    uint8_t placeholder_compatibility_mode = 0;          // 0x280
    uint8_t placeholder_hydration_always_explicit = 0;   // 0x281
    uint8_t placeholder_reserved[10] = {};               // 0x282
    uint32_t proxied_process_id = 0;                     // 0x28C
};

struct PEB32
{
    uint8_t inherited_address_space = 0;                 // 0x000
    uint8_t read_image_file_exec_options = 0;            // 0x001
    uint8_t being_debugged = 0;                          // 0x002
    uint8_t bit_field = 0;                               // 0x003

    uint32_t mutant = 0;                                 // 0x004
    uint32_t image_base_address = 0;                     // 0x008
    uint32_t ldr = 0;                                    // 0x00C
    uint32_t process_parameters = 0;                     // 0x010
    uint32_t subsystem_data = 0;                         // 0x014
    uint32_t process_heap = 0;                           // 0x018
    uint32_t fast_peb_lock = 0;                          // 0x01C
    uint32_t atl_thunk_slist_ptr = 0;                    // 0x020
    uint32_t ifeo_key = 0;                               // 0x024

    uint32_t cross_process_flags = 0;                    // 0x028
    uint32_t kernel_callback_table = 0;                  // 0x02C
    uint32_t system_reserved = 0;                        // 0x030
    uint32_t atl_thunk_slist_ptr32 = 0;                  // 0x034
    uint32_t api_set_map = 0;                            // 0x038

    uint32_t tls_expansion_counter = 0;                  // 0x03C
    uint32_t tls_bitmap = 0;                             // 0x040
    uint32_t tls_bitmap_bits[2] = {};                    // 0x044

    uint32_t read_only_shared_memory_base = 0;           // 0x04C
    uint32_t shared_data = 0;                            // 0x050
    uint32_t read_only_static_server_data = 0;           // 0x054
    uint32_t ansi_code_page_data = 0;                    // 0x058
    uint32_t oem_code_page_data = 0;                     // 0x05C
    uint32_t unicode_case_table_data = 0;                // 0x060

    uint32_t number_of_processors = 0;                   // 0x064
    uint32_t nt_global_flag = 0;                         // 0x068

    int64_t critical_section_timeout = 0;                // 0x070

    uint32_t heap_segment_reserve = 0;                   // 0x078
    uint32_t heap_segment_commit = 0;                    // 0x07C
    uint32_t heap_decommit_total_free_threshold = 0;     // 0x080
    uint32_t heap_decommit_free_block_threshold = 0;     // 0x084

    uint32_t number_of_heaps = 0;                        // 0x088
    uint32_t maximum_number_of_heaps = 0;                // 0x08C
    uint32_t process_heaps = 0;                          // 0x090
    uint32_t gdi_shared_handle_table = 0;                // 0x094
    uint32_t process_starter_helper = 0;                 // 0x098
    uint32_t gdi_dc_attribute_list = 0;                  // 0x09C
    uint32_t loader_lock = 0;                            // 0x0A0

    uint32_t os_major_version = 0;                       // 0x0A4
    uint32_t os_minor_version = 0;                       // 0x0A8
    uint16_t os_build_number = 0;                        // 0x0AC
    uint16_t os_csd_version = 0;                         // 0x0AE
    uint32_t os_platform_id = 0;                         // 0x0B0

    uint32_t image_subsystem = 0;                        // 0x0B4
    uint32_t image_subsystem_major_version = 0;          // 0x0B8
    uint32_t image_subsystem_minor_version = 0;          // 0x0BC

    uint32_t active_process_affinity_mask = 0;           // 0x0C0
    uint32_t gdi_handle_buffer[34] = {};                 // 0x0C4
};

struct PEB64
{
    uint8_t inherited_address_space = 0;                 // 0x000
    uint8_t read_image_file_exec_options = 0;            // 0x001
    uint8_t being_debugged = 0;                          // 0x002
    uint8_t bit_field = 0;                               // 0x003
    uint32_t padding0 = 0;                               // 0x004

    uint64_t mutant = 0;                                 // 0x008
    uint64_t image_base_address = 0;                     // 0x010
    uint64_t ldr = 0;                                    // 0x018
    uint64_t process_parameters = 0;                     // 0x020
    uint64_t subsystem_data = 0;                         // 0x028
    uint64_t process_heap = 0;                           // 0x030
    uint64_t fast_peb_lock = 0;                          // 0x038
    uint64_t atl_thunk_slist_ptr = 0;                    // 0x040
    uint64_t ifeo_key = 0;                               // 0x048

    uint32_t cross_process_flags = 0;                    // 0x050
    uint32_t padding1 = 0;                               // 0x054

    uint64_t kernel_callback_table = 0;                  // 0x058
    uint32_t system_reserved = 0;                        // 0x060
    uint32_t atl_thunk_slist_ptr32 = 0;                  // 0x064
    uint64_t api_set_map = 0;                            // 0x068

    uint32_t tls_expansion_counter = 0;                  // 0x070
    uint32_t padding2 = 0;                               // 0x074
    uint64_t tls_bitmap = 0;                             // 0x078
    uint32_t tls_bitmap_bits[2] = {};                    // 0x080

    uint64_t read_only_shared_memory_base = 0;           // 0x088
    uint64_t shared_data = 0;                            // 0x090
    uint64_t read_only_static_server_data = 0;           // 0x098
    uint64_t ansi_code_page_data = 0;                    // 0x0A0
    uint64_t oem_code_page_data = 0;                     // 0x0A8
    uint64_t unicode_case_table_data = 0;                // 0x0B0

    uint32_t number_of_processors = 0;                   // 0x0B8
    uint32_t nt_global_flag = 0;                         // 0x0BC

    int64_t critical_section_timeout = 0;                // 0x0C0

    uint64_t heap_segment_reserve = 0;                   // 0x0C8
    uint64_t heap_segment_commit = 0;                    // 0x0D0
    uint64_t heap_decommit_total_free_threshold = 0;     // 0x0D8
    uint64_t heap_decommit_free_block_threshold = 0;     // 0x0E0

    uint32_t number_of_heaps = 0;                        // 0x0E8
    uint32_t maximum_number_of_heaps = 0;                // 0x0EC
    uint64_t process_heaps = 0;                          // 0x0F0
    uint64_t gdi_shared_handle_table = 0;                // 0x0F8
    uint64_t process_starter_helper = 0;                 // 0x100

    uint32_t gdi_dc_attribute_list = 0;                  // 0x108
    uint32_t padding3 = 0;                               // 0x10C

    uint64_t loader_lock = 0;                            // 0x110

    uint32_t os_major_version = 0;                       // 0x118
    uint32_t os_minor_version = 0;                       // 0x11C
    uint16_t os_build_number = 0;                        // 0x120
    uint16_t os_csd_version = 0;                         // 0x122
    uint32_t os_platform_id = 0;                         // 0x124

    uint32_t image_subsystem = 0;                        // 0x128
    uint32_t image_subsystem_major_version = 0;          // 0x12C
    uint32_t image_subsystem_minor_version = 0;          // 0x130

    uint32_t padding4 = 0;                               // 0x134
    uint64_t active_process_affinity_mask = 0;           // 0x138

    uint32_t gdi_handle_buffer[60] = {};                 // 0x140
};

static bool ranges_overlap(uint64_t a_base, uint64_t a_size, uint64_t b_base, uint64_t b_size)
{
    if (a_size == 0 || b_size == 0) {
        return false;
    }

    const uint64_t a_end = a_base + a_size;
    const uint64_t b_end = b_base + b_size;
    if (a_end <= a_base || b_end <= b_base) {
        return false;
    }

    return a_base < b_end && b_base < a_end;
}

template <typename T>
static bool read_value(HANDLE process, uint64_t address, T& out_value)
{
    out_value = {};
    if (!process) {
        return false;
    }

    SIZE_T bytes_read = 0;
    if (!ReadProcessMemory(process, (LPCVOID)(uintptr_t)(address), &out_value, sizeof(T), &bytes_read)) {
        return false;
    }
    return bytes_read == sizeof(T);
}

static bool read_bytes(HANDLE process, uint64_t address, void* buffer, size_t size)
{
    if (!process || !buffer || size == 0) {
        return false;
    }

    SIZE_T bytes_read = 0;
    if (!ReadProcessMemory(process, (LPCVOID)(uintptr_t)(address), buffer, size, &bytes_read)) {
        return false;
    }
    return bytes_read == size;
}

static bool query_process_peb_addresses(
    HANDLE process,
    uint64_t& out_native_peb,
    uint64_t& out_wow64_peb)
{
    out_native_peb = 0;
    out_wow64_peb = 0;
    if (!process) {
        return false;
    }

    // ntdll function 沒辦法直接用
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) {
        return false;
    }

    typedef NTSTATUS (NTAPI *NtQueryInformationProcess_t)(
        HANDLE ProcessHandle,
        ULONG ProcessInformationClass,
        PVOID ProcessInformation,
        ULONG ProcessInformationLength,
        PULONG ReturnLength
    );

    auto NtQueryInformationProcess =
        (NtQueryInformationProcess_t)(
            GetProcAddress(ntdll, "NtQueryInformationProcess")
        );

    if (!NtQueryInformationProcess) {
        return false;
    }

    ProcessInformation pbi = {};
    LONG status = NtQueryInformationProcess(
        process,
        process_basic_information_class,
        &pbi,
        sizeof(pbi),
        nullptr);
    if (status >= 0 && pbi.peb_base_address) {
        out_native_peb = (uint64_t)(uintptr_t)(pbi.peb_base_address);
    }

    ULONG_PTR wow64_peb = 0;
    status = NtQueryInformationProcess(
        process,
        process_wow64_information_class,
        &wow64_peb,
        sizeof(wow64_peb),
        nullptr);
    if (status >= 0 && wow64_peb != 0) {
        out_wow64_peb = (uint64_t)(wow64_peb);
    }

    return out_native_peb != 0 || out_wow64_peb != 0;
}

static bool get_module_section_for_address(
    HANDLE process,
    const dbgtype::ModuleInfo& module,
    uint64_t address,
    string& out_section)
{
    // module 其實就是一個 pe 格式的檔案內容，所以可以透過 header 來得知 section 的
    out_section.clear();
    if (!process || module.base == 0 || module.size == 0 || address < module.base) {
        return false;
    }

    // dos_header
    IMAGE_DOS_HEADER dos = {};
    if (!read_bytes(process, module.base, &dos, sizeof(dos))) {
        return false;
    }
    if (dos.e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }

    // nt_header
    const uint64_t nt_address = module.base + (uint32_t)(dos.e_lfanew);
    uint32_t signature = 0;
    if (!read_value(process, nt_address, signature) || signature != IMAGE_NT_SIGNATURE) {
        return false;
    }

    // nt_header 前面 4 bytes 是 signature
    IMAGE_FILE_HEADER file_header = {};
    if (!read_bytes(process, nt_address + sizeof(uint32_t), &file_header, sizeof(file_header))) {
        return false;
    }

    const uint64_t optional_header_address = nt_address + sizeof(uint32_t) + sizeof(IMAGE_FILE_HEADER);
    const uint64_t section_headers_address = optional_header_address + file_header.SizeOfOptionalHeader;
    if (file_header.NumberOfSections == 0) {
        return false;
    }

    // section table
    vector<IMAGE_SECTION_HEADER> sections;
    sections.resize(file_header.NumberOfSections);
    if (!read_bytes(
            process,
            section_headers_address,
            sections.data(),
            sections.size() * sizeof(IMAGE_SECTION_HEADER))) {
        return false;
    }

    // rva 是目前要查的 address 在這個檔案的偏移量
    const uint64_t rva = address - module.base;
    size_t i = 0;
    while (i < sections.size()) {
        const IMAGE_SECTION_HEADER& section = sections[i];
        const uint64_t section_base = section.VirtualAddress;
        const uint64_t section_size = max<uint32_t>(section.Misc.VirtualSize, section.SizeOfRawData);
        if (section_size > 0 && rva >= section_base && rva < section_base + section_size) {
            char name_buffer[IMAGE_SIZEOF_SHORT_NAME + 1] = {};
            memcpy(name_buffer, section.Name, IMAGE_SIZEOF_SHORT_NAME);
            out_section.assign(name_buffer);
            if (out_section.empty()) { // 通常沒有的地方是裝 header 的 section
                out_section = "<section>";
            }
            return true;
        }
        ++i;
    }

    return false;
}

static vector<HeapEntry> get_heap(const DebugSession& session)
{
    vector<HeapEntry> heap_entries;
    if (!session.process_handle) {
        return heap_entries;
    }

    uint64_t native_peb = 0;
    uint64_t wow64_peb = 0;
    if (!query_process_peb_addresses(session.process_handle, native_peb, wow64_peb)) {
        return heap_entries;
    }

    if (session.target_arch == dbgtype::TargetArch::X64 && native_peb != 0) {
        PEB64 peb64 = {};
        if (read_value(session.process_handle, native_peb, peb64) &&
            peb64.number_of_heaps != 0 &&
            peb64.process_heaps != 0) {
            uint32_t heap_count = peb64.number_of_heaps > max_heaps_to_read
                ? (uint32_t)(max_heaps_to_read)
                : peb64.number_of_heaps; // peb 拿 heap_count
            uint32_t i = 0;
            while (i < heap_count) {
                uint64_t heap_base = 0;
                if (read_value(session.process_handle, peb64.process_heaps + (uint64_t)i * 8, heap_base) &&
                    heap_base != 0) {
                    HeapEntry entry = {};
                    entry.base = heap_base;
                    entry.id = (uint32_t)(heap_entries.size());
                    heap_entries.push_back(entry);
                }
                ++i;
            }
        }
    }

    // 同樣的事只是 x86 的
    if (session.target_arch == dbgtype::TargetArch::X86 && wow64_peb != 0) {
        PEB32 peb32 = {};
        if (read_value(session.process_handle, wow64_peb, peb32) &&
            peb32.number_of_heaps != 0 &&
            peb32.process_heaps != 0) {
            uint32_t heap_count = peb32.number_of_heaps > max_heaps_to_read
                ? (uint32_t)(max_heaps_to_read)
                : peb32.number_of_heaps;
            uint32_t i = 0;
            while (i < heap_count) {
                uint32_t heap_base_32 = 0;
                if (read_value(session.process_handle, peb32.process_heaps + (uint64_t)i * 4, heap_base_32)) {
                    uint64_t heap_base = heap_base_32;
                    if (heap_base != 0) {
                        HeapEntry entry = {};
                        entry.base = heap_base;
                        entry.id = (uint32_t)(heap_entries.size());
                        heap_entries.push_back(entry);
                    }
                }
                ++i;
            }
        }
    }

    return heap_entries;
}

static vector<StackEntry> get_stack(const DebugSession& session)
{
    vector<StackEntry> stack_entries;
    if (!session.process_handle) {
        return stack_entries;
    }

    // 從 thread info 的 teb 拿到每個 thread 的 stack 範圍
    auto it = session.threads.begin();
    while (it != session.threads.end()) {
        const dbgtype::ThreadInfo& thread_info = it->second;
        if (thread_info.teb != 0) {
            const uint64_t teb = thread_info.teb;
            if (session.target_arch == dbgtype::TargetArch::X64) {
                TEB64 teb64 = {};
                if (read_value(session.process_handle, teb, teb64) &&
                    teb64.NtTib.StackBase != 0 &&
                    teb64.NtTib.StackLimit != 0) {
                    StackEntry entry = {};
                    entry.stack_base = teb64.NtTib.StackBase;
                    entry.stack_limit = teb64.NtTib.StackLimit;
                    entry.tid = thread_info.tid;
                    stack_entries.push_back(entry);
                }
            } else if (session.target_arch == dbgtype::TargetArch::X86) {
                TEB32 teb32 = {};
                if (read_value(session.process_handle, teb, teb32) &&
                    teb32.NtTib.StackBase != 0 &&
                    teb32.NtTib.StackLimit != 0) {
                    StackEntry entry = {};
                    entry.stack_base = teb32.NtTib.StackBase;
                    entry.stack_limit = teb32.NtTib.StackLimit;
                    entry.tid = thread_info.tid;
                    stack_entries.push_back(entry);
                }
            }
        }
        ++it;
    }

    return stack_entries;
}

static bool is_region_module(const dbgtype::MemoryRegion& region, const dbgtype::ModuleInfo& module)
{
    if (region.size == 0 || module.size == 0) {
        return false;
    }

    const uint64_t region_end = region.base + region.size;
    const uint64_t module_end = module.base + module.size;
    if (region_end <= region.base || module_end <= module.base) {
        return false;
    }

    return region.base < module_end && module.base < region_end;
}

static bool is_region_stack(vector<StackEntry>& stack, const dbgtype::MemoryRegion& region, DWORD& out_tid)
{
    out_tid = 0;
    size_t i = 0;
    while (i < stack.size()) {
        uint64_t stack_start = stack[i].stack_limit;
        uint64_t stack_end = stack[i].stack_base;
        const uint64_t stack_size = stack_end - stack_start;
        if (stack_size != 0 && ranges_overlap(region.base, region.size, stack_start, stack_size)) {
            out_tid = stack[i].tid;
            return true;
        }
        ++i;
    }
    return false;
}

static bool is_region_heap(vector<HeapEntry>& heap, const dbgtype::MemoryRegion& region, uint32_t& out_heap_id)
{
    out_heap_id = 0;
    size_t i = 0;
    while (i < heap.size()) {
        if (heap[i].base == region.base) {
            out_heap_id = heap[i].id;
            return true;
        }
        ++i;
    }
    return false;
}

static string build_region_info(
    const DebugSession& session,
    const dbgtype::MemoryRegion& region,
    vector<StackEntry>& stack_entries,
    vector<HeapEntry>& heap_entries)
{
    // 這兩個 state 實際上在 virtual memory 裡面不占空間
    if (region.state == MEM_RESERVE || region.state == MEM_FREE){
        return "";
    }

    // 如果是 module 的話就標註 section name
    // 遍歷 module 直到吻合 region adresss
    auto module_it = session.modules.begin(); 
    while (module_it != session.modules.end()) {
        const dbgtype::ModuleInfo& module = module_it->second;
        if (is_region_module(region, module)) {
            string section_name;
            get_module_section_for_address(session.process_handle, module, region.base, section_name);

            string path = module.path.empty() ? module.name : module.path;
            if (path.empty()) {
                path = "<module>";
            }
            if (!section_name.empty()) { 
                return section_name;
            }
            return path;
        }
        ++module_it;
    }

    // 如果是 stack 的話就標註 tid
    DWORD stack_tid = 0;
    if (is_region_stack(stack_entries, region, stack_tid)) {
        char buffer[64] = {};
        sprintf_s(buffer, "stack tid=%lu", (unsigned long)(stack_tid));
        return buffer;
    }

    // 如果是 heap 的話就標註 heap_id
    uint32_t heap_id = 0;
    if (is_region_heap(heap_entries, region, heap_id)) {
        char buffer[64] = {};
        sprintf_s(buffer, "heap id=%u", (unsigned int)(heap_id));
        return buffer;
    }

    return "";
}

MemoryRegionManager::MemoryRegionManager(DebugSession& session)
    : session_(session)
{
}

std::vector<dbgtype::MemoryRegion> MemoryRegionManager::query_regions() const
{
    std::vector<dbgtype::MemoryRegion> regions;
    if (!session_.process_handle) {
        return regions;
    }

    SYSTEM_INFO info = {};
    GetSystemInfo(&info);
    vector<HeapEntry> heap_entries = get_heap(session_);
    vector<StackEntry> stack_entries = get_stack(session_);

    uint64_t address = (uint64_t)(uintptr_t)(info.lpMinimumApplicationAddress);
    const uint64_t max_address = (uint64_t)(uintptr_t)(info.lpMaximumApplicationAddress);
    const uint64_t page_size = info.dwPageSize ? (uint64_t)(info.dwPageSize) : 0x1000ull;
    const uint64_t alloc_granularity = info.dwAllocationGranularity
        ? (uint64_t)(info.dwAllocationGranularity)
        : 0x10000ull;

    const size_t max_regions = 16384;
    const size_t max_consecutive_query_failures = 4096;
    size_t consecutive_query_failures = 0;

    while (address <= max_address) {
        if (regions.size() >= max_regions) {
            break;
        }

        MEMORY_BASIC_INFORMATION mbi = {};
        SIZE_T queried = VirtualQueryEx(
            session_.process_handle,
            (LPCVOID)(address),
            &mbi,
            sizeof(mbi));

        if (queried == 0) { // 失敗
            ++consecutive_query_failures;
            if (consecutive_query_failures >= max_consecutive_query_failures) {
                break;
            }

            if (max_address - address < alloc_granularity) {
                break;
            }

            const uint64_t next = ((address / alloc_granularity) + 1) * alloc_granularity;
            if (next <= address) {
                address += page_size;
            } else {
                address = next;
            }
            continue;
        }

        consecutive_query_failures = 0;

        dbgtype::MemoryRegion region;
        region.base = (uint64_t)(uintptr_t)(mbi.BaseAddress);
        region.size = (uint64_t)(mbi.RegionSize);
        region.state = mbi.State;
        region.protect = mbi.Protect;
        region.type = mbi.Type;
        region.info = build_region_info(session_, region, stack_entries, heap_entries);
        regions.push_back(region);

        const uint64_t next = region.base + region.size;
        if (next <= address) {
            if (max_address - address < page_size) {
                break;
            }
            address += page_size;
        } else {
            address = next;
        }
    }

    return regions;
}
