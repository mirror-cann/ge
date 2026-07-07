# GetArgsFormat

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/kernel\_launch\_info.h\>
- 库文件：libregister.so

## 功能说明

获取算子的ArgsFormat。

## 函数原型

```c++
const char *GetArgsFormat() const
```

## 参数说明

无

## 返回值说明

成功时返回算子的ArgsFormat。

失败时，返回nullptr。

## 约束说明

只有AI CPU和AI Core类型的任务能获取到ArgsFormat。

## 调用示例

```c++
graphStatus Mc2GenTaskCallback(const gert::ExeResGenerationContext *context,
    std::vector<std::vector<uint8_t>> &tasks) {
  ...
  auto aicore_task = KernelLaunchInfo::LoadFromData(context, tasks.back());
  auto aicore_args_format_str = aicore_task.GetArgsFormat();
  auto aicore_args_format = ArgsFormatSerializer::Deserialize(aicore_args_format_str);
  ...
}
```
