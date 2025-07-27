# C++17 Application with libunifex

A C++17 application demonstrating the use of libunifex for asynchronous programming, built with the Meson build system.

## Features

- **C++17 Standard**: Uses modern C++17 features including filesystem library and coroutines (via compiler extensions)
- **Meson Build System**: Fast and user-friendly build system
- **libunifex Integration**: Facebook's experimental C++ library for async programming
- **Git Submodule**: libunifex is included as a git submodule for easy dependency management

## Prerequisites

- C++17 compatible compiler with coroutine support (GCC 11+, Clang 12+, MSVC 2019.6+)
- Meson build system
- Ninja (recommended backend for Meson)
- Git

### Installing Prerequisites

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential meson ninja-build git
```

**macOS:**
```bash
brew install meson ninja git
```

**Windows:**
```bash
# Using vcpkg or install manually
```

## Building the Project

### 1. Clone the repository with submodules

```bash
git clone --recursive <your-repo-url>
cd cpp20
```

If you've already cloned without `--recursive`, initialize submodules:
```bash
git submodule update --init --recursive
```

### 2. Configure the build

```bash
meson setup builddir
```

### 3. Build the application

```bash
meson compile -C builddir
```

### 4. Run the application

```bash
./builddir/src/cpp20_app
```

## Project Structure

```
cpp20/
├── meson.build              # Root build configuration
├── src/
│   ├── meson.build          # Source build configuration
│   └── main.cpp             # Main application
├── include/                 # Header files (if any)
├── subprojects/
│   ├── libunifex.wrap       # Meson wrap file for libunifex
│   └── libunifex/           # Git submodule (after init)
└── README.md                # This file
```

## Development

### Adding New Source Files

1. Add your `.cpp` files to the `src/` directory
2. Update `src/meson.build` to include new source files in the `sources` array
3. Rebuild with `meson compile -C builddir`

### Debugging

To build with debug information:
```bash
meson setup builddir --buildtype=debug
meson compile -C builddir
```

### Clean Build

```bash
rm -rf builddir
meson setup builddir
meson compile -C builddir
```

## libunifex Usage Examples

The application demonstrates libunifex usage including:
- **Sender/Receiver algorithms**: `unifex::just()`, `unifex::then()`, `unifex::let_value()`
- **Synchronous waiting**: `unifex::sync_wait()`
- **Thread pool execution**: `unifex::static_thread_pool` with custom schedulers
- **Scheduler concepts**: `unifex::inline_scheduler` and pool schedulers

### Note on Coroutines

This example uses C++17 without coroutines for maximum compatibility. While [libunifex](https://github.com/facebookexperimental/libunifex) supports C++17, coroutines (`unifex::task<T>`, `co_await`, etc.) require C++20 in modern compilers.

**To use coroutines with libunifex:**
1. Change `cpp_std=c++17` to `cpp_std=c++20` in `meson.build`
2. Update the project name from `cpp17_app` to `cpp20_app`
3. Include coroutine headers like `#include <unifex/task.hpp>`
4. Use `unifex::task<T>` functions with `co_await` and `co_return`

## License

[Specify your license here]