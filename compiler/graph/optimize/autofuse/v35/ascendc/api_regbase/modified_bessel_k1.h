/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __ASCENDC_API_REGBASE_MODIFIED_BESSEL_K1_H__
#define __ASCENDC_API_REGBASE_MODIFIED_BESSEL_K1_H__

#include "modified_bessel_i1.h"

constexpr uint32_t MODIFIED_BESSEL_K1_FLOAT_NAN = 0x7fc00000;

#ifndef INFINITY
#define INFINITY (1.0f / 0.0f)
#endif

constexpr float K1_A[11] = {
    -7.02386347938628759343e-18,
    -2.42744985051936593393e-15,
    -6.66690169419932900609e-13,
    -1.41148839263352776110e-10,
    -2.21338763073472585583e-08,
    -2.43340614156596823496e-06,
    -1.73028895751305206302e-04,
    -6.97572385963986435018e-03,
    -1.22611180822657148235e-01,
    -3.53155960776544875667e-01,
    +1.52530022733894777053e+00,
};

constexpr float K1_B[25] = {
    -5.75674448366501715755e-18, +1.79405087314755922667e-17,
    -5.68946255844285935196e-17, +1.83809354436663880070e-16,
    -6.05704724837331885336e-16, +2.03870316562433424052e-15,
    -7.01983709041831346144e-15, +2.47715442448130437068e-14,
    -8.97670518232499435011e-14, +3.34841966607842919884e-13,
    -1.28917396095102890680e-12, +5.13963967348173025100e-12,
    -2.12996783842756842877e-11, +9.21831518760500529508e-11,
    -4.19035475934189648750e-10, +2.01504975519703286596e-09,
    -1.03457624656780970260e-08, +5.74108412545004946722e-08,
    -3.50196060308781257119e-07, +2.40648494783721712015e-06,
    -1.93619797416608296024e-05, +1.95215518471351631108e-04,
    -2.85781685962277938680e-03, +1.03923736576817238437e-01,
    +2.72062619048444266945e+00,
};

template <typename T>
__aicore__ inline void ModifiedBesselK1ImplVF(__ubuf__ T* dst, __ubuf__ T* src, uint32_t calCount) {
    uint32_t vlSize = static_cast<uint32_t>(GetVecLen() / sizeof(T));
    uint16_t repeatTime = static_cast<uint16_t>(AscendC::CeilDivision(calCount, vlSize));

    AscendC::Reg::RegTensor<T> srcReg, absXReg, xFactorReg, constReg, iterReg;
    AscendC::Reg::RegTensor<T> pReg, qReg, smallDstReg, bigDstReg, dstReg, nanReg, infReg;
    AscendC::Reg::RegTensor<T> i1Reg, factorReg;
    AscendC::Reg::MaskReg mask, branchMask;

    for (uint16_t i = 0U; i < repeatTime; ++i) {
        mask = AscendC::Reg::UpdateMask<T>(calCount);
        AscendC::Reg::LoadAlign(srcReg, src + i * vlSize);

        // ===== 0 < x <= 2.0 branch =====
        AscendC::Reg::Compares<T, CMPMODE::GT>(branchMask, srcReg, (T)0.0, mask);
        AscendC::Reg::Compares<T, CMPMODE::LE>(branchMask, srcReg, (T)2.0, branchMask);

        AscendC::Reg::Duplicate(pReg, (T)0.0, mask);
        AscendC::Reg::Duplicate(qReg, (T)0.0, mask);
        // x_factor = x*x - 2
        AscendC::Reg::Mul(xFactorReg, srcReg, srcReg, branchMask);
        AscendC::Reg::Adds(xFactorReg, xFactorReg, (T)(-2.0), branchMask);

        AscendC::Reg::Duplicate(constReg, K1_A[0], branchMask);
        mainIter<T, 11, 11, K1_A>(pReg, qReg, constReg, xFactorReg, branchMask);

        // result_small = 0.5 * (a - p) - log(0.5 * x) * I0(x)
        AscendC::Reg::Sub(smallDstReg, constReg, pReg, branchMask);
        AscendC::Reg::Muls(smallDstReg, smallDstReg, (T)0.5, branchMask);
        AscendC::Reg::Div(smallDstReg, smallDstReg, srcReg, branchMask);
        
        // calculate modified_bessel_I0(x)
        AscendC::Reg::Duplicate(pReg, (T)0.0, branchMask);
        AscendC::Reg::Duplicate(qReg, (T)0.0, branchMask);
        AscendC::Reg::Abs(absXReg, srcReg, branchMask);
        AscendC::Reg::Muls(xFactorReg, absXReg, (T)0.5, branchMask);
        AscendC::Reg::Adds(xFactorReg, xFactorReg, (T)(-2.0), branchMask);

        AscendC::Reg::Duplicate(constReg, I1_A[0], branchMask);
        mainIter<T, 29, 29, I1_A>(pReg, qReg, constReg, xFactorReg, branchMask);

        AscendC::Reg::Exp(factorReg, absXReg, branchMask);
        AscendC::Reg::Sub(i1Reg, constReg, pReg, branchMask);
        AscendC::Reg::Muls(i1Reg, i1Reg, (T)0.5, branchMask);
        AscendC::Reg::Mul(i1Reg, i1Reg, factorReg, branchMask);
        AscendC::Reg::Mul(i1Reg, i1Reg, srcReg, branchMask);

        // get result
        AscendC::Reg::Muls(factorReg, srcReg, (T)0.5, branchMask);
        AscendC::Reg::Log(factorReg, factorReg, branchMask);
        AscendC::Reg::Mul(factorReg, factorReg, i1Reg, branchMask);
        AscendC::Reg::Add(smallDstReg, smallDstReg, factorReg, branchMask);

        // // ===== Large branch: x > 2.0 =====
        AscendC::Reg::Compares<T, CMPMODE::GT>(branchMask, srcReg, (T)2.0, mask);

        // x_factor = 8/x - 2
        AscendC::Reg::Duplicate(xFactorReg, (T)8, branchMask);
        AscendC::Reg::Div(xFactorReg, xFactorReg, srcReg, branchMask);
        AscendC::Reg::Adds(xFactorReg, xFactorReg, (T)(-2.0), branchMask);

        AscendC::Reg::Duplicate(constReg, (T)K1_B[0], branchMask);
        mainIter<T, 25, 25, K1_B>(pReg, qReg, constReg, xFactorReg, branchMask);

        // result_big = exp(-x) * (0.5 * (b - p)) / sqrt(x)
        AscendC::Reg::Sub(bigDstReg, constReg, pReg, branchMask);
        AscendC::Reg::Muls(bigDstReg, bigDstReg, (T)0.5, branchMask);
        AscendC::Reg::Muls(factorReg, srcReg, (T)(-1), branchMask);
        AscendC::Reg::Exp(factorReg, factorReg, branchMask);
        AscendC::Reg::Mul(bigDstReg, bigDstReg, factorReg, branchMask);
        AscendC::Reg::Sqrt(factorReg, srcReg, branchMask);
        AscendC::Reg::Div(bigDstReg, bigDstReg, factorReg, branchMask);

        AscendC::Reg::Select(dstReg, bigDstReg, smallDstReg, branchMask);

        // ===== x <= 0: NaN for x < 0=====
        AscendC::Reg::Compares<T, CMPMODE::LT>(branchMask, srcReg, (T)0.0, mask);
        AscendC::Reg::Duplicate(nanReg, (float&)MODIFIED_BESSEL_K1_FLOAT_NAN, branchMask);
        AscendC::Reg::Select(dstReg, nanReg, dstReg, branchMask);

        // ===== Inf for x == 0 =====
        AscendC::Reg::Compares<T, CMPMODE::EQ>(branchMask, srcReg, (T)0.0, mask);
        AscendC::Reg::Duplicate(infReg, INFINITY, branchMask);
        AscendC::Reg::Select(dstReg, infReg, dstReg, branchMask);

        // // Store output
        AscendC::Reg::StoreAlign(dst + i * vlSize, dstReg, mask);
    }
}

template <typename T>
__aicore__ inline void ModifiedBesselK1Extend(const LocalTensor<T> &dst, const LocalTensor<T> &src,
                                           const LocalTensor<uint8_t>& sharedTmpBuffer,
                                           const uint32_t calCount) {
    static_assert(SupportType<T, float>(), "Current data type is  not supported on current device!");
    VF_CALL<ModifiedBesselK1ImplVF<T>>((__ubuf__ T*)dst.GetPhyAddr(),
                                        (__ubuf__ T*)src.GetPhyAddr(),
                                        calCount);
}

#endif  // __ASCENDC_API_REGBASE_MODIFIED_BESSEL_K1_H__
