//
// Created by sexey on 07.01.2026.
//
module;
#include <cstdint>

export module radiance.hook.dispatcher;

import radiance.hook.base;

#define SAVE_GP_REGISTERS() \
    /* --- Аргументы (volatile) --- */ \
    "push rcx \n" \
    "push rdx \n" \
    "push r8  \n" \
    "push r9  \n" \
    /* --- Non-volatile --- */ \
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
    /* --- Восстанавливаем Non-volatile --- */ \
    "pop r15 \n" \
    "pop r14 \n" \
    "pop r13 \n" \
    "pop r12 \n" \
    "pop r10 \n" \
    "pop rsi \n" \
    "pop rdi \n" \
    "pop rbp \n" \
    "pop rbx \n" \
    /* --- Восстанавливаем аргументы --- */ \
    "pop r9  \n" \
    "pop r8  \n" \
    "pop rdx \n" \
    "pop rcx \n"

namespace radiance::hook
{
    static thread_local uint32_t RECURSION_DEPTH = 0;

    extern "C" bool __attribute__((used)) __attribute__((noinline)) __fastcall CheckRecursionAndEnter()
    {
        if (RECURSION_DEPTH > 0) {
            return true;
        }

        RECURSION_DEPTH++;
        return false;
    }

    extern "C" void __attribute__((used)) __attribute__((noinline)) __fastcall LeaveHookContext()
    {
        RECURSION_DEPTH--;
    }

    extern "C" void* __attribute__((used)) __attribute__((noinline)) __fastcall GetHookTarget(void* hookPtr)
    {
        return static_cast<C_BaseHook<void>*>(hookPtr)->target();
    }

    //DispatcherEntry(C_BaseHook<void>* hook == R10);
    export bool __attribute__ ((naked)) __fastcall DispatcherEntry() noexcept
    {
        asm volatile (
            ".intel_syntax noprefix \n"
            // Сохраняем R10 (this)
            "push r10 \n"

            SAVE_GP_REGISTERS()

            // Выравниваем стек и чекаем рекурсию
            "sub rsp, 0x28 \n"            // Shadow space (32) + Align (8) = 40
            "call CheckRecursionAndEnter \n"
            "add rsp, 0x28 \n"

            RESTORE_GP_REGISTERS()

            "cmp al, 1 \n"
            "je ExitRecursion \n"

            // --- Заходим в хук (AL=0) ---

            // Достаем R10 (this)
            "pop r10 \n"

            // Сейвим аргументы
            "push r9 \n"
            "push r8 \n"
            "push rdx \n"
            "push rcx \n"

            // Зовем GetHookTarget(r10)
            "sub rsp, 0x20 \n"            // Shadow Space (32)
            "mov rcx, r10 \n"
            "call GetHookTarget \n"
            "add rsp, 0x20 \n"

            // Восстанавливаем аргументы
            "pop rcx \n"
            "pop rdx \n"
            "pop r8 \n"
            "pop r9 \n"

            // Вызов Payload (адрес в RAX). Копируем стековые аргументы.
            "mov r11, rax \n"             // Сейвим адрес Target в R11
            
            // Сейвим реги, которые убьет rep movsq
            "push rdi \n"
            "push rsi \n"
            "push rcx \n"                 // [RSP+0x00] = RCX
            
            // Выделяем место под аргументы (160 байт total).
            // 0xA8 (168) выравнивает стек по 16 байт с учетом 3 пушей выше.
            "sub rsp, 0xA8 \n"
            
            // Копируем аргументы. Смещение 0x120 = Frame(0xA8) + Pushes(24) + Overhead(56) + OldFrame(40)
            "lea rsi, [rsp + 0x120] \n"   // Source
            "lea rdi, [rsp + 0x20] \n"    // Dest (после Shadow Space)
            "mov rcx, 16 \n"              // 16 qwords (128 байт)
            "rep movsq \n"
            
            // Восстанавливаем реги перед вызовом
            "mov rcx, [rsp + 0xA8] \n"    // Arg1
            "mov rsi, [rsp + 0xB0] \n"    // RSI
            "mov rdi, [rsp + 0xB8] \n"    // RDI
            
            "call r11 \n"                 // Дергаем Detour
            
            "add rsp, 0xA8 \n"            // Чистим стек
            
            "pop rcx \n"
            "pop rsi \n"
            "pop rdi \n"

            // Сейвим результат (RAX, RDX)
            "push rdx \n"
            "push rax \n"

            SAVE_GP_REGISTERS() 
            "sub rsp, 0x28 \n"
            "call LeaveHookContext \n"
            "add rsp, 0x28 \n"
            RESTORE_GP_REGISTERS()
            
            // Восстанавливаем результат
            "pop rax \n"
            "pop rdx \n"    

            "xor r11, r11 \n"             // r11 = 0 (Успех)
            "ret \n"

            "ExitRecursion: \n"
            "pop r10 \n"                  // Возвращаем R10 на место
            "mov r11, 1 \n"               // r11 = 1 (Рекурсия сработала)
            "ret \n"
            ".att_syntax prefix \n"
        );
    }
}
