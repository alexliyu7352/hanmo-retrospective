## 沿正式安装信任链读取1.7组合目录，导出原运行字典并核对表现根；不创建世界或修改存档。
extends SceneTree
const Catalog = preload("res://core/content_catalog.gd")
const Registry = preload("res://ui/presentation_registry.gd")
const Composition = preload("res://ui/adult_appearance_composition.gd")
const Appearance = preload("res://core/domains/identity/actor_appearance.gd")
var checks: int = 0
var failures: Array[String] = []
var details: Dictionary = {}

## 场景树就绪后运行，只接受真实安装catalog、运行字典输出和回执三个路径。
func _initialize() -> void:
	call_deferred("_run")

## 记录每项正式边界，失败不会以默认记录或作者头部代替运行目录。
func _check(valid: bool, label: String) -> bool:
	checks += 1
	if not valid:
		failures.append(label)
		printerr("FAIL: ", label)
	return valid

## 输出必须由实际运行所得；文件无法写入时保留非零失败。
func _write(path: String, value: Dictionary) -> bool:
	var file: FileAccess = FileAccess.open(path, FileAccess.WRITE)
	if file == null: return false
	file.store_string(JSON.stringify(value, "", false, true) + "\n")
	file.close()
	return true

## 用唯一正式Registry接收整包清单，验证192候选逐族反查且失败保留已接受清单。
func _verify(catalog: Dictionary) -> void:
	if not _check(catalog.get("ok", false), "Catalog.read_prepared接受完整安装代：" + str(catalog.get("code", ""))): return
	if not _check(catalog.has("pack_set_digest") and catalog.has("presentation_path"), "运行目录保留正式包身份和表现入口"): return
	details.pack_set_digest = catalog.pack_set_digest
	details.content_digest = catalog.content_digest
	details.schema_sha256 = catalog.schema_sha256
	var registry = Registry.new()
	var resource_base: String = catalog.catalog_path.get_base_dir() + "/"
	var configured: Dictionary = registry.load_from_path(catalog.presentation_path, resource_base)
	details.registry_result = configured
	if not _check(configured.ok, "Registry.configure接受包内真实资源：" + str(configured.get("code", ""))): return
	_check(registry.resource_root() == resource_base, "注册表使用精确安装代基址")
	var content_result: Dictionary = registry.validate_content(catalog.data)
	_check(content_result.ok, "正式内容所有表现引用均在当前清单：" + str(content_result.get("code", "")))
	var manifest: Dictionary = registry.describe()
	var registry_entry: Dictionary = Composition.registry_entry(manifest, resource_base)
	if not _check(registry_entry.ok, "组合根来自正式包登记"): return
	_check(registry_entry.entry.resource_root.begins_with(resource_base + "assets/art/packs/"), "组合素材从包内根读取")
	if not _check(ResourceLoader.exists(registry_entry.entry.session), "正式运行环境包含已登记的共享选件脚本"): return
	details.resource_root = registry_entry.entry.resource_root
	var scenarios: Array = []
	var resolved_count: int = 0
	for scenario in catalog.data.scenarios.values():
		if scenario.get("dynamic_world", {}).get("founder_policy") != Composition.POLICY: continue
		scenarios.append(scenario.id)
		var choices: Array = scenario.get("player_appearance_choices", [])
		_check(choices.size() == 192, "正式新政策情景声明192完整候选")
		for choice in choices:
			var visible: Dictionary = {"format_version": 1, "generator_version": Appearance.VERSION, "base_features": choice.base_features, "attire_style": choice.attire_style, "legacy": false}
			var resolved: Dictionary = Composition.resolve(visible, manifest, resource_base)
			if _check(resolved.ok and resolved.get("record", {}).get("id") == choice.id, "完整候选按本族语义解析：" + choice.id + " " + str(resolved.get("code", ""))): resolved_count += 1
	_check(scenarios.size() == 1, "1.7只登记一个正式组合创建情景")
	details.scenario_ids = scenarios
	details.resolved_choices = resolved_count
	var broken: Dictionary = manifest.duplicate(true)
	broken.portraits[broken.default_portrait].path = "res://assets/art/missing-formal-entry.png"
	var rejected: Dictionary = registry.configure(broken, resource_base)
	_check(not rejected.ok, "缺失实际资源的候选清单拒绝")
	_check(registry.describe() == manifest and registry.resource_root() == resource_base, "拒绝后旧清单和安装根同时保留")

## 首先保存真实Catalog结果供同源码原生消费者使用；回执另列验证范围，绝不宣称世界或GUI通过。
func _run() -> void:
	var args: PackedStringArray = OS.get_cmdline_user_args()
	if args.size() != 3:
		printerr("需要catalog_path、prepared_output、receipt_output三个参数")
		quit(2)
		return
	var started: int = Time.get_ticks_msec()
	var catalog: Dictionary = Catalog.read_prepared(args[0])
	_check(_write(args[1], catalog), "真实运行目录输出成功")
	_verify(catalog)
	var receipt: Dictionary = {"format": "HM-INSTALLED-ADULT-APPEARANCE-CHECK/1", "engine": Engine.get_version_info().string, "catalog_path": args[0], "prepared_output": args[1], "prepared_sha256": FileAccess.get_sha256(args[1]), "checks": checks, "failures": failures, "details": details, "seconds": (Time.get_ticks_msec() - started) / 1000.0, "world_created": false, "gui_tested": false}
	if not _write(args[2], receipt): failures.append("回执无法写入")
	print("INSTALLED_ADULT_APPEARANCE_CHECKS=", checks, " FAILURES=", failures.size())
	quit(0 if failures.is_empty() else 1)
