# DFT Forward Production Attempt 01

## Scope

Implemented a minimal Apple/non-IPP production path for:

- `bakuage::RealDft<float>::Forward`

Out of scope and intentionally not implemented in this attempt:

- `RealDft<float>::Backward`
- `RealDft<float>::ForwardPerm`
- `RealDft<float>::BackwardPerm`
- `RealDft<float>::ForwardPack` / `BackwardPack`
- `RealDft<double>`
- Complex `Dft<T>`, `Dft2D`, and `Dct2D`
- `deps/bakuage/src/vector_math.cpp`
- `local_mastering_app`

No commit was made for this attempt.

## Production Changes

Files changed:

- `deps/bakuage/src/dft.cpp`
- `CMakeLists.txt`

`dft.cpp` now selects a vDSP-only implementation only when both conditions hold:

- `__APPLE__`
- `BAKUAGE_USE_IPP=0`

IPP-enabled builds and non-Apple builds keep the existing IPP implementation.

The Apple/non-IPP implementation defines only the production surface needed by the current capture tool:

- `FftMemoryBuffer`
- `RealDft<float>::RealDft`
- `RealDft<float>::Forward`
- `RealDft<float>::work_size`

`CMakeLists.txt` changes for Apple/non-IPP:

- Removes IPP libraries from `BAKUAGE_LINK_LIBRARIES` when `BAKUAGE_USE_IPP=OFF`.
- Links `bakuage` with `Accelerate.framework`.
- Excludes IPP-only source files from the Apple/non-IPP `bakuage` build:
  - `fir_filter4.cpp`
  - `vector_math.cpp`
  - `window_func.cpp`

## Forward Layout

Even lengths:

- Uses `vDSP_DFT_zrop_CreateSetup(..., N, vDSP_DFT_FORWARD)`.
- Splits real input into vDSP zrop even/odd input arrays.
- Converts vDSP split output into the existing public interleaved complex scalar layout.
- Applies the prototype-verified `0.5f` scale correction.
- Writes DC imaginary as `0`.
- Writes even Nyquist real from `out_imag[0]`.
- Writes even Nyquist imaginary as `0`.

Odd lengths:

- Uses exact-length complex vDSP DFT.
- Real input is copied to the complex real part.
- Complex imaginary input is zero.
- No zero padding is used.
- First tries `vDSP_DFT_zop_CreateSetup` + `vDSP_DFT_Execute`.
- Falls back to legacy `vDSP_DFT_CreateSetup` + `vDSP_DFT_zop`.
- Does not apply zrop `0.5f` scaling.
- Writes bins `0..floor(N/2)` as interleaved real/imag floats.
- Writes DC imaginary as `0`.
- Preserves the final odd bin imaginary scalar.

## Build

Dedicated build directory:

- `build_mac_arm64_dft_forward_prod_attempt01`

Configure:

```sh
cmake -S . -B build_mac_arm64_dft_forward_prod_attempt01 \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DDISABLE_TARGET_BENCH=ON \
  -DDISABLE_TARGET_TEST=ON \
  -DBAKUAGE_USE_IPP=OFF \
  -DENABLE_DFT_GOLDEN_CAPTURE=ON \
  -DENABLE_DFT_GOLDEN_COMPARE=ON
```

Build:

```sh
cmake --build build_mac_arm64_dft_forward_prod_attempt01 \
  --target dft_golden_capture dft_compare_golden
```

Result:

- Configure succeeded.
- `dft_golden_capture` build succeeded.
- `dft_compare_golden` build succeeded.
- Link emitted warnings about ignoring `/usr/local/lib` x86_64 dylibs while building arm64, but the requested targets linked successfully.

## Production Candidate Generation

Command:

```sh
build_mac_arm64_dft_forward_prod_attempt01/bin/dft_golden_capture \
  --output-dir build_mac_arm64_dft_forward_prod_attempt01/prod_forward_candidate \
  --case-set minimal
```

Result:

- Generated records: `99`
- Candidate directory: `build_mac_arm64_dft_forward_prod_attempt01/prod_forward_candidate`
- Manifest tool: `dft_golden_capture`
- Method: `Forward`

Note: the manifest `git_commit` is the current committed HEAD. The candidate was generated from the worktree including the uncommitted production changes in this attempt.

## IPP Golden Comparison

Command:

```sh
build_mac_arm64_dft_forward_prod_attempt01/bin/dft_compare_golden \
  --golden-dir golden/dft_ipp_forward_float_attempt01/dft-ipp-golden-b65b62a30707e3f4ff9fe30c87800f0c28f2ad25/dft_golden \
  --candidate-dir build_mac_arm64_dft_forward_prod_attempt01/prod_forward_candidate \
  --output-report build_mac_arm64_dft_forward_prod_attempt01/prod_forward_compare_report.json
```

Result:

- Compared records: `99`
- Passed records: `99`
- Failed records: `0`
- Missing candidate records: `0`
- Scalar count mismatch records: `0`
- NaN/Inf records: `0`
- Global max abs error: `0.013671875`
- Global RMS error: `7.98397704e-05`

Even/odd split:

| Parity | Count | Passed | Failed | Max abs error | Max RMS error |
| --- | ---: | ---: | ---: | ---: | ---: |
| Even | 55 | 55 | 0 | `6.103515625e-05` | `3.041595506e-06` |
| Odd | 44 | 44 | 0 | `0.013671875` | `0.0001703652743` |

Worst max-abs record:

- `float_n12345_sine_nonbin_forward_oop_internal_work`
- Max abs error: `0.013671875`

Worst RMS record:

- `float_n12345_hand_mixed_forward_oop_internal_work`
- RMS error: `0.0001703652743`

## Assessment

The production `RealDft<float>::Forward` Apple/non-IPP path reproduced the existing IPP golden set for all current minimal cases.

No evidence of these failure modes was observed:

- scalar count mismatch
- missing candidate output
- NaN/Inf
- even scaling mismatch
- odd scaling mismatch
- DC/Nyquist layout mismatch
- odd final bin imaginary loss
- real/imag interleaving mismatch

Remaining limits:

- This validates only `RealDft<float>::Forward`.
- `Backward`, `ForwardPerm`, `BackwardPerm`, Pack paths, and double paths remain unimplemented/unvalidated for Apple/non-IPP.
- Roundtrip behavior is still not validated.
- Length `N=1` is not covered by the current golden set.
- Full application targets were not built in this attempt.
