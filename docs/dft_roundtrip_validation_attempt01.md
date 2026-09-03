# DFT Roundtrip Validation Attempt 01

## Scope

Adds a minimal validation tool for the next checkpoint after Apple/non-IPP `RealDft<float>::Forward` and `RealDft<float>::Backward` production paths.

Files changed:

- `CMakeLists.txt`
- `src/tools/dft_roundtrip_validate.cpp`
- `docs/dft_roundtrip_validation_attempt01.md`

Files intentionally not changed:

- `deps/bakuage/src/dft.cpp`
- `deps/bakuage/src/vector_math.cpp`
- `ForwardPerm`, `BackwardPerm`, Pack paths, or `RealDft<double>`
- `local_mastering_app`

## Validation Contract

The tool runs:

```text
RealDft<float>::Forward(input) -> RealDft<float>::Backward(spectrum)
```

and validates:

```text
raw output ~= N * input
normalized output ~= input
```

This keeps the API contract aligned with the existing IPP behavior: both Forward and Backward are unnormalized, and callers apply normalization when needed.

## Case Sets

`--case-set minimal` uses the same 99 time-domain cases as the current minimal Forward golden capture set.

`--case-set forward_extended` uses the same 392 time-domain cases as the extended Forward validation set, including odd/even lengths, powers of two, non-powers of two, deterministic noise, tiny/subnormal values, and large safe values.

## Build

Configure an Apple/non-IPP validation build with:

```sh
cmake -S . -B build_mac_arm64_dft_roundtrip_attempt01 \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DDISABLE_TARGET_BENCH=ON \
  -DDISABLE_TARGET_TEST=ON \
  -DBAKUAGE_USE_IPP=OFF \
  -DENABLE_DFT_ROUNDTRIP_VALIDATE=ON
```

Build:

```sh
cmake --build build_mac_arm64_dft_roundtrip_attempt01 --target dft_roundtrip_validate
```

Run:

```sh
build_mac_arm64_dft_roundtrip_attempt01/bin/dft_roundtrip_validate --case-set minimal
build_mac_arm64_dft_roundtrip_attempt01/bin/dft_roundtrip_validate --case-set forward_extended
```

## Acceptance

Default tolerances:

- normalized absolute tolerance: `1.0e-3`
- raw relative tolerance: `2.0e-5`

A case passes when the normalized absolute error is within the absolute tolerance, or the raw output is within the case-level relative tolerance. This lets zero/tiny cases use an absolute floor while large-amplitude cases are judged by signal-scale relative error.

## Attempt 01 Result

Build directory:

```text
build_mac_arm64_dft_roundtrip_attempt01
```

Configure:

```text
Succeeded
```

Build:

```text
Succeeded: dft_roundtrip_validate
```

Link emitted the same existing arm64 warnings about ignoring `/usr/local/lib` x86_64 dylibs seen in earlier validation attempts. The requested target linked successfully.

Minimal roundtrip:

```text
roundtrip case_set=minimal records=99 passed=99 failed=0 nan_or_inf=0
worst_normalized_abs n12345/ramp value=6.581611989e-05
worst_relative n12345/ramp value=6.581611989e-05
expected raw API contract: Backward(Forward(x)) ~= N*x
```

Forward extended roundtrip:

```text
roundtrip case_set=forward_extended records=392 passed=392 failed=0 nan_or_inf=0
worst_normalized_abs n4095/large_sine_nonbin_1e10 value=55587.44615
worst_relative n4097/cosine_nonbin_1p5 value=8.879762879e-06
expected raw API contract: Backward(Forward(x)) ~= N*x
```

Conclusion:

- The Apple/non-IPP production `RealDft<float>::Forward` and `RealDft<float>::Backward` paths roundtrip successfully for the existing minimal and forward-extended time-domain case families.
- The observed behavior is consistent with the unnormalized API contract: `Backward(Forward(x)) ~= N*x`.
- No production DSP code was changed for this validation step.
