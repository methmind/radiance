#include <windows.h>
#include <cassert>
#include <cstdint>
#include <memory>

import radiance.memory.allocator;
import radiance.memory.stl;
import radiance.hook.impl.splicing;
import radiance.hook.dispatcher;

WINBOOL hkBeep(DWORD dwFreq, DWORD dwDuration)
{
    MessageBoxA(nullptr, "Beep Hooked!", "Info", MB_OK);
    return Beep(dwFreq, dwDuration);
}

int main()
{
    auto allocator = std::make_shared<radiance::memory::stl::C_StlAllocator<uint8_t>>(
        std::make_shared<radiance::memory::C_MemoryAllocator>()
    );

    auto hook = std::make_shared<radiance::hook::impl::splicing::C_SplicingHook<radiance::memory::stl::C_StlAllocator<uint8_t>>>(allocator);
    if (!hook->install((void*)GetProcAddress(GetModuleHandleA("kernelbase.dll"), "Beep"),
        reinterpret_cast<void*>(hkBeep), reinterpret_cast<void*>(radiance::hook::DispatcherEntry))) {
        assert("Unable to set hook!");
        return -1;
    }

    Beep(13, 37);
    return 0;
}