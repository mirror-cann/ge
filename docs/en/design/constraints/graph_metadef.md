# Graph Compilation Common Infrastructure Constraint Document


Graph and other data structures as common infrastructure, **usability, compatibility** should be prioritized when designing. Common infrastructure is the foundation of the entire system, its design quality directly affects maintainability and extensibility of upper layer modules. Below summarizes core principles from architecture design level.

---

## Core Design Concepts

Common infrastructure design should follow these core ideas:

- **Stability First**: Once infrastructure is established, change cost is extremely high, must consider long-term evolution when designing
- **Minimal Dependencies**: Reduce dependencies on other modules, improve independence and portability
- **Backward Compatibility**: Interface evolution must maintain backward compatibility, avoid breaking existing code
- **Separation of Concerns**: Infrastructure only provides generic capabilities, does not perceive specific business logic
- **Observability**: Built-in necessary debugging and monitoring capabilities, reduce problem localization cost

---

## 1. Module Independence and Cohesion

**Design Philosophy:**
Modules should have clear boundaries and explicit responsibilities, dependency relationships should be explicit and minimized. Module internal implementation details should not leak, external dependencies should be controllable.

**Architecture Principles:**
- Dependencies between modules should be explicit and unidirectional, avoid implicit dependencies causing coupling
- Module splitting should be based on responsibility cohesion, not implementation technical details
- Modules should be self-contained, their dependency relationships should be declared through explicit interfaces

**Concrete Manifestation in GE:**
- SO file dependencies should be explicit, avoid implicit symbol dependencies (verified through `ldd -r`)
- Infrastructure SO splitting (`libgraph.so` and `libgraph_base.so`) should consider different deployment scenarios
- Historical implicit dependencies (specifically libgraph_base's dependency on libgraph) should not continue to expand, need gradual cleanup

---

## 2. Module Boundary Stability and Evolvability

**Design Philosophy:**
Module boundaries should be relatively stable, able to maintain external interface compatibility while internal implementation evolves. Boundary adjustments need careful impact scope assessment.

**Architecture Principles:**
- Module internal implementation details can freely evolve, but external interfaces must remain stable
- Boundary adjustments (such as module归属 changes) need to consider all usage scenarios
- Backward compatibility is the primary constraint of interface evolution

**Concrete Manifestation in GE:**
- CC file归属 adjustments between different SOs need to evaluate multiple deployment scenarios (stub package, non-stub package, tiny scenario)
- Resource-constrained scenario module size constraints need to be considered
- Boundary change impact analysis should cover all possible combination scenarios

---

## 3. Interface Evolution Compatibility Assurance

**Design Philosophy:**
Interfaces are contracts for module collaboration, interface evolution must guarantee not breaking existing code. Even non-public internal interfaces need to consider compatibility.

**Architecture Principles:**
- Interface design should consider possible future extension requirements
- Interface evolution should prefer adding new interfaces, not modifying existing interfaces
- Interface changes (including parameters, return values, semantics) need to evaluate all callers

**Concrete Manifestation in GE:**
- External interfaces (external) and internal interfaces both need to consider ABI compatibility
- Old version component and new version infrastructure combination scenarios need special attention
- Interface extension preferentially adds new interfaces, avoid modifying existing interface default parameters

---

## 4. Observability and Performance Balance

**Design Philosophy:**
Observability (logging, monitoring, debugging) is the foundation of system maintainability, but must balance between performance overhead and observability value.

**Architecture Principles:**
- Infrastructure should provide moderate observability capabilities, but avoid excessive logging
- Hot path observability operations must be carefully designed, avoid performance degradation
- Observability granularity should be configurable, adapt to different scenario requirements

**Concrete Manifestation in GE:**
- Infrastructure interface logging needs to consider call frequency constraints (such as no more than 2 times within 1 second)
- ERROR logs must be used cautiously (callers cannot ignore, must handle)
- High-frequency call scenarios need special logging strategies (sampling, grading, etc.)

---

## 5. Interface Design Robustness Principle

**Design Philosophy:**
Interface design should anticipate various possible calling scenarios, including normal scenarios, boundary scenarios, misuse scenarios, guarantee correct usage through design not documentation.

**Architecture Principles:**
- Interfaces should explicitly declare their calling constraints (such as reentrancy, thread safety)
- Interfaces should prevent misuse through type system, assertions and other mechanisms
- Interface performance characteristics should be reflected in design, not optimized afterwards

**Concrete Manifestation in GE:**
- Graph operation interfaces need to consider reentrancy and idempotency
- Nested calls, repeated calls and other scenarios need提前设计 foolproof mechanisms
- Hot path interface performance optimization needs to be considered at design stage (avoid repeated calculation, temporary object creation)

---

## 6. Separation of Concerns and Layered Design

**Design Philosophy:**
Common infrastructure should focus on providing generic, abstract foundational capabilities, should not perceive specific business logic. Business logic should be implemented in higher layer modules.

**Architecture Principles:**
- Infrastructure should provide domain abstractions, not business rules
- Infrastructure state should be generic and reusable, not business-specific
- Business logic changes should not affect infrastructure interfaces and implementation

**Concrete Manifestation in GE:**
- Graph data structures should not contain business state (such as `is_valid_flag_`, `need_iteration_`, etc.)
- Business-related markers and attributes should be maintained in upper layer modules
- Infrastructure-provided operations should be generic graph operations (storage, query, modification)

---

## 7. Concurrency Model Clarity Principle

**Design Philosophy:**
Concurrency model is an important constraint of module design, must be明确 at design stage. Unclear concurrency model will lead to hard-to-reproduce concurrency issues.

**Architecture Principles:**
- Modules must明确 their concurrency model (single-threaded, multi-threaded read, thread-safe, etc.)
- Concurrency model selection should be based on usage scenario and performance requirement trade-offs
- Concurrency model changes are destructive changes, need to be determined at design stage

**Concrete Manifestation in GE:**
- Graph infrastructure currently adopts single-threaded model, does not support concurrent modification
- Concurrency safety is guaranteed by business layer, not provided by infrastructure
- If need to support concurrency in future, need to明确 interface semantics and usage constraints at design stage

---

## 8. Cross-Platform Consistency Portability Principle

**Design Philosophy:**
Common infrastructure may be used on different platforms, different compilation environments, must guarantee behavior consistency. Implementation detail choices should not introduce platform differences.

**Architecture Principles:**
- Avoid relying on implementation-defined or undefined behavior
- Choose data structures and algorithms with deterministic semantics

**Concrete Manifestation in GE:**
- Avoid using containers with不确定 iteration order (such as `unordered_map`)
- Model file compatibility needs to consider differences between different compilers and standard library implementations

---

## 9. Debuggability and Maintainability Built-in Principle

**Design Philosophy:**
Debuggability should not be a feature added afterwards, but should be built-in at design stage. Infrastructure observability is part of its intrinsic quality.

**Architecture Principles:**
- Data structure complete state should be observable and exportable
- When adding new fields or features must synchronously update observability support
- Observability capabilities themselves should also be easy to extend and maintain

**Concrete Manifestation in GE:**
- Graph serialization is the main维测手段, must guarantee completeness
- When adding new fields need synchronously update serialization logic
- Serialization format needs to consider version compatibility and readability