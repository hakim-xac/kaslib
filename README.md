<!--
SPDX-FileCopyrightText: 2026 KAS
SPDX-License-Identifier: MPL-2.0
-->

# kaslib

[![License: MPL 2.0](https://img.shields.io/badge/License-MPL_2.0-brightgreen.svg)](https://opensource.org/licenses/MPL-2.0)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-%E2%89%A53.20-blue.svg)](https://cmake.org/)
[![REUSE status](https://api.reuse.software/badge/github.com/hakim-xac/kaslib)](https://api.reuse.software/info/github.com/hakim-xac/kaslib)

**kaslib** — a modular C++20 library providing reusable components for
networking, serialization, stream processing, threading, and common utilities.

The library is built with CMake, ships as a shared or static library, and
exposes an installable CMake package (`KASLIB::KAS`). Every tracked file
carries SPDX copyright and license information; the repository is
[REUSE](https://reuse.software/)-compliant.

## Features

- **C++20** — concepts, `<span>`, `<expected>`-style utilities, precompiled
  headers.
- **Modular** — enable only what you need via CMake options.
- **Installable package** — `find_package(KAS)` + `KASLIB::KAS` target.
- **Shared or static** — `BUILD_SHARED_LIBS` (default `ON`).
- **Optimized Release builds** — `-O3` and LTO for static builds.
- **Header-only logger** — `stream/logger/v1` needs no extra linkage.
- **Per-module tests** — six CTest targets, optional sanitizers
  (`address,undefined`, `thread`, `undefined`).
- **REUSE 3.3 compliant** — SPDX headers in every file, machine-verifiable.

## Requirements

- A C++20-capable compiler (GCC 11+, Clang 14+).
- CMake **3.20** or newer.
- (Optional) `ccache` — picked up automatically if found.
- (Optional) `pkg-config` + `bluez` — enables Bluetooth support in `net`
  when available.
- (Optional) `setarch` (Linux) — required only for ThreadSanitizer builds on
  Linux 6.x kernels.

## Modules

| Group | What's inside |
|-------|---------------|
| `auth_services` | Authentication service abstractions, supported-user handling. |
| `concepts` | C++20 concepts used across the library. |
| `convert` | Base64 (v1), CRC (CCITT-FALSE / CRC16), JWT (v2), MD5 (v3), SHA-1 (v1), SHA-256 (v2), and a serialization framework (`Serializer` / `Deserializer` / `RawPacket`). |
| `expected` | `expected`-type utilities (including a `v3` implementation). |
| `functional` | `FuncGuard`, `LogTime`, `MoveOnlyFunction`, `SafeData`, `TypeGuard`. |
| `net` | HTTP server, routing, range requests, WebSocket, sockets (client/server, scoped, addresses), static files, general networking utilities. |
| `serial` | Serial port access, `FileHandleScoped`, helpers. |
| `stream` | Argument handling (`ArgsHandler` v1, `InputArgs` v1), console styling (`cots` v3), logging (`logger` v1, header-only). |
| `thread` | `Thread`, `JThread`, `ThreadPool`, `AsyncThread`. |
| `utils` | General-purpose utilities (including a `v5` implementation). |

## Repository layout

```
kaslib/
├── CMakeLists.txt
├── LICENSE
├── LICENSES/
│   └── MPL-2.0.txt
├── REUSE.toml
├── README.md
├── RELEASE_NOTES.md
├── version.txt
├── cmake/
│   └── KASConfig.cmake.in
├── include/
│   └── KAS/            # public headers (installed to <prefix>/include/KAS)
├── source/
│   └── KAS/            # implementation units
└── tests/              # test suite
```

The public headers are installed under `<prefix>/include/KAS/...` and are
meant to be included as `#include <KAS/...>`. For example:

```cpp
#include <KAS/thread/Thread.h>
#include <KAS/net/http/HttpServer.h>
#include <KAS/stream/logger/logger.h>
```

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The shared library is produced at `build/libKAS.so` (or `libKAS.a` for a
static build).

## Install

```bash
sudo cmake --install build
```

By default the package is installed under `/usr/local` (per
`GNUInstallDirs`). To install elsewhere, pass `--prefix`:

```bash
sudo cmake --install build --prefix /opt/kas
```

The installation lays out:

```
<prefix>/include/KAS/...                 # public headers
<prefix>/lib/libKAS.so*                  # shared library (or libKAS.a)
<prefix>/lib/cmake/KAS/KASConfig.cmake
<prefix>/lib/cmake/KAS/KASConfigVersion.cmake
<prefix>/lib/cmake/KAS/KAS_targets.cmake
<prefix>/lib/cmake/KAS/KAS_targets-release.cmake
```

## Usage in downstream projects

```cmake
find_package(KAS REQUIRED)
target_link_libraries(my_app PRIVATE KASLIB::KAS)
```

If the package is installed to a non-default prefix, point CMake at it:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/kas
```

### `compile_commands.json` for `clangd` / `clang-tidy`

```cmake
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```

Add it **before** `project()`, or pass it on the command line:

```bash
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ln -sf build/compile_commands.json compile_commands.json
```

## Test

Tests are built as separate executables per module and registered with CTest.

```bash
cmake -S . -B build-tests -DENABLE_TESTS=ON
cmake --build build-tests -j
ctest --test-dir build-tests --output-on-failure
```

Run a single module:

```bash
ctest --test-dir build-tests -R test_net --output-on-failure
```

Available test executables (subject to enabled CMake options):

| Executable | Modules covered |
|------------|-----------------|
| `test_always` | `expected`, `utils`, `functional` |
| `test_convert` | `convert` |
| `test_thread` | `thread` |
| `test_net` | `net` |
| `test_serial` | `serial` |
| `test_stream` | `stream` (args, cots, logger) |

### Sanitizers

Tests can be built with an optional sanitizer via `KASLIB_TEST_SANITIZER`:

| Value | Flags | Notes |
|-------|-------|-------|
| `OFF` (default) | — | Fastest; no runtime overhead. |
| `address` | `-fsanitize=address,undefined` | Memory errors + UB. Recommended for local runs. |
| `thread` | `-fsanitize=thread` | Data races. Requires `setarch -R` on Linux 6.x. |
| `undefined` | `-fsanitize=undefined` | UB only. |

```bash
# AddressSanitizer + UndefinedBehaviorSanitizer
cmake -S . -B build-tests-asan -DENABLE_TESTS=ON \
    -DKASLIB_TEST_SANITIZER=address
cmake --build build-tests-asan -j
ctest --test-dir build-tests-asan --output-on-failure

# ThreadSanitizer
cmake -S . -B build-tests-tsan -DENABLE_TESTS=ON \
    -DKASLIB_TEST_SANITIZER=thread
cmake --build build-tests-tsan -j
ctest --test-dir build-tests-tsan --output-on-failure
```

> **Note:** `address` and `thread` are mutually exclusive. When
> `KASLIB_TEST_SANITIZER=thread`, CTest wraps each test in `setarch -R` to
> disable ASLR, which ThreadSanitizer requires on Linux 6.x kernels.
> `address` and `undefined` do not need `setarch`.

## CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_SHARED_LIBS` | `ON` | Build a shared library instead of a static one. |
| `ENABLE_AUTH_SERVICES` | `ON` | Include the `auth_services` headers. |
| `ENABLE_CONVERTED` | `ON` | Build encoding, hashing, and serialization components. |
| `ENABLE_NET` | `ON` | Build networking components (HTTP, WebSocket, sockets). |
| `ENABLE_SERIAL` | `ON` | Build serial port components. |
| `ENABLE_STREAM` | `ON` | Build stream processing components. |
| `ENABLE_THREAD` | `ON` | Build threading components. |
| `ENABLE_TESTS` | `OFF` | Build the test suite. |
| `KASLIBCXX_STANDART` | `20261018` | Standard identifier propagated to consumers via `PUBLIC` compile definitions. |
| `KASLIB_TEST_SANITIZER` | `OFF` | Sanitizer for tests: `OFF`, `address`, `thread`, `undefined`. |

> **Note:** module `ENABLE_*` options are `PRIVATE` compile definitions — they
> affect how the library itself is built, and are **not** propagated to
> downstream projects. Only `KASLIBCXX_STANDART` is exported via
> `INTERFACE_COMPILE_DEFINITIONS`.

Example — build only networking and threading, statically, without tests:

```bash
cmake -S . -B build \
  -DBUILD_SHARED_LIBS=OFF \
  -DENABLE_TESTS=OFF \
  -DENABLE_CONVERTED=OFF \
  -DENABLE_SERIAL=OFF \
  -DENABLE_STREAM=OFF \
  -DENABLE_AUTH_SERVICES=OFF
cmake --build build -j
```

## Uninstall

`cmake --install` does not provide an uninstall target. To remove the
installed files, delete the prefix paths directly:

```bash
sudo rm -rf /usr/local/include/KAS \
            /usr/local/lib/cmake/KAS \
            /usr/local/lib/libKAS.so*
sudo ldconfig
```

## Versioning

The project version is read from `version.txt` at configure time and
propagated through `CMakePackageConfigHelpers`, so installed packages expose
a consistent `KASConfigVersion.cmake`. Compatibility is
`SameMajorVersion` — a consumer asking for version `X.Y` will accept the
package if the installed major version matches.

## License

This project is licensed under the **Mozilla Public License 2.0 (MPL-2.0)**.

- Full text: [LICENSE](LICENSE) and [LICENSES/MPL-2.0.txt](LICENSES/MPL-2.0.txt)
- SPDX identifier: `MPL-2.0`
- Canonical text: <https://mozilla.org/MPL/2.0/>

The project is [REUSE](https://reuse.software/)-compliant: every tracked file
carries `SPDX-FileCopyrightText` and `SPDX-License-Identifier` information.
Verify with:

```bash
reuse lint
```

## Contributing

1. Fork the repository and create a topic branch.
2. Add SPDX headers to any new file — `reuse annotate` does this automatically.
3. Build and run the tests:
   ```bash
   cmake -S . -B build-tests -DENABLE_TESTS=ON
   cmake --build build-tests -j
   ctest --test-dir build-tests --output-on-failure
   ```
4. Make sure `reuse lint` passes before opening a pull request.
5. Describe your change in the pull request and link related issues.

## Acknowledgements

Developed and maintained by **KAS** (HAKIM666).