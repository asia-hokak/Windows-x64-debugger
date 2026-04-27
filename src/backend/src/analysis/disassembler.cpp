#include "disassembler.h"

#include <Zydis/Zydis.h>

#include <algorithm>
#include <string>
#include <vector>

#include "../core/memory.h"
#include "debug_session.h"
#include "instruction_formatter.h"

namespace dbg::analysis {

static constexpr size_t k_instruction_probe_bytes = 16; // x64 最大的 instruction size 是 15, 為了方便 align 成 16

static bool address_in_instruction(const Instruction& insn, uint64_t address)
{
    const uint64_t size = insn.size == 0 ? 1 : uint64_t(insn.size);
    const uint64_t end = insn.address + size;
    if (end < insn.address) {
        return address >= insn.address;
    }
    return address >= insn.address && address < end;
}

static bool find_containing_index(const std::vector<Instruction>& items, uint64_t address, size_t& index_out)
{
    for (size_t i = 0; i < items.size(); ++i) {
        if (address_in_instruction(items[i], address)) {
            index_out = i;
            return true;
        }
    }
    return false;
}

static std::vector<Instruction> slice_items(const std::vector<Instruction>& items, size_t begin, size_t end)
{
    if (begin >= end || begin >= items.size()) {
        return {};
    }
    const size_t safe_end = (std::min)(end, items.size());
    return std::vector<Instruction>(items.begin() + begin, items.begin() + safe_end);
}

static void restore_software_breakpoint_bytes(uint64_t base_address,
                                              uint8_t* buffer,
                                              size_t size,
                                              const DebugSession& session)
{
    if (!buffer || size == 0) {
        return;
    }

    auto breakpoint_it = session.breakpoints.begin();
    while (breakpoint_it != session.breakpoints.end()) {
        const dbgtype::Breakpoint& breakpoint = breakpoint_it->second;
        if (!breakpoint.enabled) {
            ++breakpoint_it;
            continue;
        }

        const bool is_int3_breakpoint =
            breakpoint.kind == dbgtype::BreakpointKind::Software ||
            breakpoint.kind == dbgtype::BreakpointKind::StepOver;
        if (!is_int3_breakpoint) {
            ++breakpoint_it;
            continue;
        }

        if (breakpoint.address < base_address) { // 如果 breakpoint address < decode start address
            ++breakpoint_it;
            continue;
        }

        const uint64_t offset_u64 = breakpoint.address - base_address; // 如果 breakpoint address > decode start address
        if (offset_u64 >= size) {
            ++breakpoint_it;
            continue;
        }

        const size_t offset = (size_t)(offset_u64);
        buffer[offset] = breakpoint.original_byte;
        ++breakpoint_it;
    }
}

static bool decode_linear_window(uint64_t base_address,
                                 size_t max_bytes,
                                 bool is_x64,
                                 Memory& memory,
                                 const DebugSession& session,
                                 std::vector<Instruction>& out_items)
{
    out_items.clear();
    if (max_bytes == 0) {
        return false;
    }

    std::vector<uint8_t> buffer(max_bytes, 0);
    size_t bytes_read = 0;
    if (!memory.read(base_address, buffer.data(), buffer.size(), &bytes_read) || bytes_read == 0) {
        return false;
    }
    restore_software_breakpoint_bytes(base_address, buffer.data(), bytes_read, session); // 把有設定 software breakpoint (0xCC) 的地方先還原，以防 decode 失敗

    // 初始化 zydis decoder，把 instruction 的格式設定為 intel syntax
    ZydisDecoder decoder;
    if (is_x64) {
        ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
    } else {
        ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LEGACY_32, ZYDIS_STACK_WIDTH_32);
    }

    ZydisFormatter formatter;
    ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);

    size_t offset = 0;
    while (offset < bytes_read) {
        // decode 單一 instruction
        ZydisDecodedInstruction decoded = {};
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
        if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder,
                                                 buffer.data() + offset,
                                                 bytes_read - offset,
                                                 &decoded,
                                                 operands))) {
            break;
        }

        // 轉換為自己專案的 instruction type
        Instruction insn = {}; 
        insn.address = base_address + uint64_t(offset);
        insn.size = decoded.length == 0 ? 1 : decoded.length;
        insn.mnemonic_id = uint32_t(decoded.mnemonic);
        insn.kind = classify_instruction_kind(insn.mnemonic_id);
        insn.readable = true;

        const size_t insn_size = std::min<size_t>(insn.size, bytes_read - offset); // 確保不越界
        insn.bytes.assign(buffer.begin() + offset, buffer.begin() + offset + insn_size);

        // 把 instruction 轉成 intel syntax
        char text_buffer[256] = {};
        if (ZYAN_SUCCESS(ZydisFormatterFormatInstruction(&formatter,
                                                         &decoded,
                                                         operands,
                                                         decoded.operand_count,
                                                         text_buffer,
                                                         sizeof(text_buffer),
                                                         insn.address,
                                                         nullptr))) {
            insn.text = text_buffer;
        } else {
            insn.text = "???";
            insn.readable = false;
        }

        const char* mnemonic = ZydisMnemonicGetString(decoded.mnemonic);
        if (mnemonic) {
            insn.mnemonic = mnemonic;
        }

        const size_t split = insn.text.find(' ');
        if (split != std::string::npos && split + 1 < insn.text.size()) {
            insn.operands = insn.text.substr(split + 1);
        } else {
            insn.operands.clear();
        }

        out_items.push_back(std::move(insn));
        offset += insn.size;
    }

    return !out_items.empty();
}

static std::vector<Instruction> merge_and_center_window(const std::vector<Instruction>& up_items,
                                                        const std::vector<Instruction>& down_items,
                                                        uint64_t address,
                                                        size_t count)
{
    std::vector<Instruction> merged = up_items;
    if (!merged.empty() && !down_items.empty() && merged.back().address == down_items.front().address) {
        merged.pop_back();
    }
    merged.insert(merged.end(), down_items.begin(), down_items.end());

    size_t center_index = 0;
    if (!find_containing_index(merged, address, center_index)) {
        return merged;
    }

    const size_t begin = center_index > count ? (center_index - count) : 0;
    const size_t end = (std::min)(merged.size(), center_index + count + 1);
    return slice_items(merged, begin, end);
}

Disassembler::Disassembler(DebugSession& session)
    : session_(session)
{
}

std::vector<Instruction> Disassembler::disassembly_window_down(uint64_t address,
                                                               size_t count,
                                                               bool is_x64,
                                                               Memory& memory) const
{
    const size_t target_count = count + 1;
    const size_t decode_bytes = k_instruction_probe_bytes * (count + 2);
    const size_t max_backtrack = decode_bytes;

    // 因為 decode 結果不一定會是完美的符合 count + 1，所以這邊迴圈一下直到幾乎完美符合，但通常第一次就會完美符合
    std::vector<Instruction> best_items; 
    for (size_t back = 0; back <= max_backtrack && back <= address; ++back) { // 如果 decode 失敗把 base 下移
        const uint64_t base = address - uint64_t(back);
        std::vector<Instruction> decoded;
        if (!decode_linear_window(base, decode_bytes, is_x64, memory, session_, decoded)) {
            continue;
        }

        size_t index = 0;
        if (!find_containing_index(decoded, address, index)) { // 確保 instruction address 起點
            continue;
        }

        const std::vector<Instruction> candidate = slice_items(decoded, index, index + target_count); // 將 instruction 數量控制在 count + 1
        if (candidate.size() > best_items.size()) {
            best_items = candidate;
        }
        if (candidate.size() >= target_count) {
            return candidate;
        }
    }

    return best_items;
}

std::vector<Instruction> Disassembler::disassembly_window_up(uint64_t address,
                                                             size_t count,
                                                             bool is_x64,
                                                             Memory& memory) const
{
    const size_t target_count = count + 1;
    const size_t decode_bytes = k_instruction_probe_bytes * (count + 2); 
    const uint64_t initial_back = uint64_t(k_instruction_probe_bytes) * uint64_t(count + 1);
    const uint64_t start = address > initial_back ? (address - initial_back) : 0;
    const size_t max_backtrack = decode_bytes;

    // 邏輯類似 disassembly_window_down，只不過是把 start address 設在 address 以上，慢慢往下 decode
    std::vector<Instruction> best_items;
    for (size_t back = 0; back <= max_backtrack && back <= start; ++back) {
        const uint64_t base = start - uint64_t(back);
        std::vector<Instruction> decoded;
        if (!decode_linear_window(base, decode_bytes, is_x64, memory, session_, decoded)) {
            continue;
        }

        size_t index = 0;
        if (!find_containing_index(decoded, address, index)) {
            continue;
        }

        const size_t begin = index > count ? (index - count) : 0;
        const std::vector<Instruction> candidate = slice_items(decoded, begin, index + 1);
        if (candidate.size() > best_items.size()) {
            best_items = candidate;
        }
        if (candidate.size() >= target_count) {
            return candidate;
        }
    }

    return best_items;
}

std::vector<Instruction> Disassembler::disassembly_at(uint64_t address,
                                                      size_t count,
                                                      bool is_x64,
                                                      Memory& memory) const
{
    std::vector<Instruction> down_items = disassembly_window_down(address, count, is_x64, memory);
    std::vector<Instruction> up_items = disassembly_window_up(address, count, is_x64, memory);

    if (down_items.empty()) {
        return up_items;
    }
    if (up_items.empty()) {
        return down_items;
    }

    //  對比 down_items 和 up_items 都有 decode 到的東西是不是同個 address
    if (!down_items.empty() && !up_items.empty() &&
        down_items.front().address != up_items.back().address) {
        // 解不到直接認定 down_items 的第一個 instruction 為合法 address，從那邊開始往下解
        std::vector<Instruction> realigned_up =
            disassembly_window_up(down_items.front().address, count, is_x64, memory); 
        if (!realigned_up.empty()) {
            up_items = realigned_up;
        }
    }

    if (down_items.front().address != up_items.back().address) {
        return down_items;
    }

    std::vector<Instruction> merged = merge_and_center_window(up_items, down_items, address, count);
    size_t center_index = 0;
    if (!find_containing_index(merged, address, center_index)) {
        return down_items;
    }
    return merged;
}

}  // namespace dbg::analysis
