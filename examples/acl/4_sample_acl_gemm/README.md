# ACL GEMM 样例使用指导

## 背景说明

该样例用于指导用户如何使用 **ACLNN 接口替代废弃的 ACLBLAS 和 ACLOP 接口**。

在 CANN 开发中，ACLBLAS（如 `aclblasGemmEx`）和 ACLOP（如 `aclopExecuteV2`）接口将逐渐被标记为废弃，新开发场景应优先使用 ACLNN 接口。本样例通过 GEMM（矩阵乘法）算子演示三种接口的等效实现方式，帮助用户理解接口迁移的完整流程。

**本样例将提供：**
1. 展示三种接口（ACLBLAS、ACLOP、ACLNN）执行相同计算的代码实现
2. 对比三种接口的调用方式差异，辅助接口迁移决策
3. 提供完整的精度验证机制，确保迁移后计算结果一致

## 功能描述

该样例对比演示三种 ACL 接口执行 GEMM 计算：
- **aclblasGemmEx**（废弃）：通过废弃的 ACLBLAS 接口执行 GEMM
- **aclopExecuteV2("GEMM")**（废弃）：通过废弃的 ACLOP 接口直接调用 GEMM 算子
- **aclnnMatmul + aclnnMuls + aclnnAdd**（推荐）：通过 ACLNN 组合完成 GEMM 计算

样例使用相同输入数据，依次执行三种接口，自动比较精度并打印 `max_error`。

## GEMM 计算公式

### 数学公式

GEMM（General Matrix Multiply，通用矩阵乘法）的计算公式为：

```
C = αAB + βC
```

其中：
- `A` 是形状为 [M, K] 的输入矩阵
- `B` 是形状为 [K, N] 的输入矩阵
- `C` 是形状为 [M, N] 的输入/输出矩阵
- `alpha` 是标量系数，用于缩放矩阵乘法结果
- `beta` 是标量系数，用于缩放原始矩阵 C

### 本样例计算参数

| 参数 | 值 | 说明 |
|------|-----|------|
| M | 64 | 矩阵 A 的行数，矩阵 C 的行数 |
| N | 64 | 矩阵 B 的列数，矩阵 C 的列数 |
| K | 64 | 矩阵 A 的列数，矩阵 B 的行数 |
| alpha | 2.0 | 矩阵乘法结果的缩放系数 |
| beta | 2.0 | 原始矩阵 C 的缩放系数 |
| 数据类型 | float16 (FP16) | 所有矩阵和标量使用 FP16 精度 |

## 接口废弃说明

| 接口类型       | 状态     | 说明                                         |
|---------------|---------|----------------------------------------------|
| ACLBLAS       | 废弃   | `aclblasGemmEx` 等接口废弃，不建议新开发使用 |
| ACLOP         | 废弃   | `aclopExecuteV2` 等接口废弃，不建议新开发使用 |
| ACLNN         | 推荐     | 新开发场景应优先使用 ACLNN 接口               |

**ACLNN 接口组合说明：**

ACLNN 接口更接近底层算子原子操作，需要将 GEMM 拆解为：
1. `aclnnMatmul`：执行矩阵乘法 `A @ B`
2. `aclnnMuls`：执行标量乘法 `2.0 × (A @ B)` 和 `2.0 × C`
3. `aclnnAdd`：执行矩阵加法，将两部分相加得到最终结果

## 目录结构

```text
├── scripts
│   ├── build.sh                        // sample 编译脚本
│   ├── run.sh                          // sample 运行脚本
├── src
│   ├── CMakeLists.txt                  // 编译配置脚本
│   ├── sample_acl_gemm.cpp             // ACL GEMM 样例实现文件
├── CMakeLists.txt                      // 编译脚本，调用 src 目录下的 CMakeLists 文件
```

## 环境准备

- 通过安装指导 [环境准备](https://gitcode.com/cann/ge/blob/master/docs/zh/build.md#2-%E5%AE%89%E8%A3%85%E8%BD%AF%E4%BB%B6%E5%8C%85) 正确安装 `toolkit` 和 `ops` 包。
- 设置环境变量（假设包安装在 `/usr/local/Ascend/`）：

```bash
source /usr/local/Ascend/cann/set_env.sh
```

- `SOC_VERSION` 为可选环境变量，未设置时默认使用 `Ascend910B3`。如需指定芯片版本，可执行：

```bash
export SOC_VERSION=Ascend910B3
```

## 构建运行

1. 进入样例脚本目录：

```bash
cd examples/acl/4_sample_acl_gemm/scripts
```

2. 编译程序：

```bash
bash build.sh
```

3. 运行程序：

```bash
bash run.sh
```

## 预期输出

执行成功后，屏幕输出包含如下关键日志：

```text
[INFO] ACL GEMM sample starts
[INFO] Running ACLBLAS GEMM...
[INFO] Running ACLOP GEMM...
[INFO] Running ACLNN GEMM...
[INFO] Comparing results...
max_error: 0
[INFO] VERIFICATION PASSED
[INFO] SAMPLE PASSED
```