# ATC Model Conversion Practice Guide: Static Shape, Dynamic Multi-Gear, and Dynamic Shape

## 1 Introduction

This document is intended for application developers and focuses on two core questions:

* **Will the input size change**
* **Can changes be enumerated in advance**

Based on these two dimensions, practical solutions for using **ATC** to convert models in Ascend inference scenarios are provided. This document does not distinguish between frontend frameworks and applies to all model formats supported by ATC (such as ONNX, TensorFlow PB, Caffe, etc.).

In Ascend inference scenarios, the choice of shape directly affects the compiler optimization level, runtime scheduling method, and final performance stability. Properly choosing between static shape, dynamic multi-gear, or dynamic shape, combined with ATC's capability characteristics, is key to achieving stable throughput and low latency.

This document assumes that readers already understand the complete process of model conversion via ATC and model loading/inference using **aclmdl** interfaces.

---

## 2 Overall Flow of Model Conversion and Execution

Before diving into specific strategies, let's unify the basic concepts of ATC and model execution phases from an overall flow perspective.

Users convert models into `.om` (Offline Model) files via **ATC** command, then load and execute these models via **aclmdl** series interfaces. From GE (Graph Engine) perspective, these two phases are called **compile** and **execute** respectively.

* **Compile Phase**
  GE reads the model file specified in ATC (such as ONNX or PB), analyzes and optimizes the computation graph, and generates a binary model file (`.om`) that can be executed on NPU.

* **Execute Phase**
  GE loads the `.om` file via aclmdl interfaces, deploys it to NPU device, and executes subsequent inference tasks.

It should be clarified that GE adopts a **clear separation of compile-time and runtime responsibilities** model:

* The compile phase takes longer but usually needs to be executed only once to generate `.om`;
* The execute phase no longer performs structural graph optimization, inference overhead is small, and `.om` can be repeatedly executed after loading.

This characteristic determines the **importance of shape information at compile time**.

## 3 Static Shape, Dynamic Shape, and Performance Characteristics

### Static Shape

**Static shape** means that during multiple executions of the model, all tensor (input, output, and intermediate tensors) dimensions are completely fixed, and no dimension is allowed to change.

In this mode, the compile phase can perform the most comprehensive optimizations and enable **sink scheduling** during execution. The specific mechanism of sink scheduling can be found in the official documentation:
[https://www.hiascend.com/developer/techArticles/20240715-1](https://www.hiascend.com/developer/techArticles/20240715-1)

In engineering practice, static shape usually achieves the best inference performance and stability.

---

### Dynamic Shape

**Dynamic shape** means that during multiple executions of the model, the dimensions of input or intermediate tensors may change.

Its advantage is flexibility, but the cost is also obvious:

* Significantly fewer optimizations available at compile time;
* Cannot enable sink scheduling;
* Inference performance and latency stability are usually poor.

Therefore, in performance-sensitive inference scenarios, completely dynamic shape should be avoided.

---

### Dynamic Multi-Gear (Recommended Balanced Solution)

Considering the significant performance advantage brought by static shape, ATC provides **dynamic multi-gear** capability to handle **scenarios where shape changes are limited and enumerable**.

The essence of dynamic multi-gear is:
During model conversion phase, **specify multiple fixed static shape gears at once**. At runtime, select the matching gear to execute based on actual input, but each gear is treated as static shape during compile phase.

For example, if only the batch dimension of the model is variable and may take the following values:

* `[1, 3, 224, 224]`
* `[8, 3, 224, 224]`
* `[16, 3, 224, 224]`

Then these three batch sizes can be passed to ATC simultaneously as three gears.

After enabling dynamic multi-gear:

* The model still appears as "dynamic" at execution level;
* The compiler can perform static shape optimization for each gear;
* Inference performance usually matches that of single static shape.

Note that while dynamic multi-gear brings performance benefits, it also introduces additional costs:

* **Model memory occupation is based on the largest gear**
  Even when executing the smallest gear, the overall model memory occupation is equivalent to the largest gear. For example, if the largest batch gear is 1024, even when executing batch=1, memory occupation is still calculated as 1024.
* **Compile time increases linearly with the number of gears**
  Generally, the compile time for N gears is approximately N times that of single static shape.

## 4 Overview of Shape-Related Parameter Configuration in ATC

This chapter explains from the **ATC parameter configuration perspective** how the three strategies of static shape, completely dynamic shape, and dynamic multi-gear are expressed in ATC.

### Parameter Configuration for Static Shape

Under the static shape strategy, the model needs to **completely determine all input tensor dimensions** during compile phase. When converting with ATC, users need to explicitly specify a fixed shape for each input.

For example: