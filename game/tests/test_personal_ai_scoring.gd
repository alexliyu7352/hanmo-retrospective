## 个人战可见态评分局部门；固定分项、目标保护、确定平局和查询纯性，不称全策略最优。
extends SceneTree
const Strict=preload("res://core/strict_json.gd")
const Round=preload("res://core/personal_round.gd")
const Score=preload("res://core/domains/personal/personal_ai_score.gd")
const Fixture=preload("res://tests/test_personal_directional_guard.gd")
const EscortFixture=preload("res://tests/test_personal_escort_exit.gd")
const AreaFixture=preload("res://tests/test_personal_area_strike.gd")
const HitFixture=preload("res://tests/test_personal_hit_round.gd")
var checks: int=0
var failures: int=0

## 每次失败指出实际分项或数据出口，不用计划变化冒称策略更聪明。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 只显式目标启用评分，其他内容仍是既有极小个人战定义。
static func content() -> Dictionary:
	var data: Dictionary=Fixture.data(); data.personal_objectives.spar.ai_model="scored_v1"
	return data

## 分别检验纯数学、真实候选/几何、可见保护风险、目标与随机边界。
func _initialize() -> void:
	var data: Dictionary=content(); var state: Dictionary=Fixture.battle([310,200])
	var intent: Dictionary=Fixture.plan(state.units.attacker,"attack","defender")
	var math: Dictionary=Score.evaluate(intent,state.units.attacker.position,true,true,84,40,15)
	check(math.score_q==589 and math.terms=={"objective_q":400,"protection_q":160,"damage_q":84,"incoming_q":-40,"cost_q":-15},"fixed quarter-point formula retains exact cost15/4")
	var reversed: Dictionary=math.duplicate(true); reversed.action_key="z"; math.action_key="a"
	check(Score.choose([reversed,math])==Score.choose([math,reversed]) and Score.choose([math,reversed]).action_key=="a","equal scores use stable encoded plan, never insertion order")
	var before: Dictionary=state.duplicate(true)
	var explanation: Dictionary=Round.ai_explanation(state,data,"attacker")
	check(explanation.ok and explanation.candidates_considered>1 and explanation.candidates_considered<=512 and not explanation.truncated,"actual legal bounded candidate set yields explanation")
	check(Round.make_ai(state,data).attacker==explanation.plan and Round.validate_plan(state,data,"attacker",explanation.plan).ok,"saved AI consumes exactly explained legal selected plan")
	check(state==before and explanation==Round.ai_explanation(state,data,"attacker"),"repeated score queries preserve input and exact result")
	var hidden: Dictionary=state.duplicate(true); hidden.player_draft={"action":"skill","target_id":"attacker","destination":[500,300]}; hidden.ai_plans={"fake":{"action":"withdraw"}}
	check(Round.ai_explanation(hidden,data,"attacker")==explanation,"current draft and saved opposing plans cannot change candidate scores")
	state.units={"attacker":state.units.attacker,"defender":state.units.defender}
	check(Round.ai_explanation(state,data,"attacker")==explanation,"unit dictionary order cannot change candidate scores or tie break")
	state=Fixture.battle([310,200]); state.units.defender.hp=10
	var assessed: Dictionary=Round._assess_plan(state,data,"attacker",Fixture.plan(state.units.attacker,"attack","defender"),0)
	check(assessed.terms.damage_q==40 and assessed.terms.objective_q==400,"effective expected damage caps at10 HP, real deterministic key defeat receives one objective bonus")
	data.items.sword.weapon.damage=100
	assessed=Round._assess_plan(state,data,"attacker",Fixture.plan(state.units.attacker,"attack","defender"),0)
	check(assessed.terms.damage_q==40,"overkill cannot inflate effective-damage score")
	data=content(); state=Fixture.battle([310,200]); state.units["protected"]=Fixture.unit("protected",0,[250,260],0); state.units["protected"].protected=true; state.units.attacker.hp=1
	var risk: int=Round._protected_risk_q(state,data,"defender",{})
	assessed=Round._assess_plan(state,data,"defender",Fixture.plan(state.units.defender,"attack","attacker"),risk)
	check(risk>0 and assessed.protected_risk_q.before==assessed.protected_risk_q.after and assessed.terms.protection_q==0,"killing attacker in projection cannot cancel its simultaneous threat to protected ally")
	state=Fixture.battle([310,200]); state.units.defender.position=[350,350]; state.units["protected"]=Fixture.unit("protected",0,[250,200],180); state.units["protected"].protected=true; state.units["protected"].weapon_id=""
	explanation=Round.ai_explanation(state,data,"protected")
	check(explanation.ok and explanation.plan.action=="defend" and explanation.plan.heading==0 and explanation.terms.protection_q==160,"protected AI may face visible threat and earns protection bonus only for actual expected-risk reduction")
	check(explanation.protected_risk_q.after<explanation.protected_risk_q.before,"protection explanation exposes actual before/after calculation")
	data=EscortFixture.content(); data.personal_objectives.spar.ai_model="scored_v1"; state=EscortFixture.battle(); state.units["protected"].position=[120,240]
	explanation=Round.ai_explanation(state,data,"protected")
	check(explanation.ok and explanation.plan.action=="wait" and explanation.terms.objective_q==400 and explanation.projected_position!=state.units["protected"].position,"protected arrival candidate uses real navigation and explicit objective bonus")
	data=content(); data.personal_objectives.spar.allow_withdrawal=true; data.personal_battlefields.yard.exits=[{"side":1,"position":[550,200],"radius":25}]
	state=Fixture.battle([550,200]); state.units.attacker.hp=25
	explanation=Round.ai_explanation(state,data,"attacker")
	check(explanation.ok and explanation.plan.action=="withdraw" and explanation.terms.objective_q==400,"existing low-health safe-exit candidate retains objective priority")
	state.units.attacker.hp=100
	check(Round.ai_explanation(state,data,"attacker").plan.action!="withdraw","healthy NPC does not treat leaving as universal full-score objective")
	data=content(); data.personal_objectives.spar.hit_model="skill_v1"; state=HitFixture.snapshot(Fixture.battle([310,200])); state.rng={"duel":{"seed":"1","state":"123"}}
	before=state.duplicate(true); explanation=Round.ai_explanation(state,data,"attacker")
	check(explanation.ok and state==before,"probability-based scoring never consumes or writes random stream")
	hidden=state.duplicate(true); hidden.rng.duel={"seed":"998","state":"777"}
	check(Round.ai_explanation(hidden,data,"attacker")==explanation,"changing unseen future RNG cannot change score or plan")
	state.units.defender.hp=1
	assessed=Round._assess_plan(state,data,"attacker",Fixture.plan(state.units.attacker,"attack","defender"),0)
	check(assessed.terms.damage_q<4 and assessed.terms.objective_q==0,"probable lethal hit is not falsely called a certainly completed objective")
	data=Fixture.data(); state=Fixture.battle([310,200]); var legacy: Dictionary=Round.make_ai(state,data)
	check(Round.ai_explanation(state,data,"attacker").get("code")=="ScoredAIUnavailable" and Round.make_ai(state,data)==legacy,"missing model keeps old AI and does not expose new scoring as active")
	for model in ["future",1,null,true]:
		data=content(); data.personal_objectives.spar.ai_model=model
		check(Round.ai_explanation(state,data,"attacker").get("code")=="PERSONAL_AI_MODEL","unknown model fails closed "+str(model))
	state=Fixture.battle([310,200]); state.units.attacker.hp=15; data=content()
	var low: Dictionary=Round.ai_explanation(state,data,"attacker")
	check(low.plan.action=="defend" and low.survival_filter.applied and low.survival_filter.original.plan.action=="attack" and low.survival_filter.original.lethal,"15HP versus21 damage preserves original score conflict and selects live defense by explicit survival priority")
	check(low.survival_filter.reason=="VisibleLethalRiskAvoided" and low.survival_filter.surviving_candidates>0 and low.maximum_incoming==10,"debug discloses exact override reason and maximum visible threat")
	state.units.attacker.hp=1
	low=Round.ai_explanation(state,data,"attacker")
	check(not low.survival_filter.applied and low.survival_filter.reason=="NoSurvivableDefense" and low.plan.action=="attack","no survivable defense keeps lawful full-score candidate comparison")
	state.units.attacker.hp=15; state.units.defender.stamina=0
	low=Round.ai_explanation(state,data,"attacker")
	check(low.plan.action=="attack" and not low.survival_filter.applied and low.survival_filter.reason=="BestAlreadySurvives","low health does not force defense when original best is already safe")
	state=Fixture.battle([310,200]); state.units.attacker.hp=9
	data.personal_skills["a.guard"]=data.personal_skills.guard.duplicate(true); data.personal_skills["a.guard"].id="a.guard"
	data.personal_skills["z.guard"]=data.personal_skills.guard.duplicate(true); data.personal_skills["z.guard"].id="z.guard"
	state.units.attacker.learned={"z.guard":100,"a.guard":100}
	low=Round.ai_explanation(state,data,"attacker")
	check(low.plan.action=="skill" and low.plan.skill_id=="a.guard" and low.survival_filter.applied,"different equal-score survivable guards use same stable action encoding")
	state.units.attacker.learned={"a.guard":100,"z.guard":100}
	check(Round.ai_explanation(state,data,"attacker")==low,"survival filtering does not reintroduce learned dictionary order dependence")
	data=AreaFixture.content(); data.personal_objectives.spar.ai_model="scored_v1"; state=AreaFixture.battle()
	assessed=Round._assess_plan(state,data,"defender",AreaFixture.plan(state),0)
	check(assessed.terms.damage_q==136 and assessed.terms.cost_q==-25,"area scoring reuses exact two-target capped damage and only one skill cost")
	data=content(); data.personal_objectives.spar.hit_model="skill_v1"; state=HitFixture.snapshot(Fixture.battle([310,200]))
	data.personal_skills["inaccurate"]={"id":"inaccurate","kind":"strike","weapons":["sword"],"stamina_cost":15,"power_bp":25000,"defense_bp":10000,"move_bonus":0,"status":"none","status_rounds":0,"hit_bonus_bp":-6000}
	state.units.attacker.learned={"inaccurate":100}
	var expected_sources: Array=Round._incoming_sources(state,data,"defender",state.units.defender.position,true)
	var maximum_sources: Array=Round._incoming_sources(state,data,"defender",state.units.defender.position)
	check(expected_sources[0].skill.is_empty() and maximum_sources[0].skill.id=="inaccurate","expected incoming threat and maximum-lethal safety deliberately use different valid candidate comparisons")
	check(Round._scored_incoming_q(state,data,"defender",Fixture.plan(state.units.defender,"wait"))==61,"low-hit high-power skill cannot hide a stronger expected ordinary threat")

	data=content(); state=Fixture.battle([310,200]); state.ai_plans=Round.make_ai(state,data)
	before=state.duplicate(true)
	var resolved: Dictionary=Round.resolve(state,data,Fixture.plan(state.units.defender,"wait"))
	var restored: Dictionary=Strict.parse_text(JSON.stringify(state)).value
	check(resolved.ok and resolved.battle.units.defender.hp<100 and state==before,"committed scored plan is actually consumed by normal synchronous round without query mutation")
	check(Round.resolve(restored,data,Fixture.plan(restored.units.defender,"wait"))==resolved,"saved scored intention replays same actual round, no hidden planner state")

	print("PERSONAL_AI_SCORING checks=",checks," failures=",failures)
	quit(1 if failures else 0)
