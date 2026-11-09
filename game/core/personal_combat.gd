## 个人战的真实来源、参战占用和一次性世界回写；局部几何与轮次交给独立规则。
extends RefCounted
const TacticalView=preload("res://core/domains/personal/personal_projection.gd")
const Sessions=preload("res://core/combat_session.gd")
const Stamina=preload("res://core/domains/personal/personal_stamina.gd")
const Objectives=preload("res://core/domains/personal/personal_objectives.gd")
const Withdrawal=preload("res://core/domains/personal/personal_withdrawal.gd")
const Round=preload("res://core/personal_round.gd")
const Condition=preload("res://core/domains/personal/personal_condition.gd")
const Eligibility=preload("res://core/domains/cultivation/skill_eligibility.gd")
const Hit=preload("res://core/domains/personal/personal_hit.gd")
const Armor=preload("res://core/domains/personal/personal_armor.gd")
const ArmorDefinition=preload("res://core/domains/personal/personal_armor_content.gd")
const Geometry=preload("res://core/personal_geometry.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Encounters=preload("res://core/encounter_rules.gd")
const Location=preload("res://core/world_location.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Factory=preload("res://core/world_factory.gd")
const Escort=preload("res://core/escort_rules.gd")
const Identity=preload("res://core/actor_identity.gd")
const KINDS=["duel_start","encounter_fight","battle_plan","battle_close"]

## 检查当面切磋、真实道路接触和已存在会话；请求参数不能指定远方敌人或伪造胜利。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	if not "personal_combat" in data.required_capabilities: return fail("PersonalCombatUnavailable")
	if not state.actors.has(actor_id) or not state.actors[actor_id].alive: return fail("ActorUnavailable")
	if kind in ["battle_plan","battle_close"]:
		var battle_id: Variant=params.get("battle_id")
		if not battle_id is String or not state.battles.has(battle_id): return fail("UnknownBattle")
		var battle: Dictionary=state.battles[battle_id]
		if battle.controller_id!=actor_id: return fail("NotBattleController")
		if kind=="battle_close":
			if battle.phase!="result" or not battle.applied: return fail("NoBattleResult")
			return {"ok":true,"duration":0,"battle_id":battle_id,"result_preview":TacticalView.result(battle,data)}
		if battle.phase!="planning": return fail("BattleAlreadyResolved")
		var plan: Dictionary=params.duplicate(true); plan.erase("battle_id")
		var valid: Dictionary=Round.validate_plan(battle,data,actor_id,plan)
		if valid.ok:
			valid.battle_id=battle_id; valid.duration=0
			valid.tactical_preview=TacticalView.plan(battle,data,actor_id,valid.plan)
		return valid
	if Sessions.pauses_world(state): return fail("BattlePending")
	var quote: Dictionary={"ok":true,"duration":0,"fee":0,"wallet_id":"","controller_id":actor_id,"sides":{},"protected_id":""}
	if kind=="duel_start":
		var contact: Dictionary=Interaction.validate(state,data,actor_id,params.get("context"),"duel_start")
		if not contact.ok: return contact
		var role: Dictionary=data.roles[contact.context.role_id]
		if not role.has("sparring"): return fail("NoSparringAgreement")
		if not Condition.can_initiate_spar(state.actors[actor_id]): return fail("SevereInjuryCannotSpar")
		var terms: Dictionary=role.sparring
		quote.merge(terms,true)
		quote.duration=1
		quote.source={"kind":"role","context":contact.context.duplicate(true)}
		quote.sides[actor_id]=0; quote.sides[contact.context.provider_actor_id]=1
		if equipped(state,data,actor_id).is_empty(): return fail("WeaponRequired")
		if Inventory.spendable(state.wallets[state.actors[actor_id].wallet_id])<quote.fee: return fail("InsufficientFunds")
	else:
		var meeting: Dictionary=Encounters.preview(state,data,actor_id,"encounter_leave",params)
		if not meeting.ok: return meeting
		var encounter: Dictionary=state.encounters[meeting.encounter_id]
		if not actor_id in encounter.leaders: return fail("NotPartyLeader")
		var other: String=encounter.leaders[1] if encounter.leaders[0]==actor_id else encounter.leaders[0]
		var conflict: Dictionary=Identity.definition(state,data,other).get("road_conflict",{})
		if conflict.is_empty(): return fail("NoHostileEncounter")
		quote.merge(conflict,true)
		quote.source={"kind":"encounter","encounter_id":encounter.id}
		for person in Location.members(state,actor_id): quote.sides[person]=0
		for person in Location.members(state,other): quote.sides[person]=1
		for task in state.tasks.values():
			if task.assignee!=actor_id or task.status!="active": continue
			var goal: Dictionary=data.offers[task.offer_id].goal
			if goal.kind=="escort" and goal.actor_id in quote.sides and quote.sides[goal.actor_id]==0:
				quote.protected_id=goal.actor_id
				quote.objective_id=goal.objective_id
	if not data.personal_battlefields.has(quote.battlefield_id) or not data.personal_objectives.has(quote.objective_id): return fail("UnknownBattleDefinition")
	var objective: Dictionary=data.personal_objectives[quote.objective_id]
	if kind=="duel_start" and objective.kind!="sparring": return fail("WrongSparringObjective")
	if objective.kind=="escort" and quote.protected_id.is_empty(): return fail("ProtectedPersonMissing")
	var capacity: Array=[0,0]
	for spawn in data.personal_battlefields[quote.battlefield_id].spawns: capacity[spawn.side]+=1
	for person in quote.sides:
		if not state.actors[person].alive or not state.actors[person].available or Sessions.occupied(state,person): return fail("ParticipantBusy")
		capacity[quote.sides[person]]-=1
	if capacity[0]<0 or capacity[1]<0: return fail("BattlefieldCapacity")
	var stamina_ready: Dictionary=Stamina.starting_amount(objective)
	if not stamina_ready.ok: return stamina_ready
	var hit_ready: Dictionary=Hit.prepare(state,objective,quote.sides.keys())
	if not hit_ready.ok: return hit_ready
	var armor_ready: Dictionary=Armor.prepare(state,data,quote.sides.keys())
	if not armor_ready.ok: return armor_ready
	return quote

## 使用已经核验的人员和费用建立会话；切磋的所属半日由Kernel先推进，道路接触不重复收费。
static func begin(state: Dictionary, data: Dictionary, quote: Dictionary) -> Dictionary:
	var controller: String=quote.controller_id
	if quote.source.kind=="role" and not Condition.can_initiate_spar(state.actors[controller]): return fail("SevereInjuryCannotSpar")
	var stamina_ready: Dictionary=Stamina.starting_amount(data.personal_objectives[quote.objective_id])
	if not stamina_ready.ok: return stamina_ready
	var hit_ready: Dictionary=Hit.prepare(state,data.personal_objectives[quote.objective_id],quote.sides.keys())
	if not hit_ready.ok: return hit_ready
	var armor_ready: Dictionary=Armor.prepare(state,data,quote.sides.keys())
	if not armor_ready.ok: return armor_ready
	if quote.fee>0:
		var payer: Dictionary=state.wallets[state.actors[controller].wallet_id]
		if Inventory.spendable(payer)<quote.fee: return fail("InsufficientFunds")
		payer.balance-=quote.fee; state.wallets[quote.wallet_id].balance+=quote.fee
	var id: String=Factory.next_id(state,"battle")
	var battle: Dictionary={"id":id,"kind":"personal","controller_id":controller,"battlefield_id":quote.battlefield_id,"objective_id":quote.objective_id,"source":quote.source.duplicate(true),"created_tick":state.tick,"completed_tick":-1,"round":1,"phase":"planning","units":{},"ai_plans":{},"outcome":"","last_events":[],"result_id":"","applied":false}
	var spawns: Array=data.personal_battlefields[quote.battlefield_id].spawns
	var used: Array=[]
	var people: Array=quote.sides.keys(); people.sort(); people.erase(controller); people.push_front(controller)
	for person in people:
		var actor: Dictionary=state.actors[person]
		var side: int=quote.sides[person]
		var spawn: Dictionary={}
		for index in range(spawns.size()):
			if not index in used and spawns[index].side==side:
				spawn=spawns[index]; used.append(index); break
		if spawn.is_empty(): return fail("BattlefieldCapacity")
		var combat: Dictionary=actor.combat
		var learned: Dictionary=Eligibility.learned_snapshot(actor,data)
		var heart_id: String=combat.heart_id if learned.has(combat.heart_id) else ""
		battle.units[person]={"actor_id":person,"side":side,"position":spawn.position.duplicate(),"heading":spawn.heading,"radius":18,"hp":actor.hp,"initial_hp":actor.hp,"stamina":stamina_ready.stamina,"weapon_id":equipped(state,data,person),"martial":Condition.effective_martial(actor),"learned":learned,"heart_id":heart_id,"statuses":{},"recovery_until":0,"protected":person==quote.protected_id}
		if hit_ready.snapshots.has(person): battle.units[person].skill_levels=hit_ready.snapshots[person].duplicate(true)
		battle.units[person].merge(armor_ready.snapshots[person].duplicate(true))
		state.interactions.erase(person)
	battle.ai_plans=Round.make_ai(battle,data)
	state.battles[id]=battle
	return {"ok":true,"details":{"battle_id":id}}

## 提交一轮或关闭结果；只有本领域写回伤势与目标事实，不产生面板报酬。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, quote: Dictionary) -> Dictionary:
	var battle: Dictionary=state.battles[quote.battle_id]
	if kind=="battle_close":
		battle.phase="closed"
		return {"ok":true,"details":{"battle_id":battle.id,"outcome":battle.outcome}}
	var stream: Dictionary={}
	if Hit.enabled(data.personal_objectives[battle.objective_id]):
		var streams: Variant=state.get("rng")
		if not streams is Dictionary or not streams.get("duel") is Dictionary: return fail("InvalidDuelRandomState")
		stream=streams.duel.duplicate(true)
	var resolved: Dictionary=Round.resolve(battle,data,quote.plan,stream)
	if not resolved.ok: return resolved
	state.battles[battle.id]=resolved.battle
	if resolved.has("duel_stream"): state.rng.duel=resolved.duel_stream
	battle=state.battles[battle.id]
	if battle.phase=="result":
		var finished: Dictionary=_finish(state,data,battle)
		if not finished.ok: return finished
	return {"ok":true,"details":{"battle_id":battle.id,"round":battle.round,"outcome":battle.outcome}}

## 装备选择必须仍对应自己的实际随身物品；丢失或转移武器不会凭空生成替身装备。
static func equipped(state: Dictionary, data: Dictionary, actor_id: String) -> String:
	var actor: Dictionary=state.actors[actor_id]
	var id: String=actor.get("combat",{}).get("weapon_id","")
	if not data.items.has(id) or not data.items[id].has("weapon"): return ""
	return id if Inventory.count(state.inventories[actor.inventory_id],id,[{"kind":"actor","id":actor_id}])>0 else ""

## 为终局建立永久事实并恢复原道路暂停状态；被击倒造成伤势，切磋不会直接杀死人物。
static func _finish(state: Dictionary, data: Dictionary, battle: Dictionary) -> Dictionary:
	if battle.applied or state.facts.has(battle.id): return fail("BattleResultAlreadyApplied")
	var objective: Dictionary=data.personal_objectives[battle.objective_id]
	var defeated: Array=[]
	for person in battle.units:
		var unit: Dictionary=battle.units[person]
		var actor: Dictionary=state.actors[person]
		actor.hp=maxi(1,unit.hp)
		actor.fatigue=mini(100,actor.fatigue+6)
		if unit.hp==0:
			defeated.append(person)
			actor.injury=mini(3,actor.injury+objective.injury_on_defeat)
	battle.result_id=Factory.next_id(state,"result")
	battle.applied=true; battle.completed_tick=state.tick
	var participants: Array=battle.units.keys(); participants.sort(); defeated.sort()
	state.facts[battle.id]={"kind":"personal_battle","battle_id":battle.id,"result_id":battle.result_id,"tick":state.tick,"outcome":battle.outcome,"participants":participants,"defeated":defeated,"source":battle.source.duplicate(true)}
	var withdrawn: Array=Withdrawal.participants(battle.units)
	if not withdrawn.is_empty(): state.facts[battle.id].withdrawn=withdrawn
	Escort.record_battle(state,data,battle)
	if battle.source.kind=="encounter":
		var left: Dictionary=Encounters.apply(state,data,battle.controller_id,"encounter_leave",{"encounter_id":battle.source.encounter_id})
		if not left.ok: return left
	return {"ok":true}

## 校验新人物成长字段、会话和持久结算标记；坏档不得靠重新执行战斗来修补。
static func validate_state(state: Dictionary, data: Dictionary) -> Dictionary:
	if not state.get("battles") is Dictionary or not state.get("facts") is Dictionary: return fail("MissingCombatState")
	for actor in state.actors.values():
		if not whole(actor.get("injury"),0,3) or not actor.get("combat") is Dictionary: return fail("InvalidPersonalGrowth")
		var combat: Dictionary=actor.combat
		if not combat.get("weapon_id") is String or not combat.get("heart_id") is String or not combat.get("learned") is Dictionary: return fail("InvalidPersonalGrowth")
		if not combat.weapon_id.is_empty() and (not data.items.has(combat.weapon_id) or not data.items[combat.weapon_id].has("weapon")): return fail("UnknownEquippedWeapon")
		var armor_choice: Dictionary=ArmorDefinition.validate_choice(combat.get("armor_id",""),data.items)
		if not armor_choice.ok: return armor_choice
		for skill_id in combat.learned:
			if not data.personal_skills.has(skill_id) or not whole(combat.learned[skill_id],0,100): return fail("InvalidLearnedSkill")
		if not combat.heart_id.is_empty() and (not combat.learned.has(combat.heart_id) or data.personal_skills[combat.heart_id].kind!="heart"): return fail("InvalidHeartConfiguration")
	var occupied: Dictionary={}
	for id in state.battles:
		var battle: Variant=state.battles[id]
		if battle is Dictionary and battle.get("kind")=="military": continue
		if not battle is Dictionary or battle.get("id")!=id or battle.get("kind")!="personal" or not battle.get("controller_id") is String: return fail("InvalidBattleSession")
		if not battle.get("phase") in ["planning","result","closed"] or not battle.get("units") is Dictionary or not battle.get("ai_plans") is Dictionary or not battle.get("last_events") is Array: return fail("InvalidBattlePhase")
		if not battle.get("source") is Dictionary or not battle.get("outcome") is String or not battle.get("result_id") is String or not battle.get("applied") is bool: return fail("InvalidBattleSession")
		if not data.personal_battlefields.has(battle.get("battlefield_id")) or not data.personal_objectives.has(battle.get("objective_id")) or not battle.units.has(battle.controller_id): return fail("InvalidBattleDefinition")
		if not whole(battle.get("round"),1,101) or not whole(battle.get("created_tick"),0,state.tick): return fail("InvalidBattleTime")
		var arena: Dictionary=data.personal_battlefields[battle.battlefield_id]
		var source: Dictionary=battle.source
		if source.get("kind")=="role":
			if not source.get("context") is Dictionary or not data.roles.has(source.context.get("role_id")): return fail("InvalidBattleSource")
		elif source.get("kind")=="encounter":
			if not source.get("encounter_id") is String: return fail("InvalidBattleSource")
			if battle.phase=="planning" and (not state.encounters.has(source.encounter_id) or state.encounters[source.encounter_id].status!="pending"): return fail("BattleEncounterMissing")
		else: return fail("InvalidBattleSource")
		for person in battle.units:
			var unit: Variant=battle.units[person]
			if not state.actors.has(person) or not unit is Dictionary or unit.get("actor_id")!=person or not whole(unit.get("side"),0,1): return fail("InvalidBattleActor")
			if not unit.get("position") is Array or unit.get("radius")!=18 or not Geometry.point_valid(arena,unit.position,18): return fail("InvalidBattlePosition")
			if not whole(unit.get("hp"),0,100) or not whole(unit.get("initial_hp"),1,100) or not whole(unit.get("stamina"),0,100) or not whole(unit.get("heading"),0,359): return fail("InvalidBattleVitals")
			if not unit.get("statuses") is Dictionary or not unit.get("learned") is Dictionary or not unit.get("protected") is bool or not whole(unit.get("recovery_until"),0,105): return fail("InvalidBattleUnit")
			if not unit.get("weapon_id") is String or not unit.get("heart_id") is String or not whole(unit.get("martial"),1,110): return fail("InvalidBattleEquipment")
			if not unit.weapon_id.is_empty() and (not data.items.has(unit.weapon_id) or not data.items[unit.weapon_id].has("weapon")): return fail("InvalidBattleEquipment")
			for skill in unit.learned:
				if not data.personal_skills.has(skill) or not whole(unit.learned[skill],0,100): return fail("InvalidBattleSkill")
			if not unit.heart_id.is_empty() and (not unit.learned.has(unit.heart_id) or data.personal_skills[unit.heart_id].kind!="heart"): return fail("InvalidBattleHeart")
			for status in unit.statuses:
				if not status in ["slow","exposed"] or not whole(unit.statuses[status],1,105): return fail("InvalidBattleStatus")
			var withdrawn: Dictionary=Withdrawal.validate_unit(battle,data,person)
			if not withdrawn.ok: return withdrawn
			var armor_snapshot: Dictionary=Armor.validate_snapshot(state,data,person,unit,battle.phase=="planning")
			if not armor_snapshot.ok: return armor_snapshot
			var hit_snapshot: Dictionary=Hit.validate_unit(unit,data.personal_objectives[battle.objective_id],state.actors[person],battle.phase=="planning")
			if not hit_snapshot.ok: return hit_snapshot
			if battle.phase=="planning":
				var qualified: Dictionary=Eligibility.validate_snapshot(state.actors[person],data,unit)
				if not qualified.ok: return qualified
			if battle.phase!="closed":
				if occupied.has(person): return fail("DoubleBattleOccupation")
				occupied[person]=id
			if battle.phase=="planning" and state.actors[person].hp!=unit.initial_hp: return fail("BattleWorldVitalsChanged")
		if battle.phase=="planning":
			if battle.applied or not battle.result_id.is_empty() or battle.outcome!="" or battle.get("completed_tick")!=-1: return fail("PrematureBattleResult")
			if battle.ai_plans!=Round.make_ai(battle,data): return fail("InvalidCommittedAIPlan")
		else:
			if not battle.outcome in ["victory","defeat","draw","withdrawn","protected_lost"]: return fail("InvalidBattleOutcome")
			if not battle.applied or battle.result_id.is_empty() or not state.facts.has(id) or not whole(battle.get("completed_tick"),battle.created_tick,state.tick): return fail("MissingBattleResult")
			var fact: Variant=state.facts[id]
			var participants: Array=battle.units.keys(); participants.sort()
			var defeated: Array=[]
			for person in participants:
				if battle.units[person].hp==0: defeated.append(person)
			if not fact is Dictionary or fact.get("kind")!="personal_battle" or fact.get("battle_id")!=id or fact.get("result_id")!=battle.result_id or fact.get("outcome")!=battle.outcome or fact.get("source")!=battle.source or fact.get("tick")!=battle.completed_tick or fact.get("participants")!=participants or fact.get("defeated")!=defeated: return fail("BattleResultMismatch")
			var withdrawn: Array=Withdrawal.participants(battle.units)
			if fact.get("withdrawn",[])!=withdrawn or (withdrawn.is_empty() and fact.has("withdrawn")): return fail("BattleWithdrawalMismatch")
		var objective_units: Dictionary=Objectives.validate_battle(battle,data.personal_objectives[battle.objective_id])
		if not objective_units.ok: return objective_units
		var objective_result: Dictionary=Objectives.validate_result(battle,data.personal_objectives[battle.objective_id],arena)
		if not objective_result.ok: return objective_result
		var geometry: Dictionary=Geometry.resolve_moves(arena,Withdrawal.bodies(battle.units),{})
		if geometry.has("code"): return fail("BattleBodyOverlap")
	return {"ok":true}

## 运行状态整数必须保持原始类型，拒绝浮点、布尔及越界值。
static func whole(value: Variant, minimum: int, maximum: int) -> bool:
	return typeof(value)==TYPE_INT and value>=minimum and value<=maximum

## 将失败交给统一候选事务丢弃，原世界、费用与存档均不变。
static func fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
