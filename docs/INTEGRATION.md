# Integrating Simucube API

This document tells you how to add Simucube API (sc-api) to an application.

## Build and install the SDK

```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=<install-dir>
cmake --build build --config Release
cmake --install build --config Release
```

Above steps compile the library with the default settings and release configuration.
Rerun steps with "Release" replaced with "Debug" config to install also debug libraries
to the install-dir.

## Install tree

```
<install-dir>/
    include/                        add this directory to the include path
        sc-api/
            api.h
            session.h
            ...
    lib/
        sc-api.lib                  release build, link this
        sc-apid.lib                 debug build, link this
        pkgconfig/sc-api.pc
        cmake/sc-api/               package config for CMake
    bin/
        sc-api.dll                  shared builds only
    share/sc-api/
        INTEGRATION.md
        GettingStarted.md
        Python.md
        README.md
        LICENSE.txt
```

MinGW names the files `libsc-api.a` and `libsc-apid.a`.

All headers are below one directory: `<install-dir>/include`. All object code
is in one library: `sc-api`. Cryptography code (libeddsa) and the network code
(Asio) are already inside that library.

## Requirements for the application

- Windows. There is no support for other operating systems.
- C++17 or newer.
- The same compiler and the same C runtime as the sc-api build (see [Compatibility](#compatibility)).

## Integration without CMake

Give these three items to the build system:

| Item              | Value                              |
| ----------------- | ---------------------------------- |
| Include directory | `<install-dir>/include`            |
| Library directory | `<install-dir>/lib`                |
| Library           | `sc-api` (`sc-apid` for debug)     |
| Extra system libs | `ws2_32` (MinGW and Clang non-MSVC)|

With MSVC the ws2_32 should be linked automatically.

No preprocessor definitions are necessary. The installed
`include/sc-api/export.h` records if the library was built as a static or as a
shared library.

## Integration with CMake

### FetchContent

FetchContent downloads and builds sc-api as part of your CMake project.
This is the easiest way to handle dependency as the same compiler and build flags are automatically used and there can't be mismatch.
Python interpreter is required to generate telemetry and sim data definitions.

```cmake
include(FetchContent)
FetchContent_Declare(
    sc_api
    GIT_REPOSITORY https://github.com/Simucube/sc-api.git
)

FetchContent_MakeAvailable(sc_api)

target_link_libraries(my_app PRIVATE sc-api)
```

### find_package

This requires that sc-api is compiled and installed separately to a directory where CMake can find it.

```cmake
find_package(sc-api REQUIRED)
target_link_libraries(my_app PRIVATE sc-api::sc-api)
```

Set `CMAKE_PREFIX_PATH` to `<install-dir>` or `sc-api_DIR` to
`<install-dir>/lib/cmake/sc-api`.

## Shared library builds

Configure with `-DSC_API_SHARED=ON` to get `bin/sc-api.dll` and the import
library `lib/sc-api.lib`. Copy the DLL next to the application executable.

A DLL does not make the library compatible with other compilers. The public
interface uses standard library types (`std::string`, `std::vector`,
`std::shared_ptr`), so the same limits as for the static library apply. Use the
DLL only if your application must load the library at run time.

## Compatibility

The public interface uses C++ classes and standard library types. Therefore the
application and sc-api must be built with:

- the same compiler family (MSVC or MinGW, not a mix),
- the same C runtime setting (`/MD`, `/MDd`, `/MT` or `/MTd` with MSVC),
- the same build configuration (do not link the release library into a debug
  application, or the opposite).

If these do not agree, the link can fail or the program can stop unexpectedly at
run time.

## First program

```cpp
#include <sc-api/api.h>
#include <sc-api/session.h>

#include <chrono>
#include <iostream>
#include <thread>

int main() {
    // The constructor starts a background thread that opens and keeps up the
    // session to Simucube Tuner.
    sc_api::Api api;

    for (int i = 0; i < 20; ++i) {
        std::shared_ptr<sc_api::Session> session = api.getSession();
        if (session) {
            std::cout << "session state: " << static_cast<int>(session->getState()) << std::endl;
        } else {
            std::cout << "no session" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return 0;
}
```

The `examples/` directory of the source tree has larger programs.
