#pragma once

#include <stddef.h>
#include <stdint.h>

#include "dbg_types.h"

class Memory;
class DebugSession;

namespace dbg::analysis {

class Disassembler {
public:
    explicit Disassembler(DebugSession& session);

    std::vector<Instruction> disassembly_at(uint64_t address,
                                            size_t count,
                                            bool is_x64,
                                            Memory& memory) const;

    std::vector<Instruction> disassembly_window_up(uint64_t address,
                                                   size_t count,
                                                   bool is_x64,
                                                   Memory& memory) const;

    std::vector<Instruction> disassembly_window_down(uint64_t address,
                                                     size_t count,
                                                     bool is_x64,
                                                     Memory& memory) const;

private:
    DebugSession& session_;
};

}  // namespace dbg::analysis



