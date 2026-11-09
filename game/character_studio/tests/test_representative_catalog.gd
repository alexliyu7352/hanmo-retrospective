## 十二名已验收代表目录回归；只读已有取样证据，不生成图像、存档或世界状态。
extends SceneTree
const Fittings = preload("res://src/fitting_catalog.gd")
const Session = preload("res://src/fitting_session.gd")
const CATALOG_PATH := "res://src/representative_catalog.gd"
var checks := 0
var failures: Array[String] = []

## 延后执行只读目录核对，缺少实现时明确失败而非因预加载错误中断。
func _init() -> void:
	_run.call_deferred()

## 核对真实十二选值、稳定族解析、失败原子性以及来源和结果的隔离。
func _run() -> void:
	_check(FileAccess.file_exists(CATALOG_PATH), "代表目录读取模块存在")
	if not failures.is_empty():
		_finish()
		return
	var samples_api = load(CATALOG_PATH)
	var registry: Dictionary = Fittings.read_registry()
	_check(registry.ok, "真实比例族目录可读")
	if not registry.ok:
		_finish()
		return
	var catalog: Dictionary = registry.catalog
	var result: Dictionary = samples_api.read(catalog)
	_check(result.get("ok", false), "正式代表目录完整通过验证")
	_check(result.get("samples") is Array and result.samples.size() == 12, "严格保留十二个已验收代表")
	if not result.get("ok", false) or result.samples.size() != 12:
		_finish()
		return
	_check(result.get("reason") == "", "成功返回统一空原因")
	_verify_source(result.samples, catalog)
	var definition: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://representatives.json"))
	_verify_isolation(samples_api, definition, catalog)
	_verify_reordered_fittings(samples_api, definition, catalog)
	_verify_rejections(samples_api, definition, catalog)
	_finish()

## 逐项对照原始取样证据及正式族入口，全部选值必须精确重现而非补默认值。
func _verify_source(samples: Array, catalog: Dictionary) -> void:
	var evidence: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://evidence/twelve-representatives.json"))
	var entries: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://fittings.json"))
	var stable_ids := ["medium_clear_long", "medium_square_jaw", "medium_round_chin", "medium_high_cheek", "female_round_broad", "female_straight_jaw", "female_short_chin", "female_high_cheek", "long_narrow", "long_straight_jaw", "long_full_oval", "long_high_cheek"]
	var stable_labels := ["清朗长脸", "宽颌方脸", "丰颊圆颏", "高颧收颌", "圆阔脸", "宽直颌", "丰颊短颏", "高颧收颊", "长窄脸", "长直颌", "丰颊长卵脸", "高颧收颊"]
	var session := Session.new()
	_check(session.configure(catalog).ok, "既有完整会话验证器可用")
	var seen := {}
	for index in 12:
		var sample: Dictionary = samples[index]
		var original: Dictionary = evidence.samples[index]
		var expected: Dictionary = original.selection.duplicate(true)
		for field in expected:
			expected[field] = int(expected[field])
		var fitting_id := ""
		for entry in entries.fittings:
			if entry.catalog == original.catalog:
				fitting_id = entry.id
		var rig := -1
		for fitting_index in catalog.fittings.size():
			if catalog.fittings[fitting_index].id == fitting_id:
				rig = fitting_index
		expected.rig = rig
		_check(sample.state == expected, "第%d名全部选值与已验收证据完全相同" % index)
		_check(session.validate(sample.state).ok, "第%d名通过既有完整状态验证" % index)
		_check(sample.get("fitting_id") == fitting_id, "第%d名使用稳定比例族ID" % index)
		_check(sample.get("fitting_label") == catalog.fittings[rig].label, "第%d名显示真实比例族名称" % index)
		_check(sample.get("id") is String and not sample.id.is_empty() and not seen.has(sample.id), "第%d名ID非空且唯一" % index)
		_check(sample.get("label") is String and not sample.label.strip_edges().is_empty(), "第%d名代表名称非空" % index)
		_check(sample.id == stable_ids[index], "第%d名保留固定样本ID" % index)
		_check(sample.label == stable_labels[index], "第%d名保留已验收结构名称" % index)
		seen[sample.id] = true
		for field in sample.state:
			_check(sample.state[field] is int, "第%d名%s传入会话前是整数" % [index, field])

## 修改来源、返回状态和重复读取结果时，其他快照与外部目录不得被连带改写。
func _verify_isolation(api: GDScript, definition: Dictionary, catalog: Dictionary) -> void:
	var source: Dictionary = definition.duplicate(true)
	var catalog_before: Dictionary = catalog.duplicate(true)
	var source_before: Dictionary = source.duplicate(true)
	var first: Dictionary = api.validate_definition(source, catalog)
	var second: Dictionary = api.validate_definition(source, catalog)
	_check(first.ok and second.ok, "相同来源可重复独立读取")
	_check(source == source_before and catalog == catalog_before, "读取和验证完全不改来源或族目录")
	var stable_id: String = first.samples[0].id
	var baseline: Dictionary = first.samples[0].state.duplicate(true)
	source.samples[0].selection.face = 3
	source.samples[0].id = "changed_source"
	_check(first.samples[0].state == baseline and first.samples[0].id == stable_id, "来源后续改动不污染已返回代表")
	first.samples[0].state.face = 3
	first.samples[0].label = "changed_result"
	_check(second.samples[0].state == baseline and second.samples[0].label != "changed_result", "返回数据修改不影响其他读取快照")
	_check(second.samples[1].state.face == 1, "代表之间没有共享选值字典")
	_check(api.read(catalog).samples == second.samples, "再次读盘仍保留原始代表")
	_check(catalog == catalog_before, "输出修改不污染原比例族目录")

## 打乱运行时比例族顺序，代表身份仍按稳定ID匹配而非固定编号。
func _verify_reordered_fittings(api: GDScript, definition: Dictionary, catalog: Dictionary) -> void:
	var reordered: Dictionary = catalog.duplicate(true)
	reordered.fittings.reverse()
	reordered.fields.rig.options.reverse()
	var result: Dictionary = api.validate_definition(definition, reordered)
	_check(result.ok and result.samples.size() == 12, "比例族排序变化仍能读取全部代表")
	if not result.ok:
		return
	for sample in result.samples:
		_check(reordered.fittings[sample.state.rig].id == sample.fitting_id, "重排后按稳定ID解析 " + sample.id)
		_check(reordered.fittings[sample.state.rig].label == sample.fitting_label, "重排后名称与状态同族 " + sample.id)
	_check(result.samples[0].state.rig == 2 and result.samples[8].state.rig == 0, "原首族和末族确实采用新的运行编号")

## 非法末项也拒绝整批，覆盖非整数、缺项、未知字段、索引、重复和兼容冲突。
func _verify_rejections(api: GDScript, definition: Dictionary, catalog: Dictionary) -> void:
	for invalid in [null, [], "samples", {}, {"version": 1}, {"version": 1, "samples": []}, {"version": 1, "samples": {}}]:
		_rejected(api.validate_definition(invalid, catalog), "非法顶层定义")
	var invalid: Dictionary = definition.duplicate(true)
	invalid.version = 2
	_rejected(api.validate_definition(invalid, catalog), "未知目录版本")
	invalid = definition.duplicate(true)
	invalid.extra = true
	_rejected(api.validate_definition(invalid, catalog), "未知顶层字段")
	invalid = definition.duplicate(true)
	invalid.samples.pop_back()
	_rejected(api.validate_definition(invalid, catalog), "版本1不能漏掉整名代表")
	invalid = definition.duplicate(true)
	var extra: Dictionary = invalid.samples[0].duplicate(true)
	extra.id = "extra_valid_selection"
	extra.selection.nose = 1
	invalid.samples.append(extra)
	_rejected(api.validate_definition(invalid, catalog), "版本1不能暗增第十三名代表")
	invalid = definition.duplicate(true)
	invalid.samples[1].selection.face = 0
	_rejected(api.validate_definition(invalid, catalog), "不能同脸型换眉冒充第四种结构")
	invalid = definition.duplicate(true)
	invalid.samples[8].fitting_id = invalid.samples[0].fitting_id
	_rejected(api.validate_definition(invalid, catalog), "不能改变每族四名结构的覆盖")
	for field in ["id", "label", "fitting_id", "selection"]:
		invalid = definition.duplicate(true)
		invalid.samples[-1].erase(field)
		_rejected(api.validate_definition(invalid, catalog), "缺少代表字段 " + field)
	for field in ["id", "label", "fitting_id"]:
		for value in ["", " ", 1, null]:
			invalid = definition.duplicate(true)
			invalid.samples[-1][field] = value
			_rejected(api.validate_definition(invalid, catalog), "非法代表文本 " + field)
	invalid = definition.duplicate(true)
	invalid.samples[-1].fitting_id = "unknown_fitting"
	_rejected(api.validate_definition(invalid, catalog), "未知比例族ID")
	invalid = definition.duplicate(true)
	invalid.samples[-1].id = invalid.samples[0].id
	_rejected(api.validate_definition(invalid, catalog), "重复代表ID")
	invalid = definition.duplicate(true)
	invalid.samples[-1] = invalid.samples[0].duplicate(true)
	invalid.samples[-1].id = "different_id_same_selection"
	_rejected(api.validate_definition(invalid, catalog), "换ID重复同一人物选值")
	invalid = definition.duplicate(true)
	invalid.samples[-1].unknown = 0
	_rejected(api.validate_definition(invalid, catalog), "未知代表字段")
	invalid = definition.duplicate(true)
	invalid.samples[-1] = null
	_rejected(api.validate_definition(invalid, catalog), "非法代表条目")
	for value in [null, [], "selection"]:
		invalid = definition.duplicate(true)
		invalid.samples[-1].selection = value
		_rejected(api.validate_definition(invalid, catalog), "非法选值字典")
	for field in definition.samples[-1].selection:
		invalid = definition.duplicate(true)
		invalid.samples[-1].selection.erase(field)
		_rejected(api.validate_definition(invalid, catalog), "不补默认的缺选项 " + field)
	for value in [0.5, -1, 99, "0", false, null, [], {}, INF, NAN, 1e30]:
		invalid = definition.duplicate(true)
		invalid.samples[-1].selection.face = value
		_rejected(api.validate_definition(invalid, catalog), "非法非整数或越界选值 " + str(value))
	for field in ["rig", "unknown"]:
		invalid = definition.duplicate(true)
		invalid.samples[-1].selection[field] = 0
		_rejected(api.validate_definition(invalid, catalog), "禁止硬编码族编号或未知选项 " + field)
	invalid = definition.duplicate(true)
	invalid.samples[-1].selection.erase("nose")
	invalid.samples[-1].selection.unknown = 0
	_rejected(api.validate_definition(invalid, catalog), "等长但错字段仍拒绝")
	invalid = definition.duplicate(true)
	invalid.samples[-1].selection.beard = 1
	_rejected(api.validate_definition(invalid, catalog), "合法索引但脸须不兼容")
	invalid = definition.duplicate(true)
	invalid.samples[4].selection.beard = 1
	_rejected(api.validate_definition(invalid, catalog), "女性真实缺须件")
	_rejected(api.validate_definition(definition, {}), "缺少运行时族目录")
	var bad_catalog: Dictionary = catalog.duplicate(true)
	bad_catalog.fittings[-1].id = bad_catalog.fittings[0].id
	_rejected(api.validate_definition(definition, bad_catalog), "运行时族ID重复")
	bad_catalog = catalog.duplicate(true)
	bad_catalog.fittings.pop_back()
	_rejected(api.validate_definition(definition, bad_catalog), "运行时族缺失不能回落首族")
	bad_catalog = catalog.duplicate(true)
	var extra_fitting: Dictionary = bad_catalog.fittings[-1].duplicate(true)
	extra_fitting.id = "unaccepted_extra_family"
	bad_catalog.fittings.append(extra_fitting)
	invalid = definition.duplicate(true)
	invalid.samples[-1].fitting_id = extra_fitting.id
	_rejected(api.validate_definition(invalid, bad_catalog), "版本1不能用未验收的新族替代现有族")
	var invalid_before: Dictionary = invalid.duplicate(true)
	var catalog_before: Dictionary = catalog.duplicate(true)
	api.validate_definition(invalid, catalog)
	_check(invalid == invalid_before and catalog == catalog_before, "失败校验同样不改来源和比例族目录")

## 每个错误必须携带原因且不泄漏任何已通过的前缀代表。
func _rejected(result: Dictionary, label: String) -> void:
	_check(result.get("ok") == false, label + "明确失败")
	_check(result.get("samples") is Array and result.samples.is_empty(), label + "整批原子拒绝")
	_check(result.get("reason") is String and not result.reason.is_empty(), label + "提供原因")

## 记录具体失败后继续其他独立检查，保证退出码与全部结果一致。
func _check(condition: bool, label: String) -> void:
	checks += 1
	if not condition:
		failures.append(label)
		push_error("REPRESENTATIVE_CATALOG_FAILED: " + label)

## 只向标准输出报告，不修改已验收证据或运行时数据文件。
func _finish() -> void:
	print(JSON.stringify({"ok": failures.is_empty(), "checks": checks, "failures": failures, "engine": Engine.get_version_info().string, "scope": "read-only twelve exact accepted representatives, strict atomic validation and stable fitting IDs"}))
	quit(0 if failures.is_empty() else 1)
