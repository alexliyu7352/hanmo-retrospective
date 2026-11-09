## 任务生命周期的窄适配器；任务规则与资产算法不在此复制，载荷校验由此唯一拥有。
extends RefCounted
const Tasks=preload("res://core/task_rules.gd")
const Values=preload("res://core/base/value_checks.gd")
const KINDS=["accept_offer","report_offer","deliver_message"]

## 深拷贝已核准报价和全部参数；不再次预览、分配ID或预留资产。
static func prepare(_state: Dictionary, _data: Dictionary, actor_id: String, _kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
	var participants: Array=[actor_id]
	if not offer.context.provider_actor_id in participants: participants.append(offer.context.provider_actor_id)
	var copied: Dictionary=params.duplicate(true)
	copied.context=offer.context.duplicate(true)
	return {"ok":true,"participants":participants,"duration":offer.duration,"params":copied}

## 共享runtime先建立action及占用，再按原顺序建立task或预留交接资源。
static func start(state: Dictionary, data: Dictionary, action: Dictionary, offer: Dictionary) -> Dictionary:
	return Tasks.begin(state,data,action.actor_id,action.kind,action.params,offer,action)

## 办理任务不受旅行hold/contact暂停影响；只减一次剩余时间，不提前完成或释放。
static func step(_state: Dictionary, _data: Dictionary, action: Dictionary, _context: Dictionary) -> Dictionary:
	action.remaining_ticks-=1
	return {"ok":true,"completed":action.remaining_ticks==0,"outcome":{"ok":true,"success":true},"suspended":false}

## 保留原拨款、产权、冻结报酬及success=false不可用结果，共用Step负责完成释放。
static func finish(state: Dictionary, data: Dictionary, action: Dictionary, finish_tick: int) -> Dictionary:
	return Tasks.finish(state,data,action,finish_tick)

## 唯一旧任务载荷校验；保持错误优先级和旧档接受范围，公共envelope及占用仍归Schema。
static func validate(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	var params: Dictionary=action.params
	if not Values.fields(params,{"offer_id":TYPE_STRING,"task_id":TYPE_STRING,"context":TYPE_DICTIONARY}) or not state.tasks.has(params.task_id) or not data.offers.has(params.offer_id): return {"ok":false,"code":"InvalidTaskAction"}
	var ctx: Dictionary=params.context
	if not Values.fields(ctx,{"place_id":TYPE_STRING,"provider_actor_id":TYPE_STRING,"role_id":TYPE_STRING}) or not data.places.has(ctx.place_id) or not state.actors.has(ctx.provider_actor_id) or not data.roles.has(ctx.role_id) or not ctx.provider_actor_id in action.participants: return {"ok":false,"code":"InvalidActionContext"}
	var task: Dictionary=state.tasks[params.task_id]
	var definition: Dictionary=data.offers[params.offer_id]
	if task.offer_id!=definition.id or task.assignee!=action.actor_id: return {"ok":false,"code":"ActionTaskMismatch"}
	if action.kind=="accept_offer":
		if task.status!="accepting" or action.remaining_ticks>definition.accept_ticks or state.wallets[definition.funding_wallet].reserved.get(action.id)!=definition.budget: return {"ok":false,"code":"AcceptanceEscrowMismatch"}
	elif action.kind=="report_offer":
		if not task.status in ["active","ready"] or not Values.whole(params.get("reward"),0,definition.reward) or action.remaining_ticks!=1 or state.wallets[definition.funding_wallet].reserved.get(action.id)!=params.reward: return {"ok":false,"code":"ReportEscrowMismatch"}
		if definition.template=="procurement" and _reserved_count(state,action,definition.goal.item_id)!=definition.goal.quantity: return {"ok":false,"code":"ReportGoodsMismatch"}
		if definition.template=="message" and task.status!="ready": return {"ok":false,"code":"UndeliveredMessageReport"}
		if definition.template=="escort" and (task.status!="ready" or (not task.escort.failure.is_empty() and params.reward!=0)): return {"ok":false,"code":"InvalidEscortReport"}
	else:
		if definition.template!="message" or task.status!="active" or action.remaining_ticks!=1 or ctx.provider_actor_id!=definition.goal.receiver_actor or _reserved_count(state,action,definition.goal.message_id)!=1: return {"ok":false,"code":"InvalidMessageAction"}
	return {"ok":true}

## 只统计当前合同交付行动实际预留的物资批次。
static func _reserved_count(state: Dictionary, action: Dictionary, item_id: String) -> int:
	var total: int=0
	for lot in state.inventories[state.actors[action.actor_id].inventory_id].lots:
		if lot.item_id==item_id and lot.reserved_by==action.id and lot.owner in [{"kind":"task","id":action.params.task_id},{"kind":"actor","id":action.actor_id}]: total+=lot.quantity
	return total

