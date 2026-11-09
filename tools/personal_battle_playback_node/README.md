# 个人战动作回放真实 Main 边界门

本驱动继承现有 `formal_appearance_main_node/main_entry.gd`，复用真实标题、槽选择、正常退出和可见控件定位，不创建另一套框架。

- 只使用指定沈蘅 planning bank 的完整只读副本。银行 SHA-256 固定为 `8b31f1e52156a83a46c97dbdbad21841716dfa17b355e2a57ad19f97624b6fb2`。
- 最多通过真实防御与确认控件结算一轮，生成真正 events；不重跑工作、购装或学艺链。
- 只有协调者确认正式 source freeze 后才允许运行。freeze JSON 必须含 `source_commit` 和冻结项目所有非 `.godot` 文件的 `product_files` SHA-256 映射。
- controls JSON 由冻结后真实源码确认，固定 `pause/play/speed_1/speed_2/skip` 五个键，各值为 `[command_kind, target_id]`，不猜未发布私有方法。
- 纯播放前后保留冷世界原文、typed checkpoint 原字节、RNG、请求序号、source/context 和实际命令回执；跳过不能多提交一轮。
- 不重编 native，校验既有 native165 SO 和用户固定 4.7.2 编辑器。所有运行均 headless；不尝试 GUI，不据此宣称视觉或鼠标全验。
- 输出目录必须全新，原失败运行不覆盖。`report.json` 将 process、产品/银行身份和边界证据分开。

`run.py --help` 给出全部输入。`game/tests/test_personal_playback_main.gd` 将复用同一 driver 作为工程内入口。

## 运行范围与分列证据

- `--mode playback`：原银行一轮真实战斗、播放控件与纯字节不变。
- `--mode cold_reload`：原银行正常继续、普通重绘、同银行正常二次读档；严格0战斗命令，用于补完独立加载边界，不重复已经通过的战斗轮。
- `source_probe_entry.gd`：只读已经产出的两个公开页，经现有Strict解析保留合法整数，独立验证Screen来源反例，不恢复银行、不创建世界。
- `evidence/attempt-01..04`保留各次原失败，不能把它们改写为全绿；attempt04的真实Native播放主链及typed checkpoint字节通过，随后独立来源反例暴露负RefCounted对象ID被拒。
- `evidence/cold-reload-01`保留0命令加载续程的独立成功报告。此结论与前述播放主链组合说明范围，不能冒称一个进程全绿或GUI验收。

最终分列结论见 `evidence/summary.json`：eb3d053 的Native播放主链与0命令正常读档分别有原始证据；c3cc8fc仅放行非零有符号kernel ID后，复用原真实页完成七维来源反例。原attempt04整体失败仍保留；不重写成单进程全绿。GUI像素、操作系统鼠标与导出发行包均未验。
