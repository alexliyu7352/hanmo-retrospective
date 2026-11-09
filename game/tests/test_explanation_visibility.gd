## 只读解释的可见性回归；使用真实社会传播与命令收据，负例阻止秘密和未知道路外泄。
extends SceneTree
const Explain=preload("res://core/runtime/explanation_query.gd")
const Kernel=preload("res://core/world_kernel.gd")
const F=preload("res://tests/companion_fixture.gd")
const Social=preload("res://core/social_rules.gd")
const Codec=preload("res://core/save_codec.gd")
var checks: int=0
var failures: int=0

## 引擎就绪后执行局部合同，不启动图形或全量模拟。
func _init() -> void:
	call_deferred("run_checks")

## 每个失败有实际标签与非零退出码，不能用空断言报告通过。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 定向查询只取指定事实，观察者为空时由接口使用当前玩家。
func fact(state: Dictionary, data: Dictionary, fact_id: String, observer: String="") -> Dictionary:
	return Explain.explain(state,data,{"kind":"fact","fact_id":fact_id},observer)

## 建立现有作者世界，让NPC通过真正的四次半日发生夺粮与次日转述。
func social_world():
	var kernel=Kernel.new()
	var opened: Dictionary=kernel.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json")
	check(opened.ok,"现有社会剧本可开启："+str(opened))
	if not opened.ok: return null
	for _tick in range(4):
		check(F.send(kernel,"wait").ok and kernel.advance_one_tick().ok,"共享半日实际生成社会事实")
	return kernel

## 亲历、听闻与未知必须来自现存记忆；同城、事实存在和反复打开不能补知识。
func check_social_visibility(kernel, state: Dictionary, data: Dictionary, id: String) -> void:
	var witness: Dictionary=fact(state,data,id,"actor.shen")
	var heard: Dictionary=fact(state,data,id,"actor.carter")
	var unknown: Dictionary=fact(state,data,id)
	check(witness.known_causes.size()==1 and heard.known_causes.size()==1,"亲历者与真实听闻者均有已有证据")
	if witness.known_causes.size()==1 and heard.known_causes.size()==1:
		check(witness.known_causes[0].knowledge=="witnessed" and heard.known_causes[0].knowledge=="reported","不把转述冒充亲历")
		check(heard.known_causes[0].source_actor_id==state.social.memories["actor.carter"][id].source_actor_id,"来源来自实际第一次转述")
		check(witness.known_causes[0].fact.kind=="theft" and witness.known_causes[0].fact.quantity==1,"解释引用同一真实物资事件")
	check(unknown.known_causes.is_empty() and not unknown.unknown_parts.is_empty() and unknown.next_options.is_empty(),"远方未知者既无事实也无泄密入口")
	check(unknown==fact(state,data,"social:nonexistent"),"隐藏事实与不存在事实不能通过返回形状区分")
	check(unknown.revision==state.revision,"查询绑定当前已提交版本")
	var before: Dictionary=state.duplicate(true)
	var content_before: Dictionary=data.duplicate(true)
	for _repeat in range(100):
		for observer in ["actor.shen","actor.carter",state.player_actor_id]:
			fact(state,data,id,observer)
		Explain.explain(state,data,{"kind":"world"})
	check(state==before and data==content_before and kernel.snapshot()==before,"重复100轮查询不改世界、RNG、ID、知识或内容")
	if not witness.known_causes.is_empty():
		witness.known_causes[0].fact.quantity=999
		witness.unknown_parts.append({"code":"changed"})
		witness.next_options.append({"query":{"kind":"world"}})
	check(state==before and fact(state,data,id,"actor.shen")!=witness,"修改解释所有容器均不写回世界或后续解释")
	var without_memory: Dictionary=state.duplicate(true)
	without_memory.social.memories["actor.shen"].erase(id)
	check(fact(without_memory,data,id,"actor.shen").known_causes.is_empty(),"参与者字段不能替代尚未存下的记忆")
	var forged: Dictionary=state.duplicate(true)
	forged.social.memories[state.player_actor_id]={id:{"learned_tick":state.tick,"source_actor_id":"actor.shen"}}
	check(fact(forged,data,id).known_causes.is_empty(),"没有实际转述记录的来源不能当听闻权限")
	for mutation in ["future","same_tick","missing_source","source_not_witness"]:
		var broken: Dictionary=state.duplicate(true)
		var memory: Dictionary=broken.social.memories["actor.carter"][id]
		var source: String=memory.source_actor_id
		match mutation:
			"future": memory.learned_tick=state.tick+1
			"same_tick": broken.social.memories[source][id].learned_tick=memory.learned_tick
			"missing_source": broken.social.memories[source].erase(id)
			"source_not_witness":
				broken.social.events[id].actor_id=state.player_actor_id
				broken.social.events[id].target_actor_id="actor.chen"
		check(fact(broken,data,id,"actor.carter").known_causes.is_empty(),"坏来源链不能扩大既有知识："+mutation)

## 本人收据只展示命令回执字段；advance的世界聚合details不是观察者全知许可。
func check_commands(state: Dictionary, data: Dictionary) -> void:
	var actor: String=state.player_actor_id
	var seq: int=state.request_sequences[actor]
	var receipt: Dictionary=state.results[actor+":"+str(seq)]
	var query: Dictionary={"kind":"command","request_seq":seq}
	var own: Dictionary=Explain.explain(state,data,query)
	check(own.known_causes.size()==1,"本人实际提交的命令可解释")
	if own.known_causes.size()==1:
		check(own.known_causes[0].command.kind==receipt.kind and own.known_causes[0].command.result_id==receipt.result_id,"使用本人缓存结果而非伪造命令内容")
		check(not own.known_causes[0].command.has("details"),"本人advance不能读全世界阶段details")
		check(own.next_options==[{"kind":"query","query":{"kind":"result","result_id":receipt.result_id}}],"下一步仅指向本人已知结果的只读查询")
	var visible: Dictionary=Explain.explain(state,data,{"kind":"result","result_id":receipt.result_id})
	check(visible.known_causes.size()==1,"已知本人结果可按result_id查询")
	var foreign: Dictionary=Explain.explain(state,data,{"kind":"result","result_id":receipt.result_id},"actor.carter")
	check(foreign.known_causes.is_empty() and foreign==Explain.explain(state,data,{"kind":"result","result_id":"result:nonexistent"},"actor.carter"),"别人的命令结果与不存在结果同样不可见")
	var evicted: Dictionary=state.duplicate(true)
	evicted.results.erase(actor+":"+str(seq))
	check(Explain.explain(evicted,data,query).known_causes.is_empty(),"缓存淘汰后不猜测request_seq和history的映射")
	check(Explain.explain(evicted,data,{"kind":"result","result_id":receipt.result_id}).known_causes.size()==1,"本人history仍可提供已存结果的最小命令证据")
	var before: Dictionary=state.duplicate(true)
	for _repeat in range(100):
		Explain.explain(state,data,query)
		Explain.explain(state,data,{"kind":"result","result_id":receipt.result_id})
		Explain.explain(state,data,{"kind":"result","result_id":receipt.result_id},"actor.carter")
	check(state==before,"反复查看本人命令与隐藏结果同样不改变状态和随机流")

## 已知任务来源来自正式阅读告示；解释不读取全内容的未知任务或擅自发现路线。
func check_offer() -> void:
	var kernel=Kernel.new()
	var opened: Dictionary=kernel.new_game("scenario.two_stories","res://data/generated/two_stories.hmcatalog.json")
	check(opened.ok,"任务知识回归使用原剧本")
	if not opened.ok: return
	var query: Dictionary={"kind":"offer","offer_id":"offer.grain"}
	check(Explain.explain(kernel.snapshot(),kernel.content().data,query).known_causes.is_empty(),"作者内容存在不等于玩家知道委托")
	check(F.send(kernel,"move_local",{"place_id":"west.office"}).ok and F.send(kernel,"inspect",{"prop_id":"notice.grain"}).ok,"真实到场阅读取得原知识记录")
	var state: Dictionary=kernel.snapshot()
	var known: Dictionary=Explain.explain(state,kernel.content().data,query)
	check(known.known_causes.size()==1,"已知委托提供来源与当前本人任务状态")
	if known.known_causes.size()==1:
		check(known.known_causes[0].knowledge.source==state.knowledge[state.player_actor_id]["offer.grain"].source and known.known_causes[0].status=="known","沿用真实来源和条件规则状态")
		known.known_causes[0].knowledge.source.id="changed"
	check(state==kernel.snapshot(),"修改嵌套任务来源不改原知识")
	check(Explain.explain(state,kernel.content().data,query,"actor.carter").known_causes.is_empty(),"他人未读告示不能继承玩家知识")
	var before: Dictionary=state.duplicate(true)
	for _repeat in range(100): Explain.explain(state,kernel.content().data,query)
	check(state==before and kernel.snapshot()==before,"委托解释100次不受领任务、生成经费或改变知识")

## 真实开始和完成教学；事实存在或本人付费仍不能让未完成修习冒充亲历完成。
func check_training() -> void:
	var kernel=Kernel.new()
	var opened: Dictionary=kernel.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json")
	check(opened.ok,"教学可见性使用原有完整剧本")
	if not opened.ok: return
	check(F.send(kernel,"move_local",{"place_id":"west.training"}).ok and F.send(kernel,"talk",{"role_id":"role.wei"}).ok,"真实到场与原武师交谈")
	check(F.send(kernel,"dialogue_next").ok and F.send(kernel,"dialogue_choose",{"option_id":"learn0"}).ok and F.send(kernel,"dialogue_confirm").ok,"正式对白提交教学而非夹具捏造完成记录")
	var state: Dictionary=kernel.snapshot()
	var player: String=state.player_actor_id
	var id: String=state.actors[player].action_id
	check(not id.is_empty() and state.facts.get(id,{}).get("phase")=="started","教学已产生原领域开始事实")
	check(fact(state,kernel.content().data,id).known_causes.is_empty(),"开始但未完成不生成社会完成见闻")
	check(kernel.advance_one_tick().ok and kernel.advance_one_tick().ok,"原世界推进完成教学并记录双方见闻")
	state=kernel.snapshot()
	var learned: Dictionary=fact(state,kernel.content().data,id)
	check(learned.known_causes.size()==1 and fact(state,kernel.content().data,id,"actor.wei").known_causes.size()==1,"师生双方只有完成后的已有记忆可见")
	if learned.known_causes.size()==1:
		var summary: Dictionary=learned.known_causes[0].fact
		check(summary.training.skill_id=="skill.cleave" and summary.success and summary.phase=="completed","教学摘要仍引用实际完成与所学招式")
		check(not summary.has("context") and not summary.training.has("payer_wallet_id") and not summary.training.has("mastery_after"),"教学摘要不展开原收费、角色授权和数值条款")
		var before: Dictionary=state.duplicate(true)
		summary.training.student_id="actor.forged"
		check(state==before and state==kernel.snapshot(),"修改教学嵌套摘要不能改变原事实或知识")
	check(fact(state,kernel.content().data,id,"actor.carter").known_causes.is_empty(),"授课完成不会令远方第三人全知")

## 领域事实仅允许已有可传播类型；军令、道路与任意嵌套来源不向解释传递。
func check_secret_boundaries(state: Dictionary, data: Dictionary, social_id: String) -> void:
	var fixture: Dictionary=state.duplicate(true)
	var witness: String="actor.shen"
	var secret: String="SECRET_ORDER_AND_UNKNOWN_ROAD"
	fixture.facts["military:secret"]={"kind":"military_command","participants":[witness],"tick":0,"route":secret}
	fixture.social.memories[witness]["military:secret"]={"learned_tick":0,"source_actor_id":witness}
	fixture.social.events[social_id]["source"]={"route":secret}
	fixture.results[state.player_actor_id+":"+str(state.request_sequences[state.player_actor_id])].details["secret"]={"military_orders":[secret]}
	fixture.facts["battle:known"]={"kind":"personal_battle","battle_id":"battle:known","result_id":"result:known-battle","tick":0,"outcome":"victory","participants":[witness],"defeated":[],"source":{"command":secret,"route":secret}}
	fixture.social.memories[witness]["battle:known"]={"learned_tick":0,"source_actor_id":witness}
	var battle: Dictionary=fact(fixture,data,"battle:known",witness)
	check(battle.known_causes.size()==1,"个人战结果亲历者可读已存战果摘要")
	if battle.known_causes.size()==1:
		check(battle.known_causes[0].fact.outcome=="victory" and not battle.known_causes[0].fact.has("source"),"可见战果不展开源事件和军令")
		battle.known_causes[0].fact.participants.append("actor.forged")
		check(fixture.facts["battle:known"].participants==[witness],"嵌套参与者数组同样是隔离副本")
	check(Explain.explain(fixture,data,{"kind":"result","result_id":"result:known-battle"},witness).known_causes.size()==1,"可见领域结果仅通过本人已知事实解析")
	fixture.social.memories["actor.carter"]["battle:known"]={"learned_tick":1,"source_actor_id":witness}
	fixture.social.events["social:heard-battle"]={"kind":"news","fact_id":"battle:known","actor_id":witness,"target_actor_id":"actor.carter","tick":1}
	var heard: Dictionary=fact(fixture,data,"battle:known","actor.carter")
	check(heard.known_causes.size()==1 and heard.known_causes[0].knowledge=="reported","听闻战斗必须有较早来源和实际转述记录")
	if heard.known_causes.size()==1:
		check(not heard.known_causes[0].fact.has("source") and not heard.known_causes[0].fact.has("outcome") and not heard.known_causes[0].fact.has("participants"),"只听到战斗消息不扩大为参战者所见细节")
	check(Explain.explain(fixture,data,{"kind":"result","result_id":"result:known-battle"},"actor.carter").known_causes.is_empty(),"转述摘要未公开的result_id不能用于旁路钻取战果")
	check(fact(fixture,data,"military:secret",witness).known_causes.is_empty(),"未经支持的军令即使伪装memory也不可展开")
	for query in [{"kind":"fact","fact_id":social_id},{"kind":"command","request_seq":state.request_sequences[state.player_actor_id]},{"kind":"fact","fact_id":"battle:known"}]:
		check(not JSON.stringify(Explain.explain(fixture,data,query,witness)).contains(secret),"允许的投影不会泄露嵌套军令和道路："+str(query.kind))
	check(not JSON.stringify(Explain.explain(fixture,data,{"kind":"command","request_seq":state.request_sequences[state.player_actor_id]})).contains(secret),"本人advance中的秘密阶段details也不会透传")
	for query in [{},{"kind":"world"},{"kind":"route","route_id":secret},{"kind":"fact"},{"kind":"fact","fact_id":3},{"kind":"fact","fact_id":social_id,"observer_id":witness},{"kind":"command","request_seq":true},{"kind":"command","request_seq":0},{"kind":"offer","offer_id":""}]:
		var rejected: Dictionary=Explain.explain(fixture,data,query)
		check(rejected.get("ok")==false and rejected.get("code") in ["InvalidExplanationQuery","UnsupportedExplanationQuery"] and rejected.known_causes.is_empty() and rejected.next_options.is_empty(),"不支持及非法查询明确失败而非枚举世界："+str(query))
	check(Explain.explain(fixture,data,{"kind":"fact","fact_id":social_id},"actor.missing").get("code")=="UnknownObserver","未知观察者不能退回玩家权限")
	check(Explain.explain({},data,{"kind":"fact","fact_id":social_id}).get("code")=="NoWorld","无世界时明确拒绝查询")

## 对已提交世界验证只读、存读和权限差异；此文件不宣称事务或完整规划器完成。
func run_checks() -> void:
	var kernel=social_world()
	if kernel==null: finish(); return
	var state: Dictionary=kernel.snapshot()
	var data: Dictionary=kernel.content().data
	var id: String=""
	for event in state.social.events.values():
		if event.kind=="theft": id=event.id; break
	check(not id.is_empty(),"测试使用真实发生的夺粮事实")
	if id.is_empty(): finish(); return
	check(Social.validate_state(state,data).ok,"社会测试输入符合原保存与来源契约")
	check_social_visibility(kernel,state,data,id)
	check_commands(state,data)
	check_offer()
	check_training()
	check_secret_boundaries(state,data,id)
	var saved: Dictionary=Codec.decode(Codec.encode(state,0,"explanation-visibility",kernel.content()),kernel.content())
	check(saved.ok and kernel.restore(saved.state).ok,"解释后完整世界仍可保存恢复")
	if saved.ok:
		check(fact(saved.state,data,id,"actor.carter")==fact(state,data,id,"actor.carter") and saved.state==state,"存读不重算来源、知识或解释权限")
	finish()

## 输出真实局部断言数；未运行图形、全量回归与Kernel正式接入不计通过。
func finish() -> void:
	print("EXPLANATION_VISIBILITY_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
