## 在真实Godot场景中验证个人战界面、输入和截图；会话来源是夹具，不冒充正式世界集成验收。
extends SceneTree
const Main = preload("res://ui/main.gd")
const PersonalCommands = preload("res://core/domains/personal/personal_commands.gd")
const Rules = preload("res://core/personal_round.gd")
const Screen = preload("res://ui/personal_battle_screen.gd")
const Art = preload("res://ui/art_catalog.gd")
var checks: int = 0
var failures: int = 0
var capture_dir: String = ""
var art_root: String = ""
var inputs: Array = []
var original_art: RefCounted
var integration: bool = false
var escort: bool = false

## 夹具沿用正式主场景和控件，只隔离尚未合入本分支的世界命令；该替身不验证世界回写与存档。
class FixtureHost extends Main:
	var commands: Array = []
	## 纯图形夹具现在也走真实个人预览回调；这里只装预览所需门面，不宣称完整Kernel注册或结算验证。
	func install_preview_fixture(definitions: Dictionary) -> void:
		kernel._catalog={"data":definitions}
		kernel._registry={"commands":{}}
		for command in PersonalCommands.descriptor().commands:
			var entry: Dictionary=command.duplicate(true); entry.owner="personal"
			kernel._registry.commands[entry.kind]=entry

	## 记录UI提交并用正式纯轮次产生下一幅画面；正式Kernel集成由主工程另行验收。
	func _perform(kind: String, params: Dictionary = {}) -> void:
		commands.append({"kind": kind, "params": params.duplicate(true)})
		if kind != "battle_plan": return
		var candidate: Dictionary = params.duplicate(true); candidate.erase("battle_id")
		var result: Dictionary = Rules.resolve(kernel._state.battles[params.battle_id], _data, candidate)
		if not result.ok: _message = result.code; _render(); return
		kernel._state.battles[params.battle_id] = result.battle
		kernel._state.revision += 1
		_render()

## 仅供隔离图形检查的注册表替身：图片从显式夹具参数装入，生产UI仍只调用Art接口。
class FixtureArt extends RefCounted:
	var images: Dictionary = {}
	var fallback: RefCounted
	## 通过与正式注册表相同的查询签名提供已经生成的纹理，不参与任何世界规则。
	func texture(group: String, id: String) -> Texture2D:
		return images.get(id, fallback.texture(group, id))
	## 初始化时仍保留原清单描述；此替身不声称完成新表现清单校验。
	func describe() -> Dictionary:
		return fallback.describe()
	## 标题页枚举仍委托正式清单，夹具纹理替换不另建可选资源名单。
	func ids(group: String) -> Array:
		return fallback.ids(group)

## 延迟到引擎就绪后再创建真实控件与可选图形捕获。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg == "--integration": integration = true
		if arg == "--escort": escort = true
		if arg.begins_with("--capture-dir="): capture_dir = arg.trim_prefix("--capture-dir=")
		if arg.begins_with("--art-root="): art_root = arg.trim_prefix("--art-root=")
	call_deferred("run_escort" if escort else ("run_integrated" if integration else "run_checks"))

## 统计实际断言，失败清楚定位且最终退出非零。
func check(value: bool, label: String) -> void:
	checks += 1
	if not value: failures += 1; printerr("FAIL: ", label)

## 为图形检查加载已生成素材；不修改主树资源、清单或像素，未指定目录时使用既有Art回退。
func prepare_art() -> void:
	if art_root.is_empty(): return
	var adapter: FixtureArt = FixtureArt.new(); original_art = Art.catalog(); adapter.fallback = original_art
	for row in [["battlefield_practice", "practice-clearing.png"], ["battlefield_ford", "river-ford.png"], ["battle_sword", "swordsman.png"], ["battle_spear", "spearman.png"], ["battle_bow", "archer.png"]]:
		var image: Image = Image.load_from_file(art_root.path_join(row[1]))
		check(image != null and not image.is_empty(), "读取真实独立素材：" + row[0])
		if image != null and not image.is_empty(): adapter.images[row[0]] = ImageTexture.create_from_image(image)
	Art._registry = adapter

## 提供正常字段形状的两武器、能力、人物和场地；它们只用于UI复用和命令参数检查。
func fixture() -> Dictionary:
	var data: Dictionary = {"required_capabilities":["personal_combat"],"texts": {"name.hero": "行旅人", "name.rival": "赵巡", "name.guest": "许成", "name.yard": "村外演武场", "name.ford": "河岸古道", "name.sword": "环首刀", "name.spear": "长枪", "name.strike": "疾风斩", "name.heart": "养气诀"}, "actors": {}, "items": {}, "personal_skills": {}, "personal_battlefields": {}, "personal_objectives": {"objective.spar": {"kind": "sparring", "max_rounds": 10, "allow_withdrawal": true}}}
	for row in [["actor.p", "name.hero", "player"], ["actor.e", "name.rival", "guard"], ["actor.g", "name.guest", "merchant"]]: data.actors[row[0]] = {"id": row[0], "name_key": row[1], "portrait_kind": row[2]}
	data.items["item.sword"] = {"id": "item.sword", "name_key": "name.sword", "battle_portrait_kind": "battle_sword", "weapon": {"category": "sword", "minimum_range": 0, "maximum_range": 70, "arc_degrees": 120, "damage": 18, "stamina_cost": 10, "recovery_rounds": 0, "move_distance": 90}}
	data.items["item.spear"] = {"id": "item.spear", "name_key": "name.spear", "battle_portrait_kind": "battle_spear", "weapon": {"category": "spear", "minimum_range": 45, "maximum_range": 135, "arc_degrees": 70, "damage": 22, "stamina_cost": 12, "recovery_rounds": 0, "move_distance": 80}}
	data.personal_skills["skill.strike"] = {"id": "skill.strike", "name_key": "name.strike", "kind": "strike", "weapons": ["sword"], "stamina_cost": 15, "power_bp": 16000, "defense_bp": 10000, "move_bonus": 10, "status": "exposed", "status_rounds": 1}
	data.personal_skills["skill.heart"] = {"id": "skill.heart", "name_key": "name.heart", "kind": "heart", "weapons": ["sword"], "stamina_cost": 0, "power_bp": 11000, "defense_bp": 9000, "move_bonus": 10, "status": "none", "status_rounds": 0}
	data.personal_battlefields["field.yard"] = {"id": "field.yard", "name_key": "name.yard", "visual_tag": "battlefield_practice", "width": 600, "height": 400, "obstacles": [{"x": 40, "y": 35, "width": 45, "height": 65, "blocks_movement": true, "blocks_sight": true}], "exits": [{"side": 0, "position": [35, 200], "radius": 25}, {"side": 1, "position": [565, 200], "radius": 25}]}
	data.personal_battlefields["field.ford"] = {"id": "field.ford", "name_key": "name.ford", "visual_tag": "battlefield_ford", "width": 800, "height": 500, "obstacles": [{"x": 500, "y": 0, "width": 300, "height": 130, "blocks_movement": true, "blocks_sight": false}], "exits": [{"side": 0, "position": [35, 300], "radius": 25}, {"side": 1, "position": [765, 300], "radius": 25}]}
	var units: Dictionary = {}
	for row in [["actor.p", 0, [200, 240], "item.sword"], ["actor.e", 1, [330, 230], "item.spear"], ["actor.g", 0, [160, 310], ""]]:
		units[row[0]] = {"actor_id": row[0], "side": row[1], "position": row[2], "heading": 0 if row[1] == 0 else 180, "radius": 18, "hp": 100, "initial_hp": 100, "stamina": 50, "weapon_id": row[3], "martial": 40, "learned": {}, "heart_id": "", "statuses": {}, "recovery_until": 0, "protected": row[0] == "actor.g"}
	units["actor.p"].learned = {"skill.strike": 60, "skill.heart": 80}; units["actor.p"].heart_id = "skill.heart"
	var battle: Dictionary = {"id": "battle.fixture", "kind": "personal", "controller_id": "actor.p", "battlefield_id": "field.yard", "objective_id": "objective.spar", "round": 1, "phase": "planning", "units": units, "ai_plans": {}, "outcome": "", "last_events": [], "applied":false}
	battle.ai_plans = Rules.make_ai(battle, data)
	var state: Dictionary = {"player_actor_id": "actor.p", "revision": 1, "tick": 2, "actors": {"actor.p": {"id": "actor.p", "wallet_id": "wallet.p", "hp": 100, "alive":true}}, "wallets": {"wallet.p": {"balance": 800}}, "battles": {battle.id: battle}}
	for actor_id in units:
		if not state.actors.has(actor_id): state.actors[actor_id] = {"id": actor_id, "hp": units[actor_id].hp, "alive":true}
	return {"data": data, "state": state}

## 查找正式按钮的语义标记，不依赖显示文字来定位命令。
func find_button(node: Node, kind: String, target: String = "") -> Button:
	if node is Button and node.get_meta("command_kind", "") == kind and (target.is_empty() or node.get_meta("target_id", "") == target): return node
	for child in node.get_children():
		var found: Button = find_button(child, kind, target)
		if found != null: return found
	return null

## 通过真实Viewport的输入分发点击控件或地面；这是引擎GUI输入夹具，不宣称操作系统鼠标试玩。
func click(point: Vector2, button: int = MOUSE_BUTTON_LEFT) -> void:
	inputs.append({"source": "GodotViewportInput", "position": [point.x, point.y], "button": button})
	for down in [true, false]:
		var event: InputEventMouseButton = InputEventMouseButton.new(); event.position = point; event.global_position = point; event.button_index = button; event.pressed = down
		root.push_input(event, true); await process_frame
	await process_frame

## 只保存渲染器实际绘制的视口，目录由显式测试参数给出；不合成或美化截图。
func capture(name: String) -> void:
	if capture_dir.is_empty(): return
	DirAccess.make_dir_recursive_absolute(capture_dir)
	await RenderingServer.frame_post_draw
	check(root.get_texture().get_image().save_png(capture_dir.path_join(name + ".png")) == OK, "保存真实GUI画面：" + name)

## 使用正式场景分支完成草案、非法落点、技能、提交、结果和第二场地画面检查。
func run_checks() -> void:
	root.size = Vector2i(1280, 800)
	prepare_art()
	var values: Dictionary = fixture()
	var main: FixtureHost = FixtureHost.new(); root.add_child(main)
	main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	main._data = values.data; main.install_preview_fixture(values.data); main.kernel._state = values.state.duplicate(true); main._started = true; main._render()
	for _i in range(4): await process_frame
	var view = main.find_child("PersonalBattleScreen", true, false)
	var canvas = main.find_child("PersonalBattleCanvas", true, false)
	check(view != null and canvas != null, "正式场景按active会话切到个人战")
	if canvas == null: quit(1); return
	var before: Dictionary = main.kernel.snapshot()
	await capture("01-planning")
	await click(canvas.global_position + canvas.screen_point([270, 240]))
	check(view.draft.destination == [270, 240], "实际画布输入产生连续整数落点")
	check(main.kernel.snapshot() == before, "移动草案不改变世界或保存AI意图")
	await click(canvas.global_position + canvas.screen_point([330, 230]) - Vector2(0, 50))
	check(view.selected_id == "actor.e" and view.draft.target_id == "actor.e", "点人物立姿选择真实敌方目标")
	await click(canvas.global_position + canvas.screen_point([350, 240]), MOUSE_BUTTON_RIGHT)
	check(view.draft.heading == 0, "右键调整朝向只影响UI草案")
	check(view._preview.text.contains("预计伤害"), "有效射程内显示同源伤害预测")
	await capture("02-range-preview")
	var submit: Button = find_button(main, "battle_submit")
	check(submit != null and not submit.disabled, "合法草案有明确提交入口")
	await click(canvas.global_position + canvas.screen_point([550, 350]))
	check(submit.disabled and view._preview.text.contains("移动范围"), "超距落点在真实面板解释并禁用提交")
	await click(canvas.global_position + canvas.screen_point([270, 240]))
	var skill: OptionButton = main.find_child("PersonalBattleSkill", true, false)
	check(skill.item_count == 2, "仅显示已掌握且兼容的主动本领，心法不混作按钮")
	skill.select(1); skill.item_selected.emit(1); await process_frame
	check(view.draft.action == "skill" and view.draft.skill_id == "skill.strike", "本领选择生成技能计划而非直接发伤害")
	main._render(); await process_frame
	view = main.find_child("PersonalBattleScreen", true, false)
	check(view.draft.destination == [270, 240] and view.draft.skill_id == "skill.strike", "同一公开轮次重绘保留UI草案")
	submit = find_button(main, "battle_submit")
	await click(submit.get_global_rect().get_center())
	check(main.commands.size() == 1 and main.commands[0].kind == "battle_plan" and main.commands[0].params.battle_id == "battle.fixture", "按钮仅发送一次完整正式命令")
	check(main.kernel._state.battles["battle.fixture"].round == 2, "夹具以正式纯轮次推进并重建下一轮画面")
	await capture("03-next-round")
	main.kernel._state.battles["battle.fixture"].phase = "result"; main.kernel._state.battles["battle.fixture"].outcome = "protected_lost"; main.kernel._state.battles["battle.fixture"].units["actor.g"].hp = 0; main.kernel._state.revision += 1; main._render(); await process_frame
	check(find_button(main, "battle_submit") == null and find_button(main, "battle_close") != null, "结果画面不再提交攻击且有返回原处入口")
	await capture("04-result-fixture")
	await click(find_button(main, "battle_close").get_global_rect().get_center())
	check(not main.commands.is_empty() and main.commands[-1].kind == "battle_close", "返回按钮仅提交关闭结果命令")
	main.kernel._state = values.state.duplicate(true); main.kernel._state.revision = 10; main.kernel._state.battles["battle.fixture"].battlefield_id = "field.ford"; main._render(); await process_frame
	canvas = main.find_child("PersonalBattleCanvas", true, false)
	check(canvas.world_point(canvas.screen_point([650, 380])) == [650, 380], "第二场地不同尺寸仍用同一坐标投影")
	await capture("05-second-field")
	if not capture_dir.is_empty():
		var file: FileAccess = FileAccess.open(capture_dir.path_join("inputs.json"), FileAccess.WRITE)
		file.store_string(JSON.stringify({"scope": "真实Godot GUI夹具；非正式Kernel/存档/发行验收", "inputs": inputs}, "  ")); file.close()
	main.queue_free(); await process_frame
	if original_art != null: Art._registry = original_art; original_art = null
	await process_frame
	print("PERSONAL_BATTLE_UI_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)

## 正式场景按钮也通过Viewport输入；不存在、禁用或超出可见区域时明确失败，不暗中提交命令。
func press(main: Control, kind: String, target: String = "") -> bool:
	var button: Button = find_button(main, kind, target)
	check(button != null and not button.disabled, "正式入口可操作：" + kind + " " + target)
	if button == null or button.disabled:
		printerr("BUTTON_DIAGNOSTIC=", {"kind": kind, "target": target, "view": main.kernel.interaction_view(), "message": main._message})
		await capture("failure-" + kind)
		return false
	var scroll: Node = button.get_parent()
	while scroll != null:
		if scroll is ScrollContainer: scroll.ensure_control_visible(button); await process_frame; break
		scroll = scroll.get_parent()
	await click(button.get_global_rect().get_center())
	for _i in range(8): await process_frame
	return true

## 正式集成检查显式要求独立测试存档；沿实际武馆对白、世界命令、存储服务走完一次切磋。
func run_integrated() -> void:
	root.size = Vector2i(1280, 800)
	var profile: String = ""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--profile=tests-"): profile = arg.trim_prefix("--profile=")
	check(not profile.is_empty(), "正式存储检查须指定tests前缀的隔离profile")
	if profile.is_empty(): quit(1); return
	var main: Control = load("res://main.tscn").instantiate(); root.add_child(main); await process_frame
	var launched: Dictionary = main.start_content("scenario.alpha_life", "res://data/generated/alpha_life.hmcatalog.json")
	check(launched.ok, "真实Main入口通过内容和表现清单校验：" + str(launched))
	if failures: main.queue_free(); await process_frame; quit(1); return
	for _i in range(4): await process_frame
	await press(main, "move_local", "west.training")
	await press(main, "talk", "role.wei")
	await press(main, "dialogue_next")
	await press(main, "dialogue_choose", "spar")
	var before: Dictionary = main.kernel.snapshot()
	check(Screen.active(before).is_empty(), "切磋确认前没有虚构的战斗会话")
	await press(main, "dialogue_confirm")
	var started: Dictionary = main.kernel.snapshot()
	var battle_id: String = Screen.active(started)
	check(not battle_id.is_empty() and main.find_child("PersonalBattleScreen", true, false) != null, "对白确认后切换到真实个人战会话")
	if battle_id.is_empty(): main.queue_free(); await process_frame; quit(1); return
	var player: String = started.player_actor_id
	check(started.tick == before.tick + 1 and started.wallets[started.actors[player].wallet_id].balance == before.wallets[before.actors[player].wallet_id].balance - 12, "切磋的半日与十二钱只在正式确认时结算")
	await capture("06-integrated-entry")
	var saved: Dictionary = main.kernel.snapshot()
	await press(main, "save")
	check(main.last_storage_result.get("ok", false), "战斗规划阶段使用真实存储成功保存")
	await press(main, "battle_action", "wait")
	await press(main, "battle_submit")
	check(main.kernel.snapshot().battles[battle_id].round == 2, "真实Kernel接受UI计划并推进一轮")
	await press(main, "load")
	check(main.last_storage_result.get("ok", false) and main.kernel.snapshot() == saved, "真实读取恢复完整会话、费用和已保存AI意图")
	await capture("07-integrated-restored")
	var rounds: Array = []
	for _turn in range(25):
		var state: Dictionary = main.kernel.snapshot()
		var battle: Dictionary = state.battles[battle_id]
		if battle.phase != "planning": break
		var unit: Dictionary = battle.units[player]
		var enemy: Dictionary = battle.units["actor.wei"]
		var canvas: Control = main.find_child("PersonalBattleCanvas", true, false)
		var start: Vector2 = Vector2(unit.position[0], unit.position[1])
		var target: Vector2 = Vector2(enemy.position[0], enemy.position[1])
		var stride: float = minf(Rules.movement_distance(battle, main._data, player), maxf(0, start.distance_to(target) - 65))
		var point: Vector2 = start + start.direction_to(target) * stride
		await click(canvas.global_position + canvas.screen_point([roundi(point.x), roundi(point.y)]))
		await click(canvas.global_position + canvas.screen_point(enemy.position) + Vector2(0, 10))
		await press(main, "battle_action", "wait" if unit.stamina < 10 else "attack")
		await click(canvas.global_position + canvas.screen_point(enemy.position), MOUSE_BUTTON_RIGHT)
		var immutable: Dictionary = main.kernel.snapshot()
		check(immutable == state, "正式草案选择不改写世界或AI：轮" + str(battle.round))
		if not await press(main, "battle_submit"): break
		var after: Dictionary = main.kernel.snapshot().battles[battle_id]
		rounds.append({"round": after.round, "phase": after.phase, "outcome": after.outcome, "units": after.units.duplicate(true), "events": after.last_events.duplicate(true)})
		check(after.round > battle.round or after.phase == "result", "真实提交推进或得出结果：轮" + str(battle.round))
		if rounds.size() == 2: await capture("08-integrated-combat")
	var finished: Dictionary = main.kernel.snapshot()
	check(finished.battles[battle_id].phase == "result" and finished.battles[battle_id].applied and finished.facts.has(battle_id), "真实切磋到达已回写结果与永久事实")
	await capture("09-integrated-result")
	if finished.battles[battle_id].phase == "result":
		await press(main, "battle_close", battle_id)
		var closed: Dictionary = main.kernel.snapshot()
		check(closed.battles[battle_id].phase == "closed" and main.find_child("PersonalBattleScreen", true, false) == null and closed.actors[player].location.place_id == "west.training", "真实关闭结果回到原武馆")
		check(closed.facts == finished.facts and closed.wallets == finished.wallets and closed.tick == finished.tick, "返回原处不重复扣费、回写或推进时间")
		await capture("10-integrated-return")
	if not capture_dir.is_empty():
		var file: FileAccess = FileAccess.open(capture_dir.path_join("integrated-inputs.json"), FileAccess.WRITE)
		file.store_string(JSON.stringify({"scope": "真实Main、Kernel、alpha_life与原生存档；引擎Viewport输入，非操作系统鼠标或发行包验收", "profile": profile, "save_directory": main.storage.native.directory, "inputs": inputs, "rounds": rounds}, "  ")); file.close()
	main.queue_free(); await process_frame
	print("PERSONAL_BATTLE_INTEGRATED_UI_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)

## 从真实委托人和车夫对白启程，遇到既有敌人后以正常落点退避；不改写合同、同行或生命。
func run_escort() -> void:
	root.size = Vector2i(1280, 800)
	var main: Control = load("res://main.tscn").instantiate(); root.add_child(main); await process_frame
	var launched: Dictionary = main.start_content("scenario.alpha_life", "res://data/generated/alpha_life.hmcatalog.json")
	check(launched.ok, "多人护卫使用真实内容入口：" + str(launched))
	if not launched.ok: main.queue_free(); await process_frame; quit(1); return
	for _i in range(4): await process_frame
	for step in [["move_local", "west.tavern"], ["talk", "role.sender"], ["dialogue_next", ""], ["dialogue_choose", "escort"], ["dialogue_next", ""], ["dialogue_choose", "accept"], ["dialogue_confirm", ""], ["dialogue_next", ""], ["talk", "role.carter"], ["dialogue_next", ""], ["dialogue_choose", "together"], ["dialogue_next", ""], ["dialogue_confirm", ""], ["dialogue_next", ""], ["move_local", "west.street"], ["travel", "village.east"]]:
		if not await press(main, step[0], step[1]): main.queue_free(); await process_frame; quit(1); return
	var conflict_id: String = ""
	for _step in range(12):
		var state: Dictionary = main.kernel.snapshot()
		var meeting: Dictionary = preload("res://core/encounter_rules.gd").pending_for(state, state.player_actor_id)
		if not meeting.is_empty():
			if main.kernel.preview("encounter_fight", {"encounter_id": meeting.id}).ok: conflict_id = meeting.id; break
			await press(main, "encounter_leave", meeting.id)
		if not main.kernel.snapshot().actors[state.player_actor_id].location.kind in ["OnRoute","Wilderness"]: break
		if not await press(main, "advance"): break
	check(not conflict_id.is_empty(), "沿真实道路时序遇到持械旅人")
	if conflict_id.is_empty(): main.queue_free(); await process_frame; quit(1); return
	await capture("11-integrated-road-encounter")
	var before: Dictionary = main.kernel.snapshot()
	await press(main, "encounter_fight", conflict_id)
	var started: Dictionary = main.kernel.snapshot()
	var battle_id: String = Screen.active(started)
	check(not battle_id.is_empty(), "持械应战按钮建立实际多人会话")
	if battle_id.is_empty(): main.queue_free(); await process_frame; quit(1); return
	var battle: Dictionary = started.battles[battle_id]
	check(battle.units.size() == 3 and battle.units["actor.carter"].protected and battle.battlefield_id == "field.ford", "河岸显示实际三人、真实车夫保护目标与第二背景")
	check(started.tick == before.tick and started.actors[started.player_actor_id].location == before.actors[before.player_actor_id].location, "进入战场保留原行程锚点与时间")
	await capture("12-integrated-escort-planning")
	var withdrawal: Array = [[250, 250], [145, 250], [60, 300], [60, 360]]
	for turn in range(8):
		battle = main.kernel.snapshot().battles[battle_id]
		if battle.phase != "planning": break
		var unit: Dictionary = battle.units[battle.controller_id]
		var target: Array = withdrawal[mini(turn, 3)]
		var start: Vector2 = Vector2(unit.position[0], unit.position[1])
		var delta: Vector2 = (Vector2(target[0], target[1]) - start).limit_length(105)
		var destination: Array = [int(start.x + delta.x), int(start.y + delta.y)]
		var canvas: Control = main.find_child("PersonalBattleCanvas", true, false)
		await click(canvas.global_position + canvas.screen_point(destination))
		await press(main, "battle_action", "withdraw")
		if not await press(main, "battle_submit"): break
		if turn == 1: await capture("13-integrated-escort-retreat")
	var finished: Dictionary = main.kernel.snapshot()
	check(finished.battles[battle_id].phase == "result" and finished.battles[battle_id].outcome == "withdrawn", "经真实移动到己方出口才安全退离")
	await capture("14-integrated-escort-withdrawn")
	if finished.battles[battle_id].phase == "result":
		await press(main, "battle_close", battle_id)
		var closed: Dictionary = main.kernel.snapshot()
		check(closed.actors[closed.player_actor_id].location.kind in ["OnRoute","Wilderness"] and closed.tasks.values()[0].status == "active" and main.find_child("WorldMapCanvas", true, false) != null, "退离后回到原地图和未完成的真实护卫行程")
		await capture("15-integrated-escort-return")
	if not capture_dir.is_empty():
		var file: FileAccess = FileAccess.open(capture_dir.path_join("escort-inputs.json"), FileAccess.WRITE)
		file.store_string(JSON.stringify({"scope": "真实Main、Kernel、护卫受托、同行、路遇与安全退离；引擎Viewport输入，非发行包验收", "inputs": inputs, "battle": finished.battles[battle_id]}, "  ")); file.close()
	main.queue_free(); await process_frame
	print("PERSONAL_BATTLE_ESCORT_UI_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
