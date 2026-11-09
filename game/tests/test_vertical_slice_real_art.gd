## 隔离纵切片候选人物：只从正常作者目录和唯一清单渲染，不注入年龄、特征或运行状态。
extends SceneTree
const Kernel = preload("res://core/world_kernel.gd")
const Art = preload("res://ui/art_catalog.gd")
const Portrait = preload("res://ui/layered_portrait.gd")
const Codec = preload("res://core/save_codec.gd")
const Native = preload("res://storage/native_storage.gd")
var checks: int = 0
var failures: Array = []
var renders: Array = []
var output: String

## 待引擎初始化后加载显式目录，未准备数据不能冒充验收。
func _init() -> void:
	call_deferred("run_checks")

## 保留具名失败，不让脚本异常或零检查变成PASS。
func check(value: bool, label: String) -> bool:
	checks += 1
	if not value:
		failures.append(label)
		printerr("FAIL: ", label)
	return value

## 同一真实人物同时用半身及头像查询，检查互斥body与独立衣服、比例及透明图像。
func render_actor(kernel, variant: String, actor_id: String) -> void:
	var state: Dictionary = kernel.snapshot()
	var appearance: Dictionary = state.actors[actor_id].appearance
	var half: Dictionary = Art.character(actor_id, state, kernel.content().data, "half_body", {"resolution": [512, 768]})
	if not check(half.ok and half.degradation.is_empty(), "真实作者半身无降级 " + variant + actor_id): return
	var band: String = "child" if actor_id == "actor.apprentice" else "adult"
	var face: String = appearance.base_features.face_shape
	var cloth: String = "olive" if appearance.attire_style == "formal" else "blue"
	var prefix: String = "v05.sample." + band
	check(half.proportion == band and half.body_basis == prefix + ".front", "使用独立年龄比例 " + actor_id)
	check(half.layers.size() == 2, "仅一个body加一个garment " + actor_id)
	check(half.layers[0].id == prefix + ".body." + face and half.layers[1].id == prefix + ".garment." + cloth, "作者语义决定互斥整头和衣服 " + actor_id)
	check(appearance.source == ("generated" if actor_id == "actor.player" else "fixed"), "原外貌来源不变 " + actor_id)
	if actor_id == "actor.carter":
		check(appearance.attire_style == ("plain" if variant == "a" else "formal"), "第二配置持久化语义仍为formal")
	var avatar: Dictionary = Art.character(actor_id, state, kernel.content().data, "avatar", {"resolution": [256, 256]})
	check(avatar.ok and avatar.layers == half.layers and avatar.identity_key == half.identity_key, "头像复用相同人物层和身份 " + actor_id)
	check(avatar.crop == ([95.0, 60.0, 330.0, 375.0] if band == "child" else [90.0, 15.0, 330.0, 355.0]), "沿用候选安全裁切 " + actor_id)
	var control = Portrait.new()
	for descriptor in [half, avatar]:
		var result: Dictionary = control.configure(descriptor, Art.catalog())
		if not check(result.ok, "生产LayeredPortrait实际合成 " + actor_id): continue
		var image: Image = control.image_copy()
		var stem: String = variant + "-" + actor_id.trim_prefix("actor.") + "-" + descriptor.view_kind
		check(image != null and image.get_used_rect().has_area() and image.get_pixel(0, 0).a == 0.0, "透明像素与人物内容非空 " + stem)
		check(image.save_png(output.path_join(stem + ".png")) == OK, "保存实际引擎渲染 " + stem)
		renders.append({"variant": variant, "actor_id": actor_id, "appearance": appearance, "descriptor": descriptor, "image": stem + ".png"})
	control.free()
	check(kernel.snapshot() == state, "渲染不改变完整世界或RNG " + actor_id)

## 两配置分别正常new_game并经真实存储写读；此测试不替代跨进程/原生窗口验收。
func run_checks() -> void:
	output = OS.get_environment("HANMO_REAL_ART_OUTPUT")
	var project: String = OS.get_environment("HANMO_REAL_ART_PROJECT")
	if not check(not output.is_empty() and not project.is_empty(), "显式隔离工程和证据目录"):
		finish(); return
	DirAccess.make_dir_recursive_absolute(output)
	if not check(Art.ready_result().ok, "唯一res清单正常载入"):
		finish(); return
	for variant in ["a", "b"]:
		var kernel = Kernel.new()
		var loaded: Dictionary = kernel.new_game("scenario.alpha_life", project.path_join("prepared/" + variant + ".hmcatalog.json"))
		if not check(loaded.ok, "正常读取已保存重开的作者目录 " + variant + " " + str(loaded)): continue
		var state: Dictionary = kernel.snapshot()
		for actor_id in ["actor.player", "actor.carter", "actor.apprentice"]:
			render_actor(kernel, variant, actor_id)
		var store = Native.new(output.path_join("save-" + variant), kernel.content())
		var saved: Dictionary = store.put("manual-1", Codec.encode(state, 1, "manual-1", kernel.content()), 0)
		if check(saved.ok, "真实NativeStorage保存 " + variant):
			var reopened: Dictionary = store.get_slot("manual-1")
			if check(reopened.ok, "真实NativeStorage读取 " + variant):
				var decoded: Dictionary = Codec.decode(reopened.text, kernel.content())
				check(decoded.ok and decoded.state == state, "完整世界与实际外貌保存后逐字段一致 " + variant)
		check(kernel.snapshot() == state, "两视图及保存不写世界 " + variant)
	var report: Dictionary = {"format": "HM-V05-REAL-ART-LOCAL/1", "engine": Engine.get_version_info().string, "checks": checks, "failures": failures, "renders": renders, "scope": "normal prepared author catalogs, read-only CPU rendering and same-process NativeStorage; no native input, age transition, battle art, dedicated art or window acceptance"}
	var file = FileAccess.open(output.path_join("render-report.json"), FileAccess.WRITE)
	if check(file != null, "渲染证据可写"):
		report.checks = checks
		file.store_string(JSON.stringify(report, "\t"))
		file.close()
	finish()

## 固定完成标记与失败退出码，保留检查数量供外层核对。
func finish() -> void:
	print("V05_REAL_ART_CHECKS=", checks, " FAILURES=", failures.size())
	quit(0 if failures.is_empty() else 1)
