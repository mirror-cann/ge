# GetStreamId

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/kernel\_launch\_info.h\>
- 库文件：libregister.so

## 功能说明

获取当前Task所在流的id。

## 函数原型

```c++
uint32_t GetStreamId() const
```

## 参数说明

无

## 返回值说明

返回当前Task所在流的id，默认值为0。

异常时，返回int32\_max。

## 约束说明

无

## 调用示例

```c++
graphStatus Mc2GenTaskCallback(const gert::ExeResGenerationContext *context,
    std::vector<std::vector<uint8_t>> &tasks) {
  ...
  auto aicore_task = KernelLaunchInfo::LoadFromData(context, tasks.back());
  auto stream_id = aicore_task.GetStreamId();
  ...
}
```
