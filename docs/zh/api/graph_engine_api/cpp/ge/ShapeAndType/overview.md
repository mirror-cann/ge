# 简介

可设置、获取相应对象的形状和数据类型。

## 需要包含的头文件

```c++
#include <graph/inference_context.h>
```

## Public成员函数

```c++
ShapeAndType()
~ShapeAndType()
ShapeAndType(const Shape &shape, DataType data_type)
void SetShape(const Shape &shape)
const Shape &GetShape() const
void SetType(DataType data_type)
DataType GetDataType() const
```
