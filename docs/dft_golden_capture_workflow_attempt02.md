# DFT Golden Capture Workflow Attempt 02

## Scope

- Applied minimal Linux x64 / IPP ON build compatibility fixes from the first GitHub Actions run.
- Did not modify `dft.cpp`.
- Did not modify `vector_math.cpp`.
- Did not change existing DSP behavior.
- Did not run local build or tests.
- Did not commit.

## Fixes

### Linux C++ Standard

Updated the non-Apple CMake branch from:

```cmake
-std=c++11
```

to:

```cmake
-std=c++14
```

Reason:

- Boost.Math 1.90 requires C++14.
- GitHub Actions Linux x64 build reached `biquad_iir_filter.cpp` and failed under C++11.

### file_utils.cpp range-loop warning

Updated:

```cpp
for (const auto temporary: temporaries_)
```

to:

```cpp
for (const auto &temporary: temporaries_)
```

Reason:

- Linux build uses `-Werror`.
- GCC reported `-Werror=range-loop-construct` because the loop copied each `std::string`.

### loudness_ebu_r128.h NULL declaration

Added:

```cpp
#include <cstddef>
```

Reason:

- `NULL` is used in the header default argument.
- GCC reported that `NULL` was not declared and noted `<cstddef>`.

## Changed Files

- `CMakeLists.txt`
- `deps/bakuage/src/file_utils.cpp`
- `deps/bakuage/include/bakuage/loudness_ebu_r128.h`

## Next Check

Rerun the manual GitHub Actions workflow:

- `.github/workflows/dft_golden_capture.yml`

Expected next result:

- Build should pass these three previous blockers.
- If it still fails, inspect the next first compiler/linker error only.
