//
// Created by sexey on 10.01.2026.
//

module;
#include <memory>
#include <vector>
#include <span>

export module radiance.hook.impl.splicing.trampoline;

import radiance.hook.impl.splicing.rebuilder;

#pragma pack(push, 1)
struct trampoline_stub_s
{
    const uint8_t push_rcx[1] { 0x51 };
    const uint8_t sub_rsp_28[4] { 0x48, 0x83, 0xEC, 0x28 };
    const uint8_t mov_r10[2] { 0x49, 0xBA };
    uint64_t r10_ptr = 0;
    const uint8_t mov_rax[2] { 0x48, 0xB8 };
    uint64_t rax_ptr = 0;
    const uint8_t call_rax[2] { 0xFF, 0xD0 };
    const uint8_t add_rsp_28[4] { 0x48, 0x83, 0xC4, 0x28 };
    const uint8_t pop_rcx[1] { 0x59 };
    const uint8_t cmp_r11b_1[4] { 0x41, 0x80, 0xFB, 0x01 };
    const uint8_t jne[1] { 0x75 };
    int8_t jne_offset = 0;
    //original code
    //ret == 0xC3
};
#pragma pack(pop)

export namespace radiance::hook::impl::splicing::trampoline
{
    template<typename allocator_t>
    class C_SplicingTrampoline
    {
    public:
        ~C_SplicingTrampoline() noexcept { this->allocator_->deallocate(this->trampoline_, 0); }

        explicit C_SplicingTrampoline(const std::shared_ptr<allocator_t>& allocator) noexcept :
            allocator_(allocator), trampoline_(nullptr) {}

        [[nodiscard]] bool initialize(void* target, const std::span<uint8_t>& original, void* detour, void* args) noexcept;

        [[nodiscard]] uint8_t* getTrampoline() const noexcept { return this->trampoline_; }

    private:
        std::shared_ptr<allocator_t> allocator_;
        uint8_t* trampoline_;
    };

    template <typename allocator_t>
    bool C_SplicingTrampoline<allocator_t>::initialize(void* target, const std::span<uint8_t>& original, void* detour, void* args) noexcept
    {
        {
            const auto tmpRebuilt = RebuildInstructions(target, nullptr, original);
            if (!IsSuccess(tmpRebuilt)) {
                return false;
            }

            this->trampoline_ = static_cast<uint8_t*>(this->allocator_->allocate(GetBytes(tmpRebuilt).size() + sizeof(trampoline_stub_s) + 1)); //+1 == ret opcode
            if (!this->trampoline_) {
                return false;
            }
        }

        const auto rebuiltPtr = this->trampoline_ + sizeof(trampoline_stub_s);
        const auto rebuiltStatus = RebuildInstructions(target, rebuiltPtr, original);
        if (!IsSuccess(rebuiltStatus)) {
            return false;
        }

        auto& rebuilt = GetBytes(rebuiltStatus);

        trampoline_stub_s stub;
        stub.r10_ptr = reinterpret_cast<uint64_t>(args);
        stub.rax_ptr = reinterpret_cast<uint64_t>(detour);
        stub.jne_offset = static_cast<int8_t>(rebuilt.size()); //+1 == ret opcode

        memcpy(this->trampoline_, &stub, sizeof(trampoline_stub_s));
        memcpy(rebuiltPtr, rebuilt.data(), rebuilt.size());
        this->trampoline_[sizeof(trampoline_stub_s) + rebuilt.size()] = 0xC3; //ret opcode

        return true;
    }
}
