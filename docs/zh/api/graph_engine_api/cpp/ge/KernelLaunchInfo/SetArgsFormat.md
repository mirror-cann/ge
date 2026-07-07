# SetArgsFormat

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <graph/kernel\_launch\_info.h\>
- 库文件：libregister.so

## 功能说明

设置算子的ArgsFormat。

## 函数原型

```c++
graphStatus SetArgsFormat(const char *args_format)
```

## 参数说明

| 参数 | 输入/输出 | 说明 |
| --- | --- | --- |
| args_format | 输入 | 算子的ArgsFormat。 |

## 返回值说明

设置成功时返回“ge::GRAPH\_SUCCESS”。

## 约束说明

只有AI CPU和AI Core类型的任务需要设置ArgsFormat。

## 调用示例

```c++
graphStatus Mc2GenTaskCallback(const gert::ExeResGenerationContext *context,
    std::vector<std::vector<uint8_t>> &tasks) {
  ...
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
  // 插入HiddenInput
  aicore_args_format.insert(aicore_args_format.begin() + i, ArgDescInfo::CreateHiddenInput(HiddenInputSubType::kHcom));
  aicore_task.SetArgsFormat(ArgsFormatSerializer::Serialize(aicore_args_format).GetString());
  ...
}
```
