//
// Created by sexey on 08.01.2026.
//

module;
#include <windows.h>
#include <memory>

export module radiance.memory.page;

export namespace radiance::memory
{
    struct virtual_destructor_s
    {
        void operator()(void* ptr) const noexcept {
            if (ptr) {
                VirtualFree(ptr, 0, MEM_RELEASE);
            }
        }
    };

    class C_MemoryPage
    {
    public:
        ~C_MemoryPage() noexcept = default;
        explicit C_MemoryPage(const size_t size) noexcept : totalSize_(size) {}
        C_MemoryPage(const C_MemoryPage&) = delete;
        C_MemoryPage& operator=(const C_MemoryPage&) = delete;
        C_MemoryPage(C_MemoryPage&&) = default;
        C_MemoryPage& operator=(C_MemoryPage&&) = default;

        bool initialize() noexcept;

        [[nodiscard]] inline uint8_t* get() const noexcept { return this->base_.get(); }

        [[nodiscard]] size_t size() const noexcept { return this->totalSize_; }

        inline bool owns(const uint8_t* ptr) const noexcept
        {
            return ptr >= this->base_.get() && ptr < (this->base_.get() + this->totalSize_);
        }

    private:
        std::unique_ptr<uint8_t, virtual_destructor_s> base_;
        size_t totalSize_;
    };

    bool C_MemoryPage::initialize() noexcept
    {
        this->base_.reset(static_cast<uint8_t*>(VirtualAlloc(nullptr, this->totalSize_, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)));
        return this->base_ != nullptr;
    }
}
