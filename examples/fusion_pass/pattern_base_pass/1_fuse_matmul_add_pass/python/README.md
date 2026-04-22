## Python Pass[v1]

引入GE Python自定义Fusion Pass开发能力（V1），使用户可以通过纯Python编写`FusionBasePass`并通过装饰器`@register_fusion_pass`注册，GE编译器在初始化阶段自动发现并执行Python pass。
[这里](./src/test_python_pass.py)演示了一个使用纯Python实现的Pass实现与注册sample，
其中设置并获取了`PassContext`中的pass名称，打印了graph对象后设置并获取了图属性。按如下步骤执行

> - run包编译与执行sample的Python版本一致（临时要求）
> - 已安装图编译流程的相关Python库（attrs、decorator、sympy、numpy、psutil、scipy）
1. 设置 Python pass 插件路径
  ```bash
   export ASCEND_GE_PY_PASS_PATH=$(pwd)/src/test_python_pass.py
  ```
2. 复用[C++ pass样例](../cpp/README.md#程序运行a-namesection4524573456563512a)的 ATC 或在线推理步骤执行模型编译。
该 sample 不是独立执行脚本，直接运行 `python test_python_pass.py` 不会触发 pass 执行；预期输出会在 GE 编译流程加载该 Python pass 插件后打印。
run包已包含 GE Python 运行时所需的 ge-py wheel，本节不需要再单独安装 `ge_py-0.0.1-py3-none-any.whl`。
运行结果预期有如下打印，打印Python Pass中设置的`PassContext`与`Graph`信息：
  ```
   --------PassContext as follow--------
    <ge.passes.ge_pass_native.PassContext object at 0x7f07508223b0>
    TestPass
    halo, i'm testpass
    -----------Graph as follow-----------
    graph("model"):
      %onnx::MatMul_0 : [#users=1] = Node[type=Data] (attrs = {index: 0})
      %onnx::MatMul_1 : [#users=1] = Node[type=Data] (attrs = {index: 1})
      %/MatMul : [#users=1] = Node[type=BatchMatMulV2] (inputs = (x1=%onnx::MatMul_0, x2=%onnx::MatMul_1), attrs = {adj_x1: false, adj_x2: false, offset_x: 0})
      %onnx::Add_2 : [#users=1] = Node[type=Data] (attrs = {index: 2})
      %/Add : [#users=1] = Node[type=Add] (inputs = (x1=%/MatMul, x2=%onnx::Add_2))

      return (%/Add)

    a test attr
  ```
