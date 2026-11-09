## 验证多个主体只共享一次半日推进；暂停一人的旅行不暂停另一人的已有行动。
extends SceneTree
const Kernel = preload("res://core/world_kernel.gd")
var checks: int = 0
var failures: int = 0
## 等待场景树可用后执行测试。
func _init() -> void:
	call_deferred("_run")
## 对失败给出中文定位并保留非零进程结果。
func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok: failures += 1; printerr("FAIL: ",label)
## 通过公开命令入口指定操作主体，不直接修改真实世界字段。
func cmd(k, who: String, kind: String, params: Dictionary = {}) -> Dictionary:
	var s: Dictionary = k.snapshot()
	return k.submit({"actor_id":who,"kind":kind,"params":params,"request_seq":s.request_sequences[who]+1,"expected_revision":s.revision})
## 同一半日同时处理两位行旅人，等待、去重和坏存档也不能改变这一口径。
func _run() -> void:
	var k = Kernel.new(); check(k.new_game().ok,"原生活内容可运行")
	var funded: Dictionary = k.snapshot()
	funded.wallets[funded.actors["actor.he"].wallet_id].balance = 80
	check(k.restore(funded).ok,"为独立NPC测试配置真实初始路费")
	var hero: String = k.snapshot().player_actor_id
	check(cmd(k,"actor.he","move_local",{"place_id":"east.entrance"}).ok,"另一人物从自宅来到道路入口")
	check(cmd(k,hero,"travel",{"destination":"village.east"}).ok,"主角启程")
	check(cmd(k,"actor.he","travel",{"destination":"city.west"}).ok,"另一人物走同一道路服务")
	var before: Dictionary = k.snapshot()
	check(cmd(k,hero,"advance").ok,"一次世界推进")
	var middle: Dictionary = k.snapshot()
	check(middle.tick == before.tick+1,"两条行程没有让时间增加两次")
	check(middle.actors[hero].location.progress == 100 and middle.actors["actor.he"].location.progress == 100,"两人都有真实进度")
	check(cmd(k,hero,"pause_travel").ok,"暂停主角")
	before = k.snapshot()
	var request: Dictionary = {"actor_id":hero,"kind":"travel_wait","params":{},"request_seq":before.request_sequences[hero]+1,"expected_revision":before.revision}
	var result: Dictionary = k.submit(request)
	check(result.ok,"途中等待完成")
	var after: Dictionary = k.snapshot()
	check(after.tick == before.tick+1 and after.actors[hero].location == before.actors[hero].location,"本人等待而不移动")
	check(after.actors["actor.he"].location.get("settlement_id") == "city.west","另一人物仍到达目的地")
	check(after.wallets[after.actors[hero].wallet_id] == before.wallets[before.actors[hero].wallet_id],"等待不重复收主角路费")
	check(k.submit(request) == result and k.snapshot() == after,"重复等待请求不重算世界")
	var bad: Dictionary = after.duplicate(true)
	bad.actions[bad.actors[hero].action_id].params.paused = "yes"
	check(not k.restore(bad).ok and k.snapshot() == after,"拒绝错误暂停类型且保留原局")
	var legacy: Dictionary = after.duplicate(true)
	legacy.actions[legacy.actors[hero].action_id].params.erase("paused")
	legacy.actions[legacy.actors[hero].action_id].params.erase("reverse_first")
	check(k.restore(legacy).ok and k.preview("advance").ok,"旧行程缺少新增可选字段仍能续走")
	print("WORLD_STEP_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures == 0 else 1)
