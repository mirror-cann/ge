# GetBlockDim

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/kernel\_launch\_info.h\>
- 库文件：libregister.so

## 功能说明

获取算子BlockDim。

## 函数原型

```c++
uint32_t GetBlockDim() const
```

## 参数说明

无

## 返回值说明

返回此算子Task的BlockDim值，默认值为0。

异常时返回int32\_max。

## 约束说明

无

## 调用示例

```c++
graphStatus Mc2GenTaskCallback(const gert::ExeResGenerationContext *context,
    std::vector<std::vector<uint8_t>> &tasks) {
  ...
  auto aicore_task = KernelLaunchInfo::LoadFromData(context, tasks.back());
  auto block_dim = aicore_task.GetBlockDim();
  ...
}
```
