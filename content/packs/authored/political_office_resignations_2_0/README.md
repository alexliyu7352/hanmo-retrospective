# 原任本人向NPC首长辞任

选择 political-office-resignations-2.0.0.json。本人始终为原在任粮务属吏许谦，NPC首长仍由原公平Director调度；无需收养或换控。

普通出口 scenario.qingchuan_npc_resignation：本人到原首府，结束会话后提出真实辞任申请。申请只保存意愿，职位不立即变动。本人继续在场并推进原半日，空闲且没有优先受领责任的原首长按显式18号策略重核原任期和专项Grant，再实际批准。只有原Office失效writer成功后，原职才空缺、epoch加一。旧Task、债务和成员关系继续由原账处理。

拒绝出口 scenario.qingchuan_npc_retained_service：同一原流程，但作者要求本任满8半日。未满时NPC首长当面明确驳回并保存 MinimumServiceRemaining 来源；没有暗中解除公职。到期后本人可以新申请，系统从真实同epoch就任头求服务时长，不从人物年龄或今日Owner猜测。目录初始任期起点为本局tick0。

两种答复均要求申请人和同一原首长真实同在首府、无Action/会话占用且原epoch、Grant仍有效。离场、忙碌、换任或申请到期只具名等待或拒绝，不远程冒批准。已收申请与本人亲历答复才进入有限页。

原辞任1段字节保持。2版新增显式NPC来源，与玩家actual result永久来源互斥。作者没有预置申请、答复、Office失效、家庭关系或控制事实。当前只有域组件和源码准入结果，完整World链尚未运行。
