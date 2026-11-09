## 军阵适配局部门验证一次抽种、首课前冻结及存读边界；实际unit/费用由另一个Training owner集成门覆盖。
extends SceneTree
const Adapter=preload("res://core/domains/military/training_drill.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0
var definition: Dictionary
var texts: Dictionary
const ACTOR="actor.teacher"
const ACTION="action:14"

## 复用正式纯叶小题面与严格JSON解析，不准备大工程或假造实际训练收据。
func _init() -> void:
	definition=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/drill_challenge/definition.json")).value
	texts=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/drill_challenge/texts.json")).value
	call_deferred("run_checks")

## 每个断言保留独立标签，任何失败退出非零；引擎错误另从输出核验。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 本fixture只提供adapter需要的真实形状；军务授权、单位账和费用不在此伪装成全Kernel验证。
func fixture(mode: String="manual", offered: bool=true) -> Dictionary:
	var random=RandomNumberGenerator.new(); random.seed=91
	var state: Dictionary={"actors":{ACTOR:{"id":ACTOR,"skills":{"tactics":300,"infantry":1200},"attributes":{"intellect":60}}},"rng":{"minigame":{"seed":str(random.seed),"state":str(random.state)},"combat":{"seed":"21","state":"22"}},"wallets":{"unchanged":150},"military_units":{"unchanged":40}}
	var data: Dictionary={"texts":texts.duplicate(true),"drill_challenges":{definition.id:definition.duplicate(true)},"garrisons":{"camp":{"training_drill_id":definition.id} if offered else {}}}
	var terms: Dictionary={"actor_id":ACTOR,"unit_id":"unit:1","troop_type_id":"troop.infantry","fit_ids":["soldier:1","soldier:2"],"garrison_id":"camp"}
	var quoted: Dictionary=Adapter.quote(data,terms,state.actors[ACTOR],mode)
	if quoted.has("drill"): terms.drill=quoted.drill
	var fact: Dictionary={"kind":"military_training","actor_id":ACTOR,"phase":"started","start_tick":10,"completed_tick":-1,"success":false,"code":"","terms":terms,"lessons":[]}
	return {"data":data,"state":state,"fact":fact,"action":{"id":ACTION,"actor_id":ACTOR,"start_tick":10},"quoted":quoted}

## 真实start入口只修改同一fact实例与minigame流，未修改任何钱包/单位数组。
func started(mode: String="manual", offered: bool=true) -> Dictionary:
	var f: Dictionary=fixture(mode,offered)
	check(f.quoted.ok,"题面可报价")
	check(Adapter.start(f.state,f.data,f.action,f.fact).ok,"同action成功开始")
	check(Adapter.validate(f.data,f.fact,ACTION).ok,"开始后稳定状态合法")
	return f

## owner只写回已验证下一实例，所有移动先草拟再确认，未读调用者自报分数。
func move(f: Dictionary, choice: Dictionary) -> void:
	var step: int=f.fact.drill.challenge.moves.size()+1
	var drafted: Dictionary=Adapter.transition(f.data,f.fact,ACTION,ACTOR,Adapter.DRAFT_KIND,{"action_id":ACTION,"expected_step":step,"group_id":choice.group_id,"target":choice.target})
	if not check(drafted.ok,"真实draft接口合法"): return
	f.fact.drill.challenge=drafted.state
	var confirmed: Dictionary=Adapter.transition(f.data,f.fact,ACTION,ACTOR,Adapter.CONFIRM_KIND,{"action_id":ACTION,"expected_step":step,"expected_pending":drafted.state.pending})
	if check(confirmed.ok,"真实confirm接口合法"): f.fact.drill.challenge=confirmed.state

## 输入与开始失败不得消耗随机或留半个实例，冻结本人等级来自兵法而不是兵种训练技能。
func inputs() -> void:
	var f: Dictionary=fixture(); var original: Dictionary=f.state.duplicate(true)
	check(f.fact.terms.drill.actor_snapshot=={"skill_level":2,"auxiliary_attribute":60},"tactics/intellect唯一来源")
	check(f.fact.terms.drill.group_source.group_ids.size()==3 and f.fact.terms.drill.group_source.kind=="abstract_training","三个题面组明确抽象来源")
	check(f.fact.terms.drill.group_source.roster_sha256==JSON.stringify(f.fact.terms.fit_ids).sha256_text(),"固定真实唯一名册摘要")
	f.state.actors[ACTOR].skills.tactics=650
	var before: Dictionary=f.state.duplicate(true)
	check(Adapter.start(f.state,f.data,f.action,f.fact).get("code")=="TrainingDrillSnapshotChanged" and f.state==before and not f.fact.has("drill"),"报价后本人技能变更拒绝且随机无变化")
	f=fixture(); f.state.actors[ACTOR].attributes.intellect=60.0
	check(not Adapter.quote(f.data,f.fact.terms,f.state.actors[ACTOR]).ok,"浮点辅助属性拒绝")
	f=fixture(); f.fact.terms.fit_ids.reverse()
	check(not Adapter.quote(f.data,f.fact.terms,f.state.actors[ACTOR]).ok,"真实名册必须唯一有序")
	f=fixture(); f.data.garrisons.camp.training_drill_id="missing"
	check(Adapter.start(f.state,f.data,f.action,f.fact).get("code")=="MissingTrainingDrill","缺题面关闭失败")
	f=fixture(); f.state.rng.minigame.seed="091"; before=f.state.duplicate(true)
	check(not Adapter.start(f.state,f.data,f.action,f.fact).ok and f.state==before and not f.fact.has("drill"),"非规范随机源拒绝且无部分成功")
	f=fixture(); check(Adapter.start(f.state,f.data,f.action,f.fact).ok,"开始抽种")
	check(f.state.rng.minigame!=original.rng.minigame and f.state.rng.combat==original.rng.combat,"只推进minigame一次")
	check(f.state.wallets==original.wallets and f.state.military_units==original.military_units,"没有第二钱包或单位写口")
	before=f.state.duplicate(true); var saved: Dictionary=f.fact.duplicate(true)
	check(Adapter.start(f.state,f.data,f.action,f.fact).get("code")=="TrainingDrillAlreadyStarted" and before==f.state and saved==f.fact,"重复开始不重抽和覆盖")
	check(not Adapter.quote(f.data,f.fact.terms,f.state.actors[ACTOR],"latest").ok,"模式白名单")
	f=started("automatic",false); check(not f.fact.has("drill") and f.state.rng==original.rng,"旧无题面自动课不添字段/随机")
	check(Adapter.freeze_before_lesson(f.data,f.fact,ACTION,11).performance_bp==10000,"旧自动课系数保持10000")
	check(Adapter.quote(f.data,f.fact.terms,f.state.actors[ACTOR],"manual").get("code")=="TrainingDrillNotOffered","未提供题面不能手动")

## 首课前手动完成但未消费；冻结与实际课时同事务，随后保存、后续课和查询只读同一结果。
func first_lesson() -> void:
	var f: Dictionary=started()
	for choice in definition.solution: move(f,choice)
	check(f.fact.drill.phase=="pending" and f.fact.drill.performance_bp==null and f.fact.drill.challenge.result.score==100,"手动终题并非已提供训练")
	var saved: Dictionary=f.fact.duplicate(true)
	check(not Adapter.freeze_before_lesson(f.data,f.fact,ACTION,12).ok and saved==f.fact,"跳过首课tick拒绝且不改事实")
	var candidate: Dictionary=f.fact.duplicate(true)
	check(Adapter.freeze_before_lesson(f.data,candidate,ACTION,11).performance_bp==12000 and f.fact==saved,"候选失败可丢弃，原事实未消费")
	check(not Adapter.validate(f.data,candidate,ACTION).ok,"只冻结未写首课的中间状态不能保存")
	var frozen: Dictionary=Adapter.freeze_before_lesson(f.data,f.fact,ACTION,11)
	check(frozen.ok and frozen.performance_bp==12000,"首个实际课时只冻结一次12000")
	f.fact.lessons.append({"tick":11})
	check(Adapter.validate(f.data,f.fact,ACTION).ok,"首课实际记录后稳定状态合法，行细节交训练validator")
	check(Adapter.transition(f.data,f.fact,ACTION,ACTOR,Adapter.EXIT_KIND,{"action_id":ACTION}).get("code")=="TrainingDrillAlreadyFrozen","首课后不能退出改成自动或重开")
	var loaded: Dictionary=Strict.parse_text(JSON.stringify(f.fact)).value
	check(Adapter.validate(f.data,loaded,ACTION).ok,"严格JSON存读保存结果和一次抽种来源")
	check(Adapter.freeze_before_lesson(f.data,loaded,ACTION,12).performance_bp==12000 and loaded==f.fact,"后续课同系数且不追加随机/改实例")
	f.state.actors[ACTOR].skills.tactics=1200
	check(Adapter.validate(f.data,f.fact,ACTION).ok,"后续人物成长不重算冻结成绩")
	var shown: Dictionary=Adapter.view(f.data,f.fact,ACTION,ACTOR)
	shown.group_source.group_ids.clear()
	check(f.fact.terms.drill.group_source.group_ids.size()==3,"只读来源投影深拷贝")
	f.fact.phase="completed"; f.fact.completed_tick=11; f.fact.code="MilitaryTrainingCancelled"
	check(Adapter.close_unprovided(f.data,f.fact,ACTION).ok and f.fact.drill.phase=="frozen","部分课取消保留已固定系数，训练owner只保已提供差额")

## 未答完/主动退出使用相同自动基线；零课终止封闭而不消费，不能以取消当一次训练。
func fallback_and_close() -> void:
	var f: Dictionary=started(); move(f,definition.solution[0])
	check(Adapter.freeze_before_lesson(f.data,f.fact,ACTION,11).performance_bp==10000 and f.fact.drill.challenge.result.completion=="exited","首课前未答完按自动基线")
	f.fact.lessons.append({"tick":11})
	check(Adapter.validate(f.data,f.fact,ACTION).ok,"自动关闭仍保留已确认步存读")
	f=started("automatic")
	check(f.fact.drill.challenge.phase=="completed" and f.fact.drill.phase=="pending","自动题面也在真实首课前未消费")
	check(Adapter.freeze_before_lesson(f.data,f.fact,ACTION,11).performance_bp==10000,"自动成绩不损原基线")
	f=started(); move(f,definition.solution[0])
	check(Adapter.close_unprovided(f.data,f.fact,ACTION).ok and f.fact.drill.phase=="closed" and f.fact.drill.performance_bp==null,"零课取消只关闭实例")
	check(not Adapter.validate(f.data,f.fact,ACTION).ok,"未同步结束原行动的中间状态不可存")
	f.fact.phase="completed"; f.fact.code="MilitaryTrainingCancelled"; f.fact.completed_tick=10
	check(Adapter.validate(f.data,f.fact,ACTION).ok and Adapter.close_unprovided(f.data,f.fact,ACTION).ok,"完整零课取消可存并幂等")
	check(not Adapter.freeze_before_lesson(f.data,f.fact,ACTION,11).ok and not Adapter.transition(f.data,f.fact,ACTION,ACTOR,Adapter.EXIT_KIND,{"action_id":ACTION}).ok,"零课终止后不可重新答题/领取系数")

## 对保存内容只改一个来源或消费事实即拒绝，不借宽松数值相等绕过真实类型。
func tampering() -> void:
	var f: Dictionary=started(); var bad: Dictionary=f.fact.duplicate(true)
	bad.terms.drill.group_source.roster_sha256="0".repeat(64)
	check(not Adapter.validate(f.data,bad,ACTION).ok,"伪名册摘要拒绝")
	bad=f.fact.duplicate(true); bad.terms.fit_ids.append("soldier:3")
	check(not Adapter.validate(f.data,bad,ACTION).ok,"另一个真实名册不能套旧组来源")
	bad=f.fact.duplicate(true); bad.drill.challenge.seed="1"
	check(not Adapter.validate(f.data,bad,ACTION).ok,"伪seed拒绝")
	bad=f.fact.duplicate(true); bad.drill.erase("performance_bp"); bad.drill.extra=null
	check(not Adapter.validate(f.data,bad,ACTION).ok,"必填null字段不能用未知null冒充")
	bad=f.fact.duplicate(true); bad.drill.challenge.binding.source_id="action:15"
	check(not Adapter.validate(f.data,bad,ACTION).ok,"另一action不能用同题面")
	bad=f.fact.duplicate(true); bad.erase("drill")
	check(not Adapter.validate(f.data,bad,ACTION).ok,"删实例不能重开")
	bad=f.fact.duplicate(true); bad.terms.erase("drill"); bad.erase("drill")
	check(not Adapter.validate(f.data,bad,ACTION).ok,"删全部新字段仍和声明提供题面冲突")
	bad=f.fact.duplicate(true); bad.drill.phase="frozen"; bad.drill.performance_bp=12000; bad.drill.frozen_tick=11
	check(not Adapter.validate(f.data,bad,ACTION).ok,"未提供课时不能伪消费")
	var changed: Dictionary=f.data.duplicate(true); changed.texts[definition.prompt_key]+="changed"
	check(Adapter.validate(changed,f.fact,ACTION).get("code")=="TrainingDrillSourceChanged","题面文本变化拒绝原快照")
	check(not Adapter.transition(f.data,f.fact,ACTION,"actor.other",Adapter.EXIT_KIND,{"action_id":ACTION}).ok,"不能代别人关闭")
	check(not Adapter.transition(f.data,f.fact,ACTION,ACTOR,Adapter.EXIT_KIND,{"action_id":"action:15"}).ok,"旧action请求拒绝")
	check(not Adapter.transition(f.data,f.fact,ACTION,ACTOR,Adapter.EXIT_KIND,{"action_id":ACTION,"performance_bp":12000}).ok,"拒调用者自报系数")
	check(not Adapter.transition(f.data,f.fact,ACTION,ACTOR,"eval",{"action_id":ACTION}).ok,"固定四个入口无脚本")
	check(Adapter.freeze_before_lesson(f.data,f.fact,ACTION,11).ok,"为终态篡改准备正式冻结")
	f.fact.lessons.append({"tick":11})
	bad=f.fact.duplicate(true); bad.drill.performance_bp=12000
	check(not Adapter.validate(f.data,bad,ACTION).ok,"自动结果不能改12000")
	bad=f.fact.duplicate(true); bad.drill.frozen_tick=12
	check(not Adapter.validate(f.data,bad,ACTION).ok,"系数只允许首实际课冻结")

## 只核双方已约定整数顺序与上限，不实现或调用第二个训练结算器。
func rounding_contract() -> void:
	for gain in range(3,8):
		var previous: int=0
		for n in range(1,5):
			@warning_ignore("integer_division")
			var normal: int=gain*n/4
			@warning_ignore("integer_division")
			var adjusted: int=normal*12000/10000
			check(adjusted>=previous and adjusted*10000<=normal*12000,"累计先正常取整再倍率，gain=%s n=%s"%[gain,n])
			previous=adjusted

## 仅新适配模块执行有界正反例，实际军务课程全Kernel集成由共享owner另验。
func run_checks() -> void:
	inputs(); first_lesson(); fallback_and_close(); tampering(); rounding_contract()
	print("TRAINING_DRILL_CHECKS=%d FAILURES=%d"%[checks,failures])
	quit(1 if failures else 0)
