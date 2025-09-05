# Integer - High-Performance C++ Arbitrary-Precision Integer Library

- [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
- [![C++](https://img.shields.io/badge/C%2B%2B-14%2B-blue.svg)](https://en.cppreference.com/)
- [![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey.svg)]()
- [![Compiler](https://img.shields.io/badge/Compiler-GCC-green.svg)](https://gcc.gnu.org/), [![Compiler](https://img.shields.io/badge/Compiler-Clang-green.svg)](https://clang.llvm.org/)
- [![CI](../../actions/workflows/ci.yml/badge.svg)](../../actions/workflows/ci.yml)

> **An extremely efficient decimal arbitrary-precision integer library** – Header-only C++ library that has set multiple performance records on [Library Checker](https://judge.yosupo.jp/)

A modern, high-performance C++ arbitrary-precision arithmetic library built for extreme speed. It supports complete operations on unsigned and signed big integers and continually refreshes performance benchmarks on authoritative platforms.

## Table of Contents

- [Integer - High-Performance C++ Arbitrary-Precision Integer Library](#integer---high-performance-c-arbitrary-precision-integer-library)
  - [Table of Contents](#table-of-contents)
- [Features](#features)
- [Quick Start](#quick-start)
- [Usage](#usage)
- [Examples](#examples)
- [Thread Safety](#thread-safety)
- [Testing](#testing)
- [Performance Showcase](#performance-showcase)
- [Full Functionality](#full-functionality)
  - [`UnsignedInteger`](#unsignedinteger)
  - [`SignedInteger`](#signedinteger)
- [Project Maintenance](#project-maintenance)
  - [License](#license)
  - [Contribution Guide](#contribution-guide)
    - [Reporting Issues](#reporting-issues)
    - [Submitting Code](#submitting-code)
    - [Coding Style](#coding-style)
  - [Version History](#version-history)
    - [V2 (2025-09-04)](#v2-2025-09-04)
    - [V1 (2025-08-24)](#v1-2025-08-24)
  - [Feedback](#feedback)
    - [Bug Report](#bug-report)
    - [Feature Request](#feature-request)
    - [Performance Issues](#performance-issues)
  - [Community](#community)
  - [FAQ](#faq)

# Features

- **Easy to Use**: Being header-only, you can start by simply including the header file.
- **Excellent Performance**: As of 2025-09-02, this template ranks first among all decimal big-integer libraries on [Library Checker](https://judge.yosupo.jp/).
- **Versatile Features**: Provides nearly all commonly needed type conversions and I/O (stream-compatible) plus the full set of basic arithmetic operations (add, subtract, multiply, divide, modulo, compare).
- **Dynamic Length Support**: Unlike some high-performance libraries, efficiency here does not rely on static buffer sizes.

# Quick Start

Detailed examples can be found in [basic usage examples](./examples/basic_usage.cpp) and [advanced usage examples](./examples/advanced_demo.cpp).

```cpp
#include "Integer.h"

int main() {
    // Create big integers
    UnsignedInteger a = "123456789012345678901234567890"_UI;
    UnsignedInteger b = "987654321098765432109876543210"_UI;
    
    // Basic operations
    std::cout << "a + b = " << a + b << std::endl;
    std::cout << "a * b = " << a * b << std::endl;
    
    // Signed operations
    SignedInteger x = -"123456789"_SI;
    SignedInteger y = "987654321"_SI;
    std::cout << "x + y = " << x + y << std::endl;
    
    return 0;
}
```

# Usage

This template library is a Header-Only library, and its usage is relatively simple, following these steps:

1. Download or copy the content of `Integer.h` to your local machine.
2. Check your compiler configuration:
  1. It is recommended to enable `-march=native` in compilation parameters to automatically use the SIMD instruction set available on the current platform (AVX2 on x86_64, NEON on AArch64) for optimal performance; however, this is not a prerequisite, and the library will automatically select an available implementation (NEON or scalar) when not enabled. At the same time, ensure that the C++ version is C++14 or later.
  2. Use GCC compiler.
3. In the header file `#include "Integer.h"` where you need this library.

# Examples

Enable and build examples (`examples/basic_usage.cpp`, `examples/advanced_demo.cpp`):

```bash
cmake -S . -B build -DBUILD_EXAMPLES=ON
cmake --build build -j
```

Run examples:

- macOS/Linux: `./build/examples/basic_usage`, `./build/examples/advanced_demo`
- Windows: `build\\examples\\basic_usage.exe`, `build\\examples\\advanced_demo.exe`

Optional: Try to enable SIMD optimization (automatically select AVX2/NEON) on supported compilers:

```bash
cmake -S . -B build -DBUILD_EXAMPLES=ON -DENABLE_EXAMPLE_SIMD=ON
cmake --build build -j
```

Install examples (optional):

```bash
cmake -S . -B build -DBUILD_EXAMPLES=ON -DINSTALL_EXAMPLES=ON
cmake --build build -j
cmake --install build
```

# Thread Safety

This library is designed with "performance first" as its goal, and locks are not introduced by default for object operations. Thread safety strategies are as follows:

- Concurrent writes to the same `UnsignedInteger`/`SignedInteger` instance: Not thread-safe. If multiple threads need to modify the same `UnsignedInteger`/`SignedInteger` instance, please use mutexes (such as `std::mutex`/`std::shared_mutex`) for synchronization at the call layer, or modify each thread independently and then merge serially/lock.
- Concurrent read-only operations on the same object: Generally safe (typical for comparison, conversion, or reading values) without concurrent writes. Please avoid "interleaving reads and writes".
- Parallel calculations on different objects: It is safe and recommended for each thread to independently hold and operate on its own object.
- Thread-local buffers (TLS): The library internally uses thread-local storage in several paths to reduce allocations and sharing (e.g., string conversion buffers, transformation workspaces, etc.). This means different threads do not interfere with each other, but there are two important constraints:
  - The pointer returned by `operator const char*()` points to a thread-local buffer, its content will be overwritten in "the next conversion on the same thread", and it may be reallocated (original pointer invalid) in the same thread. Do not hold or save this pointer across threads; if you need to use it across threads or for a long time, please convert it to `std::string` before passing it.
  - The transformation/workspace is also isolated by thread, only solving "temporary buffer contention between threads", not equivalent to "concurrent write safety for the same object".

In short:

- To parallelize, please let each thread operate on its own big integer object, or add locks yourself at the shared write point.
- To pass text across threads, please pass `std::string`, not `const char*` pointers.

See [thread safety example](examples/thread_safety.cpp).

# Testing

This project uses CTest to manage tests, tests are driven by `tests/run_tests.py`, and automatically performs deterministic and random case verification for SIMD and Fallback implementations, and enables AddressSanitizer and UBSan during compilation.

Running steps:

```bash
# Configure to enable tests
cmake -S . -B build -DBUILD_TESTS=ON

# Build (if needed)
cmake --build build -j

# Run tests
cd build && ctest --output-on-failure
```

In addition, GitHub Actions continuous integration is enabled: it automatically builds and runs all tests on x86_64 and arm64 platforms (Ubuntu and macOS) and the status is shown above the CI badge.

Note:

- Tests use `CMAKE_CXX_COMPILER` to compile `tests/integer_cli.cpp` by default. On Windows platforms, it is recommended to use Clang or GCC compatible toolchains; if MSVC is used and there are compilation parameter compatibility issues, you can change to Clang, or run on a Unix-like environment (such as WSL).
- You can also run the script directly: `python3 tests/run_tests.py` (without CTest).

# Performance Showcase

This template is extremely efficient. To date:

- Addition: Input, addition, output time for two high-precision integers of length $2\cdot10^6$ is only $29\text{ ms}$, which is the optimal solution on [Library Checker](https://judge.yosupo.jp/submission/309899).
- Multiplication: Input, multiplication, output time for two high-precision integers of length $2\cdot10^6$ is only $37\text{ ms}$, which is the optimal solution on [Library Checker](https://judge.yosupo.jp/submission/309889).
- Division: Input, division, modulus, output time for two high-precision integers of length $2\cdot10^6$ is only $143\text{ ms}$, which is the optimal solution on [Library Checker](https://judge.yosupo.jp/submission/309781).

This template's efficiency is quite excellent, which truly deserves the title "extremely efficient". Especially the efficiency of division and modulus is extremely impressive, about 34% faster than the second place!

# Full Functionality

The content in `namespace detail` are auxiliary classes, unless you know what you are doing, do not touch them. Their functions are not described.

This template mainly implements two classes: `UnsignedInteger` and `SignedInteger`. Below is their function table, first some conventions:

- $x$ is the value represented by `*this`.
- $y$ is the value represented by `other`.
- $v$ is the value represented by `value`.
- $n$ is the length of `*this`, i.e., $\lceil\lg|x|\rceil$.
- $m$ is the length of `other`, i.e., $\lceil\lg|y|\rceil$.
- $L$ is the upper limit of Fast Fourier Transform length, here it is $4194304$.
- $T$ is the algorithm switching threshold, here it is $64$.

Validity checks are only executed when the macro `ENABLE_VALIDITY_CHECK` is defined.

## `UnsignedInteger`

| Function Signature | Function Overview | Validity Check | Time Complexity | Remarks |
|:-:|:-:|:-:|:-:|:-:|
| `UnsignedInteger()` | $x\leftarrow0$ | None | $O(1)$ | Default constructor |
| `UnsignedInteger(const UnsignedInteger& other)` | $x\leftarrow y$ | None | $O(m)$ | Copy constructor |
| `UnsignedInteger(UnsignedInteger&& other) noexcept` | $x\leftarrow y,y\leftarrow0$ | None | $O(1)$ | Move constructor |
| `UnsignedInteger(const SignedInteger& other)` | $x\leftarrow y$ | $y\ge0$ | $O(m)$ | None |
| `UnsignedInteger(unsignedIntegral value)` | $x\leftarrow v$ | None | $O(\log v)$ | Enabled for all unsigned integers |
| `UnsignedInteger(signedIntegral value)` | $x\leftarrow v$ | $v\ge0$ | $O(\log v)$ | Enabled for all signed integers |
| `UnsignedInteger(floatingPoint value)` | $x\leftarrow\lfloor v\rfloor$ | $v\ge0$ | $O(\log v)$ | Enabled for all floating-point numbers |
| `UnsignedInteger(const char* value)` | $x\leftarrow v$ | $v$ is not `nullptr`, $v$ is not empty, $v$ is a number string | $O(\lg v)$ | None |
| `UnsignedInteger(const std::string& value)` | $x\leftarrow v$ | $v$ is not empty, $v$ is a number string | $O(\lg v)$ | None |
| `~UnsignedInteger() noexcept` | Deallocate memory | None | $O(1)$ | Destructor |
| `UnsignedInteger& operator=(const UnsignedInteger& other)` | $x\leftarrow y$ | None | $O(m)$ | Copy assignment operator |
| `UnsignedInteger& operator=(UnsignedInteger&& other) noexcept` | $x\leftarrow y,y\leftarrow0$ | None | $O(1)$ | Move assignment operator |
| `UnsignedInteger& operator=(const SignedInteger& other)` | $x\leftarrow y$ | $y\ge0$ | $O(m)$ | None |
| `UnsignedInteger& operator=(unsignedIntegral value)` | $x\leftarrow v$ | None | $O(\log v)$ | Enabled for all unsigned integers |
| `UnsignedInteger& operator=(signedIntegral value)` | $x\leftarrow v$ | $v\ge0$ | $O(\log v)$ | Enabled for all signed integers |
| `UnsignedInteger& operator=(floatingPoint value)` | $x\leftarrow\lfloor v\rfloor$ | $v\ge0$ | $O(\log v)$ | Enabled for all floating-point numbers |
| `UnsignedInteger& operator=(const char* value)` | $x\leftarrow v$ | $v$ is not `nullptr`, $v$ is not empty, $v$ is a number string | $O(\lg v)$ | None |
| `UnsignedInteger& operator=(const std::string& value)` | $x\leftarrow v$ | $v$ is not empty, $v$ is a number string | $O(\lg v)$ | None |
| `friend std::istream& operator>>(std::istream& stream, UnsignedInteger& destination)` | Read from `stream` into `destination` | $v$ is not empty, $v$ is a number string | $O(\lg v)$ | Stream read operator |
| `friend std::ostream& operator<<(std::ostream& stream, const UnsignedInteger& source)` | Output `source` to `stream` | None | $O(n)$ | Stream output operator |
| `operator unsignedIntegral() const` | Return the `unsignedIntegral` form of $x$ | None | $O(n)$ | Type conversion operator, enabled for all unsigned integers |
| `operator signedIntegral() const` | Return the `signedIntegral` form of $x$ | None | $O(n)$ | Type conversion operator, enabled for all signed integers |
| `operator floatingPoint() const` | Return the `floatingPoint` form of $x$ | None | $O(n)$ | Type conversion operator, enabled for all floating-point numbers |
| `operator const char*() const` | Return the `const char*` form of $x$ | None | $O(n)$ | Type conversion operator |
| `operator std::string() const` | Return the `std::string` form of $x$ | None | $O(n)$ | Type conversion operator |
| `operator bool() const noexcept` | Determine if $x$ is non-zero | None | $O(1)$ | Type conversion operator |
| `std::strong_ordering operator<=>(const UnsignedInteger& other) const` | Determine the size relationship between $x$ and $y$ | None | $O(n)$ | Three-way comparison operator, only enabled in C++20 or later |
| `bool operator==(const UnsignedInteger& other) const` | Determine if $x=y$ | None | $O(n)$ | Comparison operator |
| `bool operator!=(const UnsignedInteger& other) const` | Determine if $x\ne y$ | None | $O(n)$ | Comparison operator |
| `bool operator<(const UnsignedInteger& other) const` | Determine if $x<y$ | None | $O(n)$ | Comparison operator |
| `bool operator>(const UnsignedInteger& other) const` | Determine if $x>y$ | None | $O(n)$ | Comparison operator |
| `bool operator<=(const UnsignedInteger& other) const` | Determine if $x\le y$ | None | $O(n)$ | Comparison operator |
| `bool operator>=(const UnsignedInteger& other) const` | Determine if $x\ge y$ | None | $O(n)$ | Comparison operator |
| `UnsignedInteger& operator+=(const UnsignedInteger& other)` | $x\leftarrow x+y$ | None | $O(\max(n,m))$ | Addition assignment operator |
| `UnsignedInteger operator+(const UnsignedInteger& other) const` | Return $x+y$ | None | $O(\max(n,m))$ | Addition operator |
| `UnsignedInteger& operator++()` | $x\leftarrow x+1$ | None | $O(n)$ | Pre-increment operator |
| `UnsignedInteger operator++(int)` | $x\leftarrow x+1$ | None | $O(n)$ | Post-increment operator |
| `UnsignedInteger& operator-=(const UnsignedInteger& other)` | $x\leftarrow x-y$ | $x\ge y$ | $O(\max(n,m))$ | Subtraction assignment operator |
| `UnsignedInteger operator-(const UnsignedInteger& other) const` | Return $x-y$ | $x\ge y$ | $O(\max(n,m))$ | Subtraction operator |
| `UnsignedInteger& operator--()` | $x\leftarrow x-1$ | $x\ne0$ | $O(n)$ | Pre-decrement operator |
| `UnsignedInteger operator--(int)` | $x\leftarrow x-1$ | $x\ne0$ | $O(n)$ | Post-decrement operator |
| `UnsignedInteger& operator*=(const UnsignedInteger& other)` | $x\leftarrow x\cdot y$ | $n\le L\land m\le L$ | $O(nm),O((n+m)\log(n+m))$ | Multiplication assignment operator, uses brute force algorithm when $n\le T\lor m\le T$ |
| `UnsignedInteger operator*(const UnsignedInteger& other) const` | Return $x\cdot y$ | $n\le L\land m\le L$ | $O(nm),O((n+m)\log(n+m))$ | Multiplication operator, uses brute force algorithm when $n\le T\lor m\le T$ |
| `UnsignedInteger& operator/=(const UnsignedInteger& other)` | $x\leftarrow\lfloor\frac xy\rfloor$ | $y\ne0\land n\le L\land m\le L$ | $O(nm),O((n+m)\log(n+m))$ | Division assignment operator, uses brute force algorithm when $n\le T\lor m\le T$ |
| `UnsignedInteger operator/(const UnsignedInteger& other) const` | Return $\lfloor\frac xy\rfloor$ | $y\ne0\land n\le L\land m\le L$ | $O(nm),O((n+m)\log(n+m))$ | Division operator, uses brute force algorithm when $n\le T\lor m\le T$ |
| `UnsignedInteger& operator%=(const UnsignedInteger& other)` | $x\leftarrow x\bmod y$ | $y\ne0\land n\le L\land m\le L$ | $O(nm),O((n+m)\log(n+m))$ | Modulus assignment operator, uses brute force algorithm when $n\le T\lor m\le T$ |
| `UnsignedInteger operator%(const UnsignedInteger& other) const` | Return $x\bmod y$ | $y\ne0\land n\le L\land m\le L$ | $O(nm),O((n+m)\log(n+m))$ | Modulus operator, uses brute force algorithm when $n\le T\lor m\le T$ |
| `UnsignedInteger operator""_UI(const char* literal, std::size_t)` | Return the `UnsignedInteger` form of `literal` | `literal` is a non-empty number string | $O(n)$ | String literal |

## `SignedInteger`

| Function Signature | Function Overview | Validity Check | Time Complexity | Remarks |
|:-:|:-:|:-:|:-:|:-:|
| `SignedInteger()` | $x\leftarrow0$ | None | $O(1)$ | Default constructor |
| `SignedInteger(const SignedInteger& other)` | $x\leftarrow y$ | None | $O(m)$ | Copy constructor |
| `SignedInteger(SignedInteger&& other) noexcept` | $x\leftarrow y,y\leftarrow0$ | None | $O(1)$ | Move constructor |
| `SignedInteger(const UnsignedInteger& other)` | $x\leftarrow y$ | None | $O(m)$ | None |
| `SignedInteger(unsignedIntegral value)` | $x\leftarrow v$ | None | $O(\log v)$ | Enabled for all unsigned integers |
| `SignedInteger(signedIntegral value)` | $x\leftarrow v$ | None | $O(\log v)$ | Enabled for all signed integers |
| `SignedInteger(floatingPoint value)` | $x\leftarrow\lfloor v\rfloor$ | None | $O(\log v)$ | Enabled for all floating-point numbers |
| `SignedInteger(const char* value)` | $x\leftarrow v$ | $v$ is not `nullptr`, $v$ is not empty, $v$ is a number string | $O(\lg v)$ | None |
| `SignedInteger(const std::string& value)` | $x\leftarrow v$ | $v$ is not empty, $v$ is a number string | $O(\lg v)$ | None |
| `~SignedInteger()` | Deallocate memory | None | $O(1)$ | Destructor |
| `SignedInteger& operator=(const SignedInteger& other)` | $x\leftarrow y$ | None | $O(m)$ | Copy assignment operator |
| `SignedInteger& operator=(SignedInteger&& other) noexcept` | $x\leftarrow y,y\leftarrow0$ | None | $O(1)$ | Move assignment operator |
| `SignedInteger& operator=(const UnsignedInteger& other)` | $x\leftarrow y$ | None | $O(m)$ | None |
| `SignedInteger& operator=(unsignedIntegral value)` | $x\leftarrow v$ | None | $O(\log v)$ | Enabled for all unsigned integers |
| `SignedInteger& operator=(signedIntegral value)` | $x\leftarrow v$ | None | $O(\log v)$ | Enabled for all signed integers |
| `SignedInteger& operator=(floatingPoint value)` | $x\leftarrow\lfloor v\rfloor$ | None | $O(\log v)$ | Enabled for all floating-point numbers |
| `SignedInteger& operator=(const char* value)` | $x\leftarrow v$ | $v$ is not `nullptr`, $v$ is not empty, $v$ is a number string | $O(\lg v)$ | None |
| `SignedInteger& operator=(const std::string& value)` | $x\leftarrow v$ | $v$ is not empty, $v$ is a number string | $O(\lg v)$ | None |
| `friend std::istream& operator>>(std::istream& stream, SignedInteger& destination)` | Read from `stream` into `destination` | $v$ is not empty, $v$ is a number string | $O(\lg v)$ | Stream read operator |
| `friend std::ostream& operator<<(std::ostream& stream, const SignedInteger& source)` | Output `source` to `stream` | None | $O(n)$ | Stream output operator |
| `operator unsignedIntegral() const` | Return the `unsignedIntegral` form of $x$ | $x\ge0$ | $O(n)$ | Type conversion operator, enabled for all unsigned integers |
| `operator signedIntegral() const` | Return the `signedIntegral` form of $x$ | None | $O(n)$ | Type conversion operator, enabled for all signed integers |
| `operator floatingPoint() const` | Return the `floatingPoint` form of $x$ | None | $O(n)$ | Type conversion operator, enabled for all floating-point numbers |
| `operator const char*() const` | Return the `const char*` form of $x$ | None | $O(n)$ | Type conversion operator |
| `operator std::string() const` | Return the `std::string` form of $x$ | None | $O(n)$ | Type conversion operator |
| `operator bool() const noexcept` | Determine if $x$ is non-zero | None | $O(1)$ | Type conversion operator |
| `std::strong_ordering operator<=>(const SignedInteger& other) const` | Determine the size relationship between $x$ and $y$ | None | $O(n)$ | Three-way comparison operator, only enabled in C++20 or later |
| `bool operator==(const SignedInteger& other) const` | Determine if $x=y$ | None | $O(n)$ | Comparison operator |
| `bool operator!=(const SignedInteger& other) const` | Determine if $x\ne y$ | None | $O(n)$ | Comparison operator |
| `bool operator<(const SignedInteger& other) const` | Determine if $x<y$ | None | $O(n)$ | Comparison operator |
| `bool operator>(const SignedInteger& other) const` | Determine if $x>y$ | None | $O(n)$ | Comparison operator |
| `bool operator<=(const SignedInteger& other) const` | Determine if $x\le y$ | None | $O(n)$ | Comparison operator |
| `bool operator>=(const SignedInteger& other) const` | Determine if $x\ge y$ | None | $O(n)$ | Comparison operator |
| `SignedInteger& operator+=(const SignedInteger& other)` | $x\leftarrow x+y$ | None | $O(\max(n,m))$ | Addition assignment operator |
| `SignedInteger operator+(const SignedInteger& other) const` | Return $x+y$ | None | $O(\max(n,m))$ | Addition operator |
| `SignedInteger& operator++()` | $x\leftarrow x+1$ | None | $O(n)$ | Pre-increment operator |
| `SignedInteger operator++(int)` | $x\leftarrow x+1$ | None | $O(n)$ | Post-increment operator |
| `SignedInteger& operator-=(const SignedInteger& other)` | $x\leftarrow x-y$ | None | $O(\max(n,m))$ | Subtraction assignment operator |
| `SignedInteger operator-(const SignedInteger& other) const` | Return $x-y$ | None | $O(\max(n,m))$ | Subtraction operator |
| `SignedInteger& operator--()` | $x\leftarrow x-1$ | None | $O(n)$ | Pre-decrement operator |
| `SignedInteger operator--(int)` | $x\leftarrow x-1$ | None | $O(n)$ | Post-decrement operator |
| `SignedInteger& operator*=(const SignedInteger& other)` | $x\leftarrow x\cdot y$ | $n\le L\land m\le L$ | $O(nm),O((n+m)\log(n+m))$ | Multiplication assignment operator, uses brute force algorithm when $n\le T\lor m\le T$ |
| `SignedInteger operator*(const SignedInteger& other) const` | Return $x\cdot y$ | $n\le L\land m\le L$ | $O(nm),O((n+m)\log(n+m))$ | Multiplication operator, uses brute force algorithm when $n\le T\lor m\le T$ |
| `SignedInteger& operator/=(const SignedInteger& other)` | $x\leftarrow\lfloor\frac xy\rfloor$ | $y\ne0\land n\le L\land m\le L$ | $O(nm),O((n+m)\log(n+m))$ | Division assignment operator, uses brute force algorithm when $n\le T\lor m\le T$ |
| `SignedInteger operator/(const SignedInteger& other) const` | Return $\lfloor\frac xy\rfloor$ | $y\ne0\land n\le L\land m\le L$ | $O(nm),O((n+m)\log(n+m))$ | Division operator, uses brute force algorithm when $n\le T\lor m\le T$ |
| `SignedInteger& operator%=(const SignedInteger& other)` | $x\leftarrow x\bmod y$ | $y\ne0\land n\le L\land m\le L$ | $O(nm),O((n+m)\log(n+m))$ | Modulus assignment operator, uses brute force algorithm when $n\le T\lor m\le T$ |
| `SignedInteger operator%(const SignedInteger& other) const` | Return $x\bmod y$ | $y\ne0\land n\le L\land m\le L$ | $O(nm),O((n+m)\log(n+m))$ | Modulus operator, uses brute force algorithm when $n\le T\lor m\le T$ |
| `SignedInteger operator""_SI(const char* literal, std::size_t)` | Return the `SignedInteger` form of `literal` | `literal` is a non-empty number string | $O(n)$ | String literal |

**Special note: The results of modulus and division for `SignedInteger` are consistent with C++.** That is:

- Division results are truncated towards 0.
- The sign of the modulus result is the sign of the left operand.

# Project Maintenance

## License

This project uses the [MIT License](LICENSE).

```
MIT License

Copyright (c) 2024 masonxiong

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
```

## Contribution Guide

Any form of contribution is welcome! If you want to contribute code to this project, please follow these steps:

### Reporting Issues
- Before submitting an issue, please check the existing [Issues](../../issues) for similar issues
- Clearly describe the reproduction steps, expected behavior, and actual behavior
- Provide necessary environment information (operating system, compiler version, etc.)

### Submitting Code
1. Fork this repository to your GitHub account
2. Create a new feature branch (`git checkout -b feature/your-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin feature/your-feature`)
5. Create a Pull Request

### Coding Style
- Follow the existing code style and naming conventions
- Add appropriate test cases for new features
- Update relevant documentation
- Ensure code passes all existing tests

## Version History

### V2 (2025-09-04)

- Added AArch64 NEON support and scalar fallback implementation, runtime automatically selects AVX2 → NEON → Scalar.
- FFT workspace and output buffer changed to thread_local, ensuring thread safety, removing cross-thread sharing.
- Fixed memory leaks in `UnsignedInteger::operator=(UnsignedInteger&&)`, and realloc/delete[] leaks and race conditions in multiplication, division, modulus, and FFT.
- String conversion changed to thread-local cache, safer.
- Fixed several undefined behaviors.
- Build system: Provided Header-Only `Integer::Integer` target, improved installation and export; added `BUILD_EXAMPLES`, `BUILD_TESTS`, `ENABLE_HOST_OPT`, `ENABLE_EXAMPLE_SIMD` options.
- Added Python-driven CLI tests (with SIMD/Scalar forced) and integrated ASan/UBSan; determined and randomized coverage via CTest.
- Introduced GitHub Actions CI, supporting Ubuntu/macOS, x86_64/arm64 Release builds and running tests.
- Refreshed documentation and examples, added thread safety example paragraph.
- Added `.clang-format`, `.clangd` development tool configurations.

### V1 (2025-08-24)

- First release
  - Incoming:
    - Square root, greatest common divisor, least common multiple, factorial, prime number judgment, binary bit operations.
    - Another version of high-precision integer library, with better portability and thread safety, and `constexpr` support, but may lose some performance.

## Feedback

If you encounter any issues or have feature suggestions during use:

### Bug Report

Please create a new issue in [GitHub Issues](../../issues) and include:

- Detailed description of the issue
- Minimal reproducible code
- Compiler and system environment information
- Error output or exception information

### Feature Request

We welcome new feature suggestions! Please describe in the Issues:

- Detailed description of the expected feature (should not be too similar to the latest Incoming part)
- Use cases and reasons
- Possible implementation schemes (if any)

### Performance Issues

If performance issues are found, please provide:

- Specific performance test code
- Comparison results with other libraries
- Run environment and data scale information

## Community

- **GitHub Issues**: [Project Issue Discussion](../../issues)
- **Luogu Homepage**: [masonxiong](https://www.luogu.com.cn/user/446979)
- **Library Checker**: [Performance Record View](https://judge.yosupo.jp/)

## FAQ

- My programming environment is very old, and I see a lot of unfamiliar syntax in your code, can it pass compilation?

This template has relatively loose language requirements. You only need a GCC compiler that supports C++11 to pass compilation. To achieve optimal performance, it is recommended to add `-march=native` to enable available SIMD (AVX2 on x86_64, NEON on AArch64); however, this is not required, and the library will automatically fall back to NEON or scalar implementation if not enabled.

- Why does the compilation error `inlining failed in call to 'always_inline' '__m128d _mm_fmaddsub_pd(__m128d, __m128d, __m128d)': target specific option mismatch` occur?

This is usually due to the corresponding SIMD instruction set not being enabled (e.g., AVX2 not enabled on x86_64). You can add `-march=native` or targeted switches (e.g., `-mavx2`) to achieve higher performance; you can also disable it, the library will automatically use scalar implementation, but performance will decrease.

- Your template doesn't support `divmod`?

Ah, it actually supports it, there is a `divisionAndModulus` function in the `protected` function, which implements the `divmod` function, with time complexity consistent with `operator/`. However, because putting it in the `public` function would make the structure look ugly, it was thrown into the `protected` function (

- Under my debug mode, the program crashes and there is no error message, how to debug?

Note: In order to achieve efficiency, this template does not introduce locks by default for object operations, making it non-thread-safe (see [Thread Safety](#thread-safety)). Please check if there are multiple threads or concurrent writes to the same object. If concurrent factors are excluded, you may likely find a Bug! Please refer to the Bug feedback section for feedback.
