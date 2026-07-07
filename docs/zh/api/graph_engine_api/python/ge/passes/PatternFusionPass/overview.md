# 简介

基于模式匹配的融合Pass，继承自FusionBasePass。执行引擎会调用patterns\(\)、meet\_requirements\(\)和replacement\(\)三个钩子方法，而非run\(\)方法。

约束说明：

- **不得重写run\(\)方法**：如果子类中定义了run\(\)方法，将在类定义时抛出TypeError。
- 必须实现patterns\(\)或至少一个@pattern方法，并实现replacement\(\)方法；meet\_requirements\(\)为可选实现（默认返回True）。
- @pattern方法不能和patterns\(\)方法同时使用。
- 不支持patterns\(self, inputs\) 写法；表达式模式请使用@pattern方法。
