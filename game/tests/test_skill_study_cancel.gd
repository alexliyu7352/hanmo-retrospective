## 主动中断仅测试当前课程精确身份与稳定半日结算；唯一释放器模拟中央同候选协调，完整Kernel另验。
extends "res://tests/test_skill_study.gd"

## 按正式Module私有报价执行取消，然后调用现有唯一释放器；不自造退款或重置人物状态。
func cancel_course(f: Dictionary, action: Dictionary) -> Dictionary:
	var params: Dictionary={"action_id":action.id}
	var quoted: Dictionary=Module.preview(f.state,f.data,"student",Study.CANCEL_KIND,params)
	if not check(quoted.ok,"本人当前课程可预览中断 "+str(quoted)): return quoted
	var result: Dictionary=Module.apply(f.state,f.data,"student",Study.CANCEL_KIND,params,quoted)
	if not check(result.ok,"真实Module取消事实 "+str(result)): return result
	Step.release(f.state,action)
	check(Study.validate_state(f.state,f.data).ok,"取消并统一释放后稳定状态合法")
	return result

## 零课时、部分课时、旧行动与他人身份分别有真实断言，不运行其他成长大门。
func run_checks() -> void:
	var f: Dictionary=fixture(); var action: Dictionary=begin(f)
	if action.is_empty(): quit(1); return
	var before: Dictionary=f.state.duplicate(true)
	check(Study.preview_cancel(f.state,f.data,"teacher",{"action_id":action.id}).get("code")=="NotStudying" and f.state==before,"教师参与同课不等于可取消学生课程")
	check(Study.preview_cancel(f.state,f.data,"student",{"action_id":"action.old"}).get("code")=="StaleStudyAction" and f.state==before,"旧action_id不能取消当前课")
	var result: Dictionary=cancel_course(f,action)
	if not result.ok: quit(1); return
	check(result.details.fee_paid==0 and result.details.unprovided_released==60 and result.details.xp_gained==0,"零课时取消不收费不奖励，全额释放未提供部分")
	check(f.state.tick==0 and f.state.actors.student.skills.rites==80 and f.state.wallets["wallet.student"].balance==1000 and f.state.wallets["wallet.student"].reserved.is_empty() and f.state.actions.is_empty(),"取消不推进日期，双方资产占用如实释放")
	before=f.state.duplicate(true)
	check(not Module.apply(f.state,f.data,"student",Study.CANCEL_KIND,{"action_id":action.id},{}).ok and f.state==before,"已取消的领域重试无副作用；命令去重仍由Kernel负责")
	var next_action: Dictionary=begin(f)
	if next_action.is_empty(): quit(1); return
	check(Study.preview_cancel(f.state,f.data,"student",{"action_id":action.id}).get("code")=="StaleStudyAction" and next_action.id!=action.id,"重新开课后旧取消指令仍拒绝")
	var stale: Dictionary=Study.preview_cancel(f.state,f.data,"student",{"action_id":next_action.id})
	advance(f,next_action)
	before=f.state.duplicate(true)
	check(Module.apply(f.state,f.data,"student",Study.CANCEL_KIND,{"action_id":next_action.id},stale).get("code")=="StaleStudyQuote" and f.state==before,"课时推进使旧取消私有报价失效")
	result=cancel_course(f,next_action)
	check(result.ok and result.details.completed_lessons==1 and result.details.fee_paid==30 and result.details.unprovided_released==30 and result.details.xp_gained==20 and f.state.actors.student.skills.rites==100,"部分取消保留实际一课20XP与费用，不奖未提供课时")
	check(f.state.tick==1 and f.state.actors.student.fatigue==6 and f.state.wallets["wallet.student"].balance==970 and f.state.wallets["wallet.teacher"].balance==30 and f.state.wallets["wallet.student"].reserved.is_empty() and f.state.actors.teacher.action_id.is_empty(),"部分取消保留已过课时和费用，余款解预留且教师释放")
	var restored: Dictionary=Strict.parse_text(JSON.stringify(f.state))
	check(restored.ok and restored.value==f.state and Study.validate_state(restored.value,f.data).ok,"取消事实严格JSON恢复不重结算")
	f=fixture(); action=begin(f)
	if action.is_empty(): quit(1); return
	f.state.roles["role.school"].enabled=false
	result=cancel_course(f,action)
	check(result.ok and f.state.tick==0,"课程来源失效后本人仍可立即解除自己的课程")
	print("SKILL_STUDY_CANCEL_CHECKS=%d FAILURES=%d"%[checks,failures])
	quit(0 if failures==0 else 1)
