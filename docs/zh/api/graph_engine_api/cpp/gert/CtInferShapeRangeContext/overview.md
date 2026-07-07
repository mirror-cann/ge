# 简介

CtInferShapeRangeContext继承自InferShapeRangeContext，用于编译期ShapeRange推导，提供编译期使用的接口。

该类主要用于资源类算子，资源类算子分为创建资源的算子和使用资源的算子两类，这两类算子通过传递DT\_RESOURCE句柄来关联资源。句柄信息通过CtInferShapeRangeContext提供的接口在编译期获取。

CtInferShapeRangeContext继承关系图如下：

![图示](../../../figures/ctInfershaperangecontext.png)

## 需要包含的头文件

```c++
#include <ct_infer_shape_range_context.h>
```
