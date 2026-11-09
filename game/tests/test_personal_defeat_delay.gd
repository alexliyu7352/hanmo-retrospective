## 普通遭遇败北的两半日延误：正式Alpha命令产生来源，纯查询与真实世界hold接缝分别验证。
extends SceneTree
const Delay = preload("res://core/domains/personal/personal_defeat_delay.gd")
const Kernel = preload("res://core/world_kernel.gd")
const Catalog = preload("res://core/content_catalog.gd")
const F = preload("res://tests/companion_fixture.gd")
const Encounters = preload("res://core/encounter_rules.gd")
const Location = preload("res://core/world_location.gd")
var checks: int = 0
var failures: int = 0
var catalog_path: String = ""

## 使用personal_load正式tiny准备器的独立工程，不读取用户存档或运行GUI。
func _init() -> void:
	call_deferred("run_checks")

## 前置错误立即终止对应路径，任何失败都使进程退出非零。
func check(value: bool, label: String) -> bool:
	checks += 1
	if not value: failures += 1; printerr("FAIL: ", label)
	return value

## 只通过真实当面同行、行旅、道路接触与战斗命令得到败局，不直接改HP、结果或完成日期。
func defeated_world(with_party: bool) -> Dictionary:
	var kernel = Kernel.new()
	var made: Dictionary = kernel.new_game("draft::scenario.alpha_life", catalog_path)
	if not check(made.ok, "正式目录真实新局：" + str(made)): return {}
	if with_party:
		if not check(F.send(kernel, "move_local", {"place_id": "draft::west.tavern"}).ok, "到车夫所在酒肆"): return {}
		if not check(F.send(kernel, "join_party", {"context": F.context(kernel, "draft::role.carter")}).ok, "真实当面约定同行"): return {}
		if not check(F.send(kernel, "move_local", {"place_id": "draft::west.street"}).ok, "全队走回原出口"): return {}
	if not check(F.send(kernel, "travel", {"destination": "draft::village.east"}).ok, "真实开始原行旅"): return {}
	var encounter: Dictionary = {}
	for unused in range(12):
		var state: Dictionary = kernel.snapshot()
		encounter = Encounters.pending_for(state, state.player_actor_id)
		if not encounter.is_empty():
			if kernel.preview("encounter_fight", {"encounter_id": encounter.id}).get("ok", false): break
			if not check(F.send(kernel, "encounter_leave", {"encounter_id": encounter.id}).ok, "正常道别非敌对会面"): return {}
			encounter = {}
		if not check(kernel.advance_one_tick().ok, "按真实半日走到敌对接触"): return {}
	if not check(not encounter.is_empty(), "实际道路接触产生敌对遭遇"): return {}
	if not check(F.send(kernel, "encounter_fight", {"encounter_id": encounter.id}).ok, "正式遭遇命令开战"): return {}
	var battle_id: String = kernel.snapshot().battles.keys()[0]
	for unused in range(101):
		var state: Dictionary = kernel.snapshot(); var battle: Dictionary = state.battles[battle_id]
		if battle.phase != "planning": break
		var unit: Dictionary = battle.units[state.player_actor_id]
		var result: Dictionary = F.send(kernel, "battle_plan", {"battle_id": battle_id, "action": "wait", "destination": unit.position, "heading": unit.heading})
		if not check(result.ok, "真实同步轮次产生败局：" + str(result)): return {}
	var state: Dictionary = kernel.snapshot(); var battle: Dictionary = state.battles[battle_id]
	var data: Dictionary = kernel.content().data
	if not check(battle.outcome == "defeat" and battle.source.kind == "encounter" and data.personal_objectives[battle.objective_id].kind == "skirmish", "确为普通遭遇实际败北"): return {}
	check(battle.completed_tick == state.tick and kernel.preview("advance").get("code") == "BattlePending", "结果期不推进世界或提前消耗延误半日")
	if not check(F.send(kernel, "battle_close", {"battle_id": battle_id}).ok, "正常关闭结果"): return {}
	check(kernel.snapshot().tick == battle.completed_tick, "关闭本身不额外推进世界或复制倒计时")
	return {"kernel": kernel, "battle_id": battle_id, "actor_id": state.player_actor_id, "completed_tick": battle.completed_tick, "with_party": with_party}

## 所有纯查询都核输入不变；返回数组也只是本次派生值，不能写回世界。
func held(state: Dictionary, data: Dictionary) -> Array:
	var before: Dictionary = state.duplicate(true); var definitions: Dictionary = data.duplicate(true)
	var result: Array = Delay.held_travel_actors(state, data)
	check(state == before and data == definitions, "派生hold不修改世界、既有战果或作者定义")
	return result

## 日期、永久来源与实际当前travel参与者共同限定范围，不扩大到切磋、护卫失败、旁人或救援。
func check_leaf(f: Dictionary) -> void:
	var source: Dictionary = f.kernel.snapshot(); var data: Dictionary = f.kernel.content().data
	var actor: String = f.actor_id; var battle_id: String = f.battle_id
	var action_id: String = source.actors[actor].action_id
	for elapsed in [0, 1, 2, 3]:
		var state: Dictionary = source.duplicate(true); state.tick = f.completed_tick + elapsed
		check(held(state, data) == ([actor] if elapsed < 2 else []), "只在T和T+1两个旧tick派生hold：" + str(elapsed))
	var state: Dictionary = source.duplicate(true); state.encounters.clear()
	check(held(state, data) == [actor], "不依赖会被128条裁剪的短会面表")
	var result: Array = held(source, data); result.clear()
	check(held(source, data) == [actor], "修改本次返回值不清除下一次派生期限")
	for kind in ["role", "other"]:
		state = source.duplicate(true); state.battles[battle_id].source.kind = kind
		check(held(state, data).is_empty(), "非普通遭遇来源不受延期：" + kind)
	for kind in ["sparring", "escort"]:
		var definitions: Dictionary = data.duplicate(true)
		definitions.personal_objectives[source.battles[battle_id].objective_id].kind = kind
		check(held(source, definitions).is_empty(), "真实目标定义限定skirmish，不凭ID猜：" + kind)
	for outcome in ["victory", "draw", "withdrawn", "protected_lost"]:
		state = source.duplicate(true); state.battles[battle_id].outcome = outcome
		check(held(state, data).is_empty(), "不把其他战果扩成败北延期：" + outcome)
	for tick in [-1, f.completed_tick + 1, 0.5, "3", [], null]:
		state = source.duplicate(true); state.battles[battle_id].completed_tick = tick
		check(held(state, data).is_empty(), "完成日期坏形状或未来值安全忽略，不制造延期")
	state = source.duplicate(true); state.facts.erase(battle_id)
	check(held(state, data).is_empty(), "无永久结算来源不按孤立battle声明处罚")
	state = source.duplicate(true); state.facts[battle_id].participants.append("not-a-participant")
	check(held(state, data).is_empty(), "未实际参战名单不能冒充原同行")
	state = source.duplicate(true); state.actions[action_id].kind = "rescue"
	check(held(state, data).is_empty(), "救援不是travel，不被延误hold阻止")
	for key in ["id", "kind", "actor_id", "participants"]:
		state = source.duplicate(true); state.actions[action_id][key] = []
		check(held(state, data).is_empty(), "当前行旅读取边界坏类型安全忽略：" + key)
	state = source.duplicate(true); state.battles[battle_id].source.kind = []
	check(held(state, data).is_empty(), "来源类型错误不抛String/Array比较异常")
	state = source.duplicate(true); state.battles[battle_id].outcome = []
	check(held(state, data).is_empty(), "结果类型错误不凭真值推断败北")
	var original: Dictionary = source.battles[battle_id]
	state = source.duplicate(true)
	var duplicate: Dictionary = original.duplicate(true); duplicate.id = "battle:delay-copy"; duplicate.result_id = "result:delay-copy"
	state.battles[duplicate.id] = duplicate
	var fact: Dictionary = state.facts[battle_id].duplicate(true); fact.battle_id = duplicate.id; fact.result_id = duplicate.result_id
	state.facts[duplicate.id] = fact
	check(held(state, data) == [actor], "多个同期来源去重，不叠加时间或多次hold")
	state.tick = f.completed_tick + 2
	check(held(state, data).is_empty(), "多个同刻败局仍在同一T+2到期")

## 只用正式预览与命令验证同行不可在途中换队，改道/返回/暂停恢复不会清除既有完成T。
func check_controls(f: Dictionary) -> void:
	var kernel = f.kernel; var before: Dictionary = kernel.snapshot()
	check(kernel.preview("travel", {"destination": "draft::city.west"}).get("code") == "ActorBusy", "忙碌行旅不能用新travel替换占用")
	if f.with_party:
		check(kernel.preview("dismiss_companion", {"actor_id": "draft::actor.carter"}).get("code") == "ActorBusy", "路途中不能解散原同行来绕过")
		check(kernel.preview("travel", {"destination": "draft::city.west"}, "draft::actor.carter").get("code") == "NotPartyLeader", "跟随者不能独立提交新行旅")
	check(F.send(kernel, "pause_travel").ok and F.send(kernel, "resume_travel").ok, "保留原暂停及恢复入口")
	if f.with_party:
		var action_id: String = before.actors[f.actor_id].action_id
		check(F.send(kernel, "reroute", {"destination": "draft::city.west"}).ok and F.send(kernel, "return").ok, "正常改道与返回仍允许选择原路线")
		check(kernel.snapshot().actors[f.actor_id].action_id == action_id, "真实改道复用原行动身份")
	check(kernel.snapshot().tick == f.completed_tick and held(kernel.snapshot(), kernel.content().data) == [f.actor_id], "零耗时控制不能冲掉原败局日期或派生hold")

## 中央preflight接线后验证两个完整世界步；本叶不替测试写hook，不借手工传hold制造通过。
func check_world_steps(f: Dictionary) -> void:
	var kernel = f.kernel
	var before: Dictionary = kernel.snapshot(); var actor: String = f.actor_id
	var action_id: String = before.actors[actor].action_id
	var action: Dictionary = before.actions[action_id].duplicate(true)
	var payer: String = action.params.payer_wallet_id
	for elapsed in [1, 2]:
		var advanced: Dictionary = F.send(kernel, "travel_wait") if f.with_party and elapsed == 1 else kernel.advance_one_tick()
		if not check(advanced.ok, "唯一世界正常推进延误半日：" + str(advanced)): return
		var after: Dictionary = kernel.snapshot()
		check(after.tick == f.completed_tick + elapsed, "每次只推进一个真实半日：" + str(elapsed))
		if not check(after.actors[actor].location == before.actors[actor].location, "败方在第" + str(elapsed) + "个半日仍停在真实战后位置"): return
		check(after.actions.has(action_id) and after.actions[action_id].remaining_ticks == action.remaining_ticks, "延误不消耗原行程剩余路程")
		check(after.wallets[payer].balance == before.wallets[payer].balance, "没有移动便不产生原行旅费用，也不额外罚款")
		for participant in action.participants:
			check(Location.resolve(after, participant) == Location.resolve(before, participant) and after.actors[participant].fatigue == before.actors[participant].fatigue, "同行实际位置和行旅疲劳随原移动单位一起保持：" + participant)
		if f.with_party and elapsed == 1:
			check(after.actions[action_id].params.paused and F.send(kernel, "resume_travel").ok, "travel_wait保留用户暂停语义，正常恢复不阻止世界等待")
	check(held(kernel.snapshot(), kernel.content().data).is_empty(), "T+2后自然解除派生hold，不写新完成事实")
	check(F.send(kernel, "pause_travel").ok and kernel.preview("advance").get("code") == "TravelPaused", "期限结束不覆盖用户主动暂停")
	if not check(F.send(kernel, "resume_travel").ok and kernel.advance_one_tick().ok, "到期恢复后第三个真实半日能继续原路"): return
	var resumed: Dictionary = kernel.snapshot()
	check(resumed.tick == f.completed_tick + 3 and resumed.actors[actor].location != before.actors[actor].location, "第三步才恢复实际行走，其他世界已正常经过两半日")
	check(resumed.wallets[payer].balance == before.wallets[payer].balance - action.params.money_per_tick, "恢复只收本次实际行走的原费用")

## 默认验证完整短链；leaf-only仅供中央接线前核新纯叶，必须明确不声称生产hold已经激活。
func run_checks() -> void:
	var installed: Dictionary = Catalog.installed_catalogs()
	if not check(installed.get("ok", false) and installed.get("catalogs", []).size() == 1, "需要正式tiny准备器的单目录独立工程"): finish(); return
	catalog_path = installed.catalogs[0].catalog_path
	var leaf_only: bool = "--leaf-only" in OS.get_cmdline_user_args()
	for with_party in [false, true]:
		var f: Dictionary = defeated_world(with_party)
		if f.is_empty(): continue
		if not with_party: check_leaf(f)
		check_controls(f)
		if not leaf_only: check_world_steps(f)
	print("PERSONAL_DEFEAT_DELAY_INTEGRATION=", "NOT_RUN" if leaf_only else "RUN")
	finish()

## 保存本门真实结果；旧档迁移、完整存读、GUI与其他世界机制均不在本门结论中。
func finish() -> void:
	print("PERSONAL_DEFEAT_DELAY_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
