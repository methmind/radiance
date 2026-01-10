//
// Created by sexey on 10.01.2026.
//
module;
#include <cassert>
#include <memory>
#include <vector>
#include <xmmintrin.h>

#include "hde/hde64.h"

export module radiance.hook.impl.splicing;

import radiance.hook.base;
import radiance.hook.impl.splicing.trampoline;
import radiance.hook.impl.splicing.patcher;

#pragma pack(push, 1)
struct hook_stub_s
{
    const uint8_t mov_rax[2] { 0x48, 0xB8 };
    uint64_t rax_ptr = 0;
    const uint8_t jmp_rax[2] { 0xFF, 0xE0 };
};
#pragma pack(pop)

export namespace radiance::hook::impl::splicing
{
    template<typename allocator_t>
    class C_SplicingHook : public C_BaseHook<allocator_t>
    {
    public:
        ~C_SplicingHook() noexcept override { uninstall(); }

        explicit C_SplicingHook(const std::shared_ptr<allocator_t>& allocator) noexcept :
            C_BaseHook<allocator_t>(allocator), target_(nullptr), detour_(nullptr), trampoline_(allocator) { this->originalProlog_.reserve(sizeof(__m128)); /* We need aligned memory */ }

        [[nodiscard]] void* target() override { return this->detour_; }

        [[nodiscard]] bool install(void* target, void* detour, void* dispatcher) noexcept;

        bool uninstall() noexcept;

    private:

        static size_t GetInstructionsBoundary(void* address, size_t minSize) noexcept;

        bool saveProlog(void* target, uint32_t size) noexcept;

        void* target_;
        void* detour_;
        trampoline::C_SplicingTrampoline<allocator_t> trampoline_;
        std::vector<uint8_t> originalProlog_;
    };

    template <typename allocator_t>
    bool C_SplicingHook<allocator_t>::install(void* target, void* detour, void* dispatcher) noexcept
    {
        if (!saveProlog(target, sizeof(hook_stub_s))) {
            assert("Unable to save prolog!");
            return false;
        }

        if (!this->trampoline_.initialize(target, this->originalProlog_, dispatcher, this)) {
            assert("Unable to initialize trampoline!");
            return false;
        }

        uint8_t stub[sizeof(__m128)];
        hook_stub_s newHook;
        newHook.rax_ptr = reinterpret_cast<uintptr_t>(this->trampoline_.getTrampoline());

        memcpy(stub, &newHook, sizeof(hook_stub_s));
        memcpy(&stub[sizeof(hook_stub_s)], static_cast<uint8_t*>(target) + sizeof(hook_stub_s), sizeof(stub) - sizeof(hook_stub_s));

        if (!patcher::ApplyPatch(target, { stub, sizeof(stub) })) {
            assert("Unable to apply splicing hook patch!");
            return false;
        }

        this->target_ = target;
        this->detour_ = detour;

        return true;
    }

    template <typename allocator_t>
    bool C_SplicingHook<allocator_t>::uninstall() noexcept
    {
        if (this->originalProlog_.empty() || !this->target_) {
            return false;
        }

        uint8_t stub[sizeof(__m128)];
        memcpy(stub, this->originalProlog_.data(), this->originalProlog_.size());
        memcpy(&stub[this->originalProlog_.size()], static_cast<uint8_t*>(this->target_) + this->originalProlog_.size(), sizeof(stub) - this->originalProlog_.size());

        if (!patcher::ApplyPatch(this->target_, { stub, sizeof(stub) })) {
            assert("Unable to restore original prolog!");
            return false;
        }

        return true;
    }

    template <typename allocator_t>
    size_t C_SplicingHook<allocator_t>::GetInstructionsBoundary(void* address, const size_t minSize) noexcept
    {
        size_t offset = 0;
        hde64s hs;

        while (offset < minSize) {
            const size_t instrSize = hde64_disasm(static_cast<uint8_t*>(address) + offset, &hs);
            if (instrSize == 0 || hs.flags & F_ERROR) {
                return 0;
            }

            offset += instrSize;
        }

        return offset;
    }

    template <typename allocator_t>
    bool C_SplicingHook<allocator_t>::saveProlog(void* target, const uint32_t size) noexcept
    {
        const auto instrBoundary = GetInstructionsBoundary(target, size);
        if (!instrBoundary) {
            assert("Unable to get instruction boundary!");
            return false;
        }

        if (instrBoundary > sizeof(__m128)) {
            assert("Unable to save prolog, instruction boundary too big!");
            return false;
        }

        this->originalProlog_.resize(instrBoundary);
        memcpy(this->originalProlog_.data(), target, instrBoundary);
        return true;
    }
}
