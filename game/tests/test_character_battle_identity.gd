## 通过正式个人战屏幕验证参战身份独立于武器；规则仍决定伤害、射程与草案范围。
extends SceneTree
const Screen = preload("res://ui/personal_battle_screen.gd")
const Art = preload("res://ui/art_catalog.gd")
const Registry = preload("res://ui/presentation_registry.gd")
const Appearance = preload("res://core/domains/identity/actor_appearance.gd")
const TimeRules = preload("res://core/base/time_rules.gd")
const Round = preload("res://core/personal_round.gd")
const PLAYER = "actor:101"
const RIVAL = "actor:102"
var checks: int = 0
var failures: int = 0

## 宿主仅提供现有屏幕需要的控件工厂；不会启动世界、保存存档或替代任何战斗规则。
class FixtureHost extends Control:
	var _data: Dictionary = {}
	var _message: String = ""
	var _advancing: bool = false
	var status_label: Label
	var commands: Array = []
	## 文本沿真实容器布局，不另造身份标签或隐藏数据来源。
	func _text(parent: Node, value: String, font_size: int) -> Label:
		var label: Label = Label.new(); label.text = value
		label.add_theme_font_size_override("font_size", font_size); parent.add_child(label)
		return label
	## 按钮保留命令元数据与回调；本测试不点击提交，也不伪造核心执行成功。
	func _button(parent: Node, title: String, kind: String, target: String, callback: Callable, disabled: bool = false) -> Button:
		var button: Button = Button.new(); button.text = title; button.disabled = disabled
		button.set_meta("command_kind", kind); button.set_meta("target_id", target)
		button.pressed.connect(callback); parent.add_child(button)
		return button
	## 名称只按作者文本回退，实际人物显示名由正式身份解析提供。
	func _name(value: Dictionary) -> String:
		return _data.texts.get(value.get("name_key", ""), value.get("id", ""))
	## 记录意外提交，确保只读表现查询没有生成世界命令。
	func _perform(kind: String, params: Dictionary = {}) -> void:
		commands.append({"kind": kind, "params": params.duplicate(true)})

## 引擎初始化完成后创建注册表、实际身份档案与正式屏幕。
func _init() -> void:
	call_deferred("_run")

## 统计实际行为断言；任何失败退出非零，不把无报错当作身份正确。
func check(value: bool, label: String) -> void:
	checks += 1
	if not value: failures += 1; printerr("FAIL: ", label)

## 两名真人共用作者原型但保存独立姓名与档案；武器全身图特意不同，用于捕捉旧优先级回归。
func fixture() -> Dictionary:
	var data: Dictionary = {"texts": {"name.person": "原型姓名", "name.yard": "练武场", "name.sword": "刀", "name.spear": "枪"}, "actors": {"actor.prototype": {"id": "actor.prototype", "name_key": "name.person", "presentation_profile_id": "fixture.person", "portrait_kind": "adult.dedicated.a"}}, "items": {}, "personal_skills": {}, "personal_battlefields": {}, "personal_objectives": {"objective.spar": {"kind": "sparring", "max_rounds": 10, "allow_withdrawal": true}}}
	data.items["item.sword"] = {"id": "item.sword", "name_key": "name.sword", "battle_portrait_kind": "adult.dedicated.b", "weapon": {"category": "sword", "minimum_range": 0, "maximum_range": 70, "arc_degrees": 120, "damage": 18, "stamina_cost": 10, "recovery_rounds": 0, "move_distance": 90}}
	data.items["item.spear"] = {"id": "item.spear", "name_key": "name.spear", "battle_portrait_kind": "child.dedicated.a", "weapon": {"category": "spear", "minimum_range": 45, "maximum_range": 135, "arc_degrees": 70, "damage": 22, "stamina_cost": 12, "recovery_rounds": 0, "move_distance": 80}}
	data.personal_battlefields["field.yard"] = {"id": "field.yard", "name_key": "name.yard", "visual_tag": "fixture.scene", "width": 600, "height": 400, "obstacles": [], "exits": [{"side": 0, "position": [35, 200], "radius": 25}, {"side": 1, "position": [565, 200], "radius": 25}]}
	var state: Dictionary = {"tick": 2, "revision": 1, "player_actor_id": PLAYER, "actors": {}, "battles": {}}
	var units: Dictionary = {}
	for row in [[PLAYER, "独立甲", 0, [200, 240]], [RIVAL, "独立乙", 1, [260, 240]]]:
		var profile: Dictionary = Appearance.create(row[0], "private-fixture-seed", {"kind": "fixed", "generator_version": "appearance-v1", "attire_style": "plain", "base_features": {"face_shape": "round" if row[0] == PLAYER else "angular", "eyes": "calm", "brows": "straight", "nose": "plain", "mouth": "plain", "hair": "short", "beard": "none", "skin_palette": "warm", "build": "slender"}}).profile
		state.actors[row[0]] = {"id": row[0], "prototype_id": "actor.prototype", "display_name": row[1], "birth_tick": state.tick - 24 * TimeRules.TICKS_PER_YEAR, "appearance": profile}
		units[row[0]] = {"actor_id": row[0], "side": row[2], "position": row[3], "heading": 0 if row[2] == 0 else 180, "radius": 18, "hp": 100, "initial_hp": 100, "stamina": 50, "weapon_id": "item.sword", "martial": 40, "learned": {}, "heart_id": "", "statuses": {}, "recovery_until": 0, "protected": false}
	state.battles["battle.fixture"] = {"id": "battle.fixture", "kind": "personal", "controller_id": PLAYER, "battlefield_id": "field.yard", "objective_id": "objective.spar", "round": 1, "phase": "planning", "units": units, "ai_plans": {}, "outcome": "", "last_events": []}
	state.battles["battle.fixture"].ai_plans = Round.make_ai(state.battles["battle.fixture"], data)
	return {"state": state, "data": data}

## 从真实CPU纹理取字节；空纹理显式留空，不能把空对空相等当作有效人物。
func pixels(texture: Texture2D) -> PackedByteArray:
	return texture.get_image().get_data() if texture != null else PackedByteArray()

## 在同一屏幕入口重建两种武器，比较描述、实际像素及仍由规则决定的射界。
func check_weapons(values: Dictionary, simple: bool) -> void:
	var host: FixtureHost = FixtureHost.new(); host._data = values.data; root.add_child(host)
	host.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var state: Dictionary = values.state
	var battle: Dictionary = state.battles["battle.fixture"]
	var first: Dictionary = {}
	for weapon in ["item.sword", "item.spear"]:
		battle.units[PLAYER].weapon_id = weapon
		var before: String = JSON.stringify(state)
		var view: Control = Screen.render(host, host, state, battle.id)
		for _frame in range(2): await process_frame
		var canvas: Control = view.canvas
		var body: Dictionary = canvas.battle_descriptors[PLAYER]
		var identity: Dictionary = canvas.identity_descriptors[PLAYER]
		check(body.get("ok", false) and identity.get("ok", false), "两视图均为正式解析结果 " + weapon)
		check(body.get("actor_id") == PLAYER and identity.get("actor_id") == PLAYER and body.get("identity_key") == identity.get("identity_key"), "战斗身体与姓名头像绑定同一真人 " + weapon)
		check(identity.get("display_name") == "独立甲" and canvas.identity_descriptors[RIVAL].get("display_name") == "独立乙", "同原型双人物保留各自保存姓名 " + weapon)
		check(identity.get("identity_key") != canvas.identity_descriptors[RIVAL].get("identity_key"), "同原型双人物仍有独立稳定身份键 " + weapon)
		check(canvas._full_figures[PLAYER] == simple, "只有登记的战斗方案使用全身体型 " + weapon)
		check(not pixels(canvas._portraits[PLAYER]).is_empty() and not pixels(canvas._identity_portraits[PLAYER]).is_empty(), "战斗身体与独立头像都有真实纹理 " + weapon)
		check(canvas.actor_at(canvas.screen_point(battle.units[PLAYER].position)) == PLAYER, "地面点击继续命中实际人物 " + weapon)
		check(canvas.actor_at(canvas.screen_point(battle.units[PLAYER].position) - Vector2(0, 40)) == PLAYER, "原立姿点击区域继续命中实际人物 " + weapon)
		check(not JSON.stringify([canvas.battle_descriptors, canvas.identity_descriptors]).contains("generation_seed"), "画布描述不携带私密生成字段 " + weapon)
		var damage: Dictionary = Round.preview_attack(battle, values.data, PLAYER, [200, 240], 0, RIVAL)
		check(damage.get("ok", false), "两种武器在共同射程内均可攻击 " + weapon)
		var far_battle: Dictionary = battle.duplicate(true); far_battle.units[RIVAL].position = [300, 240]
		var far: Dictionary = Round.preview_attack(far_battle, values.data, PLAYER, [200, 240], 0, RIVAL)
		if first.is_empty():
			first = {"body": body.duplicate(true), "identity": identity.duplicate(true), "body_pixels": pixels(canvas._portraits[PLAYER]), "avatar_pixels": pixels(canvas._identity_portraits[PLAYER]), "range": canvas._range_polygons.duplicate(true), "damage": damage.get("damage", 0)}
			check(not far.ok and far.get("code") == "OutOfRange", "刀的规则拒绝远处目标")
		else:
			check(body == first.body and identity == first.identity, "换枪不改变稳定身份、身体或头像缓存键")
			check(pixels(canvas._portraits[PLAYER]) == first.body_pixels and pixels(canvas._identity_portraits[PLAYER]) == first.avatar_pixels, "换枪不改变身体与身份头像真实像素")
			check(canvas._range_polygons != first.range and not canvas._range_polygons.is_empty(), "换枪仍改变画布真实射程覆盖")
			check(damage.get("damage", 0) != first.damage and far.get("ok", false), "换枪仍改变同源伤害与远距离可攻击性")
		if not simple:
			check(body.layers.size() == 1 and body.layers[0].asset_id == "portraits/adult.dedicated.a", "缺战斗方案只回退本人物旧图而非武器全身图")
		check(JSON.stringify(state) == before and host.commands.is_empty(), "显示与武器预览不改世界或提交命令 " + weapon)
		host.remove_child(view); view.free()
	host.queue_free(); await process_frame

## 坏会话缺少真实人物时只保留规则位置，不把作者原型或武器图冒充运行身份。
func check_missing_actor() -> void:
	var values: Dictionary = fixture(); values.state.actors.erase(RIVAL)
	var before: String = JSON.stringify(values.state)
	var host: FixtureHost = FixtureHost.new(); host._data = values.data; root.add_child(host)
	host.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var view: Control = Screen.render(host, host, values.state, "battle.fixture")
	await process_frame
	check(not view.canvas.battle_descriptors[RIVAL].get("ok", false) and not view.canvas.identity_descriptors[RIVAL].get("ok", false), "缺少真实参战档案时两视图均明确失败")
	check(view.canvas._portraits[RIVAL] == null and view.canvas._identity_portraits[RIVAL] == null, "缺少真实参战者不选默认或武器人物图")
	check(JSON.stringify(values.state) == before, "坏会话显示不会补造人物档案")
	host.queue_free(); await process_frame

## 测试清单只在本进程注入；分别覆盖登记简体与缺方案旧像，结束恢复原注册表。
func _run() -> void:
	root.size = Vector2i(1280, 800)
	var original: RefCounted = Art.catalog()
	var registry: RefCounted = Registry.new()
	var path: String = ProjectSettings.globalize_path("res://").path_join("../tests/fixtures/character_presentation/manifest.json")
	var manifest: Dictionary = load("res://core/strict_json.gd").parse_text(FileAccess.get_file_as_string(path)).value
	var configured: Dictionary = registry.configure(manifest)
	check(configured.ok, "真实独立分层测试清单合法 " + str(configured))
	if not configured.ok: quit(1); return
	Art._registry = registry
	await check_weapons(fixture(), true)
	var legacy: Dictionary = fixture(); legacy.data.actors["actor.prototype"].presentation_profile_id = "unregistered.person"
	await check_weapons(legacy, false)
	await check_missing_actor()
	Art._registry = original
	print("CHARACTER_BATTLE_IDENTITY_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
