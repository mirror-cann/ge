# ToStringInfo

## 产品支持情况

全量芯片支持。

## 头文件/库文件

- 头文件：\#include <ge/ge\_graph\_compile\_summary.h\>
- 库文件：libge\_compiler.so

## 功能说明

获取当前逻辑流的信息。

## 函数原型

```c++
AscendString ToStringInfo() const
```

## 参数说明

无

## 返回值说明

AscendString类型的逻辑流信息，例如：

```c++
logic_stream_id: 0, user_stream_label: 11, is_assigned_by_user_stream_pass: false, attached_stream_ids: , physical_model_stream_num: 1, hccl_followed_stream_num: 0.
```

对应信息为：

- 逻辑流ID：0，
- 用户流标签：11，
- 是否来自用户注册的流分配Pass：false，
- 附属从流ID： ，
- 实际物理流数量：1，
- 通信从流数量：0。

## 约束说明

无
