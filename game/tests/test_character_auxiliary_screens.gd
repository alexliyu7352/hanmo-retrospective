## 在正式场所入口验证庄园管事、营务本人使用统一描述；页面往返不修改身份或随机流。
extends "res://tests/test_character_screen_integration.gd"

## 使用原Main实际导航与页面输入，仍保留原有岗位和地点可见性筛选。
func _run() -> void:
	root.size = Vector2i(1280, 800)
	original = Art.catalog().describe()
	main = Main.new(); root.add_child(main); main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	await settle()
	check(main.start_content("scenario.alpha_life", "res://data/generated/alpha_life.hmcatalog.json").ok, "辅助屏真实Main载入正式Alpha")
	await settle()
	if not main._started: await finish(); return
	await click("move_local", "west.estate")
	var before: Dictionary = main.kernel.snapshot()
	await click("estate_open", "estate.riverside")
	var manager: Control = main.find_child("EstateManagerPortrait", true, false)
	check(manager != null and manager.has_method("describe"), "庄园管事使用统一分层控件")
	if manager != null:
		var manager_id: String = before.estates["estate.riverside"].manager_actor_id
		check(manager.describe().actor_id == manager_id, "庄园头像绑定当前真实管事")
		check(manager.describe() == Art.character(manager_id, before, main._data, "half_body"), "庄园不私建另一套人物配方")
		check(manager.image_copy() != null and manager.image_copy().get_used_rect().has_area(), "旧管事兼容画像仍真实可见")
	await check_busy_duties()
	await click("estate_close")
	check(main.kernel.snapshot() == before, "庄务开关不修改整份世界或RNG")
	await click("move_local", "west.street")
	await click("move_local", "west.camp")
	before = main.kernel.snapshot()
	await click("camp_open")
	var portrait: Control = main.find_child("CampIdentityPortrait", true, false)
	check(portrait != null and portrait.has_method("describe"), "营务本人使用统一分层控件")
	if portrait != null:
		check(portrait.describe() == Art.character(before.player_actor_id, before, main._data, "half_body"), "营务头像绑定本人并复用统一描述")
		check(portrait.image_copy() != null and portrait.image_copy().get_used_rect().has_area(), "旧本人兼容画像仍真实可见")
	await click("camp_close")
	check(main.kernel.snapshot() == before, "营务开关不改变身份、RNG或规则状态")
	await click("camp_open")
	portrait = main.find_child("CampIdentityPortrait", true, false)
	check(portrait != null and portrait.describe().actor_id == before.player_actor_id, "重开营务仍绑定同一本人")
	await click("camp_close")
	await finish()

## 只启动一次正式建设以形成占用，再恢复原合法快照；不推进工期或改写领域职责。
func check_busy_duties() -> void:
	var before: Dictionary = main.kernel.snapshot()
	var actor: String = before.player_actor_id
	var request: Dictionary = {"estate_id": "estate.riverside", "facility": "granary"}
	var started: Dictionary = main.kernel.submit({"kind": "estate_build", "params": request, "actor_id": actor, "request_seq": before.request_sequences[actor] + 1, "expected_revision": before.revision})
	check(started.ok, "局部正式建设启动形成占用，不运行建设进度")
	if not started.ok: return
	var busy: Dictionary = main.kernel.snapshot()
	main._render(); await settle()
	check(busy.tick == before.tick and not busy.actors[actor].action_id.is_empty(), "只验证施工占用时刻，没有额外推进半日")
	check_resident_choices(busy, true)
	check(main.kernel.snapshot() == busy, "禁用下拉初始化没有误发职责命令")
	check(main.kernel.restore(before).ok, "通过正式恢复入口回到原合法未占用快照")
	main._render(); await settle()
	check_resident_choices(before, false)
	check(main.kernel.snapshot() == before, "恢复可用后的重绘不产生额外世界提交")
	var option: OptionButton = resident_choice("actor.estate_farmer")
	check(option != null and not option.disabled, "解除占用后实际耕作者恢复可选")
	if option != null and not option.disabled:
		for index in range(option.item_count):
			if option.get_item_metadata(index) == "guard":
				option.select(index); option.item_selected.emit(index); break
		await settle()
		var assigned: Dictionary = main.kernel.snapshot()
		check(assigned.estates["estate.riverside"].duties["actor.estate_farmer"] == "guard" and assigned.request_sequences[actor] == before.request_sequences[actor] + 1, "恢复后一次明确选择只提交一次正确职责")
	check(main.kernel.restore(before).ok, "局部检查结束恢复原快照，后续导航不带测试职责")
	main._render(); await settle()

## 使用稳定人物元数据寻找下拉，节点名可能由Godot规范化，不能拿它当人物ID。
func resident_choice(person: String) -> OptionButton:
	for option in main.find_children("*", "OptionButton", true, false):
		if option.get_meta("resident_actor_id", "") == person: return option
	return null

## 下拉的文字和元数据都必须反映保存职责；全禁用不能把末项当成当前职责。
func check_resident_choices(state: Dictionary, all_disabled: bool) -> void:
	var estate: Dictionary = state.estates["estate.riverside"]
	for person in estate.resident_actor_ids:
		var option: OptionButton = resident_choice(person)
		check(option != null, "居民职责控件存在 " + person)
		if option == null: continue
		var actual: String = estate.duties[person]
		check(option.selected >= 0 and option.get_item_metadata(option.selected) == actual and option.text == load("res://ui/estate_screen.gd").DUTIES[actual], "禁用或恢复后显示实际职责 " + person)
		if all_disabled: check(option.disabled, "施工时职责仍保持禁用 " + person)

## 单独标记辅助界面定点证据，不误计为父套件完整执行。
func finish() -> void:
	Art.catalog().configure(original)
	if is_instance_valid(main): main.queue_free(); await settle()
	print("CHARACTER_AUXILIARY_SCREENS_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
