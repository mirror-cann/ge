# Shape

## Product Support Status

| Product | Support Status |
| | :----------- | :------: |
| Atlas A3 Training Series Products/Atlas A3 Inference Series Products | √ |
| Atlas A2 Training Series Products/Atlas A2 Inference Series Products | √ |

## Module Import

```python
from ge.graph import Shape
```

## Functionality Description

Shape class inherits from Python built-in `list`, used to represent tensor shape dimension information. Besides having all operation capabilities of standard lists, it also provides convenient methods for calculating total element count of shape and judging if it is unknown shape. When dims is None, represents scalar (empty list).

Shape module also defines the following constants:

| Constant Name | Value | Description |
| | :----- | :--- | :--- |
| UNKNOWN_DIM | -1 | Represents unknown dimension |
| UNKNOWN_DIM_NUM | -2 | Represents unknown dimension count |
| UNKNOWN_DIM_SIZE | -1 | Return value of get_shape_size() when shape is unknown |

## Class Definition

```python
class Shape(list):
    def __init__(self, dims: Optional[List[int]] = None) -> None
```

## Function List

| Function | Functionality Description |
| | :--- | :--- |
| \_\_init\_\_(dims=None) | Constructor, creates Shape object. dims is integer list, None represents scalar (empty list) |
| get_shape_size() | Calculates product of all dimensions in shape, i.e., total element count of tensor |
| is_unknown_shape() | Judges if shape contains unknown dimensions |

## Parameter Description

### \_\_init\_\_ Parameter

| Parameter | Type | Required | Description |
| | :----- | :--- | :------: | :--- |
| dims | List[int] | No | Dimension value list, e.g., [1, 3, 224, 224]. None represents scalar (empty list). Default value is None |

## Return Value Description

| Function | Return Type | Description |
| | :--- | :--- | :--- |
| get_shape_size() | int | Product of all dimensions. Returns 0 when shape is empty (scalar); returns -1 when shape contains unknown dimension (UNKNOWN_DIM or UNKNOWN_DIM_NUM) |
| is_unknown_shape() | bool | Returns True if shape contains UNKNOWN_DIM (-1) or UNKNOWN_DIM_NUM (-2); otherwise returns False |

## Constraint Description

- dims parameter must be integer list (list of int) or None, otherwise throws TypeError.
- Shape inherits from list, therefore supports all standard list operations (indexing, slicing, iteration, len etc.).
- When shape contains unknown dimension, get_shape_size() returns -1, rather than throwing exception.

## Usage Example

```python
from ge.graph import Shape

# Create Shape object
shape = Shape([1, 3, 224, 224])

# Get total element count
print(shape.get_shape_size())  # 150528

# Judge if it is unknown shape
print(shape.is_unknown_shape())  # False

# Create Shape containing unknown dimension
unknown_shape = Shape([-1, 3, 224, 224])
print(unknown_shape.is_unknown_shape())  # True
print(unknown_shape.get_shape_size())    # -1

# Scalar shape (empty list)
scalar = Shape()
print(len(scalar))              # 0
print(scalar.get_shape_size())  # 0

# Supports list operations
print(shape[0])      # 1
print(len(shape))    # 4
print(list(shape))   # [1, 3, 224, 224]
```