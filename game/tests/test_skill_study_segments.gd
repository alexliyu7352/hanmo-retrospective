## 显式分段政策的局部高风险门：逐课资金/经验、晚答补差、恢复外部成长及旧政策隔离。
extends "res://tests/test_study_etiquette.gd"

## 只在夹具课程显式选定政策后应用新条款；不修改历史课程或随机条件。
func segmented_fixture(mode: String="manual", ticks: int=3) -> Dictionary:
	var f: Dictionary=challenge_fixture(mode)
	f.data.roles["role.school"].skill_courses[0].settlement_policy="per_lesson_v1"
	f.params.ticks=ticks
	return f

## 显式控制仍通过课程注册入口，失败保留真实code与稳定事实。
func control(f: Dictionary, action: Dictionary, kind: String) -> Dictionary:
	var params: Dictionary={"action_id":action.id}
	var quote: Dictionary=Module.preview(f.state,f.data,"student",kind,params)
	if not check(quote.ok,"新政策控制预览 "+kind+" "+str(quote)): return quote
	var result: Dictionary=Module.apply(f.state,f.data,"student",kind,params,quote)
	check(result.ok and Study.validate_state(f.state,f.data).ok,"新政策控制验证 "+kind+" "+str(result))
	return result

## 每半日立即获得原正常经验；完课仅封存原结果而不重新发放。
func automatic_segments() -> void:
	var f: Dictionary=segmented_fixture("automatic",2); f.state.actors.student.fatigue=38
	var action: Dictionary=begin(f)
	if action.is_empty(): return
	var fact: Dictionary=f.state.facts[action.id]
	check(fact.terms.settlement_policy=="per_lesson_v1" and Study.credited_xp(fact)==0,"新条款明确政策且开课零成长")
	advance(f,action)
	check(f.state.actors.student.skills.rites==100 and Study.credited_xp(fact)==20 and fact.lessons[0].segment.ordinal==1 and fact.lessons[0].segment.settled_tick==1,"第一真实半日立即入20且保唯一序号/tick")
	check(f.state.wallets["wallet.student"].balance==970 and f.state.wallets["wallet.student"].reserved[action.id]==30 and f.state.wallets["wallet.teacher"].balance==30,"同一课时只付原30钱，未提供30仍准确预留")
	var restored: Dictionary=Strict.parse_text(JSON.stringify(f.state))
	check(restored.ok and Study.validate_state(restored.value,f.data).ok,"逐课已发经验严格JSON回读合法")
	var twin: Dictionary=f.duplicate(true); twin.state=restored.value
	var result: Dictionary=advance(f,action); advance(twin,twin.state.actions[action.id])
	check(f.state==twin.state and f.state.actors.student.skills.rites==118 and result.outcome.xp_gained==38,"恢复不中断原20+18取整总额")
	var before: Dictionary=f.state.duplicate(true)
	check(Study.finish(f.state,f.data,action,f.state.tick).xp_gained==38 and f.state==before,"重复终结不重复发分段经验或课费")
	var legacy: Dictionary=fixture(); var old_action: Dictionary=begin(legacy); advance(legacy,old_action)
	check(not legacy.state.facts[old_action.id].terms.has("settlement_policy") and not legacy.state.facts[old_action.id].lessons[0].has("segment") and legacy.state.actors.student.skills.rites==80,"旧catalog课时保原待结语义和形状")

## 原先允许的晚答仍合法；只补实际上过课的原取整差额，不给未来课时预支。
func late_performance() -> void:
	var f: Dictionary=segmented_fixture(); f.state.actors.student.fatigue=68
	var action: Dictionary=begin(f)
	if action.is_empty(): return
	var fact: Dictionary=f.state.facts[action.id]
	var rng: Dictionary=f.state.rng.duplicate(true)
	advance(f,action); advance(f,action)
	check(f.state.actors.student.skills.rites==113 and fact.etiquette.challenge.phase=="active","未答先正常学两半日，立即18+15经验")
	var taught: Array=fact.lessons.duplicate(true)
	solve(f,action)
	check(f.state.actors.student.skills.rites==119 and fact.lessons[0].segment.adjustment.xp_gained==3 and fact.lessons[1].segment.adjustment.xp_gained==3,"晚答按每课21-18及18-15各补3")
	for index in range(taught.size()):
		var actual: Dictionary=fact.lessons[index].duplicate(true); actual.segment.erase("adjustment")
		check(actual==taught[index],"晚答不改写基础教学收据")
	check(f.state.tick==2 and f.state.rng==rng and f.state.wallets["wallet.teacher"].balance==60,"晚答不推进时间、随机或重复收费")
	var result: Dictionary=advance(f,action)
	check(result.outcome.xp_gained==57 and f.state.actors.student.skills.rites==137 and not fact.lessons[2].segment.performance_pending and not fact.lessons[2].segment.has("adjustment"),"后续课时直接按已决倍率，整课21+18+18不变")
	var early: Dictionary=segmented_fixture(); var early_action: Dictionary=begin(early); solve(early,early_action)
	check(early.state.actors.student.skills.rites==80 and Study.credited_xp(early.state.facts[early_action.id])==0,"零课时答题不预发经验")
	var skipped: Dictionary=segmented_fixture(); var skip_action: Dictionary=begin(skipped)
	advance(skipped,skip_action); advance(skipped,skip_action); result=advance(skipped,skip_action)
	check(result.outcome.xp_gained==60 and result.outcome.etiquette.completion=="exited","一直未答仍可正常结课，原退出回退不添倍率")

## 手动课程暂停保同一题面、种子和身份；外部经验与迟答只各归自己的来源。
func manual_pause_and_external_growth() -> void:
	var f: Dictionary=segmented_fixture(); var action: Dictionary=begin(f)
	if action.is_empty(): return
	advance(f,action)
	var fact: Dictionary=f.state.facts[action.id]
	var challenge: Dictionary=fact.etiquette.duplicate(true)
	control(f,action,Study.PAUSE_KIND); advance(f,action); advance(f,action)
	check(fact.lessons.size()==1 and f.state.actors.student.skills.rites==100 and action.participants.is_empty() and fact.etiquette==challenge,"未决手动暂停释放双方，两半日无暗中教学或重抽")
	f.state.actors.student.skills.rites=130
	check(Study.validate_state(f.state,f.data).ok,"暂停中外部30经验可保留并正常保存")
	control(f,action,Study.RESUME_KIND)
	check(fact.pauses[-1].credited_xp==20 and fact.pauses[-1].student_xp==130,"恢复同时记录外部基线和本课已发贡献")
	solve(f,action)
	check(f.state.actors.student.skills.rites==134 and Study.credited_xp(fact)==24,"恢复后晚答只补本课4，外部30不重复放大")
	control(f,action,Study.PAUSE_KIND); control(f,action,Study.RESUME_KIND)
	check(fact.pauses[-1].credited_xp==24,"同tick再次暂停恢复不重复计算迟答奖励")
	advance(f,action); var result: Dictionary=advance(f,action)
	check(result.outcome.xp_gained==72 and f.state.actors.student.skills.rites==182 and f.state.tick==5 and not fact.has("xp_settlement_before"),"三课72加外部30只一次保留，暂停只延时")
	check(f.state.wallets["wallet.teacher"].balance==90 and f.state.wallets["wallet.student"].reserved.is_empty(),"暂停不改原90总价且结束精确释放")

## 上限、暂停中取消及来源失效都只保已授课贡献，不回退外部经验或误发余课。
func cap_and_terminal_paths() -> void:
	var f: Dictionary=segmented_fixture(); f.params.ticks=2
	f.data.roles["role.school"].skill_courses[0].mode="facility"; f.state.actors.student.skills.rites=290
	var action: Dictionary=begin(f); advance(f,action)
	var fact: Dictionary=f.state.facts[action.id]
	check(f.state.actors.student.skills.rites==300 and Study.credited_xp(fact)==10,"设施原上限只给剩余10，不拖到结课")
	control(f,action,Study.PAUSE_KIND)
	check(Study.preview_control(f.state,f.data,"student",Study.RESUME_KIND,{"action_id":action.id}).get("code")=="StudyResumeLevelLimit","本课已真实达到上限后恢复仍按当前资格拒绝")
	f.state.actors.student.skills.rites=350
	var result: Dictionary=Study.cancel(f.state,f.data,"student",action.id); Step.release(f.state,action)
	check(result.ok and result.details.xp_gained==10 and f.state.actors.student.skills.rites==350 and Study.validate_state(f.state,f.data).ok,"越上限外部成长不回退，取消仍正确报告先前10课程经验")
	f=segmented_fixture(); action=begin(f); advance(f,action); solve(f,action)
	f.state.actors.teacher.available=false; result=advance(f,action)
	check(not result.outcome.success and result.outcome.xp_gained==24 and f.state.actors.student.skills.rites==104 and result.outcome.unprovided_released==60,"教师失效不补课，已教24与未用60各自留存")
	f=segmented_fixture(); action=begin(f); advance(f,action); control(f,action,Study.PAUSE_KIND)
	f.state.actors.student.action_id="other"
	result=Study.cancel(f.state,f.data,"student",action.id); Step.release(f.state,action)
	check(result.ok and f.state.actors.student.action_id=="other" and f.state.actors.student.skills.rites==100 and Study.validate_state(f.state,f.data).ok,"暂停取消保已有20且不释放别的行动")

## 议价课程也只补学习表现；真实钱款与练习折扣保持两个原合同。
func bargain_late_pause() -> void:
	var f: Dictionary=fixture()
	for person in ["student","teacher"]:
		f.state.actors[person].skills.commerce=80 if person=="student" else 650
		f.state.actors[person].attributes={"administration":60,"charisma":55,"intellect":50}
	var definition: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/bargain_challenge/definition.json")).value
	f.data.bargain_challenges={definition.id:definition}
	f.data.texts=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/bargain_challenge/texts.json")).value
	f.data.roles["role.school"].skill_courses=[{"skill_id":"commerce","mode":"teacher","wallet_id":"wallet.teacher","settlement_policy":"per_lesson_v1","challenge":{"kind":"bargain","id":definition.id,"context":{"budget":650,"quantity":10,"relation_discount_bp":300}}}]
	f.params.skill_id="commerce"; f.params.ticks=3; f.params.challenge_mode="manual"
	var random=RandomNumberGenerator.new(); random.seed=7351
	f.state.rng={"minigame":{"seed":str(random.seed),"state":str(random.state)}}
	var action: Dictionary=begin(f); advance(f,action)
	var fact: Dictionary=f.state.facts[action.id]
	var stream: Dictionary=f.state.rng.duplicate(true)
	control(f,action,Study.PAUSE_KIND); advance(f,action); control(f,action,Study.RESUME_KIND)
	for step_id in ["round.1","round.2","round.3"]:
		var params: Dictionary={"action_id":action.id,"step_id":step_id,"option_id":"small"}
		var offer: Dictionary=Study.preview_challenge(f.state,f.data,"student",Etiquette.GENERIC_CHOOSE_KIND,params)
		if not check(offer.ok,"议价晚答预览原题面"): return
		var answer: Dictionary=Study.apply_challenge(f.state,f.data,"student",Etiquette.GENERIC_CHOOSE_KIND,params,offer)
		check(answer.ok and not answer.details.price_applied and Study.validate_state(f.state,f.data).ok,"议价晚答验证且不改真实价格")
	check(f.state.actors.student.skills.commerce==104 and fact.lessons[0].segment.adjustment.xp_gained==4 and f.state.rng==stream and f.state.wallets["wallet.teacher"].balance==30,"议价迟答只补一课4，seed/费用保持")
	advance(f,action); var result: Dictionary=advance(f,action)
	check(result.outcome.xp_gained==72 and f.state.actors.student.skills.commerce==152 and result.outcome.fee_paid==90 and not result.outcome.challenge.price_applied,"完整议价课程逐课72且原费用90不受练习折扣影响")

## 同一作者目录可保留旧课程；新条款只影响明确选择的那门课。
func mixed_course_policies() -> void:
	var f: Dictionary=segmented_fixture()
	f.params.skill_id="martial"; f.params.erase("challenge_mode")
	var action: Dictionary=begin(f); advance(f,action)
	check(not f.state.facts[action.id].terms.has("settlement_policy") and f.state.actors.student.skills.martial==80 and Study.validate_state(f.state,f.data).ok,"目录有新课程不会隐式升级另一门旧课程")

## 存读拒绝缺失政策、缺课时证据和篡改结算，不在校验时补发历史贡献。
func tamper_rejections() -> void:
	var f: Dictionary=segmented_fixture(); var action: Dictionary=begin(f); advance(f,action); solve(f,action)
	for field in ["policy","segment","ordinal","tick","gain","adjustment","bonus","baseline"]:
		var bad: Dictionary=f.state.duplicate(true)
		var fact: Dictionary=bad.facts[action.id]
		match field:
			"policy": fact.terms.erase("settlement_policy"); bad.actions[action.id].params.study.erase("settlement_policy")
			"segment": fact.lessons[0].erase("segment")
			"ordinal": fact.lessons[0].segment.ordinal=2
			"tick": fact.lessons[0].segment.settled_tick=0
			"gain": fact.lessons[0].segment.xp_gained+=1
			"adjustment": fact.lessons[0].segment.erase("adjustment")
			"bonus": fact.lessons[0].segment.adjustment.xp_gained+=1
			"baseline": fact.lessons[0].segment.adjustment.xp_before+=1
		check(not Study.validate_state(bad,f.data).ok,"伪造分段证据被拒 "+field)
	var paused: Dictionary=segmented_fixture(); var paused_action: Dictionary=begin(paused)
	advance(paused,paused_action); advance(paused,paused_action); control(paused,paused_action,Study.PAUSE_KIND)
	var huge: Dictionary=paused.state.duplicate(true)
	huge.facts[paused_action.id].lessons[0].segment.xp_gained=9223372036854775807
	huge.facts[paused_action.id].lessons[1].segment.xp_gained=1
	check(not Study.validate_state(huge,paused.data).ok,"暂停累计前拒绝INT64_MAX课时贡献")
	huge=paused.state.duplicate(true)
	huge.tick=9223372036854775807
	huge.facts[paused_action.id].start_tick=huge.tick
	huge.facts[paused_action.id].erase("pauses")
	huge.facts[paused_action.id].lessons.resize(1)
	huge.facts[paused_action.id].lessons[0].segment.settled_tick=huge.tick
	check(not Study.validate_state(huge,paused.data).ok,"INT64_MAX开始tick不能溢出成下一虚假课时")
	var old_data: Dictionary=f.data.duplicate(true); old_data.roles["role.school"].skill_courses[0].erase("settlement_policy")
	check(not Study.validate_state(f.state,old_data).ok,"新政策不能降到旧catalog静默重解释")

## 有限门只覆盖分段课及直接相邻出口，完整作者/Main/银行存读由中央统一节点负责。
func run_checks() -> void:
	automatic_segments()
	late_performance()
	manual_pause_and_external_growth()
	cap_and_terminal_paths()
	bargain_late_pause()
	mixed_course_policies()
	tamper_rejections()
	print("SKILL_STUDY_SEGMENTS_CHECKS=%d FAILURES=%d"%[checks,failures])
	quit(0 if failures==0 else 1)
