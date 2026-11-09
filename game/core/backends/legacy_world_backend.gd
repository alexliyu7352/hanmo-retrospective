## 旧GDScript世界的唯一状态与提交所有者；领域回调只写候选，失败不替换已提交三元组。
extends RefCounted
const NativeMilitaryScheduleProvider=preload("res://core/backends/native_military_schedule_provider.gd")
const NativeTravelScheduleProvider=preload("res://core/backends/native_travel_schedule_provider.gd")
const NativeGrowthReadCache=preload("res://core/backends/native_growth_read_cache.gd")
const ActionContext=preload("res://core/views/action_context.gd")
const SiegePage=preload("res://core/views/siege_page.gd")
const StudyActivityInput=preload("res://core/views/study_activity_input.gd")
const MilitaryBattlePage=preload("res://core/views/military_battle_page.gd")
const MilitaryCampPage=preload("res://core/views/military_camp_page.gd")
const SceneJourneySidebar=preload("res://core/views/scene_journey_sidebar.gd")
const PersonalBattleView=preload("res://core/views/personal_battle_view.gd")
const FamilyPage=preload("res://core/views/family_page.gd")
const EstatePage=preload("res://core/views/estate_page.gd")
const BargainQuotes=preload("res://core/domains/economy/bargain_quote.gd")
const CharacterInput=preload("res://core/views/character_input.gd")
const OrdinarySkills=preload("res://core/views/ordinary_skills_view.gd")
const CharacterSummary=preload("res://core/views/character_summary.gd")
const CommandContext=preload("res://core/backends/legacy_command_context.gd")
const ExplanationQuery=preload("res://core/runtime/explanation_query.gd")
const HudView=preload("res://core/views/hud_view.gd")
const JourneyNotes=preload("res://core/views/journey_notes.gd")
const MapView=preload("res://core/views/map_view.gd")
const SceneQuery=preload("res://core/views/scene_projection.gd")
const SkillStudy=preload("res://core/domains/cultivation/skill_study_rules.gd")
const TradeSheet=preload("res://core/views/trade_sheet.gd")
const Training=preload("res://core/domains/military/training_rules.gd")
const ActionRuntime=preload("res://core/runtime/action_runtime.gd")
const Combat=preload("res://core/combat_session.gd")
const Cultivation=preload("res://core/cultivation_rules.gd")
const Dialogue = preload("res://core/dialogue_runner.gd")
const DialogueCommands=preload("res://core/domains/interaction/dialogue_commands.gd")
const EncounterModule=preload("res://core/domains/interaction/encounter_module.gd")
const Encounters=preload("res://core/encounter_rules.gd")
const Events = preload("res://core/event_rules.gd")
const Family=preload("res://core/family_rules.gd")
const Journey = preload("res://core/journey_commands.gd")
const JourneyModule=preload("res://core/domains/travel/journey_module.gd")
const Location=preload("res://core/world_location.gd")
const MilitaryCoordination=preload("res://core/domains/military/military_coordination.gd")
const MilitaryModule=preload("res://core/domains/military/military_module.gd")
const MilitarySchedule=preload("res://core/military_schedule.gd")
const Parties=preload("res://core/party_rules.gd")
const Personal=preload("res://core/personal_combat.gd")
const PersonalCoordination=preload("res://core/domains/personal/personal_coordination.gd")
const Rest=preload("res://core/long_rest.gd")
const Schedule=preload("res://core/travel_schedule.gd")
const Services = preload("res://core/service_rules.gd")
const SiegeWorld=preload("res://core/siege_world.gd")
const Step = preload("res://core/world_step.gd")
const Travel = preload("res://core/travel_rules.gd")
const Validation=preload("res://core/runtime/world_validation.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Rules=preload("res://core/rules_migration.gd")
const Estate=preload("res://core/estate_rules.gd")
const Schema=preload("res://core/state_schema.gd")
const TransactionContext=preload("res://core/runtime/transaction_context.gd")
const Military=preload("res://core/military_world.gd")
const Escort=preload("res://core/escort_rules.gd")
const Factory=preload("res://core/world_factory.gd")
const DomainObservers=preload("res://core/runtime/domain_observers.gd")
var state: Dictionary={}
var catalog: Dictionary={}
var registry: Dictionary={}
var _native_source_generation: int=0
var _native_growth_enabled: bool=false
var _native_growth_source_commit: String=""
var _native_growth_cache=NativeGrowthReadCache.new()
var _native_military_schedule_enabled: bool=false
var _native_military_schedule_provider=NativeMilitaryScheduleProvider.new()
var _native_travel_schedule_enabled: bool=false
var _native_travel_schedule_provider=NativeTravelScheduleProvider.new()

## 仅接收已完成原准备与终验的三元组；不重验、不深复制或发布中间回调。
func publish(next_state: Dictionary, next_catalog: Dictionary, next_registry: Dictionary) -> void:
	state=next_state; catalog=next_catalog; registry=next_registry
	invalidate_native_growth_read()

## 保留原请求首错、候选尾处理和唯一提交顺序；两个动态回调只在本次调用使用，不持有Kernel。
func submit(command: Dictionary, preview: Callable, apply: Callable) -> Dictionary:
	if state.is_empty(): return {"ok":false,"code":"NoWorld"}
	if not Schema.fields(command,{"kind":TYPE_STRING,"params":TYPE_DICTIONARY,"request_seq":TYPE_INT,"expected_revision":TYPE_INT}) or not Schema.whole(command.request_seq,1): return {"ok":false,"code":"InvalidCommand"}
	var actor_id: Variant = command.get("actor_id",state.player_actor_id)
	if not actor_id is String or not state.actors.has(actor_id): return {"ok":false,"code":"UnknownActor"}
	var key: String = actor_id+":"+str(command.request_seq)
	if command.request_seq<=state.request_sequences[actor_id]:
		return state.results.get(key,{"ok":false,"code":"AlreadyApplied"}).duplicate(true)
	if command.expected_revision!=state.revision: return {"ok":false,"code":"StalePreview"}
	var offer: Dictionary = preview.call(state,command.kind,command.params,actor_id)
	if not offer.ok: return offer
	var transaction_context: Dictionary=TransactionContext.create(state,command)
	if not transaction_context.ok: return transaction_context
	var candidate: Dictionary = state.duplicate(true)
	var applied: Dictionary = apply.call(candidate,command.kind,command.params,actor_id,offer)
	if not applied.ok: return applied
	var released: Dictionary=Military.release_invalid_commands(candidate,catalog.data,candidate.tick)
	if not released.ok: return released
	Escort.observe(candidate,catalog.data)
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
	var reconciled: Dictionary=DomainObservers.reconcile(candidate,catalog.data,transaction_context,registry)
	if not reconciled.ok: return reconciled
	var checked: Dictionary = Schema.validate(candidate,catalog,registry)
	if not checked.ok: return checked
	state=candidate
	invalidate_native_growth_read()
	return result


## 按原顺序准备注册、目录、世界及尾验；descriptor回调只在此次调用使用，失败保留旧三元组。
func new_game(module_descriptors: Callable, scenario_id: String = "scenario.two_stories", content_path: String = "res://data/generated/two_stories.hmcatalog.json", appearance_draft: Dictionary={}, player_actor_id: String="") -> Dictionary:
	var registrations: Dictionary=Registry.build(module_descriptors.call())
	if not registrations.ok: return registrations
	var command_contract: Dictionary=Builtin.command_contract_status(registrations)
	if not command_contract.ok: return command_contract
	var observer_contract: Dictionary=Builtin.observer_contract_status(registrations)
	if not observer_contract.ok: return observer_contract
	var phase_contract: Dictionary=Builtin.phase_contract_status(registrations)
	if not phase_contract.ok: return phase_contract
	var loaded: Dictionary = Catalog.read_prepared(content_path)
	if not loaded.ok: return loaded
	# 新能力只由常驻C++拥有，GD不能建立忽略宏观字段的静态替代世界。
	if "native_dynamic_world" in loaded.data.get("required_capabilities",[]): return {"ok":false,"code":"NativeBackendRequired"}
	var built: Dictionary = Factory.create(loaded,scenario_id,appearance_draft,player_actor_id)
	if not built.ok: return built
	var estates: Dictionary=Estate.initialize(built.state,built.catalog.data)
	if not estates.ok: return estates
	var military: Dictionary=Military.initialize(built.state,built.catalog.data)
	if not military.ok: return military
	var valid: Dictionary = Schema.validate(built.state,built.catalog,registrations)
	if not valid.ok: return valid
	publish(built.state,built.catalog,registrations)
	return {"ok":true}


## 冷恢复只编译原受信合同并返回独立registry，不构造默认世界或发布中间值。
func prepare_restore_registry(module_descriptors: Callable) -> Dictionary:
	var registry: Dictionary=Registry.build(module_descriptors.call())
	if not registry.ok: return registry
	var checked: Dictionary=Builtin.command_contract_status(registry)
	if not checked.ok: return checked
	checked=Builtin.observer_contract_status(registry)
	if not checked.ok: return checked
	checked=Builtin.phase_contract_status(registry)
	if not checked.ok: return checked
	return {"ok":true,"registry":registry}


## 原规则升级、Schema与引擎检查通过后只安装state深副本，保留目录和注册原引用。
func restore(saved: Dictionary) -> Dictionary:
	var upgrade: Dictionary=Rules.upgrade(saved)
	if not upgrade.ok: return upgrade
	saved=upgrade.state
	var valid: Dictionary = Schema.validate(saved,catalog,registry)
	if not valid.ok: return valid
	var build: String = str(Engine.get_version_info().get("string","unknown"))
	if saved.engine_build!=build or saved.rng_backend!="godot:"+build: return {"ok":false,"code":"IncompatibleEngine"}
	publish(saved.duplicate(true),catalog,registry)
	return {"ok":true}


## 原输入合同和尾验全部通过后才安装三个深副本；失败不改变当前三元组。
func restore_with_catalog(saved: Dictionary, catalog: Dictionary, registry: Variant=null) -> Dictionary:
	if not Validation.valid_registry(registry): return {"ok":false,"code":"InvalidModuleRegistry"}
	var command_contract: Dictionary=Builtin.command_contract_status(registry)
	if not command_contract.ok: return command_contract
	var observer_contract: Dictionary=Builtin.observer_contract_status(registry)
	if not observer_contract.ok: return observer_contract
	var phase_contract: Dictionary=Builtin.phase_contract_status(registry)
	if not phase_contract.ok: return phase_contract
	var valid: Dictionary=Schema.validate(saved,catalog,registry)
	if not valid.ok: return valid
	var build: String=str(Engine.get_version_info().get("string","unknown"))
	if saved.engine_build!=build or saved.rng_backend!="godot:"+build: return {"ok":false,"code":"IncompatibleEngine"}
	var next_state: Dictionary=saved.duplicate(true)
	var next_catalog: Dictionary=catalog.duplicate(true)
	var next_registry: Dictionary=registry.duplicate(true)
	publish(next_state,next_catalog,next_registry)
	return {"ok":true}


## 导出当前state深副本，调用者不能经读取结果改写实际世界。
func snapshot() -> Dictionary:
	return state.duplicate(true)


## 导出当前catalog深副本，保留原内容读取的隔离语义。
func content() -> Dictionary:
	return catalog.duplicate(true)


## 一次导出state/catalog/registry三个深副本，不运行Codec或物理存储。
func save_context() -> Dictionary:
	return {"state":state.duplicate(true),"catalog":catalog.duplicate(true),"registry":registry.duplicate(true)}


## 空世界先拒绝；默认actor和返回revision沿原顺序处理，动态preview回调不持久保存。
func preview(preview_hook: Callable, kind: String, params: Dictionary = {}, actor_id: String = "") -> Dictionary:
	if state.is_empty(): return {"ok":false,"code":"NoWorld"}
	if actor_id.is_empty(): actor_id=state.player_actor_id
	var result: Dictionary = preview_hook.call(state,kind,params,actor_id)
	result["revision"]=state.revision
	return result

## 校验已支持命令及真实世界前提，不能仅相信界面是否显示按钮。
func preview_command(state: Dictionary, kind: String, params: Dictionary, actor_id: String, preview_hook: Callable) -> Dictionary:
	var data: Dictionary = catalog.data
	if not state.actors.has(actor_id): return {"ok":false,"code":"UnknownActor"}
	var actor: Dictionary = state.actors[actor_id]
	if registry.commands.has(kind) and registry.commands[kind].owner=="military" and registry.commands[kind].gate_id=="military_combat_early":
		return registry.commands[kind].preview.call(state,data,actor_id,kind,params)
	if registry.commands.has(kind) and registry.commands[kind].owner=="military" and registry.commands[kind].gate_id=="military_siege_early":
		return registry.commands[kind].preview.call(state,data,actor_id,kind,params)
	var personal_command: Dictionary=registry.commands.get(kind,{})
	if personal_command.get("owner")=="personal" and personal_command.get("gate_id")=="personal_before_world" and kind in Personal.KINDS:
		if Combat.occupied(state,actor_id) and Combat.for_actor(state,actor_id).get("kind")=="military": return {"ok":false,"code":"BattlePending"}
		return personal_command.preview.call(state,data,actor_id,kind,params)
	if Combat.pauses_world(state): return {"ok":false,"code":"BattlePending"}
	if Combat.occupied(state,actor_id) and kind not in ["advance","travel_wait"]: return {"ok":false,"code":"BattlePending"}
	# 同城军需交付仍走正式权限和实物校验；前面的会战占用检查禁止节点接战时插队。
	if not SiegeWorld.for_actor(state,actor_id).is_empty() and kind not in ["advance","travel_wait","campaign_deliver"]: return {"ok":false,"code":"SiegePending"}
	if actor.location.kind=="InArmy": return {"ok":false,"code":"NotArmyCommander"}
	if actor.location.kind=="Following": return {"ok":false,"code":"NotPartyLeader"}
	var encounter_command: Dictionary=registry.commands.get(kind,{})
	if encounter_command.get("owner")=="interaction" and encounter_command.get("gate_id")=="encounter_before_pending" and kind in Encounters.KINDS:
		return encounter_command.preview.call(state,data,actor_id,kind,params)
	var meeting: Dictionary=Encounters.pending_for(state,actor_id)
	if kind in ["advance","travel_wait"] and not Encounters.pending_for(state,state.player_actor_id).is_empty(): return {"ok":false,"code":"EncounterPending"}
	if not meeting.is_empty() and not kind in ["dialogue_next","dialogue_choose","end_interaction"]: return {"ok":false,"code":"EncounterPending"}
	var party_command: Dictionary=registry.commands.get(kind,{})
	if party_command.get("owner")=="travel" and party_command.get("gate_id")=="party_before_actor" and kind in Parties.KINDS:
		return party_command.preview.call(state,data,actor_id,kind,params)
	var journey_command: Dictionary=registry.commands.get(kind,{})
	if journey_command.get("owner")=="travel" and journey_command.get("gate_id")=="journey_before_actor" and kind in Journey.KINDS:
		return journey_command.preview.call(state,data,actor_id,kind,params)
	var advance_command: Dictionary=registry.commands.get(kind,{})
	if kind=="advance" and advance_command.get("owner")=="runtime" and advance_command.get("gate_id")=="advance_before_actor":
		return advance_command.preview.call(state,data,actor_id,kind,params)
	var rescue_command: Dictionary=registry.commands.get(kind,{})
	if rescue_command.get("owner")=="travel" and rescue_command.get("gate_id")=="rescue_before_actor" and kind=="rescue":
		return rescue_command.preview.call(state,data,actor_id,kind,params)
	# 仅原休整集合可在人物公共守卫前预览；遭遇与军队限制仍先处理。
	if kind in Rest.KINDS and registry.commands.has(kind) and registry.commands[kind].owner=="services" and registry.commands[kind].gate_id=="rest_before_actor":
		return registry.commands[kind].preview.call(state,data,actor_id,kind,params)
	# 课程取消仅在本域精确命令与守卫匹配时越过忙碌检查，前面的战斗/遭遇限制保持。
	var study_cancel: Dictionary=registry.commands.get(kind,{})
	if kind in ["study_etiquette_choose","study_etiquette_exit"] and study_cancel.get("owner")=="cultivation" and study_cancel.get("gate_id")=="study_etiquette_before_actor":
		return study_cancel.preview.call(state,data,actor_id,kind,params)
	if kind in ["study_challenge_choose","study_challenge_exit"] and study_cancel.get("owner")=="cultivation" and study_cancel.get("gate_id")=="study_challenge_before_actor":
		return study_cancel.preview.call(state,data,actor_id,kind,params)
	if kind in ["study_cancel","study_pause","study_resume"] and study_cancel.get("owner")=="cultivation" and study_cancel.get("gate_id")=="study_before_actor":
		return study_cancel.preview.call(state,data,actor_id,kind,params)
	if kind in MilitaryModule.DRILL_KINDS and study_cancel.get("owner")=="military" and study_cancel.get("gate_id")==MilitaryModule.DRILL_GATE:
		return study_cancel.preview.call(state,data,actor_id,kind,params)
	if kind=="military_train_cancel" and study_cancel.get("owner")=="military" and study_cancel.get("gate_id")=="military_training_before_actor":
		return study_cancel.preview.call(state,data,actor_id,kind,params)
	if not actor.alive or not actor.available: return {"ok":false,"code":"ActorUnavailable"}
	# 在真实行军途中允许原令返程，由同一地形规划重算余路；其他在办事务仍不能插队。
	var registered: Dictionary=registry.commands.get(kind,{})
	if registered.get("owner")=="military" and registered.get("gate_id")=="campaign_travel_redirect" and not actor.action_id.is_empty() and state.actions[actor.action_id].kind=="travel":
		return registered.preview.call(state,data,actor_id,kind,params)
	if not actor.action_id.is_empty() and not (not meeting.is_empty() and kind in ["dialogue_next","dialogue_choose","end_interaction"] and state.interactions.has(actor_id) and state.interactions[actor_id].context.get("encounter_id")==meeting.id): return {"ok":false,"code":"ActorBusy"}
	if registry.commands.has(kind):
		if registered.get("owner")=="interaction" and kind in DialogueCommands.KINDS:
			return registered.preview.call(state,data,actor_id,kind,params,preview_hook)
		return registered.preview.call(state,data,actor_id,kind,params)
	return {"ok":false,"code":"UnsupportedAction"}

## 仅在隔离候选状态执行已校验规则，动画和界面回调不直接写入事实。
func apply_command(state: Dictionary, kind: String, params: Dictionary, actor_id: String, offer: Dictionary, advance_hook: Callable, begin_action_hook: Callable, drive_dialogue_hook: Callable, apply_hook: Callable) -> Dictionary:
	if registry.commands.has(kind):
		var registered: Dictionary=registry.commands[kind]
		if registered.owner=="personal" and registered.gate_id=="personal_before_world" and kind in Personal.KINDS:
			var ready: Dictionary=PersonalCoordination.before_command(state,catalog.data,actor_id,kind,params,offer,advance_hook)
			if not ready.ok: return ready
		var applied: Dictionary
		if registered.mode=="action":
			applied=begin_action_hook.call(state,actor_id,kind,params,offer) if registered.get("compat_action")=="legacy_begin_action" else ActionRuntime.begin(state,catalog.data,actor_id,kind,params,offer,registry)
		elif registered.get("owner")=="interaction" and kind in DialogueCommands.KINDS:
			applied=registered.apply.call(state,catalog.data,actor_id,kind,params,offer,drive_dialogue_hook,apply_hook)
		else:
			applied=registered.apply.call(state,catalog.data,actor_id,kind,params,offer)
		if not applied.ok: return applied
		# 临时推进请求只允许固定三锁入口；其他来源及畸形载荷不能借成功回执推动或提交世界。
		if applied.has("control"):
			if registered.owner!="runtime" or kind!="advance" or registered.gate_id!="advance_before_actor" or typeof(applied.ok)!=TYPE_BOOL or not applied.control is Dictionary or typeof(applied.control.get("advance_world"))!=TYPE_BOOL or applied!={"ok":true,"control":{"advance_world":true}}: return {"ok":false,"code":"InvalidCommandControl"}
			return advance_hook.call(state)
		if registered.owner=="runtime" and kind=="advance": return {"ok":false,"code":"InvalidCommandControl"}
		if registered.owner=="interaction" and registered.gate_id=="encounter_before_pending":
			return EncounterModule.after_command(state,actor_id,kind,applied,drive_dialogue_hook)
		if registered.owner=="travel" and registered.gate_id=="journey_before_actor" and kind in Journey.KINDS:
			return JourneyModule.after_command(state,actor_id,kind,applied,advance_hook)
		# 取消先完成原领域事实再释放原行动；兼容协调不下沉为services反向依赖。
		if registered.owner=="services" and kind=="rest_cancel" and registered.gate_id=="rest_before_actor": Step.release(state,state.actions[offer.action_id])
		if registered.owner=="cultivation" and kind=="study_cancel" and registered.gate_id=="study_before_actor": Step.release(state,state.actions[offer.action_id])
		if registered.owner=="military" and kind=="military_train_cancel" and registered.gate_id=="military_training_before_actor": Step.release(state,state.actions[offer.action_id])
		return MilitaryCoordination.after_command(state,catalog.data,kind,applied,begin_action_hook) if registered.owner=="military" else applied
	return {"ok":false,"code":"UnsupportedAction"}

## 建立行动并占用实际参与者；全部不变量校验通过后才由调用者提交。
func begin_action(state: Dictionary, actor_id: String, kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
	if not kind in ["travel","wait","service","estate_build","family_adopt","long_rest"]+Cultivation.TRAINING_KINDS: return {"ok":false,"code":"UnsupportedAction"}
	var id: String=Factory.next_id(state,"action")
	var participants: Array=Location.members(state,actor_id) if kind=="travel" else [actor_id]
	if offer.has("context") and not offer.context.provider_actor_id in participants: participants.append(offer.context.provider_actor_id)
	var action: Dictionary={"id":id,"kind":kind,"actor_id":actor_id,"participants":participants,"remaining_ticks":offer.duration,"start_tick":state.tick,"params":params.duplicate(true)}
	if offer.has("context"): action.params.context=offer.context.duplicate(true)
	state.actions[id]=action
	for participant in participants: state.actors[participant].action_id=id
	var started: Dictionary={"ok":true}
	if kind=="travel": Travel.begin(state,catalog.data,action,offer)
	elif kind=="service": started=Services.begin(state,action,offer)
	elif kind in Cultivation.TRAINING_KINDS: started=Cultivation.begin(state,catalog.data,action,offer)
	elif kind=="estate_build": started=Estate.begin(state,catalog.data,action,offer)
	elif kind=="family_adopt": started=Family.begin(state,catalog.data,action,offer)
	elif kind=="long_rest": started=Rest.begin(state,catalog.data,action,offer)
	if not started.ok: return started
	return {"ok":true,"details":{"action_id":id}}

## 委托统一半日协调器推进一次世界；旅行调度失败原位外抛，不运行后尾或发布候选。
func advance_world(state: Dictionary, hold: Array, drive_dialogue_hook: Callable, military_journey_hook: Callable, schedule_travel_hook: Callable, schedule_military_hook: Callable) -> Dictionary:
	var old_places: Dictionary = {}
	for actor_id in state.actors: old_places[actor_id] = state.actors[actor_id].location.get("place_id","")
	var advanced: Dictionary = Step.advance(state,catalog.data,registry,hold)
	if not advanced.ok: return advanced
	var outcomes: Dictionary = advanced.details.completed_actions
	var suspended_actors: Array = advanced.details.suspended_actors
	for actor_id in state.interactions.keys():
		var session: Dictionary=state.interactions[actor_id]
		if session.phase=="waiting" and outcomes.has(session.pending_action_id):
			var outcome: Dictionary=outcomes[session.pending_action_id]
			Dialogue.resolve(state,catalog.data,actor_id,outcome)
			var resumed: Dictionary=drive_dialogue_hook.call(state,actor_id)
			if not resumed.ok:
				# 行动已经完成；原说话人若不在场，不得冒用其身份继续对白。
				state.interactions.erase(actor_id)
	for actor_id in state.actors:
		if state.actors[actor_id].location.kind=="Settlement" and old_places.get(actor_id,"")!=state.actors[actor_id].location.place_id:
			var entered: Dictionary=Events.on_enter(state,catalog.data,actor_id)
			if not entered.ok: return entered
	for request in advanced.details.get("journey_requests",[]):
		var moved: Dictionary=military_journey_hook.call(state,request)
		if not moved.ok: advanced.details.suspended_actors.append(state.armies[request.army_id].commander_actor_id)
	var scheduled: Variant=schedule_travel_hook.call(state)
	if not scheduled is Dictionary or typeof(scheduled.get("ok"))!=TYPE_BOOL: return {"ok":false,"code":"InvalidTravelScheduleResult"}
	if not scheduled.ok: return scheduled
	var military_scheduled: Variant=schedule_military_hook.call(state)
	if not military_scheduled is Dictionary or typeof(military_scheduled.get("ok"))!=TYPE_BOOL: return {"ok":false,"code":"InvalidMilitaryScheduleResult"}
	if not military_scheduled.ok: return military_scheduled
	return {"ok":true,"details":{"completed_actions":outcomes,"suspended_actors":suspended_actors}}

## 只释放指定行动持有的人物占用和资源预留，不触碰其他行动。
func release_action(state: Dictionary, action: Dictionary) -> void:
	Step.release(state,action)

## 有限步数推进自动对白节点；只有登记的零时长线索发现可免除再次确认。
func drive_dialogue(state: Dictionary, actor_id: String, preview_hook: Callable, apply_hook: Callable) -> Dictionary:
	for _step in range(64):
		var pumped: Dictionary=Dialogue.pump(state,catalog.data,actor_id)
		if not pumped.ok: return pumped
		var session: Dictionary=state.interactions[actor_id]
		if session.phase=="end":
			state.interactions.erase(actor_id)
			return {"ok":true}
		if session.phase!="action" or session.pending_command.kind!="discover_offer": return {"ok":true}
		var request: Dictionary=session.pending_command.duplicate(true)
		request.context=session.context.duplicate(true)
		var previewed: Dictionary=preview_hook.call(state,request.kind,request,actor_id)
		var applied: Dictionary=apply_hook.call(state,request.kind,request,actor_id,previewed) if previewed.ok else previewed
		Dialogue.resolve(state,catalog.data,actor_id,applied)
	return {"ok":false,"code":"DialogueStepLimit"}

## 半日末处理旅人提案；仍先校验正式旅行规则，起步不提前消耗下一半日。
func schedule_travel(state: Dictionary, preview_hook: Callable, begin_action_hook: Callable) -> Dictionary:
	Schedule.finish_arrivals(state,catalog.data)
	var proposals: Array
	if _native_travel_schedule_enabled:
		var proposed: Dictionary=_native_travel_schedule_provider.propose(state,catalog.data)
		if not proposed.ok: return proposed
		proposals=proposed.proposals
	else: proposals=Schedule.proposals(state,catalog.data)
	for proposal in proposals:
		var quote: Dictionary=preview_hook.call(state,proposal.kind,proposal.params,proposal.actor_id)
		if not quote.ok: continue
		var begun: Dictionary=begin_action_hook.call(state,proposal.actor_id,proposal.kind,proposal.params,quote)
		if begun.ok:
			state.itineraries[proposal.actor_id].index=proposal.next_index
			state.itineraries[proposal.actor_id].travelling=true
	return {"ok":true}

## 军令只请求实际地图行旅；抵达本城后仍须沿场所通道入营，缺钱或不在城口不能瞬移。
func military_journey(state: Dictionary, request: Dictionary, begin_action_hook: Callable) -> Dictionary:
	return MilitaryCoordination.journey(state,catalog.data,request,begin_action_hook)

## NPC军政提案复用正式命令，不生成玩家请求序列或绕开同一状态校验。
func schedule_military(state: Dictionary, preview_hook: Callable, apply_hook: Callable) -> Dictionary:
	var proposals: Array
	if _native_military_schedule_enabled:
		var proposed: Dictionary=_native_military_schedule_provider.propose(state,catalog.data)
		if not proposed.ok: return proposed
		proposals=proposed.proposals
	else: proposals=MilitarySchedule.proposals(state,catalog.data)
	for proposal in proposals:
		var quote: Dictionary=preview_hook.call(state,proposal.kind,proposal.params,proposal.actor_id)
		if not quote.ok: continue
		var candidate: Dictionary=state.duplicate(true)
		var applied: Dictionary=apply_hook.call(candidate,proposal.kind,proposal.params,proposal.actor_id,quote)
		if not applied.ok: continue
		state.clear(); state.merge(candidate,true)
	return {"ok":true}

## 只读投影当前人物和命令序号；不复制完整世界，也不预占请求或替代原提交守卫。
func command_context(actor_id: String="") -> Dictionary:
	return CommandContext.query(state,actor_id)

## 只投影调用方已确定的单人物表现输入，不复制整世界，也不授予可见性权限。
func character_input(actor_id: String) -> Dictionary:
	return CharacterInput.query(state,catalog.get("data",{}),actor_id)

## 只返回当前玩家成长窗口的指定页有限数据，报价与交互仍由原命令入口判断。
func character_summary(page: String="skills") -> Dictionary:
	var view: Dictionary=CharacterSummary.query(state,catalog.get("data",{}),page)
	if _native_growth_enabled: view.action=_native_growth_cache.read(_native_source_generation,state,catalog,registry,_native_growth_source_commit)
	return view

## 只展开本人十项经验和选中技艺终态回执；原规则仍独占成长与费用。
func ordinary_skills(skill_id: String="") -> Dictionary:
	return OrdinarySkills.query(state,catalog.get("data",{}),skill_id)

## 显式选择唯一军政提案来源；配置不运行提案，失败留在同一半日候选中返回。
func configure_native_military_schedule(enabled: bool, expected_build: Dictionary={}) -> void:
	_native_military_schedule_enabled=enabled
	_native_military_schedule_provider.configure(expected_build if enabled else {})

## 只选择半日末的唯一旅行提案provider；配置不推进世界，坏绑定留真正调用失败。
func configure_native_travel_schedule(enabled: bool, expected_build: Dictionary={}) -> void:
	_native_travel_schedule_enabled=enabled
	_native_travel_schedule_provider.configure(expected_build if enabled else {})

## 开发配置只在实际变化时清当前代；配置本身不编码或创建原生Bridge。
func configure_native_growth_read(enabled: bool,source_commit: String="") -> void:
	if enabled==_native_growth_enabled and source_commit==_native_growth_source_commit: return
	_native_growth_enabled=enabled; _native_growth_source_commit=source_commit
	invalidate_native_growth_read()

## 唯一源代次只属本Backend；可信别名原地改写后也须显式调用，不伪装为world_id。
func invalidate_native_growth_read() -> void:
	_native_source_generation+=1
	_native_growth_cache.invalidate(_native_source_generation)

## 返回开发计数/计时深副本，不提供世界、Bridge、缓存写口或授权票据。
func native_growth_read_diagnostics() -> Dictionary:
	var result: Dictionary=_native_growth_cache.diagnostics()
	result["enabled"]=_native_growth_enabled; result["source_generation"]=_native_source_generation
	return result

## 只投影当前玩家的指定场景；保留既有知闻与错误，不新增世界ID或游标语义。
func scene_view(place_id: String, view_kind: String) -> Dictionary:
	return SceneQuery.query(state,catalog.get("data",{}),place_id,view_kind,state.get("player_actor_id",""))

## 读取原HUD当前玩家摘要；个人战仅采用调用方已选战场，不重选分支或授予行动许可。
func hud_view(personal_battle_id: String="") -> Dictionary:
	return HudView.query(state,catalog.get("data",{}),personal_battle_id)

## 返回地图画布所需的有限动态值；既有全作者地图语义不扩大为新知闻或行动许可。
func map_view() -> Dictionary:
	return MapView.query(state,catalog.get("data",{}))

## 返回原有效货单所需的有限运行数据；不报价、预留、授权或额外校验人物外貌。
func trade_view(shop_id: String, provider_actor_id: String, receipt_action_id: String="") -> Dictionary:
	return TradeSheet.query(state,catalog.get("data",{}),shop_id,provider_actor_id,receipt_action_id)

## 按当前已开始世界生成原行旅手记；只返回文本，不复制整态或扩展知识范围。
func journal_text() -> String:
	return JourneyNotes.describe(state,catalog.get("data",{}))

## 只读解释已提交状态；observer由受信调用方指定而非query授权，本入口不提供外部调用者身份认证。
func explain(query: Dictionary, observer_id: String="") -> Dictionary:
	if observer_id.is_empty(): observer_id=state.get("player_actor_id","")
	return ExplanationQuery.explain(state,catalog.get("data",{}),query,observer_id)

## 只读当前实际岗位的普通技艺课程；领域完成在场、权源与资格判断，查询不自动开课。
func study_courses(actor_id: String, context: Dictionary, ticks: int=2) -> Dictionary:
	return SkillStudy.courses(state,catalog.get("data",{}),actor_id,context,ticks)

## 只读本人当前课程公开题面，不读取其它人物实例或改变答题轮次。
func study_etiquette(actor_id: String, action_id: String) -> Dictionary:
	return SkillStudy.etiquette_view(state,catalog.get("data",{}),actor_id,action_id)

## 读取本人当前课程题面；通用练习仅返回候选表现，不应用交易折扣。
func study_challenge(actor_id: String, action_id: String) -> Dictionary:
	return SkillStudy.challenge_view(state,catalog.get("data",{}),actor_id,action_id)

## 只读本人军阵课程公开实例，不推进随机、课时或真实部队。
func military_drill(actor_id: String, action_id: String) -> Dictionary:
	return Training.drill_view(state,catalog.get("data",{}),actor_id,action_id)

## 返回当前会话的只读视图，查询期间不执行对白节点。
func interaction_view(actor_id: String = "") -> Dictionary:
	if state.is_empty(): return {"phase":"scene"}
	if actor_id.is_empty(): actor_id=state.player_actor_id
	return Dialogue.view(state,catalog.data,actor_id)

## 真实交易题面只读本人同一冻结报价，不替调用者刷新市场时效或创建另一个seed。
func trade_bargain(actor_id: String, quote_id: String) -> Dictionary:
	return BargainQuotes.view(state,catalog.get("data",{}),actor_id,quote_id)

## 开窗时读取原首家事岗位；不为页面重绘改挑可办者。
func family_provider_context() -> Dictionary:
	return FamilyPage.provider_context(state,catalog.get("data",{}))

## 当前玩家家事指定页的有限显示值；原开窗context由UI明确保留。
func family_page(tab: String="family", provider_context: Dictionary={}) -> Dictionary:
	return FamilyPage.query(state,catalog.get("data",{}),tab,provider_context)

## 原庄务存在与同地探针，先于重页面读取且不新增owner权限条件。
func estate_available(estate_id: String) -> bool:
	return EstatePage.available(state,catalog.get("data",{}),estate_id)

## 原可看庄园指定页的有限显示值；资产资格和实际结算仍走preview/submit。
func estate_page(estate_id: String, tab: String="people") -> Dictionary:
	return EstatePage.query(state,catalog.get("data",{}),estate_id,tab)

## 并行新增的基本行动值归同一后端，只读当前行动而不预读窗口。
func action_context() -> Dictionary:
	return ActionContext.query(state)

## 原旅行输入边界按需执行，仍保Combat/Siege/Encounter短路。
func action_needs_input() -> bool:
	return ActionContext.needs_input(state)

## 原手动课程窗口仅在控制器请求时读取，不缓存跨命令结果。
func action_manual_window() -> bool:
	return ActionContext.manual_window(state,state.player_actor_id)

## 同帧只读preview后定位原议价实例，不新增资格或返回整个事实。
func trade_bargain_source(actor_id: String, basis_sha256: String) -> Dictionary:
	return BargainQuotes.find_for_basis(state,actor_id,basis_sha256)

## 当前玩家个人战整页的有限输入，选择与表现仍复用同一核心算法。
func personal_battle_view() -> Dictionary:
	return PersonalBattleView.query(state,catalog.get("data",{}))

## 原个人战身份守卫按需读取，不预取画像或候选几何。
func personal_battle_context(battle_id: String) -> Dictionary:
	return PersonalBattleView.context(state,battle_id)

## 仅为原画布回退提供同源候选，不替代当前命令预览或授予控制权。
func personal_battle_geometry(battle_id: String, draft: Dictionary) -> Dictionary:
	return PersonalBattleView.geometry(state,catalog.get("data",{}),battle_id,draft)

## 普通场所侧栏只返回原可显示人物、导航和本人待开事件，不授予命令权限。
func place_sidebar() -> Dictionary:
	return SceneJourneySidebar.place(state,catalog.get("data",{}))

## 行旅右栏保原会面优先与同源供粮，画布及实际移动仍走各自入口。
func journey_sidebar() -> Dictionary:
	return SceneJourneySidebar.journey(state,catalog.get("data",{}))

## 开营务窗口时保原作者首岗位，不在重绘时另选办理人。
func military_camp_provider_context() -> Dictionary:
	return MilitaryCampPage.provider_context(state,catalog.get("data",{}))

## 本人营务指定页有限值；选择来自本地metadata，不重算报价或改草案。
func military_camp_page(tab: String="orders", selection: Dictionary={}, provider_context: Dictionary={}) -> Dictionary:
	return MilitaryCampPage.query(state,catalog.get("data",{}),tab,selection,provider_context)

## 首报价通过后的出发来源，保已到目标时不读统帅的原短路。
func military_camp_departure_source(campaign_id: String) -> Dictionary:
	return MilitaryCampPage.departure_source(state,catalog.get("data",{}),campaign_id)

## 成功报价的原序参与者姓名，不依赖画像或增加资格筛选。
func military_camp_participant_names(actor_ids: Array) -> Array:
	return MilitaryCampPage.participant_names(state,catalog.get("data",{}),actor_ids)

## 原NotAtCityExit提示只取实际城口，不把展示引导当移动授权。
func military_camp_departure_guide_place() -> String:
	return MilitaryCampPage.departure_guide_place(state,catalog.get("data",{}))

## 野战整页仅返回当前受控会话的有限表现值，空ID保原选择优先级。
func military_battle_page(battle_id: String="") -> Dictionary:
	return MilitaryBattlePage.query(state,catalog.get("data",{}),battle_id)

## 完整本地军令草案在同一真实战场上报价，结果身份不刷新旧页或授予提交权。
func military_battle_draft(battle_id: String, selected_id: String, orders: Dictionary) -> Dictionary:
	return MilitaryBattlePage.draft(state,catalog.get("data",{}),battle_id,selected_id,orders)

## 当场课程岗位按原作者序返回有限context，不提前作费用或资格报价。
func study_course_contexts() -> Array:
	return StudyActivityInput.course_contexts(state,catalog.get("data",{}))

## 当前修业只出原付款与进度标量，非课程不访问事实或钱包。
func study_activity() -> Dictionary:
	return StudyActivityInput.active(state)

## 活动票据身份仍每次对原完整data求同一摘要，不缓存或替代内容来源。
func activity_identity() -> Dictionary:
	return StudyActivityInput.identity(state,catalog)

## 外壳只读当前人物raw地点，不把跟随/随军位置改解释为城内。
func presentation_location() -> Dictionary:
	return SceneJourneySidebar.raw_location(state)

## 重绘前的货单缓存寿命仅沿原actor与raw交谈阶段判断。
func trade_cache_context() -> Dictionary:
	return TradeSheet.cache_context(state)

## 成功提交后只取指定行动的原收据显示字段，缺源不改历史或补造交易。
func trade_action_receipt(action_id: String) -> Dictionary:
	return TradeSheet.action_receipt(state,action_id)

## 按原在阵/撤离结果选择围城页，只返回本会话的有限表现输入。
func siege_page() -> Dictionary:
	return SiegePage.query(state,catalog.get("data",{}))

## 同一存活页的原单部预览按序批量执行；身份只防混读，不改变正式提交许可。
func siege_draft(identity: Dictionary, requests: Array, first_ok: bool=false) -> Dictionary:
	return SiegePage.draft(state,catalog.get("data",{}),identity,requests,first_ok)
