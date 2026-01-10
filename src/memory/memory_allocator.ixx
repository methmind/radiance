//
// Created by sexey on 08.01.2026.
//

module;
#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

export module radiance.memory.allocator;

import radiance.memory.page;
import radiance.memory.metadata;

export namespace radiance::memory
{
    constexpr uint32_t MAGIC_COOKIE = 0xDEADBEEF;

    constexpr size_t DEFAULT_PAGE_SIZE = 64 * 1024;

    class C_MemoryAllocator
    {
    public:
        ~C_MemoryAllocator() = default;

        C_MemoryAllocator() = default;

        [[nodiscard]] void* alloc(size_t size);

        void free(void* ptr);

    private:

        void insertAndCoalesce(free_block_s newBlock);

        static constexpr size_t AlignCookie(const size_t n) noexcept
        {
            return (n + (alignof(block_cookie_s) - 1)) & ~(alignof(block_cookie_s) - 1);
        }

        std::vector<C_MemoryPage> pages_;
        std::vector<free_block_s> freeBlocks_;
    };

    void* C_MemoryAllocator::alloc(const size_t size)
    {
        const size_t userSize = (size == 0) ? 1 : size;
        const size_t need = AlignCookie(sizeof(block_cookie_s) + userSize);
        constexpr size_t MIN_USABLE_BLOCK = AlignCookie(sizeof(block_cookie_s) + 16);

        uint8_t* resultPtr = nullptr;
        size_t allocTotal = need;

        // Поиск свободного блока (First Fit)
        const auto it = std::ranges::find_if(this->freeBlocks_, [need](const free_block_s& b) {
            return b.size >= need;
        });

        if (it != this->freeBlocks_.end()) {
            resultPtr = it->start;
            const size_t foundSize = it->size;

            // Проверяем, можно ли отрезать кусок
            if (const size_t remaining = foundSize - need; remaining >= MIN_USABLE_BLOCK) {
                // Отрезаем от начала, хвост остается на месте, сортировка не нарушается
                it->start += need;
                it->size = remaining;
            } else {
                // Удаляем блок целиком
                this->freeBlocks_.erase(it);
                allocTotal = foundSize;
            }
        } else {
            // Аллокация новой страницы
            const size_t newPageSize = std::max(need, DEFAULT_PAGE_SIZE);
            auto& newPage = this->pages_.emplace_back(newPageSize);
            if (!newPage.initialize()) {
                this->pages_.pop_back();
                return nullptr;
            }

            resultPtr = newPage.get();

            // Если остался хвост, добавляем его в список свободных
            if (const size_t remaining = newPageSize - need; remaining >= MIN_USABLE_BLOCK) {
                insertAndCoalesce({ resultPtr + need, remaining });
            } else {
                allocTotal = newPageSize;
            }
        }

        // Запись метаданных
        if (resultPtr) {
            new (static_cast<void*>(resultPtr)) block_cookie_s{ allocTotal, MAGIC_COOKIE };
            return resultPtr + sizeof(block_cookie_s);
        }

        return nullptr;
    }

    void C_MemoryAllocator::free(void* ptr)
    {
        if (!ptr) {
            return;
        }

        auto* cookiePtr = static_cast<uint8_t*>(ptr) - sizeof(block_cookie_s);
        auto* cookie = reinterpret_cast<block_cookie_s*>(cookiePtr);
        if (cookie->magic != MAGIC_COOKIE) {
            // Либо double-free, либо мусор
            return;
        }

        // Инвалидация для защиты от повторного освобождения
        cookie->magic = 0;
        insertAndCoalesce({ cookiePtr, cookie->size });
    }

    void C_MemoryAllocator::insertAndCoalesce(free_block_s newBlock)
    {
        const auto it = std::upper_bound(this->freeBlocks_.begin(), this->freeBlocks_.end(), newBlock);

        auto insertPos = it;
        if (it != this->freeBlocks_.end() && (newBlock.start + newBlock.size == it->start)) {
            newBlock.size += it->size;
            insertPos = this->freeBlocks_.erase(it);
        }

        if (insertPos != this->freeBlocks_.begin()) {
            if (const auto prevIt = std::prev(insertPos); prevIt->start + prevIt->size == newBlock.start) {
                prevIt->size += newBlock.size;
                return;
            }
        }

        // Если не влились в левого, вставляем (возможно, укрупненный) блок
        this->freeBlocks_.insert(insertPos, newBlock);
    }
}
