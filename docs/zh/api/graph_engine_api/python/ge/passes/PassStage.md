# PassStage

Pass执行阶段枚举，用于指定Pass在GE编译流程中的注册时机。

| 枚举值 | 说明 |
| --- | --- |
| BEFORE_INFER_SHAPE | 推理形状之前 |
| AFTER_INFER_SHAPE | 推理形状之后 |
| AFTER_BUILTIN_FUSION_PASS | 内置融合Pass之后 |
| AFTER_ORIGIN_GRAPH_OPTIMIZE | 原始图优化之后 |
