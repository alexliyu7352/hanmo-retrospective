# 原生旅行提案短门

## 边界

- `old_oracle_8f7ebc4.json` 由精确旧源 `8f7ebc4f7af5b3e9bdfc81adde4d09d2fed00d93` 的 `Schedule.proposals` 及真实 Kernel 命令首次捕获。不得用新 provider 重写期望。
- 作者来自原 `tests/dynamic_actor_content_fixture.py`；复用真实两半日收养、本人短工、正式旅行、原静态计划，不制造运行人物或修改作者规则。图片只复用已有 tiny fixture，内容仍经正式 prepare。
- 35 行覆盖完整数组及真实 state/trace。只读候选反例包含死亡/忙碌/位置/party/本人和 provider 会话、真实授权来源损坏、重复目的地只推进一次、负数组索引及负余数。空目的地仅放在旧算法不消费的玩家分支，不能声称可执行空计划合法。
- MAX 用例从真实最后一拍在途快照复制，同步调整世界时钟及已存在的社会、驻军、庄园当前时标，先经完整 restore 准入；不模拟跳过历史的玩法。随后真实 Step 抵达，ready=2147483649，原尾 Schema 返回 `InvalidItinerary`，整个候选不发布。
- 失败代理持真实新 Bridge。坏后项、scope、build、形状故障只破坏实际完整返回；C++ 故障只把此次 wire tick 改成非规范 `01`。缺库是独立进程真实无 Bridge 类，错 API 是不具目标方法的对象。所有测试仍经实际 Kernel.submit。
- 记录 candidate 已真实扣其他旅人的钱并结算庄园粮、tick 已改变；首 travel preview/begin、军事、observer 与原 Schema validators 都未执行，已提交完整 state 不变。同 request 恢复真 provider 后成功，成功重放不再次调用 provider。另核真实 `travel_wait` 的 Journey hold 传播失败且保本人位置。
- 没有运行 Codec/存读矩阵、GUI、正式 Main、旧 Bridge 矩阵或全游戏聚合。它们不能由此短门替代。

## 准备与复跑

1. 为精确候选 commit 建独立只读源码 worktree。用本准备器：

   `python tests/fixtures/native_travel_schedule/prepare_fixture.py --source /absolute/frozen-source --output /absolute/new-fixture`

2. 固定 `Godot_v4.7.2-stable_linux.x86_64`，把 XDG_DATA_HOME、XDG_CONFIG_HOME、XDG_CACHE_HOME 都设为新 fixture 下已创建的可写目录。此时工程没有 gdextension 或库，先运行：

   `godot --headless --editor --path "$FIXTURE/game" --import`

3. 旧源捕获是显式 `--record` 且必须使用不存在的新 oracle 路径；普通复跑绝不带 `--record`：

   `godot --headless --path "$FIXTURE/game" --script res://tests/test_native_travel_schedule.gd -- --dynamic-content="$FIXTURE/dynamic/dynamic-actor-fixtures.json" --oracle="$ORACLE"`

4. 从已冻结 CMake 的 `generated/bridge_identity.hpp` 取得预配 build，不能从待验运行返回值自证身份。映射 `lock_sha256` 为 `dependency_lock_sha256`，`configuration` 为 `cmake_configuration`，profile 为 `native_travel_schedule`；本次完整配置在原生 receipt 的 expected_build 中。

5. 缺库进程保持不装载扩展，在上面普通命令追加 `--unavailable --build="$BUILD" --evidence="$NEW_EVIDENCE"`。每个 evidence 路径只许新建。

6. 核对明确冻结 so 的 SHA256 后，复制到独有 `$FIXTURE/game/native_travel_schedule/bin/`。创建 gdextension，entry_symbol=`hanmo_world_library_init`、compatibility_minimum=`4.7.2`、reloadable=false，linux.debug.x86_64 指向 `res://native_travel_schedule/bin/libhanmo_world_bridge.so`。不要重跑带扩展的冷 headless editor import。

7. 运行时显式装载，在普通命令追加：

   `--native --extension=res://native_travel_schedule/hanmo_world.gdextension --build="$BUILD" --evidence="$NEW_EVIDENCE"`

本次证据：旧源 77/0；候选显式 legacy 77/0；新 so 全数组/真实链及七个故障路径 231/0；真实缺库 14/0。证据文件及所有 source/library/input 摘要见 `native_62654ab_receipt.json`。
