# pytest命令行参数
-s: 显示输出调试信息，包括print打印的信息
-v: 显示更详细的信息
-n：支持多线程运行脚本（需要保持用例彼此独立）
--reruns NUM：失败用例重跑次数
-x：表示只要有一个用例报错，那么测试停止
-k：模糊匹配字符串进行用例跑测

# 目录结构
```
scripts
├─reports # 测试报告目录
├─testModule
|   ├─resource # 测试资源文件，存放测试过程中使用到的文件
|   └─tests # 测试用例目录
|       ├─test_case1.py # 测试套件1
|       ├─test_case2.py # 测试套件2
├─main.py # 测试用例执行入口
├─pytest.ini # pytest配置文件
└─requirements.txt # 依赖文件
```

## 测试用例执行
windows环境下执行测试用例：
注意当前还未实现自动化点击hap的功能，请在测试用例执行过程中出现提示点击hap时按指引操作！
进入scripts目录，打开cmd窗口
### 方式一：

    ```
    python main.py
    ```
执行参数在pytest.main中配置

### 方式二：
- 执行所有用例
    ```
    pytest ./
    ```
- 执行指定测试文件
    ```
    pytest ./testModule/test_hichecker.py
    ```
- 执行指定测试用例
    ```
    pytest  -k "test_hichecker_jssdk"
    ```
    
> **user设备需要将预先准备好的测试hap包推入resource目录，会自动安装和启动**

## 测试报告
执行python main.py后，会在reports目录下生成测试报告
