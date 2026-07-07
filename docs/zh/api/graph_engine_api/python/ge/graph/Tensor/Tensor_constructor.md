# Tensor构造函数

## 产品支持情况

全量芯片支持。

## 功能说明

创建Tensor对象。

## 函数原型

```python
Tensor(data: Optional[UnionTensorDataType] = None, file_path: Optional[str] = None, data_type: Optional[DataType] = DataType.DT_FLOAT, format: Optional[Format] = Format.FORMAT_ND, shape: Optional[List[int]] = None, placement: Optional[Placement] = Placement.PLACEMENT_HOST)
```

## 参数说明

| 参数名 | 输入/输出 | 描述 |
| --- | --- | --- |
| data | 输入 | 数据列表。 |
| file_path | 输入 | 从文件读取的路径。 |
| data_type | 输入 | 数据类型，默认为DT_FLOAT。 |
| format | 输入 | 数据格式，默认为FORMAT_ND。 |
| shape | 输入 | 形状维度列表，None表示标量。 |
| placement | 输入 | Tensor数据的存储位置，默认为PLACEMENT_HOST。 |

## 返回值说明

无

## 约束说明

- data和file\_path必须二选一。
- 如果两者都提供，抛出RuntimeError。
- shape必须是整数列表。
- DT\_DOUBLE类型在Python Tensor构造函数中不支持。

## 调用示例

```python
# 从数据创建
tensor1 = Tensor(data=[1.0, 2.0, 3.0], data_type=DataType.DT_FLOAT, format=Format.FORMAT_ND, shape=[3])

# 从文件创建
tensor2 = Tensor(file_path="/path/to/file", data_type=DataType.DT_FLOAT, format=Format.FORMAT_ND, shape=[2, 2])
```
