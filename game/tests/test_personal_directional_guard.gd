## T39定点回归：普通防御只挡正面，侧后不减伤，秘技仍按原独立防御合同。
extends SceneTree
const Round=preload("res://core/personal_round.gd")
var checks: int=0
var failures: int=0

## 汇总行为断言，错误输出和退出码都必须标记失败。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 极小合法战场和既有字段，仅改变几何方向，不重写伤害或世界夹具。
static func data() -> Dictionary:
	return {"items":{"sword":{"weapon":{"category":"sword","minimum_range":0,"maximum_range":90,"arc_degrees":360,"damage":18,"stamina_cost":10,"recovery_rounds":0,"move_distance":100}}},"personal_skills":{"guard":{"id":"guard","kind":"guard","weapons":["sword"],"stamina_cost":15,"power_bp":10000,"defense_bp":2000,"move_bonus":0,"status":"none","status_rounds":0}},"personal_battlefields":{"yard":{"width":600,"height":400,"obstacles":[],"exits":[]}},"personal_objectives":{"spar":{"max_rounds":10,"allow_withdrawal":false}}}

## 使用保存合同已有单位字段；测试人物没有额外防御状态或世界权限。
static func unit(id: String, side: int, position: Array, heading: int) -> Dictionary:
	return {"actor_id":id,"side":side,"position":position,"heading":heading,"radius":18,"hp":100,"initial_hp":100,"stamina":100,"weapon_id":"sword","martial":40,"learned":{},"heart_id":"","statuses":{},"recovery_until":0,"protected":false}

## 敌方攻击意图在玩家提交前固定；控制者选择防御、等待或独立守势秘技。
static func battle(attacker_position: Array, defender_heading: int=0) -> Dictionary:
	var units: Dictionary={"defender":unit("defender",0,[250,200],defender_heading),"attacker":unit("attacker",1,attacker_position,180)}
	return {"controller_id":"defender","battlefield_id":"yard","objective_id":"spar","round":1,"phase":"planning","units":units,"ai_plans":{"attacker":plan(units.attacker,"attack","defender")},"outcome":"","last_events":[]}

## 计划仍走原入口，不直接指定伤害或最终位置。
static func plan(actor: Dictionary, action: String, target_id: String="", skill_id: String="") -> Dictionary:
	return {"destination":actor.position.duplicate(),"heading":actor.heading,"action":action,"target_id":target_id,"skill_id":skill_id}

## 只通过正式同步轮次读取损伤；失败不冒称零伤害成功。
func damage(state: Dictionary, action: String="defend", heading: int=-1, skill_id: String="") -> int:
	var intent: Dictionary=plan(state.units.defender,action,"",skill_id)
	if heading>=0: intent.heading=heading
	var result: Dictionary=Round.resolve(state,data(),intent)
	check(result.ok,"directional round resolves: "+str(result.get("code","")))
	return 100-result.battle.units.defender.hp if result.ok else -1

## 分别验证四正交、斜向边界、提交转向、移动后几何、秘技与纯性。
func _initialize() -> void:
	var state: Dictionary=battle([310,200])
	var before: Dictionary=state.duplicate(true)
	var normal: int=damage(state,"wait")
	check(normal==21,"original unguarded formula unchanged")
	check(damage(state)==10,"front retains original 5000bp defense")
	check(damage(battle([190,200]))==normal,"back gets no basic guard reduction")
	check(damage(battle([250,140]))==normal and damage(battle([250,260]))==normal,"both exact side vectors get no basic guard")
	check(damage(battle([250,260],90))==10,"south facing recognizes front")
	check(damage(battle([190,200],180))==10,"west facing recognizes front")
	check(damage(battle([250,140],270))==10,"north facing recognizes front")
	check(damage(battle([292,242],45))==10,"diagonal front")
	check(damage(battle([208,242],45))==normal,"diagonal exact perpendicular is side")
	check(damage(battle([209,242],45))==10,"one integer step inside front")
	check(damage(battle([207,242],45))==normal,"one integer step behind side")
	check(damage(state,"defend",180)==normal,"submitted facing replaces stale starting heading")
	check(state==before,"queries and resolution preserve input")
	state=battle([190,200]); state.units.defender.learned["guard"]=100
	check(damage(state,"skill",0,"guard")==4,"independent guard skill retains original rear coverage")
	state=battle([310,200])
	state.units["rear_attacker"]=unit("rear_attacker",1,[190,200],0)
	state.ai_plans["rear_attacker"]=plan(state.units["rear_attacker"],"attack","defender")
	check(damage(state)==31,"simultaneous front and rear attacks apply separate directional guards")
	state=battle([310,200]); state.ai_plans.attacker.destination=[250,260]
	check(damage(state)==normal,"defense uses attacker's actual post-move side")
	var intent: Dictionary=plan(state.units.defender,"defend")
	var first: Dictionary=Round.resolve(state,data(),intent)
	check(first==Round.resolve(state,data(),intent),"same committed plans replay exactly")
	state.units={"attacker":state.units.attacker,"defender":state.units.defender}
	check(first==Round.resolve(state,data(),intent),"unit insertion order does not change front checks")
	print("PERSONAL_DIRECTIONAL_GUARD checks=",checks," failures=",failures)
	quit(1 if failures else 0)
