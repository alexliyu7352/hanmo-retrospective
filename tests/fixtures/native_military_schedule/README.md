# 军政旧行为 oracle：已捕获，未验收 native

固定生产算法来自 `a4fa824f308b5b33bd602a7286dae19219dde50f` 的 `game/core/military_schedule.gd`（SHA-256 `d7e80dfe46f18d999d9dadd920465059572670c72c804889372a2b9eb926cab3`）。期望在 native 实现到来前由实际旧 GD 首次捕获；不得用新 provider 重写。

## 当前成果

- `old_oracle_a4fa824.json`：69 行，原动态军事作者及真实收养→本人任官/领令→已有驻军编组→失权取消/返营报告短链；完整提案数组、原候选状态和 quote 见证。首次捕获168/0，普通重放167/0。
- `old_return_oracle_a4fa824.json`：12 行，复用原 `test_campaign_return.prepared` 的真实任官/编组/围城易帜领域来源，短返营命令、原友营/报告读口及真实登记 preview/apply 关闭。捕获25/0，重放25/0。
- 两份 oracle 一共实际出现既定13种提案。BFS等长分叉、环、无路、同点、缺目标、异城先出城、Wilderness和place等值先于kind的边界也走完整 `proposals` 出口。
- 纯候选明确覆盖玩家/死/忙/不可用/战斗占用、最后匹配campaign含closed、失权有无军团、原operating目标分支、pending siege优先、玩家结果页保护、无友营/无路/无报告人、报告角色原序及同城优先。这些候选只做旧纯查询，不声称都是可恢复存档或实际玩法到达状态。
- `test_native_military_schedule.gd` 继承原动态测试军事段，原命令与其校验仍运行。仅将原Codec钩子改为状态记录，不执行也不计任何Codec恢复断言；原链必要的已验证状态分支恢复仍原样执行。

## 既有夹具修正

首次复用 `test_campaign_return.prepared` 时，其裸catalog缺失真实 `ok` 头，当前 `Factory.create` 返回 `NoCatalog`，旧夹具却直接取 `.state`，导致 ScriptError 且进程仍exit0。原红保留在 `evidence/return-record-1.log`，绝不算通过。

该现有测试现在沿相邻 `garrison_ownership_fixture` 的既有做法读取真实 `Catalog.read_prepared(lived_city)` 头，再替换原作者data，并检查工厂返回后取state。没有修改工厂、任何作者规则、命令或军事算法。修后短领域初始化和实际关闭均已通过。

## 准备与普通复跑

```sh
python3 tests/fixtures/native_military_schedule/prepare_fixture.py \
  --source /absolute/frozen-source \
  --output /absolute/new-military-fixture

export FIXTURE=/absolute/new-military-fixture
export GODOT=/absolute/path/to/Godot_v4.7.2-stable_linux.x86_64
export XDG_DATA_HOME="$FIXTURE/data" XDG_CONFIG_HOME="$FIXTURE/config" XDG_CACHE_HOME="$FIXTURE/cache"
mkdir -p "$XDG_DATA_HOME" "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME"
"$GODOT" --headless --path "$FIXTURE/game" --editor --import

"$GODOT" --headless --path "$FIXTURE/game" --script res://tests/test_native_military_schedule.gd -- \
  --dynamic-content="$FIXTURE/military/fixtures.json" \
  --oracle=/absolute/repository/tests/fixtures/native_military_schedule/old_oracle_a4fa824.json

"$GODOT" --headless --path "$FIXTURE/game" --script res://tests/capture_return.gd -- \
  --oracle=/absolute/repository/tests/fixtures/native_military_schedule/old_return_oracle_a4fa824.json
```

首次捕获另外显式传 `--record`，且 oracle 必须是新路径；只可使用上面的固定旧生产源。普通复跑绝不传 `--record`。同时检查输出无 `SCRIPT ERROR` 和失败项，不能只检查进程exit0。

准备器只编译原dynamic军事变体和lived_city，复用原tiny图，保留失败输出目录。它不安装或装载扩展，不写默认生成资源，不修改原作者包。

## 尚未覆盖

当前仅交付旧oracle与初始化夹具兼容修正，没有 native provider比较、坏返回原子回滚、性能结论或GUI验收。领域NPC关闭使用原测试控制者切换边界，不等于重新通过 `test_siege_npc_result_guard` 的正式全世界NPC链。未运行原40半日返程、Codec/存读矩阵、全游戏聚合；这些边界不可由测试行数替代。

## 可选provider与原子性门（等待真实构建时不计native通过）

最新runner为 `run_provider_checks.py`。先用准备器建独立工程并完成无扩展导入，再显式选择：

```sh
python3 tests/fixtures/native_military_schedule/run_provider_checks.py \
  --prepared "$FIXTURE" --engine "$GODOT" --output /absolute/new-evidence \
  --mode collect --group oracle
```

`--mode missing-library --build /absolute/frozen-build.json` 要求全新无扩展工程；`--mode native --build ... --library /absolute/frozen.so --library-sha256 <independently-frozen-sha256>` 才装载真实库。库按SHA进入该独立测试工程的专有目录，不覆盖原库，也不重新运行带扩展冷导入。`--group oracle|atomic|all` 可只跑当前可执行的依赖阶段。

- `native_oracle_probe.gd` 在原完整数组和实际军事候选处调用真实provider，native模式还破坏真实Bridge返回的后项、scope/build/形状，以及实际C++输入规范整数；要求失败没有partial，真实恢复调用得到完整原数组。
- `check_atomicity.gd` 复用已冻结“动态真实驻军编组中”点。下一真实Step会完成actor.bandit旅行；检查军事整批失败前已经发生到达、finish_arrivals和钱粮变化，但正式state/请求缓存保持原样、军事preview/apply及observer/Schema不执行。同request恢复后再成功，重复请求不重调provider。
- 另核原合法NPC首条apply写后失败只丢该子candidate，第二NPC仍沿原批次办理；这与整批provider失败区别明确。

当前早期回执在 `evidence/provider-preflight/`：collector主220/0、返程38/0；缺共享hook原子性门实际3/1，不能算绿。真实新so/build及中央hook到位前，本节不提供native验收结论。两份旧oracle从不重录。

### 017ad75真实库窄门结果

`evidence/native-017ad75/acceptance.json` 绑定固定GD/native源017ad75、测试897a0eb及so SHA-256 `49b02d5e9c43ac62cc758605a6d7e6cd8dfb3f60bcb0515e2cc8d4a34e0da07b`。expected_build在运行前从该CMake生成头与Bridge源码独立取得，包含六必需字段及显式ABI/compiler字段，不以待验返回自证身份。

- 主oracle：220/0，69旧记录，51次实际provider对照。
- 返程oracle：38/0，12旧记录，11次实际provider对照。
- 两个输入族各有真实Bridge返回后的六种全批失败/恢复检查。oracle记录还包含完整状态和候选见证，不能称81次独立native调用。
- 原子性：55/0，坏后项/scope/C++坏输入三种军事失败都发生在真实旅行到达、finish_arrivals和钱粮变化之后；正式世界完整未发布，首军事preview/apply及observer/Schema都未执行。同request恢复成功，缓存重放不重跑provider。首NPC真实apply写后失败而次NPC继续也通过。
- 独立缺库：20/0，真实无Bridge类，原半日候选完整回滚；显式切回legacy后原request成功。

旧两份oracle哈希未变，实际进程全部exit0且无ScriptError或退出对象遗留。原缺hook红仍保留。本结果仅关闭这些纯数组与事务窄门，不替代军事Main图形验收或独立跨API生命周期审查。
