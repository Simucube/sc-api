# Integrating Simucube API

This document tells you how to add Simucube API (sc-api) to an application.
The install tree is made for build systems that are not CMake, but CMake
consumers get a package config file too.

## Build and install the SDK

```
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=<install-dir>
cmake --build build --config Release
cmake --install build --config Release
```

With a Visual Studio generator, replace `Release` with the configuration that
your application uses. To install the debug and the release build into the same
directory, run the three commands two times, once for each configuration.

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
        LICENSE.txt
```

MinGW names the files `libsc-api.a` and `libsc-apid.a`.

All headers are below one directory: `<install-dir>/include`. All object code
is in one library: `sc-api`. Cryptography code (libeddsa) and the network code
(Asio) are already inside that library.

## Requirements for the application

- Windows. There is no support for other operating systems.
- C++17 or newer.
- The same compiler and the same C runtime as the sc-api build (see
  [Compatibility](#compatibility)).

## Integration without CMake

### MSVC (cl.exe)

```
cl /std:c++17 /EHsc /MD /I<install-dir>\include my_app.cpp /link /LIBPATH:<install-dir>\lib sc-api.lib
```

`ws2_32.lib` and `mswsock.lib` are linked automatically, because sc-api holds
`#pragma comment(lib, ...)` directives for them.

For a debug build, use `/MDd` and link `sc-apid.lib`.

### MinGW (g++)

```
g++ -std=c++17 -I<install-dir>/include my_app.cpp -L<install-dir>/lib -lsc-api -lws2_32 -o my_app.exe
```

MinGW has no automatic library linking, thus you must add `-lws2_32`.

### pkg-config

```
g++ -std=c++17 $(pkg-config --cflags sc-api) my_app.cpp $(pkg-config --libs --static sc-api) -o my_app.exe
```

Set `PKG_CONFIG_PATH` to `<install-dir>/lib/pkgconfig` first. The `.pc` file
refers to the release library. For a debug build, link `sc-apid` directly.

### Other build systems

Give these three items to the build system:

| Item              | Value                              |
| ----------------- | ---------------------------------- |
| Include directory | `<install-dir>/include`            |
| Library directory | `<install-dir>/lib`                |
| Library           | `sc-api` (`sc-apid` for debug)     |
| Extra system libs | `ws2_32` (MinGW and Clang non-MSVC)|

No preprocessor definitions are necessary. The installed
`include/sc-api/export.h` records if the library was built as a static or as a
shared library.

## Integration with CMake

```cmake
find_package(sc-api REQUIRED)
target_link_libraries(my_app PRIVATE sc-api::sc-api)
```

Set `CMAKE_PREFIX_PATH` to `<install-dir>` or `sc-api_DIR` to
`<install-dir>/lib/cmake/sc-api`.

To build sc-api as part of your own build instead, use `add_subdirectory()`.
Set `SC_API_INSTALL`, `SC_API_EXAMPLES` and `SC_API_GENERATE_DOCS` to `OFF` if
you do not want those targets.

## Shared library builds

Configure with `-DSC_API_SHARED=ON` to get `bin/sc-api.dll` and the import
library `lib/sc-api.lib`. Copy the DLL next to the application executable.

A DLL does not make the library compatible with other compilers. The public
interface uses standard library types (`std::string`, `std::vector`,
`std::shared_ptr`), so the same limits as for the static library apply. Use the
DLL only if your application must load the library at run time or if you want
to update sc-api without a rebuild of the application.

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
