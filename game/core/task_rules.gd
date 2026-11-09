## 用真实人物、资产和合同实例处理采购、送信与护卫；奖励只在授权对象当面清算时转移。
extends RefCounted
const Inventory = preload("res://core/inventory_rules.gd")
const Interaction = preload("res://core/interaction_rules.gd")
const Conditions = preload("res://core/condition_rules.gd")
const Factory = preload("res://core/world_factory.gd")
const Escort=preload("res://core/escort_rules.gd")

## 按已知来源、在场人员、代理权限与资源核验受托和交接，失败不预扣费用。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	if not params.get("offer_id") is String or not data.offers.has(params.offer_id): return {"ok":false,"code":"UnknownOffer"}
	var definition: Dictionary=data.offers[params.offer_id]
	var service: String="report_offer" if kind=="cancel_task" else kind
	var checked: Dictionary=Interaction.validate(state,data,actor_id,params.get("context"),service)
	if not checked.ok: return checked
	var ctx: Dictionary=checked.context
	var task: Dictionary=Conditions.task_for(state,actor_id,definition.id)
	var actor: Dictionary=state.actors[actor_id]
	if kind=="deliver_message":
		if definition.template!="message" or ctx.provider_actor_id!=definition.goal.receiver_actor: return {"ok":false,"code":"WrongRecipient"}
	else:
		var expected: String=definition.accept_role if kind=="accept_offer" else definition.report_role
		if ctx.role_id!=expected: return {"ok":false,"code":"RoleNotAuthorized"}
		if definition.issuer.kind=="actor" and ctx.provider_actor_id!=definition.issuer.id: return {"ok":false,"code":"PrivatePromiseNotDelegated"}
	if kind=="accept_offer":
		if not state.knowledge[actor_id].has(definition.id): return {"ok":false,"code":"SourceUnknown"}
		if not task.is_empty() and not Conditions.can_retry_acceptance(state,task): return {"ok":false,"code":"OfferAlreadyAccepted"}
		if Inventory.spendable(state.wallets[definition.funding_wallet])<definition.budget: return {"ok":false,"code":"IssuerInsufficientFunds"}
		if definition.template=="message" and Inventory.weight(state.inventories[actor.inventory_id],data)+data.items[definition.goal.message_id].weight>state.inventories[actor.inventory_id].capacity: return {"ok":false,"code":"InsufficientCapacity"}
		if definition.template=="escort":
			var available: Dictionary=Escort.can_accept(state,data,actor_id,definition,ctx)
			if not available.ok: return available
		return {"ok":true,"duration":definition.accept_ticks,"context":ctx}
	if task.is_empty() or not task.status in ["active","ready"]: return {"ok":false,"code":"TaskNotActive"}
	if kind=="cancel_task": return {"ok":true,"duration":0,"task_id":task.id,"context":ctx}
	var escort_failed: bool=definition.template=="escort" and not task.escort.failure.is_empty()
	if state.tick+1>task.deadline_tick+2 and not escort_failed: return {"ok":false,"code":"OfferExpired"}
	var owner: Dictionary={"kind":"task","id":task.id}
	if kind=="deliver_message":
		if task.status!="active" or Inventory.count(state.inventories[actor.inventory_id],definition.goal.message_id,[owner])!=1: return {"ok":false,"code":"NoUndeliveredMessage"}
		var recipient_bag: Dictionary=state.inventories[state.actors[ctx.provider_actor_id].inventory_id]
		if Inventory.weight(recipient_bag,data)+data.items[definition.goal.message_id].weight>recipient_bag.capacity: return {"ok":false,"code":"RecipientCapacity"}
	else:
		if definition.template=="procurement":
			if definition.goal.receiver_role!=ctx.role_id: return {"ok":false,"code":"DifferentDeliveryOffice"}
			if Inventory.count(state.inventories[actor.inventory_id],definition.goal.item_id,[owner,{"kind":"actor","id":actor_id}])<definition.goal.quantity: return {"ok":false,"code":"InsufficientGoods"}
			var receiving: Dictionary=state.inventories[Inventory.receiver_inventory(state,definition.issuer)]
			if Inventory.weight(receiving,data)+definition.goal.quantity*data.items[definition.goal.item_id].weight>receiving.capacity: return {"ok":false,"code":"RecipientCapacity"}
		elif task.status!="ready": return {"ok":false,"code":"EscortNotArrived" if definition.template=="escort" else "MessageNotDelivered"}
	var reward: int=0 if escort_failed else definition.reward
	if state.tick+1>task.deadline_tick:
		@warning_ignore("integer_division")
		reward=reward/2
	if kind=="report_offer" and Inventory.spendable(state.wallets[definition.funding_wallet])<reward: return {"ok":false,"code":"IssuerInsufficientFunds"}
	return {"ok":true,"duration":1,"task_id":task.id,"context":ctx,"reward":reward}

## 开始办理时预留真实预算或交接物资，尚未履约不提前赋予成功。
static func begin(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, offer: Dictionary, action: Dictionary) -> Dictionary:
	var definition: Dictionary=data.offers[params.offer_id]
	if kind=="accept_offer":
		var serial: int=state.next_instance_id
		var task_id: String=Factory.next_id(state,"task")
		var wallet_id: String="budget:"+task_id
		state.tasks[task_id]={"id":task_id,"offer_id":definition.id,"assignee":actor_id,"serial":serial,"status":"accepting","budget_wallet_id":wallet_id,"accepted_tick":0,"deadline_tick":0,"settlement_id":""}
		state.wallets[wallet_id]={"id":wallet_id,"owner":{"kind":"task","id":task_id},"balance":0,"reserved":{}}
		state.wallets[definition.funding_wallet].reserved[action.id]=definition.budget
		action.params.task_id=task_id
	else:
		var task: Dictionary=state.tasks[offer.task_id]
		action.params.task_id=task.id
		if kind=="report_offer":
			action.params.reward=offer.reward
			state.wallets[definition.funding_wallet].reserved[action.id]=offer.reward
			if definition.template=="procurement":
				return Inventory.reserve(state,state.actors[actor_id].inventory_id,definition.goal.item_id,definition.goal.quantity,[{"kind":"task","id":task.id},{"kind":"actor","id":actor_id}],action.id)
		elif kind=="deliver_message":
			return Inventory.reserve(state,state.actors[actor_id].inventory_id,definition.goal.message_id,1,[{"kind":"task","id":task.id}],action.id)
	return {"ok":true}

## 在所属半日完成合同动作；在场或权限失效时释放预留并报告中断，避免卡住世界推进。
static func finish(state: Dictionary, data: Dictionary, action: Dictionary, finish_tick: int) -> Dictionary:
	var task: Dictionary=state.tasks[action.params.task_id]
	var definition: Dictionary=data.offers[task.offer_id]
	var checked: Dictionary=Interaction.validate(state,data,action.actor_id,action.params.context,action.kind,action.id)
	if not checked.ok: return unavailable_result(state,data,action,checked.code)
	var source_wallet: Dictionary=state.wallets[definition.funding_wallet]
	if action.kind=="accept_offer":
		source_wallet.balance-=definition.budget
		state.wallets[task.budget_wallet_id].balance+=definition.budget
		task.status="active"
		task.accepted_tick=finish_tick
		task.deadline_tick=finish_tick+definition.deadline_ticks
		if definition.template=="escort": Escort.begin(task)
		if definition.template=="message":
			# 合同只授权寄信人书写并交付一封密信；不能借此生成可售商品。
			Inventory.add(state,state.actors[action.actor_id].inventory_id,definition.goal.message_id,1,{"kind":"task","id":task.id})
	elif action.kind=="deliver_message":
		var recipient: String=definition.goal.receiver_actor
		var moved: Dictionary=Inventory.transfer(state,state.actors[action.actor_id].inventory_id,state.actors[recipient].inventory_id,definition.goal.message_id,1,[{"kind":"task","id":task.id}],action.id,{"kind":"actor","id":recipient})
		if not moved.ok: return moved
		task.status="ready"
		task["objective_completed_tick"]=finish_tick
	elif action.kind=="report_offer":
		if definition.template=="procurement":
			var moved: Dictionary=Inventory.transfer(state,state.actors[action.actor_id].inventory_id,Inventory.receiver_inventory(state,definition.issuer),definition.goal.item_id,definition.goal.quantity,[{"kind":"task","id":task.id},{"kind":"actor","id":action.actor_id}],action.id,definition.issuer)
			if not moved.ok: return moved
		var budget: Dictionary=state.wallets[task.budget_wallet_id]
		var escort_failed: bool=definition.template=="escort" and not task.escort.failure.is_empty()
		if escort_failed: source_wallet.balance+=budget.balance
		else: state.wallets[state.actors[action.actor_id].wallet_id].balance+=budget.balance+action.params.reward
		budget.balance=0
		source_wallet.balance-=action.params.reward
		task.status="failed" if escort_failed else "success"
		task.settlement_id=Factory.next_id(state,"settlement")
		task["reported_tick"]=finish_tick
		task["late"]=finish_tick>task.deadline_tick
		if not escort_failed: _remember_promise(state,action.params.context.provider_actor_id,action.actor_id)
	Inventory.release(state,action.id)
	return {"ok":true,"success":true,"task_id":task.id}

## 仅在合法回报对象处取消合同，归还剩余预算及任务所有物资。
static func cancel(state: Dictionary, data: Dictionary, task_id: String) -> Dictionary:
	var task: Dictionary=state.tasks[task_id]
	var definition: Dictionary=data.offers[task.offer_id]
	var budget: Dictionary=state.wallets[task.budget_wallet_id]
	state.wallets[definition.funding_wallet].balance+=budget.balance
	budget.balance=0
	var source: String=state.actors[task.assignee].inventory_id
	var destination: String=Inventory.receiver_inventory(state,definition.issuer)
	var owner: Dictionary={"kind":"task","id":task.id}
	for item_id in data.items:
		var quantity: int=Inventory.count(state.inventories[source],item_id,[owner])
		if quantity>0:
			var transferred: Dictionary=Inventory.transfer(state,source,destination,item_id,quantity,[owner],"",definition.issuer)
			if not transferred.ok: return transferred
	task.status="failed" if state.tick>task.deadline_tick else "cancelled"
	task.settlement_id=Factory.next_id(state,"settlement")
	return {"ok":true}

## 履约只增进实际交接人与受托人的关系，不把私人信任赋予抽象岗位或未来接任者。
static func _remember_promise(state: Dictionary, provider: String, actor_id: String) -> void:
	var selected: Dictionary={}
	for relation in state.relations.values():
		if relation.from_actor==provider and relation.to_actor==actor_id:
			selected=relation
			break
	if selected.is_empty():
		var id: String=Factory.next_id(state,"relation")
		selected={"id":id,"from_actor":provider,"to_actor":actor_id,"affinity":0,"trust":0}
		state.relations[id]=selected
	selected.trust=mini(100,selected.trust+4)
	selected.affinity=mini(100,selected.affinity+1)

## 已作者化策略只解释当前中断，释放本动作预留；绝不自动换人、取消已接受合同或发奖。
static func unavailable_result(state: Dictionary, data: Dictionary, action: Dictionary, code: String) -> Dictionary:
	var task: Dictionary=state.tasks[action.params.task_id]
	var definition: Dictionary=data.offers[task.offer_id]
	var policy: String=definition.get("on_unavailable","")
	if not policy in ["known_replacement","wait_or_leave"] or (policy=="known_replacement" and definition.issuer.kind!="organization"):
		return {"ok":false,"code":"UnsupportedTaskPolicy"}
	Inventory.release(state,action.id)
	if action.kind=="accept_offer": task.status="cancelled"
	return {"ok":true,"success":false,"code":code,"on_unavailable":policy,"retry":"fresh_authorized_context" if policy=="known_replacement" else "original_person_available"}
