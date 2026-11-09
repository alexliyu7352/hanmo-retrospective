## 个人战目标的超时裁决；保留现有结果种类，不承担世界奖惩、任务结算或持久字段迁移。
extends RefCounted
const Withdrawal=preload("res://core/domains/personal/personal_withdrawal.gd")
const LEAD_DENOMINATOR: int=10

## 护送出口模型只能显式配置到escort目标；缺省继续原目标行为，未知值不推断。
static func validate_objective(objective: Dictionary, path: String="") -> Dictionary:
	if objective.has("escort_model") and (objective.get("kind")!="escort" or typeof(objective.escort_model)!=TYPE_STRING or objective.escort_model!="exit_v1"): return {"ok":false,"code":"PERSONAL_ESCORT_MODEL","path":path+"/escort_model"}
	return {"ok":true}

## 只有此固定模型将抵达出口作为战术胜利；不会把所有旧护送目标统一改写。
static func exit_enabled(objective: Dictionary) -> bool:
	return objective.get("escort_model","")=="exit_v1"

## 显式出口目标必须有真实友方保护者且不能是控制者本人；历史保留同一战场身份，不重新绑定世界任务。
static func validate_battle(battle: Dictionary, objective: Dictionary) -> Dictionary:
	if not exit_enabled(objective): return {"ok":true}
	var found: bool=false
	var side: int=battle.units[battle.controller_id].side
	for id in battle.units:
		var unit: Dictionary=battle.units[id]
		if not unit.protected: continue
		if unit.side!=side or id==battle.controller_id: return {"ok":false,"code":"InvalidEscortProtectedSide"}
		found=true
	return {"ok":true} if found else {"ok":false,"code":"ProtectedPersonMissing"}

## 目标抵达只读固定战场位置和存活；不是安全撤离，不要求控制者也到出口或制造退出/旅程事实。
static func escort_arrived(battle: Dictionary, arena: Dictionary) -> bool:
	if arena.is_empty(): return false
	var found: bool=false; var side: int=battle.units[battle.controller_id].side
	for unit in battle.units.values():
		if unit.side!=side or not unit.protected: continue
		found=true
		if unit.hp<=0 or not Withdrawal.point_at_exit(arena,side,unit.position): return false
	return found

## 出口目标先处理关键失败，再看实际到达；歼敌不代替抵达，未到且未超时仍继续。
static func escort_outcome(battle: Dictionary, arena: Dictionary) -> String:
	var side: int=battle.units[battle.controller_id].side
	for unit in battle.units.values():
		if unit.side==side and unit.protected and unit.hp<=0: return "protected_lost"
	if battle.units[battle.controller_id].hp<=0: return "defeat"
	return "victory" if escort_arrived(battle,arena) else ""

## 轮次与历史校验共用既有即时终态：保护失败、控制者倒地优先，出口目标不以歼敌替代抵达。
static func current_outcome(battle: Dictionary, objective: Dictionary, arena: Dictionary={}) -> String:
	var side: int=battle.units[battle.controller_id].side
	for unit in battle.units.values():
		if unit.side==side and unit.protected and unit.hp<=0: return "protected_lost"
	if battle.units[battle.controller_id].hp<=0: return "defeat"
	if exit_enabled(objective): return escort_outcome(battle,arena)
	for unit in battle.units.values():
		if unit.side!=side and unit.hp>0 and not Withdrawal.departed(unit): return ""
	return "victory"

## 普通及显式出口结果均须符合冻结战場；保留旧错误码和可选字段，不以人物后来健康/地点重算。
static func validate_result(battle: Dictionary, objective: Dictionary, arena: Dictionary) -> Dictionary:
	if battle.phase=="planning": return {"ok":true}
	if not exit_enabled(objective):
		if battle.outcome=="withdrawn":
			return {"ok":true} if _valid_withdrawal_result(battle,objective,arena) else {"ok":false,"code":"PersonalOutcomeMismatch"}
		var expected: String=current_outcome(battle,objective,arena)
		if Withdrawal.departed(battle.units[battle.controller_id]): return {"ok":false,"code":"PersonalOutcomeMismatch"}
		if expected.is_empty() and battle.round>=objective.max_rounds: expected=at_limit(battle,objective,arena)
		return {"ok":true} if not expected.is_empty() and battle.outcome==expected else {"ok":false,"code":"PersonalOutcomeMismatch"}
	var expected: String=escort_outcome(battle,arena)
	if expected.is_empty() and battle.round>=objective.max_rounds: expected="draw"
	if expected.is_empty() or battle.outcome!=expected: return {"ok":false,"code":"EscortOutcomeMismatch"}
	var event: Dictionary=result_event(battle,objective)
	if not event.is_empty() and battle.last_events.count(event)!=1: return {"ok":false,"code":"MissingEscortOutcomeReason"}
	return {"ok":true}

## 退出在本轮NPC脱离之后、超时之前判定；旧无标记历史只核仍可证明的出口与保护组，不倒推未保存意图。
static func _valid_withdrawal_result(battle: Dictionary, objective: Dictionary, arena: Dictionary) -> bool:
	if not objective.allow_withdrawal: return false
	var before_withdrawal: Dictionary=battle.duplicate(true)
	for unit in before_withdrawal.units.values():
		if unit.get("withdrawn_round",-1)==battle.round: unit.erase("withdrawn_round")
	if not current_outcome(before_withdrawal,objective,arena).is_empty(): return false
	var controller: Dictionary=battle.units[battle.controller_id]
	var group: Array=[]
	for id in battle.units:
		var unit: Dictionary=battle.units[id]
		if id!=battle.controller_id and not (unit.side==controller.side and unit.protected): continue
		group.append(id)
		if unit.hp<=0 or not Withdrawal.point_at_exit(arena,unit.side,unit.position): return false
		if unit.has("withdrawn_round") and unit.withdrawn_round!=battle.round: return false
		if controller.has("withdrawn_round") and unit.get("withdrawn_round",-1)!=battle.round: return false
	for event in battle.last_events:
		if event is Dictionary and event.get("actor_id") in group and event.get("action")=="withdraw" and event.get("code") in ["ExitNotReached","WithdrawalMeleeHit","ProtectedExitNotReached"]: return false
	return true

## 持久回顾使用原事件列表明确区分抵达与超时停战，不增加死亡/被俘结果或额外任务报酬。
static func result_event(battle: Dictionary, objective: Dictionary) -> Dictionary:
	if not exit_enabled(objective): return {}
	if battle.outcome=="victory": return {"action":"objective","code":"EscortExitReached"}
	if battle.outcome=="draw" and battle.round>=objective.max_rounds: return {"action":"objective","code":"EscortExitTimeout"}
	return {}

## 单挑切磋采用既有10个百分点候选；其他目标和未声明的多人切磋继续原平局，关键失败由轮次先判。
static func at_limit(battle: Dictionary, objective: Dictionary, arena: Dictionary={}) -> String:
	if exit_enabled(objective):
		var result: String=escort_outcome(battle,arena)
		return result if not result.is_empty() else "draw"
	if objective.get("kind")!="sparring" or battle.units.size()!=2: return "draw"
	var controller: Dictionary=battle.units[battle.controller_id]
	var opponent: Dictionary={}
	for id in battle.units:
		if id!=battle.controller_id: opponent=battle.units[id]
	if opponent.is_empty() or opponent.side==controller.side: return "draw"
	# 存储合同的初始体力范围是1—100；避免畸形纯函数输入引起零除或整数乘法溢出。
	for unit in [controller,opponent]:
		if typeof(unit.get("initial_hp"))!=TYPE_INT or unit.initial_hp<1 or unit.initial_hp>100 or typeof(unit.get("hp"))!=TYPE_INT or unit.hp<0 or unit.hp>unit.initial_hp: return "draw"
	var difference: int=controller.hp*opponent.initial_hp-opponent.hp*controller.initial_hp
	var denominator: int=controller.initial_hp*opponent.initial_hp
	if absi(difference)*LEAD_DENOMINATOR<denominator: return "draw"
	return "victory" if difference>0 else "defeat"
