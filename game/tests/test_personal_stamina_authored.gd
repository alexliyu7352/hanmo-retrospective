## 青川1.4正式回气切磋入口：旧四场完整初态不变，新场通过真实买剑、对白确认和同步战斗验证已实现模型。
extends SceneTree
const Kernel = preload("res://core/world_kernel.gd")
const Interaction = preload("res://core/interaction_rules.gd")
const Inventory = preload("res://core/inventory_rules.gd")
const Stamina = preload("res://core/domains/personal/personal_stamina.gd")
const PLAYER: String = "qingstudy::actor.apprentice"
const MASTER: String = "qingstudy::actor.sword_master"
const ROLE: String = "qingstudy::role.sword_master_breath"
const SWORD: String = "qingstudy::item.practice_sword"
const FOCUS: String = "qingstudy::skill.focus"
var checks: int = 0
var failures: int = 0
var fixture: Dictionary = {}
var observed_attack_hit: bool = false

## 仅运行明确准备的隔离工程，不启动主界面或用户存储。
func _init() -> void:
	call_deferred("run_checks")

## 失败影响退出码并保留具体标签，前置错误不继续解引用。
func check(value: bool, label: String) -> bool:
	checks += 1
	if not value: failures += 1; printerr("FAIL: ", label)
	return value

## 所有游戏变化使用真实revision和请求序号；不直接补钱、装备、XP、气势或RNG。
func send(kernel, kind: String, params: Dictionary = {}) -> Dictionary:
	var state: Dictionary = kernel.snapshot()
	return kernel.submit({"kind": kind, "params": params, "actor_id": PLAYER, "request_seq": state.request_sequences[PLAYER] + 1, "expected_revision": state.revision})

## 只排除新版本必然变化的三项内容身份，其他完整世界字段必须逐值相等。
func gameplay(state: Dictionary) -> Dictionary:
	var result: Dictionary = state.duplicate(true)
	for key in ["content_id", "content_version", "content_digest"]: result.erase(key)
	return result

## 旧四场均用原1.3与新1.4实际目录初始化，不能只靠旧JSON记录相等宣称无泄漏。
func check_old_scenarios() -> void:
	for scenario in fixture.old_scenarios:
		var old = Kernel.new(); var current = Kernel.new()
		var first: Dictionary = old.new_game("qingstudy::" + scenario, fixture.catalogs.baseline)
		var second: Dictionary = current.new_game("qingstudy::" + scenario, fixture.catalogs.candidate)
		if not check(first.ok and second.ok, "原场景双目录合法初始化：" + scenario + " " + str(first) + " " + str(second)): return
		check(gameplay(old.snapshot()) == gameplay(current.snapshot()), "原场景完整人物/资产/地点/资格/RNG/序号没有漂移：" + scenario)
		check(not current.snapshot().roles.has(ROLE), "新服务条款不泄漏到原场景：" + scenario)

## 读取所有真实钱包合计，保证买卖及切磋只转移有限的钱，不产生测试充值。
func money(state: Dictionary) -> int:
	var total: int = 0
	for wallet in state.wallets.values(): total += wallet.balance
	return total

## 实际回合后按公开真实命中事件核已定10/15/5/3/20恢复，不调用生产恢复函数充当期望值。
func play_round(kernel, action: String) -> Dictionary:
	var before: Dictionary = kernel.snapshot(); var battle: Dictionary = before.battles.values()[0]
	var unit: Dictionary = battle.units[PLAYER]
	var params: Dictionary = {"battle_id": battle.id, "action": action, "destination": unit.position.duplicate(), "heading": unit.heading}
	if action == "attack":
		params.target_id = MASTER
		var delta: Vector2 = Vector2(battle.units[MASTER].position[0] - unit.position[0], battle.units[MASTER].position[1] - unit.position[1])
		params.heading = posmod(roundi(rad_to_deg(atan2(delta.y, delta.x))), 360)
	var quote: Dictionary = kernel.preview("battle_plan", params)
	if not check(quote.ok and kernel.snapshot() == before, "当前动作正式预览只读：" + action + " " + str(quote)): return {}
	var applied: Dictionary = send(kernel, "battle_plan", params)
	if not check(applied.ok, "正式同步回合：" + action + " " + str(applied)): return {}
	var after: Dictionary = kernel.snapshot(); var next: Dictionary = after.battles[battle.id]
	var cost: int = 0; var restoration: int = 10 if action == "defend" else (15 if action == "wait" else 0)
	var ordinary_hit: bool = false
	for event in next.last_events:
		if event.get("actor_id") == PLAYER and event.get("action") == "attack" and event.get("code") in ["Hit", "Miss"]:
			cost = kernel.content().data.items[unit.weapon_id].weapon.stamina_cost
			if event.get("code") == "Hit" and event.get("damage", 0) > 0: ordinary_hit = true
		if event.get("target_id") == PLAYER and event.get("action") in ["attack", "skill", "counter"] and event.get("code") == "Hit" and event.get("damage", 0) > 0: restoration += 3
	if ordinary_hit: restoration += 5; observed_attack_hit = true
	restoration = mini(20, restoration)
	if next.units[PLAYER].hp <= 0 or next.units[PLAYER].has("withdrawn_round"): restoration = 0
	check(next.units[PLAYER].stamina == mini(100, unit.stamina - cost + restoration), "实际气势符合原回气模型，费用先付且只按真实结果恢复：" + action)
	check(after.tick == before.tick and after.wallets == before.wallets, "战术回合不重复收费或另推进半日")
	return next

## 新第五场只换孟修本场条款；原资格、有限资产、正常购买和当面确认共同形成真实战斗入口。
func check_authored_play() -> void:
	var kernel = Kernel.new()
	var made: Dictionary = kernel.new_game("qingstudy::scenario.breath_practice", fixture.catalogs.candidate)
	if not check(made.ok, "新第五场正常创建：" + str(made)): return
	var start: Dictionary = kernel.snapshot(); var data: Dictionary = kernel.content().data
	check(start.actors.size() == 4 and start.roles.has(ROLE) and not start.roles.has("qingstudy::role.sword_master"), "仍是原四人且只启用一条孟修教习挂载")
	check(start.roles[ROLE].actor_id == MASTER and data.roles[ROLE].sparring.wallet_id == "qingstudy::wallet.sword_master" and data.roles[ROLE].sparring.fee == 8, "同一老师、钱包与八钱条款，不造职业或另送钱")
	check(start.wallets[start.actors[PLAYER].wallet_id].balance == 120 and start.actors[PLAYER].combat.learned.is_empty() and start.actors[PLAYER].combat.weapon_id.is_empty(), "开局仍为原盘缠、未学武功和未装备状态")
	for xp in start.actors[PLAYER].skills.values(): check(xp == 0, "不以新场名义赠送普通技艺经验")
	check(kernel.preview("duel_start", {"context": Interaction.for_role(start, data, ROLE)}).get("code") == "WeaponRequired", "仍须真实自备武器，不能跳过购买")
	check(data.shops.has("qingstudy::shop.sword_counter_breath") and not data.shops.has("qingstudy::shop.sword_counter") and data.shops["qingstudy::shop.sword_counter_breath"].inventory_id == "qingstudy::inventory.sword_counter", "新场仅一份柜台挂载，仍用原实际库存")
	var request: Dictionary = {"shop_id": "qingstudy::shop.sword_counter_breath", "item_id": SWORD, "quantity": 1, "side": "buy", "payer_wallet_id": start.actors[PLAYER].wallet_id, "context": Interaction.for_role(start, data, ROLE)}
	var quoted: Dictionary = kernel.preview("trade", request)
	if not check(quoted.ok and quoted.total == 68, "原真实器械柜报价68钱：" + str(quoted)): return
	if not check(send(kernel, "trade", request).ok and send(kernel, "advance").ok, "原交易与一半日实际完成"): return
	var bought: Dictionary = kernel.snapshot()
	check(bought.wallets[bought.actors[PLAYER].wallet_id].balance == 52 and money(bought) == money(start), "买剑只转移68钱，整体钱包守恒")
	check(Inventory.count(bought.inventories[bought.actors[PLAYER].inventory_id], SWORD, [{"kind": "actor", "id": PLAYER}]) == 1, "剑来自本人真实库存，不是测试配置注入")
	if not check(send(kernel, "equip_weapon", {"weapon_id": SWORD}).ok, "原合法装备命令选择实物剑"): return
	if not check(send(kernel, "talk", {"role_id": ROLE}).ok, "正常交谈进入本场孟修说明"): return
	for unused in range(8):
		var state: Dictionary = kernel.snapshot()
		if state.interactions[PLAYER].phase == "choice": break
		if not check(send(kernel, "dialogue_next").ok, "正常翻过当面说明"): return
	if not check(send(kernel, "dialogue_choose", {"option_id": "spar"}).ok, "对白选择实际切磋条款"): return
	var before: Dictionary = kernel.snapshot(); var preview: Dictionary = kernel.preview("dialogue_confirm")
	check(preview.ok and kernel.snapshot() == before, "确认前报价不扣钱、不推进时间或预造战斗")
	if not check(send(kernel, "dialogue_confirm").ok, "确认后真实支付并开始切磋"): return
	var opened: Dictionary = kernel.snapshot(); var battle: Dictionary = opened.battles.values()[0]
	check(battle.source.context.role_id == ROLE and battle.units.size() == 2 and battle.units.has(MASTER), "仍与同一真实孟修交手，不造替身敌人")
	check(data.personal_objectives[battle.objective_id].stamina_model == "round_v1" and battle.units[PLAYER].stamina == 50, "新场只开启已实现模型，初始气势仍用原50")
	check(opened.tick == before.tick + 1 and opened.wallets[opened.actors[PLAYER].wallet_id].balance == 44 and money(opened) == money(start), "八钱与开场半日只结算一次，原盘缠足以进入新玩法")
	for action in ["wait", "defend", "attack"]:
		battle = play_round(kernel, action)
		if battle.is_empty(): return
	for unused in range(24):
		if battle.phase != "planning": break
		battle = play_round(kernel, "attack")
		if battle.is_empty(): return
	check(observed_attack_hit, "这条真实新场路径实际发生普通命中，不只核等待字段")
	if not check(battle.phase == "result", "原战斗规则正常到达真实终局"): return
	var finished: Dictionary = kernel.snapshot()
	if not check(send(kernel, "battle_close", {"battle_id": battle.id}).ok, "原关闭命令回到同一剑庭"): return
	var closed: Dictionary = kernel.snapshot()
	check(closed.tick == finished.tick and closed.wallets == finished.wallets and closed.facts == finished.facts and money(closed) == money(start), "结果关闭不重收钱、不重算事实或消耗额外半日")

## 只从本人当面服务建立上下文，课程、短工与学习继续消费同一教师和钱包。
func context(kernel) -> Dictionary:
	return Interaction.for_role(kernel.snapshot(), kernel.content().data, ROLE)

## 按实际剩余行动推进半日；不改XP、掌握、钱款或老师占用来缩短成长路径。
func complete_action(kernel) -> bool:
	for unused in range(8):
		if kernel.snapshot().actors[PLAYER].action_id.is_empty(): return true
		var advanced: Dictionary = send(kernel, "advance")
		if not check(advanced.ok, "成长链正常完成一个半日：" + str(advanced)): return false
	return check(kernel.snapshot().actors[PLAYER].action_id.is_empty(), "有界正常课程已经完成")

## 一次活动必须先通过正式报价，成功后才等世界推进；守恒检查覆盖同一有限教师钱包。
func timed_action(kernel, kind: String, params: Dictionary, label: String) -> bool:
	var before: Dictionary = kernel.snapshot(); var offer: Dictionary = kernel.preview(kind, params)
	if not check(offer.ok and kernel.snapshot() == before, label + "报价只读：" + str(offer)): return false
	var started: Dictionary = send(kernel, kind, params)
	if not check(started.ok, label + "真实开始：" + str(started)) or not complete_action(kernel): return false
	return check(money(kernel.snapshot()) == money(before), label + "整体钱包守恒")

## 心法子菜单沿已有对白选择，返回和旧敛息仍可达，不突破原八项主菜单预算。
func choose_path(kernel, options: Array) -> bool:
	if not check(send(kernel, "talk", {"role_id": ROLE}).ok, "当面向同一孟修交谈"): return false
	for unused in range(8):
		if kernel.snapshot().interactions[PLAYER].phase == "choice": break
		if not check(send(kernel, "dialogue_next").ok, "翻阅原教学与回气说明"): return false
	for option in options:
		var chosen: Dictionary = send(kernel, "dialogue_choose", {"option_id": option})
		if not check(chosen.ok, "真实对白选项：" + option + " " + str(chosen)): return false
	return true

## A/B仅复制真实命令得到的合法同版状态，未改任何字段，不涉及保存编解码或历史迁移。
func fork_earned(snapshot: Dictionary):
	var kernel = Kernel.new()
	if not check(kernel.new_game("qingstudy::scenario.breath_practice", fixture.catalogs.candidate).ok, "对照世界载入同版作者目录"): return null
	var restored: Dictionary = kernel.restore(snapshot)
	if not check(restored.ok and kernel.snapshot() == snapshot, "对照只使用完整真实获取状态：" + str(restored)): return null
	return kernel

## 每个分支真实交八钱入战，再用第一轮普通防御观察当前心法，不手改战斗或AI计划。
func first_defense(kernel, expected_recovery: int, label: String) -> Dictionary:
	if not choose_path(kernel, ["spar"]): return {}
	var before: Dictionary = kernel.snapshot()
	if not check(send(kernel, "dialogue_confirm").ok, label + "正常确认切磋"): return {}
	var opened: Dictionary = kernel.snapshot(); var battle: Dictionary = opened.battles.values()[0]
	check(opened.wallets[opened.actors[PLAYER].wallet_id].balance == before.wallets[before.actors[PLAYER].wallet_id].balance - 8 and money(opened) == money(before), label + "只付原八钱且真实钱包守恒")
	var unit: Dictionary = battle.units[PLAYER]
	var plan: Dictionary = {"battle_id": battle.id, "action": "defend", "destination": unit.position.duplicate(), "heading": unit.heading}
	var played: Dictionary = send(kernel, "battle_plan", plan)
	if not check(played.ok, label + "实际防御轮：" + str(played)): return {}
	var next: Dictionary = kernel.snapshot().battles[battle.id]
	check(next.units[PLAYER].stamina == 50 + expected_recovery, label + "第一轮气势50→" + str(50 + expected_recovery))
	var recorded: int = 0
	for event in next.last_events:
		if event.get("actor_id") == PLAYER and event.get("code") == "Recovered": recorded += event.amount
	check(recorded == expected_recovery and recorded <= 20, label + "真实恢复事件精确且不超每轮20")
	return battle

## 独立的只读封顶探针把作者固定5接到原查询；两个受击事件是输入边界，不冒称本1v1已发生双受击。
func check_focus_cap(battle: Dictionary, data: Dictionary) -> void:
	var before: Dictionary = battle.duplicate(true)
	var plans: Dictionary = {PLAYER: {"action": "defend", "destination": battle.units[PLAYER].position.duplicate()}}
	var events: Array = [{"code": "Hit", "action": "attack", "actor_id": MASTER, "target_id": PLAYER, "damage": 1}, {"code": "Hit", "action": "skill", "actor_id": MASTER, "target_id": PLAYER, "damage": 1}]
	var grants: Dictionary = Stamina.grants(battle.units, plans, battle.units, events, {PLAYER: data.personal_skills[FOCUS].defend_recovery_bonus})
	check(grants.get(PLAYER) == 20 and battle == before, "作者凝神15加两次受击6沿原只读查询封顶20；输入不变")

## 真实工作挣钱、普通课达武艺一阶，再付费学、练、配置；没有预赠经验或绕过教师掌握。
func check_focus_learning() -> void:
	var kernel = Kernel.new()
	if not check(kernel.new_game("qingstudy::scenario.breath_practice", fixture.catalogs.candidate).ok, "凝神成长链从原第五场开局"): return
	var start: Dictionary = kernel.snapshot(); var data: Dictionary = kernel.content().data
	check(start.actors[PLAYER].skills.martial == 0 and start.actors[PLAYER].combat.learned.is_empty(), "成长链开局仍零XP且没有赠予功法")
	check(start.actors[MASTER].combat.learned.get(FOCUS) == 100 and start.actors[MASTER].combat.heart_id.is_empty(), "同一老师本场真实能教凝神，但不暗配心法改变原对手")
	if not choose_path(kernel, ["heart", "back", "heart", "heart"]): return
	check(kernel.snapshot().interactions[PLAYER].pending_command.skill_id == "qingstudy::skill.settle_breath" and kernel.preview("dialogue_confirm").cost == 20, "心法菜单返回后原敛息课仍按原20钱可达")
	if not check(send(kernel, "dialogue_cancel").ok and send(kernel, "end_interaction").ok, "取消旧心法课没有缴费"): return
	if not choose_path(kernel, ["heart", "focus"]): return
	var before: Dictionary = kernel.snapshot(); var denied: Dictionary = send(kernel, "dialogue_confirm")
	check(not denied.ok and denied.get("code") == "SkillLevelRequired" and kernel.snapshot() == before, "武艺零级实际确认拒绝，全世界与钱包均未改变")
	if not check(send(kernel, "dialogue_cancel").ok and send(kernel, "end_interaction").ok, "取消未达资格课程"): return
	for unused in range(6):
		if not timed_action(kernel, "service", {"activity": "work", "context": context(kernel)}, "原三十钱短工"): return
	for unused in range(2):
		if not timed_action(kernel, "service", {"activity": "rest", "context": context(kernel)}, "原五钱住宿"): return
	before = kernel.snapshot()
	check(before.wallets[before.actors[PLAYER].wallet_id].balance == 290 and before.actors[PLAYER].fatigue == 0, "六次实付短工与两次住宿只动原有限钱款和疲劳")
	if not timed_action(kernel, "study_skill", {"skill_id": "martial", "ticks": 6, "context": context(kernel)}, "正常武艺课程"): return
	var studied: Dictionary = kernel.snapshot()
	check(studied.actors[PLAYER].skills.martial == 120 and studied.wallets[studied.actors[PLAYER].wallet_id].balance == 110, "六个真实半日支付180钱取得120XP，达到既有100XP一阶门槛")
	if not choose_path(kernel, ["heart", "focus"]): return
	before = kernel.snapshot(); var offer: Dictionary = kernel.preview("dialogue_confirm")
	check(offer.ok and offer.cost == 20 and offer.duration == 2 and kernel.snapshot() == before, "达标后凝神沿样本心法条款二十钱两半日")
	if not check(send(kernel, "dialogue_confirm").ok, "实际付费拜学凝神") or not complete_action(kernel): return
	var learned: Dictionary = kernel.snapshot()
	check(learned.actors[PLAYER].combat.learned.get(FOCUS, -1) == 0 and learned.wallets[learned.actors[PLAYER].wallet_id].balance == 90, "学习完成只取得零掌握，二十钱只支付一次")
	if not check(send(kernel, "end_interaction").ok, "结束已完成教学对白"): return
	if not timed_action(kernel, "trade", {"shop_id": "qingstudy::shop.sword_counter_breath", "item_id": SWORD, "quantity": 1, "side": "buy", "payer_wallet_id": learned.actors[PLAYER].wallet_id, "context": context(kernel)}, "本人正常买剑"): return
	if not check(send(kernel, "equip_weapon", {"weapon_id": SWORD}).ok, "装备自己买来的实物剑"): return
	var unpracticed = fork_earned(kernel.snapshot())
	if unpracticed == null: return
	if not check(send(unpracticed, "configure_heart", {"skill_id": FOCUS}).ok, "零掌握允许选槽但不能凭配置获得效果"): return
	if first_defense(unpracticed, 10, "已学但未练的凝神").is_empty(): return
	if not timed_action(kernel, "practice_skill", {"skill_id": FOCUS, "ticks": 1}, "真实一半日修炼"): return
	var earned: Dictionary = kernel.snapshot()
	check(earned.actors[PLAYER].combat.learned[FOCUS] == 13 and earned.actors[PLAYER].combat.heart_id.is_empty(), "按原八半日练习基数得13掌握，不自动配置")
	var unequipped = fork_earned(earned)
	if unequipped == null or first_defense(unequipped, 10, "已练但未配置的凝神").is_empty(): return
	if not check(send(kernel, "configure_heart", {"skill_id": FOCUS}).ok, "正常配置单一凝神心法"): return
	var configured: Dictionary = kernel.snapshot()
	var cleared = fork_earned(configured)
	if cleared == null or not check(send(cleared, "configure_heart", {"skill_id": ""}).ok, "正常撤下单一心法"): return
	if first_defense(cleared, 10, "撤下后的凝神").is_empty(): return
	var battle: Dictionary = first_defense(kernel, 15, "真实达标学练并配置的凝神")
	if battle.is_empty(): return
	check_focus_cap(battle, data)
	check(money(kernel.snapshot()) == money(start), "整条工作/学习/买剑/切磋路径没有测试充值或货币生成")

## 只运行新样本及四场差分；旧叶数值矩阵、GUI、完整存读与迁移不在本门重跑。
func run_checks() -> void:
	if not check(ProjectSettings.get_setting("application/config/personal_stamina_authored_isolated", false), "必须使用专用正式tiny准备工程"): finish(); return
	fixture = JSON.parse_string(FileAccess.get_file_as_string("res://tests/personal_stamina_authored.json"))
	check_old_scenarios()
	check_authored_play()
	var original_checks: int = checks
	check_focus_learning()
	print("DU01_AUTHORED_CHECKS=", checks - original_checks, " FAILURES_TOTAL=", failures)
	finish()

## 输出真实局部断言与退出状态，不把tiny模型门称为原美术或完整章节验收。
func finish() -> void:
	print("PERSONAL_STAMINA_AUTHORED_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
