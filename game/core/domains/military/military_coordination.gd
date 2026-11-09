## 军事行动完成后的显式跨域协调；规则结算仍归原模块，新时钟归营确认仍归原独立阶段。
extends RefCounted
const SiegeWorld=preload("res://core/siege_world.gd")
const Travel=preload("res://core/travel_rules.gd")
const Assets=preload("res://core/military_assets.gd")
const Military=preload("res://core/military_world.gd")
const Campaign=preload("res://core/campaign_rules.gd")

## 共用完成出口按原顺序应用真人变化和编组绑定；归营只收集回执，绝不提前确认或释放占用。
static func after_action(state: Dictionary, data: Dictionary, action: Dictionary, outcome: Dictionary, context: Dictionary) -> Dictionary:
	if not outcome.ok: return outcome
	if not action.kind in Assets.ACTION_KINDS or not outcome.get("success",false): return {"ok":true}
	var details: Dictionary=outcome.details
	Military.apply_actor_changes(state,details.get("actor_changes",[]),data)
	if action.kind=="army_form":
		var bound: Dictionary=Campaign.bind_army(state,data,details.order_id,details.army_id)
		if not bound.ok: return bound
	elif action.kind=="army_return": context.military_returns.append(details)
	return {"ok":true}

## 注册军令完成后仍按原单数请求协调行旅；人产命令不附加结算，失败原样向外回滚。
static func after_command(state: Dictionary, data: Dictionary, kind: String, outcome: Dictionary, begin_action: Callable) -> Dictionary:
	if not outcome.ok: return outcome
	if kind in Campaign.KINDS:
		var request: Dictionary=outcome.get("details",{}).get("journey_request",{})
		if not request.is_empty():
			var moved: Dictionary=journey(state,data,request,begin_action)
			if not moved.ok: return moved
	var combat: Dictionary=after_combat_command(state,data,kind,outcome)
	return after_siege_command(state,data,kind,combat,begin_action)

## 军令只请求真实行旅；规划、重定向和实际开工各委托旧入口，不复制供给或地形算法。
static func journey(state: Dictionary, data: Dictionary, request: Dictionary, begin_action: Callable) -> Dictionary:
	var army: Dictionary=state.armies[request.army_id]
	var person: String=army.commander_actor_id
	var destination: String=request.destination_settlement_id
	if Campaign.at_settlement(state,data,army.id,destination): return {"ok":true}
	var quote: Dictionary=Travel.plan(state,data,person,destination)
	if not quote.ok: return quote
	var action_id: String=state.actors[person].action_id
	if not action_id.is_empty():
		var action: Dictionary=state.actions[action_id]
		if action.kind!="travel": return {"ok":false,"code":"MilitaryCommanderBusy"}
		Travel.replace_plan(state,data,action,quote)
		return {"ok":true}
	return begin_action.call(state,person,"travel",{"destination":destination},quote)

## 会战结果先由原规则关闭；随后刷新围城AI，失败原样外抛以丢弃整个候选。
static func after_combat_command(state: Dictionary, data: Dictionary, kind: String, outcome: Dictionary) -> Dictionary:
	if not outcome.ok: return outcome
	if kind=="military_close":
		var refreshed: Dictionary=SiegeWorld.refresh_plans(state,data)
		if not refreshed.ok: return refreshed
	return outcome

## 围城先落人物变化，再按原数组次序办理行旅请求；任一失败原样抛出供整候选回滚。
static func after_siege_command(state: Dictionary, data: Dictionary, kind: String, outcome: Dictionary, begin_action: Callable) -> Dictionary:
	if not outcome.ok or not kind in SiegeWorld.KINDS: return outcome
	var details: Dictionary=outcome.get("details",{})
	Military.apply_actor_changes(state,details.get("actor_changes",[]),data)
	for request in details.get("journey_requests",[]):
		var moved: Dictionary=journey(state,data,request,begin_action)
		if not moved.ok: return moved
	return outcome
