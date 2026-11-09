## 验证路遇保留真实旅行占用、对白和存档，解决后只恢复原旅程一次。
extends SceneTree
const F=preload("res://tests/companion_fixture.gd")
const Codec=preload("res://core/save_codec.gd")
var checks: int=0
var failures: int=0
## 引擎初始化完成后运行完整遭遇流程。
func _init() -> void:
	call_deferred("run_checks")
## 记录真正执行的断言，失败不能被正常退出掩盖。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
## 从地图相遇进入同一对白核心，再通过合法道别回到原行程。
func run_checks() -> void:
	check(ResourceLoader.exists("res://core/encounter_rules.gd"),"存在可恢复的道路会话")
	if failures: finish(); return
	var e_rules=load("res://core/encounter_rules.gd")
	var k=F.fresh(true)
	check(not k.snapshot().is_empty(),"路遇内容可运行")
	if failures: finish(); return
	check(F.send(k,"travel",{"destination":"village.east"}).ok,"实际出发")
	check(k.advance_one_tick().ok and k.advance_one_tick().ok,"双方行旅在同一世界推进")
	var s: Dictionary=k.snapshot(); var encounter: Dictionary=e_rules.pending_for(s,s.player_actor_id)
	check(not encounter.is_empty(),"真实接触创建待处理会面")
	if encounter.is_empty(): finish(); return
	var id: String=encounter.id
	check(s.tick==2 and s.wallets["wallet.player"].balance==784 and s.wallets["wallet.walker"].balance==792,"世界与各自费用恰好结算一次")
	check(not F.send(k,"advance").ok and k.snapshot()==s,"待处理会面阻止世界偷偷继续")
	check(F.send(k,"encounter_talk",{"encounter_id":id}).ok,"道路上的实际旅人可以交谈")
	var view: Dictionary=k.interaction_view()
	check(view.phase=="say" and view.context.provider_actor_id=="actor.walker","复用原对白运行器并绑定实际说话人")
	var trade: Dictionary={"shop_id":"shop.grain","context":view.context.duplicate(true)}
	check(not k.preview("open_trade",trade).ok,"路上交谈不能冒用机构交易权限")
	s=k.snapshot()
	var encoded: String=Codec.encode(s,1,"road",k.content())
	var clone=F.fresh(true); var decoded: Dictionary=Codec.decode(encoded,clone.content())
	check(not encoded.is_empty() and decoded.ok and clone.restore(decoded.state).ok,"会面中保存并恢复旅行、锚点和对白")
	check(F.send(k,"dialogue_next").ok and F.send(clone,"dialogue_next").ok,"恢复后继续原句，不重新进入奖励脚本")
	check(F.send(k,"encounter_leave",{"encounter_id":id}).ok and F.send(clone,"encounter_leave",{"encounter_id":id}).ok,"明确道别恢复原来行程")
	s=k.snapshot()
	check(s==clone.snapshot() and e_rules.pending_for(s,s.player_actor_id).is_empty(),"相同操作得到相同世界且会面结束")
	check(not F.send(k,"encounter_leave",{"encounter_id":id}).ok and k.snapshot()==s,"已结束会面不能新序号重复结算")
	check(k.advance_one_tick().ok and clone.advance_one_tick().ok and k.snapshot()==clone.snapshot(),"结束后正常赶路不重复相遇或收费")
	check(k.snapshot().actors[k.snapshot().player_actor_id].location.get("settlement_id")=="village.east","到达真实村口而非返回原城")
	var bad: Dictionary=s.duplicate(true)
	bad.encounters[id].status="pending"
	bad.encounters[id].anchors["actor.walker"].progress=0
	check(not k.restore(bad).ok,"损坏会面锚点不能恢复为可交谈对象")
	finish()
## 独立输出范围，不将规则断言伪称鼠标通关。
func finish() -> void:
	print("ENCOUNTER_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
