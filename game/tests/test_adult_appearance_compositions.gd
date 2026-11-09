## 正式192组合的定点引擎门；读取独立工坊真实RGBA参照，旧十二preset仍走旧路径。
extends SceneTree
const Composition = preload("res://ui/adult_appearance_composition.gd")
const Renderer = preload("res://ui/adult_appearance_renderer.gd")
const Accepted = preload("res://ui/character_appearance_adapter.gd")
const Character = preload("res://ui/character_presentation.gd")
const Portrait = preload("res://ui/layered_portrait.gd")
const Registry = preload("res://ui/presentation_registry.gd")
const Catalog = preload("res://core/content_catalog.gd")
const Art = preload("res://ui/art_catalog.gd")
const Strict = preload("res://core/strict_json.gd")
var checks: int = 0
var failures: Array[String] = []
var _resource_base: String = "res://"
var _binding_id: String = Accepted.PROFILE_ID
var _binding_prefix: String = ""
var _catalog_path: String = ""

## 必须由同一锁定引擎生成工坊参照；缺参照明确失败，不把未比对算作通过。
func _initialize() -> void:
	var args: PackedStringArray = OS.get_cmdline_user_args()
	if args.size() < 1 or args.size() > 2: printerr("需要工坊192元组RGBA参照JSON绝对路径，可另传真实已安装catalog路径。"); quit(2); return
	var reference: Dictionary = Strict.parse_text(FileAccess.get_file_as_string(args[0]))
	_check(reference.get("ok", false), "工坊参照可读")
	if not reference.get("ok", false): _finish(); return
	_check(reference.value.engine == Engine.get_version_info().string and reference.value.count == 192, "工坊与正式使用同一引擎及完整192元组")
	var registry: RefCounted = _read_registry(args)
	if registry == null: _finish(); return
	var manifest: Dictionary = registry.describe()
	var rows: Array[Dictionary] = Composition.records()
	_check(rows.size() == 192, "完整192组合")
	_test_catalog()
	var old_rows: Array[Dictionary] = Accepted.records()
	var old_profiles: Dictionary = {}
	for row in old_rows: old_profiles[JSON.stringify(row.base_features, "", true)] = _binding_prefix + row.profile_id
	var distinct: Dictionary = {}
	var mutated: Dictionary = {}
	for row in rows:
		var view: Dictionary = _view(row)
		var before: Dictionary = view.duplicate(true)
		var pair: Dictionary = Renderer.render_pair(view, manifest, _resource_base)
		_check(pair.ok and pair.has("half_body") and pair.has("avatar"), "整对合成：" + row.id)
		if not pair.ok: continue
		var hashes: Dictionary = reference.value.records.get(row.id, {})
		_check(pair.half_body.get_size() == Vector2i(512, 768), "半身尺寸：" + row.id)
		var avatar: Image = pair.avatar.duplicate()
		avatar.resize(96, 96, Image.INTERPOLATE_LANCZOS)
		_check(_hash(pair.half_body) == hashes.get("half_body") and _hash(avatar) == hashes.get("avatar96"), "工坊正式逐像素同源：" + row.id)
		distinct[_hash(pair.half_body)] = true
		_check(view == before, "渲染不改原九槽：" + row.id)
		var actor: Dictionary = {"actor_id": "actor.preview", "age_years": 24, "presentation_profile_id": _binding_id}
		var descriptor: Dictionary = Character.resolve_visible(actor.actor_id, actor, view, "half_body", {"resolution": [256, 384]}, registry)
		_check(descriptor.ok, "正式人物解析：" + row.id)
		var old_key: String = JSON.stringify(row.base_features, "", true)
		if old_profiles.has(old_key):
			_check(not descriptor.has("adult_composition") and descriptor.get("identity_profile_id") == old_profiles[old_key], "旧preset优先原profile：" + row.id)
		else:
			_check(descriptor.has("adult_composition"), "新元组走正式合成叶：" + row.id)
			mutated = view
	_check(distinct.size() == 192, "192组合均有真实独立像素")
	_test_failures(mutated, registry)
	_test_mounted_bytes(mutated, manifest)
	_finish()

## 可选安装入口必须重新通过正式Catalog与Art信任链；整包清单、命名空间和真实根不做裁减。
func _read_registry(args: PackedStringArray) -> RefCounted:
	var registry: RefCounted
	if args.size() == 2:
		_catalog_path = args[1]
		var catalog: Dictionary = Catalog.read_prepared(_catalog_path)
		_check(catalog.get("ok", false) and catalog.has("pack_set_digest"), "真实安装catalog接受：" + str(catalog.get("code", "")))
		if not catalog.get("ok", false) or not catalog.has("pack_set_digest"): return null
		var prepared: Dictionary = Art.prepare_content(catalog)
		_check(prepared.get("ok", false), "完整安装表现与内容引用接受：" + str(prepared.get("code", "")))
		if not prepared.get("ok", false): return null
		registry = prepared.registry
		_resource_base = catalog.catalog_path.get_base_dir() + "/"
		_check(registry.resource_root() == _resource_base, "正式注册表保持安装代根")
		var bindings: Array[String] = []
		for id in registry.describe().character_profiles:
			if id.ends_with("::" + Accepted.PROFILE_ID): bindings.append(id)
		_check(bindings.size() == 1, "同一整包清单唯一成人代表绑定")
		if bindings.size() != 1: return null
		_binding_id = bindings[0]
		_binding_prefix = _binding_id.trim_suffix(Accepted.PROFILE_ID)
	else:
		registry = Registry.new()
		var configured: Dictionary = registry.load_from_path("res://data/presentation.json")
		_check(configured.ok, "正式唯一表现清单可加载")
		if not configured.ok: return null
	return registry

## 末项坏数据、重复元组和扩展字段原子拒绝，不交出192表中的合法前缀。
func _test_catalog() -> void:
	var source: Dictionary = Strict.parse_text(FileAccess.get_file_as_string(Composition.CATALOG_PATH)).value
	var mutations: Array[Dictionary] = []
	var duplicate: Dictionary = source.duplicate(true)
	duplicate.records[-1] = duplicate.records[0].duplicate(true)
	mutations.append(duplicate)
	var wrong_age: Dictionary = source.duplicate(true)
	wrong_age.age_domain = [18.0, 59]
	mutations.append(wrong_age)
	var extra: Dictionary = source.duplicate(true)
	extra.records[-1].base_features.hair_color = "black"
	mutations.append(extra)
	var missing: Dictionary = source.duplicate(true)
	missing.records[-1].base_features.erase("nose")
	mutations.append(missing)
	var mismatched: Dictionary = source.duplicate(true)
	mismatched.faces[-1].fitting_id = "outside"
	mutations.append(mismatched)
	for candidate in mutations:
		var before: Dictionary = candidate.duplicate(true)
		var rejected: Dictionary = Composition.validate_catalog(candidate)
		_check(not rejected.ok and not rejected.has("catalog") and not rejected.has("records"), "坏目录原子拒绝")
		_check(candidate == before, "坏目录校验不修改输入")

## 临时安装根只复制选中人物必要素材；同秒损坏和缺安装文件都不能从旧缓存或源工程补图。
func _test_mounted_bytes(view: Dictionary, manifest: Dictionary) -> void:
	if view.is_empty(): return
	var digest: String = (str(OS.get_process_id()) + "adult-composition-test").sha256_text()
	var resource_base: String = "res://data/generated/content-sets/" + digest + "/"
	var logical_root: String = "res://assets/art/packs/composition_probe/character_studio/"
	var actual_root: String = resource_base + logical_root.trim_prefix("res://")
	var catalog_path: String = "res://assets/art/packs/composition_probe/data/adult_appearance_compositions.json"
	var copied: Array[String] = []
	var packaged: Dictionary = manifest.duplicate(true)
	packaged[Composition.REGISTRY_KEY] = {"catalog": catalog_path, "resource_root": logical_root, "fittings": logical_root + "fittings.json", "representatives": logical_root + "representatives.json", "renderer": "res://ui/adult_appearance_renderer.gd", "session": "res://character_studio/src/studio_session.gd"}
	var original: Dictionary = Composition.resolve(view, manifest, _resource_base)
	_check(original.ok, "安装故障夹具来源有效")
	if not original.ok: return
	_check(_copy_source(original.registry.catalog, resource_base + catalog_path.trim_prefix("res://"), copied), "安装192完整目录")
	for relative in ["fittings.json", "representatives.json", "catalog_medium_v3.json", "catalog_female_v2.json", "catalog_long_v2.json"]:
		_check(_copy_source(original.registry.resource_root + relative, actual_root + relative, copied), "安装来源目录：" + relative)
	var definition: Dictionary = original.definition
	var state: Dictionary = original.selection
	var ids: Array[String] = ["head-" + str(state.face), "hair-" + str(state.face), "outfit-" + str(state.outfit)]
	for feature in Composition.FEATURES:
		var id: String = definition.face_profiles[state.face].feature_sources[feature][state[feature]]
		if not id in ids: ids.append(id)
	for id in ids:
		var relative: String = definition.assets[id].trim_prefix("res://")
		_check(_copy_source(original.registry.resource_root + relative, actual_root + relative, copied), "安装必要素材：" + id)
	var first: Dictionary = Renderer.render_pair(view, packaged, resource_base)
	_check(first.ok, "真实安装资源根可合成")
	var source_path: String = original.registry.resource_root + definition.assets[ids[0]].trim_prefix("res://")
	var target_path: String = actual_root + definition.assets[ids[0]].trim_prefix("res://")
	var source_bytes: PackedByteArray = FileAccess.get_file_as_bytes(source_path)
	# 不等待mtime变化，紧接成功缓存后用空文件制造真实坏图。
	var file: FileAccess = FileAccess.open(target_path, FileAccess.WRITE)
	_check(file != null, "可注入临时安装文件截断")
	if file != null:
		file.close()
		var failed: Dictionary = Renderer.render_pair(view, packaged, resource_base)
		_check(not failed.ok and not failed.has("avatar") and not failed.has("half_body"), "同秒截断不命中旧整对缓存")
		file = FileAccess.open(target_path, FileAccess.WRITE)
		file.store_buffer(source_bytes)
		file.close()
		var restored: Dictionary = Renderer.render_pair(view, packaged, resource_base)
		_check(restored.ok and first.ok and _hash(restored.half_body) == _hash(first.half_body), "恢复真实源后可恢复原像素")
		DirAccess.remove_absolute(ProjectSettings.globalize_path(target_path))
		var missing: Dictionary = Renderer.render_pair(view, packaged, resource_base)
		_check(not missing.ok, "安装缺件不借全局工坊副本")
	for path in copied:
		if FileAccess.file_exists(path): DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	# 只回收本测试创建且已清空的目录；不扫描或删除其他内容集。
	var directories: Array[String] = []
	for path in copied:
		var directory: String = path.get_base_dir()
		while directory.begins_with(resource_base.trim_suffix("/")):
			if not directory in directories: directories.append(directory)
			directory = directory.get_base_dir()
	directories.sort_custom(func(left: String, right: String) -> bool: return left.length() > right.length())
	for directory in directories: DirAccess.remove_absolute(ProjectSettings.globalize_path(directory))

## 按显式目标复制已知源字节；仅供临时安装夹具，不修改工坊或原素材。
func _copy_source(source: String, target: String, copied: Array[String]) -> bool:
	if not FileAccess.file_exists(source): return false
	if DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(target.get_base_dir())) != OK: return false
	var file: FileAccess = FileAccess.open(target, FileAccess.WRITE)
	if file == null: return false
	file.store_buffer(FileAccess.get_file_as_bytes(source))
	file.close()
	copied.append(target)
	return true

## 年龄、未知语义、登记根及伪造描述均失败；已配置控件保留原完整像素。
func _test_failures(view: Dictionary, registry: RefCounted) -> void:
	if view.is_empty(): _check(false, "至少有新组合可测试失败边界"); return
	var manifest: Dictionary = registry.describe()
	var bad: Dictionary = view.duplicate(true)
	bad.base_features.skin_palette = "tanned"
	_check(not Composition.resolve(bad, manifest, _resource_base).ok, "不扩展肤色")
	bad = view.duplicate(true)
	bad.base_features.hair = "headwrap"
	_check(not Composition.resolve(bad, manifest, _resource_base).ok, "不扩展发冠")
	var missing: Dictionary = manifest.duplicate(true)
	missing.erase(Composition.REGISTRY_KEY)
	var missing_result: Dictionary = Renderer.render_pair(view, missing, _resource_base)
	_check(not missing_result.ok and not missing_result.has("half_body") and not missing_result.has("avatar"), "缺登记不发布单边")
	var actor: Dictionary = {"actor_id": "actor.preview", "age_years": 24, "presentation_profile_id": _binding_id}
	for age in [17, 60]:
		actor.age_years = age
		_check(Character.resolve_visible(actor.actor_id, actor, view, "half_body", {}, registry).get("code") == "UnsupportedAppearanceAge", "未制作年龄拒绝：" + str(age))
	actor.age_years = 24
	var descriptor: Dictionary = Character.resolve_visible(actor.actor_id, actor, view, "avatar", {"resolution": [96, 96]}, registry)
	var control: Control = Portrait.new()
	_check(control.configure(descriptor, registry).ok, "新组合头像控件可配置")
	var original: Image = control.image_copy()
	var tampered: Dictionary = descriptor.duplicate(true)
	tampered.crop = [0, 0, 512, 512]
	_check(not control.configure(tampered, registry).ok, "拒绝伪造头像裁框")
	_check(original != null and control.image_copy().get_data() == original.get_data(), "失败保留上一张像素")
	control.free()

## 测试只使用公开五字段外观，不创建世界ID、来源或随机种子。
func _view(row: Dictionary) -> Dictionary:
	return {"format_version": 1, "generator_version": "appearance-v1", "base_features": row.base_features.duplicate(true), "attire_style": row.attire_style, "legacy": false}

## 摘要与工坊导出使用同一原始RGBA字节协议。
func _hash(image: Image) -> String:
	var context: HashingContext = HashingContext.new()
	context.start(HashingContext.HASH_SHA256)
	context.update(image.get_data())
	return context.finish().hex_encode()

## 各断言独立记录，不能靠日志存在推断测试通过。
func _check(value: bool, label: String) -> void:
	checks += 1
	if not value: failures.append(label)

## 以明确退出码记录原始结果；失败和未运行均不得包装为完成。
func _finish() -> void:
	print(JSON.stringify({"test": "adult_appearance_compositions", "ok": failures.is_empty(), "checks": checks, "failures": failures, "catalog_path": _catalog_path, "resource_base": _resource_base, "binding_id": _binding_id}))
	quit(0 if failures.is_empty() else 1)
