# 构建与离线测试

安装 Keil MDK，选择 ARM Compiler 5.06 update 7 build 960。分别打开 README 中的工程，选择明确 Target 执行 Rebuild；不执行 Download，不启动硬件脚本。厂商 Drivers/Middlewares 按原样保留。

Python 工具统一使用项目根目录 `.venv/Scripts/python.exe`，在新机器创建自己的 `.venv`，不要把现有环境提交进仓库。主机 C 测试需要可用的 GCC；可通过 `Tools/run_offline_regression.py` 查阅各组命令和编译依赖。

```powershell
.venv/Scripts/python.exe Tools/check_project_paths.py
.venv/Scripts/python.exe Tools/run_offline_regression.py --output Evidence/local_regression
```

输出目录必须是未使用的新目录。脚本只运行离线测试，不连接 COM。完整构建脚本 `Tools/rebuild_review.ps1` 要求 KEIL_ROOT，检查 Before/After build hooks，保存源快照和日志，并以明确 Target 调用 Rebuild。

打包示例（输出文件必须尚不存在）：

```powershell
.venv/Scripts/python.exe Tools/fw_pack.py input.bin output.fw --version 0x20000 --base 0x08004000
```

不能用 GCC 单函数大小推断 AC5 完整镜像收益。静态 RAM 已包含 heap_4 数组，不能再次累加动态任务栈；仍需验证运行时堆、任务栈与中断主栈余量。
