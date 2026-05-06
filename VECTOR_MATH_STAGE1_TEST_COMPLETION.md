# VECTOR_MATH_STAGE1_TEST_COMPLETION

## 目的

`VECTOR_MATH_STAGE1_COMMIT_DECISION.md` に基づき、Stage 1 の未カバー overload テストを補完した。

今回の主眼は `src/test/vector_math.cpp` の補強であり、音質を変える実装変更は行っていない。

## 追加したテスト

追加した coverage は以下。

- `VectorMove<double>`
- overlapping `VectorMove<std::complex<float>>`
- `VectorAddInplace<float>`
- `VectorAdd<double>`
- `VectorAdd<std::complex<double>>`
- `VectorAddInplace<std::complex<double>>`
- `VectorMulInplace<double>`
- `VectorMul<double, std::complex<double>>`
- `VectorMulInplace<double, std::complex<double>>`
- `VectorMul<std::complex<double>, std::complex<double>>`
- `VectorMulInplace<std::complex<double>, std::complex<double>>`
- `VectorMulConstant<float>` out-of-place
- `VectorMulConstant<double>` out-of-place
- scalar-to-complex double の `VectorMulConstantInplace`

また、比較ヘルパーを更新して、浮動小数点は以下で比較するようにした。

- `float`: `EXPECT_FLOAT_EQ`
- `double`: `EXPECT_DOUBLE_EQ`
- `std::complex<float>`: 実部・虚部とも `EXPECT_FLOAT_EQ`
- `std::complex<double>`: 実部・虚部とも `EXPECT_DOUBLE_EQ`

## 実行結果

### 1. test target の生成確認

`src/test/vector_math.cpp` を含む `test` executable は、`DISABLE_TARGET_BENCH=OFF` の build dir で生成されることを確認した。

使用した build dir:

- `build_mac_arm64_attempt09_tests_on`

### 2. `vector_math.cpp` test object compile

`CMakeFiles/test.dir/src/test/vector_math.cpp.o` の compile は成功した。

ログ:

- `build_mac_arm64_attempt09_tests_on/vector_math_test_object_attempt09.log`

意味:

- 追加した test コードの構文は通っている
- 追加した overload 呼び出しは、少なくとも object compile の段階では問題なし

### 3. `bin/test` link attempt

`bin/test` の link は失敗した。

ログ:

- `build_mac_arm64_attempt09_tests_on/test_link_attempt09.log`

失敗理由は今回のテスト追加そのものではなく、既存の non-vector blockers だった。

確認できた blocker:

- `deps/optim` の configure / stamp 処理が、スペースを含むパスの扱いで `sed` エラーを出している
- その後、既知の Boost 1.90 / C++14 非互換エラーが `deps/bakuage/src/biquad_iir_filter.cpp` 側で再発している

## 判断

Stage 1 の未カバー overload テストは補完できた。

ただし、`bin/test` の実行可能状態まではまだ到達していない。  
理由はテスト追加ではなく、既存の `deps/optim` と Boost C++14 系のブロッカーで止まるため。

## 変更ファイル

- `src/test/vector_math.cpp`

## 補足

今回の追加はテスト補完に限定しており、`vector_math.cpp` の fallback 実装や `CMakeLists.txt` の方針は変更していない。
