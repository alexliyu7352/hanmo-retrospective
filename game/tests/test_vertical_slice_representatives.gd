## 最小真实代表只读合成与同脸换衣；不写运行状态，不把CPU图像标为原生输入验收。
extends SceneTree
const Kernel = preload("res://core/world_kernel.gd")
const Art = preload("res://ui/art_catalog.gd")
const Portrait = preload("res://ui/layered_portrait.gd")
const F = preload("res://tests/companion_fixture.gd")
var checks: int = 0
var failures: Array = []
var renders: Array = []
var output: String

## 仅从显式准备工程读取候选，不回退默认生产内容。
func _init() -> void:
	call_deferred("run_checks")

## 记录实际失败并以非零退出阻止误报通过。
func check(value: bool, label: String) -> bool:
	checks += 1
	if not value:
		failures.append(label)
		printerr("FAIL: ", label)
	return value

## 生产解析与合成共用真实作者档案，每幅结果绑定actor和清单身份。
func render(kernel, actor_id: String, view: String, name: String) -> Dictionary:
	var before: Dictionary = kernel.snapshot()
	var descriptor: Dictionary = Art.character(actor_id, before, kernel.content().data, view, {"resolution": [512, 768] if view == "half_body" else [384, 384]})
	if not check(descriptor.ok and descriptor.degradation.is_empty(), "真实代表无降级 " + name): return {}
	var control = Portrait.new()
	if check(control.configure(descriptor, Art.catalog()).ok, "生产控件合成 " + name):
		var image: Image = control.image_copy()
		check(image != null and image.get_used_rect().has_area(), "实际图像非空 " + name)
		check(image.save_png(output.path_join(name + ".png")) == OK, "保存实际图像 " + name)
	control.free()
	check(kernel.snapshot() == before, "只读合成不改变完整世界RNG " + name)
	renders.append({"name": name, "descriptor": descriptor, "appearance": before.actors[actor_id].appearance})
	return descriptor

## 两份正式作者内容分别创建世界，证明更衣仅换衣物，专绘和战斗保留相同身份。
func run_checks() -> void:
	output = OS.get_environment("HANMO_REPRESENTATIVE_OUTPUT")
	var project: String = OS.get_environment("HANMO_REAL_ART_PROJECT")
	if not check(not output.is_empty() and not project.is_empty(), "显式输入输出"):
		finish(); return
	DirAccess.make_dir_recursive_absolute(output)
	if not check(Art.ready_result().ok, "正常唯一清单加载"):
		finish(); return
	var descriptions: Array = []
	var appearances: Array = []
	for variant in ["cloth_plain", "cloth_formal"]:
		var kernel = Kernel.new()
		if not check(kernel.new_game("scenario.alpha_life", project.path_join("prepared/" + variant + ".hmcatalog.json")).ok, "正常作者new_game " + variant): continue
		appearances.append(kernel.snapshot().actors["actor.carter"].appearance)
		descriptions.append(render(kernel, "actor.carter", "half_body", variant))
		render(kernel, "actor.carter", "avatar", variant + "-avatar")
		var dedicated: Dictionary = render(kernel, "actor.shen", "half_body", variant + "-dedicated")
		check(dedicated.layers.size() == 1 and dedicated.layers[0].asset_id == "portraits/sender", "仅复用已登记沈先生专绘")
		var avatar: Dictionary = render(kernel, "actor.shen", "avatar", variant + "-dedicated-avatar")
		var battle: Dictionary = render(kernel, "actor.shen", "battle", variant + "-battle")
		check(battle.layers.size() == 1 and battle.layers[0].asset_id == "portraits/battle_sword", "成人有限静态战斗身体")
		check(avatar.actor_id == battle.actor_id and avatar.identity_key == battle.identity_key and avatar.display_name == battle.display_name, "战斗身体不替换专绘人物身份")
		var child: Dictionary = Art.character("actor.apprentice", kernel.snapshot(), kernel.content().data, "battle")
		check(child.proportion == "child" and child.layers[0].asset_id != "portraits/battle_sword", "儿童不借成人战斗图缩小")
	if check(descriptions.size() == 2 and appearances.size() == 2, "两份同脸换衣配置均完成"):
		check(appearances[0].base_features == appearances[1].base_features, "全部九项基础外貌相同")
		check(descriptions[0].identity_key == descriptions[1].identity_key, "换衣不换身份键")
		check(descriptions[0].layers[0] == descriptions[1].layers[0], "身体整头和脸层逐字段相同")
		check(descriptions[0].layers[1] != descriptions[1].layers[1], "仅独立衣物层变化")
		check(descriptions[0].cache_key != descriptions[1].cache_key, "衣物变化刷新描述缓存")
	var file = FileAccess.open(output.path_join("representatives.json"), FileAccess.WRITE)
	if check(file != null, "结果回执可写"):
		file.store_string(JSON.stringify({"format": "HM-V05-MINIMAL-REPRESENTATIVES/1", "checks": checks, "failures": failures, "renders": renders, "scope": "prepared author source and read-only CPU renders; no OS input, child battle art or real layered granary acceptance"}, "\t"))
		file.close()
	finish()

## 必须存在明确完成标记和零失败；原生输入及视觉矩阵由另一独立门验收。
func finish() -> void:
	print("V05_REPRESENTATIVE_CHECKS=", checks, " FAILURES=", failures.size())
	quit(0 if failures.is_empty() else 1)
