## 将有限公开个人战页映射为只读动作片段；只保真实起止锚点，不重建核心未保存的中间轨迹。
extends RefCounted
const BINDING_FIELDS: Dictionary = {"kernel_id":TYPE_INT,"load_generation":TYPE_INT,"actor_id":TYPE_STRING,"world_id":TYPE_STRING,"execution_epoch":TYPE_INT}
const ACTOR_FIELDS: Dictionary = {"from_position":TYPE_ARRAY,"to_position":TYPE_ARRAY,"from_hp":TYPE_INT,"hp":TYPE_INT,"heading":TYPE_INT,"side":TYPE_INT,"protected":TYPE_BOOL,"from_present":TYPE_BOOL,"present":TYPE_BOOL,"from_statuses":TYPE_DICTIONARY,"statuses":TYPE_DICTIONARY}

## 捕获当前公开终态及既有身份戳；任何必要字段损坏均拒绝，不读取完整世界或敌方装备。
static func capture(page_input: Dictionary, binding: Dictionary) -> Dictionary:
	var source: Dictionary = _binding(binding)
	if source.is_empty() or page_input.get("ok") != true or page_input.get("actor_id") != source.actor_id: return _invalid()
	if not _whole(page_input.get("revision")) or not page_input.get("battle_id") is String or page_input.battle_id.is_empty(): return _invalid()
	var battle: Variant = page_input.get("battle")
	if not battle is Dictionary or battle.get("id") != page_input.battle_id: return _invalid()
	if not _whole(battle.get("round"),1) or not battle.get("phase") in ["planning","result","closed"] or not battle.get("units") is Dictionary: return _invalid()
	if not battle.get("controller_id") is String or not battle.units.has(battle.controller_id) or not battle.units.has(source.actor_id): return _invalid()
	if not page_input.get("present_actor_ids") is Array or not page_input.get("events") is Array: return _invalid()
	var present: Dictionary = {}
	for id in page_input.present_actor_ids:
		if not id is String or not battle.units.has(id) or present.has(id): return _invalid()
		present[id] = true
	var actors: Dictionary = {}
	for id in battle.units:
		var unit: Variant = battle.units[id]
		if not id is String or id.is_empty() or not unit is Dictionary: return _invalid()
		if not _point(unit.get("position")) or not _whole(unit.get("hp")) or not _whole(unit.get("heading")) or unit.heading >= 360: return _invalid()
		if not unit.get("side") is int or not unit.get("protected") is bool or not _statuses(unit.get("statuses")): return _invalid()
		actors[id] = {"from_position":unit.position.duplicate(),"to_position":unit.position.duplicate(),"from_hp":unit.hp,"hp":unit.hp,"heading":unit.heading,"side":unit.side,"protected":unit.protected,"from_present":present.has(id),"present":present.has(id),"from_statuses":unit.statuses.duplicate(true),"statuses":unit.statuses.duplicate(true)}
	return {"ok":true,"binding":source,"battle_id":battle.id,"revision":page_input.revision,"round":battle.round,"phase":battle.phase,"controller_id":battle.controller_id,"actors":actors}

## 仅同源相邻结算使用旧锚点；冷恢复、跳轮、重复页和换来源一律交回当前真实终态。
static func prepare(previous: Dictionary, page_input: Dictionary, binding: Dictionary) -> Dictionary:
	var current: Dictionary = capture(page_input,binding)
	if not current.ok: return _invalid()
	var clip: Dictionary = {"ok":true,"playable":false,"reason":"MissingPreviousFrame","id":_identity({},current),"binding":current.binding.duplicate(true),"battle_id":current.battle_id,"actors":current.actors.duplicate(true),"phases":[]}
	if not _anchor(previous): return clip
	clip.id = _identity(previous,current)
	if previous.binding != current.binding or previous.battle_id != current.battle_id or previous.controller_id != current.controller_id:
		clip.reason = "ChangedPlaybackSource"
		return clip
	if current.revision != previous.revision+1 or previous.phase != "planning" or not ((current.round == previous.round+1 and current.phase == "planning") or (current.round == previous.round and current.phase == "result")):
		clip.reason = "NonAdjacentSettlement"
		return clip
	if previous.actors.size() != current.actors.size():
		clip.reason = "ChangedParticipants"
		return clip
	for id in current.actors:
		if not previous.actors.has(id) or previous.actors[id].side != current.actors[id].side or previous.actors[id].protected != current.actors[id].protected:
			clip.reason = "ChangedParticipants"
			return clip
	var moved: bool = false
	var state_changed: bool = false
	for id in clip.actors:
		var actor: Dictionary = clip.actors[id]
		var prior: Dictionary = previous.actors[id]
		actor.from_position = prior.to_position.duplicate()
		actor.from_hp = prior.hp
		actor.from_present = prior.present
		actor.from_statuses = prior.statuses.duplicate(true)
		moved = moved or actor.from_position != actor.to_position
		state_changed = state_changed or actor.from_present != actor.present or actor.from_statuses != actor.statuses or (actor.from_hp > 0 and actor.hp == 0)
	var strike: Array = []
	var impact: Array = []
	var state: Array = []
	for raw in page_input.events:
		var event: Dictionary = _event(raw,clip.actors)
		if event.is_empty(): continue
		if event.action in ["attack","counter"] or (event.action == "skill" and event.code != "Guarded"):
			impact.append(event)
			if event.code in ["Hit","Miss"]: strike.append(event.duplicate(true))
		elif event.action == "defend" or (event.action == "skill" and event.code == "Guarded"):
			impact.append(event)
		else:
			state.append(event)
	clip.phases = [_phase("prepare",0.12,[]),_phase("move",0.24 if moved else 0.0,[]),_phase("strike",0.20 if not strike.is_empty() else 0.0,strike),_phase("impact",0.22 if not impact.is_empty() else 0.0,impact),_phase("state",0.18 if state_changed or not state.is_empty() else 0.0,state),_phase("settle",0.08,[])]
	clip.playable = true
	clip.reason = "AdjacentSettledRound"
	return clip

## 显式挑选五个来源字段，过滤额外元数据；GD可使用空world_id和零epoch，其他戳仍须存在。
static func _binding(value: Dictionary) -> Dictionary:
	var result: Dictionary = {}
	for key in BINDING_FIELDS:
		if typeof(value.get(key)) != BINDING_FIELDS[key]: return {}
		result[key] = value[key]
	if result.kernel_id == 0 or result.load_generation < 0 or result.actor_id.is_empty() or result.execution_epoch < 0: return {}
	return result

## 旧锚点只接受本模块的有限终态形状；不能借损坏元数据引入假起点或无效引用。
static func _anchor(value: Dictionary) -> bool:
	if value.get("ok") != true or not value.get("binding") is Dictionary or _binding(value.binding).is_empty(): return false
	if not value.get("battle_id") is String or value.battle_id.is_empty() or not value.get("controller_id") is String: return false
	if not _whole(value.get("revision")) or not _whole(value.get("round"),1) or not value.get("phase") in ["planning","result","closed"]: return false
	if not value.get("actors") is Dictionary or not value.actors.has(value.controller_id) or not value.actors.has(value.binding.actor_id): return false
	for id in value.actors:
		if not id is String or not value.actors[id] is Dictionary: return false
		var actor: Dictionary = value.actors[id]
		for key in ACTOR_FIELDS:
			if typeof(actor.get(key)) != ACTOR_FIELDS[key]: return false
		if not _point(actor.to_position) or not _point(actor.from_position) or not _whole(actor.hp) or not _whole(actor.from_hp): return false
		if not _whole(actor.heading) or actor.heading >= 360 or not _statuses(actor.statuses) or not _statuses(actor.from_statuses): return false
		if actor.from_position != actor.to_position or actor.from_hp != actor.hp or actor.from_present != actor.present or actor.from_statuses != actor.statuses: return false
	return true

## 事件逐动作挑选公开字段；守势不是格挡，成功退离与状态须有当前公开终态佐证。
static func _event(value: Variant, actors: Dictionary) -> Dictionary:
	if not value is Dictionary or not value.get("action") is String: return {}
	var action: String = value.action
	if action in ["objective","outcome"]:
		return {"action":action,"code":value.code} if value.get("code") is String else {}
	if not action in ["attack","skill","counter","defend","wait","withdraw","stamina","status"]: return {}
	if not value.get("actor_id") is String or not actors.has(value.actor_id): return {}
	var row: Dictionary = {"action":action,"actor_id":value.actor_id}
	if action == "status":
		if not value.get("target_id") is String or not actors.has(value.target_id) or not value.get("status") is String or not _whole(value.get("expires_round")): return {}
		if actors[value.target_id].statuses.get(value.status,-1) < value.expires_round: return {}
		row["target_id"] = value.target_id; row["status"] = value.status; row["expires_round"] = value.expires_round
	elif action == "stamina":
		if not _whole(value.get("amount")): return {}
		row["amount"] = value.amount
	else:
		if not value.get("code") is String: return {}
		row["code"] = value.code
		if action in ["attack","counter"] or (action == "skill" and value.code != "Guarded"):
			if not value.get("target_id") is String or not actors.has(value.target_id) or not _whole(value.get("damage")): return {}
			if value.code != "Hit" and value.damage != 0: return {}
			row["target_id"] = value.target_id; row["damage"] = value.damage
		elif action == "withdraw" and value.code == "Withdrawn" and (actors[value.actor_id].present or actors[value.actor_id].hp <= 0): return {}
		elif action == "defend" and value.code != "Guarded": return {}
		elif action == "wait" and value.code != "Rested": return {}
		if action in ["attack","skill","counter"] and value.get("skill_id") is String: row["skill_id"] = value.skill_id
	return row

## 片段身份仅由已有来源与前后版本、战场、轮次构造；不取随机或创建新世界标识。
static func _identity(previous: Dictionary, current: Dictionary) -> String:
	return JSON.stringify([current.binding,previous.get("revision",-1),current.revision,current.battle_id,previous.get("round",-1),current.round,previous.get("phase",""),current.phase],"",true).sha256_text()

## 所有伤害事件同处impact阶段；阶段长度只影响界面时间，不回写伤害或命令。
static func _phase(kind: String, duration: float, events: Array) -> Dictionary:
	return {"kind":kind,"duration":duration,"events":events}

## 锚点坐标只接受真实整数二元组，不推断缺失值或中间路径。
static func _point(value: Variant) -> bool:
	return value is Array and value.size() == 2 and value[0] is int and value[1] is int

## 已知公开状态只复制字符串键与整轮到期值，拒绝对象及嵌套私密结构。
static func _statuses(value: Variant) -> bool:
	if not value is Dictionary: return false
	for key in value:
		if not key is String or not value[key] is int: return false
	return true

## 整数身份与数值不能用浮点、布尔或负值伪装。
static func _whole(value: Variant, minimum: int = 0) -> bool:
	return value is int and value >= minimum

## 当前页损坏时返回空片段，宿主继续使用自己的真实终态或错误入口。
static func _invalid() -> Dictionary:
	return {"ok":false,"playable":false,"reason":"InvalidPersonalPlaybackInput","id":"","binding":{},"battle_id":"","actors":{},"phases":[]}
