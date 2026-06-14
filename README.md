# kisha

## Requirements

### System

- A C++23 capable compiler (GCC 13+, Clang 16+, MSVC 19.38+)
- [CMake](https://cmake.org/download/) 3.25 or newer
- [Vulkan SDK](https://vulkan.lunarg.com/) 1.3+ must be installed and discoverable by CMake
- [Git](https://git-scm.com/install/) required by CMake's `FetchContent` to download dependencies

### Dependencies

Dependencies are listed under [ProjectDependencies.cmake](cmake/ProjectDependencies.cmake)


## Building

| CMake Cache Entries | Values        |
|---------------------|---------------|
| KISHA_BUILD_ENGINE  | default TRUE  |
| BUILD_TESTING       | default FALSE |

kisha requires an out-of-source-build. Do not build in source directory, it will be rejected and manual cleanup will be required.

### Linux

To build in kisha root directory: 
```sh
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug -j
```
To also build and run tests:
```sh
cmake -S . -B cmake-build-debug -DBUILD_TESTING=TRUE
cmake --build cmake-build-debug -j
ctest --test-dir cmake-build-debug
```

### Windows

You may need to define a generator when configuring:

```sh
cmake -S . -B cmake-build-debug -G "Visual Studio 17 2022"
```
Otherwise it's the same as building on linux

```sh
cmake --build cmake-build-debug -j
```
## AI Usage Disclosure

JHorace: AI was used to generate engine tests.
