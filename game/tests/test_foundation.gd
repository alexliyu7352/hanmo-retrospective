## 通过实际内容和Kernel验证基础规则、作者对白、取消边界与持久恢复。
extends SceneTree
const Kernel = preload("res://core/world_kernel.gd")
const Catalog = preload("res://core/content_catalog.gd")
var checks: int = 0
var failures: Array[String] = []

## 等待正常资源初始化完成后再执行断言。
func _init() -> void:
	call_deferred("_run")

## 累计具名行为断言，保留全部失败而非首错提前退出。
func check(condition: bool, message: String) -> void:
	checks += 1
	if not condition:
		failures.append(message)
		printerr("FAIL: ",message)

## 使用已准备的第二作者包创建真实内核。
func fresh():
	var kernel = Kernel.new()
	var result: Dictionary = kernel.call("new_game","scenario.two_stories","res://data/generated/two_stories.hmcatalog.json")
	check(result.ok,"new author scenario initializes")
	return kernel

## 携带当前请求身份，通过场景共用的公开提交边界发出命令。
func send(kernel, kind: String, params: Dictionary = {}, actor_id: String = "") -> Dictionary:
	var state: Dictionary = kernel.snapshot()
	if actor_id.is_empty():
		actor_id = state.player_actor_id
	return kernel.submit({"kind":kind,"params":params,"actor_id":actor_id,"request_seq":state.request_sequences[actor_id]+1,"expected_revision":state.revision})

## 描述真实人物和官署上下文，不替代内核在场与权限校验。
func office_context() -> Dictionary:
	return {"place_id":"west.office","provider_actor_id":"actor.chen","role_id":"role.office"}

## 验证数量、位置、隔离副本、无凭空奖励和真实在场岗位限制。
func test_world_context() -> void:
	var kernel = fresh()
	var state: Dictionary = kernel.snapshot()
	check(state.actors.size()==6 and state.tasks.is_empty(),"actors are instances; tasks are not pre-accepted")
	check(not state.has("actor") and not state.has("task"),"legacy single actor/task fields removed")
	state.actors["actor.player"].hp=1
	check(kernel.snapshot().actors["actor.player"].hp==100,"snapshots do not mutate world")
	var before: Dictionary = kernel.snapshot()
	for _i in range(20):
		kernel.preview("move_local",{"place_id":"west.office"})
	check(kernel.snapshot()==before,"twenty previews preserve every gameplay field and RNG")
	check(not send(kernel,"move_local",{"place_id":"east.home"}).ok,"local movement cannot teleport across cities")
	check(kernel.snapshot()==before,"invalid movement has no side effects")
	check(send(kernel,"move_local",{"place_id":"west.office"}).ok,"actual local passage works")
	check(kernel.snapshot().tick==0 and kernel.snapshot().revision==1,"local position changes revision not time")
	check(kernel.snapshot().actors["actor.player"].location.place_id=="west.office","single authoritative sub-place")
	var ctx: Dictionary = office_context()
	check(kernel.preview("discover_offer",{"offer_id":"offer.grain","context":ctx}).ok,"officer can explain actual sourced offer")
	var wrong: Dictionary = ctx.duplicate(true)
	wrong.provider_actor_id="actor.zheng"
	check(not kernel.preview("discover_offer",{"offer_id":"offer.grain","context":wrong}).ok,"merchant cannot impersonate officer")
	check(send(kernel,"inspect",{"prop_id":"notice.grain"}).ok,"scene notice can be observed")
	state=kernel.snapshot()
	check(state.knowledge[state.player_actor_id].has("offer.grain"),"notice discovers a lead")
	check(state.wallets["wallet.player"].balance==800 and state.tasks.is_empty(),"reading notice does not award budget or task")
	before=state.duplicate(true)
	check(send(kernel,"inspect",{"prop_id":"notice.grain"}).ok,"re-reading notice is permitted")
	check(kernel.snapshot().wallets==before.wallets and kernel.snapshot().tasks==before.tasks,"re-reading has no repeat reward")
	state=kernel.snapshot()
	state.actors["actor.chen"].location={"kind":"Settlement","settlement_id":"city.west","place_id":"west.street"}
	check(kernel.restore(state).ok,"restoring a legal absent officer is permitted")
	check(not kernel.preview("discover_offer",{"offer_id":"offer.grain","context":ctx}).ok,"absent officer cannot transact")
	before=kernel.snapshot()
	state=before.duplicate(true)
	state.actors["actor.player"].location.place_id="unknown"
	check(not kernel.restore(state).ok and kernel.snapshot()==before,"invalid location cannot replace live state")
	state=before.duplicate(true)
	state.wallets["wallet.player"].balance=3.5
	check(not kernel.restore(state).ok and kernel.snapshot()==before,"fractional money rejected atomically")
	var first=Kernel.new()
	check(first.call("new_game","scenario.procurement","res://data/generated/procurement.hmcatalog.json").ok,"same runtime loads first pack")
	check(first.snapshot().actors.size()==4,"first pack not expanded to hard-coded six")

## 执行本文件已启用的基础行为组，统一报告实际断言结果。
func _run() -> void:
	var probe = Kernel.new()
	var ready: bool = false
	for method in probe.get_method_list():
		if method.name == "new_game" and method.args.size()>=2:
			ready=method.args[0].name=="scenario_id" and method.args[1].name=="content_path" and method.args[0].type==TYPE_STRING and method.args[1].type==TYPE_STRING and method.args.size()-method.default_args.size()<=2
	check(ready,"new_game保留scenario/content两参调用，后续参数必须可省略")
	if ready:
		test_world_context()
		test_services()
		test_traffic()
		test_dialogue()
		test_boundaries()
	print("FOUNDATION_CHECKS=",checks," FAILURES=",failures.size())
	quit(0 if failures.is_empty() else 1)

## 构造指定场所的当前授权代表上下文。
func context(place_id: String, provider: String, role_id: String) -> Dictionary:
	return {"place_id":place_id,"provider_actor_id":provider,"role_id":role_id}

## 通过实际世界命令完成在办动作，禁止直接修改日期。
func finish(kernel) -> bool:
	for _step in range(50):
		if kernel.snapshot().actions.is_empty(): return true
		var result: Dictionary = send(kernel,"advance")
		if not result.ok:
			printerr("advance refused: ",result)
			return false
	return false

## 验证两个独立任务实例、真实预算、商品产权与信件交接。
func test_services() -> void:
	var kernel=fresh()
	send(kernel,"move_local",{"place_id":"west.office"})
	var params: Dictionary = {"offer_id":"offer.grain","context":office_context()}
	check(not send(kernel,"accept_offer",params).ok,"cannot accept unknown commission")
	send(kernel,"inspect",{"prop_id":"notice.grain"})
	var before: Dictionary = kernel.snapshot()
	var accepted: Dictionary = send(kernel,"accept_offer",params)
	check(accepted.ok,"sourced commission can begin acceptance")
	if not accepted.ok: return
	var state: Dictionary = kernel.snapshot()
	var task: Dictionary = state.tasks.values()[0]
	check(state.tick==0 and task.status=="accepting","acceptance is a pending half-day action")
	check(state.wallets["wallet.office"].balance==5000 and state.wallets[task.budget_wallet_id].balance==0,"acceptance does not prematurely disburse budget")
	check(not send(kernel,"move_local",{"place_id":"west.street"}).ok,"cannot leave a committed service mid-tick")
	var duplicate: Dictionary = {"kind":"accept_offer","params":params,"actor_id":state.player_actor_id,"request_seq":state.request_sequences[state.player_actor_id],"expected_revision":before.revision}
	check(kernel.submit(duplicate).ok and kernel.snapshot()==state,"same request does not create a second task")
	check(finish(kernel),"acceptance completes through the real clock")
	state=kernel.snapshot(); task=state.tasks.values()[0]
	check(state.tick==1 and task.accepted_tick==1 and task.deadline_tick==9,"deadline begins after one-tick acceptance")
	check(task.status=="active" and state.wallets[task.budget_wallet_id].balance==650 and state.wallets["wallet.office"].balance==4350,"budget transfer occurs once on completion")
	send(kernel,"move_local",{"place_id":"west.street"})
	send(kernel,"move_local",{"place_id":"west.grain"})
	var trade: Dictionary = {"shop_id":"shop.grain","item_id":"item.grain","quantity":10,"side":"buy","payer_wallet_id":task.budget_wallet_id,"context":context("west.grain","actor.zheng","role.merchant")}
	var quote: Dictionary = kernel.preview("trade",trade)
	check(quote.ok and quote.get("total")==473,"real market uses retained marginal-price formula")
	if not quote.ok: return
	trade.quoted_total=quote.total; trade.market_revision=quote.market_revision
	var purchase: Dictionary = send(kernel,"trade",trade)
	check(purchase.ok,"purchase with task-owned budget succeeds")
	check(kernel.snapshot().wallets[task.budget_wallet_id].balance==177,"purchase deducted once before completion")
	check(finish(kernel),"transaction consumes exactly one half-day")
	var bad_sell: Dictionary = trade.duplicate(true); bad_sell.side="sell"; bad_sell.payer_wallet_id="wallet.player"
	check(not kernel.preview("trade",bad_sell).ok,"task cargo cannot be sold as personal goods")
	send(kernel,"move_local",{"place_id":"west.street"})
	check(not send(kernel,"report_offer",params).ok,"remote report rejected by core")
	send(kernel,"move_local",{"place_id":"west.office"})
	check(send(kernel,"report_offer",params).ok and finish(kernel),"actual officer receives and settles grain")
	state=kernel.snapshot(); task=state.tasks.values()[0]
	check(task.status=="success" and state.wallets["wallet.player"].balance==1057,"reward and budget surplus credited once")
	check(state.wallets[task.budget_wallet_id].balance==0,"settled task wallet cleared")
	before=state.duplicate(true)
	check(not send(kernel,"report_offer",params).ok and kernel.snapshot()==before,"duplicate report cannot duplicate money")
	# 同一运行时和世界使用不同任务模板，不引入内容专属实现分支。
	send(kernel,"move_local",{"place_id":"west.street"})
	send(kernel,"move_local",{"place_id":"west.tavern"})
	var letter_params: Dictionary={"offer_id":"offer.letter","context":context("west.tavern","actor.shen","role.sender")}
	check(send(kernel,"discover_offer",letter_params).ok,"letter has its actual sender as source")
	check(send(kernel,"accept_offer",letter_params).ok and finish(kernel),"second template accepts in same world")
	check(kernel.snapshot().tasks.size()==2,"two task instances coexist with distinct identities")
	send(kernel,"move_local",{"place_id":"west.street"})
	check(send(kernel,"travel",{"destination":"village.east"}).ok and finish(kernel),"letter travels on real route")
	send(kernel,"move_local",{"place_id":"east.home"})
	var delivery: Dictionary={"offer_id":"offer.letter","context":context("east.home","actor.he","role.recipient")}
	var money_before: int = kernel.snapshot().wallets["wallet.player"].balance
	check(send(kernel,"deliver_message",delivery).ok and finish(kernel),"named recipient accepts real task letter")
	check(kernel.snapshot().wallets["wallet.player"].balance==money_before,"recipient does not automatically pay sender reward")
	check(not send(kernel,"deliver_message",delivery).ok,"same letter cannot be delivered twice")
	send(kernel,"move_local",{"place_id":"east.entrance"})
	send(kernel,"travel",{"destination":"city.west"}); finish(kernel)
	send(kernel,"move_local",{"place_id":"west.tavern"})
	check(send(kernel,"report_offer",letter_params).ok and finish(kernel),"sender receives report and pays once")
	state=kernel.snapshot()
	check(state.wallets["wallet.player"].balance==1145,"both stories use actual travel expense and reward")
	check(state.wallets["wallet.shen"].balance==480,"private reward comes from sender's real wallet")
	check(state.tasks.values().all(func(t): return t.status=="success"),"both templates reach success using common task collection")

## 验证不依赖初始故事的旅行进度、开销、折返与可保存状态。
func test_traffic() -> void:
	var kernel=fresh()
	var quote: Dictionary=kernel.preview("travel",{"destination":"village.east"})
	check(quote.ok,"content routes are executable")
	if not quote.ok: return
	check(quote.duration==2 and quote.cost==16,"travel preview has true duration and fee")
	check(send(kernel,"travel",{"destination":"village.east"}).ok,"travel starts at city entry")
	check(send(kernel,"advance").ok,"travel advances one paid half-day")
	var state: Dictionary=kernel.snapshot()
	check(state.actors[state.player_actor_id].location.kind=="OnRoute" and state.wallets["wallet.player"].balance==792,"mid-route location and expense are real")
	var resumed=fresh()
	check(resumed.restore(state).ok,"active route can restore")
	check(send(kernel,"return").ok and finish(kernel),"return follows actual progress")
	check(kernel.snapshot().wallets["wallet.player"].balance==784 and kernel.snapshot().tick==2,"return cannot refund past travel")
	check(finish(resumed) and resumed.snapshot().actors[state.player_actor_id].location.place_id=="east.entrance","restored forward route ends at correct content entry")
	var poor=fresh(); state=poor.snapshot(); state.wallets["wallet.player"].balance=8; poor.restore(state)
	send(poor,"travel",{"destination":"village.east"}); send(poor,"advance")
	check(not send(poor,"advance").ok,"no travel with negative wallet")
	check(send(poor,"rescue").ok and finish(poor),"stranded character has a real-time recovery action")
	check(poor.snapshot().tick==3 and poor.snapshot().actors[state.player_actor_id].location.place_id=="west.street","rescue consumes two additional half-days and restores place")

## 通过作者分支确认真实合同并恢复等待对白；一次入场事件离场取消后不得复活。
func test_dialogue() -> void:
	var kernel=fresh()
	check(kernel.has_method("interaction_view"),"real dialogue runtime is present")
	if not kernel.has_method("interaction_view"): return
	send(kernel,"move_local",{"place_id":"west.office"})
	check(send(kernel,"talk",{"role_id":"role.office"}).ok,"talk binds the actual office holder")
	var view: Dictionary=kernel.call("interaction_view")
	check(view.phase=="say" and view.speaker=="actor.chen","authored speaker and location are retained")
	check(send(kernel,"dialogue_next").ok,"say node advances to choice")
	view=kernel.call("interaction_view")
	check(view.phase=="choice" and not view.options.any(func(o): return o.id=="accept"),"unknown offer is not prematurely available")
	check(send(kernel,"dialogue_choose",{"option_id":"ask"}).ok,"ask runs authored zero-time discovery action")
	check(kernel.call("interaction_view").phase=="say" and kernel.snapshot().tasks.is_empty(),"hearing terms does not accept the task")
	send(kernel,"dialogue_next")
	check(send(kernel,"dialogue_choose",{"option_id":"accept"}).ok,"choosing acceptance presents its real action")
	check(kernel.call("interaction_view").phase=="action" and kernel.snapshot().tasks.is_empty(),"action preview remains uncommitted")
	check(send(kernel,"dialogue_confirm").ok,"confirmation starts contract through production kernel")
	check(kernel.call("interaction_view").phase=="waiting","dialogue waits for action completion")
	var saved: Dictionary=kernel.snapshot()
	var restored=fresh()
	check(restored.restore(saved).ok,"pending dialogue/action snapshot restores")
	check(finish(kernel) and finish(restored),"both pending actions can finish")
	check(kernel.snapshot()==restored.snapshot(),"restoring dialogue does not re-pay or fork results")
	check(kernel.call("interaction_view").phase=="say","real completion returns to authored acknowledgement")
	send(kernel,"dialogue_next")
	check(kernel.call("interaction_view").phase=="scene","dialogue ends in actual room")
	send(kernel,"move_local",{"place_id":"west.street"}); send(kernel,"move_local",{"place_id":"west.tavern"})
	var count: int=kernel.snapshot().events.size()
	check(count==1,"authored scene event was queued on entry")
	send(kernel,"move_local",{"place_id":"west.street"}); send(kernel,"move_local",{"place_id":"west.tavern"})
	check(kernel.snapshot().events.size()==count,"same-tick repeated entrance does not duplicate event")
	check(kernel.snapshot().events["actor.player|event.letter"].status=="cancelled" and not send(kernel,"open_event",{"event_id":"event.letter"}).ok,"离场取消的一次事件重入不复活")
	# 对白断言使用新世界的首次真实入场，不再依赖已取消事件复活的旧缺陷。
	kernel=fresh(); send(kernel,"move_local",{"place_id":"west.tavern"})
	check(send(kernel,"open_event",{"event_id":"event.letter"}).ok,"event opens the real sender dialogue")
	check(kernel.call("interaction_view").speaker=="actor.shen","event uses actual scene participant")
	check(send(kernel,"end_interaction").ok,"player may decline conversation")
	var before: Dictionary=kernel.snapshot()
	for _i in range(20): kernel.call("interaction_view")
	check(before==kernel.snapshot(),"dialogue view is side-effect free")

## 拒绝危险恢复数据，在服务中断时释放预留且不丢失公款。
func test_boundaries() -> void:
	var kernel=fresh()
	send(kernel,"move_local",{"place_id":"west.office"}); send(kernel,"inspect",{"prop_id":"notice.grain"})
	var params: Dictionary={"offer_id":"offer.grain","context":office_context()}
	var accepted: Dictionary=send(kernel,"accept_offer",params)
	if not accepted.ok: return
	var state: Dictionary=kernel.snapshot()
	var bad: Dictionary=state.duplicate(true)
	bad.actions.values()[0].params={}
	check(not kernel.restore(bad).ok,"malformed active action cannot be restored and later crash")
	if kernel.snapshot()!=state:
		# 若故意损坏的恢复被错误接受，重建独立夹具以继续暴露后续边界。
		kernel=fresh(); kernel.restore(state)
	bad=state.duplicate(true); bad.tasks.values()[0].deadline_tick="later"
	check(not kernel.restore(bad).ok,"task times are integral")
	bad=state.duplicate(true); bad.wallets["wallet.office"].reserved={}
	check(not kernel.restore(bad).ok,"acceptance cannot restore without its promised escrow")
	if kernel.snapshot()!=state:
		kernel=fresh(); kernel.restore(state)
	state.actors["actor.chen"].available=false
	check(kernel.restore(state).ok,"participant may become unavailable before action completion")
	check(finish(kernel),"unavailable participant produces a normal failed outcome")
	state=kernel.snapshot()
	check(state.wallets["wallet.office"].balance==5000 and state.wallets["wallet.office"].reserved.is_empty(),"failed acceptance releases source reservation, never pays budget")
	check(state.tasks.values()[0].status=="cancelled" and state.actions.is_empty(),"failed acceptance releases all participant occupancy")
