# 隔离Keil工程

`APP_V2_HEALTH_FAIL_TEST/APP_V2_HEALTH_FAIL_TEST.uvprojx`与正式V2共享源码，但用独立故障宏阻止健康确认。输出在 `Build/test_only/APP_V2_HEALTH_FAIL_TEST/`，默认不参加批量构建；迁移后已真实AC5完整Rebuild，0 Error/0 Warning，BIN与原图标封存的隔离测试像完全相同。

该工程不放进Release，不用于正常升级。回滚测试必须有独立可信V1基线、完整48KB Backup、授权与恢复证据，当前回滚/掉电仍未验收。`Tests/hardware/phase8_boot.c`虽然路径含hardware，却是现有正式Boot的薄入口，不得按名称移走。

旧Factory工程及旧身份源码已移至本次归档，原始MDK路径与新增隔离迁移尝试均保留。其固定V1长度20884与当前20960不符，禁止作为当前出厂流程执行。
