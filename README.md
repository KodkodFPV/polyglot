# polyglot
> Polyglot is a configurable framework for bidirectional communication. It separates message handling from input sources and transports, allowing applications to receive requests, process commands, and send responses through text or binary protocols.

## Build and check headers

The library is header-only, so building the `polyglot` target alone does not compile its headers. Tests use Catch2, which CMake downloads when `BUILD_TESTING` is enabled. The `polyglot_tests` executable includes the public headers and checks a sample frame at compile time.

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

In an IDE with CMake support, configure this folder and build the `polyglot_tests` target. Compiler errors will appear in the build output. Add uses of new templates to `tests/compile_check.cpp` when you want the compiler to check their instantiation.
