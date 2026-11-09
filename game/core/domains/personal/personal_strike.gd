## 主动秘技的局部发动与射程合同；不识别技能ID，不换算旧格数，不建立蓄力或额外动作状态。
extends RefCounted
const Values=preload("res://core/base/value_checks.gd")
const MAXIMUM_BONUS: int=2000

## 两项扩展只允许strike；缺省保旧，原地标志只接受显式true，外射程增益使用当前整数规则坐标。
static func validate_skill(skill: Dictionary, path: String="") -> Dictionary:
	if skill.has("requires_stationary"):
		if skill.get("kind")!="strike" or typeof(skill.requires_stationary)!=TYPE_BOOL or not skill.requires_stationary: return {"ok":false,"code":"PERSONAL_STRIKE_STATIONARY","path":path+"/requires_stationary"}
	if skill.has("range_modifier"):
		var modifier: Variant=skill.range_modifier
		if skill.get("kind")!="strike" or not modifier is Dictionary or modifier.size()!=1 or not Values.whole(modifier.get("maximum_bonus"),1,MAXIMUM_BONUS): return {"ok":false,"code":"PERSONAL_STRIKE_RANGE","path":path+"/range_modifier"}
	return {"ok":true}

## 只增加外射程，原武器的内圈、射界、视线语义和全部成本保持原值；返回副本，不污染装备定义。
static func weapon(original: Dictionary, skill: Dictionary) -> Dictionary:
	var result: Dictionary=original.duplicate(true)
	if skill.has("range_modifier"): result.maximum_range+=skill.range_modifier.maximum_bonus
	return result

## 同时核对提交终点和移动后真实位置；没有实际位置时只作提交预览，未知强制位移也不能绕过原地要求。
static func condition(skill: Dictionary, start: Array, planned: Array, actual: Array=[]) -> Dictionary:
	if skill.get("requires_stationary",false) and (planned!=start or (not actual.is_empty() and actual!=start)): return {"ok":false,"code":"StrikeRequiresStationary"}
	return {"ok":true}
