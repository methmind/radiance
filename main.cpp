#include <windows.h>
#include <cassert>
#include <cstdint>
#include <memory>

import radiance.memory.allocator;
import radiance.memory.stl;
import radiance.hook.impl.splicing;
import radiance.hook.dispatcher;

void __attribute__((noinline)) test_stack_args(int a, int b, int c, int d)
{
    char buf[128];
    __builtin_snprintf(buf, sizeof(buf), "Args: %d %d %d %d", a, b, c, d);
    MessageBoxA(nullptr, buf, "Original Function", MB_OK);
}

// Hook that matches the signature
void hk_test_stack_args(int a, int b, int c, int d, int e, int f)
{
    char buf[128];
    __builtin_snprintf(buf, sizeof(buf), "HOOKED Args: %d %d %d %d", a, b, c, d);
    MessageBoxA(nullptr, buf, "Hooked Function", MB_OK);
    
    test_stack_args(a, b, c, d);
}

int main()
{
    auto allocator = std::make_shared<radiance::memory::stl::C_StlAllocator<uint8_t>>(
        std::make_shared<radiance::memory::C_MemoryAllocator>()
    );

    auto hook = std::make_shared<radiance::hook::impl::splicing::C_SplicingHook<radiance::memory::stl::C_StlAllocator<uint8_t>>>(allocator);

    // Install hook on test_stack_args
    if (!hook->install((void*)test_stack_args,
        reinterpret_cast<void*>(hk_test_stack_args), reinterpret_cast<void*>(radiance::hook::DispatcherEntry))) {
        assert("Unable to set hook!");
        return -1;
    }

    // Call with specific pattern: 11, 22, 33, 44, 55, 66
    test_stack_args(11, 22, 33, 44);

    return 0;
}