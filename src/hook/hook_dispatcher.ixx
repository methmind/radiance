//
// Created by sexey on 07.01.2026.
//
module;
#include <cstdint>
#include <windows.h>

export module radiance.hook.dispatcher;

import radiance.hook.base;

#define SAVE_GP_REGISTERS() \
    /* --- Argument Registers --- */ \
    "push rcx \n" \
    "push rdx \n" \
    "push r8  \n" \
    "push r9  \n" \
    /* --- Non-Volatile Registers --- */ \
    "push rbx \n" \
    "push rbp \n" \
    "push rdi \n" \
    "push rsi \n" \
    "push r10 \n" \
    "push r12 \n" \
    "push r13 \n" \
    "push r14 \n" \
    "push r15 \n"

#define RESTORE_GP_REGISTERS() \
    /* --- Non-Volatile Registers --- */ \
    "pop r15 \n" \
    "pop r14 \n" \
    "pop r13 \n" \
    "pop r12 \n" \
    "pop r10 \n" \
    "pop rsi \n" \
    "pop rdi \n" \
    "pop rbp \n" \
    "pop rbx \n" \
    /* --- Argument Registers --- */ \
    "pop r9  \n" \
    "pop r8  \n" \
    "pop rdx \n" \
    "pop rcx \n"

namespace radiance::hook
{
    static thread_local uint32_t RECURSION_DEPTH = 0;

    extern "C" bool __fastcall CheckRecursionAndEnter()
    {
        if (RECURSION_DEPTH > 0) {
            return true;
        }

        RECURSION_DEPTH++;
        return false;
    }

    extern "C" void __fastcall LeaveHookContext()
    {
        RECURSION_DEPTH--;
    }

    extern "C" void* __fastcall GetHookTarget(void* hookPtr)
    {
        return static_cast<C_BaseHook<void>*>(hookPtr)->target();
    }

    //DispatcherEntry(C_BaseHook<void>* hook == R10);
    export bool __declspec(naked) __fastcall DispatcherEntry() noexcept
    {
        asm volatile (
            ".intel_syntax noprefix \n"

            // Сохраняем R10 (this pointer), так как он не сохраняется в SAVE_GP_REGISTERS
            // Используем стек.
            "push r10 \n"

            SAVE_GP_REGISTERS()

            // Выравнивание и вызов CheckRecursion
            "sub rsp, 0x28 \n"            // Shadow space (32) + Align (8) = 40
            "call CheckRecursionAndEnter \n"
            "add rsp, 0x28 \n"

            RESTORE_GP_REGISTERS()

            "cmp al, 1 \n"
            "je ExitRecursion \n"

            // --- Входим в хук (AL=0) ---

            // Подготовка к вызову GetHookTarget
            // Нам нужно достать R10 (который мы запушили самым первым).
            // R10 лежит на вершине стека (RSP указывает на него).
            "pop r10 \n"                  // Восстанавливаем R10

            // Сохраняем аргументы RCX, RDX, R8, R9, так как GetHookTarget их убьет.
            "push r9 \n"
            "push r8 \n"
            "push rdx \n"
            "push rcx \n"

            // Вызов GetHookTarget(r10)
            "sub rsp, 0x20 \n"            // Shadow Space (32).
                                          // Стек был ...0 (4 пуша). sub 32 -> ...0.
                                          // call -> ...8 (Inside). OK.
            "mov rcx, r10 \n"
            "call GetHookTarget \n"
            "add rsp, 0x20 \n"

            // Восстанавливаем аргументы
            "pop rcx \n"
            "pop rdx \n"
            "pop r8 \n"
            "pop r9 \n"

            // Вызов Payload (адрес в RAX)
            // Стек сейчас ...0 (чистый).
            // WinAPI требует ...8 на входе (return address pushed).
            // Значит перед call стек должен быть ...0.
            "sub rsp, 0x20 \n"            // Shadow Space (32). ...0 -> ...0.
            "call rax \n"                 // call -> ...8 (Inside).
            "add rsp, 0x20 \n"

            SAVE_GP_REGISTERS()
            "sub rsp, 0x28 \n"
            "call LeaveHookContext \n"
            "add rsp, 0x28 \n"
            RESTORE_GP_REGISTERS()

            "xor eax, eax \n"             // return false (0)
            "ret \n"

            "ExitRecursion: \n"
            // Если рекурсия, нужно вернуть R10 на место (мы его запушили в начале)
            // R10 лежит на стеке.
            "pop r10 \n"

            "mov eax, 1 \n"
            "ret \n"
            ".att_syntax prefix \n"
        );
    }
}
