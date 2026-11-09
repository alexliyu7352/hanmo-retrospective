# 青川粮行正式场景接入候选

基于 `4950d5d51d20da9ba8c5b7ef8289a8a29b892e58` 的区域合同 §1/3/5。

此目录是供中央串行合入的作者交付资料，游戏运行时不读这里。`manifest.fragment.json` 的三个 background 项与 `qingchuan.grain.city` 模板应合入唯一正式清单，并随新 `local_society_1_6` 包携带；资源只复制 `sources.json` 所列三张原 PNG，逐字节核 SHA-256。原 1.5 包及独立城市工坊保持原件。

唯一 Place 映射是 `place.grain.scene_template_ids.city = "qingchuan.grain.city"`。粮行的净楼体和布棚均绑定 `place.grain`；固定完整外观不代表库存、营业或世界建设状态。酒肆、布铺和客舍未映射成其他地点。

通路热点 `grain.return` 绑定同一地面中的空地裁切 `grain.courtyard_path`，其真实目标是 `place.courtyard`。该裁切和背景逐像素重合，只为现有地点绑定提供明确几何，不新增地砖、门或假建筑；只有现投影含匹配的原 `move_local` 候选时才有热点。旁边原候选名单负责显示“前往 · 青川街口”的名称。人物槽只固定 `actor.merchant`；其 `role.merchant` 交谈有图面热点，其他在场人物与其他角色入口保留原名单。人物离场即不绘制、也不请求外貌。

新增 image_layer 可选 `display_width` 为整数逻辑像素，1..4096；源图保持纵横比，场景锚点和原图支点各按自己的空间换算。背景禁止此字段，省略时沿用旧公式。共享 Schema/Python 验证由中央合入，ScenePresentation 与 GDScript 语义门在本分支交付。

`game/tests/test_qingchuan_grain_scene.gd` 使用真实 Registry、ScenePresentation、LayeredScene、SceneHost；人物采用既有小型合同夹具，提供人忙碌/过期等由可控 preview 边界证明拒绝转发。它不冒充新章原生运行、真实 NPC 行为或最终人物图验收。正式已安装包与真实世界流程由中央组合阶段检查。

图形访问目前被拒绝，未重试窗口。新正式组合尚未进行真实窗口像素验收；源图来自已有有限市集样片，不代表本章最终美术已获用户批准。

## 已执行的局部验证

固定 Godot `4.7.2.stable.official.ed1daf0bf`：粮行宿主/几何/候选合同 40 项通过；相邻原场景引用合同 171 项通过；Python 资源元数据/字段拒绝/原图哈希与原三模块注释检查 13 项通过，追加 Registry 注释另核无问题。记录在 `verification/qingchuan-grain-scene-20261007/`。每项原始日志与实际源文件哈希保留。

相邻门先发现工作树缺少生成的内容 schema；补入 `4950d5d` 的原 `schemas/content.schema.json` 后，171 项中仍有 36 项失败。恢复基线 Contract 同输入得到逐字相同失败，原因是 Registry 未解字段及映射子项的 `$ref`，遗漏表现引用校验。这是正式新章模板的必要接缝，中央批准后只补受信 `$defs` 链解析，修后 171 项通过；原失败分列保留。初始编辑器导入还记录了稀疏工区缺 `storage/` 的 Main 解析错误，不能当作 Main 通过。

本次实际运行依赖中央 `e169efdd` 的 `game/data/presentation.schema.json` 与 `tools/presentation.py` 两个精确文件；二者由中央维护，不重复提交到本 owner 变更。新片段正式包合并、正常 Main 接线、真实 NPC 外观与像素检查仍在中央组合阶段完成。
