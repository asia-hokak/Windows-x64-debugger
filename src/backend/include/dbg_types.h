#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <windows.h>

namespace dbgapi {

enum class RunStatus : int {
    Error = -1,
    Stopped = 1,
    Exited = 2,
};

enum class Result : int {
    Ok = 0,
    Err = -1,
};

enum class TargetArch : int {
    Unknown = 0,
    X86 = 1,
    X64 = 2,
};

enum class DebugState : int {
    Inactive = 0,
    Running = 1,
    Paused = 2,
    Exited = 3,
};

enum class BreakpointKind : int {
    Software = 0,
    StepOver = 1,
    Hardware = 2,
};

enum class HardwareBreakpointType : int {
    Execute = 0,
    Write = 1,
    Access = 2,
};

struct InstructionView {
    uint64_t address;
    uint8_t size;
    char text[256];
    char bytes[128];
    int is_current;
    uint32_t kind;
};

struct MemoryRegionView {
    uint64_t base;
    uint64_t size;
    uint32_t state;
    uint32_t protect;
    uint32_t type;
    char info[512];
};

struct BreakpointView {
    uint64_t address;
    uint64_t hit_count;
    uint32_t kind;
    int enabled;
};

struct RegisterView {
    char name[32];
    uint64_t value;
};

struct ThreadInfoView {
    uint32_t tid;
    uint64_t teb;
    int alive;
    int suspended;
    int is_current;
    uint32_t frame_index;
    uint64_t callstack_address;
};

}  // namespace dbgapi

namespace dbgtype {

enum class TargetArch {
    Unknown,
    X86,
    X64,
};

enum class DebugState {
    Inactive,
    Running,
    Paused,
    Exited,
};

enum class BreakpointKind {
    Software,
    StepOver,
    Hardware,
};

enum class HardwareBreakpointType {
    Execute,
    Write,
    Access,
};

struct Breakpoint {
    int id = -1;
    BreakpointKind kind = BreakpointKind::Software;
    uint64_t address = 0;
    uint64_t hit_count = 0;
    bool enabled = false;
    bool temporary = false;
    uint8_t original_byte = 0;
    HardwareBreakpointType hw_type = HardwareBreakpointType::Execute;
    size_t hw_size = 1;
    int hw_slot = -1;
};

struct ThreadInfo {
    DWORD tid = 0;
    uint64_t teb = 0;
    bool alive = true;
    bool suspended = false;
    bool is_current = false;
    HANDLE handle = nullptr;
    std::vector<uint64_t> callstack;
};

struct ModuleInfo {
    uint64_t base = 0;
    uint64_t size = 0;
    std::string name;
    std::string path;
    bool is_main_module = false;
};

struct RegisterState {
    uint64_t rax = 0;
    uint64_t rbx = 0;
    uint64_t rcx = 0;
    uint64_t rdx = 0;
    uint64_t rsi = 0;
    uint64_t rdi = 0;
    uint64_t rbp = 0;
    uint64_t rsp = 0;
    uint64_t rip = 0;
    uint64_t r8 = 0;
    uint64_t r9 = 0;
    uint64_t r10 = 0;
    uint64_t r11 = 0;
    uint64_t r12 = 0;
    uint64_t r13 = 0;
    uint64_t r14 = 0;
    uint64_t r15 = 0;
    uint64_t eflags = 0;
    uint64_t dr0 = 0;
    uint64_t dr1 = 0;
    uint64_t dr2 = 0;
    uint64_t dr3 = 0;
    uint64_t dr6 = 0;
    uint64_t dr7 = 0;
};

struct RegisterValue {
    std::string name;
    uint64_t value = 0;
};

struct MemoryRegion {
    uint64_t base = 0;
    uint64_t size = 0;
    DWORD state = 0;
    DWORD protect = 0;
    DWORD type = 0;
    std::string info = "";
};

}  // namespace dbgtype

namespace dbg::analysis {

enum class InstructionKind {
    Invalid,
    Unknown,
    Normal,
    Call,
    Jump,
    ConditionalJump,
    Return,
    Interrupt,
};

struct Instruction {
    uint64_t address = 0;
    uint8_t size = 0;
    InstructionKind kind = InstructionKind::Invalid;
    uint32_t mnemonic_id = 0;

    std::vector<uint8_t> bytes;
    std::string mnemonic;
    std::string operands;
    std::string text;

    bool readable = false;
};

struct InstructionList {
    uint64_t start_address = 0;
    uint64_t end_address = 0;
    std::vector<Instruction> items;
};

}  // namespace dbg::analysis
