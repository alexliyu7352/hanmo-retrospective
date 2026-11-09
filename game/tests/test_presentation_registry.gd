## 验证表现注册表的扩展、资源检查和失败原子性，不以图像存在代替玩法验收。
extends SceneTree
var checks: int = 0
var failures: int = 0
## 场景树就绪后开始，以便真实加载纹理和界面控件。
func _init() -> void:
	call_deferred("_run")
## 累计断言并保留非零失败退出码。
func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		printerr("FAIL: ", label)
## 使用注册表的公开接口新增资源ID，并核对错误不会替换已生效的数据。
func _run() -> void:
	var path: String = "res://ui/presentation_registry.gd"
	check(FileAccess.file_exists(path), "存在独立表现注册表")
	if not FileAccess.file_exists(path):
		print("PRESENTATION_CHECKS=", checks, " FAILURES=", failures); quit(1); return
	var registry = load(path).new()
	check(registry.load_from_path("res://data/presentation.json").ok, "实际清单载入")
	var original: Dictionary = registry.describe()
	var extended: Dictionary = original.duplicate(true)
	extended.backgrounds.study = {"path": original.backgrounds.hall.path, "entrance": [32, 46]}
	extended.portraits.scholar = {"path": original.portraits.official.path}
	var applied: Dictionary = registry.configure(extended)
	print("REGISTRY_DIAGNOSTIC=", applied)
	check(applied.ok, "新增注册ID无需改动运行代码")
	check(registry.ids("backgrounds").has("study"), "新场所资源可枚举")
	check(registry.texture("backgrounds", "study") == load(original.backgrounds.hall.path), "新ID加载指定资源而非街道替代图")
	check(registry.entrance("study") == Vector2(.32, .46), "入口坐标来自数据")
	var detached: Dictionary = registry.describe()
	detached.backgrounds.clear()
	check(registry.ids("backgrounds").has("study"), "外部快照不能篡改注册表")
	for mode in ["missing", "outside", "default", "coordinate", "type"]:
		var broken: Dictionary = extended.duplicate(true)
		match mode:
			"missing": broken.backgrounds.study.path = "res://assets/art/missing.png"
			"outside": broken.backgrounds.study.path = "/etc/passwd"
			"default": broken.default_background = "not_registered"
			"coordinate": broken.backgrounds.study.entrance = [200, 40]
			"type": broken.portraits.scholar.path = "res://ui/main.gd"
		check(not registry.configure(broken).ok, "拒绝非法清单：" + mode)
		check(registry.describe() == extended, "拒绝后旧清单完整保留：" + mode)
	var content: Dictionary = {"places": {"test.room": {"id": "test.room", "visual_tag": "study"}}, "actors": {"test.actor": {"id": "test.actor", "portrait_kind": "scholar"}}}
	check(registry.validate_content(content).ok, "已注册资源可供内容引用")
	content.actors["test.actor"].portrait_kind = "typo"
	var result: Dictionary = registry.validate_content(content)
	check(not result.ok and result.path.contains("test.actor"), "错误引用定位具体人物")
	var art = load("res://ui/art_catalog.gd")
	check(art.catalog().configure(extended).ok, "共享表现入口接收同一清单")
	var stage = load("res://ui/scene_stage.gd").new(); stage.visual_tag = "study"
	root.add_child(stage)
	var person = load("res://ui/person_token.gd").new(); person.portrait_kind = "scholar"
	root.add_child(person); await process_frame
	check(stage._art == registry.texture("backgrounds", "study"), "实际场景没有固定种类白名单")
	check(person._portrait == registry.texture("portraits", "scholar"), "实际人物没有固定种类白名单")
	check(art.catalog().configure(original).ok, "恢复正常清单")
	stage.queue_free(); person.queue_free(); await process_frame
	print("PRESENTATION_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)
