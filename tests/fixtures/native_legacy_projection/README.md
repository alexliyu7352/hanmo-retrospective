# 离线人物承诺来源与独立旧答案

本片只给 `actor_commitments` 离线工件提供真实来源、生产 GDScript codec 和独立旧算法答案。保持 artifact-only / not-complete-M1，不创建世界 ID、保存格式或新玩法。

生产入口为 `game/core/backends/legacy_projection_codec.gd`：project 先透传原 Schema 错误，再复制完整原记录与必要引用；已识别字段与 extras 只列键分区，不重复值。provenance 在 project 阶段仅含同一 catalog/registry 的 schema/modules/可选 pack；encode_artifact 才要求可信准备器提供实际 Git commit，不能覆盖成另一来源。旧 v1 库存与两旧叶均未改。

`game/tests/export_native_legacy_projection.gd` 的 `encode_case` 可直接消费调用方一次 `save_context()` 得到的三元组。它分别调用生产 codec 和直接旧 state 关联，返回完整 artifact/queries/oracle 三文件文本；oracle 不从新 payload/native 反求期望。采集器将此函数用于真实 Kernel 当前来源；同脚本的命令行门可对保留的原来源重建受信 registry 并运行 codec 边界，不把这种重读说成重新完成了游戏命令。

来源脚本分别复用现有准备链前缀，均在独有 tiny 工程运行：

- collect_procurement：实际受托、公款13份、私人2份、report 拆批为任务3/私人2/预留10；同时组织钱包预留80，原 spendable 为4270。旧 Inventory.count/weight/spendable 原样调用。
- collect_procurement_edges：另两条短链，真实零预算预留0，以及交易已经交割但 trade 行动仍在、没有资源预留。不会重跑先前全门。
- collect_military：真实任命、完成 action 消失而 fact 保留、卸任 disabled 历史、多人编组原序及 pause/hold 时间反例。disabled 任命历史缺 fact 单独标 pure_schema_accepted_boundary，不冒充公共命令生成或授权。
- collect_family：复用 a2_effective_adoption 的既有 two_eras east 作者及唯一钱包变化，真实 move_local/family_adopt 两半日。动态 actor:6 与创建 Fact 同 ID，原型 adopt::actor.prototype 仅目录定义，独立私产；不运行原存读/成长/继任长链。

初次来源基线为真实已发布8c364ccc；19份实际命令来源加1份明确纯Schema边界。原来源与较宽旧算法答案分文件保存，供追溯；正式工件只含冻结有限记录和查询。固定20工件清单 SHA a47a832ce3af5c797008de260094859e900f71fc2fe7081f1d08138f947045b4。Godot使用已核4.7.2，HOME/XDG/TMPDIR独立；全日志必须零诊断才算绿。

2026-10-04本地阶段：来源38/40/16及补边界16均通过；GD codec初门183/0，增加类型/深度64/64MiB/int64/extras/原Schema首错与provenance边界220/0，再加一次save_context导出等值及metadata必要性246/0。metadata缺SHA/错型的10/6失败（含1处引擎诊断）保留，修后10/0。最初未执行fresh工程首次import的HMStrictJSON诊断仅作准备失败保留，不计入绿。Native实际解码/存储/CLI由各自精确回执证明，不用GD计数冒充。

独审最小修：disabled 历史引用存在且值为标量/Array/null或缺kind字典，原Schema可以接受；原34056b7的Dictionary赋值会抛引擎错误。被本片实际选中的事实现先核profile形状，稳定返回 UnsupportedLegacyFactShape 及原facts转义路径，不伪报非法世界、不把existing擦成missing。窄门23/7→23/0（包括missing、正常enabled与合法弱历史正控）；原246和20工件没有重复运行或改称修后全门。坏kind布尔/Array/int/null在旧验证器已有诊断/InvalidStateValidatorResult，调查日志独立保留，不作为Schema合法反例或本片修复范围。

同轮metadata局部修：旧Schema也可能接受registry.modules的非String/缺失/空version。project在原Schema之后、模块排序及读取前核非空String id/版本与Dictionary条目，返回InvalidProjectionProvenance原路径；不输出native必拒的来源。版本六例17/6红含缺version诊断；修后六个版本反例已通过；额外entry/id试探属于原Schema已拒绝输入，不能标为合法metadata反例。首次合并试探20/3含旧验证器integer_id诊断，保留失败日志；最终窄门将entry/empty_id改为原首错对照，integer_id不重跑、不修改旧Schema。正常受信registry正控保持，未重跑原246或20工件。

模块metadata最终窄门21/0、exit0零诊断：六个version形状、entry/empty_id原Schema首错透传及正常正控。上一提交中的提前23/0文字已纠正，真实探索20/3日志保持失败身份；生产codec字节仍为8f066a4，不再增加领域校验。

## 便携入口

准备全新tiny目录：

```sh
python3 tests/fixtures/native_legacy_projection/prepare_fixture.py \
  --source-root <本次源码> --output <全新独立目录> --original-art <原两张77字节图目录>
```

仅显式 `--run --godot <固定4.7.2>` 才执行已限定的真实来源采集和GD门。`source_capture`在每次正式save_context上调用同一encode_case，完整工件/queries/独立oracle写入exports；sources保原state/catalog/模块身份及旧算法答案。重复使用必须另选新输出。准备器不运行native或下载依赖。

```sh
python3 tests/fixtures/native_legacy_projection/run_oracle.py \
  --exports <exports> --sources <sources> --ownership <ownership-source.json> \
  --executable <已核hanmo_sim> --output <新对照目录>
```

如已有精确真实输出，用 `--existing-results <结果目录>` 替代 executable，只做输出/源值审计而不重复native运行。driver仅把artifact和queries传给native。8记录域重组、4元数据摘要、旧库存组件/批次序、原字节绑定与结果均对照；组件期望直接来自旧state/catalog及独立读取的旧required_ownership，不经新codec payload生成。

本轮便携准备烟测：3份tiny compose/stage成功；三份最终helper工程4.7.2 import/解析均exit0零诊断，未重跑原20游戏链。新driver在C已完成的真实B+C结果上20/20（60 query），8域/metadata、旧inventory/lot序、摘要绑定均一致；此为existing-output-check，不冒称重新运行CLI。`--run`便利串联本轮未另全跑，所调用短链及编码门各有前述真实证据。
