/**
 * @file Integer.h
 * @brief Header file for an efficient C++ arbitrary-precision integer arithmetic library.
 *
 * This header provides high-performance arbitrary-precision integer types and related algorithms,
 * supporting construction, arithmetic operations, and input/output for both unsigned and signed big integers.
 * It is suitable for scenarios requiring large number computations.
 *
 * @author [masonxiong](https://www.luogu.com.cn/user/446979), [yuygfgg](https://www.luogu.com.cn/user/251551)
 * @date 2025-9-10
 */

#ifndef INTEGER_H
#define INTEGER_H 20250910L

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#ifdef ENABLE_VALIDITY_CHECK
#define VALIDITY_CHECK(condition, errorType, message) \
    if (!(condition)) throw errorType(message);
#else
#define VALIDITY_CHECK(condition, errorType, message)
#endif

#if defined(__AVX2__)
#include <immintrin.h>
#elif defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

#ifndef __CONSTEXPR
#if __cplusplus >= 201402L
#define __CONSTEXPR constexpr
#else
#define __CONSTEXPR
#endif
#endif

namespace detail {
    constexpr std::uint32_t log2(std::uint32_t n) {
        VALIDITY_CHECK(n, std::invalid_argument, "log2 error: the provided integer is zero.");
#if defined(__GNUC__) && !defined(__clang__)
        return std::__lg(n);
#else
        return std::log2(n);
#endif
    }

    struct InputHelper {
        std::uint32_t table[0x10000];

        __CONSTEXPR InputHelper() : table() {
            for (std::uint32_t i = 48; i != 58; ++i)
                for (std::uint32_t j = 48; j != 58; ++j)
                    table[i << 8 | j] = (j & 15) * 10 + (i & 15);
        }

        std::uint32_t operator()(const char* value) const {
            const std::uint32_t high = static_cast<std::uint8_t>(value[1]);
            const std::uint32_t low = static_cast<std::uint8_t>(value[0]);
            return table[(high << 8) | low];
        }
    };

    struct OutputHelper {
        std::uint32_t table[10000];

        __CONSTEXPR OutputHelper() : table() {
            for (std::uint32_t *i = table, a = 48; a != 58; ++a)
                for (std::uint32_t b = 48; b != 58; ++b)
                    for (std::uint32_t c = 48; c != 58; ++c)
                        for (std::uint32_t d = 48; d != 58; ++d)
                            *i++ = a | (b << 8) | (c << 16) | (d << 24);
        }

        const char* operator()(std::uint32_t value) const {
            return reinterpret_cast<const char*>(table + value);
        }
    };

#if defined(__AVX2__)
    struct TransformHelper {
        __m128d* twiddleFactors;
        std::uint32_t length;

        TransformHelper() : twiddleFactors(new __m128d[1]()), length(1) {
            *twiddleFactors = _mm_set_pd(0.0, 1.0);
        }

        ~TransformHelper() noexcept {
            delete[] twiddleFactors;
        }

        static inline __m128d complexMultiply(__m128d first, __m128d second) {
            return _mm_fmaddsub_pd(_mm_unpacklo_pd(first, first), second, _mm_unpackhi_pd(first, first) * _mm_permute_pd(second, 1));
        }

        static inline __m128d complexMultiplyConjugate(__m128d first, __m128d second) {
            return _mm_fmsubadd_pd(_mm_unpacklo_pd(second, second), first, _mm_unpackhi_pd(second, second) * _mm_permute_pd(first, 1));
        }

        static inline __m128d complexMultiplySpecial(__m128d first, __m128d second) {
            return _mm_fmadd_pd(_mm_unpacklo_pd(first, first), second, _mm_unpackhi_pd(first, first) * _mm_permute_pd(second, 1));
        }

        static inline __m128d complexScalarMultiply(__m128d complex, double scalar) {
            return complex * _mm_set1_pd(scalar);
        }

        void resize(std::uint32_t transformLength) {
            if (transformLength > length << 1) {
                std::uint32_t halfLog = detail::log2(transformLength) >> 1, halfSize = 1 << halfLog;
                __m128d* baseFactors = new __m128d[halfSize << 1]();
                const double angleStep = std::acos(-1.0) / halfSize, fineAngleStep = angleStep / halfSize;
                for (std::uint32_t i = 0, j = (halfSize * 3) >> 1, phaseAccumulator = 0; i != halfSize; phaseAccumulator -= halfSize - (j >> __builtin_ctz(++i))) {
                    std::complex<double> first = std::polar(1.0, phaseAccumulator * angleStep), second = std::polar(1.0, phaseAccumulator * fineAngleStep);
                    baseFactors[i] = _mm_set_pd(first.imag(), first.real()), baseFactors[i | halfSize] = _mm_set_pd(second.imag(), second.real());
                }
                __m128d* newFactors = reinterpret_cast<__m128d*>(std::memcpy(new __m128d[transformLength >> 1], twiddleFactors, length << 4));
                delete[] twiddleFactors, twiddleFactors = newFactors;
                for (std::uint32_t i = length; i != transformLength >> 1; ++i)
                    twiddleFactors[i] = complexMultiply(baseFactors[i & (halfSize - 1)], baseFactors[halfSize | (i >> halfLog)]);
                delete[] baseFactors, length = transformLength >> 1;
            }
        }

        void decimationInFrequency(__m128d* dataArray, std::uint32_t transformSize) {
            for (std::uint32_t blockSize = transformSize >> 1, stepSize = transformSize; blockSize; stepSize = blockSize, blockSize >>= 1) {
                for (__m128d* currentElement = dataArray; currentElement != dataArray + blockSize; ++currentElement) {
                    const __m128d evenElement = *currentElement, oddElement = currentElement[blockSize];
                    *currentElement = evenElement + oddElement, currentElement[blockSize] = evenElement - oddElement;
                }
                for (__m128d *blockStart = dataArray + stepSize, *twiddlePointer = twiddleFactors + 1; blockStart != dataArray + transformSize; blockStart += stepSize, ++twiddlePointer) {
                    for (__m128d* currentElement = blockStart; currentElement != blockStart + blockSize; ++currentElement) {
                        const __m128d evenElement = *currentElement, oddElement = complexMultiply(currentElement[blockSize], *twiddlePointer);
                        *currentElement = evenElement + oddElement, currentElement[blockSize] = evenElement - oddElement;
                    }
                }
            }
        }

        void decimationInTime(__m128d* dataArray, std::uint32_t transformSize) {
            for (std::uint32_t blockSize = 1, stepSize = 2; blockSize != transformSize; blockSize = stepSize, stepSize <<= 1) {
                for (__m128d* currentElement = dataArray; currentElement != dataArray + blockSize; ++currentElement) {
                    const __m128d evenElement = *currentElement, oddElement = currentElement[blockSize];
                    *currentElement = evenElement + oddElement, currentElement[blockSize] = evenElement - oddElement;
                }
                for (__m128d *blockStart = dataArray + stepSize, *twiddlePointer = twiddleFactors + 1; blockStart != dataArray + transformSize; blockStart += stepSize, ++twiddlePointer) {
                    for (__m128d* currentElement = blockStart; currentElement != blockStart + blockSize; ++currentElement) {
                        const __m128d evenElement = *currentElement, oddElement = currentElement[blockSize];
                        *currentElement = evenElement + oddElement, currentElement[blockSize] = complexMultiplyConjugate(evenElement - oddElement, *twiddlePointer);
                    }
                }
            }
        }

        void frequencyDomainPointwiseMultiply(__m128d* firstArray, __m128d* secondArray, std::uint32_t transformSize) {
            const double normalizationFactor = 1.0 / transformSize, scalingFactor = normalizationFactor * 0.25;
            firstArray[0] = complexScalarMultiply(complexMultiplySpecial(firstArray[0], secondArray[0]), normalizationFactor);
            firstArray[1] = complexScalarMultiply(complexMultiply(firstArray[1], secondArray[1]), normalizationFactor);
            const __m128d conjugateMask = _mm_castsi128_pd(_mm_set_epi64x(std::int64_t(1ull << 63), 0));
            const __m128d negateMask = _mm_castsi128_pd(_mm_set_epi64x(std::int64_t(1ull << 63), std::int64_t(1ull << 63)));
            for (std::uint32_t blockStart = 2, blockEnd = 3; blockStart != transformSize; blockStart <<= 1, blockEnd <<= 1) {
                for (std::uint32_t forwardIndex = blockStart, backwardIndex = forwardIndex + blockStart - 1; forwardIndex != blockEnd; ++forwardIndex, --backwardIndex) {
                    const __m128d firstEven = _mm_add_pd(firstArray[forwardIndex], _mm_xor_pd(firstArray[backwardIndex], conjugateMask)), firstOdd = _mm_sub_pd(firstArray[forwardIndex], _mm_xor_pd(firstArray[backwardIndex], conjugateMask));
                    const __m128d secondEven = _mm_add_pd(secondArray[forwardIndex], _mm_xor_pd(secondArray[backwardIndex], conjugateMask)), secondOdd = _mm_sub_pd(secondArray[forwardIndex], _mm_xor_pd(secondArray[backwardIndex], conjugateMask));
                    const __m128d productA = _mm_sub_pd(complexMultiply(firstEven, secondEven), complexMultiply(complexMultiply(firstOdd, secondOdd), (forwardIndex & 1 ? _mm_xor_pd(twiddleFactors[forwardIndex >> 1], negateMask) : twiddleFactors[forwardIndex >> 1]))), productB = _mm_add_pd(complexMultiply(secondEven, firstOdd), complexMultiply(firstEven, secondOdd));
                    firstArray[forwardIndex] = complexScalarMultiply(_mm_add_pd(productA, productB), scalingFactor);
                    firstArray[backwardIndex] = _mm_xor_pd(complexScalarMultiply(_mm_sub_pd(productA, productB), scalingFactor), conjugateMask);
                }
            }
        }
    };
#elif defined(__ARM_NEON__)
    struct TransformHelper {
        float64x2_t* twiddleFactors;
        std::uint32_t length;

        TransformHelper() : twiddleFactors(new float64x2_t[1]()), length(1) {
            *twiddleFactors = vsetq_lane_f64(0.0, vsetq_lane_f64(1.0, vdupq_n_f64(0.0), 0), 1);
        }

        ~TransformHelper() noexcept {
            delete[] twiddleFactors;
        }

        static inline float64x2_t complexMultiply(float64x2_t first, float64x2_t second) {
            float64x2_t term1 = vmulq_laneq_f64(second, first, 0);
            float64x2_t second_swapped = vextq_f64(second, second, 1);
            float64x2_t term2_raw = vmulq_laneq_f64(second_swapped, first, 1);
            const float64x2_t mask_neg_real = vsetq_lane_f64(1.0, vsetq_lane_f64(-1.0, vdupq_n_f64(0.0), 0), 1);
            return vfmaq_f64(term1, term2_raw, mask_neg_real);
        }

        static inline float64x2_t complexMultiplyConjugate(float64x2_t first, float64x2_t second) {
            float64x2_t second_swapped = vextq_f64(second, second, 1);
            float64x2_t term1 = vmulq_laneq_f64(second_swapped, first, 1);
            float64x2_t term2_raw = vmulq_laneq_f64(second, first, 0);
            const float64x2_t mask_add_real_sub_imag = vsetq_lane_f64(-1.0, vsetq_lane_f64(1.0, vdupq_n_f64(0.0), 0), 1);
            return vfmaq_f64(term1, term2_raw, mask_add_real_sub_imag);
        }

        static inline float64x2_t complexMultiplySpecial(float64x2_t first, float64x2_t second) {
            float64x2_t term1 = vmulq_laneq_f64(second, first, 0);
            float64x2_t second_rev = vextq_f64(second, second, 1);
            return vfmaq_laneq_f64(term1, second_rev, first, 1);
        }

        static inline float64x2_t complexScalarMultiply(float64x2_t complex, double scalar) {
            return vmulq_n_f64(complex, scalar);
        }

        void resize(std::uint32_t transformLength) {
            if (transformLength > length << 1) {
                std::uint32_t halfLog = detail::log2(transformLength) >> 1, halfSize = 1 << halfLog;
                float64x2_t* baseFactors = new float64x2_t[halfSize << 1]();
                const double angleStep = std::acos(-1.0) / halfSize, fineAngleStep = angleStep / halfSize;
                for (std::uint32_t i = 0, j = (halfSize * 3) >> 1, phaseAccumulator = 0; i != halfSize; phaseAccumulator -= halfSize - (j >> __builtin_ctz(++i))) {
                    std::complex<double> first = std::polar(1.0, phaseAccumulator * angleStep), second = std::polar(1.0, phaseAccumulator * fineAngleStep);
                    baseFactors[i] = vsetq_lane_f64(first.imag(), vsetq_lane_f64(first.real(), vdupq_n_f64(0.0), 0), 1);
                    baseFactors[i | halfSize] = vsetq_lane_f64(second.imag(), vsetq_lane_f64(second.real(), vdupq_n_f64(0.0), 0), 1);
                }
                float64x2_t* newFactors = new float64x2_t[transformLength >> 1];
                std::memcpy(newFactors, twiddleFactors, length * sizeof(float64x2_t));
                delete[] twiddleFactors;
                twiddleFactors = newFactors;
                for (std::uint32_t i = length; i != transformLength >> 1; ++i)
                    twiddleFactors[i] = complexMultiply(baseFactors[i & (halfSize - 1)], baseFactors[halfSize | (i >> halfLog)]);
                delete[] baseFactors;
                length = transformLength >> 1;
            }
        }

        void decimationInFrequency(float64x2_t* dataArray, std::uint32_t transformSize) {
            for (std::uint32_t blockSize = transformSize >> 1, stepSize = transformSize; blockSize; stepSize = blockSize, blockSize >>= 1) {
                for (float64x2_t* currentElement = dataArray; currentElement != dataArray + blockSize; ++currentElement) {
                    const float64x2_t evenElement = *currentElement, oddElement = currentElement[blockSize];
                    *currentElement = vaddq_f64(evenElement, oddElement);
                    currentElement[blockSize] = vsubq_f64(evenElement, oddElement);
                }
                for (float64x2_t *blockStart = dataArray + stepSize, *twiddlePointer = twiddleFactors + 1; blockStart != dataArray + transformSize; blockStart += stepSize, ++twiddlePointer) {
                    for (float64x2_t* currentElement = blockStart; currentElement != blockStart + blockSize; ++currentElement) {
                        const float64x2_t evenElement = *currentElement, oddElement = complexMultiply(currentElement[blockSize], *twiddlePointer);
                        *currentElement = vaddq_f64(evenElement, oddElement);
                        currentElement[blockSize] = vsubq_f64(evenElement, oddElement);
                    }
                }
            }
        }

        void decimationInTime(float64x2_t* dataArray, std::uint32_t transformSize) {
            for (std::uint32_t blockSize = 1, stepSize = 2; blockSize != transformSize; blockSize = stepSize, stepSize <<= 1) {
                for (float64x2_t* currentElement = dataArray; currentElement != dataArray + blockSize; ++currentElement) {
                    const float64x2_t evenElement = *currentElement, oddElement = currentElement[blockSize];
                    *currentElement = vaddq_f64(evenElement, oddElement);
                    currentElement[blockSize] = vsubq_f64(evenElement, oddElement);
                }
                for (float64x2_t *blockStart = dataArray + stepSize, *twiddlePointer = twiddleFactors + 1; blockStart != dataArray + transformSize; blockStart += stepSize, ++twiddlePointer) {
                    for (float64x2_t* currentElement = blockStart; currentElement != blockStart + blockSize; ++currentElement) {
                        const float64x2_t evenElement = *currentElement, oddElement = currentElement[blockSize];
                        *currentElement = vaddq_f64(evenElement, oddElement);
                        currentElement[blockSize] = complexMultiplyConjugate(vsubq_f64(evenElement, oddElement), *twiddlePointer);
                    }
                }
            }
        }

        void frequencyDomainPointwiseMultiply(float64x2_t* firstArray, float64x2_t* secondArray, std::uint32_t transformSize) {
            const double normalizationFactor = 1.0 / transformSize, scalingFactor = normalizationFactor * 0.25;
            firstArray[0] = complexScalarMultiply(complexMultiplySpecial(firstArray[0], secondArray[0]), normalizationFactor);
            firstArray[1] = complexScalarMultiply(complexMultiply(firstArray[1], secondArray[1]), normalizationFactor);
            const float64x2_t conjugateMask = vsetq_lane_f64(-1.0, vsetq_lane_f64(1.0, vdupq_n_f64(0.0), 0), 1);
            const float64x2_t negateMask = vdupq_n_f64(-1.0);
            for (std::uint32_t blockStart = 2, blockEnd = 3; blockStart != transformSize; blockStart <<= 1, blockEnd <<= 1) {
                for (std::uint32_t forwardIndex = blockStart, backwardIndex = forwardIndex + blockStart - 1; forwardIndex != blockEnd; ++forwardIndex, --backwardIndex) {
                    auto conj = [&](float64x2_t v) { return vmulq_f64(v, conjugateMask); };
                    const float64x2_t firstEven = vaddq_f64(firstArray[forwardIndex], conj(firstArray[backwardIndex])), firstOdd = vsubq_f64(firstArray[forwardIndex], conj(firstArray[backwardIndex]));
                    const float64x2_t secondEven = vaddq_f64(secondArray[forwardIndex], conj(secondArray[backwardIndex])), secondOdd = vsubq_f64(secondArray[forwardIndex], conj(secondArray[backwardIndex]));
                    const float64x2_t twiddle = (forwardIndex & 1 ? vmulq_f64(twiddleFactors[forwardIndex >> 1], negateMask) : twiddleFactors[forwardIndex >> 1]);
                    const float64x2_t productA = vsubq_f64(complexMultiply(firstEven, secondEven), complexMultiply(complexMultiply(firstOdd, secondOdd), twiddle)), productB = vaddq_f64(complexMultiply(secondEven, firstOdd), complexMultiply(firstEven, secondOdd));
                    firstArray[forwardIndex] = complexScalarMultiply(vaddq_f64(productA, productB), scalingFactor);
                    firstArray[backwardIndex] = conj(complexScalarMultiply(vsubq_f64(productA, productB), scalingFactor));
                }
            }
        }
    };
#else
    struct TransformHelper {
        std::complex<double>* twiddleFactors;
        std::uint32_t length;

        TransformHelper() : twiddleFactors(new std::complex<double>[1]()), length(1) {
            *twiddleFactors = {1.0, 0.0};
        }

        ~TransformHelper() noexcept {
            delete[] twiddleFactors;
        }

        static inline std::complex<double> complexMultiply(std::complex<double> first, std::complex<double> second) {
            return first * second;
        }

        static inline std::complex<double> complexMultiplyConjugate(std::complex<double> first, std::complex<double> second) {
            return first * std::conj(second);
        }

        std::complex<double> complexMultiplySpecial(
            const std::complex<double>& first,
            const std::complex<double>& second) {
            double r1 = first.real();
            double i1 = first.imag();
            double r2 = second.real();
            double i2 = second.imag();

            double result_real = r1 * r2 + i1 * i2;

            double result_imag = r1 * i2 + i1 * r2;

            return std::complex<double>(result_real, result_imag);
        }

        static inline std::complex<double> complexScalarMultiply(std::complex<double> complex, double scalar) {
            return complex * scalar;
        }

        void resize(std::uint32_t transformLength) {
            if (transformLength > length << 1) {
                std::uint32_t halfLog = detail::log2(transformLength) >> 1, halfSize = 1 << halfLog;
                std::complex<double>* baseFactors = new std::complex<double>[halfSize << 1]();
                const double angleStep = std::acos(-1.0) / halfSize, fineAngleStep = angleStep / halfSize;
                for (std::uint32_t i = 0, j = (halfSize * 3) >> 1, phaseAccumulator = 0; i != halfSize; phaseAccumulator -= halfSize - (j >> __builtin_ctz(++i))) {
                    baseFactors[i] = std::polar(1.0, phaseAccumulator * angleStep);
                    baseFactors[i | halfSize] = std::polar(1.0, phaseAccumulator * fineAngleStep);
                }
                std::complex<double>* newFactors = new std::complex<double>[transformLength >> 1];
                std::memcpy(newFactors, twiddleFactors, length * sizeof(std::complex<double>));
                delete[] twiddleFactors;
                twiddleFactors = newFactors;
                for (std::uint32_t i = length; i != transformLength >> 1; ++i)
                    twiddleFactors[i] = baseFactors[i & (halfSize - 1)] * baseFactors[halfSize | (i >> halfLog)];
                delete[] baseFactors;
                length = transformLength >> 1;
            }
        }

        void decimationInFrequency(std::complex<double>* dataArray, std::uint32_t transformSize) {
            for (std::uint32_t blockSize = transformSize >> 1, stepSize = transformSize; blockSize; stepSize = blockSize, blockSize >>= 1) {
                for (std::complex<double>* currentElement = dataArray; currentElement != dataArray + blockSize; ++currentElement) {
                    const std::complex<double> evenElement = *currentElement, oddElement = currentElement[blockSize];
                    *currentElement = evenElement + oddElement;
                    currentElement[blockSize] = evenElement - oddElement;
                }
                for (std::complex<double>*blockStart = dataArray + stepSize, *twiddlePointer = twiddleFactors + 1; blockStart != dataArray + transformSize; blockStart += stepSize, ++twiddlePointer) {
                    for (std::complex<double>* currentElement = blockStart; currentElement != blockStart + blockSize; ++currentElement) {
                        const std::complex<double> evenElement = *currentElement, oddElement = currentElement[blockSize] * *twiddlePointer;
                        *currentElement = evenElement + oddElement;
                        currentElement[blockSize] = evenElement - oddElement;
                    }
                }
            }
        }

        void decimationInTime(std::complex<double>* dataArray, std::uint32_t transformSize) {
            for (std::uint32_t blockSize = 1, stepSize = 2; blockSize != transformSize; blockSize = stepSize, stepSize <<= 1) {
                for (std::complex<double>* currentElement = dataArray; currentElement != dataArray + blockSize; ++currentElement) {
                    const std::complex<double> evenElement = *currentElement, oddElement = currentElement[blockSize];
                    *currentElement = evenElement + oddElement;
                    currentElement[blockSize] = evenElement - oddElement;
                }
                for (std::complex<double>*blockStart = dataArray + stepSize, *twiddlePointer = twiddleFactors + 1; blockStart != dataArray + transformSize; blockStart += stepSize, ++twiddlePointer) {
                    for (std::complex<double>* currentElement = blockStart; currentElement != blockStart + blockSize; ++currentElement) {
                        const std::complex<double> evenElement = *currentElement, oddElement = currentElement[blockSize];
                        *currentElement = evenElement + oddElement;
                        currentElement[blockSize] = (evenElement - oddElement) * std::conj(*twiddlePointer);
                    }
                }
            }
        }

        void frequencyDomainPointwiseMultiply(std::complex<double>* firstArray, std::complex<double>* secondArray, std::uint32_t transformSize) {
            const double normalizationFactor = 1.0 / transformSize, scalingFactor = normalizationFactor * 0.25;
            firstArray[0] = complexScalarMultiply(complexMultiplySpecial(firstArray[0], secondArray[0]), normalizationFactor);
            firstArray[1] = complexScalarMultiply(complexMultiply(firstArray[1], secondArray[1]), normalizationFactor);
            for (std::uint32_t blockStart = 2, blockEnd = 3; blockStart != transformSize; blockStart <<= 1, blockEnd <<= 1) {
                for (std::uint32_t forwardIndex = blockStart, backwardIndex = forwardIndex + blockStart - 1; forwardIndex != blockEnd; ++forwardIndex, --backwardIndex) {
                    const std::complex<double> firstEven = firstArray[forwardIndex] + std::conj(firstArray[backwardIndex]), firstOdd = firstArray[forwardIndex] - std::conj(firstArray[backwardIndex]);
                    const std::complex<double> secondEven = secondArray[forwardIndex] + std::conj(secondArray[backwardIndex]), secondOdd = secondArray[forwardIndex] - std::conj(secondArray[backwardIndex]);
                    const std::complex<double> twiddle = (forwardIndex & 1 ? -twiddleFactors[forwardIndex >> 1] : twiddleFactors[forwardIndex >> 1]);
                    const std::complex<double> productA = firstEven * secondEven - firstOdd * secondOdd * twiddle, productB = secondEven * firstOdd + firstEven * secondOdd;
                    firstArray[forwardIndex] = (productA + productB) * scalingFactor;
                    firstArray[backwardIndex] = std::conj((productA - productB) * scalingFactor);
                }
            }
        }
    };
#endif

    static __CONSTEXPR InputHelper I = {};
    static __CONSTEXPR OutputHelper O = {};
    static thread_local TransformHelper T = {};
} // namespace detail

class UnsignedInteger;
class SignedInteger;

class UnsignedInteger {
    static constexpr std::uint32_t Base = 100000000;
    static constexpr std::uint32_t TransformLimit = 4194304;
    static constexpr std::uint32_t BruteforceThreshold = 64;

    std::uint32_t *digits, length, capacity;

  protected:
    UnsignedInteger(std::uint32_t initialLength, std::uint32_t initialCapacity) : digits(new std::uint32_t[initialCapacity]), length(initialLength), capacity(initialCapacity) {}

    void resize(std::uint32_t newLength) {
        if (newLength > capacity) {
            std::uint32_t* newMemory = reinterpret_cast<std::uint32_t*>(std::memcpy(new std::uint32_t[capacity = newLength], digits, length << 2));
            delete[] digits, digits = newMemory;
        }
        length = newLength;
    }

    void construct(const char* value, std::uint32_t stringLength) {
        std::uint32_t* currentDigit = digits + length - 1;
        switch (stringLength & 7) {
            case 0:
                ++currentDigit;
                break;
            case 1:
                *currentDigit = *value & 15;
                break;
            case 2:
                *currentDigit = detail::I(value);
                break;
            case 3:
                *currentDigit = (*value & 15) * 100 + detail::I(value + 1);
                break;
            case 4:
                *currentDigit = detail::I(value) * 100 + detail::I(value + 2);
                break;
            case 5:
                *currentDigit = (*value & 15) * 10000 + detail::I(value + 1) * 100 + detail::I(value + 3);
                break;
            case 6:
                *currentDigit = detail::I(value) * 10000 + detail::I(value + 2) * 100 + detail::I(value + 4);
                break;
            case 7:
                *currentDigit = (*value & 15) * 1000000 + detail::I(value + 1) * 10000 + detail::I(value + 3) * 100 + detail::I(value + 5);
                break;
        }
        for (const char* position = value + (stringLength & 7); currentDigit != digits; *--currentDigit = detail::I(position) * 1000000 + detail::I(position + 2) * 10000 + detail::I(position + 4) * 100 + detail::I(position + 6), position += 8);
        for (; length > 1 && !digits[length - 1]; --length);
    }

    std::int32_t reverseCompare(const UnsignedInteger& other) const {
        constexpr std::uint32_t BlockSize = 64;
        const std::uint32_t remaining = length % BlockSize, *firstBlockPointer = digits + length - remaining, *secondBlockPointer = other.digits + length - remaining;
        if (std::memcmp(firstBlockPointer, secondBlockPointer, remaining << 2))
            for (const std::uint32_t *firstElementPointer = firstBlockPointer + remaining, *secondElementPointer = secondBlockPointer + remaining; firstElementPointer != firstBlockPointer;)
                if (*--firstElementPointer != *--secondElementPointer)
                    return std::int32_t(*firstElementPointer - *secondElementPointer);
        while (firstBlockPointer != digits)
            if (std::memcmp(firstBlockPointer -= BlockSize, secondBlockPointer -= BlockSize, BlockSize << 2))
                for (const std::uint32_t *firstElementPointer = firstBlockPointer + BlockSize, *secondElementPointer = secondBlockPointer + BlockSize; firstElementPointer != firstBlockPointer;)
                    if (*--firstElementPointer != *--secondElementPointer)
                        return std::int32_t(*firstElementPointer - *secondElementPointer);
        return 0;
    }

    std::pair<UnsignedInteger, UnsignedInteger> bruteforceDivisionAndModulus(const UnsignedInteger& divisor) const {
        if (*this < divisor)
            return std::make_pair(UnsignedInteger(), *this);
        UnsignedInteger quotient(length - divisor.length + 1, length - divisor.length + 1), remainder = *this;
        quotient.length = length - divisor.length + 1;
        auto getEstimatedValue = [](const std::uint32_t* digitArray, std::uint32_t highIndex, std::uint32_t arrayLength) -> std::uint64_t {
            return std::uint64_t(10) * Base * ((highIndex + 1) < arrayLength ? digitArray[highIndex + 1] : 0) + std::uint64_t(10) * digitArray[highIndex] + (highIndex ? digitArray[highIndex - 1] : 0) / (Base / 10);
        };
        for (std::uint32_t currentPosition = length - divisor.length; ~currentPosition; --currentPosition) {
            std::uint32_t partialQuotient = 0;
            auto performSubtraction = [&]() {
                std::int64_t carry = 0;
                for (std::uint32_t digitIndex = 0; digitIndex != divisor.length; ++digitIndex) {
                    carry = carry - std::int64_t(partialQuotient) * divisor.digits[digitIndex] + remainder.digits[currentPosition + digitIndex];
                    remainder.digits[currentPosition + digitIndex] = std::uint32_t(carry % Base), carry /= Base;
                    if (remainder.digits[currentPosition + digitIndex] >= Base)
                        remainder.digits[currentPosition + digitIndex] += Base, --carry;
                }
                if (carry)
                    remainder.digits[currentPosition + divisor.length] += std::uint32_t(carry);
                quotient.digits[currentPosition] += partialQuotient;
            };
            for (quotient.digits[currentPosition] = 0; (partialQuotient = std::uint32_t(getEstimatedValue(remainder.digits, currentPosition + divisor.length - 1, length) / (getEstimatedValue(divisor.digits, divisor.length - 1, divisor.length) + 1))); performSubtraction());
            partialQuotient = 1;
            for (std::uint32_t digitIndex = divisor.length; digitIndex--;)
                if (remainder.digits[digitIndex + currentPosition] != divisor.digits[digitIndex] && (partialQuotient = divisor.digits[digitIndex] < remainder.digits[digitIndex + currentPosition], true))
                    break;
            if (partialQuotient)
                performSubtraction();
        }
        for (; quotient.length > 1 && !quotient.digits[quotient.length - 1]; --quotient.length);
        for (; remainder.length > 1 && !remainder.digits[remainder.length - 1]; --remainder.length);
        return std::make_pair(std::move(quotient), std::move(remainder));
    }

    UnsignedInteger rightShift(std::uint32_t shiftAmount) const {
        if (shiftAmount >= length)
            return UnsignedInteger();
        UnsignedInteger result(length - shiftAmount, length - shiftAmount);
        std::memcpy(result.digits, digits + shiftAmount, result.length << 2);
        for (; result.length > 1 && !result.digits[result.length - 1]; --result.length);
        return result;
    }

    UnsignedInteger leftShift(std::uint32_t shiftAmount) const {
        if (length == 0 || shiftAmount == 0)
            return *this;
        UnsignedInteger result(length + shiftAmount, length + shiftAmount);
        std::memset(result.digits, 0, shiftAmount << 2), std::memcpy(result.digits + shiftAmount, digits, length << 2);
        return result;
    }

    UnsignedInteger computeInverse(std::uint32_t precisionBits) const {
        if (length < BruteforceThreshold || precisionBits < length + BruteforceThreshold) {
            UnsignedInteger numerator(precisionBits + 1, precisionBits + 1);
            std::memset(numerator.digits, 0, precisionBits << 2), numerator.digits[precisionBits] = 1;
            return numerator.bruteforceDivisionAndModulus(*this).first;
        }
        const std::uint32_t halfPrecision = (precisionBits - length + 5) >> 1, shiftBack = halfPrecision > length ? 0 : length - halfPrecision;
        UnsignedInteger truncated = rightShift(shiftBack);
        const std::uint32_t newPrecision = halfPrecision + truncated.length;
        UnsignedInteger approximateInverse = truncated.computeInverse(newPrecision);
        UnsignedInteger result = (approximateInverse + approximateInverse).leftShift(precisionBits - newPrecision - shiftBack) - (*this * approximateInverse * approximateInverse).rightShift(2 * (newPrecision + shiftBack) - precisionBits);
        return --result;
    }

    std::pair<UnsignedInteger, UnsignedInteger> divisionAndModulus(const UnsignedInteger& other) const {
        if (*this < other)
            return std::make_pair(UnsignedInteger(), *this);
        if (length < BruteforceThreshold || other.length < BruteforceThreshold)
            return bruteforceDivisionAndModulus(other);
        const std::uint32_t precisionBits = length - other.length + 5, shiftBack = precisionBits > other.length ? 0 : other.length - precisionBits;
        UnsignedInteger adjustedDivisor = other.rightShift(shiftBack);
        if (shiftBack)
            ++adjustedDivisor;
        const std::uint32_t inversePrecision = precisionBits + adjustedDivisor.length;
        UnsignedInteger quotient = (*this * adjustedDivisor.computeInverse(inversePrecision)).rightShift(inversePrecision + shiftBack);
        while (quotient * other > *this)
            --quotient;
        UnsignedInteger remainder = *this - quotient * other;
        for (; remainder >= other; ++quotient, remainder -= other);
        return std::make_pair(std::move(quotient), std::move(remainder));
    }

  public:
    UnsignedInteger() : digits(new std::uint32_t[1]()), length(1), capacity(1) {}

    UnsignedInteger(const UnsignedInteger& other) : digits(reinterpret_cast<std::uint32_t*>(std::memcpy(new std::uint32_t[other.length], other.digits, other.length << 2))), length(other.length), capacity(other.length) {}

    UnsignedInteger(UnsignedInteger&& other) noexcept : digits(other.digits), length(other.length), capacity(other.capacity) {
        other.digits = new std::uint32_t[other.length = other.capacity = 1]();
    }

    UnsignedInteger(const SignedInteger& other);

    template <typename unsignedIntegral, typename std::enable_if<std::is_unsigned<unsignedIntegral>::value>::type* = nullptr>
    UnsignedInteger(unsignedIntegral value) : UnsignedInteger(0, (std::numeric_limits<unsignedIntegral>::digits10 + 7) >> 3) {
        while (digits[length++] = std::uint32_t(value) % Base, value /= unsignedIntegral(Base));
    }

    template <typename signedIntegral, typename std::enable_if<std::is_signed<signedIntegral>::value && !std::is_floating_point<signedIntegral>::value>::type* = nullptr>
    UnsignedInteger(signedIntegral value) : UnsignedInteger(0, (std::numeric_limits<signedIntegral>::digits10 + 7) >> 3) {
        VALIDITY_CHECK(value >= 0, std::invalid_argument, "UnsignedInteger constructor error: the provided signed integer value = " + std::to_string(value) + " is negative. UnsignedInteger can only represent non-negative integers.")
        while (digits[length++] = std::uint32_t(value) % Base, value /= signedIntegral(Base));
    }

    template <typename floatingPoint, typename std::enable_if<std::is_floating_point<floatingPoint>::value>::type* = nullptr>
    UnsignedInteger(floatingPoint value) : UnsignedInteger(0, (std::numeric_limits<floatingPoint>::max_exponent10 + 7) >> 3) {
        VALIDITY_CHECK(value >= 0, std::invalid_argument, "UnsignedInteger constructor error: the provided floating point value = " + std::to_string(value) + " is negative. UnsignedInteger can only represent non-negative integers.")
        VALIDITY_CHECK(std::isfinite(value), std::invalid_argument, "UnsignedInteger constructor error: the provided floating point value = " + std::to_string(value) + " is not finite.")
        while (digits[length++] = std::uint32_t(std::fmod(value, Base)), (value = std::floor(value / Base)));
    }

    UnsignedInteger(const char* value) {
        VALIDITY_CHECK(value, std::invalid_argument, "UnsignedInteger constructor error: the provided C-style string is a null pointer.")
        const std::uint32_t stringLength = std::uint32_t(std::strlen(value));
        digits = new std::uint32_t[length = capacity = (stringLength + 7) >> 3];
        VALIDITY_CHECK(stringLength, std::invalid_argument, "UnsignedInteger constructor error: the provided C-style string is empty. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        VALIDITY_CHECK(std::all_of(value, value + stringLength, [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "UnsignedInteger constructor error: the provided C-style string value = "
                                                                                                                                                + std::string(value) +
                                                                                                                                                " contains non-digit characters. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        construct(value, stringLength);
    }

    UnsignedInteger(const std::string& value) : UnsignedInteger(std::uint32_t(value.size() + 7) >> 3, std::uint32_t(value.size() + 7) >> 3) {
        VALIDITY_CHECK(value.size(), std::invalid_argument, "UnsignedInteger constructor error: the provided string is empty. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        VALIDITY_CHECK(std::all_of(value.begin(), value.end(), [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "UnsignedInteger constructor error: the provided string value = "
                                                                                                                                               + value +
                                                                                                                                               " contains non-digit characters. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        construct(value.data(), std::uint32_t(value.size()));
    }

    ~UnsignedInteger() noexcept {
        delete[] digits;
    }

    UnsignedInteger& operator=(const UnsignedInteger& other) {
        if (digits != other.digits) {
            if (capacity < other.length)
                delete[] digits, digits = new std::uint32_t[other.length], capacity = other.length;
            std::memcpy(digits, other.digits, (length = other.length) << 2);
        }
        return *this;
    }

    UnsignedInteger& operator=(UnsignedInteger&& other) noexcept {
        if (this != &other) {
            if (digits != other.digits) {
                delete[] digits;
                digits = other.digits;
                length = other.length;
                capacity = other.capacity;
                other.digits = new std::uint32_t[other.length = other.capacity = 1]();
            }
        }
        return *this;
    }

    UnsignedInteger& operator=(const SignedInteger& other);

    template <typename unsignedIntegral, typename std::enable_if<std::is_unsigned<unsignedIntegral>::value>::type* = nullptr>
    UnsignedInteger& operator=(unsignedIntegral value) {
        if (length = 0, capacity < (std::numeric_limits<unsignedIntegral>::digits10 + 7) >> 3)
            delete[] digits, digits = new std::uint32_t[capacity = (std::numeric_limits<unsignedIntegral>::digits10 + 7) >> 3];
        while (digits[length++] = std::uint32_t(value) % Base, value /= unsignedIntegral(Base));
        return *this;
    }

    template <typename signedIntegral, typename std::enable_if<std::is_signed<signedIntegral>::value && !std::is_floating_point<signedIntegral>::value>::type* = nullptr>
    UnsignedInteger& operator=(signedIntegral value) {
        VALIDITY_CHECK(value >= 0, std::invalid_argument, "UnsignedInteger operator= error: the provided signed integer value = " + std::to_string(value) + " is negative. UnsignedInteger can only represent non-negative integers.")
        if (length = 0, capacity < (std::numeric_limits<signedIntegral>::digits10 + 7) >> 3)
            delete[] digits, digits = new std::uint32_t[capacity = (std::numeric_limits<signedIntegral>::digits10 + 7) >> 3];
        while (digits[length++] = std::uint32_t(value) % Base, value /= signedIntegral(Base));
        return *this;
    }

    template <typename floatingPoint, typename std::enable_if<std::is_floating_point<floatingPoint>::value>::type* = nullptr>
    UnsignedInteger& operator=(floatingPoint value) {
        VALIDITY_CHECK(value >= 0, std::invalid_argument, "UnsignedInteger operator= error: the provided floating point value = " + std::to_string(value) + " is negative. UnsignedInteger can only represent non-negative integers.")
        VALIDITY_CHECK(std::isfinite(value), std::invalid_argument, "UnsignedInteger operator= error: the provided floating point value = " + std::to_string(value) + " is not finite.")
        if (length = 0, capacity < (std::numeric_limits<floatingPoint>::max_exponent10 + 7) >> 3)
            delete[] digits, digits = new std::uint32_t[capacity = (std::numeric_limits<floatingPoint>::max_exponent10 + 7) >> 3];
        while (digits[length++] = std::uint32_t(std::fmod(value, Base)), (value = std::floor(value / Base)));
        return *this;
    }

    UnsignedInteger& operator=(const char* value) {
        VALIDITY_CHECK(value, std::invalid_argument, "UnsignedInteger operator= error: the provided C-style string is a null pointer.")
        const std::uint32_t stringLength = std::uint32_t(std::strlen(value));
        VALIDITY_CHECK(stringLength, std::invalid_argument, "UnsignedInteger operator= error: the provided C-style string is empty. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        VALIDITY_CHECK(std::all_of(value, value + stringLength, [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "UnsignedInteger operator= error: the provided C-style string value = "
                                                                                                                                                + std::string(value) +
                                                                                                                                                " contains non-digit characters. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        if (capacity < (length = (stringLength + 7) >> 3))
            delete[] digits, digits = new std::uint32_t[capacity = length];
        return construct(value, stringLength), *this;
    }

    UnsignedInteger& operator=(const std::string& value) {
        VALIDITY_CHECK(value.size(), std::invalid_argument, "UnsignedInteger operator= error: the provided string is empty. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        VALIDITY_CHECK(std::all_of(value.begin(), value.end(), [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "UnsignedInteger operator= error: the provided string value = "
                                                                                                                                               + value +
                                                                                                                                               " contains non-digit characters. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        if (capacity < (length = std::uint32_t(value.size() + 7) >> 3))
            delete[] digits, digits = new std::uint32_t[capacity = length];
        return construct(value.data(), std::uint32_t(value.size())), *this;
    }

    friend std::istream& operator>>(std::istream& stream, UnsignedInteger& destination) {
        std::string buffer;
        return stream >> buffer, destination = buffer, stream;
    }

    friend std::ostream& operator<<(std::ostream& stream, const UnsignedInteger& source) {
        return stream << static_cast<const char*>(source);
    }

    template <typename unsignedIntegral, typename std::enable_if<std::is_unsigned<unsignedIntegral>::value>::type* = nullptr>
    operator unsignedIntegral() const {
        unsignedIntegral result = 0;
        for (std::uint32_t* i = digits + length; i != digits; result = result * unsignedIntegral(Base) + unsignedIntegral(*--i));
        return result;
    }

    template <typename signedIntegral, typename std::enable_if<std::is_signed<signedIntegral>::value && !std::is_floating_point<signedIntegral>::value>::type* = nullptr>
    operator signedIntegral() const {
        using UnsignedT = typename std::make_unsigned<signedIntegral>::type;
        UnsignedT result = 0;
        for (std::uint32_t* i = digits + length; i != digits; result = result * UnsignedT(Base) + UnsignedT(*--i));
        return static_cast<signedIntegral>(result);
    }

    template <typename floatingPoint, typename std::enable_if<std::is_floating_point<floatingPoint>::value>::type* = nullptr>
    operator floatingPoint() const {
        floatingPoint result = 0;
        for (std::uint32_t* i = digits + length; i != digits; result = result * floatingPoint(Base) + floatingPoint(*--i));
        return result;
    }

    explicit operator const char*() const {
        thread_local char* result = nullptr;
        thread_local std::uint32_t resultLength = 0;
        if (!length) {
            if (resultLength < 2)
                delete[] result, result = new char[resultLength = 2];
            result[0] = '0';
            result[1] = 0;
            return result;
        }
        if (resultLength < (length << 3 | 7))
            delete[] result, result = new char[resultLength = length << 3 | 7];
        char* resultPointer = result + 8;
        std::uint32_t *i = digits + length, numberLength = 0;
        for (std::uint32_t number = *--i; *--resultPointer = char(48 | number % 10), ++numberLength, number /= 10;);
        for (std::memmove(result, resultPointer, numberLength), resultPointer = result + numberLength; i-- != digits; std::memcpy(resultPointer, detail::O(*i / 10000), 4), resultPointer += 4, std::memcpy(resultPointer, detail::O(*i % 10000), 4), resultPointer += 4);
        return *resultPointer = 0, result;
    }

    operator std::string() const {
        if (!length)
            return "0";
        std::uint32_t* i = digits + length;
        std::string result = std::to_string(*--i);
        for (result.reserve(length << 3); i-- != digits; result.append(detail::O(*i / 10000), 4), result.append(detail::O(*i % 10000), 4));
        return result;
    }

    operator bool() const noexcept {
        return length != 1 || *digits;
    }

#if __cplusplus >= 202002L
    std::strong_ordering operator<=>(const UnsignedInteger& other) const {
        return length == other.length ? reverseCompare(other) <=> 0 : length <=> other.length;
    }
#endif

    bool operator==(const UnsignedInteger& other) const {
        return length == other.length && !std::memcmp(digits, other.digits, length << 2);
    }

    bool operator!=(const UnsignedInteger& other) const {
        return length != other.length || std::memcmp(digits, other.digits, length << 2);
    }

    bool operator<(const UnsignedInteger& other) const {
        return length < other.length || (length == other.length && reverseCompare(other) < 0);
    }

    bool operator>(const UnsignedInteger& other) const {
        return length > other.length || (length == other.length && reverseCompare(other) > 0);
    }

    bool operator<=(const UnsignedInteger& other) const {
        return length < other.length || (length == other.length && reverseCompare(other) <= 0);
    }

    bool operator>=(const UnsignedInteger& other) const {
        return length > other.length || (length == other.length && reverseCompare(other) >= 0);
    }

    UnsignedInteger& operator+=(const UnsignedInteger& other) {
        if (length <= other.length) {
            const std::uint32_t oldLength = length;
            resize(other.length + 1), std::memset(digits + oldLength, 0, (length - oldLength) << 2);
        }
        std::uint32_t *thisDigit = digits, *thisEnd = digits + length - 1;
        for (std::uint32_t *otherDigit = other.digits, *otherEnd = other.digits + other.length; otherDigit != otherEnd; ++thisDigit, ++otherDigit)
            if ((*thisDigit += *otherDigit) >= Base)
                *thisDigit -= Base, ++*(thisDigit + 1);
        for (; thisDigit != thisEnd && *thisDigit >= Base; *thisDigit -= Base, ++*++thisDigit);
        for (; length > 1 && !digits[length - 1]; --length);
        return *this;
    }

    UnsignedInteger operator+(const UnsignedInteger& other) const {
        return UnsignedInteger(*this) += other;
    }

    UnsignedInteger& operator++() {
        std::uint32_t *thisDigit = digits, *thisEnd = digits + length - 1;
        for (++*thisDigit; thisDigit != thisEnd && *thisDigit >= Base; *thisDigit -= Base, ++*++thisDigit);
        if (thisDigit == thisEnd && *thisDigit >= Base)
            resize(length + 1), digits[length - 2] -= Base, digits[length - 1] = 1;
        return *this;
    }

    UnsignedInteger operator++(int) {
        UnsignedInteger result = *this;
        return ++*this, result;
    }

    UnsignedInteger& operator-=(const UnsignedInteger& other) {
        VALIDITY_CHECK(
            *this >= other,
            std::invalid_argument,
            std::string("UnsignedInteger subtraction error: attempted to subtract a larger UnsignedInteger ") +
                other.operator std::string() +
                " from a smaller one " +
                this->operator std::string() +
                ".")
        std::uint32_t *thisDigit = digits, *thisEnd = digits + length;
        for (std::uint32_t *otherDigit = other.digits, *otherEnd = other.digits + other.length; otherDigit != otherEnd; ++thisDigit, ++otherDigit)
            if ((*thisDigit -= *otherDigit) >= Base)
                *thisDigit += Base, --*(thisDigit + 1);
        for (; thisDigit != thisEnd && *thisDigit >= Base; *thisDigit += Base, --*++thisDigit);
        for (; length > 1 && !digits[length - 1]; --length);
        return *this;
    }

    UnsignedInteger operator-(const UnsignedInteger& other) const {
        return UnsignedInteger(*this) -= other;
    }

    UnsignedInteger& operator--() {
        VALIDITY_CHECK(bool(*this), std::invalid_argument, "UnsignedInteger decrement error: value is already zero.")
        std::uint32_t *thisDigit = digits, *thisEnd = digits + length - 1;
        for (--*thisDigit; thisDigit != thisEnd && *thisDigit >= Base; *thisDigit += Base, --*++thisDigit);
        return *this;
    }

    UnsignedInteger operator--(int) {
        UnsignedInteger result = *this;
        return --*this, result;
    }

    UnsignedInteger& operator*=(const UnsignedInteger& other) {
        if (length < BruteforceThreshold || other.length < BruteforceThreshold) {
            UnsignedInteger result(length + other.length - 1, length + other.length);
            std::uint64_t carry = 0;
            for (std::uint32_t i = 0; i != result.length; result.digits[i++] = std::uint32_t(carry % Base), carry /= Base)
                for (std::uint32_t j = (i >= length ? i - length + 1 : 0); j <= i && j < other.length; ++j)
                    carry += std::uint64_t(digits[i - j]) * other.digits[j];
            if (carry)
                result.digits[result.length] = std::uint32_t(carry), ++result.length;
            for (; result.length > 1 && !result.digits[result.length - 1]; --result.length);
            return *this = std::move(result);
        }
        VALIDITY_CHECK(length <= TransformLimit, std::invalid_argument, "UnsignedInteger multiplication error: left operand length (" + std::to_string(length) + ") exceeds Transform limit (" + std::to_string(TransformLimit) + ").")
        VALIDITY_CHECK(other.length <= TransformLimit, std::invalid_argument, "UnsignedInteger multiplication error: right operand length (" + std::to_string(other.length) + ") exceeds Transform limit (" + std::to_string(TransformLimit) + ").")
#if defined(__AVX2__) || defined(__ARM_NEON__)
        const std::uint32_t resultLength = length + other.length, transformLength = 2u << detail::log2(resultLength - 1);
#if defined(__AVX2__)
        thread_local __m128d *firstArray = nullptr, *secondArray = nullptr;
        thread_local std::uint32_t allocatedSize = 0;
        if (allocatedSize < transformLength)
            delete[] firstArray, delete[] secondArray, firstArray = new __m128d[transformLength](), secondArray = new __m128d[transformLength](), allocatedSize = transformLength;
        std::memset(firstArray, 0, transformLength * sizeof(__m128d)), std::memset(secondArray, 0, transformLength * sizeof(__m128d));
        for (std::uint32_t i = 0; i != length; ++i)
            firstArray[i] = _mm_set_pd(std::floor(static_cast<double>(digits[i]) / 10000.0), static_cast<double>(digits[i] % 10000u));
        for (std::uint32_t i = 0; i != other.length; ++i)
            secondArray[i] = _mm_set_pd(std::floor(static_cast<double>(other.digits[i]) / 10000.0), static_cast<double>(other.digits[i] % 10000u));
        detail::T.resize(transformLength), detail::T.decimationInFrequency(firstArray, transformLength), detail::T.decimationInFrequency(secondArray, transformLength);
        detail::T.frequencyDomainPointwiseMultiply(firstArray, secondArray, transformLength), detail::T.decimationInTime(firstArray, transformLength);
#else
        thread_local float64x2_t *firstArray = nullptr, *secondArray = nullptr;
        thread_local std::uint32_t allocatedSize = 0;
        if (allocatedSize < transformLength)
            delete[] firstArray, delete[] secondArray, firstArray = new float64x2_t[transformLength](), secondArray = new float64x2_t[transformLength](), allocatedSize = transformLength;
        std::memset(firstArray, 0, transformLength * sizeof(float64x2_t)), std::memset(secondArray, 0, transformLength * sizeof(float64x2_t));
        for (std::uint32_t i = 0; i != length; ++i) {
            float64x2_t v = vdupq_n_f64(0.0);
            v = vsetq_lane_f64(static_cast<double>(digits[i] % 10000u), v, 0);
            v = vsetq_lane_f64(std::floor(static_cast<double>(digits[i]) / 10000.0), v, 1);
            firstArray[i] = v;
        }
        for (std::uint32_t i = 0; i != other.length; ++i) {
            float64x2_t v = vdupq_n_f64(0.0);
            v = vsetq_lane_f64(static_cast<double>(other.digits[i] % 10000u), v, 0);
            v = vsetq_lane_f64(std::floor(static_cast<double>(other.digits[i]) / 10000.0), v, 1);
            secondArray[i] = v;
        }
        detail::T.resize(transformLength), detail::T.decimationInFrequency(firstArray, transformLength), detail::T.decimationInFrequency(secondArray, transformLength);
        detail::T.frequencyDomainPointwiseMultiply(firstArray, secondArray, transformLength), detail::T.decimationInTime(firstArray, transformLength);
#endif
        UnsignedInteger result(resultLength, resultLength);
        std::uint64_t carry = 0;
#if defined(__AVX2__)
        for (std::uint32_t i = 0; i != resultLength; ++i) {
            __m128d v = firstArray[i];
            double realPart = _mm_cvtsd_f64(v);
            double imagPart = _mm_cvtsd_f64(_mm_unpackhi_pd(v, v));
            carry += std::uint64_t(std::int64_t(realPart + 0.5) + std::int64_t(imagPart + 0.5) * 10000);
            result.digits[i] = std::uint32_t(carry % Base), carry /= Base;
        }
#else
        for (std::uint32_t i = 0; i != resultLength; ++i) {
            float64x2_t v = firstArray[i];
            double realPart = vgetq_lane_f64(v, 0);
            double imagPart = vgetq_lane_f64(v, 1);
            carry += std::uint64_t(std::int64_t(realPart + 0.5) + std::int64_t(imagPart + 0.5) * 10000);
            result.digits[i] = std::uint32_t(carry % Base), carry /= Base;
        }
#endif
#else
        thread_local std::complex<double>*firstArray = nullptr, *secondArray = nullptr;
        thread_local std::uint32_t allocatedSize = 0;
        const std::uint32_t resultLength = length + other.length, transformLength = 2u << detail::log2(resultLength - 1);
        if (allocatedSize < transformLength)
            delete[] firstArray, delete[] secondArray, firstArray = new std::complex<double>[transformLength](), secondArray = new std::complex<double>[transformLength](), allocatedSize = transformLength;
        std::memset(firstArray, 0, transformLength * sizeof(std::complex<double>)), std::memset(secondArray, 0, transformLength * sizeof(std::complex<double>));
        for (std::uint32_t i = 0; i != length; ++i)
            firstArray[i] = {static_cast<double>(digits[i] % 10000u), std::floor(static_cast<double>(digits[i]) / 10000.0)};
        for (std::uint32_t i = 0; i != other.length; ++i)
            secondArray[i] = {static_cast<double>(other.digits[i] % 10000u), std::floor(static_cast<double>(other.digits[i]) / 10000.0)};
        detail::T.resize(transformLength), detail::T.decimationInFrequency(firstArray, transformLength), detail::T.decimationInFrequency(secondArray, transformLength);
        detail::T.frequencyDomainPointwiseMultiply(firstArray, secondArray, transformLength), detail::T.decimationInTime(firstArray, transformLength);
        UnsignedInteger result(resultLength, resultLength);
        std::uint64_t carry = 0;
        for (std::uint32_t i = 0; i != resultLength; ++i) {
            carry += std::uint64_t(std::int64_t(firstArray[i].real() + 0.5) + std::int64_t(firstArray[i].imag() + 0.5) * 10000);
            result.digits[i] = std::uint32_t(carry % Base), carry /= Base;
        }
#endif
        for (; carry && result.length != result.capacity; result.digits[result.length++] = std::uint32_t(carry % Base), carry /= Base)
            if (result.length == result.capacity)
                result.resize(result.capacity << 1);
        for (; result.length > 1 && !result.digits[result.length - 1]; --result.length);
        return *this = std::move(result);
    }

    UnsignedInteger operator*(const UnsignedInteger& other) const {
        return UnsignedInteger(*this) *= other;
    }

    UnsignedInteger& operator/=(const UnsignedInteger& other) {
        VALIDITY_CHECK(bool(other), std::invalid_argument, "UnsignedInteger division error: divisor is zero.")
        return *this = std::move(divisionAndModulus(other).first);
    }

    UnsignedInteger operator/(const UnsignedInteger& other) const {
        return UnsignedInteger(*this) /= other;
    }

    UnsignedInteger& operator%=(const UnsignedInteger& other) {
        VALIDITY_CHECK(bool(other), std::invalid_argument, "UnsignedInteger modulus error: modulus is zero.")
        return *this = std::move(divisionAndModulus(other).second);
    }

    UnsignedInteger operator%(const UnsignedInteger& other) const {
        return UnsignedInteger(*this) %= other;
    }
};

inline UnsignedInteger operator""_UI(const char* literal, std::size_t) {
    return UnsignedInteger(literal);
}

class SignedInteger {
    UnsignedInteger absolute;
    bool sign;

  protected:
    SignedInteger(const UnsignedInteger& initialAbsolute, bool initialSign) : absolute(initialAbsolute), sign(initialSign) {}

  public:
    friend class UnsignedInteger;
    SignedInteger() : absolute(), sign() {}

    SignedInteger(const SignedInteger& other) : absolute(other.absolute), sign(other.sign) {}

    SignedInteger(SignedInteger&& other) noexcept : absolute(std::move(other.absolute)), sign(other.sign) {}

    SignedInteger(const UnsignedInteger& other) : absolute(other), sign() {}

    template <typename unsignedIntegral, typename std::enable_if<std::is_unsigned<unsignedIntegral>::value>::type* = nullptr>
    SignedInteger(unsignedIntegral value) : absolute(value), sign() {}

    template <typename signedIntegral, typename std::enable_if<std::is_signed<signedIntegral>::value && !std::is_floating_point<signedIntegral>::value>::type* = nullptr>
    SignedInteger(signedIntegral value) : absolute(std::abs(value)), sign(value < 0) {}

    template <typename floatingPoint, typename std::enable_if<std::is_floating_point<floatingPoint>::value>::type* = nullptr>
    SignedInteger(floatingPoint value) : absolute(std::abs(value)), sign(value < 0) {}

    SignedInteger(const char* value) {
        VALIDITY_CHECK(value, std::invalid_argument, "SignedInteger constructor error: the provided C-style string is a null pointer.")
        VALIDITY_CHECK(*value, std::invalid_argument, "SignedInteger constructor error: the provided C-style string is empty. SignedInteger can only be constructed from non-empty strings containing only digits (optionally prefixed with '-').")
        VALIDITY_CHECK(std::all_of(value + (*value == '-'), value + std::strlen(value), [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "SignedInteger constructor error: the provided C-style string value = "
                                                                                                                                                                        + std::string(value) +
                                                                                                                                                                        " contains non-digit characters. SignedInteger can only be constructed from non-empty strings containing only digits (optionally prefixed with '-').")
        absolute = value + (sign = *value == '-'), sign = sign && bool(absolute);
    }

    SignedInteger(const std::string& value) {
        VALIDITY_CHECK(value.size(), std::invalid_argument, "SignedInteger constructor error: the provided string is empty. SignedInteger can only be constructed from non-empty strings containing only digits (optionally prefixed with '-').")
        VALIDITY_CHECK(std::all_of(value.begin() + (value.front() == '-'), value.end(), [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "SignedInteger constructor error: the provided string value = "
                                                                                                                                                                        + value +
                                                                                                                                                                        " contains non-digit characters. SignedInteger can only be constructed from non-empty strings containing only digits (optionally prefixed with '-').")
        absolute = value.data() + (sign = value.front() == '-'), sign = sign && bool(absolute);
    }

    ~SignedInteger() = default;

    SignedInteger& operator=(const SignedInteger& other) {
        return absolute = other.absolute, sign = other.sign, *this;
    }

    SignedInteger& operator=(SignedInteger&& other) noexcept {
        return absolute = std::move(other.absolute), sign = other.sign, *this;
    }

    SignedInteger& operator=(const UnsignedInteger& other) {
        return absolute = other, sign = false, *this;
    }

    template <typename unsignedIntegral, typename std::enable_if<std::is_unsigned<unsignedIntegral>::value>::type* = nullptr>
    SignedInteger& operator=(unsignedIntegral value) {
        return absolute = value, sign = false, *this;
    }

    template <typename signedIntegral, typename std::enable_if<std::is_signed<signedIntegral>::value && !std::is_floating_point<signedIntegral>::value>::type* = nullptr>
    SignedInteger& operator=(signedIntegral value) {
        return absolute = std::abs(value), sign = value < 0, *this;
    }

    template <typename floatingPoint, typename std::enable_if<std::is_floating_point<floatingPoint>::value>::type* = nullptr>
    SignedInteger& operator=(floatingPoint value) {
        return absolute = std::abs(value), sign = value < 0, *this;
    }

    SignedInteger& operator=(const char* value) {
        VALIDITY_CHECK(value, std::invalid_argument, "SignedInteger operator= error: the provided C-style string is a null pointer.")
        VALIDITY_CHECK(*value, std::invalid_argument, "SignedInteger operator= error: the provided C-style string is empty. SignedInteger can only be assigned from non-empty strings containing only digits (optionally prefixed with '-').")
        VALIDITY_CHECK(std::all_of(value + (*value == '-'), value + std::strlen(value), [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "SignedInteger operator= error: the provided C-style string value = "
                                                                                                                                                                        + std::string(value) +
                                                                                                                                                                        " contains non-digit characters. SignedInteger can only be assigned from non-empty strings containing only digits (optionally prefixed with '-').")
        return absolute = value + (sign = *value == '-'), sign = sign && bool(absolute), *this;
    }

    SignedInteger& operator=(const std::string& value) {
        VALIDITY_CHECK(value.size(), std::invalid_argument, "SignedInteger operator= error: the provided string is empty. SignedInteger can only be assigned from non-empty strings containing only digits (optionally prefixed with '-').")
        VALIDITY_CHECK(std::all_of(value.begin() + (value.front() == '-'), value.end(), [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "SignedInteger operator= error: the provided string value = "
                                                                                                                                                                        + value +
                                                                                                                                                                        " contains non-digit characters. SignedInteger can only be assigned from non-empty strings containing only digits (optionally prefixed with '-').")
        return absolute = value.data() + (sign = value.front() == '-'), sign = sign && bool(absolute), *this;
    }

    friend std::istream& operator>>(std::istream& stream, SignedInteger& destination) {
        std::string buffer;
        return stream >> buffer, destination = buffer, stream;
    }

    friend std::ostream& operator<<(std::ostream& stream, const SignedInteger& source) {
        return stream << static_cast<const char*>(source);
    }

    template <typename unsignedIntegral, typename std::enable_if<std::is_unsigned<unsignedIntegral>::value>::type* = nullptr>
    operator unsignedIntegral() const {
        VALIDITY_CHECK(!sign, std::invalid_argument, "SignedInteger conversion error: Cannot convert negative SignedInteger to unsigned type.")
        return unsignedIntegral(absolute);
    }

    template <typename signedIntegral, typename std::enable_if<std::is_signed<signedIntegral>::value && !std::is_floating_point<signedIntegral>::value>::type* = nullptr>
    operator signedIntegral() const {
        using UnsignedT = typename std::make_unsigned<signedIntegral>::type;
        UnsignedT magnitude = static_cast<UnsignedT>(absolute);
        if (!sign) return static_cast<signedIntegral>(magnitude);
        UnsignedT twosComplement = UnsignedT(0) - magnitude;
        return static_cast<signedIntegral>(twosComplement);
    }

    template <typename floatingPoint, typename std::enable_if<std::is_floating_point<floatingPoint>::value>::type* = nullptr>
    operator floatingPoint() const {
        return sign ? -floatingPoint(absolute) : floatingPoint(absolute);
    }

    explicit operator const char*() const {
        thread_local char* result = nullptr;
        thread_local std::uint32_t resultLength = 0;
        const char* absoluteString = static_cast<const char*>(absolute);
        std::uint32_t absoluteLength = std::uint32_t(std::strlen(absoluteString));
        if (resultLength < absoluteLength + 5)
            delete[] result, result = new char[resultLength = absoluteLength + 5];
        char* digitStart = sign && bool(absolute) ? (*result = '-', result + 1) : result;
        std::memcpy(digitStart, absoluteString, absoluteLength), *(digitStart + absoluteLength) = 0;
        return result;
    }

    operator std::string() const {
        return sign && bool(absolute) ? "-" + absolute.operator std::string() : absolute.operator std::string();
    }

    operator bool() const noexcept {
        return bool(absolute);
    }

#if __cplusplus >= 202002L
    std::strong_ordering operator<=>(const SignedInteger& other) const {
        return sign != other.sign ? other.sign <=> sign : (sign ? other.absolute <=> absolute : absolute <=> other.absolute);
    }
#endif

    bool operator==(const SignedInteger& other) const {
        return sign == other.sign && absolute == other.absolute;
    }

    bool operator!=(const SignedInteger& other) const {
        return sign != other.sign || absolute != other.absolute;
    }

    bool operator<(const SignedInteger& other) const {
        return sign ? !other.sign || absolute > other.absolute : !other.sign && absolute < other.absolute;
    }

    bool operator>(const SignedInteger& other) const {
        return sign ? other.sign && absolute < other.absolute : other.sign || absolute > other.absolute;
    }

    bool operator<=(const SignedInteger& other) const {
        return sign ? !other.sign || absolute >= other.absolute : !other.sign && absolute <= other.absolute;
    }

    bool operator>=(const SignedInteger& other) const {
        return sign ? other.sign && absolute <= other.absolute : other.sign || absolute >= other.absolute;
    }

    SignedInteger& operator+=(const SignedInteger& other) {
        sign == other.sign ? absolute += other.absolute : (absolute < other.absolute ? sign = !sign, absolute = other.absolute - absolute : absolute -= other.absolute), sign = sign && bool(absolute);
        return *this;
    }

    SignedInteger operator+(const SignedInteger& other) const {
        return SignedInteger(*this) += other;
    }

    SignedInteger& operator-=(const SignedInteger& other) {
        sign != other.sign ? absolute += other.absolute : (absolute < other.absolute ? sign = !sign, absolute = other.absolute - absolute : absolute -= other.absolute), sign = sign && bool(absolute);
        return *this;
    }

    SignedInteger operator-(const SignedInteger& other) const {
        return SignedInteger(*this) -= other;
    }

    SignedInteger& operator*=(const SignedInteger& other) {
        absolute *= other.absolute, sign = (sign ^ other.sign) && bool(absolute);
        return *this;
    }

    SignedInteger operator*(const SignedInteger& other) const {
        return SignedInteger(*this) *= other;
    }

    SignedInteger& operator/=(const SignedInteger& other) {
        VALIDITY_CHECK(bool(other), std::invalid_argument, "SignedInteger division error: divisor is zero.")
        absolute /= other.absolute, sign ^= other.sign, sign = sign && bool(absolute);
        return *this;
    }

    SignedInteger operator/(const SignedInteger& other) const {
        return SignedInteger(*this) /= other;
    }

    SignedInteger& operator%=(const SignedInteger& other) {
        VALIDITY_CHECK(bool(other), std::invalid_argument, "SignedInteger modulus error: modulus is zero.")
        *this -= (*this / other) * other;
        return *this;
    }

    SignedInteger operator%(const SignedInteger& other) const {
        return SignedInteger(*this) %= other;
    }
};

inline SignedInteger operator""_SI(const char* literal, std::size_t) {
    return SignedInteger(literal);
}

inline UnsignedInteger::UnsignedInteger(const SignedInteger& other) : UnsignedInteger(other.absolute) {
    VALIDITY_CHECK(!other.sign, std::invalid_argument, "UnsignedInteger constructor error: the provided SignedInteger is negative. UnsignedInteger can only represent non-negative integers.")
}

inline UnsignedInteger& UnsignedInteger::operator=(const SignedInteger& other) {
    VALIDITY_CHECK(!other.sign, std::invalid_argument, "UnsignedInteger operator= error: the provided SignedInteger is negative. UnsignedInteger can only represent non-negative integers.")
    return *this = other.absolute;
}

#undef VALIDITY_CHECK
#undef __CONSTEXPR
#endif
