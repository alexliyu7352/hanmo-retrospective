# A2 R：带精确锁的 v2 encode 小片

本页保留写入阶段的原始证据与当时未验范围。后续显式 registry 的 v2 decode/候选选择进展见 `V2_DECODING.md`；Kernel/Storage/UI 的整合仍须独立验证。

基线 `5cf55efac4bcb5922e03c28b8ac7e6755a27891d`；最小源码检查点 `8d137af9bea772328b8de182007b86b104ff13ac`。仅修改 `SaveCodec.encode` 所在文件及独有测试/fixture，没有修改 NativeStorage、Strict、Main、Kernel 或 StorageService。

## 冻结门面

当前门面为 `encode(state, generation, save_id, content={}, registry=null) -> String`，保留原四参顺序及失败空串语义。锁 v2 必须显式传入调用方已有的受信 registry；初始 WIP 的四参带锁自动构建行为已在 `0f42be4` 修正，不作为正式旧 ABI。

- 目录完全没有 `pack_set/pack_set_digest`：保持旧 v1 编码
- 任一个字段存在：必须按严格 v2 路径；缺项、类型错误、摘要不符或不兼容均返回空串，绝不降回 v1
- 继续使用实际 `Schema.validate(state, catalog, registry)` 验证状态及旧内容三元组，沿已有家庭事实链验证当前玩家。不能把玩家硬等同于剧本最初 player_actor，否则会误拒合法继任
- v2 封装追加 scenario_id/player_actor_id、完整 pack_set/digest，以及从调用方传入的受信 registry 按 id 排序投影的 rule_modules；目录自报模块清单没有权威性
- 只调用一次既有 `ContentSetResolver.resolve(metadata, [catalog], registry)`，复用精确锁形状、自摘要及身份门，与 Schema 使用同一 registry。Codec/Resolver 不重建 Builtin/Registry，缺失或坏注册拒写；无锁旧调用仍使用 Schema 原兼容路径。不调用 ScenarioSnapshot、Factory 或重新选择 effective view
- payload 仍为原权威状态的规范 JSON；不会插入第二份锁或模块身份。已有这类并行副本的 state 明确拒写 v2，不偷偷删字段来伪装成功
- 不修改 state、catalog、world 或 RNG；重复输入逐字节一致

上游前置条件仍是受信已验证 catalog。此编码器不是资源下载器、安装器或新的 catalog 文件验证器；不会凭存档路径装入目录。完整 v2 decode、NativeStorage.put(v2)、effective view、Kernel 原子 restore 仍未接，本片不能算 M3 完成。

## fixture 真实性与明确限界

`v2-boundary-alpha.hmcatalog.json` 是受信归档原件的逐字节副本，文件 SHA256 `0f448e49acbddab8f244f90ad89ecebcf16a026b246bdec76ed56a9098fb514a`；数据摘要仍为 `f0017626a226b2c1b322b55af128d722943e3d9da086ee3cdb1e5fbe99bc4516`，没有把当前新 Alpha 源的另一摘要配到旧身份。原作者源来自 `ae523bb69d2d6cfb9bf78b5739fafcc2e1eeddd5`，已由 P 的真实 `_legacy_header` 核原源、目录与 Schema。

`v2-boundary-sidecar.json` 使用同一 `_lock` 纯函数生成封闭形状侧锁。真实目录在 Godot 内仍经过正式 `ContentCatalog.load_checked`；原始 v1 捕获也经过真实 Codec.decode/迁移/完整状态校验后才编码 v2。

但此侧锁的资源清单明确为空边界清单：没有运行 public compose_pack_set，没有验证资源安装，也没有通过未来生产 sidecar loader。文件中的 claims 均明确记录 false，不能把它注册或宣传为已验证生产内容集。当前旧 Alpha 公开 compose 仍会因源已变化而报 LegacySourceChanged，这一限制没有被本测试绕过或关闭。

## 实际局部证据

固定 Godot `4.7.2.stable.official.ed1daf0bf`，使用既有 `tools.build.run_checked`：

- 新 `test_save_v2_encoding.gd` **54/54**：真实 v1 世界→v2、实际临时文件同字节回读、完整元信息、payload 原状态、坏锁/缺字段不降级、错目录/剧本/玩家拒写、模块可信来源、原档/world/RNG 不变
- 为验证新用例确实区分旧行为，短暂使用精确 `5cf55ef` Codec 字节作独立对照，新测试在第 8 断言处因“带锁仍输出 v1”明确失败；随后恢复并核对 `8d137af` 字节，再运行得到 54/54。没有把实现后的对照说成事前测试
- 原 metadata 门 **79/79** 在源码检查点上通过；旧 `test_storage.gd` **86/86** 在最终实现上通过，保留三条既有极端指数 Warning，无 SCRIPT ERROR/ERROR
- 单次只读真实 `10-after-succession.hmsave`（来源 commit `4f16935`，SHA256 `8cda635c718721839f47e4b4d8418a98e66e7b1df719e948ef91e58edd80572c`）经现有 v1 decode 后转写 v2，当前 player 仍是 `actor:35`、founder 为 `actor.player`，world/RNG 与原文件不变。没有模拟长年世界，没有改版本伪造历史存档
- 中文职责注释与 `git diff --check` 通过

原始输出、只读继任探针及源码/fixture 摘要在独立工区 `verification/a2-resolver/`。原始 v1 输入保持不变；新 v2 临时输出只在测试隔离路径写入并回读。

明确未验：`V2_DECODE_AND_RESTORE`、`NATIVE_V2_PUT`、`UI_CONTINUE`、生产侧锁安装、完整 Linux 聚合、长矩阵、Web 实际运行。

## 依赖审查后的显式注册修复

基线 `0b78786`，源码检查点 `0f42be467b5dc16117ccff739bf2faf026c97c18`。源码仅改 Codec 与 ContentSetResolver，未改 Kernel、Main、Storage、Strict 或 Schema。新增测试与回执见 `registry-context-receipt.json`。

- 原源码真实失败回执：resolver 60 断言/1 红、encoder 55/1 红，均为缺显式 registry 仍然成功；保留原输出，没有覆盖旧证据
- 修复后 resolver 90/90、encoder 77/77：坏注册容器、坏模块 id/version、id 与键不一致均稳定失败；格式、锁摘要、引擎错误仍优先于缺注册，错误不变成物理损坏
- 实际受信注册的模块版本可被精确投影；测试包装既有真实领域校验并验证一次调用，证明 Schema 与 Resolver 使用同一注入注册，不以内建组合替代
- 原四参无锁 v1 编码及两参 v1 resolver 保持；显式坏 registry 的编码沿 Schema 关闭入口
- 未重新运行长世界或继任长档；前片继任证据仅属于其原源码身份，未声称是本片新执行

完整 v2 decode/restore、Native v2 put、UI、生产侧锁安装及完整 Linux 聚合仍未验证。
