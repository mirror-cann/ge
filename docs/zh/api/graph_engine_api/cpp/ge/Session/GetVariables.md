# GetVariables

## 头文件/库文件

- 头文件：\#include <ge/ge\_api.h\>
- 库文件：libge\_runner.so

## 功能说明

变量查询接口，获取Session内所有variable算子或指定variable算子的Tensor内容。

## 函数原型

> [!NOTE]说明
>数据类型为string的接口后续版本会废弃，建议使用数据类型为非string的接口。

```c++
Status GetVariables(const std::vector<std::string> &var_names, std::vector<Tensor> &var_values)
Status GetVariables(const std::vector<AscendString> &var_names, std::vector<Tensor> &var_values)
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| var_names | 输入 | variable算子的名字，为空时返回所有variable类型算子的值。 |
| var_values | 输出 | variable算子的值，按照tensor格式返回；与var_names保持一致序列。 |

## 返回值说明

| 参数名 | 类型 | 说明 |
| --- | --- | --- |
| - | Status | SUCCESS：成功<br>其他：不存在该name的variable算子；variable未执行初始化等场景。 |

## 约束说明

无
