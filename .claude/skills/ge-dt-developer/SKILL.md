---
name: ge-dt-developer
description: |-
  GE 项目 UT/ST 测试用例开发，必须在以下场景触发：添加 UT 用例、添加 ST 用例、开发测试、写 UT、写 ST。
---

# GE DT 测试开发规范

开发 UT/ST 用例前，务必先阅读 `docs/zh/contributions/dev_test_guide` 下的三份指导文档：
- `docs/zh/contributions/dev_test_guide/UT_testcase_dev_guide.md`
- `docs/zh/contributions/dev_test_guide/ST_testcase_dev_guide.md`
- `docs/zh/contributions/dev_test_guide/test_framework_guide.md`

## 强制规范

### 1. 在文件末尾追加代码时必须搞清 namespace 边界

C++ 测试文件通常有多层 namespace，文件末尾的 `}` 分别关闭不同的作用域。例如：

```cpp
TEST_F(FooTest, LastTest) {
  // ...
}               // <-- 关闭测试函数
}               // <-- 关闭 namespace inner
}               // <-- 关闭 namespace outer
```

新增测试必须放在**最后一个 `TEST_F` 的 `}` 之后、namespace 的 `}` 之前**。如果误插入到测试函数体内，编译会报 `local class shall not have static data member` 等错误。

**操作方法**：追加代码前，先用 Read 工具读取文件末尾 30-50 行，识别每个 `}` 的含义，确认插入点。

### 2. 写完代码后立即编译验证

在大型 C++ 项目中，目视检查不可靠。编译器报错信息通常非常清晰，能快速定位问题。推荐流程：

1. 编辑代码
2. 立即编译（使用 `ge-dt-runner` skill）
3. 编译失败则根据报错修复，再次编译
4. 编译成功后运行测试验证逻辑正确性

不要在"看代码觉得没问题"后跳过编译。

### 3. 先找到可复制的同类用例，再动手写

大型代码库中 80% 的测试工作来自复制已有模式。写新用例前：

1. 在 `tests/` 目录下搜索与被测代码相关的关键词（函数名、类名、特性名）
2. 优先找同目录下的 ST 文件（ST 参考 ST，UT 参考 UT）
3. 阅读已有用例的：includes、fixture 结构、构造数据的方式、校验手段
4. 在已有用例模式基础上组合新的测试场景
