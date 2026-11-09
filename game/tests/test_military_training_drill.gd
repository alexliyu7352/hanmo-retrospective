## 军阵与真实训练唯一结算口的局部门；复用原任职/供给/runtime，不重跑191或冒称共享Kernel入口。
extends "res://tests/test_military_training.gd"
const TrainingDrill=preload("res://core/domains/military/training_drill.gd")
var drill_definition: Dictionary

## 新局作者边界显式提高本教官步兵XP以观测整数倍率差额；没有课程开始后的伪成长或训练凭证。
func drill_fixture(offered: bool=true) -> Dictionary:
	var f: Dictionary=fresh()
	f.data.actors[f.actor].skills.infantry=650
	f.state.actors[f.actor].skills.infantry=f.data.actors[f.actor].skills.infantry
	if offered:
		f.data.drill_challenges={drill_definition.id:drill_definition.duplicate(true)}
		f.data.texts.merge(Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/drill_challenge/texts.json")).value)
		f.data.garrisons[f.camp].training_drill_id=drill_definition.id
	commission(f)
	return f

## 原runtime创建真实训练行动；核开课唯一minigame抽种，尚未上课不得付款/训练或冻结倍率。
func start_drill(f: Dictionary, mode: String="manual") -> Dictionary:
	var params: Dictionary={"office_id":f.office,"unit_id":f.unit,"challenge_mode":mode}
	var before: Dictionary=f.state.duplicate(true)
	var quoted: Dictionary=Training.preview(f.state,f.data,f.actor,params)
	check(quoted.ok and before==f.state,"真实题面报价只读")
	if not quoted.ok: printerr(quoted); return {}
	var started: Dictionary=Runtime.begin(f.state,f.data,f.actor,Training.KIND,params,quoted,registry())
	check(started.ok,"真实runtime开始带军阵课程")
	if not started.ok: printerr(started); return {}
	var action: Dictionary=f.state.actions[started.details.action_id]
	check(Training.validate_state(f.state,f.data).ok,"带实例开工状态合法")
	var expected: Dictionary=before.rng.duplicate(true)
	if quoted.terms.has("drill"):
		var random=RandomNumberGenerator.new(); random.seed=expected.minigame.seed.to_int(); random.state=expected.minigame.state.to_int(); random.randi()
		expected.minigame.state=str(random.state)
		check(f.state.facts[action.id].drill.phase=="pending" and f.state.facts[action.id].drill.performance_bp==null,"开课实例未消费成绩")
	check(f.state.rng==expected,"开课仅一次minigame抽种或无题面零抽种")
	check(f.state.military_units[f.unit].training==0 and f.state.wallets[action.params.training.recipient_wallet_id].balance==0,"开课只预留不提前给奖")
	return action

## 四步答案都走本人的精确busy转换，不直接写分数/题面或课程倍率。
func answer(f: Dictionary, action: Dictionary, first: int=0, last: int=4) -> void:
	for index in range(first,last):
		var choice: Dictionary=drill_definition.solution[index]
		var params: Dictionary={"action_id":action.id,"expected_step":index+1,"group_id":choice.group_id,"target":choice.target}
		check(Training.transition_drill(f.state,f.data,f.actor,TrainingDrill.DRAFT_KIND,params).ok,"真实训练入口草拟军阵步")
		params={"action_id":action.id,"expected_step":index+1,"expected_pending":f.state.facts[action.id].drill.challenge.pending.duplicate(true)}
		check(Training.transition_drill(f.state,f.data,f.actor,TrainingDrill.CONFIRM_KIND,params).ok,"真实训练入口确认军阵步")

## 手动成绩在首课前完成，真实供给后才消费；保存续跑与未加载对照整态/RNG/ID逐字相同。
func manual_course() -> void:
	var f: Dictionary=drill_fixture(); var action: Dictionary=start_drill(f)
	if action.is_empty(): return
	var money: int=total_money(f.state); answer(f,action,0,1)
	var pending: Dictionary=Strict.parse_text(JSON.stringify(f.state))
	check(pending.ok and Training.validate_state(pending.value,f.data).ok and pending.value==f.state,"首课前含已确认步的状态可严格存读")
	f.state=pending.value; action=f.state.actions[action.id]
	answer(f,action,1,4)
	check(f.state.facts[action.id].drill.challenge.result.performance_bp==12000 and f.state.facts[action.id].drill.phase=="pending","四步满分只得未消费候选12000")
	var before: Dictionary=f.state.duplicate(true)
	check(Training.step(f.state,f.data,action,f.state.tick+1).get("code")=="MilitaryTrainingSupplyRequired" and f.state==before,"没有真实供给不冻结成绩且整态不变")
	var loaded: Dictionary={}; var expected: Array=[1,3,4,7]
	for index in range(4):
		var advanced: Dictionary=advance(f,action)
		if not advanced.ok: return
		var fact: Dictionary=f.state.facts[action.id]
		check(fact.drill.phase=="frozen" and fact.drill.performance_bp==12000 and fact.drill.frozen_tick==fact.start_tick+1,"仅首实际课锁定12000及首课tick")
		check(f.state.military_units[f.unit].training==expected[index] and f.state.military_units[f.unit].fatigue==4*(index+1) and f.state.wallets[fact.terms.recipient_wallet_id].balance==150*(index+1)/4,"正常累计先floor再乘120%，费用/疲劳原值")
		before=f.state.duplicate(true)
		check(Training.step(f.state,f.data,action,f.state.tick).get("applied")==false and before==f.state,"同action/tick重复不冻第二次或双结算")
		if index==0:
			check(Training.transition_drill(f.state,f.data,f.actor,TrainingDrill.EXIT_KIND,{"action_id":action.id}).get("code")=="TrainingDrillAlreadyFrozen" and before==f.state,"首课后原action不能再改题/重置")
			var tampered: Dictionary=f.state.duplicate(true); tampered.facts[action.id].drill.performance_bp=11900
			check(not Training.validate_state(tampered,f.data).ok,"保存成绩不允许调用者改倍率")
			# 模拟同世界其他合法成长来源后的只读快照边界；不改本训练技能或历史。
			f.state.actors[f.actor].skills.tactics+=1; f.state.actors[f.actor].attributes.intellect+=1
		elif index==1:
			var parsed: Dictionary=Strict.parse_text(JSON.stringify(f.state))
			check(parsed.ok and Training.validate_state(parsed.value,f.data).ok,"两课冻结状态及真实源收据存读合法")
			loaded={"state":parsed.value,"data":f.data,"actor":f.actor,"unit":f.unit}
		else:
			advance(loaded,loaded.state.actions[action.id])
	check(f.state==loaded.state,"保存后相同两课续跑整state/RNG/ID一致且不重抽")
	check(total_money(f.state)==money and f.state.wallets[action.params.training.recipient_wallet_id].balance==150,"军阵收益不创造钱或重复支付150")
	before=f.state.duplicate(true)
	check(Training.finish(f.state,f.data,action,f.state.tick).already_completed and before==f.state,"完成后重复finish不追补或再给倍率")
	check(not Training.transition_drill(f.state,f.data,f.actor,TrainingDrill.EXIT_KIND,{"action_id":action.id}).ok and before==f.state,"完成动作不存在后旧输入不能重开")

## 零课取消只封实例且释放150；部分取消保已经兑现的倍率差额，不补完剩余奖励。
func cancellation_boundaries() -> void:
	for provided in [0,2]:
		var f: Dictionary=drill_fixture(); var action: Dictionary=start_drill(f)
		if action.is_empty(): return
		answer(f,action,0,1 if provided==0 else 4)
		for index in range(provided): advance(f,action)
		var before: Dictionary=f.state.duplicate(true)
		var cancelled: Dictionary=Training.cancel(f.state,f.data,f.actor,action.id)
		check(cancelled.ok and cancelled.details.fee_paid==150*provided/4 and cancelled.details.released_unprovided_fee==150-150*provided/4,"取消只按已提供课时结费用")
		Step.release(f.state,action)
		var fact: Dictionary=f.state.facts[action.id]
		check(fact.drill.phase==("closed" if provided==0 else "frozen") and fact.drill.performance_bp==(null if provided==0 else 12000),"零课只封题，部分取消保持首课成绩")
		check(f.state.military_units[f.unit].training==(0 if provided==0 else 3) and f.state.military_units[f.unit].fatigue==4*provided and f.state.rng==before.rng,"取消不加收益/疲劳或抽新seed")
		check(Training.validate_state(f.state,f.data).ok and Strict.parse_text(JSON.stringify(f.state)).value==f.state,"取消后完整训练历史可严格保存")
		before=f.state.duplicate(true)
		check(not Training.transition_drill(f.state,f.data,f.actor,TrainingDrill.EXIT_KIND,{"action_id":action.id}).ok and before==f.state,"取消后旧题面输入不能复活课程")

## 未完成手动题首课按自动基线；无题面旧23/9形状及零抽种保留，坏实例拒绝而不降级。
func baseline_and_failures() -> void:
	var f: Dictionary=drill_fixture(); var action: Dictionary=start_drill(f)
	if action.is_empty(): return
	answer(f,action,0,1); advance(f,action)
	check(f.state.facts[action.id].drill.performance_bp==10000 and f.state.facts[action.id].drill.challenge.result.completion=="exited","未答完在首真实课前关闭为10000")
	var before: Dictionary=f.state.duplicate(true)
	check(Training.transition_drill(f.state,f.data,f.actor,TrainingDrill.EXIT_KIND,{"action_id":"action:stale"}).get("code")=="StaleMilitaryTrainingAction" and before==f.state,"过时action早拒绝且状态不变")
	check(not Training.transition_drill(f.state,f.data,"actor.he",TrainingDrill.EXIT_KIND,{"action_id":action.id}).ok and before==f.state,"非本人不能改当前实例")
	f=drill_fixture(false); action=start_drill(f,"automatic")
	if action.is_empty(): return
	check(action.params.training.size()==23 and f.state.facts[action.id].size()==9,"无题面保留原terms23/fact9")
	advance(f,action)
	check(f.state.military_units[f.unit].training==1 and not f.state.facts[action.id].has("drill"),"原自动基线未插入默认实例")
	f=drill_fixture(); action=start_drill(f)
	if action.is_empty(): return
	check(Training.validate_state(f.state,f.data).ok,"正常一次抽种来源可回算")
	var broken: Dictionary=f.state.duplicate(true); broken.facts[action.id].erase("drill")
	before=broken.duplicate(true)
	check(not Training.step(broken,f.data,broken.actions[action.id],broken.tick+1).ok and broken==before,"缺实例不能退回10000或发生部分扣款")
	before=f.state.duplicate(true)
	check(Training.begin(f.state,f.data,action,{}).get("code")=="MilitaryTrainingAlreadyStarted" and f.state==before,"同action重开不重抽seed")

## 本门只跑军阵新接缝，继承帮助函数而不执行旧191组；脚本错误由run_checked另外拦截。
func _run() -> void:
	drill_definition=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/drill_challenge/definition.json")).value
	manual_course(); cancellation_boundaries(); baseline_and_failures()
	print("MILITARY_TRAINING_DRILL_CHECKS=%d FAILURES=%d"%[checks,failures])
	quit(1 if failures else 0)
