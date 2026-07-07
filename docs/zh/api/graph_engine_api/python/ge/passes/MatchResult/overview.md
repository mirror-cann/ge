# 简介

一次模式匹配的结果，为借用的C++侧MatchResult，其生命周期由引擎/桥接层持有。在PatternFusionPass.meet\_requirements与replacement中作为入参使用。

**约束说明**

- 不要在当前meet\_requirements/replacement调用之外保留MatchResult。
- 桥接层在finally中调用\_invalidate\(\)后，后续方法调用可能失败。
- get\_captured\_tensor\(i\)：i必须与capture\_tensor的顺序一致，否则C++层会失败并抛出异常。
- \_invalidate\(\)仅供GE桥接层调用，Pass代码中不要调用。
