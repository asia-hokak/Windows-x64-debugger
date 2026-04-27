#pragma once

#include <stdint.h>
#include <string>

#include "dbg_types.h"

namespace dbg::analysis {

bool is_conditional_jump_mnemonic(uint32_t mnemonic_id);
bool is_call_mnemonic(uint32_t mnemonic_id);
bool is_return_mnemonic(uint32_t mnemonic_id);
bool is_jump_mnemonic(uint32_t mnemonic_id);
bool is_interrupt_mnemonic(uint32_t mnemonic_id);
InstructionKind classify_instruction_kind(uint32_t mnemonic_id);

std::string FormatInstruction(const Instruction& insn);

} // namespace dbg::analysis

