# A2 R：显式上下文的 v2 decode 与恢复候选选择

基线 `de7a52a76f239e9a670fec4b753b8e968c38c35a`；源码检查点 `6c0c978672866353fc87cac6379c544a97460c7c`。本片仅改生产 `save_codec.gd`、独有测试/证据，未改 Resolver、Kernel、Snapshot、Storage、Schema 或规则迁移。

## 接口与信任边界

`decode(text, content={}, registry=null)` 保留 v1 原两参行为。v2 必须显式传入调用方已经选择的 effective catalog 和同一个受信 registry，缺失/坏注册返回 `IncompatibleRuleModules`，缺失/坏目录返回 `InvalidInstalledCatalog`。不会自行读取默认目录、构造 Builtin/Registry、运行 Snapshot/Factory 或创建新世界。

封装仍使用原大小/严格 JSON/类型/payload UTF-8 长度/checksum 门。正文走原 Strict 的字符串、深度和数值限制；保留 v1 的规则、checksum、正文、头体镜像、引擎、目录、迁移与 Schema 首错顺序。v2 额外核对 scenario_id/player_actor_id 与正文完全一致，拒绝正文中的第二份 pack_set/digest/rule_modules 身份。错误不会附加物理回退许可；Storage 的物理检查白名单未改。

v2 对给定的同一个 effective catalog 复用一次 `ContentSetResolver.resolve(metadata, [content], registry)` 以核精确锁、digest、模块与内容身份；不另选目录。随后复用现有 Rules.upgrade，并将同一 registry 交完整 Schema/Family 校验。成功只返回独立候选 state、generation、save_id；这里没有运行 Kernel 原子替换。

`inspect_restore_selection(text)` 复用有界物理/正文解析和头体镜像检查，使用既有 Rules.upgrade 取得独立候选，成功仅返回 `ok, scenario_id, founder_actor_id`。该结果未取得玩法语义信任，不公开 state，也不接收 already_validated 之类的信任旗标。

- family 只做 Dictionary 形状门，founder 只做非空 String 门，足以供后续 Snapshot 选候选
- v2 原正文缺坏 family/founder 先拒，CURRENT v1 迁移后仍缺坏则拒
- 只有现有 pre-family v1 规则迁移能补历史 founder；不新增迁移版本表，不默认 actor.player 或当前继任玩家
- 合法字符串的假 founder 可以成为未信任候选，完整 Schema/Family 必须再拒绝。children/designations/successions 的完整形状和历史链仍由实际 Family 规则校验
- 当前玩家可因真实继任成为 actor:N；选择 effective view 使用 founder，不能把当前玩家当作最初 playable

整合方的既定调用顺序为：同一 text 的 inspect → Resolver 选原目录及 registry → selection → 对原目录单次 Snapshot(scenario, founder) → decode(text, effective, registry) → Kernel 全校验后原子替换。本片只实现其中 Codec 边界，不证明整条流水已接通。

## 实际证据

固定 Godot `4.7.2.stable.official.ed1daf0bf`，使用 `tools.build.run_checked`；来源与日志 SHA 在 `v2-decoding-receipt.json`。

- 原 decoder 在新最小门的 3 断言中有 1 个预期失败：真正 writer 输出的 v2 仍报 UnsupportedSaveVersion，源失败回执已保留
- 初始源码 5/5、原 metadata 79/79；扩大门第一次 91 断言有 1 红，原因是测试用越界 `family:999999`，原规则正确先报 InstanceCounterCollision。改为合法计数范围的假继任 ID 后才覆盖 InvalidSuccessionChain，未改变生产首错
- 最终 v2 decoder 95/95：真实 v1→writer v2→完整 state/RNG/ID 往返、f09 原正文在新 v2 封装的既有迁移、头体镜像、坏锁/模块/目录/registry、Strict 限制、假 founder/继任、候选不公开 state、同 registry 的实际校验拒绝及原输入不变
- 原写入门 77/77、metadata 79/79、独立 profile 槽选代 111/111、旧 v1 storage 86/86。storage 保留 3 条既有指数 Warning，无 SCRIPT ERROR/ERROR
- 单次有界读取真实 `traces/family_succession/first/10-after-succession.hmsave`，来源 `4f16935`，SHA `8cda635c718721839f47e4b4d8418a98e66e7b1df719e948ef91e58edd80572c`：当前 player `actor:35`，selection founder `actor.player`；v2 写入→decode 的完整 state/RNG/ID 严格相等，历史原字节不变。没有模拟长轨迹，没有 Kernel.restore 声称

小档与真实归档 catalog 的来源保持前片 SHA，边界侧锁仍是原件派生、空资源清单的测试 fixture。没有 public composer/生产 sidecar 安装验证，也未把合成 pre-family 最小迁移输入或内存 v2 包装称为历史黄金档。

未验证：Kernel 原子恢复、NativeStorage v2 put、主菜单继续、生产 sidecar 安装、完整 Linux 聚合、长矩阵、Web 实际运行。原始失败/成功输出与只读继任 probe 位于独立工区 `verification/a2-resolver/`，清理前必须保全。
