# 显式人物承诺Bridge独立转换门

准备器只复制已冻结20组artifact/query/独立GD oracle和一组真实inventory-v1输入，不执行旧世界来源链，不复制core或美术。每个库候选必须使用新的output目录，manifest记录库及输入SHA。测试脚本自行把旧TaggedValue期望解为Godot六种纯值，递归比较类型；不从Bridge输出生成期望。

额外深转换样本明确为纯wire边界：在actual_reporting的活动params、task预留lot完整owner添加int64两端、bool/null、String和深数组/字典；对应期望只改原oracle同一值位置。深NUL、键NUL及迟条非法UTF8分别核全批拒绝及好query/审计不变，不声称这些修改通过旧全Schema或是可达游戏流程。

准备：`python prepare_fixture.py --exports <冻结20组目录> --inventory <原v1 actual_purchase目录> --library <本候选so> --output <新目录>`。随后固定Godot4.7.2在output/game执行`--script res://tests/test_native_actor_commitments_bridge.gd`。这是headless运行门；没有启动editor或GUI结论。

真实GUI位可以复用同一脚本并追加`-- --require-auto-loaded`；该模式先核GDExtensionManager已加载，并且绝不主动load_extension。新的`--install-exception-only`只在链接专用wrapper的独立候选上运行，环境变量`HANMO_REVIEW_FAIL_NEXT_LEGACY_AUDIT`必须真实消耗一次。正式so没有hook；wrapper只将一次真实ProjectionStore::audit变成bad_alloc，其他调用转发原实现，不假造成功结果。
