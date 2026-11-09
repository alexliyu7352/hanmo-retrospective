## 用十二张原正式PNG及生产注册表验证选件双视图；限CPU像素，不冒称实际窗口或Main新局验收。
extends SceneTree
const Picker = preload("res://ui/regional_appearance_picker.gd")
const Registry = preload("res://ui/presentation_registry.gd")
const Accepted = preload("res://ui/character_appearance_adapter.gd")
const Strict = preload("res://core/strict_json.gd")
var checks: int = 0
var failures: int = 0

## 延迟到引擎资源初始化完成再读取正式PNG。
func _init() -> void:
	call_deferred("_run")

## 单条失败影响最终退出码，保留具体代表标签。
func check(value: bool, label: String) -> void:
	checks += 1
	if not value: failures += 1; printerr("FAIL: ", label)

## 从唯一清单保留原成人配方与原图；测试背景仅借同源PNG满足注册表默认字段，不加入游戏清单。
func manifest() -> Dictionary:
	var data: Dictionary = Strict.parse_text(FileAccess.get_file_as_string("res://data/presentation.json")).value
	for id in data.portraits.keys():
		if not id.begins_with("accepted_adult_"): data.portraits.erase(id)
	for id in data.character_profiles.keys():
		if not id.begins_with("accepted_adult.") and id != Accepted.PROFILE_ID: data.character_profiles.erase(id)
	data.components = {}; data.scene_templates = {}
	data.default_portrait = data.portraits.keys()[0]
	data.backgrounds = {"source_fixture": data.portraits[data.default_portrait].duplicate(true)}
	data.default_background = "source_fixture"
	return data

## 按真实按钮元数据选择完整代表，保留UI层同一个输入入口。
func choice_button(node: Node, id: String) -> Button:
	if node is Button and node.get_meta("appearance_choice_id", "") == id: return node
	for child in node.get_children():
		var found: Button = choice_button(child, id)
		if found != null: return found
	return null

## 正式源须通过几何/alpha注册验证，再经真实控件输出半身与头像；二者始终同人。
func _run() -> void:
	var registry = Registry.new()
	var accepted: Dictionary = registry.configure(manifest())
	check(accepted.ok, "正式原PNG经生产注册表校验 " + str(accepted))
	if not accepted.ok: quit(1); return
	var records: Array = Accepted.records()
	var rows: Array = []
	for record in records: rows.append({"id": record.id, "base_features": record.base_features.duplicate(true), "attire_style": record.attire_style})
	var actor: Dictionary = {"id": "actor.apprentice", "display_name": "学徒", "origin": "fictional", "presentation_profile_id": Accepted.PROFILE_ID, "appearance_definition": {"kind": "fixed", "generator_version": "appearance-v1", "base_features": rows[0].base_features.duplicate(true), "attire_style": rows[0].attire_style}}
	var picker: Control = Picker.new(); root.add_child(picker)
	var configured: Dictionary = picker.configure(actor.id, actor, 24, registry, rows)
	check(configured.ok, "正式源默认双图完整合成 " + str(configured))
	if not configured.ok: picker.free(); quit(1); return
	var distinct: Dictionary = {}
	for record in records:
		choice_button(picker, record.id).pressed.emit()
		var half: Dictionary = picker._half.describe()
		var avatar: Dictionary = picker._avatar.describe()
		check(picker.draft().base_features == record.base_features and picker.draft().attire_style == record.attire_style and half.identity_profile_id == record.profile_id, "真实按钮精确代表 " + record.id)
		check(half.identity_key == avatar.identity_key and half.layers == avatar.layers and half.crop != avatar.crop, "同源半身头像身份与裁切 " + record.id)
		var half_image: Image = picker._half.image_copy()
		var avatar_image: Image = picker._avatar.image_copy()
		var half_hash: String = half_image.get_data().hex_encode().sha256_text()
		check(half_image.get_used_rect().has_area() and avatar_image.get_used_rect().has_area() and half_image.get_pixel(0, 0).a == 0 and not distinct.has(half_hash), "真实透明像素且不重复前一代表 " + record.id)
		distinct[half_hash] = true
	check(distinct.size() == 12, "十二个正式完整代表均有独立实际半身像素")
	picker.free(); await process_frame
	print("REGIONAL_APPEARANCE_SOURCE_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
