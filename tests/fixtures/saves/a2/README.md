# A2 R lane：精确内容解析合同

本目录的 `resolver-contract.json` 是显式合成的**受信边界输入**，不是实际通过 `ContentCatalog.load_checked` 的内容包，也不是历史存档。测试没有运行完整 M3 restore，没有证明存储选代、Codec v2、迁移、跨进程奖励唯一性或主菜单接线已完成。

## 冻结接口

`ContentSetResolver.resolve(metadata: Dictionary, installed: Array, registry: Variant=null) -> Dictionary`

- `metadata` 来自未来中央 `SaveCodec.inspect_metadata` 的封装身份结果；本模块不读取 `payload_json`，也不接收存档提供的路径来打开目录
- v1 必需 `format_version:1`、`content_id/content_version/content_digest`、`engine_build/rng_backend`，仅按受信 legacy sidecar 关联的精确旧三元组选择
- v2 额外必需 `scenario_id/player_actor_id`、完整 `pack_set`、`pack_set_digest`、按 id 排序且唯一的 `rule_modules:[{id,version}]`
- `installed` 只能是安装索引持有的 `ContentCatalog.load_checked` 成功结果数组：`ok:true, data, content_id, content_version, content_digest, pack_set, pack_set_digest`，可带可信 `presentation_path` 等已验证目录附加字段。它不是目录路径数组；解析器不替上游验证资源、Schema 或 catalog 文件字节
- `pack_set` 使用 P lane 冻结的 `HM-PACK-SET/1` 完整形状；对象不得加字段，包顺序参与身份，不含自摘要。外部 digest 为 `io.canonical(lock)` 的 SHA256，即排序紧凑 UTF-8 JSON 加一个换行；不是完整 `.hmcatalog.json` 文件 SHA
- v2 必须显式传入调用方已有的受信 `registry`；经 `WorldValidation.valid_registry` 与模块行 id/version 检查后只读投影，不在解析器内重建 Builtin/Registry，也不信任 installed 或存档自报兼容列表。v1 保留原两参行为。引擎及 RNG 后端严格沿当前 `Engine.get_version_info().string` 与 `godot:` 前缀
- 成功仅返回 `{ok:true,catalog:<深复制已验证目录>}`；解析器不建立世界、不迁移、不消费 RNG、不写文件、不选择 A/B 代

错误为 `MissingPack`（`missing:[{id,version}]`）、`PackVersionMismatch`（`mismatches:[{id,version,installed_versions}]`）、`PackDigestMismatch`（含精确包顺序/完整组合不可替代）、`IncompatibleRuleModules`、`IncompatibleEngine`、`UnsupportedSaveVersion`、`InvalidSaveMetadata`、`InvalidInstalledCatalog`。v1 诊断明确带 `identity_kind:legacy_content`，不能把旧 content_id 当作已知包名。

错误优先级：未知/非法格式 → 必需身份与锁形状/自摘要 → 引擎 → 当前模块 → 安装索引形状 → 精确目录 → 缺包 → 版本不符 → 完整锁不符。所有错误均不授权 storage 降代；物理错误白名单由中央 Codec/Storage 独立处理。

## 局部证据

基线 `187e38c`；固定 Godot `4.7.2.stable.official.ed1daf0bf`；沿云工具链 `HANMO_PYTHON` 的实际 Python 为 `3.12.14`（不是本机 AGENTS 的 `/usr/bin/python3.11`）。

通过 `tools.build.run_checked` 实际运行：

`$GODOT_BIN --headless --path game --script res://tests/test_pack_set_restore.gd`

- 初始合同骨架：45 断言，40 个预期失败，失败原因均为 `ContentResolverNotImplemented`
- 纯 resolver 实现：45 断言，0 失败，无 `SCRIPT ERROR`/`ERROR`
- 回执在独立 worktree 的 `verification/a2-resolver/{red,green}-contract.log`；没有重复全量或长矩阵

## 原字节 v1 与物理/语义探针

`v1-4f16935-family-after-adoption.hmsave` 来自官方备份 release `baseline-saves-4f16935` 中 `traces/family_succession/first/04-after-adoption.hmsave`，来源 commit `4f16935ca7e838480839143e2a92f9e0992c0683`。文件 58672 字节，SHA256 `153c7b14d6024ad7759ee8e7610b5b5d0d40568f9bfcffd83b4413d6c9325690`。原 ZIP SHA256 `b83e67191ade7a500a59b6d7b12b187c0c8ddce80f2a8823acdeac223d0e773d`，已核 ZIP 成员、原 manifest 记录和原 trace 记录全部一致。`v1-provenance.json` 保留这些可核查字段与原始收据的 `INCOMPLETE/ok:false`、ARCH09 历史原档 `GAP`。

这是重构前构建实际写出的 v1 回归捕获，含完成收养后的非空家庭；不是 F0.5 发行原档。没有改 version/ruleset、重新生成或归一化此文件。`codec-probe-cases.json` 的负例仅从原档内存派生，不是新的历史黄金档。

已通过 `tools.build.run_checked` 运行 `test_storage_metadata.gd`：24 断言、0 失败。测试用真实现有 `Codec.decode` 核验截断、非法封装、长度/摘要、未来格式/规则、引擎、opaque body、header/body身份错误；故意传入 `MissingContent` 目录边界，避免把物理探针误作 decode/restore 成功。原文件 SHA 前后相等。该脚本输出 `SAVE_INSPECT_AND_SLOT_SELECTION=NOT_RUN; M3_FULL_RESTORE=NOT_RUN`。

另有一次真实现行为诊断：`synthetic-unknown-future-envelope.hmsave` 的 magic 正确但版本 999、字段不再像 v1；基线 Codec 返回 `InvalidSaveHeader`，这是中央 M3 待修缺口，不能授权旧 storage 回退。目标必须为 `UnsupportedSaveVersion` 并停止自动恢复。`storage-selection-contract.json` 只记录这些目标分类与未运行状态，绝不是第二份 Codec/选代实现，也不算已有测试通过。

## 审查后边界修复

短尾 LF/CRLF 与最大长度边界新增 14 断言。修复前真实运行共 59 断言，5 个短尾 LF 用例错误落到 MissingPack/PackVersionMismatch/PackDigestMismatch，证明 `$` 并非绝对终锚。现与 P lane 统一 `(?![\s\S])` 完整结束合同，59 断言全部通过；v1 Codec 24 探针相邻复验仍通过。没有以 trusted installed 前提跳过元信息自身形状校验。

## 后续小片：Codec 元信息检查已经实现

在明确移交 `save_codec.gd` 后新增 `SaveCodec.inspect_metadata(text)`，成功返回 `{ok:true, metadata, generation, save_id}`。metadata 包含对应 v1/v2 的封装字段（包括长度及 checksum），不包含 `payload_json`；嵌套字典/数组深复制。v2 的 `scenario_id/player_actor_id/pack_set/pack_set_digest/rule_modules` 必须具备正确封装容器类型，锁内部及模块版本仍交 resolver 校验。

检查只做大小、严格 JSON、已知封装类型、generation/save_id 边界和 payload 原 UTF-8 字节长度/摘要。不会打开目录、校验当前引擎或规则版本、解析 opaque payload JSON、迁移、构造世界或消费 RNG。`encode` 保持旧签名及 v1 写入；`decode` 保持旧签名与 v1 规则错误优先级，并提前把未知 HM-SAVE 格式返回 `UnsupportedSaveVersion`；本片没有实现 v2 正文 decode/restore。

每个 inspect 失败都带 `physical_corruption:bool`。只有代码中的明确损坏白名单为 true；`UnsupportedSaveVersion`、`SaveSizeLimit`、`TooDeep`、`StringTooLong`、`NonFiniteNumber` 及未知错误均为 false。零字节封装由 inspect 报 `InvalidSaveHeader/true`；旧 decode 空串仍保留 `SaveSizeLimit`。Storage 后续只能依据明确 true 决定回退，不能按任意失败或错误字符串猜旧代。

新门揭示旧 Strict 在对象/数组递归时把 `TooDeep` 覆盖为 `ExpectedComma`。经中央明确移交后，`strict_json.gd` 只在两处递归返回时保留首次 `TooDeep`，不无差别保留其他错误；原 `[1e+]`/`[1e309x]`/`[01]` 等优先级继续复验。

实际局部结果：
- 初始 inspector 骨架 58 断言/31 失败；实现后 65 断言仅深度分类 1 红（WIP `e486ef5`），该 WIP 不能单独声称物理保护已收口
- 深度首错与错误优先级补例后，`test_storage_metadata.gd` 79/79；`test_pack_set_restore.gd` 59/59
- 相邻真实 `test_storage.gd` 86/86，含旧 v1 编解码、双代和跨进程恢复；三条已存在极端指数 Warning 保留，无 SCRIPT ERROR/ERROR
- 另用原始 v1 fixture 与匹配的真实 `ContentCatalog.read_prepared(alpha_life)` 做一次实际 Codec.decode，成功返回 generation=3、一个孩子；没有把这次 v1 检查写成 v2/完整 M3 验收
- 原档 Git blob SHA 仍为 `153c7b14d6024ad7759ee8e7610b5b5d0d40568f9bfcffd83b4413d6c9325690`

三份相邻 catalog 与 Schema 由既有准备产物复制到独立生成目录，并核实 schema SHA 与本工区版本完全一致；没有修改 generated 产物救测试。原始命令输出、fixture 来源 SHA 和单次真实 v1 decode 探针保存在独立工区 `verification/a2-resolver/`。当前 `SLOT_SELECTION=NOT_RUN; M3_FULL_RESTORE=NOT_RUN`，未运行全量或长矩阵。
