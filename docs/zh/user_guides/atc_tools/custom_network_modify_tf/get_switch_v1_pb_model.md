# 获取Switch\_v1.pb网络模型

1. 将如下文件中的脚本复制到.py文件中，例如复制到_Switch\_v1.py_文件中。

    ```python
    import os
    import numpy as np
    import tensorflow as tf

    x = tf.compat.v1.placeholder(tf.float32, name='x')
    y = tf.compat.v1.placeholder(tf.float32, name='y')
    z = tf.compat.v1.placeholder(tf.float32, name='z')


    def then_branch(x, y, z):
        m = tf.square(x)
        return m + tf.multiply(y, z)

    def else_branch(x, y, z):
        m = tf.pow(x, y)
        return m - tf.div(y, z)

    # 控制流算子使用入口，执行脚本之后，在图中生成对应的V1控制流算子
    def testDefun(x, y, z):
        return tf.cond(pred=x < y, true_fn=lambda: then_branch(x, y, z), false_fn=lambda: else_branch(x, y, z)), y

    def testCase(x, y, z):
        a, b = testDefun(x, y, z)
        return a + b * z


    with tf.compat.v1.Session() as sess:
        result = sess.run(testCase(x, y, z), feed_dict={x: 1., y: .6, z: .2})

        with tf.io.gfile.GFile('./Switch_v1.pb', 'wb') as f:
            f.write(sess.graph_def.SerializeToString())

    ```

2. 切换到_Switch\_v1.py_脚本所在目录，执行如下命令生成Switch\_v1.pb网络模型：

    ```python
    python3.7.5 Switch_v1.py
    ```

    命令执行完毕，在当前目录会生成Switch\_v1.pb网络模型。
