## 普通课程中断终态保留真实课时成长；课中仍待结算，不新增答题时序、课程公式或恢复协议。
extends "res://tests/test_study_etiquette.gd"

## 取消沿正式Module，来源失效沿原Runtime；统一释放仍只由既有Step执行。
func end_partial(f: Dictionary, action: Dictionary, ending: String) -> Dictionary:
	if ending=="teacher_unavailable":
		f.state.actors.teacher.available=false
		return advance(f,action).get("outcome",{})
	var params: Dictionary={"action_id":action.id}
	var offer: Dictionary=Module.preview(f.state,f.data,"student",Study.CANCEL_KIND,params)
	if not check(offer.ok,"当前部分课程可取消 "+str(offer)): return {}
	var result: Dictionary=Module.apply(f.state,f.data,"student",Study.CANCEL_KIND,params,offer)
	if not check(result.ok,"正式Module结算部分成长 "+str(result)): return {}
	Step.release(f.state,action)
	return result.details

## 零课、半课与临末课只保留已教金额和成长；完整时长不被当作已完成，取消不多走半日。
func partial_lessons() -> void:
	for duration in [2,6]:
		for provided in ([0,1] if duration==2 else [0,1,duration-1]):
			for ending in ["cancel","teacher_unavailable"]:
				var f: Dictionary=fixture(); f.params.ticks=duration
				var action: Dictionary=begin(f)
				if action.is_empty(): continue
				for number in range(provided): advance(f,action)
				check(f.state.actors.student.skills.rites==80,"课中pending语义不变")
				var paid_before: Dictionary=f.state.wallets.duplicate(true)
				var outcome: Dictionary=end_partial(f,action,ending)
				check(outcome.get("xp_gained")==20*provided and f.state.actors.student.skills.rites==80+20*provided,"只保留真实已教XP：%s/%d/%d"%[ending,duration,provided])
				check(not outcome.get("success",true) and outcome.get("completed_lessons")==provided and outcome.get("fee_paid")==30*provided and outcome.get("unprovided_released")==30*(duration-provided),"失败终态区分实际课时及未提供份额")
				check(f.state.wallets["wallet.student"].balance==paid_before["wallet.student"].balance and f.state.wallets["wallet.teacher"].balance==paid_before["wallet.teacher"].balance and f.state.wallets["wallet.student"].reserved.is_empty(),"终态不复算已支付费用，唯一释放余款")
				check(f.state.tick==provided+(1 if ending=="teacher_unavailable" else 0) and f.state.actors.student.fatigue==6*provided,"保持原失效时钟与已上课疲劳")
				var before: Dictionary=f.state.duplicate(true)
				var repeated: Dictionary=Study.finish(f.state,f.data,action,f.state.tick)
				check(repeated.get("xp_gained")==20*provided and f.state==before,"重复finish只读历史部分结果")
				check(not Study.cancel(f.state,f.data,"student",action.id).ok and f.state==before,"旧取消不再发奖或付款")
				var restored: Dictionary=Strict.parse_text(JSON.stringify(f.state))
				check(restored.ok and restored.value==f.state and Study.validate_state(restored.value,f.data).ok,"终态存读不重发已结算成长")

## 设施和教师上限仍限制实际可领部分，不以账面课时乘数突破既有资格。
func partial_caps() -> void:
	for mode in ["facility","teacher"]:
		for ending in ["cancel","teacher_unavailable"]:
			var initial: int=290 if mode=="facility" else 1195
			var limit: int=300 if mode=="facility" else 1200
			var f: Dictionary=fixture(mode,initial,1200)
			var action: Dictionary=begin(f)
			if action.is_empty(): continue
			advance(f,action)
			var outcome: Dictionary=end_partial(f,action,ending)
			check(outcome.get("xp_gained")==limit-initial and f.state.actors.student.skills.rites==limit,"半课实际经验按原上限截断 "+mode)

## 先答或一课后才答均保留原窗口；中断沿同一关闭与倍率取整，不增加首课强制自动。
func original_challenge_timing() -> void:
	for solve_after_lesson in [false,true]:
		for ending in ["cancel","teacher_unavailable"]:
			var f: Dictionary=challenge_fixture(); f.state.actors.student.fatigue=68
			var action: Dictionary=begin(f)
			if action.is_empty(): continue
			if not solve_after_lesson: solve(f,action)
			advance(f,action)
			if solve_after_lesson: solve(f,action)
			check(f.state.actors.student.skills.rites==80,"题面操作和课中均不抢先写XP")
			var random_before: Dictionary=f.state.rng.duplicate(true)
			var outcome: Dictionary=end_partial(f,action,ending)
			check(outcome.get("xp_gained")==21 and f.state.actors.student.skills.rites==101,"半课18先取整再乘120%为21，终态保留实际奖励")
			check(f.state.rng==random_before and f.state.facts[action.id].etiquette.consumed,"关闭不重抽题，原实例消费一次")
	var pending: Dictionary=challenge_fixture(); var pending_action: Dictionary=begin(pending)
	if not pending_action.is_empty():
		advance(pending,pending_action)
		var result: Dictionary=end_partial(pending,pending_action,"cancel")
		check(result.get("xp_gained")==20 and result.get("etiquette",{}).get("completion")=="exited","未完成手动题面在原终态才使用自动基础成绩")

## 已完成历史不能被后来的课程覆盖或再奖励；旧事实只返回当时自己的已提供份额。
func historical_retry() -> void:
	var f: Dictionary=fixture(); var old: Dictionary=begin(f)
	if old.is_empty(): return
	advance(f,old); end_partial(f,old,"cancel")
	var action: Dictionary=begin(f)
	if action.is_empty(): return
	advance(f,action); advance(f,action)
	var before: Dictionary=f.state.duplicate(true)
	check(Study.finish(f.state,f.data,old,f.state.tick).get("xp_gained")==20 and f.state==before,"后课完成后重读旧部分回执不覆盖新成长")
	check(f.state.actors.student.skills.rites==140 and Study.validate_state(f.state,f.data).ok,"正常后课仍给原40且历史同时有效")

## 来源失效终态也不得覆盖外变成长；失败在关闭题面和写入之前拒绝，等待外层原子丢弃。
func changed_growth_is_rejected() -> void:
	var f: Dictionary=fixture(); var action: Dictionary=begin(f)
	if action.is_empty(): return
	advance(f,action)
	f.state.facts[action.id].code="ActorUnavailable"; action.remaining_ticks=0
	f.state.actors.student.skills.rites+=1
	var before: Dictionary=f.state.duplicate(true)
	check(Study.finish(f.state,f.data,action,f.state.tick+1).get("code")=="StudyGrowthChanged" and f.state==before,"失效结课不能把外变XP重置为开课XP加收益")

## 只跑新的终态保留合同；原完整课程、题面和正式Kernel邻接门另行按真实输入验证。
func run_checks() -> void:
	partial_lessons(); partial_caps(); original_challenge_timing(); historical_retry(); changed_growth_is_rejected()
	print("SKILL_STUDY_PROGRESS_CHECKS=%d FAILURES=%d"%[checks,failures])
	quit(0 if failures==0 else 1)
