# Radiance

> Proof of concept: processor-affinity–based hooks for x86-64 on Windows.

Radiance is an experimental C++20 library for implementing runtime code hooks with processor affinity control on x86-64 Windows systems. This is a proof-of-concept project designed to explore advanced techniques for dynamic function interception, CPU-bound hooking, and low-level memory manipulation.

## Features

- **Processor Affinity Control**: Run hook handlers on specific CPU cores using `C_CpuAffinityScope`
- **Code Splicing**: Modify function entry points by inserting x86-64 assembly redirects
- **Trampoline Generation**: Automatically generate and manage jump trampolines for hooked functions
- **Custom Memory Management**: Efficient block-based allocator with metadata support
- **C++20 Modules**: Modern modular architecture using C++20 module system
- **x86-64 Assembly Integration**: Direct manipulation of machine code and CPU registers
- - **Direct API Calls from Hooks**: No need to store original function pointers - call any API directly from hook handlers

## Project Status

⚠️ **Proof of Concept** - This project is in early experimental stage and should not be used in production environments.

- Initial commit: January 3, 2026
- Active development: January 2026
- Stability: Experimental
- API Stability: Subject to change

## Architecture

### Project Structure

```
radiance/
├── src/
│   ├── cpu/              # CPU affinity management
│   │   └── cpu_affinity_scope.ixx      # RAII wrapper for CPU affinity
│   ├── hook/             # Hooking system core
│   │   ├── hook_base.ixx               # Base hook interface
│   │   ├── hook_dispatcher.ixx         # Hook dispatch mechanism
│   │   └── impl/         # Implementation details
│   │       ├── splicing_hook.ixx       # Function splicing implementation
│   │       ├── splicing_trampoline.ixx # Trampoline code generation
│   │       ├── splicing_patcher.ixx    # Code patching utilities
│   │       └── splicing_rebuilder.ixx  # Instruction rebuilding
│   └── memory/           # Memory management
│       ├── memory_allocator.ixx        # Custom block allocator
│       ├── memory_metadata.ixx         # Memory block metadata
│       ├── memory_pageix.ixx           # Page management
│       └── memory_stl_adapter.ixx      # STL allocator interface
├── external/             # External dependencies
├── CMakeLists.txt        # CMake build configuration
├── main.cpp              # Demonstration program
└── README.md             # This file
```

### Core Components

#### CPU Affinity Scope (`src/cpu/cpu_affinity_scope.ixx`)
Manages thread affinity masks using RAII patterns:
- Sets thread execution affinity to specific CPU cores
- Automatically restores original affinity on destruction
- Windows API integration via `SetProcessAffinityMask`

#### Hooking System (`src/hook/*`)
Implements runtime function interception:
- **Base Hook**: Abstract interface for different hooking strategies
- **Hook Dispatcher**: Low-level dispatch mechanism with register save/restore
- **Splicing**: Modifies function prologues by inserting jump instructions
- **Trampoline**: Generates transition code for redirecting execution

#### Memory Management (`src/memory/*`)
Custom memory allocator with metadata:
- Block-based allocation with configurable page size (64 KB default)
- Magic cookie validation for corruption detection
- Free block tracking and coalescence support
- STL allocator adapter interface

## Technical Details

### Hooking Mechanism

Radiance uses **code splicing** to intercept function calls:

1. Identifies function entry point boundaries
2. Allocates trampoline memory for the hooked function
3. Modifies the original function prologue with a jump to the hook handler
4. Preserves original instructions in the trampoline for chaining
5. Uses processor affinity to execute hook handlers on specific cores

### Memory Layout

```
[Original Function]
      ↓
  [JMP Redirect] ← Modified prologue
      ↓
  [Trampoline Code] ← Processor affinity scope
      ↓
  [Hook Handler]
      ↓
  [Continue to original code]
```

### Build Requirements

- **C++ Standard**: C++20 (with module support)
- **OS**: Windows (x86-64)
- **Build Tool**: CMake 4.1+
- **Compiler**: MSVC or Clang with C++20 modules support

## Building

### Prerequisites

```bash
# Windows with Visual Studio 2022 or newer
# Requires C++20 Standard Library modules support
```

### Build Steps

```bash
# Clone the repository
git clone https://github.com/methmind/radiance.git
cd radiance

# Create build directory
mkdir build
cd build

# Configure with CMake
cmake .. -DCMAKE_CXX_STANDARD=20

# Build
cmake --build . --config Release
```

## Usage Example

Basic usage pattern (conceptual - actual API may differ):

```cpp
#include <radiance/cpu/cpu_affinity_scope.hpp>
#include <radiance/hook/hook_dispatcher.hpp>

// Set execution to CPU core 0
C_CpuAffinityScope affinity_scope(0);

// Create hook handler
C_SplicingHook hook(target_function, hook_handler);

// Install the hook
hook.Install();

// ... hook is now active ...

// Hook is automatically removed on destruction
```

## Limitations and Caveats

- ⚠️ **x86-64 Windows Only**: Limited to Windows systems with x86-64 architecture
- ⚠️ **No Binary Compatibility**: API and ABI may change without notice
- ⚠️ **Limited Testing**: Minimal test coverage, use with caution
- ⚠️ **Performance Trade-offs**: Hooking adds execution overhead
- ⚠️ **Debugging Complexity**: Difficult to debug hooked code
- ⚠️ **Security Considerations**: Hooks can be detected/bypassed by security software
- - ⚠️ **Maximum Prologue Bytes**: Maximum 16 bytes of original function prologue can be copied due to implementation constraints

## Known Issues

- External dependencies directory structure incomplete
- Minimal error handling and validation
- No recovery mechanism for failed hook installation
- Documentation limited to code comments

## Future Development

- [ ] Add comprehensive unit tests
- [ ] Implement hook chaining support
- [ ] Add hook logging and diagnostics
- [ ] Support for inline hooks (beyond splicing)
- [ ] Performance profiling and optimization
- [ ] Extended documentation and examples
- [ ] Error handling and recovery
- [ ] Multi-platform support (Linux, macOS)

## Contributing

This is an experimental proof-of-concept project. Contributions, bug reports, and discussions are welcome, but please note:

- This is not production-ready software
- The API is subject to breaking changes
- Code review may be limited due to experimental nature

## License

MIT License - Copyright (c) 2026 

See [LICENSE](LICENSE) file for details.

## Disclaimer

This software is provided for educational and research purposes only. Users are responsible for:

- Understanding the legal implications of code hooking in their jurisdiction
- Ensuring compatibility with antivirus and security software
- Testing thoroughly in isolated environments
- Following applicable software licensing agreements

The author is not liable for any damage or misuse of this software.

## References

- x86-64 Architecture: Intel and AMD processor manuals
- Windows API: Microsoft documentation
- Code Hooking Techniques: Academic papers and security research
- C++20 Modules: ISO/IEC 14882:2020 standard

- MinHook: Modern library for x86-64 function hooking
## Author

**** - [methmind](https://github.com/methmind)

## Acknowledgments

This project explores concepts from various security research publications and reverse engineering communities.
