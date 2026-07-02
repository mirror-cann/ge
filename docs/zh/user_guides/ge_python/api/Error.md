# Error

## 模块导入

```python
from ge.error import GeError
```

## 功能说明

`GeError` 是 GE Python API 在调用底层 GE 接口失败时抛出的异常类型，继承自 `RuntimeError`。
原有捕获 `RuntimeError` 的代码仍然可以捕获该异常；需要读取 GE ErrMgr 内部错误信息和接口上下文时，
可以捕获 `GeError`。

## 类定义

```python
class GeError(RuntimeError):
    error_message: Optional[str]
    api_name: Optional[str]
    context: Dict[str, Any]
```

## 属性说明

| 属性 | 类型 | 说明 |
| :--- | :--- | :--- |
| error_message | Optional[str] | GE ErrMgr 上报的内部错误信息 |
| api_name | Optional[str] | 失败的 Python API 或底层 GE API 名称 |
| context | Dict[str, Any] | Python 接口补充的上下文信息，例如 graph_id、stream、output_file |

## 使用示例

```python
from ge.error import GeError
from ge.session import Session

try:
    session = Session()
    outputs = session.run_graph(0, [])
except GeError as err:
    print(err.error_message)
```
