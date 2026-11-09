## 已有公事与私人合同中断回归；重办必须使用当前正式上下文且所有产权保持守恒。
extends SceneTree
const P=preload("res://tests/policy_fixture.gd")
const F=preload("res://tests/companion_fixture.gd")
const Tasks=preload("res://core/task_rules.gd")
const Conditions=preload("res://core/condition_rules.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Encounters=preload("res://core/encounter_rules.gd")
const Fault=preload("res://tests/test_event_policy.gd").FaultKernel
var checks: int=0
var failures: int=0

## 实际引擎准备后执行，不模拟世界结算。
func _init() -> void:
	call_deferred("run_checks")

## 每个条件保留独立失败标签，完整结果反映在退出码。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 第一次受领中断后保留旧记录，下一次正式受领才获得一份预算。
func public_acceptance() -> void:
	var k=P.accepting(); var original: Dictionary=P.params(k); var before: Dictionary=k.snapshot()
	var current: Dictionary=P.replace_officer(k)
	check(k.advance_one_tick().ok,"换岗不冻结世界推进")
	var s: Dictionary=k.snapshot(); var old: Dictionary=s.tasks.values()[0]
	check(old.status=="cancelled" and old.accepted_tick==0 and old.deadline_tick==0 and old.settlement_id=="","受领未完成取消而无结算")
	check(s.wallets["wallet.office"].balance==before.wallets["wallet.office"].balance and s.wallets["wallet.office"].reserved.is_empty() and s.wallets[old.budget_wallet_id].balance==0,"中断释放源预算且未拨款")
	check(not k.preview("accept_offer",original).ok,"旧provider上下文拒绝")
	check(k.preview("accept_offer",current).ok,"组织同岗位真实继任人允许新上下文重办")
	check(Conditions.task_status(s,"actor.player","offer.grain")=="known","未拨款中断在作者条件投影回known")
	# 实际office对白属于陈主簿，恢复其岗位只为检查真实作者选项，不虚构继任者对白。
	s.roles["role.office"].actor_id="actor.chen"; check(k.restore(s).ok,"原官回岗用于作者对白")
	check(F.send(k,"talk",{"role_id":"role.office"}).ok and F.send(k,"dialogue_next").ok,"真实作者对白到选择边界")
	var options: Array=k.interaction_view().get("options",[])
	check(options.any(func(row): return row.id=="accept"),"作者受领选项可再次显示")
	F.send(k,"end_interaction"); current=P.replace_officer(k)
	var cmd: Dictionary=P.command(k,"accept_offer",current); var result: Dictionary=k.submit(cmd); var begun: Dictionary=k.snapshot()
	if not check(result.ok,"正式新受领开始"): return
	check(k.submit(cmd)==result and k.snapshot()==begun,"受领重复请求只建立一个新合同")
	check(k.advance_one_tick().ok,"新受领正常完成")
	s=k.snapshot(); var latest: Dictionary=Conditions.task_for(s,"actor.player","offer.grain")
	check(s.tasks.size()==2 and s.tasks[old.id]==old and latest.status=="active","旧取消记录不改，新实例独立生效")
	check(s.wallets[latest.budget_wallet_id].balance==650 and s.wallets["wallet.office"].balance==before.wallets["wallet.office"].balance-650 and P.money(s)==P.money(before),"只拨一次650且所有资金守恒")

## 报账中断不退还已受领预算或转移私产，合法继任者可再次当面结算一次。
func public_report() -> void:
	var k=P.grain_ready(); var s: Dictionary=k.snapshot(); var task: Dictionary=s.tasks.values()[0]
	var original: Dictionary=P.params(k); check(F.send(k,"report_offer",original).ok,"正常报告开始并预留物资报酬")
	var current: Dictionary=P.replace_officer(k)
	check(k.advance_one_tick().ok,"报告中途换岗仍推进")
	var interrupted: Dictionary=k.snapshot()
	check(interrupted.tasks[task.id]==task and interrupted.wallets==s.wallets and interrupted.relations==s.relations,"中断保留完整合同、余额、关系，不发奖不清算")
	check(Inventory.count(interrupted.inventories["bag:actor.player"],"item.grain",[{"kind":"task","id":task.id}])==10,"任务粮食解除预留但不变更归属")
	check(not k.preview("report_offer",original).ok and k.preview("report_offer",current).ok,"旧上下文失败，真实继任上下文可报告")
	for invalid in ["disabled","dead","absent","wrong_role"]:
		var variant: Dictionary=interrupted.duplicate(true); var params: Dictionary=current.duplicate(true)
		if invalid=="disabled": variant.roles["role.office"].enabled=false
		if invalid=="dead": variant.actors["actor.apprentice"].alive=false
		if invalid=="absent": variant.actors["actor.apprentice"].location.place_id="west.street"
		if invalid=="wrong_role": params.context=F.context(k,"role.merchant")
		check(not Tasks.preview(variant,k.content().data,"actor.player","report_offer",params).ok,"不合法继任者拒绝："+invalid)
	var cmd: Dictionary=P.command(k,"report_offer",current); var started: Dictionary=k.submit(cmd)
	if not check(started.ok and k.advance_one_tick().ok,"新继任上下文完成结算"): return
	var settled: Dictionary=k.snapshot()
	check(settled.tasks[task.id].status=="success" and settled.wallets["wallet.player"].balance==1057 and P.money(settled)==P.money(s),"真实资金一次交割，合计守恒")
	check(k.submit(cmd)==started and k.snapshot()==settled and not k.preview("report_offer",current).ok,"旧请求及再报告均不得二次发奖")

## 信件只能交给固定收件人、向原私人委托人回报，中断不得取消已接受承诺。
func private_message() -> void:
	var k=P.queued(); var params: Dictionary=P.params(k,"offer.letter","role.sender")
	check(F.send(k,"discover_offer",params).ok and F.send(k,"accept_offer",params).ok and k.advance_one_tick().ok,"真实私人受领")
	var task: Dictionary=Conditions.task_for(k.snapshot(),"actor.player","offer.letter")
	check(not k.preview("accept_offer",params).ok,"已受领零预算承诺不得再次受领")
	F.send(k,"move_local",{"place_id":"west.street"}); check(F.send(k,"travel",{"destination":"village.east"}).ok,"真实旅程至收件人城市")
	for _i in range(10):
		if k.snapshot().actors["actor.player"].location.kind=="Settlement": break
		check(k.advance_one_tick().ok,"送信旅行推进")
	check(F.send(k,"move_local",{"place_id":"east.home"}).ok,"到真实收件处")
	params=P.params(k,"offer.letter","role.recipient")
	check(F.send(k,"deliver_message",params).ok,"交信开始预留信件")
	var s: Dictionary=k.snapshot(); s.actors["actor.he"].available=false; check(k.restore(s).ok,"收件人中途不可用")
	check(k.advance_one_tick().ok,"交信中断仍推进")
	s=k.snapshot(); check(s.tasks[task.id]==task and Inventory.count(s.inventories["bag:actor.player"],"item.family_letter",[{"kind":"task","id":task.id}])==1,"wait_or_leave保留信件和已接受合同")
	var replacement: Dictionary=s.duplicate(true); replacement.actors["actor.zheng"].location=s.actors["actor.he"].location.duplicate(true); replacement.roles["role.recipient"].actor_id="actor.zheng"
	var substitute: Dictionary=params.duplicate(true); substitute.context.provider_actor_id="actor.zheng"
	check(Tasks.preview(replacement,k.content().data,"actor.player","deliver_message",substitute).get("code")=="WrongRecipient","固定收件人不得被岗位继任者代替")
	s.actors["actor.he"].available=true; check(k.restore(s).ok and F.send(k,"deliver_message",params).ok and k.advance_one_tick().ok,"原收件人回来可依法重交")
	check(k.snapshot().tasks[task.id].status=="ready","只真实交信产生ready")
	F.send(k,"move_local",{"place_id":"east.entrance"}); check(F.send(k,"travel",{"destination":"city.west"}).ok,"真实返回")
	for _i in range(10):
		if k.snapshot().actors["actor.player"].location.kind=="Settlement": break
		check(k.advance_one_tick().ok,"返程推进")
	F.send(k,"move_local",{"place_id":"west.tavern"}); params=P.params(k,"offer.letter","role.sender")
	check(F.send(k,"report_offer",params).ok,"私人回报开始")
	s=k.snapshot(); s.actors["actor.shen"].available=false; k.restore(s); check(k.advance_one_tick().ok,"私人回报中断仍推进")
	s=k.snapshot(); check(s.tasks[task.id].status=="ready" and s.tasks[task.id].settlement_id=="" and s.wallets["wallet.shen"].reserved.is_empty(),"中断保留ready且释放奖励预留")
	replacement=s.duplicate(true); replacement.roles["role.sender"].actor_id="actor.zheng"; replacement.actors["actor.zheng"].location=s.actors["actor.player"].location.duplicate(true)
	substitute=params.duplicate(true); substitute.context.provider_actor_id="actor.zheng"
	check(Tasks.preview(replacement,k.content().data,"actor.player","report_offer",substitute).get("code")=="PrivatePromiseNotDelegated","私人承诺不得交给新sender")

## 沿实际道路继续到终点并对普通会面道别，不传送或构造护卫抵达事实。
func finish_travel(k) -> bool:
	for _step in range(24):
		var state: Dictionary=k.snapshot()
		if state.actors[state.player_actor_id].location.kind=="Settlement": return true
		var encounter: Dictionary=Encounters.pending_for(state,state.player_actor_id)
		if not encounter.is_empty() and not F.send(k,"encounter_leave",{"encounter_id":encounter.id}).ok: return false
		if not k.advance_one_tick().ok: return false
	return false

## 既有Alpha护卫真实同行到达后报告中断，完整保留护卫数据、永久抵达事实和未发放奖励。
func private_escort() -> void:
	var k=P.fresh(true); check(F.send(k,"move_local",{"place_id":"west.tavern"}).ok,"护卫到真实委托处")
	var params: Dictionary=P.params(k,"offer.escort","role.sender")
	check(F.send(k,"discover_offer",params).ok and F.send(k,"accept_offer",params).ok and k.advance_one_tick().ok,"护卫真实受领")
	check(F.send(k,"join_party",{"context":F.context(k,"role.carter")}).ok,"保护对象真实加入同行")
	F.send(k,"move_local",{"place_id":"west.street"})
	if not check(F.send(k,"travel",{"destination":"village.east"}).ok and finish_travel(k),"护卫实际同行抵达东里"): return
	check(F.send(k,"move_local",{"place_id":"east.home"}).ok,"同行抵达约定位置")
	var task: Dictionary=Conditions.task_for(k.snapshot(),"actor.player","offer.escort")
	check(task.status=="ready" and k.snapshot().facts.has("escort:"+task.id),"真实ready和抵达事实已生成")
	F.send(k,"move_local",{"place_id":"east.entrance"})
	if not check(F.send(k,"travel",{"destination":"city.west"}).ok and finish_travel(k),"护卫本人真实返回"): return
	F.send(k,"move_local",{"place_id":"west.tavern"}); params=P.params(k,"offer.escort","role.sender")
	var before: Dictionary=k.snapshot()
	check(F.send(k,"report_offer",params).ok,"护卫开始正式报告")
	var state: Dictionary=k.snapshot(); state.actors["actor.shen"].available=false; check(k.restore(state).ok,"护卫委托人中途不可用")
	check(k.advance_one_tick().ok,"护卫报告中断仍推进")
	state=k.snapshot()
	check(state.tasks[task.id]==before.tasks[task.id] and state.facts["escort:"+task.id]==before.facts["escort:"+task.id],"护卫中断完整保留已接受任务和事实")
	var control=P.fresh(true); var control_state: Dictionary=before.duplicate(true); control_state.actors["actor.shen"].available=false
	check(control.restore(control_state).ok and F.send(control,"wait").ok and control.advance_one_tick().ok,"对照世界正常经过同一半日，保留无关军事旅行支出")
	check(state.wallets==control.snapshot().wallets and state.wallets["wallet.shen"].reserved.is_empty(),"护卫中断与正常等待相同资金，无暗中清算或发奖")
	state.actors["actor.shen"].available=true; check(k.restore(state).ok and F.send(k,"report_offer",params).ok and k.advance_one_tick().ok,"原护卫委托人回归才可重办")
	check(k.snapshot().tasks[task.id].status=="success" and k.snapshot().wallets["wallet.player"].balance==before.wallets["wallet.player"].balance+180,"真实护卫成功只发一次180")

## 放宽重试只允许从未拨款且无产权残留的受领中断，不能借伪造最新记录重复领取。
func retry_guards() -> void:
	var k=P.accepting(); P.replace_officer(k); k.advance_one_tick()
	var clean: Dictionary=k.snapshot(); var task: Dictionary=clean.tasks.values()[0]; var params: Dictionary=P.params(k)
	for guard in ["success","failed","user_cancelled","accepted_zero","balance","reserved","lot","pending","prior_funded"]:
		var s: Dictionary=clean.duplicate(true); var current: Dictionary=s.tasks[task.id]
		if guard in ["success","failed"]: current.status=guard
		if guard=="user_cancelled": current.settlement_id="settlement:999"
		if guard=="accepted_zero": current.accepted_tick=1; current.deadline_tick=9
		if guard=="balance": s.wallets[current.budget_wallet_id].balance=1
		if guard=="reserved": s.wallets[current.budget_wallet_id].reserved["action:999"]=0
		if guard=="lot": Inventory.add(s,"bag:actor.player","item.grain",1,{"kind":"task","id":current.id})
		if guard=="pending": s.actions["action:999"]={"params":{"task_id":current.id}}
		if guard=="prior_funded":
			var prior: Dictionary=current.duplicate(true); prior.id="task:0"; prior.serial=0; prior.accepted_tick=1; prior.deadline_tick=9; prior.status="success"; s.tasks[prior.id]=prior
		check(not Tasks.preview(s,k.content().data,"actor.player","accept_offer",params).ok,"重试反滥用边界："+guard)
	var source: int=clean.wallets["wallet.office"].balance
	for _i in range(2):
		if not check(F.send(k,"accept_offer",P.params(k)).ok,"再次中断尝试可合法开始"): break
		var s: Dictionary=k.snapshot(); s.roles["role.office"].enabled=false; k.restore(s); check(k.advance_one_tick().ok,"再次中断")
		s=k.snapshot(); s.roles["role.office"].enabled=true; k.restore(s)
		check(s.wallets["wallet.office"].balance==source and s.wallets["wallet.office"].reserved.is_empty(),"反复中断从不扣源预算")
	var rules=load("res://core/task_rules.gd")
	check(rules.has_method("unavailable_result"),"任务有显式不可用策略分派")
	if rules.has_method("unavailable_result"):
		var s: Dictionary=clean.duplicate(true); var data: Dictionary=k.content().data; data.offers["offer.grain"].on_unavailable="invent_successor"
		var before: Dictionary=s.duplicate(true)
		var result: Dictionary=rules.call("unavailable_result",s,data,{"id":"action:999","kind":"accept_offer","params":{"task_id":task.id}},"PersonAbsent")
		check(not result.ok and result.code=="UnsupportedTaskPolicy" and s==before,"未知任务策略明确拒绝且不变更")

## 中断受领与结算尾部失败恢复原预留，重试和请求重放只拨一次资金。
func task_tail_rollback() -> void:
	var source=P.accepting(); var k=Fault.new(); k.new_game(); check(k.restore(source.snapshot()).ok,"故障子类恢复真实待受领")
	var before: Dictionary=k.snapshot(); var cmd: Dictionary=P.command(k,"advance"); k.armed=true
	check(not k.submit(cmd).ok and k.snapshot()==before,"受领已拨款后尾部失败完整回滚预算预留和计数器")
	k.armed=false; var result: Dictionary=k.submit(cmd); var after: Dictionary=k.snapshot()
	check(result.ok and after.wallets[after.tasks.values()[0].budget_wallet_id].balance==650,"尾部故障修复后恰好拨一次款")
	check(k.submit(cmd)==result and k.snapshot()==after,"完成命令重复请求不再次拨款")

## 实际采购结算已经发生后注入尾部故障，货物、信任、预算、结果缓存必须一起回滚。
func report_tail_rollback() -> void:
	var source=P.grain_ready(); check(F.send(source,"report_offer",P.params(source)).ok,"尾部报账测试真实预留")
	var k=Fault.new(); k.new_game(); check(k.restore(source.snapshot()).ok,"故障子类恢复待报账")
	var before: Dictionary=k.snapshot(); var cmd: Dictionary=P.command(k,"advance"); k.armed=true
	check(not k.submit(cmd).ok and k.snapshot()==before,"结算后尾部故障完整回滚财产、关系、历史和预留")
	k.armed=false; var result: Dictionary=k.submit(cmd); var after: Dictionary=k.snapshot()
	check(result.ok and after.tasks.values()[0].status=="success" and after.wallets["wallet.player"].balance==1057,"修复后报账结算一次")
	check(k.submit(cmd)==result and k.snapshot()==after,"已完成报账重复请求不二次发奖")

## 依次执行已作者化公事、私信和中断边界。
func run_checks() -> void:
	public_acceptance(); public_report(); private_message(); private_escort(); retry_guards(); task_tail_rollback(); report_tail_rollback()
	print("TASK_POLICY_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
