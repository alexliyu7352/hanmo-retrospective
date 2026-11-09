# 普通课程分段与手动续答的单一Main节点

本目录复用现有 `main.tscn`、实际课程/题面按钮和 `test_regional_life_player_flow.gd` 的系统菜单物理槽流程。新的世界只由正常标题剧本选择产生；任何debug导出只用于核验证据，不再次注入世界。

`prepare_inputs.py --repo <中央源码> --output <新独占目录> --stage-game <已有唯一stage/game> --engine <固定4.7.2>` 正式组合青川1.6，并生成独立安装代及 `runtime-source-binding.json`。准备阶段不修改stage、不安装世界、不运行玩法；状态保持 `MISSING_REVIEWED_ARTIFACT`。源码、Schema、资源、库或驱动变更后须重新记录绑定，不能沿用旧验证身份。

这里的安装代只含新1.6。中央实际合入stage时须保全并核验原已安装各代，合并安装清单或明确独立stage范围，不能用本目录的单包索引覆盖旧目录后声称旧银行仍可使用。

中央最终冻结后补齐精确 `source_commit == reviewed_source_commit`、实际 `native_extension`、`native_build_receipt`、`registration`、`descriptor`、stage源与导入资源摘要，才可将最终绑定状态设为 `BOUND_REVIEWED_ARTIFACT_NOT_EXECUTED`。安装代的所有文件及driver均须使用最终实际路径/摘要；`run_node.py` 会验证后才启动。

同一 `--binding <最终绑定> --case-root <新独占路径>`，按以下顺序分别运行 `run_node.py --phase ...`：

1. `produce`：标题选择原“初来集上”程禾，确认固定作者外貌，用原120钱到讲舍报名三半日手动礼法，按原价预留90钱。先教一课基线，答原第一轮，暂停后系统菜单保存manual-1。恢复原课、完成原余题，只补已听一课的原120%差额；暂停保存manual-2。再恢复、教第二课、取消未提供第三课。
2. `consume-pending`：生产者成功退出后，由新进程空标题“继续”读取原manual-1。精确恢复同一行动、教师、课程条款、课时、种子、第一轮选择与已有经验，执行相同答题和课程尾程。
3. `consume-answered`：另一独立空标题读取原manual-2，继续相同尾程。恢复、继续、取消均不得重复领取已有答题加成。

三个阶段精确比较完整世界JSON与有效checkpoint，两个消费者各比较其剩余正式业务回执和原物理bank全部摘要。没有消费阶段写新bank，也不把副本JSON称为物理存档。`--rendered` 只用于实际视口截图；headless记录不会冒充视觉验收。

本输入只覆盖原礼法课程的真实入口与两个关键持久边界。礼议/筹算原算法、旧终态政策、老师忙碌/离岗、资金上限、分段证据篡改及晚答补差去重由课程owner的针对性规则门覆盖，不能将本节点数量当作广泛矩阵已经运行。准备成功和解析成功不代表玩法已运行。
