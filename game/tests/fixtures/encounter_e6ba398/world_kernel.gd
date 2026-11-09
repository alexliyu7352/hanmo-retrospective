## 内容驱动世界的唯一提交入口；只有完整通过校验的候选状态才能替换真实世界。
extends RefCounted
const Registry=preload("res://core/runtime/module_registry.gd")
const TransactionContext=preload("res://core/runtime/transaction_context.gd")
const DomainObservers=preload("res://core/runtime/domain_observers.gd")
const Builtin=preload("res://tests/fixtures/encounter_e6ba398/builtin_modules.gd")
const ExplanationQuery=preload("res://core/runtime/explanation_query.gd")
const SiegeWorld=preload("res://core/siege_world.gd")
const MilitarySchedule=preload("res://core/military_schedule.gd")
const Military=preload("res://core/military_world.gd")
const MilitaryCoordination=preload("res://core/domains/military/military_coordination.gd")
const MilitaryCombat=preload("res://core/military_combat.gd")
const Combat=preload("res://core/combat_session.gd")
const Personal=preload("res://core/personal_combat.gd")
const Escort=preload("res://core/escort_rules.gd")
const Social=preload("res://core/social_rules.gd")
const Estate=preload("res://core/estate_rules.gd")
const Family=preload("res://core/family_rules.gd")
const Rest=preload("res://core/long_rest.gd")
const Cultivation=preload("res://core/cultivation_rules.gd")
const Encounters=preload("res://core/encounter_rules.gd")
const Schedule=preload("res://core/travel_schedule.gd")
const Parties=preload("res://core/party_rules.gd")
const Location=preload("res://core/world_location.gd")
const Rules = preload("res://core/rules_migration.gd")
const Catalog = preload("res://core/content_catalog.gd")
const Factory = preload("res://core/world_factory.gd")
const Schema = preload("res://core/state_schema.gd")
const Conditions = preload("res://core/condition_rules.gd")
const Interaction = preload("res://core/interaction_rules.gd")
const ActionRuntime=preload("res://core/runtime/action_runtime.gd")
const Inventory = preload("res://core/inventory_rules.gd")
const Travel = preload("res://core/travel_rules.gd")
const Step = preload("res://core/world_step.gd")
const Journey = preload("res://core/journey_commands.gd")
const Dialogue = preload("res://core/dialogue_runner.gd")
const Services = preload("res://core/service_rules.gd")
const Events = preload("res://core/event_rules.gd")
var _catalog: Dictionary = {}
var _state: Dictionary = {}
var _registry: Dictionary = {}

## 只提供发行代码绑定的描述；测试可继承替换以验证启动失败保留旧世界。
func _module_descriptors() -> Array:
	return Builtin.descriptors()

## 读取已准备内容并建立剧本；任一步失败均保留原有世界。
func new_game(scenario_id: String = "scenario.two_stories", content_path: String = "res://data/generated/two_stories.hmcatalog.json", appearance_draft: Dictionary={}) -> Dictionary:
	var registrations: Dictionary=Registry.build(_module_descriptors())
	if not registrations.ok: return registrations
	var command_contract: Dictionary=Builtin.command_contract_status(registrations)
	if not command_contract.ok: return command_contract
	var observer_contract: Dictionary=Builtin.observer_contract_status(registrations)
	if not observer_contract.ok: return observer_contract
	var loaded: Dictionary = Catalog.read_prepared(content_path)
	if not loaded.ok: return loaded
	var built: Dictionary = Factory.create(loaded,scenario_id,appearance_draft)
	if not built.ok: return built
	var estates: Dictionary=Estate.initialize(built.state,loaded.data)
	if not estates.ok: return estates
	var military: Dictionary=Military.initialize(built.state,loaded.data)
	if not military.ok: return military
	var valid: Dictionary = Schema.validate(built.state,loaded,registrations)
	if not valid.ok: return valid
	_catalog=loaded
	_state=built.state
	_registry=registrations
	return {"ok":true}

## 返回隔离快照，防止界面或测试通过共享引用篡改真实状态。
func snapshot() -> Dictionary:
	return _state.duplicate(true)

## 提供内容定义副本，供表现层和带版本校验的存储适配器读取。
func content() -> Dictionary:
	return _catalog.duplicate(true)

## 只读解释已提交状态；observer由受信调用方指定而非query授权，本入口不提供外部调用者身份认证。
func explain(query: Dictionary, observer_id: String="") -> Dictionary:
	if observer_id.is_empty(): observer_id=_state.get("player_actor_id","")
	return ExplanationQuery.explain(_state,_catalog.get("data",{}),query,observer_id)

## 仅恢复内容和引擎兼容的状态，不重放已经提交的消费或奖励。
func restore(saved: Dictionary) -> Dictionary:
	var upgrade: Dictionary=Rules.upgrade(saved)
	if not upgrade.ok: return upgrade
	saved=upgrade.state
	var valid: Dictionary = Schema.validate(saved,_catalog,_registry)
	if not valid.ok: return valid
	var build: String = str(Engine.get_version_info().get("string","unknown"))
	if saved.engine_build!=build or saved.rng_backend!="godot:"+build: return {"ok":false,"code":"IncompatibleEngine"}
	_state=saved.duplicate(true)
	return {"ok":true}

## 按当前版本执行纯查询预览，不消费随机数或预扣资源。
func preview(kind: String, params: Dictionary = {}, actor_id: String = "") -> Dictionary:
	if _state.is_empty(): return {"ok":false,"code":"NoWorld"}
	if actor_id.is_empty(): actor_id=_state.player_actor_id
	var result: Dictionary = _preview(_state,kind,params,actor_id)
	result["revision"]=_state.revision
	return result

## 原子提交带版本的命令；重复请求返回原结果，不再次执行。
func submit(command: Dictionary) -> Dictionary:
	if _state.is_empty(): return {"ok":false,"code":"NoWorld"}
	if not Schema.fields(command,{"kind":TYPE_STRING,"params":TYPE_DICTIONARY,"request_seq":TYPE_INT,"expected_revision":TYPE_INT}) or not Schema.whole(command.request_seq,1): return {"ok":false,"code":"InvalidCommand"}
	var actor_id: Variant = command.get("actor_id",_state.player_actor_id)
	if not actor_id is String or not _state.actors.has(actor_id): return {"ok":false,"code":"UnknownActor"}
	var key: String = actor_id+":"+str(command.request_seq)
	if command.request_seq<=_state.request_sequences[actor_id]:
		return _state.results.get(key,{"ok":false,"code":"AlreadyApplied"}).duplicate(true)
	if command.expected_revision!=_state.revision: return {"ok":false,"code":"StalePreview"}
	var offer: Dictionary = _preview(_state,command.kind,command.params,actor_id)
	if not offer.ok: return offer
	var transaction_context: Dictionary=TransactionContext.create(_state,command)
	if not transaction_context.ok: return transaction_context
	var candidate: Dictionary = _state.duplicate(true)
	var applied: Dictionary = _apply(candidate,command.kind,command.params,actor_id,offer)
	if not applied.ok: return applied
	var released: Dictionary=Military.release_invalid_commands(candidate,_catalog.data,candidate.tick)
	if not released.ok: return released
	Escort.observe(candidate,_catalog.data)
	candidate.revision+=1
	candidate.request_sequences[actor_id]=command.request_seq
	var result: Dictionary = {"ok":true,"kind":command.kind,"tick":candidate.tick,"revision":candidate.revision,"result_id":Factory.next_id(candidate,"result"),"details":applied.get("details",{})}
	candidate.results[key]=result.duplicate(true)
	while candidate.results.size()>64:
		var oldest: String=""
		var minimum: int=2147483647
		for cached_key in candidate.results:
			if candidate.results[cached_key].revision<minimum:
				minimum=candidate.results[cached_key].revision; oldest=cached_key
		candidate.results.erase(oldest)
	candidate.history.append({"tick":candidate.tick,"kind":command.kind,"actor_id":actor_id,"result_id":result.result_id})
	if candidate.history.size()>2000: candidate.history.pop_front()
	var reconciled: Dictionary=DomainObservers.reconcile(candidate,_catalog.data,transaction_context,_registry)
	if not reconciled.ok: return reconciled
	var checked: Dictionary = Schema.validate(candidate,_catalog,_registry)
	if not checked.ok: return checked
	_state=candidate
	return result

## 校验已支持命令及真实世界前提，不能仅相信界面是否显示按钮。
func _preview(state: Dictionary, kind: String, params: Dictionary, actor_id: String) -> Dictionary:
	var data: Dictionary = _catalog.data
	if not state.actors.has(actor_id): return {"ok":false,"code":"UnknownActor"}
	var actor: Dictionary = state.actors[actor_id]
	if _registry.commands.has(kind) and _registry.commands[kind].owner=="military" and _registry.commands[kind].gate_id=="military_combat_early":
		return _registry.commands[kind].preview.call(state,data,actor_id,kind,params)
	if _registry.commands.has(kind) and _registry.commands[kind].owner=="military" and _registry.commands[kind].gate_id=="military_siege_early":
		return _registry.commands[kind].preview.call(state,data,actor_id,kind,params)
	if kind in Personal.KINDS:
		if Combat.occupied(state,actor_id) and Combat.for_actor(state,actor_id).get("kind")=="military": return {"ok":false,"code":"BattlePending"}
		return Personal.preview(state,data,actor_id,kind,params)
	if Combat.pauses_world(state): return {"ok":false,"code":"BattlePending"}
	if Combat.occupied(state,actor_id) and kind not in ["advance","travel_wait"]: return {"ok":false,"code":"BattlePending"}
	# 同城军需交付仍走正式权限和实物校验；前面的会战占用检查禁止节点接战时插队。
	if not SiegeWorld.for_actor(state,actor_id).is_empty() and kind not in ["advance","travel_wait","campaign_deliver"]: return {"ok":false,"code":"SiegePending"}
	if actor.location.kind=="InArmy": return {"ok":false,"code":"NotArmyCommander"}
	if actor.location.kind=="Following": return {"ok":false,"code":"NotPartyLeader"}
	if kind in Encounters.KINDS: return Encounters.preview(state,data,actor_id,kind,params)
	var meeting: Dictionary=Encounters.pending_for(state,actor_id)
	if kind in ["advance","travel_wait"] and not Encounters.pending_for(state,state.player_actor_id).is_empty(): return {"ok":false,"code":"EncounterPending"}
	if not meeting.is_empty() and not kind in ["dialogue_next","dialogue_choose","end_interaction"]: return {"ok":false,"code":"EncounterPending"}
	var party_command: Dictionary=_registry.commands.get(kind,{})
	if party_command.get("owner")=="travel" and party_command.get("gate_id")=="party_before_actor" and kind in Parties.KINDS:
		return party_command.preview.call(state,data,actor_id,kind,params)
	if kind in Journey.KINDS: return Journey.preview(state,data,actor_id,kind,params)
	if kind=="advance":
		var military: Dictionary=MilitaryCombat.preview_step(state,data,state.tick+1)
		if not military.ok: return military
		var siege: Dictionary=SiegeWorld.preview_step(state,data)
		if not siege.ok: return siege
		if state.actions.is_empty() and military.battle_ids.is_empty() and siege.siege_ids.is_empty(): return {"ok":false,"code":"NoCurrentAction"}
		for action in state.actions.values():
			if action.actor_id==actor_id and action.kind=="travel" and not Combat.occupied(state,actor_id):
				var travel_check: Dictionary=Travel.can_step(state,data,action)
				if not travel_check.ok: return travel_check
		return {"ok":true,"duration":1}
	if kind=="rescue":
		if not Location.army_id(state,actor_id).is_empty(): return {"ok":false,"code":"ArmyNotReturned"}
		if actor.action_id.is_empty() or state.actions[actor.action_id].kind!="travel": return {"ok":false,"code":"NotTravelling"}
		var action: Dictionary=state.actions[actor.action_id]
		if Inventory.spendable(state.wallets[action.params.payer_wallet_id])>=Travel.next_cost(state,data,action): return {"ok":false,"code":"NotStranded"}
		return {"ok":true,"duration":2}
	if kind in Rest.KINDS:
		if not "family" in data.required_capabilities: return {"ok":false,"code":"FamilyUnavailable"}
		return Rest.preview(state,data,actor_id,kind,params)
	if not actor.alive or not actor.available: return {"ok":false,"code":"ActorUnavailable"}
	# 在真实行军途中允许原令返程，由同一地形规划重算余路；其他在办事务仍不能插队。
	var registered: Dictionary=_registry.commands.get(kind,{})
	if registered.get("owner")=="military" and registered.get("gate_id")=="campaign_travel_redirect" and not actor.action_id.is_empty() and state.actions[actor.action_id].kind=="travel":
		return registered.preview.call(state,data,actor_id,kind,params)
	if not actor.action_id.is_empty() and not (not meeting.is_empty() and kind in ["dialogue_next","dialogue_choose","end_interaction"] and state.interactions.has(actor_id) and state.interactions[actor_id].context.get("encounter_id")==meeting.id): return {"ok":false,"code":"ActorBusy"}
	if _registry.commands.has(kind): return _registry.commands[kind].preview.call(state,data,actor_id,kind,params)
	if kind in Estate.KINDS:
		if not "estates" in data.required_capabilities: return {"ok":false,"code":"EstatesUnavailable"}
		return Estate.preview(state,data,actor_id,kind,params)
	if kind in Family.KINDS:
		if not "family" in data.required_capabilities: return {"ok":false,"code":"FamilyUnavailable"}
		return Family.preview(state,data,actor_id,kind,params)
	match kind:
		"ask_news":
			if not "npc_society" in data.required_capabilities: return {"ok":false,"code":"SocialUnavailable"}
			var conversation: Dictionary=state.interactions.get(actor_id,{})
			if not conversation.get("phase","") in ["say","choice"]: return {"ok":false,"code":"NoConversation"}
			var valid: Dictionary=Interaction.validate(state,data,actor_id,conversation.context)
			if not valid.ok: return valid
			return {"ok":true,"duration":0,"context":valid.context}
		"talk": return Dialogue.can_begin(state,data,actor_id,params.get("role_id"))
		"open_event": return Events.preview(state,data,actor_id,params.get("event_id"))
		"dialogue_next","dialogue_choose": return Dialogue.validate_input(state,data,actor_id,kind,params)
		"dialogue_confirm":
			if not state.interactions.has(actor_id) or state.interactions[actor_id].phase!="action": return {"ok":false,"code":"NoPendingDialogueAction"}
			var session: Dictionary=state.interactions[actor_id]
			var request: Dictionary=session.pending_command.duplicate(true)
			request.context=session.context.duplicate(true)
			return _preview(state,request.kind,request,actor_id)
		"dialogue_cancel","close_trade":
			if not state.interactions.has(actor_id) or not state.interactions[actor_id].phase in ["action","trade"]: return {"ok":false,"code":"NoPendingDialogueAction"}
			return {"ok":true,"duration":0}
		"end_interaction": return {"ok":true,"duration":0}
		"open_trade":
			if not params.get("shop_id") is String or not data.shops.has(params.shop_id): return {"ok":false,"code":"UnknownShop"}
			var valid: Dictionary=Interaction.validate(state,data,actor_id,params.get("context"),"trade")
			if not valid.ok: return valid
			if not valid.context.role_id in data.shops[params.shop_id].representative_roles or valid.context.place_id!=data.shops[params.shop_id].place_id: return {"ok":false,"code":"WrongShopRepresentative"}
			return {"ok":true,"duration":0,"context":valid.context}
		"service": return Services.preview(state,data,actor_id,params)
		"travel": return Travel.plan(state,data,actor_id,params.get("destination"),params.get("via",[]))
		"wait": return {"ok":true,"duration":1}
		"move_local":
			var target: Variant = params.get("place_id")
			if not target is String or not data.places.has(target): return {"ok":false,"code":"UnknownPlace"}
			if actor.location.kind!="Settlement" or not target in data.places[actor.location.place_id].passages: return {"ok":false,"code":"NoLocalPassage"}
			if not Parties.available(state,actor_id): return {"ok":false,"code":"ParticipantBusy"}
			for member in Location.members(state,actor_id):
				if not Conditions.all_true(state,member,data.places[target].admission): return {"ok":false,"code":"AudienceDenied"}
			return {"ok":true,"duration":0}
		"inspect":
			var prop_id: Variant = params.get("prop_id")
			if not prop_id is String or not data.props.has(prop_id): return {"ok":false,"code":"UnknownProp"}
			var prop: Dictionary = data.props[prop_id]
			if actor.location.get("place_id")!=prop.place_id: return {"ok":false,"code":"NotAtPlace"}
			for offer_id in prop.offer_ids:
				var discovery: Dictionary = Interaction.discovery(state,data,actor_id,offer_id,params)
				if not discovery.ok: return discovery
			return {"ok":true,"duration":0}
		"discover_offer":
			if not params.get("offer_id") is String: return {"ok":false,"code":"InvalidOffer"}
			return Interaction.discovery(state,data,actor_id,params.offer_id,params)
	return {"ok":false,"code":"UnsupportedAction"}

## 仅在隔离候选状态执行已校验规则，动画和界面回调不直接写入事实。
func _apply(state: Dictionary, kind: String, params: Dictionary, actor_id: String, offer: Dictionary) -> Dictionary:
	if kind in Estate.KINDS:
		return _begin_action(state,actor_id,kind,params,offer) if kind=="estate_build" else Estate.apply(state,_catalog.data,actor_id,kind,offer)
	if kind in Family.KINDS:
		return _begin_action(state,actor_id,kind,params,offer) if kind=="family_adopt" else Family.apply(state,_catalog.data,actor_id,kind,offer)
	if kind=="long_rest": return _begin_action(state,actor_id,kind,params,offer)
	if kind=="rest_cancel":
		var cancelled: Dictionary=Rest.cancel(state,offer.action_id)
		Step.release(state,state.actions[offer.action_id])
		return cancelled
	if kind in Personal.KINDS:
		if kind in ["duel_start","encounter_fight"]:
			if offer.duration>0:
				var elapsed: Dictionary=_advance(state,offer.sides.keys())
				if not elapsed.ok: return elapsed
				var still_present: Dictionary=Personal.preview(state,_catalog.data,actor_id,kind,params)
				if not still_present.ok: return still_present
			return Personal.begin(state,_catalog.data,offer)
		return Personal.apply(state,_catalog.data,actor_id,kind,offer)
	if kind in Encounters.KINDS:
		var applied: Dictionary=Encounters.apply(state,_catalog.data,actor_id,kind,offer)
		return _drive_dialogue(state,actor_id) if applied.ok and kind=="encounter_talk" else applied
	if kind in Journey.KINDS:
		var changed: Dictionary=Journey.apply(state,_catalog.data,actor_id,kind,offer)
		return _advance(state,[actor_id]) if changed.wait else {"ok":true}
	if _registry.commands.has(kind):
		var registered: Dictionary=_registry.commands[kind]
		var applied: Dictionary
		if registered.mode=="action":
			applied=_begin_action(state,actor_id,kind,params,offer) if registered.get("compat_action")=="legacy_begin_action" else ActionRuntime.begin(state,_catalog.data,actor_id,kind,params,offer,_registry)
		else:
			applied=registered.apply.call(state,_catalog.data,actor_id,kind,params,offer)
		return MilitaryCoordination.after_command(state,_catalog.data,kind,applied,_begin_action) if registered.owner=="military" else applied
	match kind:
		"ask_news":
			var heard: Dictionary=Social.hear_news(state,_catalog.data,actor_id,offer.context.provider_actor_id,state.tick)
			return {"ok":true,"details":{"news_ids":heard.events}} if heard.ok else heard
		"talk","open_event":
			Dialogue.begin(state,_catalog.data,actor_id,offer)
			if kind=="open_event": Events.presented(state,actor_id,params.event_id)
			return _drive_dialogue(state,actor_id)
		"dialogue_next","dialogue_choose":
			Dialogue.advance(state,actor_id,offer.next)
			return _drive_dialogue(state,actor_id)
		"dialogue_confirm":
			var session: Dictionary=state.interactions[actor_id]
			var request: Dictionary=session.pending_command.duplicate(true)
			request.context=session.context.duplicate(true)
			var applied: Dictionary=_apply(state,request.kind,request,actor_id,offer)
			if not applied.ok: return applied
			if not state.interactions.has(actor_id): return applied
			if request.kind=="open_trade":
				session.phase="trade"; session.trade_completed=false
			elif applied.get("details",{}).has("action_id"):
				session.phase="waiting"; session.pending_action_id=applied.details.action_id
			else:
				Dialogue.resolve(state,_catalog.data,actor_id,{"ok":true})
				return _drive_dialogue(state,actor_id)
			return applied
		"dialogue_cancel","close_trade":
			Dialogue.resolve(state,_catalog.data,actor_id,{"ok":kind=="close_trade" and state.interactions[actor_id].trade_completed,"code":"Cancelled"})
			return _drive_dialogue(state,actor_id)
		"end_interaction":
			state.interactions.erase(actor_id)
			return {"ok":true}
		"open_trade": return {"ok":true}
		"advance": return _advance(state)
		"travel","wait","service":
			return _begin_action(state,actor_id,kind,params,offer)
		"rescue":
			var actor: Dictionary=state.actors[actor_id]
			var action: Dictionary=state.actions[actor.action_id]
			action.kind="rescue"; action.remaining_ticks=2; action.params={"origin":actor.location.origin}
			return {"ok":true}
		"move_local":
			state.actors[actor_id].location=Factory.settlement_location(_catalog.data,params.place_id)
			state.interactions.erase(actor_id)
			var entered: Dictionary=Events.on_enter(state,_catalog.data,actor_id)
			if not entered.ok: return entered
			return {"ok":true}
		"inspect":
			for offer_id in _catalog.data.props[params.prop_id].offer_ids:
				_remember(state,actor_id,offer_id,{"kind":"prop","id":params.prop_id})
			return {"ok":true}
		"discover_offer":
			_remember(state,actor_id,params.offer_id,offer.source)
			return {"ok":true}
	return {"ok":false,"code":"UnsupportedAction"}

## 记录第一次获知线索的来源，不同时受领任务或发放预算。
func _remember(state: Dictionary, actor_id: String, offer_id: String, source: Dictionary) -> void:
	if not state.knowledge[actor_id].has(offer_id):
		state.knowledge[actor_id][offer_id]={"source":source.duplicate(true),"tick":state.tick}

## 建立行动并占用实际参与者；全部不变量校验通过后才由调用者提交。
func _begin_action(state: Dictionary, actor_id: String, kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
	if not kind in ["travel","wait","service","estate_build","family_adopt","long_rest"]+Cultivation.TRAINING_KINDS: return {"ok":false,"code":"UnsupportedAction"}
	var id: String=Factory.next_id(state,"action")
	var participants: Array=Location.members(state,actor_id) if kind=="travel" else [actor_id]
	if offer.has("context") and not offer.context.provider_actor_id in participants: participants.append(offer.context.provider_actor_id)
	var action: Dictionary={"id":id,"kind":kind,"actor_id":actor_id,"participants":participants,"remaining_ticks":offer.duration,"start_tick":state.tick,"params":params.duplicate(true)}
	if offer.has("context"): action.params.context=offer.context.duplicate(true)
	state.actions[id]=action
	for participant in participants: state.actors[participant].action_id=id
	var started: Dictionary={"ok":true}
	if kind=="travel": Travel.begin(state,_catalog.data,action,offer)
	elif kind=="service": started=Services.begin(state,action,offer)
	elif kind in Cultivation.TRAINING_KINDS: started=Cultivation.begin(state,_catalog.data,action,offer)
	elif kind=="estate_build": started=Estate.begin(state,_catalog.data,action,offer)
	elif kind=="family_adopt": started=Family.begin(state,_catalog.data,action,offer)
	elif kind=="long_rest": started=Rest.begin(state,_catalog.data,action,offer)
	if not started.ok: return started
	return {"ok":true,"details":{"action_id":id}}

## 委托统一半日协调器推进一次世界，再恢复已完成行动的对白和场景事件。
func _advance(state: Dictionary, hold: Array = []) -> Dictionary:
	var old_places: Dictionary = {}
	for actor_id in state.actors: old_places[actor_id] = state.actors[actor_id].location.get("place_id","")
	var advanced: Dictionary = Step.advance(state,_catalog.data,_registry,hold)
	if not advanced.ok: return advanced
	var outcomes: Dictionary = advanced.details.completed_actions
	var suspended_actors: Array = advanced.details.suspended_actors
	for actor_id in state.interactions.keys():
		var session: Dictionary=state.interactions[actor_id]
		if session.phase=="waiting" and outcomes.has(session.pending_action_id):
			var outcome: Dictionary=outcomes[session.pending_action_id]
			Dialogue.resolve(state,_catalog.data,actor_id,outcome)
			var resumed: Dictionary=_drive_dialogue(state,actor_id)
			if not resumed.ok:
				# 行动已经完成；原说话人若不在场，不得冒用其身份继续对白。
				state.interactions.erase(actor_id)
	for actor_id in state.actors:
		if state.actors[actor_id].location.kind=="Settlement" and old_places.get(actor_id,"")!=state.actors[actor_id].location.place_id:
			var entered: Dictionary=Events.on_enter(state,_catalog.data,actor_id)
			if not entered.ok: return entered
	for request in advanced.details.get("journey_requests",[]):
		var moved: Dictionary=_military_journey(state,request)
		if not moved.ok: advanced.details.suspended_actors.append(state.armies[request.army_id].commander_actor_id)
	_schedule_travel(state)
	_schedule_military(state)
	return {"ok":true,"details":{"completed_actions":outcomes,"suspended_actors":suspended_actors}}

## 只释放指定行动持有的人物占用和资源预留，不触碰其他行动。
func _release_action(state: Dictionary, action: Dictionary) -> void:
	Step.release(state,action)

## 便捷半日调用仍走同一版本化命令入口，不建立第二套模拟时钟。
func advance_one_tick() -> Dictionary:
	if _state.is_empty(): return {"ok":false,"code":"NoWorld"}
	return submit({"kind":"advance","params":{},"actor_id":_state.player_actor_id,"request_seq":_state.request_sequences[_state.player_actor_id]+1,"expected_revision":_state.revision})

## 有限步数推进自动对白节点；只有登记的零时长线索发现可免除再次确认。
func _drive_dialogue(state: Dictionary, actor_id: String) -> Dictionary:
	for _step in range(64):
		var pumped: Dictionary=Dialogue.pump(state,_catalog.data,actor_id)
		if not pumped.ok: return pumped
		var session: Dictionary=state.interactions[actor_id]
		if session.phase=="end":
			state.interactions.erase(actor_id)
			return {"ok":true}
		if session.phase!="action" or session.pending_command.kind!="discover_offer": return {"ok":true}
		var request: Dictionary=session.pending_command.duplicate(true)
		request.context=session.context.duplicate(true)
		var previewed: Dictionary=_preview(state,request.kind,request,actor_id)
		var applied: Dictionary=_apply(state,request.kind,request,actor_id,previewed) if previewed.ok else previewed
		Dialogue.resolve(state,_catalog.data,actor_id,applied)
	return {"ok":false,"code":"DialogueStepLimit"}

## 返回当前会话的只读视图，查询期间不执行对白节点。
func interaction_view(actor_id: String = "") -> Dictionary:
	if _state.is_empty(): return {"phase":"scene"}
	if actor_id.is_empty(): actor_id=_state.player_actor_id
	return Dialogue.view(_state,_catalog.data,actor_id)

## 半日末处理旅人提案；仍先校验正式旅行规则，起步不提前消耗下一半日。
func _schedule_travel(state: Dictionary) -> void:
	Schedule.finish_arrivals(state,_catalog.data)
	for proposal in Schedule.proposals(state,_catalog.data):
		var quote: Dictionary=_preview(state,proposal.kind,proposal.params,proposal.actor_id)
		if not quote.ok: continue
		var begun: Dictionary=_begin_action(state,proposal.actor_id,proposal.kind,proposal.params,quote)
		if begun.ok:
			state.itineraries[proposal.actor_id].index=proposal.next_index
			state.itineraries[proposal.actor_id].travelling=true

## 军令只请求实际地图行旅；抵达本城后仍须沿场所通道入营，缺钱或不在城口不能瞬移。
func _military_journey(state: Dictionary, request: Dictionary) -> Dictionary:
	return MilitaryCoordination.journey(state,_catalog.data,request,_begin_action)

## NPC军政提案复用正式命令，不生成玩家请求序列或绕开同一状态校验。
func _schedule_military(state: Dictionary) -> void:
	for proposal in MilitarySchedule.proposals(state,_catalog.data):
		var quote: Dictionary=_preview(state,proposal.kind,proposal.params,proposal.actor_id)
		if not quote.ok: continue
		var candidate: Dictionary=state.duplicate(true)
		var applied: Dictionary=_apply(candidate,proposal.kind,proposal.params,proposal.actor_id,quote)
		if not applied.ok: continue
		state.clear(); state.merge(candidate,true)
