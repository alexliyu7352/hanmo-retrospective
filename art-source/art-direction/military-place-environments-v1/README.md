# 军营与山谷环境线 v1

本轮只有一张新栅格：`valley-clean-ground-candidate-01.png`。军营原图经正式设计、酒肆风格参考与实际像素核对后保留，没有为凑图组而重画。

山谷候选由内建 imagegen 编辑原 `alpha-military/valley.png`，用于消除固定岩山、树林和河道与现有真实格位的冲突。它是地表层，实际地形与部队仍由引擎叠加，不能替代全战场分层美术。

- 原始生成结果：候选 01，完整保存未重采样
- 最终 prompt：`prompt-01-valley-clean-ground.txt`
- 来源和状态：`provenance.json`
- 对照与接入建议：`../../../docs/presentation/military-place-environments/REVIEW.md`
- 像素审核：`../../../verification/military-place-environments-20261008/pixel-review.json`

离线像素审核仅通过清洁地表层；后续真实Canvas无头资源与输入检查见接入文档及同组 `headless-result.json`。没有用户批准、真实 GUI、正式viewport像素或完整布局验收声明。旧军营、旧山谷及正式参考图的评审状态全部保留。
