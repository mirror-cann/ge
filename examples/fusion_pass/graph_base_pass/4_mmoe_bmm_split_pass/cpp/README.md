# 样例使用指导<a name="ZH-CN_TOPIC_0345664697"></a>

## 功能描述<a name="section5991635456363"></a>

本样例为 MMOE 网络 MatMul 融合自定义 pass 样例，基于 `REGISTER_CUSTOM_PASS`（graph_base_pass）方式注册。

针对 MMOE 网络非首层中多个"同源输入、相同权重 shape"的 MatMul 算子，将其输入通过 Pack 算子沿首轴合并为 `[N, ...]` 的 shape，通过 BatchMatMulV2 执行批量矩阵乘法，再通过 Split + Squeeze 还原为原多个 MatMul 算子的输出形态，从而减少 MatMul 算子调用次数、提升执行效率。

### 与 pattern_base_pass 版本的区别

| 特性 | pattern_base_pass 版本 | graph_base_pass 版本（本样例） |
|------|----------------------|------------------------------|
| 注册方式 | REG_FUSION_PASS + PatternFusionPass | REGISTER_CUSTOM_PASS + CustomPassFn |
| 合并节点数 | 固定 2 节点合并 | 支持全量 N 节点合并 |
| 依赖 | 需要 gen_es_api、es_all 库 | 仅依赖 graph、register 库 |
| 图修改接口 | SubgraphRewriter + es API | Graph/GNode 原生接口 |

### 数学等价性

原结构（N 个独立 MatMul）：

```text
y1 = x1 @ w1     [m, k] @ [k, n] = [m, n]
y2 = x2 @ w2     [m, k] @ [k, n] = [m, n]
...
yN = xN @ wN     [m, k] @ [k, n] = [m, n]
```

融合后结构（Pack + BatchMatMul + Split + Squeeze）：

```text
X  = Pack(x1, x2, ..., xN, axis=0)     [N, m, k]
W  = Pack(w1, w2, ..., wN, axis=0)     [N, k, n]
Y  = BatchMatMulV2(X, W)               [N, m, k] @ [N, k, n] = [N, m, n]
(y1, y2, ..., yN) = Split(Y, num_split=N, axis=0)
yi = Squeeze(yi_raw, axis=0)           [m, n]
```

Pack 沿首轴堆叠不改变矩阵乘法结果，Split + Squeeze 沿首轴拆分还原各输出，因此各输出与原 MatMul 输出数学等价。

## 目录结构<a name="section7668345634665"></a>

```text
├── src
│   └── mmoe_bmm_split_pass.cpp    // pass实现文件
├── CMakeLists.txt               // 编译脚本
├── data
│   ├── gen_onnx.py             // 生成MMOE风格的ONNX模型
│   └── quick_verify.sh         // 一键验证脚本
```

## 环境要求<a name="section383335652346"></a>

- 编译器：GCC >= 7.3.x
- CANN 版本 >= 8.5.0（REGISTER_CUSTOM_PASS 自 8.5.0 开始支持）
- Python >= 3.7，依赖 onnx、numpy
- 已完成[相关环境准备](../../../../../docs/zh/build.md)。

## 实现步骤<a name="section383335652347"></a>

1. **FindSameLevelMatmulNode**：遍历图中所有节点，收集 MatMulV2 和 MatMulV3 节点，递归向上查找 root 节点（Data 节点或拥有多个非 const 输入的节点），按 root 名 + matmul 层级分组。

2. **CheckMatmulShape**：校验组内所有 MatMul 节点的 transpose_x1/transpose_x2 属性和输入 shape 一致。

3. **ProcessPackNode**：对每个命中组的两个输入分别创建 Pack 算子，沿 axis=0 合并为 `[N, ...]`。

4. **ProcessBatchMatmulNode**：创建 BatchMatMulV2，继承 transpose 属性，连接 Pack 输出。

5. **ProcessSplitAndSqueezeNode**：创建 Split（axis=0）+ Squeeze（axis=0），将各原 MatMul 下游重连到对应 Squeeze 输出，删除原 MatMul 节点。

## 程序编译<a name="section6645633456813"></a>

假设CANN软件包的安装目录为INSTALL_PATH，例如`/home/HwHiAiUser/Ascend/`。

1. 配置环境变量。

   运行软件包中设置环境变量脚本，命令如下：

```bash
   source ${ASCEND_PATH}/set_env.sh
```

   `${ASCEND_PATH}`为CANN软件包安装目录下的cann路径。请替换相关软件包的实际安装路径，例如`${INSTALL_PATH}/cann`。

2. 执行如下命令进行编译，编译结束后，在**build**目录下生成动态库文件**libmmoe_bmm_split_pass.so**。

   依次执行:

```bash
   mkdir build && cd build
   cmake .. && make
```

3. 成功编译后通过make install将动态库文件libmmoe_bmm_split_pass.so安装到自定义融合pass目录下。

```bash
   make install
```

   样例验证完成后，执行如下命令清理安装到 CANN 包下的自定义 pass so，避免影响后续 UT/ST：

```bash
   make clean_custom_pass
```

## 程序运行<a name="section4524573456563512"></a>

1. 配置环境变量(如已执行，跳过)。

```bash
   source ${ASCEND_PATH}/set_env.sh
```

2. 使用ATC离线推理。

   - 设置环境变量，dump出编译过程中的模型图：

```bash
     export DUMP_GE_GRAPH=1
```

   - 在**data**目录执行ONNX模型生成脚本：

```bash
     python3 gen_onnx.py --experts 4
```

     执行结束后，在**data**目录下生成ONNX模型文件**model.onnx**。

   - 执行ATC命令，其中soc_version根据实际模型运行环境填写：

```bash
     atc --model=./model.onnx --framework=5 --soc_version=Ascend910_9362 --output=./model_fused
```

     执行完命令后会在**data**目录下生成**model_fused.om**模型文件。

   - 检查执行结果：

     - 自定义Pass生效时，对比NPU编译过程中间dump图，发现模型已按照预期被优化：
       - 融合前（PreRunBegin图）：存在 N*2 个 MatMul 节点（首层 N + 非首层 N）
       - 融合后（RunCustomPass_AfterInferShape图）：首层 N 个 MatMul 保留，非首层 N 个 MatMul 合并为 Pack + BatchMatMulV2 + Split + Squeeze

     - 日志中出现如下打印：

```text
       MmoeBmmSplitPass begin.
       MmoeBmmSplitPass: merged 1 groups.
       MmoeBmmSplitPass end.
```

3. 使用一键验证脚本。

```bash
   cd data
   ./quick_verify.sh
```

   脚本自动完成编译、生成ONNX、ATC编译和dump图验证。
