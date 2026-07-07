# 简介

算子分解Pass，继承自FusionBasePass。执行引擎会对匹配到的节点调用meet\_requirements\(\)和replacement\(\)方法，而非run\(\)方法。

**约束说明**：

- **不得重写run\(\)方法**：如果子类中定义了run\(\)方法，将在类定义时抛出TypeError。
- 必须实现replacement\(\)方法，meet\_requirements\(\)为可选实现（默认返回True）。
- 子类可定义类属性op\_types: Optional\[List\[str\]\]，用于指定需要分解的算子类型列表。使用register\_decompose\_pass装饰器时会自动设置该属性。
