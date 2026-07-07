# meet\_requirements

## 产品支持情况

全量芯片支持。

## 功能说明

判断匹配结果是否满足替换条件。为可选实现，默认返回True。

## 函数原型

```python
meet_requirements(self, match_result: MatchResult) -> bool
```

## 参数说明

| 参数名 | 输入/输出 | 说明 |
| --- | --- | --- |
| match_result | 输入 | 模式匹配结果，类型为MatchResult，包含匹配到的节点和边信息。 |

## 返回值说明

返回True表示满足替换条件，将执行替换；返回False表示不满足，跳过本次替换。默认返回True。

## 约束说明

无
