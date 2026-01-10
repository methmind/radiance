//
// Created by sexey on 10.01.2026.
//

module;
#include <memory>

export module radiance.memory.stl;

import radiance.memory.allocator;

export namespace radiance::memory::stl
{
    template <typename T>
    class C_StlAllocator
    {
    public:
        using value_type = T;

        explicit C_StlAllocator(const std::shared_ptr<C_MemoryAllocator>& allocator) noexcept
            : allocator_(allocator) {}

        template<typename U>
        explicit C_StlAllocator(const C_StlAllocator<U>& other) noexcept
            : allocator_(other.backend()) {}

        [[nodiscard]] T* allocate(const size_t n)
        {
            return static_cast<T*>(this->allocator_->alloc(n * sizeof(T)));
        }

        void deallocate(T* p, size_t) noexcept
        {
            this->allocator_->free(p);
        }

        bool operator==(const C_StlAllocator& other) const noexcept {
            return allocator_ == other.allocator_;
        }

        [[nodiscard]] std::shared_ptr<C_MemoryAllocator> backend() const noexcept {
            return allocator_;
        }

    private:
        std::shared_ptr<C_MemoryAllocator> allocator_;
    };
}
