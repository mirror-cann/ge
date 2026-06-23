# ES (Eager Style) Documentation

## Overview

ES (Eager Style) is a functional interface module in GraphEngine for building computation graphs, providing convenient graph building functionality. The ES module supports multiple programming languages (C, C++, Python), offering flexible and easy-to-use graph building methods.

**Core Characteristics of ES Series API:**

* **Auto-generation**: API is not handwritten but auto-generated based on operator prototype definitions, reducing operator developer burden, especially for custom operators that also need to use ES API for graph building scenarios.
* **Multi-language support**: Native support for C, C++, and extensible support for Python, meeting different development habits.
* **Full-dimension compatibility**: Through good API design and code generation mechanism, combined with IR semantic compatibility handling, achieves forward/backward API and ABI compatibility guarantee from the source.

**Three Key Components of ES:**

1. **ES Basic Data Structures** (GE Repository/GE Package): Provides core infrastructure like `EsGraphBuilder` (graph builder), `EsTensorHolder` (tensor holder), which are the foundation for graph building.
2. **ES Code Generator** (GE Repository/GE Package): Core tool `gen_esb`, responsible for reading operator prototype definitions and auto-generating graph building API code for each operator.
3. **Generated ES API** (OPP Package): The final artifact built by generator, included in operator package, is the function interface that users directly call.

## Quick Navigation

### API Reference Documentation

- **[Python API](api/es_python.md)** - Eager Style Graph Builder Python Interface Documentation
  - Main interfaces of GraphBuilder, TensorHolder and other basic classes
- **[C++/C API](api/es_cpp.md)** - Eager Style Graph Builder C++/C Interface Documentation
  - Main interfaces of EsGraphBuilder, EsTensorHolder and other basic classes

### Design Documentation

- **[Overall Architecture Design](.././../design/modules/es_graph/design/architecture_design.md)** - ES module overall architecture design explanation
- **[Ownership Relationship Analysis](../../design/modules/es_graph/design/ownership_analysis.md)** - ES module Python layer and C++ layer resource management mechanism explanation

### Tool Documentation

- **[gen_esb Code Generation Tool](tools/gen_esb.md)** - ES code generation tool usage instructions
  - Tool functionality explanation
  - Usage method and parameter explanation

- **[generate_es_package.cmake](tools/generate_es_package_cmake_readme.md)** - ES code generation, build, install one-stop cmake function
  - Functionality explanation
  - Usage method and parameter explanation

### RFC Documentation

- **RFC Directory** - Design proposal documents
  - New feature proposals
  - Architecture improvement plans
  - API change suggestions

## Document Structure

```
docs/es/
├── README.md                    # This file, document navigation entry
├── api/                         # API reference documentation
├── design/                      # Design and technical analysis documentation
├── tools/                       # Tool usage documentation
└── rfc/                         # RFC proposal documents
```

## Sample

- [Multi-language samples using ES for graph building](../../../../examples/es)

- [Multi-language samples for custom ES API graph building](../../../../examples/custom_es_api)


## Development Roadmap

We first introduced ES functionality in 2025, aiming to provide convenient multi-language graph building capability, support full operator auto API generation, and achieve good forward/backward compatibility. In Q1 2026, we will focus on completing API integration and C++ graph building backward compatibility capability, allowing users to directly install ops package and use ES graph building capability, without manually generating code. Specific development roadmap is as follows:

---

### Core Architecture

- [x] [***December 2025***]ES core architecture design completed and implemented, supporting full prototype code generation and multi-language (C, C++, Python) graph building.

### API Integration

- [x] [***December 2025***]math sub-package completed ES API integration.
- [x] [***February 2026***]nn, cv, transformer completed ES API integration.
- [ ] [***June 2026***]hcom sub-package to complete ES API integration.

Note: Before full prototype ES API is packaged into various operator sub-packages, if you need to use ES API, you can refer to [generate_es_package.cmake](tools/generate_es_package_cmake_readme.md) in tool documentation for code generation and integration.

### Samples and Documentation

- [x] [***December 2025***]Multi-language samples provided, covering common use cases.
- [x] [***December 2025***]Detailed documentation provided, i.e., this directory.
- [x] [***January 2026***]Collective communication samples delivered.

### Backward Compatibility

- [x] [***December 2025***]Python API backward compatibility design completed and implemented.
- [x] [***January 2026***]C++ API backward compatibility design completed; C++ API backward compatibility depends on completing `Historical Prototype Library` design plan.
- [x] [***March 2026***]C++ API backward compatibility code development completed; achieve complete backward compatibility capability for C++ graph building scenarios.

### Custom Operator Project Integration

- [ ] [***TBD***]ascendc custom operator project integration of ES code generation capability, no plan yet.