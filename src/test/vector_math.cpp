#include "gtest/gtest.h"
#include <complex>
#include <limits>
#include <vector>
#include "bakuage/vector_math.h"

namespace {
template <class T>
void ExpectVectorEq(const std::vector<T> &expected, const std::vector<T> &actual) {
    ASSERT_EQ(expected.size(), actual.size());
    for (size_t i = 0; i < expected.size(); i++) {
        EXPECT_EQ(expected[i], actual[i]) << "index " << i;
    }
}

template <>
void ExpectVectorEq<float>(const std::vector<float> &expected, const std::vector<float> &actual) {
    ASSERT_EQ(expected.size(), actual.size());
    for (size_t i = 0; i < expected.size(); i++) {
        EXPECT_FLOAT_EQ(expected[i], actual[i]) << "index " << i;
    }
}

template <>
void ExpectVectorEq<double>(const std::vector<double> &expected, const std::vector<double> &actual) {
    ASSERT_EQ(expected.size(), actual.size());
    for (size_t i = 0; i < expected.size(); i++) {
        EXPECT_DOUBLE_EQ(expected[i], actual[i]) << "index " << i;
    }
}

void ExpectComplexFloatVectorEq(const std::vector<std::complex<float>> &expected,
                                const std::vector<std::complex<float>> &actual) {
    ASSERT_EQ(expected.size(), actual.size());
    for (size_t i = 0; i < expected.size(); i++) {
        EXPECT_FLOAT_EQ(expected[i].real(), actual[i].real()) << "real index " << i;
        EXPECT_FLOAT_EQ(expected[i].imag(), actual[i].imag()) << "imag index " << i;
    }
}

void ExpectComplexDoubleVectorEq(const std::vector<std::complex<double>> &expected,
                                 const std::vector<std::complex<double>> &actual) {
    ASSERT_EQ(expected.size(), actual.size());
    for (size_t i = 0; i < expected.size(); i++) {
        EXPECT_DOUBLE_EQ(expected[i].real(), actual[i].real()) << "real index " << i;
        EXPECT_DOUBLE_EQ(expected[i].imag(), actual[i].imag()) << "imag index " << i;
    }
}
}

TEST(VectorMath, VectorSanitizeInplace) {
    using namespace bakuage;
    typedef float Float;
    const Float threshold = 1;
    
    Float zero = 0;
    VectorSanitizeInplace<Float>(threshold, &zero, 1);
    EXPECT_EQ(0, zero);
    
    Float inf = std::numeric_limits<float>::infinity();
    VectorSanitizeInplace<Float>(threshold, &inf, 1);
    EXPECT_EQ(1, inf);
    
    Float neg_inf = -std::numeric_limits<float>::infinity();
    VectorSanitizeInplace<Float>(threshold, &neg_inf, 1);
    EXPECT_EQ(-1, neg_inf);
    
    Float quiet_nan = std::numeric_limits<Float>::quiet_NaN();
    VectorSanitizeInplace<Float>(threshold, &quiet_nan, 1);
    EXPECT_EQ(0, quiet_nan);
    
    Float sig_nan = std::numeric_limits<Float>::signaling_NaN();
    VectorSanitizeInplace<Float>(threshold, &sig_nan, 1);
    EXPECT_EQ(0, sig_nan);
    
    Float inside = 0.5;
    VectorSanitizeInplace<Float>(threshold, &inside, 1);
    EXPECT_EQ(0.5, inside);
    
    Float neg_inside = -0.5;
    VectorSanitizeInplace<Float>(threshold, &neg_inside, 1);
    EXPECT_EQ(-0.5, neg_inside);
    
    Float outside = 2;
    VectorSanitizeInplace<Float>(threshold, &outside, 1);
    EXPECT_EQ(1, outside);
    
    Float neg_outside = -2;
    VectorSanitizeInplace<Float>(threshold, &neg_outside, 1);
	    EXPECT_EQ(-1, neg_outside);
	}

TEST(VectorMath, VectorSetZeroMove) {
    {
        std::vector<float> x(4);
        bakuage::VectorSet<float>(1.5f, x.data(), x.size());
        ExpectVectorEq<float>({1.5f, 1.5f, 1.5f, 1.5f}, x);
    }
    {
        std::vector<double> x(3);
        bakuage::VectorSet<double>(-2.25, x.data(), x.size());
        ExpectVectorEq<double>({-2.25, -2.25, -2.25}, x);
    }
    {
        std::vector<int> x(5);
        bakuage::VectorSet<int>(7, x.data(), x.size());
        ExpectVectorEq<int>({7, 7, 7, 7, 7}, x);
    }
    {
        std::vector<float> x {1, -2, 3};
        bakuage::VectorZero<float>(x.data(), x.size());
        ExpectVectorEq<float>({0, 0, 0}, x);
    }
    {
        std::vector<std::complex<float>> x {{1, -2}, {3, 4}};
        bakuage::VectorZero<std::complex<float>>(x.data(), x.size());
        ExpectComplexFloatVectorEq({{0, 0}, {0, 0}}, x);
    }
    {
        std::vector<float> x {1, 2, 3, 4, 5};
        bakuage::VectorMove<float>(x.data(), x.data() + 1, 4);
        ExpectVectorEq<float>({1, 1, 2, 3, 4}, x);
    }
    {
        std::vector<float> x {1, 2, 3, 4, 5};
        bakuage::VectorMove<float>(x.data() + 1, x.data(), 4);
        ExpectVectorEq<float>({2, 3, 4, 5, 5}, x);
    }
    {
        std::vector<double> x {1, 2, 3, 4, 5};
        bakuage::VectorMove<double>(x.data(), x.data() + 1, 4);
        ExpectVectorEq<double>({1, 1, 2, 3, 4}, x);
    }
    {
        std::vector<std::complex<float>> x {{1, 2}, {3, 4}, {5, 6}};
        std::vector<std::complex<float>> y(3);
        bakuage::VectorMove<std::complex<float>>(x.data(), y.data(), x.size());
        ExpectComplexFloatVectorEq(x, y);
    }
    {
        std::vector<std::complex<float>> x {{1, 2}, {3, 4}, {5, 6}, {7, 8}};
        bakuage::VectorMove<std::complex<float>>(x.data(), x.data() + 1, 3);
        ExpectComplexFloatVectorEq({{1, 2}, {1, 2}, {3, 4}, {5, 6}}, x);
    }
}

TEST(VectorMath, VectorAddAndInplace) {
    {
        std::vector<float> x {1, -2, 3};
        std::vector<float> y {4, 5, -6};
        std::vector<float> out(3);
        bakuage::VectorAdd<float>(x.data(), y.data(), out.data(), out.size());
        ExpectVectorEq<float>({5, 3, -3}, out);
    }
    {
        std::vector<double> x {1, -2, 3};
        std::vector<double> y {4, 5, -6};
        std::vector<double> out(3);
        bakuage::VectorAdd<double>(x.data(), y.data(), out.data(), out.size());
        ExpectVectorEq<double>({5, 3, -3}, out);
        bakuage::VectorAddInplace<double>(x.data(), y.data(), y.size());
        ExpectVectorEq<double>({5, 3, -3}, y);
    }
    {
        std::vector<float> x {1, -2, 3};
        std::vector<float> y {4, 5, -6};
        bakuage::VectorAddInplace<float>(x.data(), y.data(), y.size());
        ExpectVectorEq<float>({5, 3, -3}, y);
    }
    {
        std::vector<std::complex<float>> x {{1, 2}, {-3, 4}};
        std::vector<std::complex<float>> y {{5, -6}, {7, 8}};
        std::vector<std::complex<float>> out(2);
        bakuage::VectorAdd<std::complex<float>>(x.data(), y.data(), out.data(), out.size());
        ExpectComplexFloatVectorEq({{6, -4}, {4, 12}}, out);
        bakuage::VectorAddInplace<std::complex<float>>(x.data(), y.data(), y.size());
        ExpectComplexFloatVectorEq({{6, -4}, {4, 12}}, y);
    }
    {
        std::vector<std::complex<double>> x {{1, 2}, {-3, 4}};
        std::vector<std::complex<double>> y {{5, -6}, {7, 8}};
        std::vector<std::complex<double>> out(2);
        bakuage::VectorAdd<std::complex<double>>(x.data(), y.data(), out.data(), out.size());
        ExpectComplexDoubleVectorEq({{6, -4}, {4, 12}}, out);
        bakuage::VectorAddInplace<std::complex<double>>(x.data(), y.data(), y.size());
        ExpectComplexDoubleVectorEq({{6, -4}, {4, 12}}, y);
    }
}

TEST(VectorMath, VectorMulAndInplace) {
    {
        std::vector<float> x {1, -2, 3};
        std::vector<float> y {4, 5, -6};
        std::vector<float> out(3);
        bakuage::VectorMul<float>(x.data(), y.data(), out.data(), out.size());
        ExpectVectorEq<float>({4, -10, -18}, out);
        bakuage::VectorMulInplace<float>(x.data(), y.data(), y.size());
        ExpectVectorEq<float>({4, -10, -18}, y);
    }
    {
        std::vector<double> x {1, -2, 3};
        std::vector<double> y {4, 5, -6};
        std::vector<double> out(3);
        bakuage::VectorMul<double>(x.data(), y.data(), out.data(), out.size());
        ExpectVectorEq<double>({4, -10, -18}, out);
        bakuage::VectorMulInplace<double>(x.data(), y.data(), y.size());
        ExpectVectorEq<double>({4, -10, -18}, y);
    }
    {
        std::vector<float> x {1, -2, 3};
        std::vector<float> out(3);
        bakuage::VectorMulConstant<float>(x.data(), 2.0f, out.data(), out.size());
        ExpectVectorEq<float>({2, -4, 6}, out);
    }
    {
        std::vector<double> x {1, -2, 3};
        std::vector<double> out(3);
        bakuage::VectorMulConstant<double>(x.data(), -0.5, out.data(), out.size());
        ExpectVectorEq<double>({-0.5, 1.0, -1.5}, out);
    }
    {
        std::vector<float> scale {2, -3};
        std::vector<std::complex<float>> x {{1, 2}, {-3, 4}};
        std::vector<std::complex<float>> out(2);
        bakuage::VectorMul<float, std::complex<float>>(scale.data(), x.data(), out.data(), out.size());
        ExpectComplexFloatVectorEq({{2, 4}, {9, -12}}, out);
        bakuage::VectorMulInplace<float, std::complex<float>>(scale.data(), x.data(), x.size());
        ExpectComplexFloatVectorEq({{2, 4}, {9, -12}}, x);
    }
    {
        std::vector<std::complex<float>> x {{1, 2}, {-3, 4}};
        std::vector<std::complex<float>> y {{5, -6}, {7, 8}};
        std::vector<std::complex<float>> out(2);
        bakuage::VectorMul<std::complex<float>, std::complex<float>>(x.data(), y.data(), out.data(), out.size());
        ExpectComplexFloatVectorEq({{17, 4}, {-53, 4}}, out);
        bakuage::VectorMulInplace<std::complex<float>, std::complex<float>>(x.data(), y.data(), y.size());
        ExpectComplexFloatVectorEq({{17, 4}, {-53, 4}}, y);
    }
    {
        std::vector<double> scale {2, -3};
        std::vector<std::complex<double>> x {{1, 2}, {-3, 4}};
        std::vector<std::complex<double>> out(2);
        bakuage::VectorMul<double, std::complex<double>>(scale.data(), x.data(), out.data(), out.size());
        ExpectComplexDoubleVectorEq({{2, 4}, {9, -12}}, out);
        bakuage::VectorMulInplace<double, std::complex<double>>(scale.data(), x.data(), x.size());
        ExpectComplexDoubleVectorEq({{2, 4}, {9, -12}}, x);
    }
    {
        std::vector<std::complex<double>> x {{1, 2}, {-3, 4}};
        std::vector<std::complex<double>> y {{5, -6}, {7, 8}};
        std::vector<std::complex<double>> out(2);
        bakuage::VectorMul<std::complex<double>, std::complex<double>>(x.data(), y.data(), out.data(), out.size());
        ExpectComplexDoubleVectorEq({{17, 4}, {-53, 4}}, out);
        bakuage::VectorMulInplace<std::complex<double>, std::complex<double>>(x.data(), y.data(), y.size());
        ExpectComplexDoubleVectorEq({{17, 4}, {-53, 4}}, y);
    }
}

TEST(VectorMath, VectorMulConstantInplace) {
    {
        std::vector<float> x {1, -2, 3};
        bakuage::VectorMulConstantInplace<float, float>(2.5f, x.data(), x.size());
        ExpectVectorEq<float>({2.5f, -5.0f, 7.5f}, x);
    }
    {
        std::vector<double> x {1, -2, 3};
        bakuage::VectorMulConstantInplace<double, double>(-0.5, x.data(), x.size());
        ExpectVectorEq<double>({-0.5, 1.0, -1.5}, x);
    }
    {
        std::vector<std::complex<float>> x {{1, 2}, {-3, 4}};
        bakuage::VectorMulConstantInplace<float, std::complex<float>>(2.0f, x.data(), x.size());
        ExpectComplexFloatVectorEq({{2, 4}, {-6, 8}}, x);
    }
    {
        std::vector<std::complex<float>> x {{1, 2}, {-3, 4}};
        bakuage::VectorMulConstantInplace<std::complex<float>, std::complex<float>>({2, -1}, x.data(), x.size());
        ExpectComplexFloatVectorEq({{4, 3}, {-2, 11}}, x);
    }
    {
        std::vector<std::complex<double>> x {{1, 2}, {-3, 4}};
        bakuage::VectorMulConstantInplace<std::complex<double>, std::complex<double>>({2, -1}, x.data(), x.size());
        ExpectComplexDoubleVectorEq({{4, 3}, {-2, 11}}, x);
    }
}
