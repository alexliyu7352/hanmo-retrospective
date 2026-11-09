## 唯一任务实例校验；由Schema在原全局校验位置调用，不新增结算或实时权限约束。
extends RefCounted
const Values=preload("res://core/base/value_checks.gd")
const Escort=preload("res://core/escort_rules.gd")

## 按原迭代及失败顺序检查字段、状态、预算归属与护卫事实；settlement_id只保持String要求。
static func validate(state: Dictionary, data: Dictionary) -> Dictionary:
	for id in state.tasks:
		var task: Variant = state.tasks[id]
		if not Values.fields(task,{"id":TYPE_STRING,"offer_id":TYPE_STRING,"assignee":TYPE_STRING,"serial":TYPE_INT,"status":TYPE_STRING,"budget_wallet_id":TYPE_STRING,"accepted_tick":TYPE_INT,"deadline_tick":TYPE_INT,"settlement_id":TYPE_STRING}) or task.id!=id or not data.offers.has(task.offer_id) or not state.actors.has(task.assignee): return {"ok":false,"code":"InvalidTask"}
		if not task.status in ["accepting","active","ready","success","failed","cancelled"] or not Values.whole(task.serial,1) or not Values.whole(task.accepted_tick) or not Values.whole(task.deadline_tick): return {"ok":false,"code":"InvalidTaskStatus"}
		if not state.wallets.has(task.budget_wallet_id) or state.wallets[task.budget_wallet_id].owner!={"kind":"task","id":id}: return {"ok":false,"code":"TaskBudgetOwnership"}
		if data.offers[task.offer_id].template=="escort":
			var escort_status: Dictionary=Escort.validate_task(state,data,task)
			if not escort_status.ok: return escort_status
	return {"ok":true}
