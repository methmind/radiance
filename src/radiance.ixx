//
// Created by sexey on 18.01.2026.
//

module;
#include <memory>
#include <cstdint>

export module radiance;

import radiance.memory.allocator;
import radiance.memory.stl;
import radiance.hook.impl.splicing;
import radiance.hook.dispatcher;

export namespace radiance
{
    class C_Radiance
    {
    public:
        using allocator_t = memory::stl::C_StlAllocator<uint8_t>;

        C_Radiance() : allocator_(std::make_shared<allocator_t>(std::make_shared<memory::C_MemoryAllocator>())) {}

        auto create(void* target, void* detour)
        {
            auto hook = std::make_shared<hook::impl::splicing::C_SplicingHook<allocator_t>>(this->allocator_);
            if (hook->install(target, detour, reinterpret_cast<void*>(hook::DispatcherEntry))) {
                return hook;
            }

            return std::shared_ptr<hook::impl::splicing::C_SplicingHook<allocator_t>>{nullptr};
        }

    private:
        std::shared_ptr<allocator_t> allocator_;
    };
}
