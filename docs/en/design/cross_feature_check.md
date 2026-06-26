# Feature Cross Analysis

> **Purpose**: Ensure new requirements are covered under GE's core execution scenarios, avoiding omissions that lead to missing functionality or abnormal behavior in some scenarios.
> Analyze the relationship between this requirement and the following scenarios item by item, filling in "Applicable/Not Applicable". Applicable scenarios need to provide corresponding solutions in the design section; not applicable scenarios need to explain reasons.

| Scenario | Applicability | Analysis Description |
|----------|---------------|----------------------|
| Static Shape | {Applicable/Not Applicable} | {Describe the impact and design points of the requirement in this scenario} |
| Dynamic Shape | {Applicable/Not Applicable} | {Describe the impact and design points of the requirement in this scenario} |
| Dynamic Shape Static Subgraph (Graph Split) | {Applicable/Not Applicable} | {Describe the impact of the requirement in mixed graph split scenarios, whether it affects split boundaries, subgraph behavior, etc.} |
| Offline Scenario (atc Compilation) | {Applicable/Not Applicable} | {Describe whether the requirement affects offline compilation flow, OM artifact structure, or serialization/deserialization} |
| Online Scenario (Framework Adaptation) | {Applicable/Not Applicable} | {Describe whether the requirement affects online compilation execution flow, interaction with Adapters, etc.} |

## Analysis Guidance

- **Static Shape**: Does the requirement affect known shape compilation optimization (memory reuse, operator online compilation, sink scheduling)? Does it involve `runtime/v1/` path? Pay special attention to `DavinciModel` loading/initialization/execution flow.
- **Dynamic Shape**: Does the requirement involve operators or execution paths with uncertain shape? Does it involve runtime tiling, dynamic memory allocation? Does it involve `runtime/v2/` path? Pay special attention to `LoweringGlobalData`, `ModelConverter`, `LoweringFileConstantNode` and other lowering phase components.
- **Dynamic Shape Static Subgraph (Critical Cross Scenario)**: This is the most easily overlooked scenario. Static subgraphs in dynamic shape models go through v1 path via `DavinciModelKernel`. Analysis points:
  1. **Data crossing v2→v1 boundary**: If the requirement adds data in v2's `LoweringGlobalData`, does this data need to be passed to v1's `DavinciModel`? Specifically, does `CreateDavinciModelOnInitRoot` in `static_compiled_graph_converter.cc` need to pass the new data as input to `DavinciModelCreate`/`DavinciModelCreateV2` kernel?
  2. **DavinciModel interface changes**: If the requirement modifies `DavinciModel` interfaces (such as adding Set methods), do `DavinciModelCreate`/`DavinciModelCreateV2` in `DavinciModelKernel` need to call the new interfaces? Does `davinci_model_kernel.cc` need new KernelContext input indices?
  3. **Check method**: grep `DavinciModelCreate` and `DavinciModelCreateV2` to confirm all paths creating DavinciModel can access the new data.
- **Offline Scenario**: Does the requirement affect atc compilation flow, OM file format, model serialization/deserialization? Do offline compilation artifacts need new fields?
- **Online Scenario**: Does the requirement affect GE's interaction with frontend framework adaptation layers (TorchAir/TF Adapter)? Does it affect `session`-level compilation and execution flow?
