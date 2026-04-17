# 特性交叉分析

> **目的**：确保新需求在 GE 的核心执行场景下均被覆盖，避免遗漏导致部分场景功能缺失或行为异常。
> 逐项分析本需求与以下场景的关系，填写"适用/不适用"。适用的场景需在设计章节给出对应方案；不适用的场景需说明理由。

| 场景 | 适用性 | 分析说明 |
|------|--------|----------|
| 静态 Shape | {适用/不适用} | {说明需求在该场景下的影响和设计要点} |
| 动态 Shape | {适用/不适用} | {说明需求在该场景下的影响和设计要点} |
| 动态 Shape 静态子图（图拆分） | {适用/不适用} | {说明需求在混合图拆分场景下的影响，是否影响拆分边界、子图内行为等} |
| 离线场景（atc 编译） | {适用/不适用} | {说明需求是否影响离线编译流程、OM 产物结构或序列化/反序列化} |
| 在线场景（框架适配） | {适用/不适用} | {说明需求是否影响在线编译执行流程、与 Adapter 的交互等} |

## 分析指引

- **静态 Shape**：需求是否影响已知 shape 的编译优化（内存复用、算子在线编译、下沉调度）？是否涉及 `runtime/v1/` 路径？特别关注 `DavinciModel` 的加载/初始化/执行流程。
- **动态 Shape**：需求是否涉及 shape 不确定的算子或执行路径？是否涉及运行时 tiling、动态内存分配？是否涉及 `runtime/v2/` 路径？特别关注 `LoweringGlobalData`、`ModelConverter`、`LoweringFileConstantNode` 等 lowering 阶段组件。
- **动态 Shape 静态子图（关键交叉场景）**：这是最容易遗漏的场景。动态 shape 模型中的静态子图通过 `DavinciModelKernel` 走 v1 路径。分析要点：
  1. **数据跨越 v2→v1 边界**：如果需求在 v2 的 `LoweringGlobalData` 中新增了数据，这些数据是否需要传递到 v1 的 `DavinciModel`？具体来说，`static_compiled_graph_converter.cc` 的 `CreateDavinciModelOnInitRoot` 是否需要将新增数据作为输入传给 `DavinciModelCreate`/`DavinciModelCreateV2` kernel？
  2. **DavinciModel 接口变更**：如果需求修改了 `DavinciModel` 的接口（如新增 Set 方法），`DavinciModelKernel` 中的 `DavinciModelCreate`/`DavinciModelCreateV2` 是否需要调用新接口？`davinci_model_kernel.cc` 是否需要新增 KernelContext 输入索引？
  3. **检查方法**：grep `DavinciModelCreate` 和 `DavinciModelCreateV2`，确认所有创建 DavinciModel 的路径都能获取到新增的数据。
- **离线场景**：需求是否影响 atc 编译流程、OM 文件格式、模型序列化/反序列化？离线编译产物是否需要新增字段？
- **在线场景**：需求是否影响 GE 与前端框架适配层（TorchAir/TF Adapter）的交互？是否影响 `session` 级别的编译和执行流程？
