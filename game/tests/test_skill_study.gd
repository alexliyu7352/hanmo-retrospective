## 普通技艺课程局部规则门：真实注册动作适配、已有释放器和严格JSON恢复，不代替整局或体验验收。
extends SceneTree
const Study=preload("res://core/domains/cultivation/skill_study_rules.gd")
const Module=preload("res://core/domains/cultivation/cultivation_module.gd")
const Runtime=preload("res://core/runtime/action_runtime.gd")
const Step=preload("res://core/world_step.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 延迟执行有限课程，不开窗口或推进长世界。
func _init() -> void:
	call_deferred("run_checks")

## 保留断言标签与实际退出状态，发现失败不伪报通过。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 最小真人与岗位夹具只补本课程所需真实字段，明确不是已发布作者目录。
func fixture(mode: String="teacher", xp: int=80, teacher_xp: int=650) -> Dictionary:
	var people: Dictionary={}
	for id in ["student","teacher"]:
		people[id]={"id":id,"alive":true,"available":true,"hp":100,"fatigue":0,"injury":0,"action_id":"","wallet_id":"wallet."+id,"skills":{"rites":xp if id=="student" else teacher_xp,"martial":xp if id=="student" else teacher_xp},"location":{"kind":"Settlement","settlement_id":"city","place_id":"school"}}
	var state: Dictionary={"tick":0,"next_instance_id":1,"actors":people,"roles":{"role.school":{"actor_id":"teacher","enabled":true}},"wallets":{"wallet.student":{"owner":{"kind":"actor","id":"student"},"balance":1000,"reserved":{}},"wallet.teacher":{"owner":{"kind":"actor","id":"teacher"},"balance":0,"reserved":{}}},"inventories":{},"facts":{},"actions":{},"battles":{}}
	var data: Dictionary={"required_capabilities":["cultivation"],"roles":{"role.school":{"place_id":"school","organization_id":null,"services":["study_skill"],"skill_courses":[{"skill_id":"rites","mode":mode,"wallet_id":"wallet.teacher"},{"skill_id":"martial","mode":mode,"wallet_id":"wallet.teacher"}]}},"places":{"school":{"settlement_id":"city"}}}
	var descriptor: Dictionary=Module.descriptor()
	var registry: Dictionary={"commands":{},"actions":{}}
	for row in descriptor.commands: registry.commands[row.kind]=row
	for row in descriptor.actions: registry.actions[row.kind]=row
	return {"state":state,"data":data,"registry":registry,"params":{"skill_id":"rites","ticks":2,"context":{"place_id":"school","provider_actor_id":"teacher","role_id":"role.school"}}}

## 从公开领域报价经真实ActionRuntime开课，测试不自行写入占用或支付结果。
func begin(f: Dictionary) -> Dictionary:
	var before: Dictionary=f.state.duplicate(true)
	var offer: Dictionary=Module.preview(f.state,f.data,"student",Study.KIND,f.params)
	if not check(offer.ok and f.state==before,"合法只读课程预览 "+str(offer)): return {}
	var result: Dictionary=Runtime.begin(f.state,f.data,"student",Study.KIND,f.params,offer,f.registry)
	if not check(result.ok,"真实注册开课 "+str(result)): return {}
	return f.state.actions[result.details.action_id]

## 以正式调度与释放器结束一个半日，保持原runtime的先完成再提交时钟次序。
func advance(f: Dictionary, action: Dictionary) -> Dictionary:
	var result: Dictionary=Runtime.step(f.state,f.data,action,f.registry.actions,{"finish_tick":f.state.tick+1})
	if not check(result.ok,"真实课程半日 "+str(result)): return result
	if result.completed: Step.release(f.state,action)
	f.state.tick+=1
	check(Study.validate_state(f.state,f.data).ok,"稳定半日课程事实可校验")
	return result

## 两半日之间跨疲劳阈值，费用按实际课时支付、经验结课一次，严格JSON恢复不改变继续结果。
func full_course() -> void:
	var f: Dictionary=fixture(); f.state.actors.student.fatigue=38
	var action: Dictionary=begin(f)
	if action.is_empty(): return
	check(f.state.wallets["wallet.student"].balance==1000 and f.state.wallets["wallet.student"].reserved[action.id]==60 and f.state.wallets["wallet.teacher"].balance==0,"开课仅预留60，零半日不预付未提供课程")
	check(f.state.actors.student.skills.rites==80 and f.state.actors.teacher.action_id==action.id,"开始不授XP且双方已占用")
	var before: Dictionary=f.state.duplicate(true)
	check(Study.begin(f.state,f.data,action,{}).code=="StudyAlreadyStarted" and f.state==before,"重复开工不重收费")
	check(not Study.finish(f.state,f.data,action,1).ok,"课时未到不可提前领取")
	check(f.registry.actions[Study.KIND].interrupt_policy.cancel=="supported" and f.state==before,"取消能力明确声明，读取声明不修改课程")
	advance(f,action)
	check(f.state.actors.student.skills.rites==80 and f.state.actors.student.fatigue==44,"第一半日只记录真实疲劳和待结课证据")
	check(f.state.wallets["wallet.student"].balance==970 and f.state.wallets["wallet.teacher"].balance==30 and f.state.wallets["wallet.student"].reserved[action.id]==30,"第一真实半日只转账30，剩余30仍预留")
	var decoded: Dictionary=Strict.parse_text(JSON.stringify(f.state))
	if not check(decoded.ok and decoded.value==f.state,"真实严格JSON恢复课中状态"): return
	var restored: Dictionary=f.duplicate(true); restored.state=decoded.value
	var outcome: Dictionary=advance(f,action)
	advance(restored,restored.state.actions[action.id])
	check(f.state==restored.state,"恢复后继续不重收费、重走课时或重发XP")
	check(outcome.outcome.success and outcome.outcome.xp_gained==38 and f.state.actors.student.skills.rites==118,"疲劳跨40后20+18XP且只写唯一skills")
	check(f.state.actions.is_empty() and f.state.actors.student.action_id.is_empty() and f.state.actors.teacher.action_id.is_empty(),"结课通过原释放器解除双方占用")
	check(outcome.outcome.fee_paid==60 and outcome.outcome.unprovided_released==0 and f.state.wallets["wallet.student"].reserved.is_empty() and f.state.wallets["wallet.student"].balance+f.state.wallets["wallet.teacher"].balance==1000,"完整课程无余款，双方余额守恒")
	before=f.state.duplicate(true)
	check(Study.finish(f.state,f.data,action,f.state.tick).xp_gained==38 and f.state==before,"重试完成仅回原结果，不再奖XP")
	var forged: Dictionary=f.state.duplicate(true); forged.facts[action.id].lessons[0].xp=1200
	check(not Study.validate_state(forged,f.data).ok,"伪造半日经验被拒绝")

## 设施只能到二级，教师资格来自当前经验派生等级，旧武学掌握不会授予教师资格。
func limits() -> void:
	check([Study.level(99),Study.level(100),Study.level(299),Study.level(300),Study.level(649),Study.level(650),Study.level(1200)]==[0,1,1,2,2,3,4],"固定XP阈值不维护第二等级")
	var f: Dictionary=fixture("facility",290,0)
	var quote: Dictionary=Study.preview(f.state,f.data,"student",f.params)
	check(quote.ok and quote.expected_xp==10 and quote.cost==80,"公共设施无需假造教师等级，明确到二级只余10XP")
	var action: Dictionary=begin(f)
	if action.is_empty(): return
	advance(f,action); advance(f,action)
	check(f.state.actors.student.skills.rites==300 and Study.preview(f.state,f.data,"student",f.params).code=="StudyCourseLevelLimit","设施上限不能刷至三级")
	f=fixture("teacher",630,1200); action=begin(f)
	if action.is_empty(): return
	advance(f,action); advance(f,action)
	check(f.state.actors.student.skills.rites==670 and f.state.wallets["wallet.student"].balance==900,"真实高级教师跨三级阈值，目标级课费冻结")
	f=fixture("teacher",300,649)
	check(Study.preview(f.state,f.data,"student",f.params).code=="StudyTeacherNotQualified","同等级更多XP不冒充高级教师")
	f=fixture(); f.state.actors.student.injury=3
	check(Study.preview(f.state,f.data,"student",f.params).ok,"重伤仍可读书礼学")
	f.params.skill_id="martial"
	check(Study.preview(f.state,f.data,"student",f.params).code=="StudyNeedsRecovery","重伤拒绝重体能武艺训练")
	check(Study.lesson_xp(0,2)==15 and Study.lesson_xp(95,0)==12,"伤势与疲劳取较低的正常效率")

## 服务消失后保留已教课时成长及费用，释放未提供预留，不把中断冒充完整课程。
func interruption() -> void:
	var f: Dictionary=fixture(); var action: Dictionary=begin(f)
	if action.is_empty(): return
	advance(f,action)
	f.state.roles["role.school"].enabled=false
	var result: Dictionary=advance(f,action)
	check(result.completed and not result.outcome.success and result.outcome.xp_gained==20 and result.outcome.fee_paid==30 and result.outcome.unprovided_released==30,"中断回执明确已教20经验、已付30和释放30")
	check(f.state.actors.student.skills.rites==100 and f.state.wallets["wallet.student"].balance==970 and f.state.wallets["wallet.teacher"].balance==30 and f.state.wallets["wallet.student"].reserved.is_empty() and f.state.actors.student.fatigue==6,"中断保留实际课时且未提供资金可重新支配")
	check(f.state.actions.is_empty() and f.state.actors.student.action_id.is_empty() and f.state.actors.teacher.action_id.is_empty(),"岗位失效仍释放双方")

## 明确参数、来源和资产边界失败均只读；课程不接受自报倍率或挪用公共钱包。
func failures_are_readonly() -> void:
	for mode in ["funds","authority","absent","busy","course","ticks","multiplier"]:
		var f: Dictionary=fixture()
		match mode:
			"funds": f.state.wallets["wallet.student"].reserved={"other":999}
			"authority": f.state.wallets["wallet.teacher"].owner={"kind":"actor","id":"student"}
			"absent": f.state.actors.teacher.location.place_id="elsewhere"
			"busy": f.state.actors.teacher.action_id="another"
			"course": f.data.roles["role.school"].skill_courses=[]
			"ticks": f.params.ticks=1
			"multiplier": f.params.performance_bp=12000
		var before: Dictionary=f.state.duplicate(true)
		check(not Study.preview(f.state,f.data,"student",f.params).ok and before==f.state,"失败无副作用 "+mode)

## 零课时失效释放全额；收款临时溢出也正常结束，重复推进或完成不再支付。
func unprovided_payment() -> void:
	var f: Dictionary=fixture(); var action: Dictionary=begin(f)
	if action.is_empty(): return
	f.state.actors.teacher.available=false
	var result: Dictionary=advance(f,action)
	check(not result.outcome.success and result.outcome.fee_paid==0 and result.outcome.unprovided_released==60 and f.state.wallets["wallet.student"].balance==1000 and f.state.wallets["wallet.teacher"].balance==0 and f.state.wallets["wallet.student"].reserved.is_empty(),"零课时失效全额解预留，双方资产守恒")
	var before: Dictionary=f.state.duplicate(true)
	check(Study.finish(f.state,f.data,action,f.state.tick)==result.outcome and f.state==before,"同次失效重复完成不重复支付或释放")
	f=fixture(); action=begin(f)
	if action.is_empty(): return
	f.state.wallets["wallet.teacher"].balance=2147483647
	result=advance(f,action)
	check(not result.outcome.success and result.outcome.code=="StudyPaymentBlocked" and f.state.wallets["wallet.student"].balance==1000 and f.state.wallets["wallet.student"].reserved.is_empty(),"收款能力变化不扣学生资金、不永久卡住世界")
	f=fixture(); action=begin(f)
	if action.is_empty(): return
	f.state.wallets["wallet.student"].reserved[action.id]=1
	check(Study.validate_state(f.state,f.data).get("code")=="StudyReservationMismatch","保存不能减掉课程资金保障仍保留有效课程")

## 支付前的陈旧报价不改变资产；保存中的条款与进度不能任意放宽课程上限。
func terms_integrity() -> void:
	var f: Dictionary=fixture()
	var old: Dictionary=Study.preview(f.state,f.data,"student",f.params)
	f.state.actors.student.skills.rites+=1
	var money: Dictionary=f.state.wallets.duplicate(true)
	var rejected: Dictionary=Runtime.begin(f.state,f.data,"student",Study.KIND,f.params,old,f.registry)
	check(rejected.get("code")=="StaleStudyQuote" and f.state.wallets==money and f.state.facts.is_empty(),"陈旧私有报价支付前拒绝，外层丢弃占用候选")
	f=fixture(); var action: Dictionary=begin(f)
	if action.is_empty(): return
	for field in ["fee","xp_limit"]:
		var forged: Dictionary=f.state.duplicate(true)
		forged.facts[action.id].terms[field]+=1
		forged.actions[action.id].params.study[field]+=1
		check(not Study.validate_state(forged,f.data).ok,"存档同改两处不能伪造课程条款 "+field)
	var bad: Dictionary=f.state.duplicate(true)
	bad.wallets["wallet.student"].owner={"kind":"organization","id":"public"}
	check(not Study.validate_state(bad,f.data).ok,"课程付款事实不能把学生钱包换成公款")

## 仅执行本课程新机制和失败边界，不重复武学或整世界长套件。
func run_checks() -> void:
	full_course(); limits(); interruption(); failures_are_readonly(); terms_integrity(); unprovided_payment()
	print("SKILL_STUDY_CHECKS=%d FAILURES=%d"%[checks,failures])
	quit(0 if failures==0 else 1)
