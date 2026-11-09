## 长兵主动招式的封闭线形/扇区多目标规则；保留主目标，不换算旧格数，不执行作者脚本。
extends RefCounted
const Geometry=preload("res://core/personal_geometry.gd")
const Values=preload("res://core/base/value_checks.gd")

## 首片仅长兵strike允许area；线宽和扇角为显式连续规则值，字段与目标预算严格互斥。
static func validate_skill(skill: Dictionary, path: String="") -> Dictionary:
	if not skill.has("area"): return {"ok":true}
	var area: Variant=skill.area
	if skill.get("kind")!="strike" or skill.get("weapons")!=["spear"] or not area is Dictionary: return _definition_failure(path)
	if area.get("shape")=="line":
		if area.size()!=3 or not Values.whole(area.get("half_width"),1,2000) or not Values.whole(area.get("max_targets"),2,2): return _definition_failure(path)
	elif area.get("shape")=="sector":
		if area.size()!=3 or not Values.whole(area.get("arc_degrees"),1,180) or not Values.whole(area.get("max_targets"),2,3): return _definition_failure(path)
	else: return _definition_failure(path)
	return {"ok":true}

## 主目标须在移动后的同一几何/区域中成立；附加敌人按距离再ID选，返回目标ID顺序供唯一随机流消费。
static func select(battle: Dictionary, data: Dictionary, actor_id: String, target_id: String, skill: Dictionary, weapon: Dictionary) -> Dictionary:
	var contract: Dictionary=validate_skill(skill)
	if not contract.ok: return contract
	var attacker: Dictionary=battle.units[actor_id]
	if not battle.units.has(target_id) or not _enemy(attacker,battle.units[target_id]): return {"ok":false,"code":"InvalidTarget"}
	var arena: Dictionary=data.personal_battlefields[battle.battlefield_id]
	var primary: Dictionary=Geometry.can_attack(arena,attacker.position,battle.units[target_id].position,attacker.heading,weapon)
	if not primary.ok: return primary
	if not skill.has("area"): return {"ok":true,"target_ids":[target_id]}
	var area: Dictionary=skill.area
	if not _inside(attacker,battle.units[target_id],area): return {"ok":false,"code":"OutsideStrikeArea"}
	var extras: Array=[]
	for id in battle.units:
		if id==target_id or not _enemy(attacker,battle.units[id]): continue
		var target: Dictionary=battle.units[id]
		if _inside(attacker,target,area) and Geometry.can_attack(arena,attacker.position,target.position,attacker.heading,weapon).ok: extras.append(id)
	extras.sort_custom(func(first: String, second: String) -> bool:
		var a: float=_distance_squared(attacker.position,battle.units[first].position)
		var b: float=_distance_squared(attacker.position,battle.units[second].position)
		return first<second if a==b else a<b)
	var selected: Array=[target_id]
	for id in extras:
		if selected.size()>=area.max_targets: break
		selected.append(id)
	selected.sort()
	return {"ok":true,"target_ids":selected}

## 同侧与已经离场/倒地者不进入区域；友军没有新增误伤，也不替代原障碍视线。
static func _enemy(attacker: Dictionary, target: Dictionary) -> bool:
	return target.side!=attacker.side and target.hp>0 and not target.has("withdrawn_round")

## 区域以人物最终朝向为轴且仅比较目标中心；射程与原武器射界由前置几何再次限制。
static func _inside(attacker: Dictionary, target: Dictionary, area: Dictionary) -> bool:
	var dx: float=float(target.position[0])-attacker.position[0]
	var dy: float=float(target.position[1])-attacker.position[1]
	if area.shape=="line":
		var axis: Vector2=Vector2.from_angle(deg_to_rad(attacker.heading))
		return dx*axis.x+dy*axis.y>Geometry.EPSILON and absf(dx*axis.y-dy*axis.x)<=area.half_width+Geometry.EPSILON
	var angle: float=rad_to_deg(atan2(dy,dx))
	var difference: float=absf(fposmod(angle-attacker.heading+180.0,360.0)-180.0)
	return difference<=area.arc_degrees/2.0+Geometry.EPSILON

## 距离排序不受字典顺序影响；距离只选附加目标，不决定伤害或随机消费顺序。
static func _distance_squared(first: Array, second: Array) -> float:
	var dx: float=float(first[0])-second[0]; var dy: float=float(first[1])-second[1]
	return dx*dx+dy*dy

## 结构/语义失败统一返回可定位作者路径，不回退成单体技能。
static func _definition_failure(path: String) -> Dictionary:
	return {"ok":false,"code":"PERSONAL_STRIKE_AREA","path":path+"/area"}
