## 军事领域在同一世界的接线边界；初始化真实名册并复核据点归属，不自行推进时钟或制造兵粮。
extends RefCounted
const Training=preload("res://core/domains/military/training_rules.gd")
const Assets=preload("res://core/military_assets.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const Combat=preload("res://core/military_combat.gd")
const SiegeWorld=preload("res://core/siege_world.gd")
const Location=preload("res://core/world_location.gd")
const TABLES=["offices","garrisons","military_units","armies","campaigns","strongholds","sieges"]

## 新局从现有作者定义登记，旧世界只有显式迁移后的空集合；不在读档时重新补满驻军。
static func initialize(state: Dictionary, data: Dictionary) -> Dictionary:
	var fresh_units: bool=not state.has("military_units")
	var checked: Dictionary=Campaign.initialize(state,data)
	if not checked.ok: return checked
	checked=Assets.initialize(state,data)
	if not checked.ok: return checked
	if fresh_units:
		for camp in data.get("garrisons",{}).values():
			for definition in camp.units:
				if not definition.has("initial_training") and not definition.has("initial_fatigue"): continue
				var profile: Dictionary=Training.initial_profile(definition)
				if not profile.ok: return profile
				state.military_units[definition.id].merge(profile.profile)
	if not state.has("strongholds"):
		state.strongholds={}
		for id in data.get("strongholds",{}):
			var definition: Dictionary=data.strongholds[id]
			state.strongholds[id]={"id":id,"owner_organization_id":data.garrisons[definition.garrison_id].organization_id,"occupation_id":"","siege_id":""}
	if not state.has("sieges"): state.sieges={}
	return validate_state(state,data)

## 稳定边界逐域校验，先排除缺字段，再检查人物唯一行程和法定军事来源。
static func validate_state(state: Dictionary, data: Dictionary) -> Dictionary:
	for key in TABLES:
		if not state.get(key) is Dictionary: return {"ok":false,"code":"InvalidMilitaryState"}
	var checked: Dictionary=Assets.validate_state(state,data)
	if not checked.ok: return checked
	checked=Training.validate_state(state,data)
	if not checked.ok: return checked
	checked=Campaign.validate_state(state,data)
	if not checked.ok: return checked
	checked=Combat.validate_state(state,data)
	if not checked.ok: return checked
	if state.strongholds.size()!=data.get("strongholds",{}).size(): return {"ok":false,"code":"MissingStrongholdState"}
	for id in state.strongholds:
		var hold: Variant=state.strongholds[id]
		if not hold is Dictionary or hold.get("id")!=id or not data.get("strongholds",{}).has(id): return {"ok":false,"code":"InvalidStrongholdState"}
		var ownership: Dictionary=Assets.garrison_owner(state,data,data.strongholds[id].garrison_id)
		if not ownership.ok: return ownership
		if hold.get("owner_organization_id")!=ownership.organization_id or hold.get("occupation_id")!=ownership.occupation_id or not hold.get("siege_id") is String: return {"ok":false,"code":"InvalidStrongholdOwner"}
	for actor in state.actors.values():
		if actor.location.get("kind")!="InArmy": continue
		if Location.resolve(state,actor.id).is_empty(): return {"ok":false,"code":"DanglingArmyLocation"}
		var army: Dictionary=state.armies[actor.location.army_id]
		if actor.id in army.member_actor_ids:
			var action_id: String=state.actors[army.commander_actor_id].action_id
			if not action_id.is_empty() and state.actions.has(action_id) and state.actions[action_id].kind=="travel" and actor.action_id!=action_id: return {"ok":false,"code":"ArmyOccupationMismatch"}
	return SiegeWorld.validate_state(state,data)

## 未关闭会战暂停所有参战军团统帅的宏观路线，统帅无需恰好属于该节点参战部队。
static func held_actors(state: Dictionary) -> Array:
	var held: Array=[]
	for battle in state.battles.values():
		if battle.get("kind")!="military" or battle.phase=="closed": continue
		for army_id in battle.army_ids:
			for person in state.armies[army_id].member_actor_ids:
				if not person in held: held.append(person)
	return held

## 同一候选内撤销已失效的会战指挥，防止易帜或真人战损留下无法提交下一轮的会话。
static func release_invalid_commands(state: Dictionary, data: Dictionary, tick: int) -> Dictionary:
	var reasons: Dictionary={}
	for campaign in state.get("campaigns",{}).values():
		if campaign.army_id.is_empty() or state.armies[campaign.army_id].status!="active": continue
		var person: Dictionary=state.actors[campaign.actor_id]
		var office: Dictionary=state.offices[campaign.office_id]
		if not person.alive or not person.available: reasons[campaign.army_id]="responsible_actor_lost"
		elif not office.enabled or office.holder_actor_id!=person.id: reasons[campaign.army_id]="authority_lost"
		elif campaign.objective_result.get("kind") in ["expired","cancelled","withdrawn"]: reasons[campaign.army_id]="cancelled"
	if reasons.is_empty(): return {"ok":true}
	for siege in state.sieges.values():
		if siege.phase!="active": continue
		var army_id: String=state.campaigns[siege.campaign_id].army_id
		if not reasons.has(army_id): continue
		var stopped: Dictionary=SiegeWorld.terminate(state,data,siege.id,reasons[army_id],tick)
		if not stopped.ok: return stopped
		apply_actor_changes(state,stopped.details.actor_changes,data)
	for battle in state.battles.values():
		if battle.get("kind")!="military" or battle.phase=="closed": continue
		for army_id in battle.army_ids:
			if not reasons.has(army_id): continue
			var stopped: Dictionary=Combat.terminate(state,data,battle.id,reasons[army_id],tick)
			if not stopped.ok: return stopped
			apply_actor_changes(state,stopped.actor_changes,data)
			var nodes: Dictionary=SiegeWorld.battles_finished(state,data,stopped.completed_battle_ids,tick)
			if not nodes.ok: return nodes
			break
	return {"ok":true}

## 真人战损只依据已落账分类回写；阵亡和逃散留在真实战场，俘虏随实际看管军团或已抵达驻军。
static func apply_actor_changes(state: Dictionary, changes: Array, data: Dictionary={}) -> void:
	for change in changes:
		var actor: Dictionary=state.actors[change.actor_id]
		var actual: Dictionary=actor.location.duplicate(true)
		if actual.get("kind")=="InArmy":
			var previous: Dictionary=state.armies[actual.army_id]
			actual=state.actors[previous.commander_actor_id].location.duplicate(true)
		if change.status in ["dead","captured","deserted"]:
			if not actor.action_id.is_empty() and state.actions.has(actor.action_id):
				var action: Dictionary=state.actions[actor.action_id]
				if action.kind=="travel" and action.actor_id!=actor.id:
					action.participants.erase(actor.id); actor.action_id=""
			actor.location=actual
		match change.status:
			"dead": actor.alive=false; actor.available=false; actor.hp=0
			"deserted": actor.available=false
			"wounded": actor.injury=maxi(1,actor.injury); actor.hp=mini(actor.hp,70)
			"captured":
				actor.available=false
				if change.has("garrison_id"):
					var place_id: String=data.garrisons[change.garrison_id].place_id
					actor.location={"kind":"Settlement","place_id":place_id,"settlement_id":data.places[place_id].settlement_id}
				else: actor.location={"kind":"InArmy","army_id":change.captor_army_id}
