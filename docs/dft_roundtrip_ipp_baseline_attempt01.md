# DFT Roundtrip IPP Baseline Attempt 01

## Scope

Follow-up after adding `dft_roundtrip_validate`.

Goal:

- Build the same `RealDft<float>::Forward` -> `RealDft<float>::Backward` roundtrip validator with `BAKUAGE_USE_IPP=ON`.
- Use it as the IPP-side baseline for the unnormalized API contract.

Files intentionally not changed:

- `deps/bakuage/src/dft.cpp`
- `deps/bakuage/src/vector_math.cpp`
- `ForwardPerm`, `BackwardPerm`, Pack paths, or `RealDft<double>`
- `local_mastering_app`

## Apple/non-IPP Checkpoint

The Apple/non-IPP validator was already built and run successfully:

```text
roundtrip case_set=minimal records=99 passed=99 failed=0 nan_or_inf=0
roundtrip case_set=forward_extended records=392 passed=392 failed=0 nan_or_inf=0
```

This confirms:

```text
Backward(Forward(x)) ~= N*x
```

for the current Apple/non-IPP production `RealDft<float>` pair.

## IPP Baseline Attempt

Configure:

```sh
cmake -S . -B build_mac_arm64_dft_roundtrip_ipp_attempt01 \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DDISABLE_TARGET_BENCH=ON \
  -DDISABLE_TARGET_TEST=ON \
  -DBAKUAGE_USE_IPP=ON \
  -DENABLE_DFT_ROUNDTRIP_VALIDATE=ON
```

Result:

```text
Configure succeeded.
```

Build:

```sh
cmake --build build_mac_arm64_dft_roundtrip_ipp_attempt01 --target dft_roundtrip_validate
```

Result:

```text
Failed while compiling deps/bakuage/src/dft.cpp:
fatal error: 'ipp.h' file not found
```

Environment check:

```text
/opt/intel/ipp/include was not present in this local Mac environment.
```

## Conclusion

The IPP roundtrip baseline could not be run locally because the Intel IPP development headers are unavailable in this environment.

No production DSP code was changed. The next safe options are:

- run the new validator in a Windows/Linux/CI environment where IPP is installed, or
- use the already captured IPP golden artifacts as the direct compatibility baseline and proceed to real `phase_limiter` smoke testing on Apple/non-IPP.
