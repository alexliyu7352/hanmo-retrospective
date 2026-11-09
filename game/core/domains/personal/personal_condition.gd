## 个人战入场身体效率；沿03§3.3疲劳分档与05§5.4较小效率组合，不修改人物基础真值或HP上限。
extends RefCounted
const INJURY_EFFICIENCY: Array[int]=[10000,9000,7500,6000]

## 输入来自已校验人物状态；疲劳与伤势取较小值，不相乘叠罚，也不在战斗每轮重新读取世界。
static func efficiency_bp(actor: Dictionary) -> int:
	var fatigue: int=actor.fatigue
	var fatigue_bp: int=10000
	if fatigue>=90: fatigue_bp=6000
	elif fatigue>=70: fatigue_bp=7500
	elif fatigue>=40: fatigue_bp=9000
	return mini(fatigue_bp,INJURY_EFFICIENCY[actor.get("injury",0)])

## 在既有martial快照字段存入向下取整后的有效武力，最低一；原属性、技能和疲劳均不写回。
static func effective_martial(actor: Dictionary) -> int:
	@warning_ignore("integer_division")
	var effective: int=actor.attributes.martial*efficiency_bp(actor)/10000
	return maxi(1,effective)

## 重伤者不能主动切磋；这不是禁止参加真实遭遇中的自卫。
static func can_initiate_spar(actor: Dictionary) -> bool:
	return actor.get("injury",0)<3
