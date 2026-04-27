#include "instruction_formatter.h"

#include <Zydis/Zydis.h>

namespace dbg::analysis {

bool is_conditional_jump_mnemonic(uint32_t mnemonic_id)
{
    switch (mnemonic_id) {
    case ZYDIS_MNEMONIC_JB:
    case ZYDIS_MNEMONIC_JBE:
    case ZYDIS_MNEMONIC_JCXZ:
    case ZYDIS_MNEMONIC_JECXZ:
    case ZYDIS_MNEMONIC_JKNZD:
    case ZYDIS_MNEMONIC_JKZD:
    case ZYDIS_MNEMONIC_JL:
    case ZYDIS_MNEMONIC_JLE:
    case ZYDIS_MNEMONIC_JNB:
    case ZYDIS_MNEMONIC_JNBE:
    case ZYDIS_MNEMONIC_JNL:
    case ZYDIS_MNEMONIC_JNLE:
    case ZYDIS_MNEMONIC_JNO:
    case ZYDIS_MNEMONIC_JNP:
    case ZYDIS_MNEMONIC_JNS:
    case ZYDIS_MNEMONIC_JNZ:
    case ZYDIS_MNEMONIC_JO:
    case ZYDIS_MNEMONIC_JP:
    case ZYDIS_MNEMONIC_JRCXZ:
    case ZYDIS_MNEMONIC_JS:
    case ZYDIS_MNEMONIC_JZ:
        return true;
    default:
        return false;
    }
}

bool is_call_mnemonic(uint32_t mnemonic_id)
{
    return mnemonic_id == ZYDIS_MNEMONIC_CALL;
}

bool is_return_mnemonic(uint32_t mnemonic_id)
{
    switch (mnemonic_id) {
    case ZYDIS_MNEMONIC_RET:
    case ZYDIS_MNEMONIC_IRET:
    case ZYDIS_MNEMONIC_IRETD:
    case ZYDIS_MNEMONIC_IRETQ:
        return true;
    default:
        return false;
    }
}

bool is_jump_mnemonic(uint32_t mnemonic_id)
{
    if (is_call_mnemonic(mnemonic_id) || is_return_mnemonic(mnemonic_id)) {
        return false;
    }

    switch (mnemonic_id) {
    case ZYDIS_MNEMONIC_JMP:
        return true;
    default:
        return is_conditional_jump_mnemonic(mnemonic_id);
    }
}

bool is_interrupt_mnemonic(uint32_t mnemonic_id)
{
    switch (mnemonic_id) {
    case ZYDIS_MNEMONIC_INT:
    case ZYDIS_MNEMONIC_INT1:
    case ZYDIS_MNEMONIC_INT3:
#ifdef ZYDIS_MNEMONIC_INTO
    case ZYDIS_MNEMONIC_INTO:
#endif
        return true;
    default:
        return false;
    }
}

InstructionKind classify_instruction_kind(uint32_t mnemonic_id)
{
    if (mnemonic_id == ZYDIS_MNEMONIC_INVALID) {
        return InstructionKind::Invalid;
    }

    if (is_call_mnemonic(mnemonic_id)) {
        return InstructionKind::Call;
    }

    if (is_return_mnemonic(mnemonic_id)) {
        return InstructionKind::Return;
    }

    if (is_interrupt_mnemonic(mnemonic_id)) {
        return InstructionKind::Interrupt;
    }

    if (is_conditional_jump_mnemonic(mnemonic_id)) {
        return InstructionKind::ConditionalJump;
    }

    if (is_jump_mnemonic(mnemonic_id)) {
        return InstructionKind::Jump;
    }

    const char* mnemonic_name = ZydisMnemonicGetString((ZydisMnemonic)mnemonic_id);
    if (!mnemonic_name || mnemonic_name[0] == '\0') {
        return InstructionKind::Unknown;
    }

    return InstructionKind::Normal;
}

std::string FormatInstruction(const Instruction& insn)
{
    return insn.text;
}

} // namespace dbg::analysis

