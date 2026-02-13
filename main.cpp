#include <windows.h>
#include <cassert>
#include <memory>

import radiance.hook.dispatcher;
import radiance;

void __attribute__((noinline)) test_stack_args(int a, int b, int c, int d)
{
    char buf[128];
    __builtin_snprintf(buf, sizeof(buf), "Args: %d %d %d %d", a, b, c, d);
    MessageBoxA(nullptr, buf, "Original Function", MB_OK);
}

void hk_test_stack_args(int a, int b, int c, int d)
{
    char buf[128];
    __builtin_snprintf(buf, sizeof(buf), "HOOKED Args: %d %d %d %d", a, b, c, d);
    MessageBoxA(nullptr, buf, "Hooked Function", MB_OK);
    
    test_stack_args(1, 3, 3, 7);
}

int main()
{
    radiance::C_Radiance radiance;

    auto hook = radiance.create(
        reinterpret_cast<void*>(test_stack_args),
        reinterpret_cast<void*>(hk_test_stack_args)
    );

    if (!hook) {
        assert("Unable to set hook!");
        return -1;
    }

    test_stack_args(11, 22, 33, 44);

    return 0;
}