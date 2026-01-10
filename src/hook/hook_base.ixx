//
// Created by sexey on 06.01.2026.
//
module;

#include <memory>

export module radiance.hook.base;

export namespace radiance::hook
{
    template<typename allocator_t = void>
    class C_BaseHook
    {
    public:
        virtual ~C_BaseHook() noexcept = default;

        explicit C_BaseHook(const std::shared_ptr<allocator_t>& allocator) noexcept : allocator_(allocator) {}

        C_BaseHook(const C_BaseHook&) = delete;
        C_BaseHook& operator=(const C_BaseHook&) = delete;

        C_BaseHook(C_BaseHook&&) = default;
        C_BaseHook& operator=(C_BaseHook&&) = default;

        [[nodiscard]] virtual void* target() = 0;
    private:
        std::shared_ptr<allocator_t> allocator_;
    };
}
