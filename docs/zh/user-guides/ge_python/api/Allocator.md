# Allocator

## 产品支持情况

| 产品 | 是否支持 |
| :----------- | :------: |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | √ |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | √ |

## 模块导入

```python
from ge.allocator import Allocator, MemBlock
```

## 功能说明

Allocator 是外部内存分配器的抽象基类，用于自定义设备内存管理策略。MemBlock 表示已分配的内存块，持有设备内存地址和大小。用户需要继承 Allocator 类并实现 malloc 和 free 方法，然后通过 Session.register_external_allocator() 注册到指定 stream。

## 类定义

### MemBlock 类

```python
class MemBlock:
    def __init__(self, addr: int, size: int)

    @property
    def addr(self) -> int

    @property
    def size(self) -> int
```

### Allocator 类

```python
class Allocator(ABC):
    @abstractmethod
    def malloc(self, size: int) -> MemBlock

    @abstractmethod
    def free(self, block: MemBlock) -> None
```

## 函数说明

### MemBlock 类

#### \_\_init\_\_

```python
def __init__(self, addr: int, size: int)
```

**功能说明**：创建内存块实例，持有设备内存地址和大小信息。

**参数说明**：

| 参数名 | 类型 | 必选/可选 | 说明 |
| :----- | :--- | :-------- | :--- |
| addr | int | 必选 | 设备内存地址。 |
| size | int | 必选 | 内存大小，单位为字节。 |

**返回值说明**：无返回值。

#### addr（属性）

```python
@property
def addr(self) -> int
```

**功能说明**：获取设备内存地址。

**参数说明**：无参数。

**返回值说明**：

| 返回值类型 | 说明 |
| :--------- | :--- |
| int | 设备内存地址。 |

#### size（属性）

```python
@property
def size(self) -> int
```

**功能说明**：获取内存大小。

**参数说明**：无参数。

**返回值说明**：

| 返回值类型 | 说明 |
| :--------- | :--- |
| int | 内存大小，单位为字节。 |

### Allocator 类

#### malloc

```python
@abstractmethod
def malloc(self, size: int) -> MemBlock
```

**功能说明**：分配指定大小的设备内存，返回包含设备内存地址和大小的 MemBlock 对象。此为抽象方法，子类必须实现。

**参数说明**：

| 参数名 | 类型 | 必选/可选 | 说明 |
| :----- | :--- | :-------- | :--- |
| size | int | 必选 | 待分配的内存大小，单位为字节。 |

**返回值说明**：

| 返回值类型 | 说明 |
| :--------- | :--- |
| MemBlock | 已分配的内存块对象，包含有效的设备内存地址。 |

**约束说明**：
- 子类必须实现此方法，否则无法实例化。
- 分配失败时应抛出 MemoryError。

#### free

```python
@abstractmethod
def free(self, block: MemBlock) -> None
```

**功能说明**：释放之前通过 malloc 分配的设备内存。此为抽象方法，子类必须实现。

**参数说明**：

| 参数名 | 类型 | 必选/可选 | 说明 |
| :----- | :--- | :-------- | :--- |
| block | MemBlock | 必选 | 待释放的内存块对象，应为之前通过 malloc 返回的 MemBlock 实例。 |

**返回值说明**：无返回值。

**约束说明**：
- 子类必须实现此方法，否则无法实例化。
- 不应对同一内存块重复释放。
