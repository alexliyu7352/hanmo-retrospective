## 用正式世界入口验证同行、唯一位置、占用、费用、解除和坏档拒绝。
extends SceneTree
const F = preload("res://tests/companion_fixture.gd")
const Codec = preload("res://core/save_codec.gd")
var checks: int=0
var failures: int=0
## 等待引擎就绪后再建立真实场景数据。
func _init() -> void:
	call_deferred("run_checks")
## 记录可观察断言，任何失败都使进程返回非零。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
## 完成同行全流程，不用给UI临时塞入分身替代位置机制。
func run_checks() -> void:
	check(ResourceLoader.exists("res://core/party_rules.gd"),"存在同行领域规则")
	if failures: finish(); return
	var location = load("res://core/world_location.gd")
	var k=F.fresh()
	check(not k.snapshot().is_empty(),"同行测试内容可由正式引擎装载")
	if failures: finish(); return
	var before: Dictionary=k.snapshot()
	check(not F.send(k,"join_party",{"context":F.context(k)}).ok and k.snapshot()==before,"不能远程邀请")
	check(F.send(k,"move_local",{"place_id":"west.tavern"}).ok,"亲自进入酒肆")
	var invited: Dictionary=F.send(k,"join_party",{"context":F.context(k)})
	check(invited.ok,"当面同意后建立同行关系")
	if not invited.ok: printerr(invited); finish(); return
	var s: Dictionary=k.snapshot(); var party: Dictionary=s.parties.values()[0]
	check(party.members==[s.player_actor_id,"actor.carter"],"队伍绑定实际人物")
	var conditions=load("res://core/condition_rules.gd")
	check(conditions.evaluate(s,s.player_actor_id,{"op":"party_member","actor_id":"actor.carter","equals":true}),"对白可以读取真实同行状态")
	check(conditions.evaluate(s,s.player_actor_id,{"op":"party_member","actor_id":"actor.chen","equals":false}),"未同行人物不会套用熟伴对白")
	check(s.actors["actor.carter"].location=={"kind":"Following","party_id":party.id},"同伴不保存第二份地点")
	check(location.resolve(s,"actor.carter")==s.actors[s.player_actor_id].location,"读取同一权威位置")
	check(not F.send(k,"move_local",{"place_id":"west.street"},"actor.carter").ok,"同伴不能独立移动")
	check(not F.send(k,"join_party",{"context":F.context(k)}).ok,"不重复加入同一人物")
	check(F.send(k,"move_local",{"place_id":"west.street"}).ok,"领队正常出门")
	check(location.resolve(k.snapshot(),"actor.carter").place_id=="west.street","同伴同步出门而非留在旧岗位")
	var quote: Dictionary=k.preview("travel",{"destination":"village.east"})
	check(quote.ok and quote.cost==28,"报价含两半日真实同行费用")
	var bag: Dictionary=k.snapshot().inventories["bag:actor.carter"].duplicate(true)
	check(F.send(k,"travel",{"destination":"village.east"}).ok,"同行实际启程")
	s=k.snapshot()
	check(s.actors["actor.carter"].action_id==s.actors[s.player_actor_id].action_id,"所有成员被同一旅行占用")
	check(k.advance_one_tick().ok,"同行推进一次半日")
	s=k.snapshot()
	check(s.wallets["wallet.player"].balance==786 and s.actors["actor.carter"].fatigue==5,"只扣一次组合路费，各自记录疲劳")
	check(not F.send(k,"dismiss_companion",{"actor_id":"actor.carter"}).ok,"不在路上丢下同伴")
	var text: String=Codec.encode(s,1,"party",k.content())
	var copy=F.fresh(); var decoded: Dictionary=Codec.decode(text,copy.content())
	check(not text.is_empty() and decoded.ok and copy.restore(decoded.state).ok,"同行中点保存恢复")
	check(k.advance_one_tick().ok and copy.advance_one_tick().ok and k.snapshot()==copy.snapshot(),"读档后相同动作得到相同费用与位置")
	check(F.send(k,"dismiss_companion",{"actor_id":"actor.carter"}).ok,"抵达后当面解散")
	s=k.snapshot()
	check(s.parties.is_empty() and s.actors["actor.carter"].location.place_id=="east.entrance","解散落在真实目的地")
	check(s.inventories["bag:actor.carter"]==bag,"私人财物不因同行被复制或归玩家")
	var bad: Dictionary=s.duplicate(true); bad.actors["actor.carter"].location={"kind":"Following","party_id":"missing"}
	check(not k.restore(bad).ok and k.snapshot()==s,"坏队伍引用不能覆盖好世界")
	finish()
## 输出断言数量与退出码，不能以引擎正常启动冒充机制通过。
func finish() -> void:
	print("PARTY_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
