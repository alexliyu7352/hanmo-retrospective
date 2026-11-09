# 青川习业 1.5.0：回气切磋中的量势应敌

本包是 `qingchuan.study@1.4.1` 的明确后继版本，使用独立作者目录和 `content/packs/selections/qingchuan-study-1.5.0.json`。1.4.1目录、选择集和已有证据保持原件；新版本必须重新组合并取得自己的锁，不能把1.4.1的锁或验收记录改名使用。

## 本次玩法增量

只有第五场 `scenario.breath_practice` 使用的独立目标 `objective.breath_spar` 新增 `ai_model: scored_v1`。孟修沿已经实现的有限合法候选评分，权衡有效出手、预计承伤和气势消耗；低体力时沿已有生存规则衡量防守或合法撤离。该场原对白补充这些可理解的对手习惯，不展示调试原因、内部评分或对手下一轮计划。

原前四场、全部人物与开局、柜台库存与钱包、授业与凝神课程、武器与技能、场地、24轮上限、命中与侧背修正、`round_v1` 回气及结果规则均保持1.4.1原值。没有全局AI默认变更、新算法、新数值或预赠成长。基础人物仍是程禾与孟修，仍在同一剑舍习武场，程禾开局120钱、未装备剑、未学武功且普通技艺经验为零。原1.4.1的课程及回气说明见相邻 `qingchuan_study_1_4/README.md`。

五张项目原图、来源说明和表现清单均逐字节复用1.4.1；不生成占位图，不把测试美术锁当作正式资源身份。启用约定来自正式05§5.5与 `game/core/domains/personal/IMPLEMENTATION_GAPS.md` 中已采用的有限评分、512候选预算和低体力生存政策。

## 正常选择与安装

从项目根目录用正常组合入口选择精确版本：

```sh
python3 tools/content_pipeline.py compose --packs content/packs/authored --selection content/packs/selections/qingchuan-study-1.5.0.json --output build/content/qingchuan-study-1.5.0
python3 tools/build.py stage --content-set build/content/qingchuan-study-1.5.0
```

组合目录入口为 `build/content/qingchuan-study-1.5.0/catalog.hmcatalog.json`。`stage` 只安装已核验的组合产物，不自动启动游戏或证明玩法通过。实际游玩从正常Main标题页“启程”打开已安装剧本，选择这份锁对应的“回气切磋 · 程禾”。若本地并存多个青川版本，先以安装清单中的 `qingchuan.study@1.5.0` 和完整锁摘要确认目标；菜单项的提示包含该锁的前12位。不要直接写世界快照、调用测试替身或用旧版同名项代替新包。

## 大整合节点的代表游玩输入

以下ID均为正常组合后带命名空间的ID；括号内命令用于对照实际控件入口，不是直接注入状态的替代路径。

1. 从正常Main菜单选择 `qingstudy::scenario.breath_practice` 与程禾 `qingstudy::actor.apprentice`，经外观确认进入剑庭。核对实际人物、剑庭和原画，内容锁必须属于1.5.0。
2. 当面与孟修交谈（`talk`，`role_id: qingstudy::role.sword_master_breath`），经 `dialogue_next` 到主选项，选择 `dialogue_choose` 的 `gear`。在柜台 `qingstudy::shop.sword_counter_breath` 正常购入一柄 `qingstudy::item.practice_sword`（`trade`，`side: buy`，`quantity: 1`），完成原交易半日，再由装备入口 `equip_weapon` 选择本人已购的剑。真实报价68钱，余额120→52，不补钱或注入装备。
3. 再找同一孟修交谈，读到新增的应敌说明，选择 `dialogue_choose` 的 `spar`。`dialogue_confirm` 前余额仍52；确认后余额44并进入两人切磋。目标必须是 `qingstudy::objective.breath_spar`，场地为 `qingstudy::field.sword_court`，对方为 `qingstudy::actor.sword_master`；开场半日与费用各结算一次，玩家初始气势仍50。
4. 通过实际个人战控件 `battle_action` 和 `battle_submit` 先原地 `wait` 一轮、普通 `defend` 一轮，再选择孟修、按合法范围靠近并 `attack`。这些控件提交原 `battle_plan`。观察对方实际走位/出手和本轮结算；等待、防御、命中与受击继续使用原回气规则。公开预览只说明玩家当前候选，不显示对方未来计划。
5. 继续从控件提交合法行动直到原规则形成终局，最多24轮。经结果页“返回原处，继续生活”（`battle_close`）关闭，确认回到同一剑庭、余额仍44；轮内和关闭时均不再收费或另推进世界半日。

这条输入用于下一次大整合的实际玩法验收，胜、负、平或合法撤离须按真实战斗结果记录，不能为追求指定结果调整人物或AI。作者/Schema/资源组合验证与实际游玩验收分开记录；本作者提交不宣称GUI、完整章节通关、存档迁移、性能或全量回归已通过。

## 2026-10-06作者检查点

与1.4.1的完整JSON差异仅为 `personal_objectives[1].ai_model` 和 `texts.stamina.sparring`；五个场景定义全部相同，manifest只改版本。五张PNG、来源说明与表现清单逐字节相同。正常compose通过，warnings与missing_resources均为空；安装层 `composed_set_record` 对这一完整组合代的核验通过。新锁为 `8956ea2e3522766679b63f8bc381508fd060f703280d1040c69a12af3dcd2ec3`，目录内容摘要为 `29119a255dbaea8fbe62d2f2672e95e5c4da11e5ab5267195eab9122365d8ed6`。

本隔离源码工作树的完整 `build.py stage` 被既有全局资源缺口 `game/assets/art/alpha-personal/river-ford.png` 阻断；失败发生在全局表现预检，未安装新包。该图不属于本包的五张资源；完整原资源在整合节点另行提供。本检查点保留 `runtime_verified: false` 与 `pilot_acceptance: false`，实际Main游玩尚未运行。
