# 受信旧内容原件

此目录是 `tools.content.pack_compose.LEGACY_BASELINE_ROOT` 的固定受信位置，不能通过MOD manifest、selection或存档指定另一条归档路径。

## 当前构建与历史归档分开

正常 `tools.build.stage_content(..., foundation_sets=True)` 安装五份当前作者源：`alpha_life`、`journey_life`、`lived_city`、`procurement`、`two_stories`。每份集合绑定当前Schema、当前规则能力描述及当前表现资源，用 `packset.<pack_set_digest> / 1.0.0` 作为目录身份；目录数据和原实体ID保持真实prepare结果，不生成历史 `legacy-identity.json`。下一次staging沿既有合同保留已安装且当前规则仍支持的完整旧代，因此作者或美术变化会产生新锁，不覆盖旧锁。

本目录六份canonical原件不加入正常当前索引。`alpha_life.current` 是原归档时的槽名，不代表今天的Schema或规则版本。`stage_archived_foundation_sets` 仅供独立原件复核；它使用摘要命名的精确旧描述，不能授权当前运行时接受该历史描述。当前运行时继续严格要求当前描述摘要，已有或传入安装集合的描述不匹配时staging明确失败并保留上一代，不自动删除、替换或迁移。HM-SAVE既有显式版本迁移和HM-RESIDENT-SAVE格式边界不变。

`5ec81e22847b0cfb634df779d57afe277f633d512f23d1402660b432c584da9a.rule-capabilities.json` 原字节来自 Git `eed82ba0c1eba6c27709998ca393aa4a99ee10c7:game/data/rule_capabilities.json`（24,087字节）。它补全既有六份canonical锁的原输入，未替换 `game/data/rule_capabilities.json`。旧catalog及存档仍是已有pre-refactor观测原件，不能据此宣称找回F0.5发行原档或证明当前规则兼容。

## 原件与身份

- `manifest.json` 的 format 为 `HM-A2-OBSERVED-LEGACY-BASELINE/1`，登记五份实际已存在的旧 catalog 原字节、对应作者源与旧 Schema摘要；不是重新 prepare 的结果，也不是 F0.5 历史发行档
- 每个 `<name>.hmcatalog.json` 保持原header与完整文件字节，没有增加 `pack_set_digest`
- `<name>.source.json` 的字节必须同时匹配原header.source_sha256和索引source_sha256
- `<schema_sha256>.schema.json` 是原Schema字节，文件名与内容摘要必须一致
- 四份源可从 `0094d7b37d3a66e6648c5ad5ead4f5d347b2fef3:content/foundation/<name>.json` 核验；Alpha原源单独来自 `ae523bb69d2d6cfb9bf78b5739fafcc2e1eeddd5:content/foundation/alpha_life.json`
- 旧Schema与 `396ef55e:schemas/content.schema.json` 逐字一致；五份原catalog的观测来源记录在索引中，不能把观测路径当作用户可选择的加载入口

归档索引、catalog、source与Schema均须实际核摘要。对所选legacy单包，只有当前固定映射源仍等于原件时，composer才复制原catalog并生成绑定精确三元组的sidecar；不能根据文件名或版本号猜测匹配。

## 当前Alpha与旧Alpha分开

2026-10-03、源码基线 `e227518` 的已核事实：

- 旧Alpha内容身份：`foundation.alpha_life / 0.7.0`
- 旧Alpha source_sha256：`eb0a57e1a439b21513300b474d27519ada723b34cadd4d18cbae3bfd052a7296`
- 旧Alpha catalog_sha256：`f0017626a226b2c1b322b55af128d722943e3d9da086ee3cdb1e5fbe99bc4516`
- 当前Alpha source_sha256：`6b19b6479dc8166f6a60798625f4d34de7a0e57ca3137f9c3b73e854efec330b`
- 当前Alpha catalog数据摘要：`36388b3cd4b4cfd7cf19c7a154b53f5be8fcc1ef761919ff61540cc2126b2b15`

因此，用 `legacy-alpha_life.json` 选择当前固定映射源时，CLI须返回非零及 `LegacySourceChanged`，并保留原代。此诊断不是损坏旧档，也不允许自动改用另一个catalog。

当前Alpha可继续使用已有prepare入口生成独立的新目录文件；该操作不改作者源或内容版本，也不把新文件注册成旧锁：

```sh
SOURCE_DIGEST=$(sha256sum content/foundation/alpha_life.json | cut -d ' ' -f 1)
"$HANMO_PYTHON" tools/content_pipeline.py prepare content/foundation/alpha_life.json \
  --output "builds/content/current-alpha/${SOURCE_DIGEST}/alpha_life.hmcatalog.json"
```

旧存档继续依赖其精确旧三元组与受信安装索引。新prepare产物仍是 `AUTHORING_ONLY`、`runtime_verified:false`；生成成功不是安装、真实运行或旧档恢复验收。

## CLI组合范围

```sh
"$HANMO_PYTHON" tools/content_pipeline.py compose \
  --packs content/packs/legacy/procurement \
  --selection content/sets/legacy-procurement.json \
  --output builds/content/legacy-procurement
```

`--packs` 可以重复。selection文件在CLI启动时严格读取并冻结为本次选择值；之后修改文件不会混入本次组合，新的选择须另发一次命令。包manifest、内容、资源及受信合同仍在发布前核对 `SourceChanged`。失败不重用陈旧generated，不覆盖作者源或未受控文件。
