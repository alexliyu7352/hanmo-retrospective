## 成人代表适配器的无窗口合同检查；仅读取样本，不创建世界、保存或表现节点。
extends SceneTree
const Appearance = preload("res://core/domains/identity/actor_appearance.gd")
const ADAPTER_PATH = "res://ui/character_appearance_adapter.gd"
const CATALOG_PATH = "res://data/accepted_adult_appearances.json"
var _adapter: Script
var _checks: int = 0
var _failures: Array[String] = []

## 缺少实现时明确报告断言失败；全部行为检查完成后按失败数退出。
func _initialize() -> void:
	_check(FileAccess.file_exists(ADAPTER_PATH), "正式成人外观适配器存在")
	if not _failures.is_empty():
		_finish()
		return
	_adapter = load(ADAPTER_PATH)
	_check(_adapter != null and _adapter.can_instantiate(), "适配器可加载")
	if not _failures.is_empty():
		_finish()
		return
	_test_records()
	if not _failures.is_empty():
		_finish()
		return
	_test_visible_inputs()
	_test_catalog()
	_finish()

## 十二组完整参数各自映射到登记资源；复制后的选择不能改写后续解析。
func _test_records() -> void:
	_check(_adapter.PROFILE_ID == "regional.adult_representatives_v1", "固定分派入口标记")
	_check(_adapter.status() == {"ok": true}, "正式目录可完整读取")
	var records: Array[Dictionary] = _adapter.records()
	_check(records.size() == 12, "恰有十二个成人代表")
	if records.size() != 12: return
	var original: Array[Dictionary] = records.duplicate(true)
	var seen: Array[String] = []
	for record in records:
		_check(record.size() == 5, "代表只公开五个作者选择字段")
		_check(not record.id in seen, "代表ID唯一")
		seen.append(record.id)
		var view: Dictionary = _view(record)
		var before: Dictionary = view.duplicate(true)
		var resolved: Dictionary = _adapter.resolve_profile_id(view)
		_check(resolved.get("ok") == true and resolved.get("profile_id") == record.profile_id, "完整元组解析：" + record.id)
		_check(view == before, "解析不改写输入：" + record.id)
		for feature in Appearance.FEATURES:
			var changed: Dictionary = view.duplicate(true)
			changed.base_features[feature] = "unregistered"
			_rejected(changed, "UnmappedAdultAppearance", "九槽均参与精确匹配：" + feature)
		var attire: Dictionary = view.duplicate(true)
		attire.attire_style = "unregistered"
		_rejected(attire, "UnmappedAdultAppearance", "衣着参与精确匹配")
	records[0].base_features.eyes = "changed"
	records[0].label = "已改"
	records.remove_at(1)
	_check(_adapter.records() == original, "返回数组及嵌套特征均为深复制")
	var mixed: Dictionary = _view(original[0])
	mixed.base_features.eyes = original[3].base_features.eyes
	_rejected(mixed, "UnmappedAdultAppearance", "同脸跨代表拼接不获资源绑定")
	var reversed_features: Dictionary = {}
	var keys: Array = original[0].base_features.keys()
	keys.reverse()
	for key in keys: reversed_features[key] = original[0].base_features[key]
	var reordered: Dictionary = _view(original[0])
	reordered.base_features = reversed_features
	_check(_adapter.resolve_profile_id(reordered).get("profile_id") == original[0].profile_id, "语义匹配不依赖特征键序")

## 沿原五字段验证器拒绝私密来源、坏类型、版本与旧人物，不补默认外观。
func _test_visible_inputs() -> void:
	var valid: Dictionary = _view(_adapter.records()[0])
	for key in valid:
		var missing: Dictionary = valid.duplicate(true)
		missing.erase(key)
		_rejected(missing, "InvalidAppearanceProfile", "缺少公开字段：" + key)
	var extra: Dictionary = valid.duplicate(true)
	extra.generation_seed = "hidden"
	_rejected(extra, "InvalidAppearanceProfile", "拒绝第六个公开字段")
	for feature in Appearance.FEATURES:
		var missing: Dictionary = valid.duplicate(true)
		missing.base_features.erase(feature)
		_rejected(missing, "InvalidAppearanceProfile", "缺少基础槽：" + feature)
	var extra_feature: Dictionary = valid.duplicate(true)
	extra_feature.base_features.hair_colour = "black"
	_rejected(extra_feature, "InvalidAppearanceProfile", "拒绝未知基础槽")
	for bad in [null, [], "appearance", 1, true]:
		_rejected(bad, "InvalidAppearanceProfile", "拒绝非字典公开输入")
	for key in valid:
		for bad in [null, [], {}, 1.0, true]:
			if key == "legacy" and bad is bool: continue
			var changed: Dictionary = valid.duplicate(true)
			changed[key] = bad
			var original_check: Dictionary = Appearance.validate_visible(changed)
			_rejected(changed, original_check.get("code", ""), "公开字段坏类型：" + key)
	for feature in Appearance.FEATURES:
		var changed: Dictionary = valid.duplicate(true)
		changed.base_features[feature] = 1
		_rejected(changed, "InvalidAppearanceProfile", "基础槽坏类型：" + feature)
	var unknown_version: Dictionary = valid.duplicate(true)
	unknown_version.generator_version = "appearance-v2"
	_rejected(unknown_version, "UnsupportedAppearanceGenerator", "未知生成器版本")
	unknown_version = valid.duplicate(true)
	unknown_version.format_version = 2
	_rejected(unknown_version, "InvalidAppearanceProfile", "未知公开格式版本")
	var legacy: Dictionary = Appearance.project_visible(Appearance.legacy()).appearance_view
	_rejected(legacy, "UnsupportedAdultAppearanceLegacy", "旧人物不可匹配成人默认样本")
	legacy.base_features = valid.base_features.duplicate(true)
	_rejected(legacy, "InvalidLegacyAppearance", "旧人物与普通特征混合")

## 目录逐项校验必须原子失败；表序变化不改变完整参数与资源身份。
func _test_catalog() -> void:
	var source: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(CATALOG_PATH))
	source.version = int(source.version)
	source.age_domain = [int(source.age_domain[0]), int(source.age_domain[1])]
	var before: Dictionary = source.duplicate(true)
	var checked: Dictionary = _adapter.validate_catalog(source)
	_check(checked.get("ok") == true, "内存目录通过合同校验")
	_check(source == before, "校验不修改作者目录")
	if checked.get("ok") != true: return
	checked.records[0].base_features.eyes = "changed"
	_check(source == before, "目录校验结果深复制")
	var reversed: Dictionary = source.duplicate(true)
	reversed.records.reverse()
	var reversed_check: Dictionary = _adapter.validate_catalog(reversed)
	_check(reversed_check.get("ok") == true, "目录顺序可调整")
	if reversed_check.get("ok") == true:
		for record in reversed_check.records:
			_check(_adapter.resolve_profile_id(_view(record)).get("profile_id") == record.profile_id, "顺序不改变匹配：" + record.id)
	for key in source:
		var missing: Dictionary = source.duplicate(true)
		missing.erase(key)
		_catalog_rejected(missing, "目录缺字段：" + key)
	for key in source:
		for bad in [null, "wrong", {}, true]:
			var changed: Dictionary = source.duplicate(true)
			changed[key] = bad
			_catalog_rejected(changed, "目录字段坏类型：" + key)
	var mutations: Array[Dictionary] = [source.duplicate(true), source.duplicate(true), source.duplicate(true), source.duplicate(true), source.duplicate(true), source.duplicate(true)]
	mutations[0].extra = true
	mutations[1].version = 2
	mutations[2].scope = "other"
	mutations[3].age_domain = [0, 59]
	mutations[4].age_domain = [18, 60]
	mutations[5].records.pop_back()
	for changed in mutations: _catalog_rejected(changed, "目录版本、范围或数量错误")
	var duplicate_id: Dictionary = source.duplicate(true)
	duplicate_id.records[11] = duplicate_id.records[0].duplicate(true)
	_catalog_rejected(duplicate_id, "重复代表ID", "DuplicateAdultAppearanceId")
	var duplicate_tuple: Dictionary = source.duplicate(true)
	duplicate_tuple.records[11].base_features = duplicate_tuple.records[0].base_features.duplicate(true)
	_catalog_rejected(duplicate_tuple, "重复或身份不一致的完整元组")
	for key in source.records[11]:
		var missing: Dictionary = source.duplicate(true)
		missing.records[11].erase(key)
		_catalog_rejected(missing, "末项缺字段也不能暴露前十一项：" + key)
		for bad in [null, [], {}, 1, true]:
			var changed: Dictionary = source.duplicate(true)
			changed.records[11][key] = bad
			_catalog_rejected(changed, "末项坏类型也不能暴露前十一项：" + key)
	var wrong_binding: Dictionary = source.duplicate(true)
	wrong_binding.records[11].profile_id = source.records[0].profile_id
	_catalog_rejected(wrong_binding, "错误资源身份不能冒充另一代表")
	var missing_feature: Dictionary = source.duplicate(true)
	missing_feature.records[11].base_features.erase("eyes")
	_catalog_rejected(missing_feature, "目录记录不能缺九槽")
	var extra_feature: Dictionary = source.duplicate(true)
	extra_feature.records[11].base_features.extra = "unknown"
	_catalog_rejected(extra_feature, "目录记录不能有第十槽")
	var extra_record_field: Dictionary = source.duplicate(true)
	extra_record_field.records[11].path = "res://image.png"
	_catalog_rejected(extra_record_field, "目录记录不能夹带资源路径")
	for bad in [null, [], "record", 1, true]:
		var changed: Dictionary = source.duplicate(true)
		changed.records[11] = bad
		_catalog_rejected(changed, "末项不是字典仍原子拒绝", "InvalidAdultAppearanceRecord")
	for bad_version in [1.0, "1", true]:
		var changed: Dictionary = source.duplicate(true)
		changed.version = bad_version
		_catalog_rejected(changed, "内存版本类型必须为整数", "UnsupportedAdultAppearanceCatalogVersion")
	for bad_ages in [[18.0, 59], [18, 59.0], [18, 59, 60], [18, null]]:
		var changed: Dictionary = source.duplicate(true)
		changed.age_domain = bad_ages
		_catalog_rejected(changed, "成人闭区间精确类型和形状", "InvalidAdultAppearanceAgeDomain")
	for bad in [null, [], "catalog", 1, true]: _catalog_rejected(bad, "拒绝非字典目录")

## 由完整样本构造原公开协议；测试无需伪造来源、种子或世界实体。
func _view(record: Dictionary) -> Dictionary:
	return {"format_version": 1, "generator_version": Appearance.VERSION, "base_features": record.base_features.duplicate(true), "attire_style": record.attire_style, "legacy": false}

## 拒绝结果必须保留明确诊断，并且不携带任何可误用的资源绑定。
func _rejected(view: Variant, code: String, label: String) -> void:
	var result: Dictionary = _adapter.resolve_profile_id(view)
	_check(result.get("ok") == false and result.get("code") == code and not result.has("profile_id"), label)

## 目录任一坏项都使整个结果失效，不返回已通过的记录前缀。
func _catalog_rejected(candidate: Variant, label: String, code: String = "") -> void:
	var result: Dictionary = _adapter.validate_catalog(candidate)
	_check(result.get("ok") == false and result.get("code") is String and not result.get("code", "").is_empty() and not result.has("records") and (code.is_empty() or result.get("code") == code), label)

## 汇总所有可复现断言，不依赖图形节点或异常日志作为成功条件。
func _check(condition: bool, label: String) -> void:
	_checks += 1
	if not condition: _failures.append(label)

## 只输出检查数与具体失败项，方便单独无窗口脚本和组合门读取。
func _finish() -> void:
	print("CHARACTER_APPEARANCE_ADAPTER_CHECKS=",_checks," FAILURES=",_failures.size())
	print(JSON.stringify({"test": "character_appearance_adapter", "ok": _failures.is_empty(), "checks": _checks, "failures": _failures}))
	quit(0 if _failures.is_empty() else 1)
