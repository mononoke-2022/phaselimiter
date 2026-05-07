# PHASE_LIMITER_DFT_IPP_ANALYSIS

## Current State

- `git status --short --branch`: `## mac-arm64-minimal-cli`, `M CMakeLists.txt`
- 調査のみ。コード変更、CMake変更、configure/build、commit は未実行。
- 既存の未コミット変更は Attempt 10/11 の `CMakeLists.txt` 変更。

## Public API

Header:

- `deps/bakuage/include/bakuage/dft.h`

Main classes:

- `FftMemoryBuffer`: IPP malloc/free backed work/spec memory wrapper.
- `Dft<float>` / `Dft<double>`: complex-to-complex 1D DFT.
- `Dft2D<float>`: complex-to-complex 2D DFT.
- `Dct2D<float>`: 2D forward DCT.
- `RealDft<float>` / `RealDft<double>`: real-to-complex and complex-to-real 1D DFT.
- `ThreadLocalDftWork` / `ThreadLocalDftPool`: shared per-thread work buffer and cached DFT objects.

## IPP Functions In `dft.cpp`

Memory:

- `ippsMalloc_8u`, `ippsFree`: aligned byte allocation/free for spec and work buffers.

Real 1D DFT, arbitrary length:

- `ippsDFTGetSize_R_32f/64f`: query real DFT spec/init/work sizes.
- `ippsDFTInit_R_32f/64f`: initialize real DFT spec.
- `ippsDFTFwd_RToCCS_32f/64f`: real input to CCS output.
- `ippsDFTFwd_RToPerm_32f/64f`: real input to Perm output.
- `ippsDFTFwd_RToPack_32f/64f`: real input to Pack output.
- `ippsDFTInv_CCSToR_32f/64f`: CCS input to real output.
- `ippsDFTInv_PermToR_32f/64f`: Perm input to real output.
- `ippsDFTInv_PackToR_32f/64f`: Pack input to real output.

Real 1D FFT, power-of-two/in-place paths:

- `ippsFFTGetSize_R_32f/64f`, `ippsFFTInit_R_32f/64f`: initialize real FFT specs.
- `ippsFFTFwd_RToCCS_32f/64f`, `ippsFFTInv_CCSToR_32f/64f`: CCS real FFT pair.
- `ippsFFTFwd_RToPerm_32f/64f`, `ippsFFTInv_PermToR_32f/64f`: Perm real FFT pair.
- `ippsFFTFwd_RToPerm_32f_I/64f_I`, `ippsFFTInv_PermToR_32f_I/64f_I`: in-place Perm pair.
- `ippsFFTFwd_RToPack_32f/64f`, `ippsFFTInv_PackToR_32f/64f`: Pack pair.
- `ippsFFTFwd_RToPack_32f_I/64f_I`, `ippsFFTInv_PackToR_32f_I/64f_I`: in-place Pack pair.

Complex 1D DFT:

- `ippsDFTGetSize_C_32fc/64fc`, `ippsDFTInit_C_32fc/64fc`: complex DFT setup.
- `ippsDFTFwd_CToC_32fc/64fc`, `ippsDFTInv_CToC_32fc/64fc`: interleaved complex forward/inverse.

Complex 2D / DCT:

- `ippiDFTGetSize_C_32fc`, `ippiDFTInit_C_32fc`: 2D complex DFT setup.
- `ippiDFTFwd_CToC_32fc_C1R`, `ippiDFTInv_CToC_32fc_C1R`: 2D interleaved complex forward/inverse.
- `ippiDCTFwdGetSize_32f`, `ippiDCTFwdInit_32f`, `ippiDCTFwd_32f_C1R`: 2D forward DCT.

## Data Format

Scaling:

- All current IPP setups use `IPP_FFT_NODIV_BY_ANY`.
- Forward and inverse are unnormalized.
- Existing callers manually apply normalization where needed. Example: `src/audio_analyzer/test_dft.cpp` divides inverse result by `width`.

`RealDft::Forward` / `Backward`:

- Input: real array length `N`.
- Output expected by callers as interleaved `std::complex<T>` buffer with `N/2 + 1` bins.
- `mfcc.h` documents DFT input as `(real, image, real, image, ...)`.
- `Forward` uses IPP CCS, but callers cast output to `std::complex<T>*`; therefore observed/required public layout is effectively:
  - bin `0`: real DC, imag zero
  - bins `1..N/2-1`: interleaved real/imag
  - even `N` bin `N/2`: real Nyquist, imag zero
- `Backward` consumes the same public layout and returns real `N`, unnormalized.

`RealDft::ForwardPerm` / `BackwardPerm`:

- Used heavily by `FirFilter2` and `GradCore`.
- Out-of-place path uses IPP DFT Perm.
- In-place path uses IPP FFT Perm `_I`.
- Public buffers are treated as raw `T*`, often backed by `std::complex<T>` arrays of size `N/2` for Perm paths.
- `GradCore` special-cases `spec[1] = spec[len]` before in-place `BackwardPerm`, so exact Perm DC/Nyquist placement is critical.

`RealDft::ForwardPack` / `BackwardPack`:

- Present and benchmarked, but main phase_limiter/audio_analyzer paths appear less dependent than `Forward` and `ForwardPerm`.

`Dft<float/double>`:

- Complex-to-complex 1D.
- Input/output are interleaved complex arrays represented as `Float*`.
- Unnormalized forward/inverse.

`Dft2D<float>` / `Dct2D<float>`:

- `Dft2D<float>` uses interleaved complex image data with row stride `2 * sizeof(float) * size0`.
- `Dct2D<float>` uses real image data with row stride `sizeof(float) * size0`.
- `Dft2D<float>` is referenced in `rhythm_spectrogram.h` only under a disabled `#else` path in the currently active code.

Windowing:

- `dft.cpp` does no windowing.
- Windowing is done by callers, e.g. `CopyHanning`, sqrt-Hanning, or precomputed phase_limiter windows.

In-place:

- `RealDft` only uses in-place IPP FFT paths for `ForwardPerm`, `ForwardPack`, `BackwardPerm`, `BackwardPack` when `input == output`.
- `Forward`/`Backward` are out-of-place only in current wrapper.

## vDSP Candidate APIs

Most relevant candidates:

- `vDSP_DFT_zrop_CreateSetup` / `vDSP_DFT_zrop_CreateSetupD` plus `vDSP_DFT_Execute` / `vDSP_DFT_ExecuteD`: split-complex real-to-complex and complex-to-real DFT. Supports non-power-of-two families, but has vDSP split layout and a forward factor `C = 2` documented in SDK headers, so scaling/layout conversion must be verified.
- `vDSP_DFT_Interleaved_CreateSetup` / `vDSP_DFT_Interleaved_CreateSetupD` plus `vDSP_DFT_Interleaved_Execute`: interleaved DFT API, macOS 12+. Promising for `Dft<T>` and maybe real interleaved paths, but real-to-complex length/layout semantics must be proven.
- `vDSP_create_fftsetup` / `vDSP_create_fftsetupD` plus `vDSP_fft_zrip`, `vDSP_fft_zop`, `vDSP_fft2d_*`: older FFT APIs, strong for power-of-two FFTs; split-complex layout requires `vDSP_ctoz` / `vDSP_ztoc` conversions.
- `vDSP_DCT_CreateSetup` / `vDSP_DCT_Execute`: possible `Dct2D` building block, but current IPP path is 2D DCT and vDSP DCT is 1D, so 2-pass row/column design would need validation.

## Verification Required For No Audio Regression

- Confirm exact IPP CCS/Perm/Pack public layout for even and odd `N`.
- Confirm DC/Nyquist placement, especially `RealDft::Forward` complex buffer compatibility and `GradCore`'s `spec[1] = spec[len]` before `BackwardPerm`.
- Confirm scaling: IPP uses no normalization; vDSP real DFT may introduce documented factors depending on API. Forward+inverse must match old `N` gain.
- Confirm sign convention for forward/inverse.
- Confirm arbitrary length support. Existing test uses `width = 12345`; vDSP fast DFT supports only certain factorizations, so fallback strategy may be required for unsupported lengths.
- Confirm in-place behavior for Perm/Pack equivalent paths, or deliberately use safe out-of-place temp buffers with identical output.
- Confirm float and double parity.
- Compare against old IPP outputs for representative lengths: odd, even non-power-of-two, power-of-two, phase_limiter `PL_FFT_MAX_LEN`, audio analyzer widths.
- Validate downstream metrics, not only raw FFT: `GradCore` eval/grad, `FirFilter2` convolution, spectrogram/mel spectrum, `test_dft` roundtrip.

## Difficulty Classification

- `RealDft::Forward` CCS-compatible public output: design confirmation required.
- `RealDft::ForwardPerm` / `BackwardPerm`: dangerous,追加調査が必要. This is phase_limiter-critical and layout-sensitive.
- `Dft<float/double>` complex 1D: relatively straightforward after choosing split vs interleaved vDSP API.
- `Dft2D<float>`: design confirmation required; likely lower priority for minimal CLI.
- `Dct2D<float>`:追加調査が必要 if needed, because IPP 2D DCT does not map 1:1 to a single obvious vDSP 2D DCT call.

## Recommended Next Step

Do not implement yet. First create a tiny comparison/design test plan that records IPP outputs for `RealDft<float>` `Forward`, `ForwardPerm`, `Backward`, and `BackwardPerm` on small even/odd lengths, then map vDSP output into the same public layout. The highest-risk item is Perm layout compatibility, not the math itself.
