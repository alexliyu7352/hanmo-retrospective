## 场所、对白、任务与事件共用的纯条件判断，不修改世界状态。
extends RefCounted
const Location=preload("res://core/world_location.gd")

## 按人物与委托ID读取最新任务实例，不依赖固定任务名称。
static func task_for(state: Dictionary, actor_id: String, offer_id: String) -> Dictionary:
	var latest: Dictionary = {}
	for task in state.tasks.values():
		if task.assignee==actor_id and task.offer_id==offer_id and (latest.is_empty() or task.serial>latest.serial):
			latest=task
	return latest

## 将真实任务事实映射到作者使用的状态条件。
static func task_status(state: Dictionary, actor_id: String, offer_id: String) -> String:
	var task: Dictionary = task_for(state,actor_id,offer_id)
	if not task.is_empty():
		match task.status:
			"success": return "success"
			"failed": return "failed"
			"cancelled": return "known" if state.knowledge.get(actor_id,{}).has(offer_id) and can_retry_acceptance(state,task) else "failed"
			"ready": return "ready"
			_: return "active"
	return "known" if state.knowledge.get(actor_id,{}).has(offer_id) else "unknown"

## 判断已通过结构校验的条件，不改变世界或消费随机数。
static func evaluate(state: Dictionary, actor_id: String, condition: Dictionary) -> bool:
	match condition.get("op",""):
		"flag": return state.flags.get(condition.flag)!=null and state.flags[condition.flag]==condition.equals
		"party_member":
			var party_id: String=Location.party_id(state,actor_id)
			return (not party_id.is_empty() and condition.actor_id in state.parties[party_id].members)==condition.equals
		"actor_at":
			var actor: Dictionary = state.actors.get(condition.actor_id,{})
			return not actor.is_empty() and actor.alive and Location.resolve(state,condition.actor_id).get("place_id","")==condition.place_id
		"task_state": return task_status(state,actor_id,condition.offer_id)==condition.state
		"relation_min":
			for edge in state.relations.values():
				if edge.from_actor==condition.from_actor and edge.to_actor==condition.to_actor:
					return edge.trust>=condition.trust
			return condition.trust==0
		"all": return all_true(state,actor_id,condition.terms)
		"any":
			for term in condition.terms:
				if evaluate(state,actor_id,term): return true
			return false
	return false

## 将条件数组按逻辑与判断；空数组表示没有附加限制。
static func all_true(state: Dictionary, actor_id: String, conditions: Array) -> bool:
	for condition in conditions:
		if not condition is Dictionary or not evaluate(state,actor_id,condition): return false
	return true

## 只认从未拨款的受领中断；已受领的零预算私信、主动取消或留存产权均不能重开。
static func _unfunded_interruption(state: Dictionary, task: Dictionary) -> bool:
	if task.get("status")!="cancelled" or task.get("accepted_tick")!=0 or task.get("deadline_tick")!=0 or task.get("settlement_id")!="": return false
	var owner: Dictionary={"kind":"task","id":task.id}
	var wallet: Dictionary=state.wallets.get(task.budget_wallet_id,{})
	if wallet.get("owner")!=owner or wallet.get("balance")!=0 or not wallet.get("reserved",{}).is_empty(): return false
	for inventory in state.inventories.values():
		for lot in inventory.lots:
			if lot.owner==owner: return false
	for action in state.actions.values():
		if action.get("params",{}).get("task_id")==task.id: return false
	return true

## 同一受托人同一委托的全部旧实例都必须是无产权中断，较新空记录不能遮蔽已拨款合同。
static func can_retry_acceptance(state: Dictionary, task: Dictionary) -> bool:
	if not _unfunded_interruption(state,task): return false
	for prior in state.tasks.values():
		if prior.assignee==task.assignee and prior.offer_id==task.offer_id and not _unfunded_interruption(state,prior): return false
	return true
