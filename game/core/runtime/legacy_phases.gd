## 发行组合根拥有的旧阶段桥；保持既有领域调用、引用寿命与先后失败，不定义新规则。
extends RefCounted
const SiegeWorld=preload("res://core/siege_world.gd")
const MilitaryContact=preload("res://core/military_contact.gd")
const Military=preload("res://core/military_world.gd")
const MilitaryAssets=preload("res://core/military_assets.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const MilitaryCombat=preload("res://core/military_combat.gd")
const Combat=preload("res://core/combat_session.gd")
const Contact=preload("res://core/road_contact.gd")
const Encounters=preload("res://core/encounter_rules.gd")
const Travel = preload("res://core/travel_rules.gd")
const Services = preload("res://core/service_rules.gd")
const Cultivation=preload("res://core/cultivation_rules.gd")
const Social=preload("res://core/social_rules.gd")
const Estate=preload("res://core/estate_rules.gd")
const Family=preload("res://core/family_rules.gd")
const Rest=preload("res://core/long_rest.gd")

const MilitaryCoordination=preload("res://core/domains/military/military_coordination.gd")
const ActionRuntime=preload("res://core/runtime/action_runtime.gd")
const Step=preload("res://core/world_step.gd")
const PersonalDelay=preload("res://core/domains/personal/personal_defeat_delay.gd")
const Location=preload("res://core/world_location.gd")
const March=preload("res://core/domains/military/march_conditions.gd")

## 为17个真实块声明相邻依赖；保守写覆盖不改变状态拥有权，也不隐含排序边。
static func descriptors() -> Array:
	return [
		{"id":"preflight","owner":"builtin","before":["military_supply"],"after":[],"reads":["state","context.hold","context.to_tick"],"writes":["context.hold"],"run":preflight},
		{"id":"military_supply","owner":"builtin","before":["siege_step"],"after":["preflight"],"reads":["state","context.to_tick"],"writes":["state.inventories","state.military_units","state.actors","state.garrisons","state.armies","state.facts","state.next_instance_id"],"run":military_supply},
		{"id":"siege_step","owner":"builtin","before":["military_battle"],"after":["military_supply"],"reads":["state","context.to_tick"],"writes":["state.sieges","state.inventories","state.facts","state.next_instance_id","context._sieges"],"run":siege_step},
		{"id":"military_battle","owner":"builtin","before":["battle_actor_changes"],"after":["siege_step"],"reads":["state","context.to_tick"],"writes":["state.battles","state.military_units","state.armies","state.facts","state.next_instance_id","context._battles"],"run":military_battle},
		{"id":"battle_actor_changes","owner":"builtin","before":["siege_battles_finished"],"after":["military_battle"],"reads":["state","context._battles"],"writes":["state.actors","state.actions"],"run":battle_actor_changes},
		{"id":"siege_battles_finished","owner":"builtin","before":["estate_production"],"after":["battle_actor_changes"],"reads":["state","context._battles","context.to_tick"],"writes":["state.sieges"],"run":siege_battles_finished},
		{"id":"estate_production","owner":"builtin","before":["contact_detect"],"after":["siege_battles_finished"],"reads":["state","context.hold","context.to_tick"],"writes":["state.estates","state.actors","state.inventories","state.facts","state.next_instance_id"],"run":estate_production},
		{"id":"contact_detect","owner":"builtin","before":["actions_step"],"after":["estate_production"],"reads":["state","context.hold"],"writes":["context.contacts","context.military_contacts","context._contacted"],"run":contact_detect},
		{"id":"actions_step","owner":"builtin","before":["contact_install"],"after":["contact_detect"],"reads":["state","context._actions","context.hold","context._contacted","context.to_tick","context.outcomes","context.family_completed","context.military_returns","context.suspended_actors"],"writes":["state","context.hold","context._contacted","context._military_executions","context.outcomes","context.family_completed","context.military_returns","context.suspended_actors"],"run":actions_step},
		{"id":"contact_install","owner":"builtin","before":["social_step"],"after":["actions_step"],"reads":["state","context.contacts","context.military_contacts","context._military_executions"],"writes":["state.actors","state.actions","state.encounters","state.facts","state.battles","state.military_units","state.next_instance_id"],"run":contact_install},
		{"id":"social_step","owner":"builtin","before":["siege_results"],"after":["contact_install"],"reads":["state","context.to_tick"],"writes":["state.social","state.relations","state.inventories","state.next_instance_id"],"run":social_step},
		{"id":"siege_results","owner":"builtin","before":["campaign_step"],"after":["social_step"],"reads":["state","context._sieges","context.to_tick"],"writes":["state","context._receipts","context._siege_journeys"],"run":siege_results},
		{"id":"campaign_step","owner":"builtin","before":["release_invalid_commands"],"after":["siege_results"],"reads":["state","context._receipts","context.to_tick"],"writes":["state.campaigns","state.facts","state.next_instance_id","context.journey_requests"],"run":campaign_step},
		{"id":"release_invalid_commands","owner":"builtin","before":["clock_commit"],"after":["campaign_step"],"reads":["state","context.journey_requests","context._siege_journeys","context.to_tick"],"writes":["state","context.journey_requests"],"run":release_invalid_commands},
		{"id":"clock_commit","owner":"builtin","before":["military_return_complete"],"after":["release_invalid_commands"],"reads":["state","context.to_tick"],"writes":["state.tick"],"run":clock_commit},
		{"id":"military_return_complete","owner":"builtin","before":["family_complete"],"after":["clock_commit"],"reads":["state","context.military_returns"],"writes":["state.campaigns","state.facts","state.next_instance_id"],"run":military_return_complete},
		{"id":"family_complete","owner":"builtin","before":[],"after":["military_return_complete"],"reads":["state","context.family_completed","context.outcomes"],"writes":["state","context.outcomes"],"run":family_complete},
	]

## 先检查个人战暂停、军事计划和围城计划；成功后独立克隆调用者hold。
static func preflight(state: Dictionary, data: Dictionary, context: Dictionary) -> Dictionary:
	if Combat.pauses_world(state): return {"ok":false,"code":"BattlePending"}
	var ready: Dictionary=MilitaryCombat.preview_step(state,data,context.to_tick)
	if not ready.ok: return ready
	ready=SiegeWorld.preview_step(state,data)
	if not ready.ok: return ready
	context.hold=context.hold.duplicate()
	for person in Military.held_actors(state):
		if not person in context.hold: context.hold.append(person)
	for person in PersonalDelay.held_travel_actors(state,data):
		if not person in context.hold: context.hold.append(person)
	return {"ok":true}

## 按旧时钟结算本半日军粮；保留领域首错。
static func military_supply(state: Dictionary, data: Dictionary, context: Dictionary) -> Dictionary:
	var supply: Dictionary=MilitaryAssets.step(state,data,context.to_tick)
	if not supply.ok: return supply
	return {"ok":true}

## 推进旧围城并保留其战场请求和事件，不提前安装结果。
static func siege_step(state: Dictionary, data: Dictionary, context: Dictionary) -> Dictionary:
	context._sieges=SiegeWorld.step(state,data,context.to_tick)
	if not context._sieges.ok: return context._sieges
	return {"ok":true}

## 推进已有战场；新建战场仍由原领域日期守卫跳过。
static func military_battle(state: Dictionary, data: Dictionary, context: Dictionary) -> Dictionary:
	context._battles=MilitaryCombat.step(state,data,context.to_tick)
	if not context._battles.ok: return context._battles
	return {"ok":true}

## 立即应用战场返回的人物变更，不持有跨阶段演员容器引用。
static func battle_actor_changes(state: Dictionary, data: Dictionary, context: Dictionary) -> Dictionary:
	Military.apply_actor_changes(state,context._battles.actor_changes,data)
	return {"ok":true}

## 旧战场完成回执先进入围城节点，再执行其他生产。
static func siege_battles_finished(state: Dictionary, data: Dictionary, context: Dictionary) -> Dictionary:
	var nodes: Dictionary=SiegeWorld.battles_finished(state,data,context._battles.completed_battle_ids,context.to_tick)
	if not nodes.ok: return nodes
	return {"ok":true}

## 行动占用释放前执行庄务生产和口粮，hold沿用同一克隆。
static func estate_production(state: Dictionary, data: Dictionary, context: Dictionary) -> Dictionary:
	var production: Dictionary=Estate.step(state,data,context.to_tick,context.hold)
	if not production.ok: return production
	return {"ok":true}

## 按普通、军事顺序检测接触，保留行动前快照与联合端点集合。
static func contact_detect(state: Dictionary, data: Dictionary, context: Dictionary) -> Dictionary:
	context.contacts=Contact.detect(state,data,context.hold)
	context.military_contacts=MilitaryContact.detect(state,data,context.hold)
	context._contacted={}
	for contact in context.contacts+context.military_contacts:
		context._contacted[contact.a]=true; context._contacted[contact.b]=true
	return {"ok":true}

## 此刻才快照排序行动ID；注册优先，旧特殊生命周期仍显式分支。
static func actions_step(state: Dictionary, data: Dictionary, context: Dictionary) -> Dictionary:
	var ids: Array=state.actions.keys(); ids.sort()
	for id in ids:
		var action: Dictionary=state.actions[id]
		var completed: bool=false
		var outcome: Dictionary={"ok":true,"success":true}
		if context._actions.has(action.kind):
			var stepped: Dictionary=ActionRuntime.step(state,data,action,context._actions,{"finish_tick":context.to_tick,"hold":context.hold,"contacted":context._contacted})
			if not stepped.ok: return stepped
			if stepped.suspended:
				context.suspended_actors.append(action.actor_id)
				continue
			completed=stepped.completed
			outcome=stepped.outcome
		elif action.kind=="travel":
			if action.actor_id in context.hold or not Travel.can_step(state,data,action).ok:
				context.suspended_actors.append(action.actor_id)
				continue
			var capture: bool=_capture_march(state,data,action.actor_id)
			outcome=Travel.step(state,data,action,capture)
			if not outcome.ok: return outcome
			var execution: Dictionary=outcome.get("execution",{})
			outcome.erase("execution")
			if not execution.is_empty():
				if context._contacted.has(action.actor_id):
					if not context.has("_military_executions"): context._military_executions={}
					context._military_executions[action.actor_id]=execution
				else:
					var marched: Dictionary=March.apply_execution(state,data,execution,context.to_tick)
					if not marched.ok: return marched
			completed=outcome.completed and not context._contacted.has(action.actor_id)
		elif action.kind=="long_rest":
			outcome=Rest.step(state,data,action)
			if not outcome.ok: return outcome
			completed=outcome.completed
		else:
			if not action.kind in ["service","estate_build","family_adopt","rescue","wait"]+Cultivation.TRAINING_KINDS: return {"ok":false,"code":"UnsupportedAction"}
			action.remaining_ticks-=1
			completed=action.remaining_ticks==0
			if completed:
				if action.kind=="service": outcome=Services.finish(state,action)
				elif action.kind in Cultivation.TRAINING_KINDS: outcome=Cultivation.finish(state,data,action)
				elif action.kind=="estate_build": outcome=Estate.finish(state,data,action)
				elif action.kind=="rescue": Travel.finish_rescue(state,data,action)
				if not outcome.ok: return outcome
		if completed:
			var coordinated: Dictionary=MilitaryCoordination.after_action(state,data,action,outcome,context)
			if not coordinated.ok: return coordinated
			if action.kind=="family_adopt":
				context.family_completed.append(id)
				continue
			context.outcomes[id]=outcome
			Step.release(state,action)
	return {"ok":true}

## 显式新剧本且由真实在役军团统帅行旅时才请求执行票据；其他人物和旧内容保持原出口。
static func _capture_march(state: Dictionary, data: Dictionary, actor_id: String) -> bool:
	if not March.enabled(state,data): return false
	var id: String=Location.army_id(state,actor_id)
	var army: Dictionary=state.get("armies",{}).get(id,{})
	return army.get("status")=="active" and army.get("commander_actor_id")==actor_id

## 普通路遇先安装，军事接触后安装；后者可替换整个状态容器。
static func contact_install(state: Dictionary, data: Dictionary, context: Dictionary) -> Dictionary:
	for contact in context.contacts: Encounters.install(state,contact)
	for contact in context.military_contacts:
		var engaged: Dictionary=MilitaryContact.install(state,data,contact,context.get("_military_executions",{}))
		if not engaged.ok: return engaged
	return {"ok":true}

## 完成行动释放与接触安装后才执行社会推进。
static func social_step(state: Dictionary, data: Dictionary, context: Dictionary) -> Dictionary:
	var society: Dictionary=Social.step(state,data,context.to_tick)
	if not society.ok: return society
	return {"ok":true}

## 按原插入顺序创建战场、分组事件、结算围城并应用人物变化。
static func siege_results(state: Dictionary, data: Dictionary, context: Dictionary) -> Dictionary:
	context._receipts=[]; context._siege_journeys=[]
	for entry in context._sieges.battle_requests:
		var joined: Dictionary=SiegeWorld.create_battle(state,data,entry.siege_id,entry.request,context.to_tick)
		if not joined.ok: return joined
	var events_by_siege: Dictionary={}
	for entry in context._sieges.events:
		if not events_by_siege.has(entry.siege_id): events_by_siege[entry.siege_id]=[]
		events_by_siege[entry.siege_id].append(entry.event)
	for id in events_by_siege:
		var settled: Dictionary=SiegeWorld.finish_events(state,data,state.sieges[id],events_by_siege[id],context.to_tick)
		if not settled.ok: return settled
		context._receipts.append_array(settled.details.completed_receipt_ids)
		context._siege_journeys.append_array(settled.details.journey_requests)
		Military.apply_actor_changes(state,settled.details.actor_changes,data)
	return {"ok":true}

## 围城回执进入军令推进，保留返回行程数组原引用。
static func campaign_step(state: Dictionary, data: Dictionary, context: Dictionary) -> Dictionary:
	var campaigns: Dictionary=Campaign.step(state,data,context.to_tick,{"completed_receipt_ids":context._receipts})
	if not campaigns.ok: return campaigns
	context.journey_requests=campaigns.journey_requests
	return {"ok":true}

## 失效军职释放成功后才追加围城返程，失败不暴露部分成功。
static func release_invalid_commands(state: Dictionary, data: Dictionary, context: Dictionary) -> Dictionary:
	var released: Dictionary=Military.release_invalid_commands(state,data,context.to_tick)
	if not released.ok: return released
	context.journey_requests.append_array(context._siege_journeys)
	return {"ok":true}

## 唯一世界时钟写入；之前领域仍观察旧tick。
static func clock_commit(state: Dictionary, _data: Dictionary, context: Dictionary) -> Dictionary:
	state.tick+=1
	return {"ok":true}

## 行动已释放的返营在新时钟确认军令，不复制交接明细。
static func military_return_complete(state: Dictionary, data: Dictionary, context: Dictionary) -> Dictionary:
	for details in context.military_returns:
		var returned: Dictionary=Campaign.army_returned(state,data,details.order_id,details.receipt_id)
		if not returned.ok: return returned
	return {"ok":true}

## 新时钟下重新读取仍占用的收养行动，完成后才释放参与者。
static func family_complete(state: Dictionary, data: Dictionary, context: Dictionary) -> Dictionary:
	for id in context.family_completed:
		var outcome: Dictionary=Family.finish(state,data,state.actions[id])
		if not outcome.ok: return outcome
		context.outcomes[id]=outcome
		Step.release(state,state.actions[id])
	return {"ok":true}
