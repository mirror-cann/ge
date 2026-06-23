# Custom Operator Integration into GE Documentation

This directory contains documentation for two mechanisms for integrating custom operators into GE.

## Directory Structure

| Directory | Mechanism | Registration Method | Applicable Scenario |
|------|------|---------|------------------------------|
| [`custom_op_v2/`](./custom_op_v2/custom_op_architecture.md) | BaseCustomOp + REG_AUTO_MAPPING_OP | Virtual function inheritance + factory registration | External custom operators (arbitrary kernel binary) |
| [`custom_op_v1/`](./custom_op_v1/op_impl_dev_guide.md) | IMPL_OP + OpImplRegisterV2 | Chained function pointers | Standard AscendC developed Aicore operators (framework fully managed) |

## Core Differences

| Dimension | V1 (IMPL_OP) | V2 (BaseCustomOp) |
|------|---------------|---------------------|
| **Execution** | Framework embedded (engine schedules tiling + kernel launch) | User-defined (`EagerExecuteOp::Execute()`) |
| **Online Compilation** | Framework embedded (FE engine handles on behalf) | User-defined (`CompilableOp::Compile()`) |
| **Serialization** | Framework embedded (FE engine automatically serializes) | User-defined (`PortableOp::Serialize/Deserialize`) |
| **Address Refresh** | Framework embedded (DavinciModel automatically handles) | User-defined (`ArgsUpdater::UpdateHostArgs()`) |
| **kernel Language** | Ascend C / TBE (framework compiles) | Arbitrary (user compiles or RTC) |

**Both are complementary rather than replacements**: V1 is suitable for standard AICore operators (framework fully managed, less development effort), V2 is suitable for non-standard kernels (user fully controlled, high flexibility).