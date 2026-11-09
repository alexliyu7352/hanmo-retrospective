# 正式 native-growth profile 独立验证

本门复用中央唯一正式全资源输出。没有作者变体、tiny工程、资源复制器或第二个顶层runner。

Python模块为标准库 `unittest.TestCase`，也可被项目已有pytest收集。未指定 `HANMO_NATIVE_GROWTH_STAGED_GAME` 时只运行CLI／前置守卫；正式输出五项只读组明确SKIP，不能把它记为已核资源成功。

最终只读验证须从生产源root运行，明确：

- `HANMO_PROFILE_SOURCE_ROOT`：保持clean且运行源码／native身份与stage固定commit一致的真实Git源（纯文档后继可复用）
- `HANMO_NATIVE_GROWTH_STAGED_GAME`：唯一完整输出的`game`目录
- 测试路径：`tests/test_native_growth_profile.py`

正式输出先执行一次完整生产 `validate_profile`。后续错配在读取边界通过临时monkeypatch提供坏字节或manifest；源／配置测试将目标文件排到扫描首位，避免每例重读全部图片。其余分支可复用已核staged-source摘要，明确只测试对应后续守卫。所有补丁在进程内撤销，最终复核manifest／扩展／so原字节与project的完整节／键值语义；允许GUI正常重排，配置值改变仍拒绝。不修改GUI正在使用的文件，不另造一份资产树。

代表Godot门只在已正常GUI导入的输出运行：

`--script res://tests/test_native_growth_profile.gd -- --manifest=<output/native-growth-stage.json> --output=<新JSON>`

脚本按manifest选择完整 `foundation.alpha_life`，实际Kernel新局／wait／advance，直接复用原CharacterSummary叶的行动答案作对照。首次和各次提交后的读取只构建一次，重复读取／preview不得增加project、encode、Bridge实例、install或query。记录原始分段微秒、当前世界规模、实际缓存Bridge的编译身份。审计在真正首次成长读取之后执行，不另建诊断Bridge或混入cache阶段时间。

`--gui-observe` 还须原样附加manifest的`configuration.launch_arguments`（原Main的`--content`／`--scenario`），使用同脚本实例化原 `main.tscn`，F5只读实际Main的诊断／规模／可见行动，F8正常退出。它不自动开局、打开成长或提交；所有真实GUI操作由操作者完成。缺导入／扩展发现时不会主动load或运行headless editor，更不复跑首次134失败后把热重试叫冷成功。

旧 inventory Bridge、actor Bridge、live growth 三套仍只通过中央明确可选suite及其原独立fixture执行。本代表门不重复旧20/60或tiny120矩阵，不写保存文件，不代表完整M1或发行验收。
