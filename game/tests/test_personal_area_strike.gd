## 长兵区域的有限目标、同步伤害和唯一抽样局部门；原技能ID和旧单体默认不参与规则分支。
extends SceneTree
const Round=preload("res://core/personal_round.gd")
const Area=preload("res://core/domains/personal/personal_area.gd")
const Hit=preload("res://core/domains/personal/personal_hit.gd")
const Strict=preload("res://core/strict_json.gd")
const Fixture=preload("res://tests/test_personal_directional_guard.gd")
const CounterFixture=preload("res://tests/test_personal_counter.gd")
const HitFixture=preload("res://tests/test_personal_hit_leaf.gd")
var checks: int=0
var failures: int=0

## 检查所选目标与真实状态，不以技能命名或输出事件个数代替伤害证据。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 区域宽度/扇角为明确测试作者值，复用现长兵内外射程，不换算旧格数。
static func content() -> Dictionary:
	var data: Dictionary=Fixture.data()
	data.items.sword.weapon={"category":"spear","minimum_range":40,"maximum_range":200,"arc_degrees":120,"damage":18,"stamina_cost":10,"recovery_rounds":0,"move_distance":60}
	data.personal_skills={"pierce":{"id":"pierce","kind":"strike","weapons":["spear"],"stamina_cost":25,"power_bp":8500,"defense_bp":10000,"move_bonus":0,"status":"slow","status_rounds":1,"area":{"shape":"line","half_width":20,"max_targets":2}},"sweep":{"id":"sweep","kind":"strike","weapons":["spear"],"stamina_cost":30,"power_bp":7500,"defense_bp":10000,"move_bonus":0,"status":"none","status_rounds":0,"area":{"shape":"sector","arc_degrees":120,"max_targets":3}}}
	return data

## 主目标故意比两个附加敌远，检验主必含与附加距离排序；人物间隔均符合圆身几何。
static func battle() -> Dictionary:
	var state: Dictionary=Fixture.battle([380,200]); state.units.erase("attacker"); state.ai_plans={}
	state.units.defender.position=[200,200]; state.units.defender.learned={"pierce":100,"sweep":100}
	for row in [["z.primary",[380,200]],["a.far",[340,200]],["b.near",[300,200]]]:
		state.units[row[0]]=Fixture.unit(row[0],1,row[1],180)
		state.ai_plans[row[0]]=Fixture.plan(state.units[row[0]],"wait")
	return state

## 主动技能仍用原target_id命令，没有第二套范围施法命令或专用世界账。
static func plan(state: Dictionary, skill_id: String="pierce") -> Dictionary:
	return Fixture.plan(state.units.defender,"skill","z.primary",skill_id)

## 只检索本轮实际造成伤害或miss的出手，不把移动/状态/恢复事件计为攻击。
static func attacks(state: Dictionary, action: String="skill") -> Array:
	var result: Array=[]
	for event in state.last_events:
		if event.action==action and event.has("damage"): result.append(event)
	return result

## 覆盖作者合同、固定区域、一次费用、每目标随机与各守势者反击边界。
func _initialize() -> void:
	var data: Dictionary=content(); var state: Dictionary=battle(); var before: Dictionary=state.duplicate(true)
	var selected: Dictionary=Area.select(state,data,"defender","z.primary",data.personal_skills.pierce,data.items.sword.weapon)
	check(selected.ok and selected.target_ids==["b.near","z.primary"],"line always includes primary then chooses nearest extra, emits ID order")
	var preview: Dictionary=Round.preview_attack(state,data,"defender",[200,200],0,"z.primary","pierce")
	check(preview.ok and preview.damage==17 and preview.targets==[{"target_id":"b.near","damage":17},{"target_id":"z.primary","damage":17}],"preview exposes exact bounded target set and keeps main damage")
	var result: Dictionary=Round.resolve(state,data,plan(state))
	check(result.ok and result.battle.units["z.primary"].hp==83 and result.battle.units["b.near"].hp==83 and result.battle.units["a.far"].hp==100,"only actually selected units take simultaneous area damage")
	check(result.ok and result.battle.units.defender.stamina==75,"two legal targets pay only one25 cost")
	check(result.battle.units["z.primary"].statuses.get("slow")==2 and result.battle.units["b.near"].statuses.get("slow")==2 and result.battle.units["a.far"].statuses.is_empty(),"each hit gets same future-effective status, excluded actor does not")
	check(state==before and result==Round.resolve(state,data,plan(state)),"area preview and resolution preserve inputs and exact replay")
	state=battle(); state.units["b.near"].position=[300,240]; state.units["a.far"].position=[340,160]
	selected=Area.select(state,data,"defender","z.primary",data.personal_skills.pierce,data.items.sword.weapon)
	check(selected.ok and selected.target_ids==["z.primary"],"line excludes off-axis targets despite ordinary weapon legality")
	selected=Area.select(state,data,"defender","z.primary",data.personal_skills.sweep,data.items.sword.weapon)
	check(selected.ok and selected.target_ids==["a.far","b.near","z.primary"],"sector includes only its configured bounded front set")
	state.units["b.near"].side=0
	selected=Area.select(state,data,"defender","z.primary",data.personal_skills.sweep,data.items.sword.weapon)
	check(selected.ok and not "b.near" in selected.target_ids,"same-side actor never becomes collateral damage")
	state=battle(); state.units["b.near"].position=[200,100]
	check(not "b.near" in Area.select(state,data,"defender","z.primary",data.personal_skills.sweep,data.items.sword.weapon).target_ids,"configured sector and original weapon arc exclude side actor")
	data.personal_skills.sweep.area.arc_degrees=180
	check(not "b.near" in Area.select(state,data,"defender","z.primary",data.personal_skills.sweep,data.items.sword.weapon).target_ids,"wide authored sector cannot expand original120 degree weapon arc")
	state.units["b.near"].position=[230,200]
	check(not "b.near" in Area.select(state,data,"defender","z.primary",data.personal_skills.sweep,data.items.sword.weapon).target_ids,"area cannot erase weapon inner dead zone")
	state=battle(); data.personal_battlefields.yard.obstacles=[{"x":355,"y":190,"width":10,"height":20,"blocks_movement":false,"blocks_sight":true}]
	check(Area.select(state,data,"defender","z.primary",data.personal_skills.pierce,data.items.sword.weapon).get("code")=="LineBlocked","primary behind wall prevents entire skill launch")
	data=content(); state=battle(); state.units["a.far"].position=[340,260]
	data.personal_battlefields.yard.obstacles=[{"x":280,"y":231,"width":10,"height":12,"blocks_movement":false,"blocks_sight":true}]
	check(not "a.far" in Area.select(state,data,"defender","z.primary",data.personal_skills.sweep,data.items.sword.weapon).target_ids,"blocked extra target excluded independently while primary remains legal")
	data=content(); state=battle(); state.units["b.near"].hp=0
	check(not "b.near" in Area.select(state,data,"defender","z.primary",data.personal_skills.pierce,data.items.sword.weapon).target_ids,"downed actor is not an extra target")
	state=battle(); state.units["b.near"].withdrawn_round=1
	check(not "b.near" in Area.select(state,data,"defender","z.primary",data.personal_skills.pierce,data.items.sword.weapon).target_ids,"already withdrawn actor is not an extra target")
	state=battle(); state.ai_plans["z.primary"].destination=[440,200]
	result=Round.resolve(state,data,plan(state))
	check(result.ok and result.battle.units.defender.stamina==100 and result.battle.units["b.near"].hp==100,"lost primary cancels entire strike without borrowing another legal target or fee")
	state=battle(); state.ai_plans["b.near"].destination=[300,260]
	result=Round.resolve(state,data,plan(state))
	check(result.ok and result.battle.units["b.near"].hp==100 and result.battle.units["a.far"].hp==83,"selection uses actual simultaneous positions, not preview's earlier extra")
	state=battle(); data.personal_skills.pierce.erase("area")
	preview=Round.preview_attack(state,data,"defender",[200,200],0,"z.primary","pierce"); result=Round.resolve(state,data,plan(state))
	check(preview.ok and not preview.has("targets") and attacks(result.battle).size()==1 and result.battle.units["b.near"].hp==100,"absent area preserves old single-target result shape and damage")
	data=content(); state=battle(); data.personal_objectives.spar.hit_model="skill_v1"
	for unit in state.units.values(): unit.skill_levels={"martial":2,"archery":0}
	var stream: Dictionary=HitFixture.stream(11); var draw_one: Dictionary=Hit.draw(stream,7500); var draw_two: Dictionary=Hit.draw(draw_one.duel_stream,7500)
	result=Round.resolve(state,data,plan(state),stream)
	var events: Array=attacks(result.battle)
	check(result.ok and events.size()==2 and events[0].target_id=="b.near" and events[0].roll==draw_one.roll and events[1].target_id=="z.primary" and events[1].roll==draw_two.roll,"one draw per legal target in target ID order, not selection distance order")
	check(result.duel_stream==draw_two.duel_stream and result.battle.units.defender.stamina==75,"two targets advance exactly twice but charge once")
	var saved: Dictionary=Strict.parse_text(JSON.stringify({"battle":state,"stream":stream})).value
	check(result==Round.resolve(saved.battle,data,plan(saved.battle),saved.stream),"strict JSON replay retains area RNG and result")
	state.units={"z.primary":state.units["z.primary"],"a.far":state.units["a.far"],"defender":state.units.defender,"b.near":state.units["b.near"]}
	check(result==Round.resolve(state,data,plan(state),stream),"unit insertion order cannot reassign target random rolls")
	state.ai_plans["z.primary"].destination=[440,200]
	result=Round.resolve(state,data,plan(state),stream)
	check(result.ok and result.duel_stream==stream and result.battle.units.defender.stamina==100,"invalidated primary draws zero and spends zero under random model")
	data=content(); var counter: Dictionary=CounterFixture.content().personal_skills.guard; counter.weapons=["spear"]; data.personal_skills["guard"]=counter
	state=battle()
	for id in ["b.near","z.primary"]:
		state.units[id].learned={"guard":100}; state.ai_plans[id]=Fixture.plan(state.units[id],"skill","","guard")
	result=Round.resolve(state,data,plan(state))
	check(result.ok and attacks(result.battle,"counter").size()==2 and result.battle.units.defender.hp==84,"each declared target can counter once without recursion")
	state.units.defender.hp=16; state.units["b.near"].hp=10; state.units["z.primary"].hp=10
	result=Round.resolve(state,data,plan(state))
	check(result.ok and result.battle.units.defender.hp==0 and result.battle.units["b.near"].hp==0 and result.battle.units["z.primary"].hp==0,"all original area hits and two reactions survive simultaneous fatal damage")
	data=content(); state=battle(); state.units.defender.side=1; state.controller_id="z.primary"
	for id in ["a.far","b.near","z.primary"]: state.units[id].side=0
	var ai: Dictionary=Round.make_ai(state,data)
	check(ai.defender.action=="skill" and ai.defender.skill_id in ["pierce","sweep"],"AI selects lower per-target skill when actual area expected total is higher")
	var hidden: Dictionary=state.duplicate(true); hidden.player_draft={"destination":[50,390]}; hidden.ai_plans={}
	check(Round.make_ai(hidden,data)==ai,"area AI never reads unsubmitted player draft")
	data=content(); data.personal_objectives.spar.hit_model="skill_v1"; state=battle()
	for unit in state.units.values(): unit.skill_levels={"martial":2,"archery":0}
	var mixed: Dictionary={}
	for seed_value in range(1,1000):
		var candidate: Dictionary=HitFixture.stream(seed_value)
		var first: Dictionary=Hit.draw(candidate,7500); var second: Dictionary=Hit.draw(first.duel_stream,7500)
		if first.hit and not second.hit: mixed=candidate; break
	result=Round.resolve(state,data,plan(state),mixed)
	check(result.ok and result.battle.units["b.near"].hp==83 and result.battle.units["z.primary"].hp==100 and result.battle.units.defender.stamina==75,"primary random miss does not cancel independently landed extra after legal single-cost launch")
	check(result.ok and result.battle.units["b.near"].statuses.has("slow") and not result.battle.units["z.primary"].statuses.has("slow"),"only actual hits apply per-target state effects")
	data=content(); data.personal_objectives.spar.stamina_model="round_v1"; state=battle()
	for unit in state.units.values(): unit.stamina=50
	result=Round.resolve(state,data,plan(state))
	check(result.ok and result.battle.units.defender.stamina==25 and result.battle.units["b.near"].stamina==68 and result.battle.units["z.primary"].stamina==68,"multi-target strike still earns no ordinary recovery; each actually hurt victim gets3")
	state=battle(); state.units["b.near"].position=[300,220]; state.units["a.far"].position=[300,180]
	selected=Area.select(state,data,"defender","z.primary",data.personal_skills.pierce,data.items.sword.weapon)
	check(selected.ok and selected.target_ids==["a.far","z.primary"],"equal-distance extras break tie by stable identity including line-width boundary")

	var malformed: Dictionary=data.personal_skills.pierce.duplicate(true); malformed.weapons=["sword"]
	check(not Area.validate_skill(malformed).ok,"initial area capability is spear-only")
	for value in [1,3,2.0,true]:
		malformed=data.personal_skills.pierce.duplicate(true); malformed.area.max_targets=value
		check(not Area.validate_skill(malformed).ok,"line exact two-target budget rejects "+str(value))
	malformed=data.personal_skills.sweep.duplicate(true); malformed.area.max_targets=4
	check(not Area.validate_skill(malformed).ok,"sector cannot exceed three targets")
	malformed=data.personal_skills.pierce.duplicate(true); malformed.area.arc_degrees=120
	check(not Area.validate_skill(malformed).ok,"line cannot carry mixed sector fields")
	print("PERSONAL_AREA_STRIKE checks=",checks," failures=",failures)
	quit(1 if failures else 0)
