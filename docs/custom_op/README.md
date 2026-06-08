# 自定义算子接入 GE 文档

本目录包含自定义算子接入 GE 的两套机制的文档。

## 目录结构

| 目录 | 机制 | 注册方式 | 适用场景                         |
|------|------|---------|------------------------------|
| [`custom_op_v2/`](./custom_op_v2/) | BaseCustomOp + REG_AUTO_MAPPING_OP | 虚函数继承 + 工厂注册 | 外部自定义算子（任意 kernel binary）    |
| [`custom_op_v1/`](./custom_op_v1/) | IMPL_OP + OpImplRegisterV2 | 链式函数指针 | 标准 AscendC开发的Aicore算子（框架全托管） |

## 核心差异

| 维度 | V1（IMPL_OP） | V2（BaseCustomOp） |
|------|---------------|---------------------|
| **执行** | 框架内嵌（引擎调度 tiling + kernel launch） | 用户自定义（`EagerExecuteOp::Execute()`） |
| **在线编译** | 框架内嵌（FE 引擎代劳） | 用户自定义（`CompilableOp::Compile()`） |
| **序列化** | 框架内嵌（FE 引擎自动序列化） | 用户自定义（`PortableOp::Serialize/Deserialize`） |
| **地址刷新** | 框架内嵌（DavinciModel 自动处理） | 用户自定义（`ArgsUpdater::UpdateHostArgs()`） |
| **kernel 语言** | Ascend C / TBE（框架编译） | 任意（用户自行编译或 RTC） |

**两者互补而非替代**：V1 适合标准 AICore 算子（框架全托管，开发量小），V2 适合非标准 kernel（用户全控制，灵活性高）。
