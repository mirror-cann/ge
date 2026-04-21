/**
* Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CANN_GRAPH_ENGINE_ERFINV_H
#define CANN_GRAPH_ENGINE_ERFINV_H

namespace AscendC {
namespace ErfinvAPI {

constexpr Reg::CastTrait castTraitF162F32 = {
    Reg::RegLayout::ZERO, Reg::SatMode::UNKNOWN, Reg::MaskMergeMode::ZEROING, RoundMode::UNKNOWN};
constexpr Reg::CastTrait castTraitF322F16 = {
    Reg::RegLayout::ZERO, Reg::SatMode::NO_SAT, Reg::MaskMergeMode::ZEROING, RoundMode::CAST_RINT};

// Threshold for selecting approximation method
constexpr float ERFINV_LOG2_THRESHOLD = -8.2f;
// nan
constexpr uint32_t FLOAT_NAN = 0x7fc00000;

#ifndef INFINITY
#define INFINITY (1.0f / 0.0f)
#endif

// Polynomial coefficients for near-±1 approximation (using rsqrtNegLog)
// poly = ((((- 0.5899144 * rsqrtNegLog - 0.6630042) * rsqrtNegLog + 1.5970111) * rsqrtNegLog - 0.67521554) * rsqrtNegLog - 0.09522479) * rsqrtNegLog + 0.83535343
__simd_callee__ inline void ErfinvComputeNearOne(
    Reg::RegTensor<float>& dstReg, Reg::RegTensor<float>& negLog2Reg, Reg::RegTensor<float>& srcReg, Reg::MaskReg& mask)
{
    Reg::RegTensor<float> sqrtNegLogReg, rsqrtNegLogReg, oneReg, zeroReg, denominatorReg, finalTerm, absFinalReg, negFinalReg;
    Reg::MaskReg signBitReg;

    Reg::Sqrt(sqrtNegLogReg, negLog2Reg, mask);
    Reg::Duplicate(oneReg, 1.0f, mask);
    Reg::Div(rsqrtNegLogReg, oneReg, sqrtNegLogReg, mask);

    constexpr float COEF0 = -0.5899144f;
    constexpr float COEF1 = -0.6630042f;
    constexpr float COEF2 = 1.5970111f;
    constexpr float COEF3 = -0.67521554f;
    constexpr float COEF4 = -0.09522479f;
    constexpr float COEF5 = 0.83535343f;

    Reg::RegTensor<float> tmpReg;
    Reg::Muls(tmpReg, rsqrtNegLogReg, COEF0, mask);
    Reg::Adds(tmpReg, tmpReg, COEF1, mask);
    Reg::Mul(tmpReg, tmpReg, rsqrtNegLogReg, mask);
    Reg::Adds(tmpReg, tmpReg, COEF2, mask);
    Reg::Mul(tmpReg, tmpReg, rsqrtNegLogReg, mask);
    Reg::Adds(tmpReg, tmpReg, COEF3, mask);
    Reg::Mul(tmpReg, tmpReg, rsqrtNegLogReg, mask);
    Reg::Adds(tmpReg, tmpReg, COEF4, mask);
    Reg::Mul(tmpReg, tmpReg, rsqrtNegLogReg, mask);
    Reg::Adds(tmpReg, tmpReg, COEF5, mask);

    Reg::Div(denominatorReg, oneReg, rsqrtNegLogReg, mask);
    // finalTerm = tmpReg / rsqrtNegLog
    Reg::Mul(finalTerm, denominatorReg, tmpReg, mask);

    // Extract sign bit from original x
    Reg::Duplicate(zeroReg, 0.0f, mask);
    Reg::Compare<float, CMPMODE::LT>(signBitReg, srcReg, zeroReg, mask);
    Reg::Abs(absFinalReg, finalTerm, mask);
    Reg::Neg(negFinalReg, absFinalReg, signBitReg);
    // Apply sign bit to near-±1 result
    Reg::Select(dstReg, negFinalReg, absFinalReg, signBitReg);
}

// Polynomial coefficients for middle range approximation (using negLog2)
// poly = (((((((-2.5172708e-10 * negLog2 + 9.427429e-9) * negLog2 - 1.2054752e-7) * negLog2 + 2.1697005e-7) * negLog2 + 8.0621484e-6) * negLog2 - 3.1675492e-5) * negLog2 - 0.0007743631) * negLog2 + 0.005546588) * negLog2 + 0.16082023) * negLog2 + 0.8862269
__simd_callee__ inline void ErfinvComputeMiddle(
    Reg::RegTensor<float>& dstReg, Reg::RegTensor<float>& negLog2Reg, Reg::RegTensor<float>& srcReg, Reg::MaskReg& mask)
{
    constexpr float COEF0 = -2.5172708e-10f;
    constexpr float COEF1 = 9.427429e-9f;
    constexpr float COEF2 = -1.2054752e-7f;
    constexpr float COEF3 = 2.1697005e-7f;
    constexpr float COEF4 = 0.0000080621484f;
    constexpr float COEF5 = -0.000031675492f;
    constexpr float COEF6 = -0.0007743631f;
    constexpr float COEF7 = 0.005546588f;
    constexpr float COEF8 = 0.16082023f;
    constexpr float COEF9 = 0.8862269f;

    Reg::RegTensor<float> tmpReg;
    Reg::Muls(tmpReg, negLog2Reg, COEF0, mask);
    Reg::Adds(tmpReg, tmpReg, COEF1, mask);
    Reg::Mul(tmpReg, tmpReg, negLog2Reg, mask);
    Reg::Adds(tmpReg, tmpReg, COEF2, mask);
    Reg::Mul(tmpReg, tmpReg, negLog2Reg, mask);
    Reg::Adds(tmpReg, tmpReg, COEF3, mask);
    Reg::Mul(tmpReg, tmpReg, negLog2Reg, mask);
    Reg::Adds(tmpReg, tmpReg, COEF4, mask);
    Reg::Mul(tmpReg, tmpReg, negLog2Reg, mask);
    Reg::Adds(tmpReg, tmpReg, COEF5, mask);
    Reg::Mul(tmpReg, tmpReg, negLog2Reg, mask);
    Reg::Adds(tmpReg, tmpReg, COEF6, mask);
    Reg::Mul(tmpReg, tmpReg, negLog2Reg, mask);
    Reg::Adds(tmpReg, tmpReg, COEF7, mask);
    Reg::Mul(tmpReg, tmpReg, negLog2Reg, mask);
    Reg::Adds(tmpReg, tmpReg, COEF8, mask);
    Reg::Mul(tmpReg, tmpReg, negLog2Reg, mask);
    Reg::Adds(tmpReg, tmpReg, COEF9, mask);

    Reg::Mul(dstReg, tmpReg, srcReg, mask);
}

// Handle special cases: NaN, Inf, |x| > 1, |x| == 1
__simd_callee__ inline void ErfinvHandleSpecialCases(
    Reg::RegTensor<float>& dstReg, Reg::RegTensor<float>& srcReg, Reg::MaskReg& mask)
{
    Reg::RegTensor<float> absSrcReg, oneReg, negOneReg, nanReg, infReg, negInfReg;
    Reg::MaskReg isNanMask, isPosInfMask, isNegInfMask, isGtOneMask, isEqOneMask, isEqNegOneMask, specialCaseMask;

    Reg::Duplicate(oneReg, 1.0f, mask);
    Reg::Neg(negOneReg, oneReg, mask);
    Reg::Duplicate(infReg, INFINITY, mask);
    Reg::Neg(negInfReg, infReg, mask);
    Reg::Duplicate(nanReg, (float&)FLOAT_NAN, mask);

    Reg::Abs(absSrcReg, srcReg, mask);

    Reg::Compare<float, CMPMODE::NE>(isNanMask, srcReg, srcReg, mask);
    Reg::Compare<float, CMPMODE::EQ>(isPosInfMask, srcReg, infReg, mask);
    Reg::Compare<float, CMPMODE::EQ>(isNegInfMask, srcReg, negInfReg, mask);
    Reg::Compare<float, CMPMODE::GT>(isGtOneMask, absSrcReg, oneReg, mask);
    Reg::Compare<float, CMPMODE::EQ>(isEqOneMask, srcReg, oneReg, mask);
    Reg::Compare<float, CMPMODE::EQ>(isEqNegOneMask, srcReg, negOneReg, mask);

    // Combine masks for special cases
    Reg::Or(specialCaseMask, isNanMask, isPosInfMask, mask);
    Reg::Or(specialCaseMask, specialCaseMask, isNegInfMask, mask);
    Reg::Or(specialCaseMask, specialCaseMask, isGtOneMask, mask);

    Reg::Select(dstReg, nanReg, dstReg, specialCaseMask);
    Reg::Select(dstReg, infReg, dstReg, isEqOneMask);
    Reg::Select(dstReg, negInfReg, dstReg, isEqNegOneMask);

    // Update mask to exclude processed special cases
    Reg::MaskReg excludeMask;
    Reg::Or(excludeMask, specialCaseMask, isEqOneMask, mask);
    Reg::Or(excludeMask, excludeMask, isEqNegOneMask, mask);
    Reg::Not(excludeMask, excludeMask, mask);
    Reg::And(mask, mask, excludeMask, mask);
}

// Core computation for erfinv
__simd_callee__ inline void ErfinvCoreCompute(
    Reg::RegTensor<float>& dstReg, Reg::RegTensor<float>& srcReg, Reg::MaskReg& mask)
{
    Reg::MaskReg copyMask;
    Reg::Move(copyMask, mask);
    // solve special cases
    ErfinvHandleSpecialCases(dstReg, srcReg, copyMask);

    Reg::RegTensor<float> oppositeXReg, temp1Reg, log2TempReg, negLog2Reg;
    Reg::RegTensor<float> sqrtNegLogReg, rsqrtNegLogReg, nearOneResult, middleResult, tempDstReg;
    Reg::RegTensor<uint32_t> signBitReg, resultBitsReg;
    Reg::MaskReg nearOneMask, middleMask;

    // oppositeX = -x
    Reg::Neg(oppositeXReg, srcReg, copyMask);

    // temp1 = 1 - x^2
    Reg::Mul(temp1Reg, srcReg, oppositeXReg, copyMask);
    Reg::Adds(temp1Reg, temp1Reg, 1.0f, copyMask);

    // log2Temp = log2(temp1)
    Reg::Log2(log2TempReg, temp1Reg, copyMask);

    // negLog2 = -log2Temp
    Reg::Neg(negLog2Reg, log2TempReg, copyMask);

    // Check if log2Temp < -8.2f (near ±1 case)
    Reg::Duplicate(oppositeXReg, ERFINV_LOG2_THRESHOLD, copyMask);
    Reg::Compare<float, CMPMODE::LT>(nearOneMask, log2TempReg, oppositeXReg, copyMask);
    Reg::Compare<float, CMPMODE::GE>(middleMask, log2TempReg, oppositeXReg, copyMask);

    // Compute near-±1 approximation
    ErfinvComputeNearOne(nearOneResult, negLog2Reg, srcReg, nearOneMask);

    // Compute middle range approximation
    ErfinvComputeMiddle(middleResult, negLog2Reg, srcReg, middleMask);

    // Select result based on threshold
    Reg::Select(tempDstReg, nearOneResult, middleResult, nearOneMask);
    Reg::Select(dstReg, tempDstReg, dstReg, copyMask);
}

template <typename T, bool isReuseSource = false>
__simd_vf__ inline void ErfinvCoreImpl(__ubuf__ T* dstUb, __ubuf__ T* srcUb, uint32_t calCount, uint16_t repeatTimes)
{
    Reg::MaskReg mask;
    Reg::RegTensor<T> srcReg;
    Reg::RegTensor<float> castReg;
    Reg::RegTensor<float> dstReg;

    for (uint16_t i = 0; i < repeatTimes; ++i) {
        mask = Reg::UpdateMask<float>(calCount);
        if constexpr (sizeof(T) == sizeof(half)) {
            Reg::LoadAlign<T, Reg::LoadDist::DIST_UNPACK_B16>(srcReg, srcUb + i * B32_DATA_NUM_PER_REPEAT);
            Reg::Cast<float, T, castTraitF162F32>(castReg, srcReg, mask);
        } else {
            Reg::LoadAlign(castReg, srcUb + i * B32_DATA_NUM_PER_REPEAT);
        }

        ErfinvCoreCompute(dstReg, castReg, mask);

        if constexpr (sizeof(T) == sizeof(half)) {
            Reg::Cast<T, float, castTraitF322F16>(srcReg, dstReg, mask);
            Reg::StoreAlign<T, Reg::StoreDist::DIST_PACK_B32>(dstUb + i * B32_DATA_NUM_PER_REPEAT, srcReg, mask);
        } else {
            Reg::StoreAlign(dstUb + i * B32_DATA_NUM_PER_REPEAT, dstReg, mask);
        }
    }
}

} // namespace ErfinvAPI

template <typename T, bool isReuseSource = false>
__aicore__ inline void ErfinvCheckParams(
    const LocalTensor<T>& dstTensor, const LocalTensor<T>& srcTensor, const LocalTensor<uint8_t>& sharedTmpBuffer,
    const uint32_t calCount)
{
    static_assert(SupportType<T, half, float>(), "current data type is not supported on current device!");
    CheckTensorPos<T>(dstTensor, Hardware::UB, "dstTensor", "VECIN / VECCALC / VECOUT", "Erfinv");
    CheckTensorPos<T>(srcTensor, Hardware::UB, "srcTensor", "VECIN / VECCALC / VECOUT", "Erfinv");
    CheckTensorPos<uint8_t>(sharedTmpBuffer, Hardware::UB, "sharedTmpBuffer", "VECIN / VECCALC / VECOUT", "Erfinv");
    CheckCalCount(calCount, "calCount", srcTensor, "srcTensor", "Erfinv");
    CheckCalCount(calCount, "calCount", dstTensor, "dstTensor", "Erfinv");
}

template <typename T, bool isReuseSource = false>
__aicore__ inline void Erfinv(
    const LocalTensor<T>& dstTensor, const LocalTensor<T>& srcTensor, const LocalTensor<uint8_t>& sharedTmpBuffer,
    const uint32_t calCount)
{
    // Only for AI Vector Core.
    if ASCEND_IS_AIC {
        return;
    }

    ErfinvCheckParams<T, isReuseSource>(dstTensor, srcTensor, sharedTmpBuffer, calCount);
    __ubuf__ T* dstUb = (__ubuf__ T*)dstTensor.GetPhyAddr();
    __ubuf__ T* srcUb = (__ubuf__ T*)srcTensor.GetPhyAddr();
    uint16_t repeatTimes = CeilDivision(calCount, B32_DATA_NUM_PER_REPEAT);
    ErfinvAPI::ErfinvCoreImpl<T, isReuseSource>(dstUb, srcUb, calCount, repeatTimes);
}

template <typename T, bool isReuseSource = false>
__aicore__ inline void Erfinv(
    const LocalTensor<T>& dstTensor, const LocalTensor<T>& srcTensor, const uint32_t calCount)
{
    // Only for AI Vector Core.
    if ASCEND_IS_AIC {
        return;
    }

    // Using Stack Space to Allocate tmpBuffer
    LocalTensor<uint8_t> sharedTmpBuffer;
    bool ans = PopStackBuffer<uint8_t, TPosition::LCM>(sharedTmpBuffer);
    ASCENDC_ASSERT((ans), { KERNEL_LOG(KERNEL_ERROR, "PopStackBuffer Error!"); });
    Erfinv<T, isReuseSource>(dstTensor, srcTensor, sharedTmpBuffer, calCount);
}

} // namespace AscendC

#endif //CANN_GRAPH_ENGINE_ERFINV_H