# VECTOR_MATH_STAGE1_COMMIT_DECISION

## 結論

この Stage 1 の `vector_math.cpp` 非IPP fallback は、**現時点ではコミットしない**判断です。

実装の方向性は妥当で、空実装や stub は見当たりません。  
ただし、`vector_math.cpp` の Stage 1 変更を「品質ゲートとして固定する」には、未カバーの overload と未実行の test target が残っています。

## 判断基準

### 修正必須

1. Stage 1 対象の overload で、テスト未カバーのものが残っている。

   特に以下は、今回のレビューで未確認または不十分です。

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
   - `VectorMulConstant<float>` / `VectorMulConstant<double>` out-of-place
   - scalar-to-complex double の `VectorMulConstantInplace`

2. 新しい `src/test/vector_math.cpp` の test target 自体が、この環境で compile / run されていない。

   `vector_math.cpp` 単体 object build は成功しているが、test target の実行確認はまだない。  
   そのため、Stage 1 の「正しさ固定」は未完了。

3. `BAKUAGE_USE_IPP=OFF` では、今回対象外の vector API が意図的に未定義のまま残っている。

   これは stub より正しい方針だが、将来の non-IPP build でリンク不足になる可能性がある。  
   Stage 1 のコミットとしては、どこまで非IPP化したのかを明確にしたうえで次段に進めるべき。

### コミット前にやるべき

1. Stage 1 対象 overload のテストを埋める。

   最低でも以下を追加したい。

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

2. test target を実際に build / run できる段階まで進める。

   ここで初めて Stage 1 の fallback をコミット候補として扱える。

3. `vector_math.cpp` の後続対象外関数が、今後の non-IPP build でどの時点で必要になるかを整理する。

   まだ link phase の整理は終わっていないため、vector fallback のみでは non-IPP CLI 完成にはならない。

### コミット後でよい

1. `vector_math.cpp` の formatting noise の整理。

2. `BAKUAGE_USE_IPP` に連動した IPP link library の全面整理。

   これは full non-IPP build のために必要だが、Stage 1 の vector fallback 固定とは別の作業。

3. `std::complex<float>` / `std::complex<double>` の raw layout 検証強化。

   現在の `std::memmove` fallback は実用上妥当だが、より厳密な移植性チェックは後段でよい。

## テスト不足の評価

今回のテスト不足は、**現時点ではコミットを止めるべき重大問題ではない**が、**コミット前に補強すべき課題**です。

理由:

1. すでに `vector_math.cpp` の単体 object build は成功しており、fallback 実装そのものは壊れていない可能性が高い。
2. 追加されたテストは、少なくとも Stage 1 の主要な elementwise 演算と `VectorMove` の安全性を確認している。
3. ただし、今回の変更を「安全に固定した」と言い切るには、double / complex<double> 系の未確認 overload が多い。

したがって、テスト不足は「次コミットで補強すべき課題」であり、今すぐの再設計理由ではないが、**このままの状態でコミットするのは早い**。

## 推奨アクション

1. Stage 1 対象の overload テストを補完する。
2. そのうえで test target を build / run する。
3. すべて確認できたら、Stage 1 としてコミットする。

## 最終判断

**この時点ではコミット保留。**

実装方針は妥当だが、Stage 1 を品質固定するにはテスト不足が残っている。
