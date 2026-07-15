# 样例使用指导<a name="ZH-CN_TOPIC_0345664697"></a>

## 功能描述<a name="section5991635456363"></a>

本样例为 MMOE 网络首层 MatMul 融合自定义 pass 样例，基于 `REGISTER_CUSTOM_PASS`（graph_base_pass）方式注册。

针对 MMOE 网络首层中多个"同源输入、相同权重 shape"的 MatMul 算子，将其合并为单个大尺寸 MatMul，再用 SplitV 按原输出维度拆分还原，从而减少 MatMul 算子数量、提升执行效率。

### 与 pattern_base_pass 版本的区别

| 特性 | pattern_base_pass 版本 | graph_base_pass 版本（本样例） |
|------|----------------------|------------------------------|
| 注册方式 | REG_FUSION_PASS + FusionBasePass | REGISTER_CUSTOM_PASS + CustomPassFn |
| 合并节点数 | 固定 2 节点合并 | 支持全量 N 节点合并 |
| 依赖 | 需要 gen_es_api、es_all 库 | 仅依赖 graph、register 库 |
| 图修改接口 | SubgraphRewriter + es API | Graph/GNode 原生接口 |

### 数学等价性

原结构（N 个独立 MatMul）：

```text
y1 = x @ w1     [m, k] @ [k, n] = [m, n]
y2 = x @ w2     [m, k] @ [k, n] = [m, n]
...
yN = x @ wN     [m, k] @ [k, n] = [m, n]
```

融合后结构（1 个大 MatMul + SplitV）：

```text
W  = ConcatV2D(w1, w2, ..., wN, axis=const_idx)   [k, n*N]
Y  = x @ W                                        [m, k] @ [k, n*N] = [m, n*N]
(y1, y2, ..., yN) = SplitV(Y, size_splits=[n, n, ..., n], split_dim=const_idx)
```

矩阵乘法对列拼接满足分配律：`x @ [w1 | w2 | ... | wN] = [x@w1 | x@w2 | ... | x@wN]`，因此 SplitV 拆分后各输出与原 MatMul 输出数学等价。

## 目录结构<a name="section7668345634665"></a>

```text
├── src
│   └── mmoe_matmul_pass.cpp    // pass实现文件
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

1. **FindNodesCanFusion**：遍历图中所有节点，收集 MatMulV2 和 MatMulV3 节点。

2. **FindConstIdx**：对每个 MatMul，判断哪个输入为 Const 权重（const_idx），另一个为 data 输入（data_idx）。

3. **FindRootNode**：从 MatMul 的 data 输入向上递归查找 root（穿过 Reshape/Squeeze 等中间节点），返回 root 节点和 root_output_idx。

4. **FindMatmulNodes**：从 root 出发，沿 Reshape/Squeeze 链路向下游搜索，找到所有满足以下条件的同源 MatMul：
   - const_idx 与 base 一致
   - transpose_x1 / transpose_x2 与 base 一致
   - data 输入 shape 与 base 一致
   - combined_dim（权重沿 const_idx 轴的维度）与 base 一致

5. **ReplaceMmNodes**：对每个命中组合并：
   - `concat_matrices`：通过 ConcatV2D 将各 Const 权重按 const_idx 轴拼接
   - 新建 MatMulV2：data 输入 + 拼接权重，继承 transpose 属性
   - 新建 SplitV：num_split=N，size_splits=[combined_dim, ...]，split_dim=const_idx
   - 将各原 MatMul 下游重连到 SplitV 对应输出
   - 删除原 MatMul 节点，标记已合并节点

## 程序编译<a name="section6645633456813"></a>

假设CANN软件包的安装目录为INSTALL_PATH，例如`/home/HwHiAiUser/Ascend/`。

1. 配置环境变量。

   运行软件包中设置环境变量脚本，命令如下：

```bash
   source ${ASCEND_PATH}/set_env.sh
```

   `${ASCEND_PATH}`为CANN软件包安装目录下的cann路径。请替换相关软件包的实际安装路径，例如`${INSTALL_PATH}/cann`。

2. 执行如下命令进行编译，编译结束后，在**build**目录下生成动态库文件**libmmoe_matmul_pass.so**。

   依次执行:

```bash
   mkdir build && cd build
   cmake .. && make
```

3. 成功编译后通过make install将动态库文件libmmoe_matmul_pass.so安装到自定义融合pass目录下。

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
     atc --model=./model.onnx --framework=5 --soc_version=Ascend910B3 --output=./model_fused
```

     执行完命令后会在**data**目录下生成**model_fused.om**模型文件。

   - 检查执行结果：

     - 自定义Pass生效时，对比NPU编译过程中间dump图，发现模型已按照预期被优化：
       - 融合前（PreRunBegin图）：存在 N 个 MatMul 节点
       - 融合后（RunCustomPass_AfterInferShape图）：存在 1 个大 MatMul + 1 个 SplitV + 1 个 ConcatV2D

     - 日志中出现如下打印：

```text
       MmoeMatmulPass begin.
       MmoeMatmulPass: found 4 MatMul nodes.
       MmoeMatmulPass: trying to merge 4 nodes from root.
       MmoeMatmulPass: merged 4 MatMul nodes.
       MmoeMatmulPass end.
```

3. 使用一键验证脚本。

```bash
   cd data
   ./quick_verify.sh
```

   脚本自动完成编译、生成ONNX、ATC编译和dump图验证。
