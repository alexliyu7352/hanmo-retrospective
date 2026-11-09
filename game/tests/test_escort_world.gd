## 用正常命令贯通受托、同行、真实道路战斗、抵达与回报，不能靠篡改合同状态制造履约。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const F=preload("res://tests/companion_fixture.gd")
const Encounters=preload("res://core/encounter_rules.gd")
const Sessions=preload("res://core/combat_session.gd")
const Round=preload("res://core/personal_round.gd")
const Codec=preload("res://core/save_codec.gd")
var checks: int=0
var failures: int=0

## 等正式目录可读取后执行，结果不替代真实鼠标与画面验收。
func _init() -> void:
	call_deferred("run_checks")

## 记录真实断言及具体失败标签，并在结尾反映到进程退出码。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 创建同一作者包的新世界并当面接取护卫；暂未邀请的目标仍是酒肆原人物。
func contracted():
	var k=Kernel.new(); var loaded: Dictionary=k.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json")
	check(loaded.ok,"护卫样本可运行："+str(loaded))
	if not loaded.ok: return k
	check(F.send(k,"move_local",{"place_id":"west.tavern"}).ok,"到酒肆询问实际委托人")
	var params: Dictionary={"offer_id":"offer.escort","context":F.context(k,"role.sender")}
	check(F.send(k,"discover_offer",params).ok and F.send(k,"accept_offer",params).ok and k.advance_one_tick().ok,"从真实来源受托并等待确认")
	check(F.send(k,"join_party",{"context":F.context(k,"role.carter")}).ok,"另与实际车夫约定同行")
	F.send(k,"move_local",{"place_id":"west.street"})
	check(F.send(k,"travel",{"destination":"village.east"}).ok,"整支真实队伍开始同一段行程")
	return k

## 沿实际半日轨迹寻找可战接触，普通会面道别后仍走原路，不临时生成敌人。
func reach_conflict(k) -> Dictionary:
	for step in range(12):
		var state: Dictionary=k.snapshot()
		var meeting: Dictionary=Encounters.pending_for(state,state.player_actor_id)
		if not meeting.is_empty():
			if k.preview("encounter_fight",{"encounter_id":meeting.id}).ok: return meeting
			F.send(k,"encounter_leave",{"encounter_id":meeting.id})
		if not k.snapshot().actors[state.player_actor_id].location.kind in ["OnRoute","Wilderness"]: break
		var advance: Dictionary=k.advance_one_tick()
		if not advance.ok: printerr("TRAVEL: ",advance); break
	return {}

## 正常继续旅行到终点，路上选择道别；本函数不会传送人物或改写保护状态。
func finish_travel(k) -> bool:
	for step in range(20):
		var s: Dictionary=k.snapshot()
		if s.actors[s.player_actor_id].location.kind=="Settlement": return true
		var e: Dictionary=Encounters.pending_for(s,s.player_actor_id)
		if not e.is_empty() and not F.send(k,"encounter_leave",{"encounter_id":e.id}).ok: return false
		if not k.advance_one_tick().ok: return false
	return false

## 保存轮间后正常取胜并回到同行原行程，最后通过真实委托人收取一次报酬。
func run_checks() -> void:
	var k=contracted()
	if failures: finish(); return
	var e: Dictionary=reach_conflict(k)
	check(not e.is_empty(),"沿道路真实接触配置中的持械旅人")
	if e.is_empty(): finish(); return
	var before: Dictionary=k.snapshot(); var player: String=before.player_actor_id
	var start: Dictionary=F.send(k,"encounter_fight",{"encounter_id":e.id})
	check(start.ok,"实际路遇进入护卫战："+str(start))
	if not start.ok: finish(); return
	var b: Dictionary=Sessions.for_actor(k.snapshot(),player)
	check(b.objective_id=="objective.escort" and b.units["actor.carter"].protected,"合同中的同行人真实进入保护目标")
	check(k.snapshot().tick==before.tick and k.snapshot().actors[player].location==before.actors[player].location,"道路战不重复推进所属半日或改写原旅程锚点")
	var saved: Dictionary=Codec.decode(Codec.encode(k.snapshot(),0,"escort-world",k.content()),k.content())
	check(saved.ok and k.restore(saved.state).ok,"道路战轮间保存保留合同、同行与原行程")
	for turn in range(26):
		if b.phase!="planning": break
		var policy: Dictionary=b.duplicate(true); policy.controller_id="actor.bandit"
		var plan: Dictionary=Round.make_ai(policy,k.content().data)[player]; plan.battle_id=b.id
		var result: Dictionary=F.send(k,"battle_plan",plan)
		if not result.ok: printerr("ESCORT ROUND: ",result); break
		b=Sessions.for_actor(k.snapshot(),player)
	check(b.phase=="result" and b.outcome=="victory","实际战斗胜利且保护对象仍站立："+b.outcome)
	if b.phase!="result": finish(); return
	var task: Dictionary=k.snapshot().tasks.values()[0]
	check(task.status=="active" and task.escort.battle_ids==[b.id],"道路战胜不直接冒充送达或发钱")
	check(F.send(k,"battle_close",{"battle_id":b.id}).ok and finish_travel(k),"关闭战果后继续原行程并抵达东里")
	check(F.send(k,"move_local",{"place_id":"east.home"}).ok,"与车夫一起走到约定贺家")
	task=k.snapshot().tasks.values()[0]
	check(task.status=="ready" and task.escort.arrival_tick>=task.accepted_tick,"同队实际抵达生成可回报事实")
	saved=Codec.decode(Codec.encode(k.snapshot(),1,"escort-world",k.content()),k.content())
	check(saved.ok and k.restore(saved.state).ok,"未回报的到达事实可完整保存并恢复")
	F.send(k,"move_local",{"place_id":"east.entrance"})
	check(F.send(k,"travel",{"destination":"city.west"}).ok and finish_travel(k),"沿真实道路返回青川，不从任务页远程领赏")
	F.send(k,"move_local",{"place_id":"west.tavern"})
	var balance: int=k.snapshot().wallets["wallet.player"].balance
	var funds: int=k.snapshot().wallets["wallet.shen"].balance
	var params: Dictionary={"offer_id":"offer.escort","context":F.context(k,"role.sender")}
	check(F.send(k,"report_offer",params).ok and k.advance_one_tick().ok,"回到实际委托人处办理清算")
	task=k.snapshot().tasks.values()[0]
	check(task.status=="success" and k.snapshot().wallets["wallet.player"].balance==balance+180 and k.snapshot().wallets["wallet.shen"].balance==funds-180,"真实委托资金转移一次，不从战果面板增发")
	check(not F.send(k,"report_offer",params).ok,"不能重复领取护卫报酬")
	check(F.send(k,"wait").ok and k.advance_one_tick().ok,"护卫办妥后仍可继续自由生活")
	check_other_outcomes("withdrawn")
	check_other_outcomes("protected_lost")
	finish()

## 使用正常移动意图分别撤到本方出口或疏于保护，失败不靠改写保护对象生命制造。
func check_other_outcomes(expected: String) -> void:
	var k=contracted()
	var e: Dictionary=reach_conflict(k)
	check(not e.is_empty(),"另一条选择仍从同一真实道路接触进入")
	if e.is_empty(): return
	var start: Dictionary=F.send(k,"encounter_fight",{"encounter_id":e.id})
	check(start.ok,"进入另一种护卫结局的实际冲突")
	if not start.ok: return
	var player: String=k.snapshot().player_actor_id
	var b: Dictionary=Sessions.for_actor(k.snapshot(),player)
	var withdrawal: Array=[[250,250],[145,250],[60,300],[60,360]]
	for turn in range(26):
		if b.phase!="planning": break
		var position: Array=b.units[player].position
		var target: Vector2=Vector2(100,540)
		if expected=="withdrawn": target=Vector2(withdrawal[mini(turn,3)][0],withdrawal[mini(turn,3)][1])
		var current: Vector2=Vector2(position[0],position[1])
		var delta: Vector2=(target-current).limit_length(105)
		var destination: Array=[int(current.x+delta.x),int(current.y+delta.y)]
		var params: Dictionary={"battle_id":b.id,"destination":destination,"heading":0,"action":"withdraw" if expected=="withdrawn" else "wait"}
		var result: Dictionary=F.send(k,"battle_plan",params)
		if not result.ok: printerr("ALTERNATE ROUND: ",expected," ",result); break
		b=Sessions.for_actor(k.snapshot(),player)
	check(b.phase=="result" and b.outcome==expected,"正常行动得到护卫分支："+expected+" 实际："+b.outcome)
	if b.phase!="result": return
	var task: Dictionary=k.snapshot().tasks.values()[0]
	if expected=="withdrawn":
		check(task.status=="active" and task.escort.failure.is_empty(),"安全退避保留未完成护送，不自动判定到达")
		var protected_at_exit: bool=false
		var arena: Dictionary=k.content().data.personal_battlefields[b.battlefield_id]
		for person in b.units.values():
			if not person.protected: continue
			for gate in arena.exits:
				if gate.side==person.side and Vector2(person.position[0]-gate.position[0],person.position[1]-gate.position[1]).length()<=gate.radius: protected_at_exit=true
		check(protected_at_exit,"真实保护者随正常轮次走到本方出口，不因主角独自撤离而传送")
	else:
		check(b.units[player].hp>0 and task.status=="ready" and task.escort.failure=="ProtectedPersonDown","主角尚活不能抵消真实保护对象倒下，合同须回报失败")
		var persisted: Dictionary=Codec.decode(Codec.encode(k.snapshot(),0,"escort-failed",k.content()),k.content())
		check(persisted.ok and k.restore(persisted.state).ok,"保护失败的未交接事实可保存重启")
	F.send(k,"battle_close",{"battle_id":b.id})
	check(finish_travel(k),"胜负之外的结果仍能继续原实际行程")
	if expected!="protected_lost": return
	F.send(k,"travel",{"destination":"city.west"}); finish_travel(k)
	F.send(k,"move_local",{"place_id":"west.tavern"})
	var before: Dictionary=k.snapshot()
	var reported: Dictionary=F.send(k,"report_offer",{"offer_id":"offer.escort","context":F.context(k,"role.sender")})
	check(reported.ok and k.advance_one_tick().ok,"当面报告保护失败并结束合同")
	task=k.snapshot().tasks.values()[0]
	check(task.status=="failed" and k.snapshot().wallets["wallet.player"].balance==before.wallets["wallet.player"].balance and k.snapshot().wallets["wallet.shen"].balance==before.wallets["wallet.shen"].balance,"失败清算不领取成功报酬，委托人的资金仍有真实归属")

## 输出本脚本执行结果，若任何步骤失败则返回非零退出码。
func finish() -> void:
	print("ESCORT_WORLD_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
