## 礼议规则的独立作者/状态门；不创建世界、GUI或发放课程收益，实例授权仍属于正式owner。
extends SceneTree
const Challenge=preload("res://core/domains/cultivation/etiquette_challenge.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0
var definition: Dictionary={}
var texts: Dictionary={}
const SNAPSHOT={"skill_level":2,"auxiliary_attribute":60}
const BINDING={"actor_id":"base::actor.player","source_id":"action:7"}

## 实际引擎加载后仅执行本叶短门。
func _init() -> void:
	call_deferred("run_checks")

## 不吞掉失败标签，退出码与规则断言一致。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 显式创建一个新测试实例，不能将它当作owner已经核过来源及唯一结算。
func create(mode: String="manual") -> Dictionary:
	var result: Dictionary=Challenge.create(definition,texts,BINDING,SNAPSHOT,"7654321",mode)
	check(result.ok,"合规来源与人物快照建立实例")
	return result.get("state",{})

## 固定三轮实际选择，节点ID随回复提交以拒绝旧节点重放。
func play(options: Array) -> Dictionary:
	var state: Dictionary=create()
	for index in range(3):
		var old: Dictionary=state.duplicate(true)
		var next: Dictionary=Challenge.choose(definition,texts,state,definition.nodes[index].id,options[index])
		if not check(next.ok and old==state,"逐轮选择保持旧值 "+str(index)): return {}
		state=next.state
	return state

## 自动与手动各走实际状态流程，检查整数结果、退出兜底和终态不可重开。
func check_scoring() -> void:
	check(Challenge.automatic_score(SNAPSHOT,1)==59,"自动分数使用派生等级/属性/难度")
	check(Challenge.automatic_score({"skill_level":0,"auxiliary_attribute":1},3)==30,"低属性难题正常基线有下限")
	var automatic: Dictionary=create("automatic")
	check(automatic.phase=="completed" and automatic.result.score==59 and automatic.result.request_delta==-1 and automatic.result.performance_bp==10000,"自动立即结算，59分按floor给-1")
	var best: Dictionary=play(["register","register","register"])
	if best.is_empty(): return
	check(best.result.raw_score==100 and best.result.score==100 and best.result.performance_bp==12000 and best.result.request_delta==5,"25基础加三轮25，倍率与请求调整有界")
	var scaled: Dictionary=Challenge.scaled_experience(definition,texts,best,13)
	check(scaled.ok and scaled.xp==15 and scaled.xp*10000<=13*12000,"先给定正常经验再做一次有界整数奖励")
	check(Challenge.scaled_experience(definition,texts,automatic,13).xp==13,"自动不被经验倍率惩罚")
	var mismatch: Dictionary=play(["seats","register","register"])
	check(mismatch.result.raw_score==25 and mismatch.result.score==49 and mismatch.result.performance_bp==10000,"显式历史不符得0，手动下限自动减10且基础成长不打折")
	var state: Dictionary=create()
	var view: Dictionary=Challenge.view(definition,texts,state)
	check(view.ok and view.round==1 and view.options.size()==3 and view.topic==texts["count.topic"] and not view.has("nodes"),"当前轮只有公开题目立场论据")
	state=Challenge.choose(definition,texts,state,"count","register").state
	check(Challenge.choose(definition,texts,state,"count","register").get("code")=="StaleEtiquetteNode","重发旧轮回复不能误选下一轮")
	var exited: Dictionary=Challenge.exit_challenge(definition,texts,state).state
	check(exited.result.score==automatic.result.score and exited.result.performance_bp==10000 and exited.result.completion=="exited" and exited.result.choices.size()==1,"中途退出保留选择记录但严格按自动基线结算")
	check(Challenge.exit_challenge(definition,texts,best).state==best and Challenge.choose(definition,texts,best,"close","agenda").get("code")=="EtiquetteAlreadyCompleted","终态不改成绩、不再接受选择")

## 作者输入不允许隐藏字段、脚本、超限或前向条件；不强迫低分题面存在100分路径。
func check_authoring() -> void:
	check(Challenge.validate_definition(definition,texts).maximum_raw_score==100,"作者可查最高可达分")
	for kind in ["script","difficulty","nodes","options","node_id","option_id","points","float","forward","unknown_choice","duplicate_condition","missing_text","newline"]:
		var bad: Dictionary=definition.duplicate(true)
		match kind:
			"script": bad.script="run_anything()"
			"difficulty": bad.difficulty=4
			"nodes": bad.nodes.pop_back()
			"options": bad.nodes[0].options.pop_back()
			"node_id": bad.nodes[1].id=bad.nodes[0].id
			"option_id": bad.nodes[0].options[1].id=bad.nodes[0].options[0].id
			"points": bad.nodes[0].options[0].points=26
			"float": bad.nodes[0].options[0].points=25.0
			"forward": bad.nodes[0].options[0].requires_choices=[{"node_id":"close","option_id":"register"}]
			"unknown_choice": bad.nodes[1].options[0].requires_choices[0].option_id="absent"
			"duplicate_condition": bad.nodes[2].options[0].requires_choices.append(bad.nodes[2].options[0].requires_choices[0].duplicate())
			"missing_text": bad.nodes[0].topic_key="absent"
			"newline": bad.nodes[0].id="count\n"
		check(not Challenge.validate_definition(bad,texts).ok,"作者非法输入拒绝 "+kind)
	var low: Dictionary=definition.duplicate(true)
	for node in low.nodes:
		for option in node.options: option.points=0
	check(Challenge.validate_definition(low,texts).maximum_raw_score==25,"最大可达仅诊断，不凭空要求满分")

## 保存记录必须从实际题面和选择重算，变更公开文本也视为来源变化。
func check_state_identity() -> void:
	var state: Dictionary=play(["register","register","register"])
	var parsed: Dictionary=Strict.parse_text(JSON.stringify(state))
	check(parsed.ok and parsed.value==state and Challenge.validate_state(definition,texts,parsed.value).ok,"严格JSON往返保留终态和seed，不重抽")
	var changed: Dictionary=texts.duplicate(true); changed["count.topic"]+=" 新题面"
	check(Challenge.validate_state(definition,changed,state).get("code")=="EtiquetteSourceChanged","同键显示文字变化拒绝旧题面结果")
	for kind in ["grade","choice","float_result","bonus","script","seed"]:
		var bad: Dictionary=state.duplicate(true)
		match kind:
			"grade": bad.result.score=99
			"choice": bad.choices[0].points=0
			"float_result": bad.result.score=100.0
			"bonus": bad.result.performance_bp=20000
			"script": bad.callback="eval"
			"seed": bad.seed="07654321"
		check(not Challenge.validate_state(definition,texts,bad).ok,"拒绝伪造/畸形保存值 "+kind)
	var viewed: Dictionary=Challenge.view(definition,texts,state); viewed.result.choices.clear()
	check(state.choices.size()==3 and state.result.choices.size()==3,"只读结果没有可改源记录的引用")

## fixture作者文本在独立测试工程读取，缺件直接失败，不用空题面伪通过。
func run_checks() -> void:
	var source: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/etiquette_challenge/definition.json"))
	var strings: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/etiquette_challenge/texts.json"))
	if not check(source.ok and strings.ok,"真实小作者题面可读取"): quit(1); return
	definition=source.value; texts=strings.value
	check_authoring(); check_scoring(); check_state_identity()
	print("ETIQUETTE_CHALLENGE_CHECKS=%d FAILURES=%d"%[checks,failures])
	quit(0 if failures==0 else 1)
