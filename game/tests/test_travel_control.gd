## 验证行程暂停、等待和改道均通过原世界入口，不在界面另算位置或费用。
extends SceneTree
const Kernel = preload("res://core/world_kernel.gd")
var checks: int = 0
var failures: int = 0
## 引擎就绪后执行真实规则测试。
func _init() -> void:
	call_deferred("_run")
## 累积失败并保持非零退出，缺失机制不能被解释为测试跳过。
func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		printerr("FAIL: ", label)
## 为指定人物提交一条带版本和序号的命令。
func cmd(k, kind: String, params: Dictionary = {}, who: String = "") -> Dictionary:
	var s: Dictionary = k.snapshot()
	if who.is_empty(): who = s.player_actor_id
	return k.submit({"actor_id":who,"kind":kind,"params":params,"expected_revision":s.revision,"request_seq":s.request_sequences[who]+1})
## 用已有两地剧本证明新控制能恢复旧行程，并且其他人物照常耗时。
func _run() -> void:
	var k = Kernel.new()
	check(k.new_game().ok, "已有内容仍可启动")
	check(cmd(k,"travel",{"destination":"village.east"}).ok, "从真实城门出发")
	check(cmd(k,"advance").ok, "实际走过半日")
	var start: Dictionary = k.snapshot()
	check(start.actors[start.player_actor_id].location.progress == 100, "已走到道路中间")
	var paused: Dictionary = cmd(k,"pause_travel")
	check(paused.ok, "存在可保存的暂停状态")
	if paused.ok:
		var s: Dictionary = k.snapshot()
		check(s.tick == start.tick and s.actors[s.player_actor_id].location == start.actors[s.player_actor_id].location, "暂停不退费、不推进时间或移动")
		check(not k.preview("advance").ok, "暂停不接受意外行走指令")
		var loaded = Kernel.new(); loaded.new_game()
		check(loaded.restore(s).ok and not loaded.preview("advance").ok, "读档保留暂停")
		check(cmd(loaded,"travel_wait").ok, "途中可等待半日")
		var waited: Dictionary = loaded.snapshot()
		check(waited.tick == s.tick+1 and waited.wallets == s.wallets, "等待只推进一次时钟而不重收路费")
		check(waited.actors[waited.player_actor_id].location == s.actors[s.player_actor_id].location, "等待保留当前位置")
		check(cmd(loaded,"resume_travel").ok and cmd(loaded,"advance").ok, "等待后继续并抵达")
		check(loaded.snapshot().actors[s.player_actor_id].location.settlement_id == "village.east", "到达真实目的地")
	var before: Dictionary = k.snapshot()
	var proposal: Dictionary = k.preview("reroute",{"destination":"city.west"})
	check(proposal.ok, "道路中间可以规划返回")
	check(k.snapshot() == before, "改道预览不修改状态或随机数")
	if proposal.ok:
		check(cmd(k,"reroute",{"destination":"city.west"}).ok, "确认改道")
		check(k.snapshot().actors[before.player_actor_id].location == before.actors[before.player_actor_id].location, "改道不先传送到端点")
		check(cmd(k,"advance").ok, "按已走路段真正退回")
		var done: Dictionary = k.snapshot()
		check(done.actors[done.player_actor_id].location.settlement_id == "city.west", "已退回原城")
		check(done.wallets[done.actors[done.player_actor_id].wallet_id].balance == 784, "去返各收8钱、不退旧费用")
	var invalid = Kernel.new(); invalid.new_game()
	before = invalid.snapshot()
	check(not invalid.preview("travel",{"destination":"village.east","via":["missing"]}).ok, "未知途经点不能被静默忽略")
	check(invalid.snapshot() == before, "错误计划无副作用")
	print("TRAVEL_CONTROL_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures == 0 else 1)
