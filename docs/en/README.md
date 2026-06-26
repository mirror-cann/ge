# GE Documentation Overview

## Documentation

- [Graph Mode Development Guide](https://hiascend.com/document/redirect/CannCommunityGraphguide)

  Designed for single-card graph compilation and execution, providing GE basic concepts, principles, and how to use GE graph engine interfaces for graph construction, compilation and execution.

- [DataFlow Development Guide](https://hiascend.com/document/redirect/CannCommunityDataflow)

  Designed for heterogeneous and cluster graph compilation and execution, introducing how to build, modify, compile and execute computation graphs through DataFlow interfaces.

- [LLM DataDist Development Guide](https://hiascend.com/document/redirect/CannCommunityLLMDatadistdev)

  Designed for large models, introducing how to use LLM-DataDist interfaces to implement data transmission between clusters and build large model inference disaggregated frameworks.

## Technical Articles

- [Computation Graph Optimization](https://www.hiascend.com/zh/developer/techArticles/20240621-1)

    Introduces how GE improves algorithm computation efficiency through general graph optimization techniques (such as constant folding) and unique enhanced graph optimization techniques (such as Shape optimization).

- [Multi-Stream Parallelism](https://www.hiascend.com/zh/developer/techArticles/20240701-1)

    Introduces the implementation principles and enabling methods of multi-stream parallelism technology, and how to improve hardware resource utilization through this technology.

- [Memory Reuse](https://www.hiascend.com/zh/developer/techArticles/202407005-1)

    Introduces how GE combines industry-standard memory optimization methods, utilizes full-graph perspective to finely tune memory reuse algorithms and topological sorting, further compressing network memory occupation.

- [Model Sinking](https://www.hiascend.com/zh/developer/techArticles/20240715-1)

    Introduces how GE improves model scheduling performance and shortens model E2E execution time through graph mode Host scheduling and model sinking scheduling methods.

- [Dynamic Shape Graph Scheduling Acceleration](https://www.hiascend.com/zh/developer/techArticles/20250911-1)

    Introduces key technologies for Host scheduling optimization and how to improve heterogeneous system resource utilization through these technologies.

- [Automatic Fusion](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/850alpha002/graph/graphguide/autofuse_1_0001.html)

    Introduces the implementation principles and enabling methods of automatic fusion, and how to shorten model E2E time through this technology.

## **API**

- GraphEngine API
- DataFlow API
  - C++ Interface
  - Python Interface
- LLM DataDist API
  - C++ Interface
  - Python Interface

## Architecture and Development Guide

- [Architecture Documentation](./design/README.md)

  Introduces GE architecture design from different dimensions, including module architecture, feature design documents and constraint & specification documents, helping developers quickly understand the overall project structure and core design decisions.

- [Development Guide](./contributions/precommit_guide.md)

  Records key design principles, constraint summary and coding red lines during the development process, guiding code writing and review.
