//
// Created by sexey on 10.01.2026.
//
module;
#include <cstdint>
#include <limits>
#include <span>
#include <variant>
#include <vector>
#include "hde/hde64.h"

export module radiance.hook.impl.splicing.rebuilder;

enum class op_code_e : uint8_t
{
    JMP_Rel8 = 0xEB,
    JMP_Rel32 = 0xE9,
    CALL_Rel32 = 0xE8,
    JCC_Rel8_Start = 0x70,
    JCC_Rel8_End [[maybe_unused]] = 0x7F,
    JCC_Rel32_Prefix = 0x0F,
    JCC_Rel32_Base = 0x80, // 0x0F 0x8x
    Loop_Prefix = 0xE0,    // LOOP, LOOPE, LOOPNE, JECXZ
    Ret [[maybe_unused]] = 0xC3,
    Ret_Imm [[maybe_unused]] = 0xC2,
    ModRM_RIP = 0x05       // ModR/M pattern for RIP-relative [00 xxx 101]
};

#pragma pack(push, 1)
struct jmp_abs_s
{
    [[maybe_unused]] uint8_t opcode[2] {0xFF, 0x25};    // FF 25 (JMP [RIP+0])
    [[maybe_unused]] int32_t rip_offset {0};            // 00 00 00 00
    uint64_t address {0};
};

struct call_abs_s
{
    [[maybe_unused]] uint8_t opcode[2] {0xFF, 0x15};    // FF 15 (CALL [RIP+2])
    [[maybe_unused]] int32_t rip_offset {0x00000002};   // 00 00 00 02
    [[maybe_unused]] uint8_t jmp_short[2] {0xEB, 0x08}; // EB 08 (JMP +8 to skip address)
    uint64_t address {0};
};
#pragma pack(pop)

void Append(std::vector<uint8_t>& buf, const void* data, size_t size)
{
    const auto* bytes = static_cast<const uint8_t*>(data);
    buf.insert(buf.end(), bytes, bytes + size);
}

void EmitAbsoluteJmp(std::vector<uint8_t>& buf, uintptr_t target)
{
    jmp_abs_s jmp;
    jmp.address = static_cast<uint64_t>(target);
    Append(buf, &jmp, sizeof(jmp));
}

void EmitAbsoluteCall(std::vector<uint8_t>& buf, uintptr_t target)
{
    call_abs_s call;
    call.address = static_cast<uint64_t>(target);
    Append(buf, &call, sizeof(call));
}

export namespace radiance::hook::impl::splicing
{
    enum class rebuild_error_e
    {
        None,
        DecodingFailed,
        TargetTooFar,       // RIP-смещение > 2GB (неисправимо)
        UnsupportedInstruction, // LOOP/JECXZ наружу
        BufferTooSmall
    };

    using rebuild_err_t = std::variant<std::vector<uint8_t>, rebuild_error_e>;

    constexpr bool IsSuccess(const rebuild_err_t& res)
    {
        return std::holds_alternative<std::vector<uint8_t>>(res);
    }

    constexpr rebuild_error_e GetError(const rebuild_err_t& res)
    {
        if (std::holds_alternative<rebuild_error_e>(res)) {
            return std::get<rebuild_error_e>(res);
        }

        return rebuild_error_e::None;
    }

    const std::vector<uint8_t>& GetBytes(const rebuild_err_t& res)
    {
        return std::get<std::vector<uint8_t>>(res);
    }

    rebuild_err_t RebuildInstructions(void* targetSource, void* targetDest, const std::span<uint8_t>& instructions) noexcept
    {
        std::vector<uint8_t> rebuiltCode;
        rebuiltCode.reserve(instructions.size() * 2);

        const auto oldBase = reinterpret_cast<uintptr_t>(targetSource);
        const auto newBase = reinterpret_cast<uintptr_t>(targetDest);
        size_t offset = 0;

        while (offset < instructions.size()) {
            hde64s hs;
            const auto pInst = reinterpret_cast<const void*>(&instructions[offset]);
            const unsigned int len = hde64_disasm(pInst, &hs);
            if (hs.flags & F_ERROR) {
                return rebuild_error_e::DecodingFailed;
            }

            const uintptr_t currentOldAddr = oldBase + offset;
            // Адрес текущей инструкции в новом буфере
            const uintptr_t currentNewAddr = newBase + rebuiltCode.size();

            // ---------------------------------------------------------
            // 1. RIP-Relative Data Addressing (MOV RAX, [RIP+xxx])
            // ---------------------------------------------------------
            if ((hs.flags & F_MODRM) && (hs.modrm & 0xC7) == static_cast<uint8_t>(op_code_e::ModRM_RIP)) {
                Append(rebuiltCode, pInst, len);

                const auto oldDisp = static_cast<int64_t>(hs.disp.disp32);
                const uintptr_t dataTarget = currentOldAddr + len + oldDisp;

                const int64_t newDisp = dataTarget - (currentNewAddr + len);
                if (newDisp > std::numeric_limits<int32_t>::max() ||
                    newDisp < std::numeric_limits<int32_t>::min()) {
                    return rebuild_error_e::TargetTooFar;
                }

                const size_t immSize = (hs.flags & F_IMM32) ? 4 :
                                       ((hs.flags & F_IMM64) ? 8 :
                                       ((hs.flags & F_IMM16) ? 2 :
                                       ((hs.flags & F_IMM8) ? 1 : 0)));

                const size_t dispPos = rebuiltCode.size() - immSize - 4;
                *reinterpret_cast<int32_t*>(&rebuiltCode[dispPos]) = static_cast<int32_t>(newDisp);
            }
            // ---------------------------------------------------------
            // 2. Relative CALL (E8)
            // ---------------------------------------------------------
            else if (hs.opcode == static_cast<uint8_t>(op_code_e::CALL_Rel32)) {
                const uintptr_t target = currentOldAddr + len + hs.imm.imm32;
                EmitAbsoluteCall(rebuiltCode, target);
            }
            // ---------------------------------------------------------
            // 3. Relative JMP (E9 or EB)
            // ---------------------------------------------------------
            else if (hs.opcode == static_cast<uint8_t>(op_code_e::JMP_Rel32) ||
                     hs.opcode == static_cast<uint8_t>(op_code_e::JMP_Rel8)) {

                const int32_t imm = (hs.opcode == static_cast<uint8_t>(op_code_e::JMP_Rel8)) ?
                    static_cast<int8_t>(hs.imm.imm8) : static_cast<int32_t>(hs.imm.imm32);

                // Проверяем, является ли прыжок внутренним (в пределах копируемого блока)
                if (const uintptr_t target = currentOldAddr + len + imm; target >= oldBase && target < (oldBase + instructions.size())) {
                    Append(rebuiltCode, pInst, len);
                } else {
                    EmitAbsoluteJmp(rebuiltCode, target);
                }
            }
            // ---------------------------------------------------------
            // 4. Conditional Jumps (Jcc)
            // ---------------------------------------------------------
            else if ((hs.opcode & 0xF0) == static_cast<uint8_t>(op_code_e::JCC_Rel8_Start) ||
                (hs.opcode == static_cast<uint8_t>(op_code_e::JCC_Rel32_Prefix) &&
                (hs.opcode2 & 0xF0) == static_cast<uint8_t>(op_code_e::JCC_Rel32_Base))) {
                const bool isShort = (hs.opcode & 0xF0) == static_cast<uint8_t>(op_code_e::JCC_Rel8_Start);
                const int32_t imm = isShort ? static_cast<int8_t>(hs.imm.imm8) : static_cast<int32_t>(hs.imm.imm32);

                if (const uintptr_t target = currentOldAddr + len + imm; target >= oldBase && target < (oldBase + instructions.size())) {
                    Append(rebuiltCode, pInst, len);
                } else {
                    const uint8_t condCode = isShort ? (hs.opcode & 0x0F) : (hs.opcode2 & 0x0F);

                    // x86 трюк: xor 1 инвертирует условие (0x74 JE <-> 0x75 JNE)
                    const uint8_t invertedOp = 0x70 | (condCode ^ 1);
                    const uint8_t invertedJcc[2] = {
                        invertedOp,
                        sizeof(jmp_abs_s)
                    };

                    Append(rebuiltCode, invertedJcc, 2);
                    EmitAbsoluteJmp(rebuiltCode, target);
                }
            }
            // ---------------------------------------------------------
            // 5. LOOP / JECXZ (Unsupported for external)
            // ---------------------------------------------------------
            else if ((hs.opcode & 0xFC) == static_cast<uint8_t>(op_code_e::Loop_Prefix)) {
                const auto imm = static_cast<int8_t>(hs.imm.imm8);
                if (const uintptr_t target = currentOldAddr + len + imm; target < oldBase || target >= (oldBase + instructions.size())) {
                    // Эти инструкции не имеют "длинной" версии. Если они ведут наружу, мы не можем их безопасно перенести в далекий трамплин.
                    return rebuild_error_e::UnsupportedInstruction;
                }

                Append(rebuiltCode, pInst, len);
            }
            // ---------------------------------------------------------
            // 6. Обычная инструкция
            // ---------------------------------------------------------
            else {
                Append(rebuiltCode, pInst, len);
            }

            offset += len;
        }

        // В конце трамплина ставим прыжок обратно в оригинальную функцию.
        const uintptr_t continuationAddr = oldBase + instructions.size();
        EmitAbsoluteJmp(rebuiltCode, continuationAddr);

        return rebuiltCode;
    }
}