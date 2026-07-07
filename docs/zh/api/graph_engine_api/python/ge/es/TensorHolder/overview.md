# 简介

TensorHolder类用于在Eager Style图构建中表示张量操作。

- TensorHolder对象不能直接创建，由GraphBuilder操作内部创建。
- TensorHolder在GraphBuilder.build\_and\_reset\(\)后不能调用setter方法。
- 所有TensorHolder对象自动解析并保持对其GraphBuilder的强引用，以确保底层C++资源在TensorHolder对象仍在使用时保持有效。
