# Radiance

**Radiance** is a modern function hooking library for Windows x64, written in C++20 with native module support (C++ Modules).

## 🎯 Features

- **Modern C++20** — modules, concepts, ranges, std::span
- **Splicing hooks** — interception via function prologue modification (inline patching)
- **Atomic patching** — safe 16-byte writes using SSE/CMPXCHG16B
- **Recursion protection** — thread-local counter prevents infinite recursion in hooks
- **Intelligent rebuilder** — instruction reconstruction with RIP-relative addressing support
- **Custom allocator** — executable memory allocation with free block coalescing
- **Stack argument support** — proper argument copying when calling detour

## 📋 Requirements

- **Compiler**: Clang 17+ with C++ Modules support
- **Platform**: Windows x64
- **CMake**: 4.1+
- **Standard**: C++20

## 🚀 Quick Start

```cpp
#include <windows.h>
import radiance;

void __attribute__((noinline)) original_func(int a, int b) {
    MessageBoxA(nullptr, "Original", "Test", MB_OK);
}

void hooked_func(int a, int b) {
    MessageBoxA(nullptr, "Hooked!", "Test", MB_OK);
    original_func(a, b); // Call original
}

int main() {
    radiance::C_Radiance radiance;
    
    auto hook = radiance.create(
        reinterpret_cast<void*>(original_func),
        reinterpret_cast<void*>(hooked_func)
    );
    
    if (hook) {
        original_func(1, 2); // Will call hooked_func
    }
    
    return 0;
}
```

## 🏗️ Architecture

### Modules

```
radiance
├── radiance.ixx                    # Main module (C_Radiance)
├── hook/
│   ├── hook_base.ixx               # Base class C_BaseHook
│   ├── hook_dispatcher.ixx         # ASM dispatcher (DispatcherEntry)
│   └── impl/
│       ├── splicing_hook.ixx       # Main hook class C_SplicingHook
│       ├── splicing_trampoline.ixx # Trampoline code generation
│       ├── splicing_rebuilder.ixx  # Instruction reconstruction
│       └── splicing_patcher.ixx    # Atomic memory patching
├── memory/
│   ├── memory_allocator.ixx        # Executable memory allocator
│   ├── memory_page.ixx             # VirtualAlloc wrapper
│   ├── memory_metadata.ixx         # Block metadata (cookie)
│   └── memory_stl_adapter.ixx      # STL-compatible allocator
└── cpu/
    └── cpu_affinity_scope.ixx      # RAII CPU affinity control
```

### Execution Flow Diagram

```
                    ┌──────────────────────┐
                    │  Call original_func  │
                    └──────────┬───────────┘
                               │
                    ┌──────────▼───────────┐
                    │   JMP [trampoline]   │ ← Patch at function start
                    │   (12-byte stub)     │
                    └──────────┬───────────┘
                               │
                    ┌──────────▼───────────┐
                    │   Trampoline Stub    │
                    │  - push rcx          │
                    │  - mov r10, hook_ptr │ ← Pass hook pointer
                    │  - call dispatcher   │
                    └──────────┬───────────┘
                               │
                    ┌──────────▼───────────┐
                    │   DispatcherEntry    │
                    │  - Check recursion   │ ← thread_local counter
                    │  - Copy stack args   │ ← 128 bytes (16 qwords)
                    │  - Call detour       │
                    │  - Leave context     │
                    └──────────┬───────────┘
                               │
                    ┌──────────▼───────────┐
                    │    detour_func()     │ ← Your handler
                    │  (may call original) │
                    └──────────────────────┘
```

## 📦 Components

### C_Radiance (radiance.ixx)
Main library facade. Manages hook creation and allocator lifetime.

```cpp
class C_Radiance {
    std::shared_ptr<hook::impl::splicing::C_SplicingHook<allocator_t>> 
    create(void* target, void* detour);
};
```

### C_SplicingHook (splicing_hook.ixx)
Implements splicing hook: interception via patching the first instructions of a function.

**Installation algorithm:**
1. Disassemble prologue (HDE64) to determine instruction boundaries
2. Save original bytes (up to 16 bytes)
3. Create trampoline with reconstructed prologue
4. Atomic write of `MOV RAX, addr; JMP RAX` (12 bytes)

**Stub structure:**
```cpp
#pragma pack(push, 1)
struct hook_stub_s {
    uint8_t mov_rax[2]  = { 0x48, 0xB8 };  // MOV RAX, imm64
    uint64_t rax_ptr;                       // Trampoline address
    uint8_t jmp_rax[2]  = { 0xFF, 0xE0 };  // JMP RAX
}; // = 12 bytes
#pragma pack(pop)
```

### C_SplicingTrampoline (splicing_trampoline.ixx)
Generates trampoline code that:
1. Saves context and passes hook pointer via R10
2. Calls dispatcher
3. Executes reconstructed original function prologue
4. Jumps back to original (after patch)

### RebuildInstructions (splicing_rebuilder.ixx)
Intelligent instruction reconstruction with address correction:

| Instruction Type | Handling |
|------------------|----------|
| RIP-relative data (MOV, LEA) | Recalculate disp32 |
| CALL rel32 (E8) | Convert to CALL [RIP+2]; addr64 |
| JMP rel8/rel32 (EB/E9) | Convert to JMP [RIP+0]; addr64 |
| Jcc (conditional jumps) | Invert condition + JMP abs |
| LOOP/JECXZ | Error if jump is external |

### ApplyPatch (splicing_patcher.ixx)
Atomic memory patching:

```cpp
// For aligned address:
__sync_swap(reinterpret_cast<__int128*>(target), newValue);

// For unaligned:
VirtualLock(target, size);
__asm__ volatile ("movups %1, %0" : "=m" (*target) : "x" (newValue));
VirtualUnlock(target, size);
```

Uses `C_CpuAffinityScope` to pin to a single CPU during patching (avoiding race conditions between cores).

### DispatcherEntry (hook_dispatcher.ixx)
Written in inline ASM (Intel syntax). Functions:
- Recursion protection via `thread_local RECURSION_DEPTH`
- Copy 128 bytes of stack arguments
- Save/restore GP registers
- Call detour and return result

### C_MemoryAllocator (memory_allocator.ixx)
Executable memory allocator:
- 64KB pages with `PAGE_EXECUTE_READWRITE`
- First-fit search strategy
- Coalescing of adjacent free blocks
- Magic cookie (`0xDEADBEEF`) for double-free protection

## 🔧 Building

```bash
mkdir build && cd build
cmake -G Ninja -DCMAKE_CXX_COMPILER=clang++ ..
cmake --build .
```

### Project Integration

```cmake
add_subdirectory(external/radiance)
target_link_libraries(your_target PRIVATE radiance)
```

## ⚠️ Limitations

1. **x64 only** — 32-bit architecture is not supported
2. **Minimum 12 bytes** — function must have prologue ≥12 bytes
3. **No hot-patching** — no `int 3` / `nop` padding support
4. **LOOP/JECXZ** — instructions with external jumps are not supported
5. **Clang only** — requires Clang for inline ASM with Intel syntax

## 📚 Dependencies

- **HDE64** — Hacker Disassembler Engine 64 by Vyacheslav Patkov

## 📄 License

See [LICENSE](LICENSE) file.

## 🔗 References

- [Intel x86/x64 Manual](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
- [HDE64 Disassembler](https://github.com/vovkos/hde)
- [C++ Modules](https://en.cppreference.com/w/cpp/language/modules)

