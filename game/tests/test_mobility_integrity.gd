## 检验损坏的同行、会面和行动引用被拒绝，而不是让读档形成远程会话。
extends SceneTree
const F=preload("res://tests/companion_fixture.gd")
const Encounters=preload("res://core/encounter_rules.gd")
var checks: int=0
var failures: int=0
## 在引擎准备完成后测试实际世界校验入口。
func _init() -> void:
	call_deferred("run_checks")
## 记录断言与失败，不在错误后把当前世界当作已经恢复。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
## 使用独立坏快照验证拒绝路径，原有效世界必须保持完整。
func run_checks() -> void:
	var k=F.fresh(true)
	check(F.send(k,"travel",{"destination":"village.east"}).ok,"实际启程")
	check(k.advance_one_tick().ok and k.advance_one_tick().ok,"实际相遇")
	var good: Dictionary=k.snapshot()
	var e: Dictionary=Encounters.pending_for(good,good.player_actor_id)
	var bad: Dictionary=good.duplicate(true)
	bad.encounters[e.id].anchors["actor.walker"].progress=5
	bad.actors["actor.walker"].location.progress=5
	check(not k.restore(bad).ok and k.snapshot()==good,"相互引用一致但实际远离的锚点不能形成会面")
	k.restore(good)
	bad=good.duplicate(true)
	bad.actions[e.action_ids[e.leaders[0]]]=17
	check(not k.restore(bad).ok and k.snapshot()==good,"先拒绝错误行动类型，不能在检查队伍时崩溃")
	check(k.restore(good).ok,"有效锚点仍然可恢复")
	var party=F.fresh()
	F.send(party,"move_local",{"place_id":"west.tavern"})
	check(F.send(party,"join_party",{"context":F.context(party)}).ok,"正常当面邀请")
	F.send(party,"move_local",{"place_id":"west.street"})
	check(F.send(party,"travel",{"destination":"village.east"}).ok,"同行真实出发")
	good=party.snapshot(); bad=good.duplicate(true)
	bad.actions[bad.actors[bad.player_actor_id].action_id]={}
	check(not party.restore(bad).ok and party.snapshot()==good,"同行中的不完整行动被结构化拒绝")
	bad=good.duplicate(true)
	bad.parties.values()[0].members.append("actor.carter")
	check(not party.restore(bad).ok and party.snapshot()==good,"同一成员不能占用两个队伍槽")
	var host=F.fresh(false,"role.host")
	F.send(host,"move_local",{"place_id":"west.tavern"})
	check(F.send(host,"join_party",{"context":F.context(host,"role.host")}).ok,"另一岗位的合法同行也能建立")
	check(F.send(host,"service",{"activity":"work","context":F.context(host,"role.host")}).ok,"同伴在自己的实际岗位办理服务仍使用统一位置")
	check(host.advance_one_tick().ok,"实际服务能完成并释放双方占用")
	print("MOBILITY_INTEGRITY_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
