# 玩家生活安排的单一Main节点

本目录复用原 `main.tscn`、原生活/课程按钮helper与现有有限继续节点的稳定等待和物理银行基础。没有测试注入世界，也不创建第二stage。

`prepare_inputs.py --repo <中央源码> --output <新独占目录> --stage-game <已有唯一stage/game> --engine <固定4.7.2>` 正式组合local-society1.5，输出独立安装代与待绑定清单。准备不会修改stage或执行玩法；中央须保留所有既有安装代，再合入本安装代，不能用单包索引覆盖旧银行依赖。

最终冻结后，精确绑定 source/reviewed_source、native_source/build receipt、实际库、注册文件、descriptor、stage资源及全部driver字节。只有 `BOUND_REVIEWED_ARTIFACT_NOT_EXECUTED` 可运行。主driver继承现有activity continuation driver；两个文件都必须在原相对目录且同时绑定。不能只复制新脚本后漏掉基础driver。

正常 `funded` 路线：

1. `run_node.py --case funded --phase produce ...`：空标题选新章程禾；原县仓承接托付；实地查看旧宅、先生课程、粮行短工和通路，收集8步。课程逐步明确允许，工作保持未勾。手记显示六课时预计完成和原四半日托付期限冲突，实际调序把办理提醒放前；开始后必须停住。本人用原受托经费买货并交付，原报告成功回执才将同一提醒记为已办理，驱动仍停。再次明确预算才启动课程，一课后从系统菜单写manual-1。提醒不会代交付、领取原任务奖励或自行续行。
2. 生产进程成功退出后 `--phase consume-auto`：新进程空标题“继续地方旅途”读取原银行，核全世界/计划相同并静置不恢复许可。明确本次预算，继续原课到未授权工作停住；勾选并确认该步原条款，继续实际短工和自宅两半日休整。全程保持原银行字节。
3. 另一新进程 `--phase consume-manual`：同一原银行，课程用原按钮逐半日完成，随后以相同原条款继续工作/休整。核原钱包、库存、人物、行动和facts，不把计划驱动预算历史要求为相同。

独立 `busy` 路线只运行produce：零半日时原场所看过同一短工并勾选，返回集上、实际等候一半日，让原NPC谋生争用店家；开始move+work安排，核原忙闲规则停止且不发工资。不能按剧本名假定店家必忙，实际不同则节点失败并保留原回执。此分支无存档消费者。

目前这些是准备好的流程和断言，不是已运行的证据。静态compose、控件check-only、UI单元门、原生规则门、真实Main与GUI分别记录。只有真实视口可生成图像；GUI像素尚须独立观察，不把截图文件存在当作体验验收。

## 青川1.6恢复分支

只有显式 `--chapter-profile qingchuan_region` 才选择新1.6目录和区域driver；省略时仍是原1.5 `player_life`，原funded/busy路线不变。准备与启动两处都必须指定相同profile。新分支仅有 `fulfill`、`course_first` 两case，以及 `produce`、`consume-auto` 两阶段；不重新运行旧自动/手动矩阵。

- fulfill：正常标题选完整非默认 `medium_square_jaw` → 院前原介绍 → 原地图往松门，承接四粟24半日原约 → 回青川，粮行真实 `grain.talk` HitArea → 原货单用同合同预算买四粟 → 原 `grain.return` HitArea → 原路到松门实际报告 → 同合同只读事实回访。say页仍打开时经系统菜单保存，生产进程退出；新进程从空标题地方继续恢复原物理银行，严格核世界、原外貌/资源与会话后继续原对白。
- course_first：同正式初态与同成人代表 → 院前介绍 → 县仓四粟4半日原约 → 讲舍礼法六课，原Main首课后保存。新进程核整世界与外貌后，首次自动预算必须因本人真实未决邀请停住。沿原课程暂停按钮回院前，展开同一县府邀请条款并由本人明确婉拒，再回原讲舍恢复同action课程，按实际剩余课时重新确认预算。核原六课、180钱总费与120经验，再回院前查同合同的实际逾期、零交付、80钱采购金与8钱未付报酬退款，最后走原只读结果话题。

所有行动使用当前可见控件和原Main异步循环。忙碌只允许当面明确等一半日；仍忙或缺货就留下失败，不调NPC、库存或期限。粮行返程不在helper中预热，HitArea信号只证明功能路由，不能冒充GUI鼠标或像素验收。

每份新回执明确标记 `NEW_RECOVERY_RUN_NOT_ORIGINAL_RESTORED_PROOF`，并保存实际运行helper路径与SHA256。历史post-6c原银行与原日志失落，重建的流程和新存档不证明原银行恢复。提交检查点、Python语法与Godot check-only各自只证明对应范围；真实Main和fresh-title结果须另记。

原stage与prior-binding失落时，中央可用 `tools/activity_continuation_main_node/overlay_stage.py --fresh-recovery-stage --execute-reviewed-freeze` 初始化唯一新路径 `<本地路径>`。目标必须存在且完全为空，output必须全新，不提供伪造prior-binding。入口照常要求精确source/native/helper commit、成功原生build receipt、实际库SHA和正式prepare清单；先完整验源，再复制必要源/所选资产，以固定引擎headless editor仅生成新导入缓存，最后绑定每项实际字节。失败保全目录与原日志，不覆盖重试；旧player_life的既存stage、prior-binding和原导入验证路径不变。新导入缓存明确是本次重建，不能称恢复的原缓存。

已复核的单一UI读屏障修复可使用 `--reviewed-ui-continuation <JSON>` 继续原course_first银行；仅允许带明确producer-binding与attempt的qingchuan_region consume-auto。省略时仍严格禁止任何生产源码变化。关系文件精确字段为：`format=HM-QINGCHUAN-REVIEWED-UI-CONTINUATION/1`、`producer_binding_sha256`、`consumer_binding_sha256`、`producer_source_commit`、`consumer_source_commit`、`reviewed_source_commit`、`source_files`。最后一项只能是 `game/ui/resident_page_requests.gd` 到 `{before_sha256, after_sha256}` 的映射。新source必须与reviewed source一致；其余生产路径、原生、引擎、case、catalog、资源与导入均精确相同。四个既有stage测试助手仍允许独立helper更新，但固定路径及前后drivers摘要必须与inputs一致，不能用driver别名放行产品文件。此关系及助手差异写入launch/process和独立回执，不改原producer绑定或银行。
