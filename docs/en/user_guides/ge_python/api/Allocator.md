# Allocator

## Product Support Status

| Product | Support Status |
| | :----------- | :------: |
| Atlas A3 Training Series Products/Atlas A3 Inference Series Products | √ |
| Atlas A2 Training Series Products/Atlas A2 Inference Series Products | √ |

## Module Import

```python
from ge.allocator import Allocator, MemBlock
```

## Functionality Description

Allocator is an abstract base class for external memory allocators, used to customize device memory management strategies. MemBlock represents an allocated memory block, holding device memory address and size. Users need to inherit the Allocator class and implement malloc and free methods, then register to specified stream through Session.register_external_allocator().

## Class Definition

### MemBlock Class

```python
class MemBlock:
    def __init__(self, addr: int, size: int)

    @property
    def addr(self) -> int

    @property
    def size(self) -> int
```

### Allocator Class

```python
class Allocator(ABC):
    @abstractmethod
    def malloc(self, size: int) -> MemBlock

    @abstractmethod
    def free(self, block: MemBlock) -> None
```

## Function Description

### MemBlock Class

#### \_\_init\_\_

```python
def __init__(self, addr: int, size: int)
```

**Functionality Description**: Creates a memory block instance, holding device memory address and size information.

**Parameter Description**:

| Parameter | Type | Required/Optional | Description |
| | :----- | :--- | :-------- | :--- |
| addr | int | Required | Device memory address. |
| size | int | Required | Memory size, in bytes. |

**Return Value Description**: No return value.

#### addr (property)

```python
@property
def addr(self) -> int
```

**Functionality Description**: Gets the device memory address.

**Parameter Description**: No parameters.

**Return Value Description**:

| Return Type | Description |
| | :--------- | :--- |
| int | Device memory address. |

#### size (property)

```python
@property
def size(self) -> int
```

**Functionality Description**: Gets the memory size.

**Parameter Description**: No parameters.

**Return Value Description**:

| Return Type | Description |
| | :--------- | :--- |
| int | Memory size, in bytes. |

### Allocator Class

#### malloc

```python
@abstractmethod
def malloc(self, size: int) -> MemBlock
```

**Functionality Description**: Allocates device memory of specified size, returns a MemBlock object containing device memory address and size. This is an abstract method, subclasses must implement it.

**Parameter Description**:

| Parameter | Type | Required/Optional | Description |
| | :----- | :--- | :-------- | :--- |
| size | int | Required | Size of memory to be allocated, in bytes. |

**Return Value Description**:

| Return Type | Description |
| | :--------- | :--- |
| MemBlock | Allocated memory block object, containing valid device memory address. |

**Constraint Description**:
- Subclasses must implement this method, otherwise cannot be instantiated.
- Should throw MemoryError when allocation fails.

#### free

```python
@abstractmethod
def free(self, block: MemBlock) -> None
```

**Functionality Description**: Releases device memory previously allocated through malloc. This is an abstract method, subclasses must implement it.

**Parameter Description**:

| Parameter | Type | Required/Optional | Description |
| | :----- | :--- | :-------- | :--- |
| block | MemBlock | Required | Memory block object to be released, should be a MemBlock instance previously returned by malloc. |

**Return Value Description**: No return value.

**Constraint Description**:
- Subclasses must implement this method, otherwise cannot be instantiated.
- Should not repeatedly release the same memory block.
