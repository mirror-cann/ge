# Error

## Module Import

```python
from ge.error import GeError
```

## Functionality Description

`GeError` is the exception type thrown when GE Python API fails to call underlying GE interfaces, inheriting from `RuntimeError`.
Existing code catching `RuntimeError` can still catch this exception; when needing to read GE ErrMgr internal error information and interface context,
can catch `GeError`.

## Class Definition

```python
class GeError(RuntimeError):
    error_message: Optional[str]
    api_name: Optional[str]
    context: Dict[str, Any]
```

## Attribute Description

| Attribute | Type | Description |
| | :--- | :--- | :--- |
| error_message | Optional[str] | Internal error information reported by GE ErrMgr |
| api_name | Optional[str] | Name of failed Python API or underlying GE API |
| context | Dict[str, Any] | Context information supplemented by Python interface, such as graph_id, stream, output_file |

## Usage Example

```python
from ge.error import GeError
from ge.session import Session

try:
    session = Session()
    outputs = session.run_graph(0, [])
except GeError as err:
    print(err.error_message)
```
