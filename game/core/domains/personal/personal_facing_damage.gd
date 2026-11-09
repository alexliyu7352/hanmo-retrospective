## 作者目标显式启用的侧背候选倍率；旧定义缺省为100%，只读既有位置和朝向。
extends RefCounted
const Guard=preload("res://core/domains/personal/personal_guard.gd")
const Values=preload("res://core/base/value_checks.gd")

## 首片只接受已批准的110%侧击/125%背击组合，不将任意作者对象解释成通用伤害脚本。
static func validate_objective(objective: Dictionary, path: String="") -> Dictionary:
	if not objective.has("facing_damage"): return {"ok":true}
	var profile: Variant=objective.facing_damage
	if not Values.fields(profile,{"side_bp":TYPE_INT,"back_bp":TYPE_INT}) or profile.size()!=2 or profile.side_bp!=11000 or profile.back_bp!=12500: return {"ok":false,"code":"PERSONAL_FACING_DAMAGE","path":path+"/facing_damage"}
	return {"ok":true}

## 从攻击者与目标的实际位置判断目标受击方向；普通攻击和反击复用同一方向定义。
static func multiplier(attacker: Dictionary, target: Dictionary, profile: Dictionary={}) -> int:
	if profile.is_empty(): return 10000
	match Guard.sector(target.position,target.heading,attacker.position):
		"side": return profile.side_bp
		"back": return profile.back_bp
	return 10000
