# GeApi

## 产品支持情况

| 产品 | 是否支持 |
| :----------- | :------: |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 模块导入

```python
from ge.ge_global import GeApi
from ge.error import GeError
```

## 功能说明

GeApi 类提供 GE 的全局初始化和资源释放接口。ge_initialize 必须在使用其他 GE 接口之前调用，ge_finalize 在程序退出时调用以释放资源。两个方法都是类方法，无需实例化 GeApi 即可调用。

## 类定义

```python
class GeApi:
    @classmethod
    def ge_initialize(cls, config: dict) -> None

    @classmethod
    def ge_finalize(cls) -> None
```

## 函数说明

### ge_initialize

```python
@classmethod
def ge_initialize(cls, config: dict) -> None
```

**功能说明**：初始化 GE，准备执行环境。此方法为类方法，无需实例化 GeApi 即可调用。必须在使用其他 GE 接口之前调用。

**参数说明**：

| 参数名 | 类型 | 必选/可选 | 说明 |
| :----- | :--- | :-------- | :--- |
| config | dict | 必选 | GE 初始化配置字典，键值对均为字符串类型，用于指定 GE 的运行参数。 |

**返回值说明**：无返回值。

**约束说明**：
- config 必须为 dict 类型，否则抛出 TypeError。
- config 不能为空字典，否则抛出 TypeError。
- GE 初始化失败时将抛出 GeError，异常信息包含 GE 内部错误信息和接口上下文。
- 必须在使用其他 GE 接口之前调用此方法。
- 请勿重复调用 ge_initialize，重复调用可能导致未定义行为。

### ge_finalize

```python
@classmethod
def ge_finalize(cls) -> None
```

**功能说明**：释放 GE 资源，清理执行环境。此方法为类方法，无需实例化 GeApi 即可调用。应在程序退出或不再需要 GE 时调用。

**参数说明**：无参数。

**返回值说明**：无返回值。

**约束说明**：
- 在调用 ge_finalize 之前，须确保所有 Session 已销毁，所有图资源已释放。
- GE 资源释放失败时将抛出 GeError，异常信息包含 GE 内部错误信息和接口上下文。
- 请勿在 ge_finalize 之后继续使用任何 GE 接口。
