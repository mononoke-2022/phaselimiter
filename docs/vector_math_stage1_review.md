# VECTOR_MATH_STAGE1_REVIEW

## Scope

Reviewed only the current diff for:

- `CMakeLists.txt`
- `deps/bakuage/src/vector_math.cpp`
- `src/test/vector_math.cpp`

No code changes, build, or commit were performed during this review.

## Current Diff Summary

- `CMakeLists.txt` adds `BAKUAGE_USE_IPP`, defaulting to `ON`, and exports it as a compile definition.
- `vector_math.cpp` keeps the existing IPP path under `BAKUAGE_USE_IPP=ON` and adds standard C++ fallbacks under `BAKUAGE_USE_IPP=OFF` for the Stage 1 vector operations.
- `src/test/vector_math.cpp` adds small-array tests for set, zero, move, add, multiply, and multiply-by-constant in-place paths.

## Review Findings

### 修正必須

1. Test coverage is not complete enough to treat Stage 1 as locked.

   The implementation covers more overloads than the new tests currently exercise. Missing or incomplete test coverage includes:

   - `VectorMove<double>`
   - overlapping `VectorMove<std::complex<float>>`
   - `VectorAddInplace<float>`
   - `VectorAdd<double>` out-of-place
   - `VectorAdd<std::complex<double>>`
   - `VectorAddInplace<std::complex<double>>`
   - `VectorMulInplace<double>`
   - `VectorMul<double, std::complex<double>>`
   - `VectorMulInplace<double, std::complex<double>>`
   - `VectorMul<std::complex<double>, std::complex<double>>`
   - `VectorMulInplace<std::complex<double>, std::complex<double>>`
   - `VectorMulConstant<float>` and `VectorMulConstant<double>` out-of-place, which were added although not listed in the requested target set
   - scalar-to-complex double variants of `VectorMulConstantInplace`

   The current tests are useful smoke tests, but they do not yet prove output compatibility for all Stage 1 overloads.

2. The test target has not actually been compiled or run in this environment.

   The `vector_math.cpp` object build succeeded with `BAKUAGE_USE_IPP=OFF`, but the full build stops earlier on `dft.cpp`, `fir_filter4.cpp`, and Boost C++14 issues. Therefore the new `src/test/vector_math.cpp` additions have not been validated by compilation or execution yet.

3. `BAKUAGE_USE_IPP=OFF` still leaves non-target vector functions intentionally unavailable.

   Several existing functions are now compiled only under `BAKUAGE_USE_IPP=ON`, including IPP-heavy functions such as convolution, norm, reverse, conversion, decimate/interpolate, and complex real/imag conversion. This is preferable to unsafe stubs, but it means future non-IPP builds may fail at link time if any of those APIs are required by `phase_limiter` or `audio_analyzer`.

### 後で改善

1. `CMakeLists.txt` only controls the compile definition, not IPP library linkage.

   `BAKUAGE_USE_IPP=OFF` prevents `vector_math.cpp` from including `ipp.h`, but the global CMake link lists still contain IPP libraries elsewhere. This is outside the Stage 1 vector fallback scope, but it must be handled before a full non-IPP CLI link can succeed.

2. Fallbacks added outside the requested function set should be documented or separately tested.

   `VectorAddConstantInplace`, `VectorMulConstant`, `VectorReplaceNanInplace`, `VectorEnsureNonnegativeInplace`, and `VectorBothThresholdInplace` now have non-IPP paths. These are mathematically straightforward and not stubs, but they were outside the explicit Stage 1 target list, so they need either explicit acceptance as support functions or tighter scoping.

3. `std::memmove` on `std::complex<float>` is practically aligned with the existing interleaved layout assumption, but the current static assertion checks size only.

   The fallback asserts that complex values are interleaved real/imag by size. For stronger portability, a later pass should consider a compile-time or runtime layout/trivial-copyability check before relying on raw byte movement for complex data.

4. Formatting noise is high in `vector_math.cpp`.

   The semantic change is understandable, but mixed indentation increases review cost. A later formatting-only pass would make future DSP reviews safer. This should be kept separate from DSP logic changes.

### 問題なし

1. `BAKUAGE_USE_IPP=ON` appears to preserve the existing IPP behavior for reviewed target functions.

   The original IPP calls remain active under `#if BAKUAGE_USE_IPP`. The default CMake option is `ON`, so existing IPP builds keep the old path unless explicitly configured otherwise.

2. No empty implementation, zero-return stub, or obviously fake approximation was found in the reviewed fallback paths.

   The fallback implementations use direct arithmetic loops, `std::memmove`, or explicit threshold/NAN handling. Missing non-target functions are omitted under `BAKUAGE_USE_IPP=OFF` rather than replaced with unsafe dummy behavior.

3. Stage 1 arithmetic semantics match the corresponding IPP operation at the operation level.

   Reviewed fallbacks preserve the basic operation meanings:

   - `VectorSet`: assign constant to each element
   - `VectorZero`: assign zero to each element
   - `VectorMove`: byte move with overlap safety
   - `VectorAdd`: `output[i] = x[i] + y[i]`
   - `VectorAddInplace`: `output[i] += x[i]`
   - `VectorMul`: `output[i] = x[i] * y[i]`
   - `VectorMulInplace`: `output[i] *= x[i]`
   - `VectorMulConstantInplace`: `output[i] *= c`

4. In-place ordering is safe for the reviewed simple elementwise operations.

   Each loop reads and writes the same index only. Exact aliasing such as `x == output` remains mathematically valid for add and multiply. Partial-overlap semantics are not guaranteed for add/multiply, but that was not an existing API promise; `VectorMove` is the overlap-sensitive operation and uses `std::memmove` under the fallback.

5. Complex multiplication signs are correct in the fallback.

   Using `std::complex` multiplication preserves `(a + bi)(c + di) = (ac - bd) + (ad + bc)i`. The added tests check non-trivial signs for `std::complex<float>` multiplication and complex constant multiplication.

6. `VectorMove` is overlap-safe in the fallback path.

   The non-IPP path uses `std::memmove` for `float`, `double`, and `std::complex<float>`, which is the correct direction for preserving memmove-like behavior.

## Risk Classification

- 修正必須: Add missing tests for all Stage 1 overloads before committing this as a correctness baseline.
- 修正必須: Compile and run the vector_math tests once the build system allows the test target to be built.
- 修正必須: Keep omitted non-target functions as link-time blockers, not silent stubs, until each is implemented and tested.
- 後で改善: Guard IPP link libraries with `BAKUAGE_USE_IPP` when moving from object build to full non-IPP CLI link.
- 後で改善: Reduce formatting noise in a separate cleanup patch.
- 問題なし: No stub/zero fake implementation was found in the reviewed fallback paths.
- 問題なし: `BAKUAGE_USE_IPP=ON` keeps reviewed IPP calls active by default.

## Conclusion

The Stage 1 fallback implementation is directionally sound for the safe, elementwise vector operations. It does not appear to introduce fake DSP behavior or obvious arithmetic changes in the reviewed target functions.

However, it should not be committed as a final Stage 1 quality gate yet. The next safe step is to add the missing overload tests and later compile/run the test target when the current non-vector build blockers are cleared.
