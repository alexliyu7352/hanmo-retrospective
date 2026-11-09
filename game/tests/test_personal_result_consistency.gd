## 个人战冻结终态一致性：从正式目录与真实命令生成结果，读档不能仅凭协同改写的结果和事实接受假胜负。
extends SceneTree
const Kernel = preload("res://core/world_kernel.gd")
const Catalog = preload("res://core/content_catalog.gd")
const Codec = preload("res://core/save_codec.gd")
const Strict = preload("res://core/strict_json.gd")
const F = preload("res://tests/companion_fixture.gd")
const Round = preload("res://core/personal_round.gd")
const Objectives = preload("res://core/domains/personal/personal_objectives.gd")
const Withdrawal = preload("res://core/domains/personal/personal_withdrawal.gd")
const Fixture = preload("res://tests/test_personal_directional_guard.gd")
const ExitFixture = preload("res://tests/test_personal_npc_withdrawal.gd")
const EscortFixture = preload("res://tests/test_personal_escort_withdrawal.gd")
const EscortExitFixture = preload("res://tests/test_personal_escort_exit.gd")
var checks: int = 0
var failures: int = 0

## 仅在已有正式tiny准备器的独立工程执行，不读取用户存档或运行界面。
func _init() -> void:
	call_deferred("run_checks")

## 前置失败立即停下对应路径，任何断言失败都计入非零退出。
func check(value: bool, label: String) -> bool:
	checks += 1
	if not value: failures += 1; printerr("FAIL: ", label)
	return value

## 只替换实际存档正文并重算物理校验值；目录、模块版本和封装身份保持原真值。
func replace_payload(encoded: String, state: Dictionary) -> String:
	var envelope: Dictionary = Strict.parse_text(encoded, Codec.MAX_BYTES).value
	envelope.payload_json = JSON.stringify(state, "", true, true)
	envelope.payload_length = envelope.payload_json.to_utf8_buffer().size()
	envelope.payload_sha256 = envelope.payload_json.sha256_text()
	return JSON.stringify(envelope, "", true, true)

## 篡改结果、永久事实与对应展示事件，使原自洽校验不能代替真实终态检查。
func forged_outcome(state: Dictionary, battle_id: String, outcome: String) -> Dictionary:
	var forged: Dictionary = state.duplicate(true)
	forged.battles[battle_id].outcome = outcome
	forged.facts[battle_id].outcome = outcome
	for event in forged.battles[battle_id].last_events:
		if event.get("action") == "outcome": event.code = outcome
	return forged

## 正常开战和逐轮等待产生原规则真实终局，再通过完整Kernel与v2解码检验协同篡改。
func check_real_restore() -> void:
	var installed: Dictionary = Catalog.installed_catalogs()
	if not check(installed.get("ok", false) and installed.get("catalogs", []).size() == 1, "独立正式目录可读取"): return
	var kernel = Kernel.new()
	var made: Dictionary = kernel.new_game("draft::scenario.alpha_life", installed.catalogs[0].catalog_path)
	if not check(made.ok, "正式Alpha作者内容经真实Kernel初始化：" + str(made)): return
	if not check(F.send(kernel, "move_local", {"place_id": "draft::west.training"}).ok, "本人实际走到武馆"): return
	var started: Dictionary = F.send(kernel, "duel_start", {"context": F.context(kernel, "draft::role.wei")})
	if not check(started.ok, "真实当面切磋命令建立会话：" + str(started)): return
	var planning_context: Dictionary = kernel.save_context()
	var battle_id: String = kernel.snapshot().battles.keys()[0]
	for unused in range(101):
		var state: Dictionary = kernel.snapshot()
		var battle: Dictionary = state.battles[battle_id]
		if battle.phase != "planning": break
		var unit: Dictionary = battle.units[state.player_actor_id]
		var result: Dictionary = F.send(kernel, "battle_plan", {"battle_id": battle_id, "action": "wait", "destination": unit.position.duplicate(), "heading": unit.heading})
		if not check(result.ok, "真实同步轮次提交：" + str(result)): return
	var context: Dictionary = kernel.save_context()
	var finished: Dictionary = context.state.battles[battle_id]
	if not check(finished.phase == "result" and finished.applied, "真实轮次产生一次永久终局"): return
	print("REAL_PERSONAL_RESULT outcome=", finished.outcome, " round=", finished.round)
	var encoded: String = Codec.encode(context.state, 1, "personal-result-consistency", context.catalog, context.registry)
	if not check(not encoded.is_empty() and Codec.decode(encoded, context.catalog, context.registry).get("ok", false), "未经篡改的真实终局可完整保存解码"): return
	var forged: Dictionary = forged_outcome(context.state, battle_id, "victory" if finished.outcome != "victory" else "defeat")
	var fresh = Kernel.new()
	if not check(fresh.restore_with_catalog(context.state, context.catalog, context.registry).ok, "冷Kernel可恢复合法终局"): return
	var before: Dictionary = fresh.snapshot()
	var restored: Dictionary = fresh.restore(forged)
	print("FORGED_PERSONAL_RESTORE ", restored)
	check(restored.get("code") == "PersonalOutcomeMismatch", "协同篡改battle、fact及事件仍须被真实restore拒绝")
	check(fresh.snapshot() == before, "拒绝错误终局保持整个已加载世界不变")
	var decoded: Dictionary = Codec.decode(replace_payload(encoded, forged), context.catalog, context.registry)
	print("FORGED_PERSONAL_DECODE ok=", decoded.get("ok", false), " code=", decoded.get("code", ""))
	check(decoded.get("code") == "PersonalOutcomeMismatch", "校验和正确的v2正文仍须通过真实终态语义门")
	if not check(F.send(kernel, "battle_close", {"battle_id": battle_id}).ok, "真实关闭合法结果仍只释放原会话"): return
	context = kernel.save_context()
	check(context.state.battles[battle_id].phase == "closed" and context.state.facts == before.facts, "关闭不重算冻结结果或改永久事实")
	if not check(fresh.restore_with_catalog(context.state, context.catalog, context.registry).ok, "已关闭合法历史可冷恢复"): return
	before = fresh.snapshot()
	forged = forged_outcome(context.state, battle_id, "victory" if finished.outcome != "victory" else "defeat")
	check(fresh.restore(forged).get("code") == "PersonalOutcomeMismatch" and fresh.snapshot() == before, "关闭后协同改写同样拒绝，不能靠释放占用跳过结果校验")
	check(F.send(kernel, "move_local", {"place_id": "draft::west.street"}).ok, "结束后仍沿真实命令继续原世界移动")
	context = kernel.save_context()
	check(fresh.restore_with_catalog(context.state, context.catalog, context.registry).ok and fresh.snapshot() == context.state, "后来世界位置变化不追改或拒绝合法历史")
	check_real_withdrawal_restore(planning_context, battle_id)

## 从同一真实规划存档走到本方出口；合法新退出和缺可选标记的旧形状均通过完整Kernel存读。
func check_real_withdrawal_restore(context: Dictionary, battle_id: String) -> void:
	var kernel = Kernel.new()
	if not check(kernel.restore_with_catalog(context.state, context.catalog, context.registry).ok, "另一Kernel沿同一真实规划点继续撤离"): return
	for unused in range(context.catalog.data.personal_objectives[context.state.battles[battle_id].objective_id].max_rounds + 1):
		var state: Dictionary = kernel.snapshot(); var battle: Dictionary = state.battles[battle_id]
		if battle.phase != "planning": break
		var actor_id: String = state.player_actor_id
		var stride: int = Round.movement_distance(battle, context.catalog.data, actor_id, "", "withdraw")
		var destination: Array = Withdrawal.destination(battle, context.catalog.data, actor_id, stride)
		var result: Dictionary = F.send(kernel, "battle_plan", {"battle_id": battle_id, "action": "withdraw", "destination": destination, "heading": battle.units[actor_id].heading})
		if not check(result.ok, "正式命令按合法有限路径逐轮退出：" + str(result)): return
	var saved: Dictionary = kernel.save_context()
	if not check(saved.state.battles[battle_id].outcome == "withdrawn", "真实玩家确实安全到达自己的出口"): return
	var encoded: String = Codec.encode(saved.state, 2, "personal-withdrawal", saved.catalog, saved.registry)
	check(not encoded.is_empty() and Codec.decode(encoded, saved.catalog, saved.registry).get("ok", false), "当前退出标记与永久事实可完整v2存读")
	var legacy: Dictionary = saved.state.duplicate(true)
	for unit in legacy.battles[battle_id].units.values(): unit.erase("withdrawn_round")
	legacy.facts[battle_id].erase("withdrawn")
	legacy.battles[battle_id].last_events = []
	var fresh = Kernel.new()
	check(fresh.restore_with_catalog(legacy, saved.catalog, saved.registry).ok and fresh.snapshot() == legacy, "真实出口历史缺少后来可选退出标记与事件时仍能恢复")
	encoded = Codec.encode(legacy, 3, "personal-legacy-withdrawal", saved.catalog, saved.registry)
	check(not encoded.is_empty() and Codec.decode(encoded, saved.catalog, saved.registry).get("ok", false), "保留合法旧字段形状，不借本修复强制迁移存档")

## 每次只读终态查询都核完整输入；不以校验为名补字段、重跑AI或修改历史事件。
func validate_frozen(battle: Dictionary, data: Dictionary) -> Dictionary:
	var before: Dictionary = {"battle": battle.duplicate(true), "data": data.duplicate(true)}
	var result: Dictionary = Objectives.validate_result(battle, data.personal_objectives[battle.objective_id], data.personal_battlefields[battle.battlefield_id])
	check(battle == before.battle and data == before.data, "终态校验保留冻结战场、作者定义和事件原字节")
	return result

## 用真实Round输出比较结果期与关闭期；不能用新增校验制造新胜负或抹去原超时规则。
func check_terminal(battle: Dictionary, data: Dictionary, plan: Dictionary, expected: String) -> Dictionary:
	var original: Dictionary = battle.duplicate(true)
	var resolved: Dictionary = Round.resolve(battle, data, plan)
	if not check(resolved.get("ok", false) and battle == original, "原同步规则成功且输入不变：" + expected): return {}
	var terminal: Dictionary = resolved.battle
	if not check(terminal.phase == "result" and terminal.outcome == expected, "原终态和优先级不变：" + expected): return {}
	check(validate_frozen(terminal, data).ok, "实际Round终局通过一致性校验：" + expected)
	var closed: Dictionary = terminal.duplicate(true); closed.phase = "closed"
	check(validate_frozen(closed, data).ok, "同一冻结结果关闭后仍合法：" + expected)
	return terminal

## 原10个百分点按精确分数、当前伤害后HP与目标种类裁决；缺少新模型字段的历史也保持合法。
func check_timeout_and_priority() -> void:
	for row in [[60, 100, 50, 100, "sparring", "victory"], [50, 100, 60, 100, "sparring", "defeat"], [59, 100, 50, 100, "sparring", "draw"], [29, 60, 23, 60, "sparring", "victory"], [30, 61, 24, 61, "sparring", "draw"], [99, 100, 1, 100, "skirmish", "draw"], [0, 100, 0, 100, "sparring", "defeat"], [1, 100, 0, 100, "sparring", "victory"]]:
		var data: Dictionary = Fixture.data(); data.personal_objectives.spar.kind = row[4]
		var battle: Dictionary = Fixture.battle([310, 200]); battle.round = 10
		battle.units.defender.hp = row[0]; battle.units.defender.initial_hp = row[1]
		battle.units.attacker.hp = row[2]; battle.units.attacker.initial_hp = row[3]
		battle.ai_plans.attacker = Fixture.plan(battle.units.attacker, "wait")
		var terminal: Dictionary = check_terminal(battle, data, Fixture.plan(battle.units.defender, "wait"), row[5])
		if terminal.is_empty(): continue
		for wrong in ["victory", "defeat", "draw", "protected_lost", "withdrawn"]:
			if wrong == row[5]: continue
			var forged: Dictionary = terminal.duplicate(true); forged.outcome = wrong
			check(validate_frozen(forged, data).get("code") == "PersonalOutcomeMismatch", "同一冻结比例或倒地事实不能换成其他终局：" + row[5] + "→" + wrong)
	var data: Dictionary = Fixture.data()
	var battle: Dictionary = Fixture.battle([310, 200])
	battle.units["protected"] = Fixture.unit("protected", 0, [100, 100], 0)
	battle.units["protected"].protected = true; battle.units["protected"].hp = 0; battle.units.attacker.hp = 0
	var terminal: Dictionary = check_terminal(battle, data, Fixture.plan(battle.units.defender, "wait"), "protected_lost")
	if not terminal.is_empty():
		terminal.outcome = "victory"
		check(validate_frozen(terminal, data).get("code") == "PersonalOutcomeMismatch", "歼敌不能覆盖同轮保护对象失败")
	battle = Fixture.battle([310, 200]); battle.phase = "result"; battle.outcome = "draw"
	check(validate_frozen(battle, data).get("code") == "PersonalOutcomeMismatch", "双方尚在场且未到上限不能提前伪造平局")
	battle.phase = "planning"; battle.outcome = ""
	check(validate_frozen(battle, data).ok, "规划期仍由原规划校验负责，不强制已有结果")

## 同轮双方安全退出仍是玩家撤离；旧无标记退出、NPC先离与真正出口失败分别保留原边界。
func check_withdrawal_history() -> void:
	var data: Dictionary = ExitFixture.content()
	var battle: Dictionary = ExitFixture.battle(false); ExitFixture.commit(battle, data)
	var terminal: Dictionary = check_terminal(battle, data, Fixture.plan(battle.units.defender, "withdraw"), "withdrawn")
	if not terminal.is_empty():
		check(terminal.units.attacker.get("withdrawn_round") == terminal.round, "正例确实包括同轮NPC退出，不把此时歼敌优先级套在玩家退出前")
		var legacy: Dictionary = terminal.duplicate(true)
		for unit in legacy.units.values(): unit.erase("withdrawn_round")
		legacy.last_events = []
		check(validate_frozen(legacy, data).ok, "旧无退出标记或专门事件的合法出口历史不被强制迁移")
		var forged: Dictionary = legacy.duplicate(true); forged.units.defender.position = [100, 100]
		check(validate_frozen(forged, data).get("code") == "PersonalOutcomeMismatch", "没有真实到本方出口不能声明旧式退出")
		forged = legacy.duplicate(true); forged.last_events = [{"actor_id": "defender", "action": "withdraw", "code": "WithdrawalMeleeHit"}]
		check(validate_frozen(forged, data).get("code") == "PersonalOutcomeMismatch", "已保存的近战缠斗失败不能伪装安全退出")
		forged = terminal.duplicate(true); forged.units.attacker.hp = 0; forged.units.attacker.erase("withdrawn_round")
		check(validate_frozen(forged, data).get("code") == "PersonalOutcomeMismatch", "敌人全倒地时原即时胜利先于后续撤离")
	battle = ExitFixture.battle(false); ExitFixture.commit(battle, data)
	check_terminal(battle, data, Fixture.plan(battle.units.defender, "wait"), "victory")
	data = EscortFixture.content(); battle = EscortFixture.battle()
	var result: Dictionary = {}
	for unused in range(10):
		EscortFixture.commit_ai(battle, data)
		result = Round.resolve(battle, data, Fixture.plan(battle.units.defender, "withdraw"))
		if not check(result.get("ok", false), "保护者沿原导航逐轮跟随撤离"): return
		battle = result.battle
		if battle.phase == "result": break
	if not check(battle.phase == "result" and battle.outcome == "withdrawn", "控制者与保护者实际共同到出口"): return
	check(validate_frozen(battle, data).ok, "实际护送组退保持合法")
	var forged: Dictionary = battle.duplicate(true); forged.units["protected"].position = [300, 300]
	check(validate_frozen(forged, data).get("code") == "PersonalOutcomeMismatch", "保护者未在出口不能靠主角退出完成组退")
	forged = battle.duplicate(true); forged.units["protected"].erase("withdrawn_round")
	check(validate_frozen(forged, data).get("code") == "PersonalOutcomeMismatch", "新标记控制者不能漏掉同轮保护组的退出事实")

## 显式exit_v1保留既有到达与超时事件合同，不借普通终局补门改变原错误码或胜负。
func check_explicit_exit_model() -> void:
	var data: Dictionary = EscortExitFixture.content()
	var battle: Dictionary = EscortExitFixture.battle(); battle.round = data.personal_objectives.spar.max_rounds
	battle.ai_plans = Round.make_ai(battle, data); battle.ai_plans.attacker = Fixture.plan(battle.units.attacker, "wait")
	var terminal: Dictionary = check_terminal(battle, data, Fixture.plan(battle.units.defender, "wait"), "draw")
	if terminal.is_empty(): return
	var forged: Dictionary = terminal.duplicate(true); forged.outcome = "victory"
	check(validate_frozen(forged, data).get("code") == "EscortOutcomeMismatch", "显式出口未到达仍返回原EscortOutcomeMismatch")
	forged = terminal.duplicate(true)
	forged.last_events.erase({"action": "objective", "code": "EscortExitTimeout"})
	check(validate_frozen(forged, data).get("code") == "MissingEscortOutcomeReason", "显式出口超时原因仍按原专门事件合同校验")

## 本门只证明个人结果存读语义，不替代全存档迁移、完整战斗或GUI验收。
func run_checks() -> void:
	check_real_restore()
	check_timeout_and_priority()
	check_withdrawal_history()
	check_explicit_exit_model()
	print("PERSONAL_RESULT_CONSISTENCY_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
