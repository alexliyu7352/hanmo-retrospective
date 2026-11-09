## 议价课程只复用原学习长行动和唯一结算；检查显式类型、练习上下文及旧礼议形状，不假称交易已接。
extends "res://tests/test_skill_study.gd"
const Adapter=preload("res://core/domains/cultivation/study_etiquette.gd")

## 在原课程夹具增加正式商术经验及公开练习题面，不构造商铺、真实折扣或第二钱包账。
func bargain_fixture(mode: String="manual", skill_id: String="commerce") -> Dictionary:
	var f: Dictionary=fixture()
	for id in ["student","teacher"]:
		f.state.actors[id].skills.commerce=80 if id=="student" else 650
		f.state.actors[id].skills.negotiation=80 if id=="student" else 650
		f.state.actors[id].attributes={"administration":60,"charisma":55,"intellect":50}
	var definition: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/bargain_challenge/definition.json")).value
	f.data.bargain_challenges={definition.id:definition}
	f.data.texts=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/bargain_challenge/texts.json")).value
	f.data.roles["role.school"].skill_courses=[{"skill_id":skill_id,"mode":"teacher","wallet_id":"wallet.teacher","challenge":{"kind":"bargain","id":definition.id,"context":{"budget":650,"quantity":10,"relation_discount_bp":300}}}]
	f.params.skill_id=skill_id; f.params.challenge_mode=mode
	var random=RandomNumberGenerator.new(); random.seed=7351
	f.state.rng={"minigame":{"seed":str(random.seed),"state":str(random.state)}}
	return f

## 新入口只用当前step_id和原action_id，提交后仍由课程事实验证唯一实例。
func choose_bargain(f: Dictionary, action: Dictionary, step_id: String, option_id: String) -> Dictionary:
	var params: Dictionary={"action_id":action.id,"step_id":step_id,"option_id":option_id}
	var offer: Dictionary=Study.preview_challenge(f.state,f.data,"student",Adapter.GENERIC_CHOOSE_KIND,params)
	if not check(offer.ok,"通用课程入口合法报价 "+str(offer)): return offer
	var applied: Dictionary=Study.apply_challenge(f.state,f.data,"student",Adapter.GENERIC_CHOOSE_KIND,params,offer)
	check(applied.ok and not applied.details.price_applied and Study.validate_state(f.state,f.data).ok,"同一课程实例提交选择合法且不改价格 "+str(applied))
	return applied

## 议价完整课程存读同态，只有原finish写一次商术XP，题面折扣从不降低实际课费。
func course_lifecycle() -> void:
	var f: Dictionary=bargain_fixture()
	var source: Dictionary=f.state.duplicate(true)
	var query: Dictionary=Study.courses(f.state,f.data,"student",f.params.context)
	check(query.ok and query.courses[0].offer.challenge.kind=="bargain" and query.courses[0].offer.challenge.context.quote==null and f.state==source,"课程查询明确练习种类与quote=null且不抽种")
	var action: Dictionary=begin(f)
	if action.is_empty(): return
	var fact: Dictionary=f.state.facts[action.id]
	check(fact.has("challenge") and not fact.has("etiquette") and fact.terms.challenge.kind=="bargain" and fact.terms.challenge.actor_snapshot=={"skill_level":0,"auxiliary_attribute":60},"新课程显式kind并冻结商术/政务真实快照")
	check(fact.challenge.challenge.context=={"budget":650,"quantity":10,"relation_discount_bp":300,"quote":null} and fact.challenge.challenge.binding.source_id==action.id,"作者练习context由owner补固定null并绑定原行动")
	var stream: Dictionary=f.state.rng.duplicate(true)
	var view: Dictionary=Study.challenge_view(f.state,f.data,"student",action.id)
	check(view.ok and view.challenge_kind=="bargain" and not view.price_applied and view.round==1,"通用只读入口不把议价练习冒充已改价")
	check(not Study.etiquette_view(f.state,f.data,"student",action.id).ok,"旧礼议查询不会把议价当旧节点")
	check(Study.preview_etiquette(f.state,f.data,"student",Adapter.CHOOSE_KIND,{"action_id":action.id,"node_id":"round.1","option_id":"small"}).get("code")=="UnsupportedAction","旧专用命令不能操作议价实例")
	choose_bargain(f,action,"round.1","small")
	var decoded: Dictionary=Strict.parse_text(JSON.stringify(f.state))
	check(decoded.ok and decoded.value==f.state and Study.validate_state(decoded.value,f.data).ok,"新实例第一轮后StrictJSON可读")
	var twin: Dictionary=f.duplicate(true); twin.state=decoded.value
	for id in ["round.2","round.3"]:
		choose_bargain(f,action,id,"small"); choose_bargain(twin,twin.state.actions[action.id],id,"small")
	check(f.state==twin.state and f.state.rng==stream and fact.challenge.challenge.result.discount_bp==800 and f.state.actors.student.skills.commerce==80,"读回同选择同seed，800仅候选且答题不授XP")
	advance(f,action); advance(twin,twin.state.actions[action.id])
	var outcome: Dictionary=advance(f,action); advance(twin,twin.state.actions[action.id])
	check(f.state==twin.state and outcome.outcome.xp_gained==48 and f.state.actors.student.skills.commerce==128,"原唯一课程finish授48XP且存读同态")
	check(outcome.outcome.fee_paid==60 and f.state.wallets["wallet.teacher"].balance==60 and not outcome.outcome.challenge.price_applied and outcome.outcome.challenge.kind=="bargain","练习折扣不改变真实课程60费或任何交易")
	var before: Dictionary=f.state.duplicate(true)
	check(fact.challenge.consumed and Study.finish(f.state,f.data,action,f.state.tick)==outcome.outcome and f.state==before,"完成消费只一次，重试不发XP或重付课程")

## 自动、退出和取消保持原基线/T13；种类与作者练习上下文变化不能继续原实例。
func boundaries() -> void:
	var f: Dictionary=bargain_fixture("automatic","negotiation")
	var quoted: Dictionary=Study.preview(f.state,f.data,"student",f.params)
	check(quoted.ok and quoted.study.challenge.actor_snapshot.auxiliary_attribute==55,"交涉使用本人魅力，不套商术政务")
	var action: Dictionary=begin(f)
	if not action.is_empty():
		advance(f,action); var result: Dictionary=advance(f,action)
		check(result.outcome.xp_gained==40 and result.outcome.challenge.result.performance_bp==10000,"自动课程保正常40XP")
	f=bargain_fixture(); action=begin(f)
	if action.is_empty(): return
	choose_bargain(f,action,"round.1","small")
	var exit_params: Dictionary={"action_id":action.id}
	var exit_offer: Dictionary=Study.preview_challenge(f.state,f.data,"student",Adapter.GENERIC_EXIT_KIND,exit_params)
	check(exit_offer.ok and Study.apply_challenge(f.state,f.data,"student",Adapter.GENERIC_EXIT_KIND,exit_params,exit_offer).ok,"新通用退出复用同一实例自动基线")
	advance(f,action)
	var cancelled: Dictionary=Study.cancel(f.state,f.data,"student",action.id)
	Step.release(f.state,action)
	check(cancelled.ok and cancelled.details.xp_gained==20 and f.state.actors.student.skills.commerce==100 and cancelled.details.fee_paid==30 and cancelled.details.unprovided_released==30 and f.state.facts[action.id].challenge.consumed and Study.validate_state(f.state,f.data).ok,"取消保留原退出基线的一课20XP，不计未提供课时")
	f=bargain_fixture(); action=begin(f)
	if action.is_empty(): return
	var before: Dictionary=f.state.duplicate(true)
	var course: Dictionary=f.data.roles["role.school"].skill_courses[0]
	course.challenge.context.quantity=11
	check(not Study.preview_challenge(f.state,f.data,"student",Adapter.GENERIC_EXIT_KIND,{"action_id":action.id}).ok and f.state==before,"作者练习数量外变使原source上下文拒绝")
	course.challenge.context.quantity=10
	var forged: Dictionary=f.state.duplicate(true); forged.facts[action.id].challenge.challenge.context.quote={"quote_id":"quote:17","quote_sha256":"a".repeat(64),"market_revision":0,"valid_tick":0}
	check(not Study.validate_state(forged,f.data).ok,"课程存档不能夹带真实quote权限")
	forged=f.state.duplicate(true); forged.facts[action.id].terms.challenge.kind="etiquette"; forged.actions[action.id].params.study.challenge.kind="etiquette"
	check(not Study.validate_state(forged,f.data).ok,"实例种类不能从议价改为礼议重解释选择")
	forged=f.state.duplicate(true); forged.facts[action.id].etiquette={}
	check(not Study.validate_state(forged,f.data).ok,"新旧实例载荷不能并存")
	forged=f.state.duplicate(true); forged.facts[action.id].terms.etiquette={}; forged.actions[action.id].params.study.etiquette={}
	check(not Study.validate_state(forged,f.data).ok,"新旧课程条款也不能并存")
	f=bargain_fixture(); f.data.roles["role.school"].skill_courses[0].etiquette_challenge_id="legacy"
	check(not Study.preview(f.state,f.data,"student",f.params).ok,"作者课程新旧绑定互斥")
	f=bargain_fixture(); f.data.roles["role.school"].skill_courses[0].challenge.context.quote_id="quote:fake"
	check(not Study.preview(f.state,f.data,"student",f.params).ok,"作者不能在练习context注入quote_id")
	f=bargain_fixture(); f.data.roles["role.school"].skill_courses[0].challenge.kind="script"
	check(not Study.preview(f.state,f.data,"student",f.params).ok,"只识别礼议/议价两个固定种类")

## 旧礼议声明创建旧四字段条款及旧实例；JSON读回继续同一选择，不自动搬迁为新challenge。
func legacy_and_new_etiquette() -> void:
	var f: Dictionary=bargain_fixture()
	var old_definition: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/etiquette_challenge/definition.json")).value
	f.data.etiquette_challenges={old_definition.id:old_definition}
	f.data.texts=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/etiquette_challenge/texts.json")).value
	f.params.skill_id="rites"
	f.data.roles["role.school"].skill_courses=[{"skill_id":"rites","mode":"teacher","wallet_id":"wallet.teacher","etiquette_challenge_id":old_definition.id}]
	var generic: Dictionary=f.duplicate(true)
	generic.data.roles["role.school"].skill_courses[0].erase("etiquette_challenge_id")
	generic.data.roles["role.school"].skill_courses[0].challenge={"kind":"etiquette","id":old_definition.id}
	var action: Dictionary=begin(f)
	if action.is_empty(): return
	var node: Dictionary=old_definition.nodes[0]
	var params: Dictionary={"action_id":action.id,"node_id":node.id,"option_id":node.options[0].id}
	var offer: Dictionary=Study.preview_etiquette(f.state,f.data,"student",Adapter.CHOOSE_KIND,params)
	check(offer.ok and Study.apply_etiquette(f.state,f.data,"student",Adapter.CHOOSE_KIND,params,offer).ok,"旧专用API继续原node_id合同")
	var decoded: Dictionary=Strict.parse_text(JSON.stringify(f.state))
	check(decoded.ok and Study.validate_state(decoded.value,f.data).ok and decoded.value.facts[action.id].terms.etiquette.size()==4 and not decoded.value.facts[action.id].has("challenge"),"旧字段原样存读，未静默改写或丢首轮选择")
	f.state=decoded.value; action=f.state.actions[action.id]
	for index in range(1,3): choose_bargain(f,action,old_definition.nodes[index].id,old_definition.nodes[index].options[0].id)
	advance(f,action); var outcome: Dictionary=advance(f,action)
	check(outcome.outcome.etiquette.score==100 and outcome.outcome.xp_gained==48 and not outcome.outcome.has("challenge"),"旧礼议回执与唯一奖励保持兼容")
	action=begin(generic)
	if action.is_empty(): return
	check(generic.state.facts[action.id].terms.challenge.kind=="etiquette" and generic.state.facts[action.id].terms.challenge.context==null,"新礼议显式类型使用新字段且不强塞议价上下文")
	for round_row in old_definition.nodes: choose_bargain(generic,action,round_row.id,round_row.options[0].id)
	advance(generic,action); outcome=advance(generic,action)
	check(outcome.outcome.challenge.kind=="etiquette" and outcome.outcome.xp_gained==48,"新旧礼议都经同一实例和结课路径")

## 只跑两个题面类型共用实例的新增接缝，旧114与纯叶55/50不整体重跑。
func run_checks() -> void:
	course_lifecycle(); boundaries(); legacy_and_new_etiquette()
	print("STUDY_BARGAIN_CHECKS=%d FAILURES=%d"%[checks,failures])
	quit(0 if failures==0 else 1)
