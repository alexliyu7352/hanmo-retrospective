## 自动普通课程暂停的受影响生命周期门；同一钱包/身份、真实课时和恢复基线，不代替整合节点。
extends "res://tests/test_skill_study.gd"
const Activity=preload("res://core/views/study_activity_input.gd")

## 暂停与恢复均调用注册领域命令；不直接写占用或保留款。
func control(f: Dictionary, action: Dictionary, kind: String) -> Dictionary:
	var params: Dictionary={"action_id":action.id}
	var quoted: Dictionary=Module.preview(f.state,f.data,"student",kind,params)
	if not check(quoted.ok,"显式课程控制报价 "+kind+" "+str(quoted)): return quoted
	var result: Dictionary=Module.apply(f.state,f.data,"student",kind,params,quoted)
	check(result.ok and Study.validate_state(f.state,f.data).ok,"领域控制保持稳定状态 "+kind+" "+str(result))
	return result

## 同一课中释放师徒、异地等待、疲劳变化后恢复，课费和经验只结算实际授课。
func run_checks() -> void:
	var f: Dictionary=fixture(); f.state.player_actor_id="student"; f.state.actors.student.fatigue=38; f.params.ticks=3
	var action: Dictionary=begin(f)
	if action.is_empty(): quit(1); return
	advance(f,action)
	var original: Dictionary=f.state.duplicate(true)
	if not control(f,action,Study.PAUSE_KIND).ok: quit(1); return
	var fact: Dictionary=f.state.facts[action.id]
	check(action.participants.is_empty() and f.state.actors.student.action_id.is_empty() and f.state.actors.teacher.action_id.is_empty(),"暂停释放双方且保留同一行动")
	check(fact.terms==original.facts[action.id].terms and fact.lessons==original.facts[action.id].lessons and f.state.wallets==original.wallets and f.state.actors.student.skills==original.actors.student.skills,"暂停不改原条款、课时、资金或待结经验")
	check(Study.preview_control(f.state,f.data,"student",Study.PAUSE_KIND,{"action_id":action.id}).get("code")=="StudyAlreadyPaused","重复暂停明确拒绝")
	check(Study.preview(f.state,f.data,"student",f.params).get("code")=="StudyAlreadyPending","原课程未结束不能另开平行课程")
	advance(f,action); advance(f,action)
	check(fact.lessons.size()==1 and f.state.wallets==original.wallets and action.remaining_ticks==2,"两半日暂停期间不授课不收费")
	var activity: Dictionary=Activity.active(f.state)
	check(activity.paused and activity.action_id==action.id and activity.paused_ticks==2 and activity.reserved==60,"有限入口从暂停事实找回原课程且单列耗时")
	f.state.actors.teacher.action_id="other"
	var before: Dictionary=f.state.duplicate(true)
	check(Study.preview_control(f.state,f.data,"student",Study.RESUME_KIND,{"action_id":action.id}).get("code")=="ActorBusy" and f.state==before,"教师另有安排不能被恢复强占")
	f.state.actors.teacher.action_id=""; f.state.actors.student.skills.rites=120
	check(Study.validate_state(f.state,f.data).ok,"暂停中正常外部经验增长不阻断世界")
	f.state.actors.student.location.place_id="away"
	before=f.state.duplicate(true)
	check(not Study.preview_control(f.state,f.data,"student",Study.RESUME_KIND,{"action_id":action.id}).ok and f.state==before,"异地恢复失败不挪人物和课程")
	f.state.actors.student.location.place_id="school"; f.state.actors.student.fatigue=0; f.state.actors.teacher.fatigue=30
	f.state.actors.teacher.skills.rites=99
	check(Study.preview_control(f.state,f.data,"student",Study.RESUME_KIND,{"action_id":action.id}).get("code")=="StudyTeacherNotQualified","原教师已不合格不能暗换教师")
	f.state.actors.teacher.skills.rites=700
	if not control(f,action,Study.RESUME_KIND).ok: quit(1); return
	check(action.participants==["student","teacher"] and f.state.actors.teacher.action_id==action.id and fact.terms==original.facts[action.id].terms,"恢复同一身份和原教师，旧条款不被更新")
	check(Study.preview_control(f.state,f.data,"student",Study.RESUME_KIND,{"action_id":action.id}).get("code")=="StudyNotPaused","重复恢复不再获取占用")
	var decoded: Dictionary=Strict.parse_text(JSON.stringify(f.state))
	check(decoded.ok and decoded.value==f.state and Study.validate_state(decoded.value,f.data).ok,"暂停后恢复的历史原样严格JSON回读")
	advance(f,action)
	check(fact.lessons[1].fatigue_before==0 and fact.lessons[1].xp==20 and f.state.actors.teacher.fatigue==32,"恢复从当前真实疲劳继续原公式")
	control(f,action,Study.PAUSE_KIND); advance(f,action); control(f,action,Study.RESUME_KIND)
	var result: Dictionary=advance(f,action)
	check(result.outcome.success and result.outcome.xp_gained==60 and f.state.actors.student.skills.rites==180 and fact.xp_settlement_before==120 and f.state.tick==6 and fact.completed_tick==6,"三课时加三停课半日，终态仅一次发60经验")
	check(f.state.wallets["wallet.student"].balance==910 and f.state.wallets["wallet.teacher"].balance==90 and f.state.wallets["wallet.student"].reserved.is_empty(),"完整课程只付三课费用且最终释放零余款")
	check(Study.preview_control(f.state,f.data,"student",Study.RESUME_KIND,{"action_id":action.id}).get("code")=="StaleStudyAction","已终结身份不能恢复")
	f=fixture(); action=begin(f); advance(f,action); control(f,action,Study.PAUSE_KIND); advance(f,action)
	f.state.actors.student.action_id="other"
	var params: Dictionary={"action_id":action.id}
	var quote: Dictionary=Module.preview(f.state,f.data,"student",Study.CANCEL_KIND,params)
	result=Module.apply(f.state,f.data,"student",Study.CANCEL_KIND,params,quote)
	check(result.ok and result.details.completed_lessons==1 and result.details.xp_gained==20,"暂停中取消只结算既有一课")
	Step.release(f.state,action)
	check(f.state.actors.student.action_id=="other" and f.state.wallets["wallet.student"].reserved.is_empty() and Study.validate_state(f.state,f.data).ok,"取消只释放本课余款，不清除学生另一个行动")
	f=fixture("facility",290,0); action=begin(f); advance(f,action); control(f,action,Study.PAUSE_KIND)
	f.state.actors.student.skills.rites=350
	check(Study.validate_state(f.state,f.data).ok and Study.preview_control(f.state,f.data,"student",Study.RESUME_KIND,{"action_id":action.id}).get("code")=="StudyResumeLevelLimit","外部成长越过原课程上限可保存但不能再恢复低级课程")
	params={"action_id":action.id}; quote=Module.preview(f.state,f.data,"student",Study.CANCEL_KIND,params)
	result=Module.apply(f.state,f.data,"student",Study.CANCEL_KIND,params,quote); Step.release(f.state,action)
	check(result.ok and result.details.xp_gained==0 and f.state.actors.student.skills.rites==350 and Study.validate_state(f.state,f.data).ok,"已越上限的外部经验不被回退，取消回执不虚报本课经验")
	print("SKILL_STUDY_PAUSE_CHECKS=%d FAILURES=%d"%[checks,failures])
	quit(0 if failures==0 else 1)
