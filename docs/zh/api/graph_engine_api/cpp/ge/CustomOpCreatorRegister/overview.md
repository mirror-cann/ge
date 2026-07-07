# 简介

自定义算子注册器，用于注册自定义算子。

## 需要包含的头文件

```c++
#include <graph/custom_op.h>
```

## Public成员函数

```c++
CustomOpCreatorRegister(const AscendString &operator_type, const BaseOpCreator &op_creator)
```
