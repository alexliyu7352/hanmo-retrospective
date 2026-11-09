## 反击首个合法机会的纯选择器；不扣血、收费、掷随机或生成世界事实，也不消费反击产生的事件。
extends RefCounted
const Geometry=preload("res://core/personal_geometry.gd")
const Values=preload("res://core/base/value_checks.gd")

## 首片冻结为guard上的可选40%反击；只支持自身近身武器，缺字段完全保留旧守势。
static func validate_skill(skill: Dictionary, path: String="") -> Dictionary:
	if not skill.has("counter"): return {"ok":true}
	if skill.get("kind")!="guard" or not Values.fields(skill.counter,{"damage_bp":TYPE_INT}) or skill.counter.size()!=1 or skill.counter.damage_bp!=4000: return {"ok":false,"code":"PERSONAL_COUNTER_DEFINITION","path":path+"/counter"}
	if not Values.fields(skill.get("required_skill_level"),{"skill_id":TYPE_STRING,"level":TYPE_INT}) or skill.required_skill_level!={"skill_id":"martial","level":3}: return {"ok":false,"code":"PERSONAL_COUNTER_REQUIREMENT","path":path+"/required_skill_level"}
	if not skill.get("weapons") is Array or skill.weapons.is_empty(): return {"ok":false,"code":"PERSONAL_COUNTER_WEAPON","path":path+"/weapons"}
	for category in skill.weapons:
		if not category in ["sword","spear"]: return {"ok":false,"code":"PERSONAL_COUNTER_WEAPON","path":path+"/weapons"}
	return {"ok":true}

## 原攻击按稳定身份排定首次合法机会；不合法的侧后、射程或视线机会不消耗一次触发额度。
static func select(battle: Dictionary, data: Dictionary, declarations: Dictionary, primary_hits: Array) -> Dictionary:
	for actor_id in declarations:
		if not battle.units.has(actor_id) or typeof(declarations[actor_id])!=TYPE_INT or declarations[actor_id]!=4000: return {"ok":false,"code":"InvalidCounterDeclaration"}
	var ordered: Array=[]
	for hit in primary_hits:
		if not Values.fields(hit,{"actor_id":TYPE_STRING,"target_id":TYPE_STRING,"action":TYPE_STRING,"code":TYPE_STRING}): return {"ok":false,"code":"InvalidCounterHit"}
		if hit.code!="Hit" or not hit.action in ["attack","skill"]: continue
		if not battle.units.has(hit.actor_id) or not battle.units.has(hit.target_id): return {"ok":false,"code":"InvalidCounterHit"}
		ordered.append(hit)
	ordered.sort_custom(_hit_before)
	var used: Dictionary={}; var reactions: Array=[]
	for hit in ordered:
		if not declarations.has(hit.target_id) or used.has(hit.target_id): continue
		var attacker: Dictionary=battle.units[hit.actor_id]
		var counter: Dictionary=battle.units[hit.target_id]
		if attacker.side==counter.side: continue
		var incoming_weapon: Dictionary=data.items.get(attacker.weapon_id,{}).get("weapon",{})
		var weapon: Dictionary=data.items.get(counter.weapon_id,{}).get("weapon",{})
		if not incoming_weapon.get("category") in ["sword","spear"] or not weapon.get("category") in ["sword","spear"]: continue
		var legal: Dictionary=Geometry.can_attack(data.personal_battlefields[battle.battlefield_id],counter.position,attacker.position,counter.heading,weapon)
		if not legal.ok: continue
		used[hit.target_id]=true
		reactions.append({"actor_id":hit.target_id,"target_id":hit.actor_id,"damage_bp":declarations[hit.target_id]})
	return {"ok":true,"reactions":reactions}

## 相同原攻击集合不因字典或调用方数组顺序改变首次反击对象。
static func _hit_before(first: Dictionary, second: Dictionary) -> bool:
	return first.target_id<second.target_id if first.actor_id==second.actor_id else first.actor_id<second.actor_id
