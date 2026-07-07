# 简介

模式图包装器，用于在目标图中匹配子图。通常通过create\_pattern构造。

**约束说明**

- \_\_init\_\_\(graph\)会将graph从Python包装器中**move**出去并使其失效，构造完成后不要继续使用该Graph变量。
- release\(\)为GE桥接层内部方法，Python Pass开者无需直接调用。框架在patterns\(\)返回Pattern后会自动调用release\(\)将原生Pattern\*交给C++侧流水线。
- capture\_tensor接受TensorHolder/Node配合index，或一个NodeIo风格对象（具备node/index属性），此时index参数留空None。
