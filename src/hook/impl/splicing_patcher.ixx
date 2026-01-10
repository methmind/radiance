//
// Created by sexey on 10.01.2026.
//
module;
#include <cassert>
#include <windows.h>
#include <cstdint>
#include <span>

export module radiance.hook.impl.splicing.patcher;

import radiance.process.affinity_scope;

namespace radiance::hook::impl::splicing::patcher
{
    bool WriteToPage(uint8_t* target, const std::span<uint8_t>& data) noexcept
    {
        __int128 newValue = 0; memcpy(&newValue, data.data(), data.size());
        if (reinterpret_cast<uintptr_t>(target) % 16 == 0) {
            __sync_swap(reinterpret_cast<__int128*>(target), newValue);
            return true;
        }

        if (!VirtualLock(target, data.size())) {
            return false;
        }

        __asm__ volatile (
            "movups %1, %0"
            : "=m" (*target)
            : "x" (newValue)
        );

        //todo А нужно ли вообще???
        //FlushInstructionCache

        return VirtualUnlock(target, data.size());
    }

    export bool ApplyPatch(void* target, const std::span<uint8_t>& patch) noexcept
    {
        assert(patch.size() == sizeof(__m128));

        process::C_CpuAffinityScope scope;
        if (!scope.initialize()) {
            assert("Unable to set process CPU affinity!");
            return false;
        }

        DWORD originalProtect = 0;
        if (!VirtualProtect(target, patch.size(), PAGE_EXECUTE_READWRITE, &originalProtect)) {
            assert("Unable to change memory protection!");
            return false;
        }

        if (!WriteToPage(static_cast<uint8_t*>(target), patch)) {
            assert("Unable to write patch to target!");
            return false;
        }

        if (!VirtualProtect(target, patch.size(), originalProtect, &originalProtect)) {
            assert("Unable to restore memory protection!");
        }

        return true;
    }
}
