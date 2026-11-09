## 验证真实选件控件的草案原子性、单次确认、迟到信号与正式世界存读边界。
extends SceneTree
const Picker = preload("res://ui/appearance_picker.gd")
const Registry = preload("res://ui/presentation_registry.gd")
const Appearance = preload("res://core/domains/identity/actor_appearance.gd")
const Strict = preload("res://core/strict_json.gd")
const Kernel = preload("res://core/world_kernel.gd")
const Codec = preload("res://core/save_codec.gd")
var checks: int = 0
var failures: int = 0
var confirmations: Array = []
var cancellations: int = 0

## 等待引擎资源系统就绪后创建实际Control。
func _init() -> void:
	call_deferred("_run")

## 每条行为失败都影响退出码，不以无异常当作通过。
func check(value: bool, label: String) -> void:
	checks += 1
	if not value: failures += 1; printerr("FAIL: ", label)

## 受控候选只来自作者定义，未制作语义也保留实际保存值。
func actor_definition() -> Dictionary:
	return {"id": "actor.player", "display_name": "草案人物", "origin": "fictional", "initial_age_years": 24, "presentation_profile_id": "fixture.person", "appearance_definition": {"kind": "generated", "generator_version": "appearance-v1", "attire_style": "plain", "candidates": {"face_shape": ["round", "angular"], "eyes": ["calm", "soft"], "brows": ["straight"], "nose": ["plain"], "mouth": ["plain"], "hair": ["short", "bun"], "beard": ["none"], "skin_palette": ["warm"], "build": ["slender", "broad"]}}}

## 从真正可见选项控件触发语义选择，不直接修改内部草案。
func choose(picker: Control, feature: String, value: String) -> void:
	var control: OptionButton = picker._choices[feature]
	for index in range(control.item_count):
		if control.get_item_metadata(index) == value:
			check(not control.disabled and not control.is_item_disabled(index), "实际选项可用 " + feature + ":" + value)
			control.select(index); control.item_selected.emit(index); return
	check(false, "实际选项存在 " + value)

## 收集独立信号值，后续篡改收到的字典不能反写控件。
func confirmed(value: Dictionary) -> void:
	confirmations.append(value)

## 取消只记录通知次数，不能替代草案或世界状态。
func cancelled() -> void:
	cancellations += 1

## 使用真实注册表、PNG、作者定义与Kernel验证整个限定草案边界。
func _run() -> void:
	check(load("res://ui/appearance_picker.gd").is_tool(), "草案控件在真实编辑器内启用生命周期回调")
	check(load("res://ui/layered_portrait.gd").is_tool(), "分层画像在作者编辑器内启用只读绘制")
	var manifest: Dictionary = Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/character_presentation/manifest.json")).value
	var registry = Registry.new(); check(registry.configure(manifest).ok, "测试清单经同一注册表加载")
	var actor: Dictionary = actor_definition(); var original: Dictionary = actor.duplicate(true)
	var picker = Picker.new(); root.add_child(picker); picker.size = Vector2(440, 700)
	picker.confirmed.connect(confirmed); picker.cancelled.connect(cancelled)
	check(picker.configure(actor.id, actor, "picker-seed", 24, registry).ok, "正式configure接受作者候选")
	var initial: Dictionary = picker.draft(); var initial_view: Dictionary = picker.describe()
	var pixels: PackedByteArray = picker._portrait.image_copy().get_data()
	check(not picker._confirm.disabled, "合法草案可确认")
	check(picker._choices.eyes.disabled and "未映射" in picker._choices.eyes.get_item_text(0), "未制作眼睛明确禁用而非假装有预览")
	var leaked: Dictionary = picker.draft(); leaked.base_features.face_shape = "not-allowed"
	check(picker.draft() == initial, "读取草案是独立副本")
	var next_face: String = "angular" if initial.base_features.face_shape == "round" else "round"
	choose(picker, "face_shape", next_face)
	check(picker.draft().base_features.face_shape == next_face and picker._portrait.image_copy().get_data() != pixels, "脸形选择实际改变草案与PNG像素")
	check(actor == original and confirmations.is_empty(), "选件不修改作者输入且不提前提交")
	var changed: Dictionary = picker.draft(); var changed_pixels: PackedByteArray = picker._portrait.image_copy().get_data()
	var face: OptionButton = picker._choices.face_shape
	face.item_selected.emit(-1); face.item_selected.emit(99)
	var metadata: Variant = face.get_item_metadata(0); face.set_item_metadata(0, "not-allowed"); face.item_selected.emit(0)
	check(picker.draft() == changed and picker._portrait.image_copy().get_data() == changed_pixels, "坏索引与伪造候选均保留草案和像素")
	face.set_item_metadata(0, metadata)
	picker._choices.eyes.item_selected.emit(1)
	check(picker.draft() == changed, "禁用未映射槽手动发信号也不改值")
	var old_confirm: Button = picker._confirm
	picker.cancel_selection(); picker.close(); picker.confirm_selection()
	check(cancellations == 1 and confirmations.is_empty() and actor == original, "取消关闭重复确定只取消一次且源不变")
	check(picker.configure(actor.id, actor, "picker-seed", 24, registry).ok and picker.draft() == initial and picker.describe() == initial_view, "重开同人同种子不重抽另一份草案")
	old_confirm.pressed.emit(); face.item_selected.emit(0)
	check(confirmations.is_empty() and picker.draft() == initial, "上一代确认和选件迟到信号不作用于新草案")
	choose(picker, "hair", "bun")
	var chosen: Dictionary = picker.draft()
	picker.confirm_selection(); picker.confirm_selection(); picker.cancel_selection()
	check(confirmations.size() == 1 and confirmations[0] == chosen and cancellations == 1, "有效确认恰好一次返回完整合法草案")
	confirmations[0].base_features.face_shape = "external-mutation"
	check(picker.draft() == chosen, "确认信号数据不反写内部状态")
	var invalid: Dictionary = chosen.duplicate(true); invalid.attire_style = "formal"
	check(not picker.configure(actor.id, actor, "picker-seed", 24, registry, invalid).ok, "资源虽有formal但作者未允许的换装仍拒绝")
	picker.confirm_selection(); check(confirmations.size() == 1, "坏配置不能继续确认上一草案")
	var fixed: Dictionary = actor.duplicate(true); fixed.origin = "historical"
	fixed.appearance_definition = {"kind": "fixed", "generator_version": "appearance-v1", "base_features": initial.base_features.duplicate(true), "attire_style": "plain"}
	check(picker.configure(actor.id, fixed, "different-seed", 24, registry).ok, "固定历史人物可只读预览")
	check(picker._choices.face_shape.disabled and picker.draft() == initial, "历史fixed不解锁候选")
	picker.confirm_selection(); check(confirmations.size() == 2 and confirmations[1].is_empty(), "历史确认空草案保持原fixed来源")
	var bad_actor: Dictionary = actor.duplicate(true); bad_actor.origin = "historical"
	check(not picker.configure(actor.id, bad_actor, "seed", 24, registry).ok, "历史generated非法定义不能打开")
	check(picker.configure(actor.id, actor, "seed", -1, registry).ok and picker._confirm.disabled, "未知年龄明确预览降级且不伪造成人可确认")
	picker.confirm_selection(); check(confirmations.size() == 2, "降级组合不能被程序直接确认绕过")
	picker.close()
	check(picker.configure(actor.id, actor, "picker-seed", 24, registry, chosen).ok, "合法草案重开保留实际已选值")
	root.remove_child(picker); picker.confirm_selection(); check(confirmations.size() == 2, "已离树控件拒绝晚确认")
	picker.free(); await process_frame
	check_world_boundary(actor, chosen)
	print("APPEARANCE_PICKER_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)

## 真正交给Kernel的同一草案一次保存实际特征；有限Codec恢复不冒称长期出生回归。
func check_world_boundary(actor: Dictionary, selection: Dictionary) -> void:
	var header: Dictionary = Strict.parse_text(FileAccess.get_file_as_string("res://data/generated/alpha_life.hmcatalog.json")).value
	header.catalog.actors[actor.id].appearance_definition = actor.appearance_definition.duplicate(true)
	header.catalog_sha256 = (JSON.stringify(header.catalog, "", true, true) + "\n").sha256_text()
	var path: String = "user://picker-world-" + str(Time.get_ticks_usec()) + ".json"
	var file = FileAccess.open(path, FileAccess.WRITE); file.store_string(JSON.stringify(header)); file.close()
	var kernel = Kernel.new(); check(kernel.new_game("scenario.alpha_life", path).ok, "基线正式新局成功")
	var before: Dictionary = kernel.snapshot()
	check(kernel.new_game("scenario.alpha_life", path, selection).ok, "控件确认输出直接通过正式新局合同")
	var state: Dictionary = kernel.snapshot(); var appearance: Dictionary = state.actors[actor.id].appearance
	check(appearance.base_features == selection.base_features and appearance.source == "custom", "真实新局保存实际选择和custom来源")
	var comparable: Dictionary = state.duplicate(true); comparable.actors[actor.id].appearance = before.actors[actor.id].appearance
	check(comparable == before and state.rng == before.rng, "外貌确认不改变任何其他世界字段与五条RNG")
	var restored: Dictionary = Codec.decode(Codec.encode(state, 0, "picker-save", kernel.content()), kernel.content())
	check(restored.ok and restored.state == state, "完整Codec读回同人实际特征，不重新生成")
	DirAccess.remove_absolute(path)
