/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef __ASCENDC_API_REGBASE_MODIFIED_BESSEL_K0_H__
#define __ASCENDC_API_REGBASE_MODIFIED_BESSEL_K0_H__

#include "modified_bessel_i0.h"

constexpr uint32_t MODIFIED_BESSEL_K0_FLOAT_NAN = 0x7fc00000;

#ifndef INFINITY
#define INFINITY (1.0f / 0.0f)
#endif

constexpr float K0_A[10] = {
    +1.37446543561352307156e-16,
    +4.25981614279661018399e-14,
    +1.03496952576338420167e-11,
    +1.90451637722020886025e-09,
    +2.53479107902614945675e-07,
    +2.28621210311945178607e-05,
    +1.26461541144692592338e-03,
    +3.59799365153615016266e-02,
    +3.44289899924628486886e-01,
    -5.35327393233902768720e-01,
};

constexpr float K0_B[25] = {
    +5.30043377268626276149e-18, -1.64758043015242134646e-17,
    +5.21039150503902756861e-17, -1.67823109680541210385e-16,
    +5.51205597852431940784e-16, -1.84859337734377901440e-15,
    +6.34007647740507060557e-15, -2.22751332699166985548e-14,
    +8.03289077536357521100e-14, -2.98009692317273043925e-13,
    +1.14034058820847496303e-12, -4.51459788337394416547e-12,
    +1.85594911495471785253e-11, -7.95748924447710747776e-11,
    +3.57739728140030116597e-10, -1.69753450938905987466e-09,
    +8.57403401741422608519e-09, -4.66048989768794782956e-08,
    +2.76681363944501510342e-07, -1.83175552271911948767e-06,
    +1.39498137188764993662e-05, -1.28495495816278026384e-04,
    +1.56988388573005337491e-03, -3.14481013119645005427e-02,
    +2.44030308206595545468e+00,
};

template <typename T>
__aicore__ inline void ModifiedBesselK0ImplVF(__ubuf__ T* dst, __ubuf__ T* src, uint32_t calCount) {
    uint32_t vlSize = static_cast<uint32_t>(GetVecLen() / sizeof(T));
    uint16_t repeatTime = static_cast<uint16_t>(AscendC::CeilDivision(calCount, vlSize));

    AscendC::Reg::RegTensor<T> srcReg, absXReg, xFactorReg, constReg, iterReg;
    AscendC::Reg::RegTensor<T> pReg, qReg, smallDstReg, bigDstReg, dstReg, nanReg, infReg;
    AscendC::Reg::RegTensor<T> i0Reg, factorReg;
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

        AscendC::Reg::Duplicate(constReg, K0_A[0], branchMask);
        mainIter<T, 10, 10, K0_A>(pReg, qReg, constReg, xFactorReg, branchMask);

        // result_small = 0.5 * (a - p) - log(0.5 * x) * I0(x)
        AscendC::Reg::Sub(smallDstReg, constReg, pReg, branchMask);
        AscendC::Reg::Muls(smallDstReg, smallDstReg, (T)0.5, branchMask);
        
        // calculate modified_bessel_I0(x)
        AscendC::Reg::Duplicate(pReg, (T)0.0, branchMask);
        AscendC::Reg::Duplicate(qReg, (T)0.0, branchMask);
        AscendC::Reg::Abs(absXReg, srcReg, branchMask);
        AscendC::Reg::Muls(xFactorReg, absXReg, (T)0.5, branchMask);
        AscendC::Reg::Adds(xFactorReg, xFactorReg, (T)(-2.0), branchMask);

        AscendC::Reg::Duplicate(constReg, I0_A[0], branchMask);
        mainIter<T, 30, 30, I0_A>(pReg, qReg, constReg, xFactorReg, branchMask);

        AscendC::Reg::Exp(factorReg, absXReg, branchMask);
        AscendC::Reg::Sub(i0Reg, constReg, pReg, branchMask);
        AscendC::Reg::Muls(i0Reg, i0Reg, (T)0.5, branchMask);
        AscendC::Reg::Mul(i0Reg, i0Reg, factorReg, branchMask);

        // get result
        AscendC::Reg::Muls(factorReg, srcReg, (T)0.5, branchMask);
        AscendC::Reg::Log(factorReg, factorReg, branchMask);
        AscendC::Reg::Mul(factorReg, factorReg, i0Reg, branchMask);
        AscendC::Reg::Sub(smallDstReg, smallDstReg, factorReg, branchMask);

        // // ===== Large branch: x > 2.0 =====
        AscendC::Reg::Compares<T, CMPMODE::GT>(branchMask, srcReg, (T)2.0, mask);

        // x_factor = 8/x - 2
        AscendC::Reg::Duplicate(xFactorReg, (T)8, branchMask);
        AscendC::Reg::Div(xFactorReg, xFactorReg, srcReg, branchMask);
        AscendC::Reg::Adds(xFactorReg, xFactorReg, (T)(-2.0), branchMask);

        AscendC::Reg::Duplicate(constReg, (T)K0_B[0], branchMask);
        mainIter<T, 25, 25, K0_B>(pReg, qReg, constReg, xFactorReg, branchMask);

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
        AscendC::Reg::Duplicate(nanReg, (float&)MODIFIED_BESSEL_K0_FLOAT_NAN, branchMask);
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
__aicore__ inline void ModifiedBesselK0Extend(const LocalTensor<T> &dst, const LocalTensor<T> &src,
                                           const LocalTensor<uint8_t>& sharedTmpBuffer,
                                           const uint32_t calCount) {
    static_assert(SupportType<T, float>(), "Current data type is  not supported on current device!");
    VF_CALL<ModifiedBesselK0ImplVF<T>>((__ubuf__ T*)dst.GetPhyAddr(),
                                        (__ubuf__ T*)src.GetPhyAddr(),
                                        calCount);
}

#endif  // __ASCENDC_API_REGBASE_MODIFIED_BESSEL_K0_H__
