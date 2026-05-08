# DFT Backward Production Attempt 01

## Scope

Implemented the Apple/non-IPP production path for:

- `bakuage::RealDft<float>::Backward`

Files changed:

- `deps/bakuage/src/dft.cpp`
- `src/tools/dft_golden_capture.cpp`
- `docs/dft_backward_production_attempt01.md`

Files intentionally not changed:

- `deps/bakuage/src/vector_math.cpp`
- `CMakeLists.txt`
- existing DSP implementation outside `RealDft<float>::Backward`
- `local_mastering_app`

No commit was made for this attempt.

Initial status:

```text
## mac-arm64-minimal-cli...origin/mac-arm64-minimal-cli
```

## Production Implementation

The Apple/non-IPP branch now adds a cached vDSP inverse implementation for `RealDft<float>::Backward`.

Even lengths:

- Uses `vDSP_DFT_zrop_CreateSetup(..., N, vDSP_DFT_INVERSE)`.
- Keeps transform length exactly `N`.
- Does not use zero padding.
- Maps the public IPP-compatible CCS/interleaved spectrum to zrop split input.
- Maps even Nyquist real from `src[2 * (N / 2)]` to `in_imag[0]`.
- Ignores DC imaginary and keeps Backward unnormalized.
- Writes exactly `N` real output scalars.

Odd lengths:

- Uses exact-length complex inverse DFT.
- Tries `vDSP_DFT_zop_CreateSetup(..., N, vDSP_DFT_INVERSE)`.
- Falls back to `vDSP_DFT_CreateSetup` plus `vDSP_DFT_zop(..., vDSP_DFT_INVERSE)`.
- Builds the exact conjugate-symmetric complex spectrum.
- Preserves the final odd bin real and imaginary scalars.
- Does not use zero padding.
- Writes exactly `N` real output scalars.

Scaling:

- No `1 / N` normalization is applied.
- No extra `2x`, `0.5x`, or other global Backward scaling is applied.
- The API contract remains `Backward(Forward(x)) ~= N * x`.

Implementation note:

- Backward setup is acquired lazily inside `Backward()` instead of during `RealDft<float>` construction. This keeps Forward-only use from constructing inverse vDSP setups while still letting `work_size()` reserve enough scratch memory for both Forward and Backward.

## Capture Tool Update

`src/tools/dft_golden_capture.cpp` had an Apple/non-IPP guard from the previous Forward-only state:

```text
--case-set minimal_backward requires RealDft<float>::Backward; Apple/non-IPP production build currently has Forward only
```

That guard was removed so `--case-set minimal_backward` can call the new production `RealDft<float>::Backward` path.

No comparison logic was changed.

## Build

Dedicated build directory:

```text
build_mac_arm64_dft_backward_prod_attempt01
```

Configure:

```sh
cmake -S . -B build_mac_arm64_dft_backward_prod_attempt01 \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DDISABLE_TARGET_BENCH=ON \
  -DDISABLE_TARGET_TEST=ON \
  -DBAKUAGE_USE_IPP=OFF \
  -DENABLE_DFT_GOLDEN_CAPTURE=ON \
  -DENABLE_DFT_GOLDEN_COMPARE=ON
```

Result:

- Configure succeeded.

Build:

```sh
cmake --build build_mac_arm64_dft_backward_prod_attempt01 \
  --target dft_golden_capture dft_compare_golden
```

Result:

- `dft_golden_capture` succeeded.
- `dft_compare_golden` succeeded.
- Link emitted existing arm64 warnings about ignoring `/usr/local/lib` x86_64 dylibs, but the requested targets linked successfully.

## Backward Minimal Comparison

IPP golden root:

```text
golden/dft_ipp_backward_float_minimal_attempt01/dft-golden-capture-minimal_backward-459edd2988af6dff041ee1665edf9750feb0b723/dft_golden_minimal_backward
```

Candidate generation:

```sh
build_mac_arm64_dft_backward_prod_attempt01/bin/dft_golden_capture \
  --output-dir build_mac_arm64_dft_backward_prod_attempt01/prod_backward_candidate_minimal \
  --case-set minimal_backward \
  --format json-binary
```

Result:

```text
wrote 52 DFT golden capture records to build_mac_arm64_dft_backward_prod_attempt01/prod_backward_candidate_minimal
```

Comparison:

```sh
build_mac_arm64_dft_backward_prod_attempt01/bin/dft_compare_golden \
  --golden-dir golden/dft_ipp_backward_float_minimal_attempt01/dft-golden-capture-minimal_backward-459edd2988af6dff041ee1665edf9750feb0b723/dft_golden_minimal_backward \
  --candidate-dir build_mac_arm64_dft_backward_prod_attempt01/prod_backward_candidate_minimal \
  --output-report build_mac_arm64_dft_backward_prod_attempt01/prod_backward_compare_report.json
```

Result:

```text
compared 52 records, failures=0
```

Report summary:

- Record count: `52`
- Passed: `52`
- Failed: `0`
- NaN/Inf records: `0`
- Non-pass records: `0`
- Max abs error: `0.2890625`
- RMS error: `0.01673618987`
- Worst record: `float_n12345_forward_output_from_time_cases_backward_oop_internal_work`
- Worst record RMS: `0.04273528096`

No missing output, scalar count mismatch, NaN/Inf, sign mismatch, or broad scaling mismatch was observed.

## Forward Regression

Forward minimal regression:

```text
compared 99 records, failures=0
```

Report summary:

- Record count: `99`
- Passed: `99`
- Failed: `0`
- Max abs error: `0.013671875`
- RMS error: `7.98397704e-05`

Forward extended regression:

```text
compared 392 records, failures=0
```

Report summary:

- Record count: `392`
- Passed: `392`
- Failed: `0`
- Max abs error: `24117248`
- RMS error: `55404.42544`

These match the previously accepted Forward production comparison profile.

## Notes

An initial eager Backward setup design was rejected during local validation because it made Forward-only runs construct inverse vDSP setups. The final implementation keeps Backward setup lazy and reserves scratch size without creating the inverse setup.

`ForwardPerm`, `BackwardPerm`, Pack paths, `RealDft<double>`, `vector_math.cpp`, and application code remain unmodified.

## Conclusion

`RealDft<float>::Backward` Apple/non-IPP production path now matches the current `minimal_backward` IPP golden set for all `52` records.

Backward remains unnormalized and preserves the prototype's exact-length behavior:

- even length: zrop inverse path
- odd length: exact-length complex inverse path with legacy fallback

Forward regression remains green for both the minimal `99` record set and the `forward_extended` `392` record set.
