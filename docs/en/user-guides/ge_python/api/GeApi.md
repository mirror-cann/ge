# GeApi

## Product Support Status

| Product | Support Status |
| :----------- | :------: |
| Atlas A3 Training Series Products/Atlas A3 Inference Series Products | √ |
| Atlas A2 Training Series Products/Atlas A2 Inference Series Products | √ |

## Module Import

```python
from ge.ge_global import GeApi
from ge.error import GeError
```

## Functionality Description

GeApi class provides GE global initialization and resource release interfaces. ge_initialize must be called before using other GE interfaces, ge_finalize is called when program exits to release resources. Both methods are class methods, can be called without instantiating GeApi.

## Class Definition

```python
class GeApi:
    @classmethod
    def ge_initialize(cls, config: dict) -> None

    @classmethod
    def ge_finalize(cls) -> None
```

## Function Description

### ge_initialize

```python
@classmethod
def ge_initialize(cls, config: dict) -> None
```

**Functionality Description**: Initializes GE, prepares execution environment. This method is a class method, can be called without instantiating GeApi. Must be called before using other GE interfaces.

**Parameter Description**:

| Parameter | Type | Required/Optional | Description |
| :----- | :--- | :-------- | :--- |
| config | dict | Required | GE initialization configuration dictionary, key-value pairs are string type, used to specify GE runtime parameters. |

**Return Value Description**: No return value.

**Constraint Description**:
- config must be dict type, otherwise throws TypeError.
- config cannot be empty dictionary, otherwise throws TypeError.
- When GE initialization fails, will throw GeError, exception information contains GE internal error information and interface context.
- Must call this method before using other GE interfaces.
- Do not repeatedly call ge_initialize, repeated calls may cause undefined behavior.

### ge_finalize

```python
@classmethod
def ge_finalize(cls) -> None
```

**Functionality Description**: Releases GE resources, cleans up execution environment. This method is a class method, can be called without instantiating GeApi. Should be called when program exits or no longer needs GE.

**Parameter Description**: No parameters.

**Return Value Description**: No return value.

**Constraint Description**:
- Before calling ge_finalize, must ensure all Sessions are destroyed, all graph resources are released.
- When GE resource release fails, will throw GeError, exception information contains GE internal error information and interface context.
- Do not continue using any GE interfaces after ge_finalize.
