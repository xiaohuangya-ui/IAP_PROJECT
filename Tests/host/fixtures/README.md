`iwdg_before_start_fix.c`是用于重现旧启动失败的隔离负例原件，来源为本次归档中的 `Evidence/phase9_iwdg_start_fix_20261016/before/iwdg.c`，复制后保持字节与SHA不变。仅由HOST测试动态插桩编译，正式Keil工程不引用它；不能拿它恢复当前Boot源码。

`router_before_role_specialization.c`来自同一归档根 `Evidence/capacity_opt_20261015_01/source_before.zip` 的 `Shared/router.c` 原字节条目，仅用于三角色实际C轨迹差分oracle；不进入任何固件。
