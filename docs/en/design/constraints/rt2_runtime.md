# RT2 Runtime Constraints Document


## General Business Rules

**Design Philosophy:** Rules distilled from business areas prone to errors, ensuring correctness and consistency during development.

**Specific Principles:**

**During Loading:**
- When calling RT2 load and execute interfaces, must ensure loading and execution use the same stream and allocator. If using different stream and allocator, need to call stream synchronization interface after loading completes to ensure loading is complete.
- During RT2 executor construction, should not modify computational graph. Otherwise, may cause multiple executor construction failures.
- RT2 Lowering generated execution node order does not represent executor's execution node order. When writing Lowering functions, must pay attention to node dependencies to ensure execution timing correctness.

**During Execution:**
- When involving resource handling, need to consider resource specifications and limits, as well as resource lifecycle.
- Asynchronous H2D copy must be paired with HOST_TO_DEVICE_EX option, otherwise may cause host memory premature destruction, leading to issues.

---

## Performance Rules

**Design Philosophy:** Ensure system can still run efficiently under high load, avoid performance bottlenecks and degradation.

**Specific Principles:**

- During execution should try to avoid dynamic memory allocation, which may cause random performance degradation.
- When adding new kernels or modifying kernel implementations, need to evaluate impact on execution performance. When adding kernels, should clarify performance impact in design phase and confirm performance specifications. When modifying kernels, performance degradation should not exceed 100ns.

---

## Compatibility Rules

**Design Philosophy:** Ensure system compatibility across different versions and environments, avoid compatibility issues due to changes.

**Specific Principles:**

- When involving changes to external options, environment variables, interfaces, data structures etc., may affect compatibility. These changes need to go through review and obtain passing conclusion before implementation.

---

## Concurrency Handling Rules

**Design Philosophy:** Ensure system stability and performance in multi-threaded environments, avoid resource competition and exceptions.

**Specific Principles:**

- When adding new kernel implementations, need to support multi-threaded calling. If involving handling critical resources (e.g., memory-related kernels), need to mark that kernel (through ConcurrentCriticalSectionKey interface during kernel registration) to avoid resource competition and exceptions in multi-threaded execution scenarios.

---

## Debuggability & Maintainability Principles

**Design Philosophy:** Ensure system is easy to debug and maintain during development and operation, reduce time for problem localization and resolution.

**Specific Principles:**

- Be cautious when adding logs, avoid high-frequency log printing. Log content should be concise and clear, contain necessary context information for quick problem localization.
- Recommend registering KernelTrace function to print Kernel debugging info during execution. Design necessary and effective positioning information to improve debugging efficiency and accuracy.

---
