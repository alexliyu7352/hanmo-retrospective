## 普通防御与显式正面守势的方向查询；只读移动后坐标和已提交朝向，不保存新状态或修改伤害基数。
extends RefCounted
const SIDE_TOLERANCE: float=0.0000000001

## 新方向字段仅限守势技能并只识别front；缺省保持旧全向秘技，未知值不能静默降级。
static func validate_skill(skill: Dictionary, path: String="") -> Dictionary:
	if skill.has("guard_facing") and (skill.get("kind")!="guard" or typeof(skill.guard_facing)!=TYPE_STRING or skill.guard_facing!="front"): return {"ok":false,"code":"PERSONAL_GUARD_FACING","path":path+"/guard_facing"}
	return {"ok":true}

## 以面朝向量点积区分前、侧、后；浮点容差仅消除精确直角的三角函数舍入。
static func sector(defender_position: Array, heading: int, attacker_position: Array) -> String:
	if not _point(defender_position) or not _point(attacker_position) or heading<0 or heading>359: return "invalid"
	var dx: float=float(attacker_position[0])-float(defender_position[0])
	var dy: float=float(attacker_position[1])-float(defender_position[1])
	if dx==0.0 and dy==0.0: return "invalid"
	var angle: float=deg_to_rad(float(heading))
	var dot: float=cos(angle)*dx+sin(angle)*dy
	var tolerance: float=maxf(1.0,absf(dx)+absf(dy))*SIDE_TOLERANCE
	if absf(dot)<=tolerance: return "side"
	return "front" if dot>0.0 else "back"

## 普通防御与显式front守势仅在正面保留各自完整倍率；侧后不减伤，既不自动叠普通防御也不限制旧全向秘技。
static func multiplier(attacker_position: Array, defender_position: Array, heading: int, guarded_bp: int) -> int:
	return guarded_bp if sector(defender_position,heading,attacker_position)=="front" else 10000

## 坐标严格接受两整数，不以隐式取整改变战斗的规则位置。
static func _point(value: Array) -> bool:
	return value.size()==2 and typeof(value[0])==TYPE_INT and typeof(value[1])==TYPE_INT
