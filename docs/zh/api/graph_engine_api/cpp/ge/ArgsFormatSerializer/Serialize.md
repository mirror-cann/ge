# Serialize

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/arg\_desc\_info.h\>
- 库文件：libgraph.so

## 函数功能

将ArgsFormat信息序列化成数据流。

## 函数原型

```c++
static AscendString Serialize(const std::vector<ArgDescInfo> &args_format)
```

## 参数说明

| 参数 | 输入/输出 | 说明 |
| --- | --- | --- |
| args_format | 输入 | ArgsFormat信息，由若干个[ArgDescInfo](../ArgDescInfo/ArgDescInfo.md)组成。 |

## 返回值

返回ArgsFormat的序列化结果。

失败时返回空字符串。

## 约束说明

无

## 调用示例

```c++
graphStatus Mc2GenTaskCallback(const gert::ExeResGenerationContext *context, std::vector<std::vector<uint8_t>> &tasks) {
....
// 更改原AI Core任务的ArgsFormat
  auto aicore_task = KernelLaunchInfo::LoadFromData(context, tasks.back());
  auto aicore_args_format_str = aicore_task.GetArgsFormat();
  auto aicore_args_format = ArgsFormatSerializer::Deserialize(aicore_args_format_str);
  size_t i = 0UL;
  for (; i < aicore_args_format.size(); i++) {
    if (aicore_args_format[i].GetType() == ArgDescType::kIrInput ||
        aicore_args_format[i].GetType() == ArgDescType::kInputInstance) {
      break;
    }
  }
  aicore_args_format.insert(aicore_args_format.begin() + i, ArgDescInfo::CreateHiddenInput(HiddenInputSubType::kHcom));
  aicore_task.SetArgsFormat(ArgsFormatSerializer::Serialize(aicore_args_format).GetString());
  tasks.back() = aicore_task.Serialize();
  return SUCCESS;
}
```
